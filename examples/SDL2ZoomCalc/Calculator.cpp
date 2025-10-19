#include <CtrlLib/CtrlLib.h>
#include <Painter/Painter.h>
#include <VirtualGui/SDL2GL/SDL2GL.h>

#include <cmath>

using namespace Upp;

struct CalcButton : Moveable<CalcButton> {
struct CalcButton {
    String label;
    Rectf  rect;
    Color  color;
    bool   operator_button;
};

class CalculatorCtrl : public Ctrl {
public:
    CalculatorCtrl();

    virtual void Paint(Draw& w) override;
    virtual void LeftDown(Point p, dword keyflags) override;
    virtual void LeftUp(Point p, dword keyflags) override;
    virtual void MouseMove(Point p, dword keyflags) override;
    virtual void MouseLeave() override;

private:
    enum Operator {
        OP_NONE,
        OP_ADD,
        OP_SUBTRACT,
        OP_MULTIPLY,
        OP_DIVIDE,
    };

    static constexpr double margin        = 20.0;
    static constexpr double spacing       = 10.0;
    static constexpr double button_size   = 70.0;
    static constexpr double display_height = 110.0;
    static constexpr double design_width  = 360.0;
    static constexpr double design_height = 540.0;

    Vector<CalcButton> buttons;
    int                pressed_index = -1;

    double             accumulator   = 0.0;
    String             display       = "0";
    String             history;
    Operator           pending_op    = OP_NONE;
    bool               entering_new  = true;
    bool               error_state   = false;

    void        CreateButtons();
    void        ActivateButton(int index);
    void        ProcessLabel(const String& label);
    void        ApplyPending(double value);
    double      ApplyOperator(Operator op, double lhs, double rhs);
    void        ClearAll();
    void        SetDisplay(double value);
    Pointf      ToDesign(Point p) const;
    double      GetScale() const;
    Pointf      GetOffset(double scale) const;
    String      FormatNumber(double value) const;
    bool        IsOperator(const String& label) const;
};

CalculatorCtrl::CalculatorCtrl()
{
    CreateButtons();
    BackPaint();
}

void CalculatorCtrl::CreateButtons()
{
    buttons.Clear();

    struct ButtonDef { const char *label; bool op; double span; };

    static const ButtonDef row1[] = {
        {"C", true, 1}, {"±", true, 1}, {"%", true, 1}, {"÷", true, 1}
    };
    static const ButtonDef row2[] = {
        {"7", false, 1}, {"8", false, 1}, {"9", false, 1}, {"×", true, 1}
    };
    static const ButtonDef row3[] = {
        {"4", false, 1}, {"5", false, 1}, {"6", false, 1}, {"−", true, 1}
    };
    static const ButtonDef row4[] = {
        {"1", false, 1}, {"2", false, 1}, {"3", false, 1}, {"+", true, 1}
    };
    static const ButtonDef row5[] = {
        {"0", false, 2}, {".", false, 1}, {"=", true, 1}
    };

    struct ButtonRow { const ButtonDef *defs; int count; };
    static const ButtonRow layout[] = {
        {row1, int(sizeof(row1) / sizeof(ButtonDef))},
        {row2, int(sizeof(row2) / sizeof(ButtonDef))},
        {row3, int(sizeof(row3) / sizeof(ButtonDef))},
        {row4, int(sizeof(row4) / sizeof(ButtonDef))},
        {row5, int(sizeof(row5) / sizeof(ButtonDef))},
    };

    double y = margin + display_height + spacing;
    for(const ButtonRow& row : layout) {
        double x = margin;
        for(int i = 0; i < row.count; i++) {
            const ButtonDef& def = row.defs[i];
            double width = button_size * def.span + spacing * (def.span - 1);
            CalcButton& b = buttons.Add();
            b.label = def.label;
            b.rect = Rectf(x, y, x + width, y + button_size);
            b.operator_button = def.op;
            if(def.label == String("C"))
                b.color = Color(0xE6, 0x64, 0x64);
            else if(def.label == String("="))
                b.color = Color(0xFF, 0x95, 0x00);
            else if(def.op)
                b.color = Color(0xFF, 0xA6, 0x3A);
            else
                b.color = Color(0x35, 0x3A, 0x44);
            x += width + spacing;
        }
        y += button_size + spacing;
    }
}

namespace {

Painter& RoundedRectPath(Painter& painter, const Rectf& rect, double radius)
{
    double rx = min(radius, rect.GetWidth() / 2.0);
    double ry = min(radius, rect.GetHeight() / 2.0);
    double x  = rect.left;
    double y  = rect.top;
    double cx = rect.GetWidth();
    double cy = rect.GetHeight();

    return painter
        .Move(x + rx, y)
        .Arc(x + rx, y + ry, rx, ry, -M_PI / 2, -M_PI / 2)
        .Line(x, y + cy - ry)
        .Arc(x + rx, y + cy - ry, rx, ry, M_PI, -M_PI / 2)
        .Line(x + cx - rx, y + cy)
        .Arc(x + cx - rx, y + cy - ry, rx, ry, M_PI / 2, -M_PI / 2)
        .Line(x + cx, y + ry)
        .Arc(x + cx - rx, y + ry, rx, ry, 0, -M_PI / 2)
        .Line(x + rx, y)
        .Close();
}

void FillRoundedRect(Painter& painter, const Rectf& rect, double radius, const Color& fill)
{
    RoundedRectPath(painter, rect, radius);
    painter.Fill(fill);
}

void StrokeRoundedRect(Painter& painter, const Rectf& rect, double radius, double width, const Color& stroke)
{
    RoundedRectPath(painter, rect, radius);
    painter.Stroke(width, stroke);
}

}

void CalculatorCtrl::Paint(Draw& w)
{
    Size sz = GetSize();
    DrawPainter p(w, sz);
    p.Clear(Color(0x16, 0x18, 0x1F));

    double scale = GetScale();
    if(scale <= 0)
        return;
    Pointf offset = GetOffset(scale);

    p.Scale(scale);
    p.Translate(offset.x, offset.y);

    Rectf display_rect(margin, margin, design_width - margin, margin + display_height);

    FillRoundedRect(p, display_rect, 18, Color(0x22, 0x26, 0x32));
    p.RoundJoin().RoundCap();
    p.RoundRect(display_rect, 18).Fill(Color(0x22, 0x26, 0x32));

    Font history_font = SansSerif(18);
    Font display_font = SansSerif(40).Bold();

    p.Opacity(0.6);
    double history_baseline = display_rect.top + history_font.GetAscent() + 12;
    p.Text(Pointf(display_rect.right - 10 - GetTextSize(history, history_font).cx,
                  history_baseline), history, history_font);
    p.Opacity(1.0);

    Size disp_sz = GetTextSize(display, display_font);
    double baseline = display_rect.bottom - 24;
    p.Text(Pointf(display_rect.right - 10 - disp_sz.cx, baseline), display, display_font);

    for(int i = 0; i < buttons.GetCount(); i++) {
        const CalcButton& b = buttons[i];
        Color fill = b.color;
        if(i == pressed_index)
            fill = Blend(fill, White(), 70);
        FillRoundedRect(p, b.rect, 14, fill);
        StrokeRoundedRect(p, b.rect, 14, 2, Blend(fill, Black(), 40));
        p.RoundRect(b.rect, 14).Fill(fill);
        p.RoundRect(b.rect, 14).Stroke(2, Blend(fill, Black(), 40));

        Font btn_font = b.operator_button ? SansSerif(28).Bold() : SansSerif(26);
        Size text_sz = GetTextSize(b.label, btn_font);
        double text_x = b.rect.left + (b.rect.GetSize().cx - text_sz.cx) / 2.0;
        double text_y = b.rect.top + (b.rect.GetSize().cy + btn_font.GetAscent() - btn_font.GetDescent()) / 2.0;
        p.Text(Pointf(text_x, text_y), b.label, btn_font);
    }
}

Pointf CalculatorCtrl::ToDesign(Point p) const
{
    double scale = GetScale();
    if(scale <= 0)
        return Pointf(0, 0);
    Pointf offset = GetOffset(scale);
    return Pointf((p.x - offset.x) / scale, (p.y - offset.y) / scale);
}

double CalculatorCtrl::GetScale() const
{
    Size sz = GetSize();
    double sx = sz.cx / design_width;
    double sy = sz.cy / design_height;
    return min(sx, sy);
}

Pointf CalculatorCtrl::GetOffset(double scale) const
{
    Size sz = GetSize();
    double cx = (sz.cx - design_width * scale) / 2.0;
    double cy = (sz.cy - design_height * scale) / 2.0;
    return Pointf(cx, cy);
}

void CalculatorCtrl::LeftDown(Point p, dword keyflags)
{
    pressed_index = -1;
    Pointf d = ToDesign(p);
    for(int i = 0; i < buttons.GetCount(); i++)
        if(buttons[i].rect.Contains(d)) {
            pressed_index = i;
            Refresh();
            break;
        }
    if(pressed_index >= 0)
        SetCapture();
}

void CalculatorCtrl::LeftUp(Point p, dword keyflags)
{
    if(HasCapture())
        ReleaseCapture();

    int idx = pressed_index;
    pressed_index = -1;
    Refresh();

    if(idx < 0)
        return;

    Pointf d = ToDesign(p);
    if(buttons[idx].rect.Contains(d))
        ActivateButton(idx);
}

void CalculatorCtrl::MouseMove(Point p, dword keyflags)
{
    if(!HasCapture())
        return;
    Pointf d = ToDesign(p);
    int inside = -1;
    for(int i = 0; i < buttons.GetCount(); i++)
        if(buttons[i].rect.Contains(d)) {
            inside = i;
            break;
        }
    if(inside != pressed_index) {
        pressed_index = inside;
        Refresh();
    }
}

void CalculatorCtrl::MouseLeave()
{
    if(!HasCapture()) {
        if(pressed_index >= 0) {
            pressed_index = -1;
            Refresh();
        }
    }
}

void CalculatorCtrl::ActivateButton(int index)
{
    if(index < 0 || index >= buttons.GetCount())
        return;
    ProcessLabel(buttons[index].label);
}

bool CalculatorCtrl::IsOperator(const String& label) const
{
    return label == "+" || label == "−" || label == "×" || label == "÷";
}

void CalculatorCtrl::ProcessLabel(const String& label)
{
    if(label == "C") {
        ClearAll();
        Refresh();
        return;
    }

    if(error_state) {
        ClearAll();
    }

    if(label == "±") {
        double value = -StrDbl(display);
        SetDisplay(value);
        entering_new = false;
        Refresh();
        return;
    }

    if(label == "%") {
        double value = StrDbl(display);
        value /= 100.0;
        SetDisplay(value);
        entering_new = true;
        Refresh();
        return;
    }

    if(label == ".") {
        if(entering_new) {
            display = "0.";
            entering_new = false;
        }
        else if(display.Find('.') < 0) {
            display.Cat('.');
        }
        Refresh();
        return;
    }

    if(label == "=") {
        double current = StrDbl(display);
        ApplyPending(current);
        pending_op = OP_NONE;
        entering_new = true;
        history.Clear();
        Refresh();
        return;
    }

    if(IsOperator(label)) {
        Operator new_pending = OP_NONE;
        if(label == "+")
            new_pending = OP_ADD;
        else if(label == "−")
            new_pending = OP_SUBTRACT;
        else if(label == "×")
            new_pending = OP_MULTIPLY;
        else if(label == "÷")
            new_pending = OP_DIVIDE;

        if(pending_op != OP_NONE && entering_new) {
            pending_op = new_pending;
            history = Format("%s %s", FormatNumber(accumulator), label);
            Refresh();
            return;
        }

        double current = StrDbl(display);
        ApplyPending(current);
        if(error_state) {
            Refresh();
            return;
        }
        pending_op = new_pending;
        entering_new = true;
        history = Format("%s %s", FormatNumber(accumulator), label);
        Refresh();
        return;
    }

    if(label.GetCount() == 1 && IsDigit((byte)label[0])) {
        if(entering_new || display == "0")
            display = label;
        else
            display.Cat(label);
        entering_new = false;
        Refresh();
        return;
    }

    Refresh();
}

void CalculatorCtrl::ApplyPending(double value)
{
    if(pending_op == OP_NONE) {
        accumulator = value;
        SetDisplay(accumulator);
        return;
    }

    double result = ApplyOperator(pending_op, accumulator, value);
    if(error_state) {
        pending_op = OP_NONE;
        return;
    }

    accumulator = result;
    SetDisplay(accumulator);
}

double CalculatorCtrl::ApplyOperator(Operator op, double lhs, double rhs)
{
    switch(op) {
    case OP_ADD:      return lhs + rhs;
    case OP_SUBTRACT: return lhs - rhs;
    case OP_MULTIPLY: return lhs * rhs;
    case OP_DIVIDE:
        if(fabs(rhs) < 1e-12) {
            display = "Error";
            history.Clear();
            error_state = true;
            return lhs;
        }
        return lhs / rhs;
    default:
        return rhs;
    }
}

void CalculatorCtrl::ClearAll()
{
    accumulator = 0.0;
    display = "0";
    history.Clear();
    pending_op = OP_NONE;
    entering_new = true;
    error_state = false;
}

void CalculatorCtrl::SetDisplay(double value)
{
    if(IsNaN(value) || !IsFin(value)) {
        display = "Error";
        error_state = true;
        return;
    }
    display = FormatNumber(value);
}

String CalculatorCtrl::FormatNumber(double value) const
{
    String s = FormatDouble(value);
    int dot = s.Find('.');
    if(dot >= 0) {
        while(s.GetCount() > dot + 1 && s[s.GetCount() - 1] == '0')
            s.Trim(s.GetCount() - 1);
        if(s.GetCount() > dot && s[s.GetCount() - 1] == '.')
            s.Trim(s.GetCount() - 1);
    }
    return s.IsEmpty() ? String("0") : s;
}

class CalculatorWindow : public TopWindow {
public:
    CalculatorWindow()
    {
        Title("SDL2GL Virtual Calculator");
        Sizeable().Zoomable();
        calc.SetFrame(NullFrame());
        Add(calc.SizePos());
        SetRect(0, 0, 400, 600);
    }

private:
    CalculatorCtrl calc;
};

CONSOLE_APP_MAIN
{
    SDL2GUI gui;
    gui.Create(RectC(100, 100, 500, 720), "Virtual SDL2GL Calculator");

    RunVirtualGui(gui, [] {
        SetDefaultCharset(CHARSET_UTF8);
        SetLanguage(LNG_('E','S','E','S'));
        SetLanguage(LNG_SPANISH);

        CalculatorWindow app;
        app.OpenMain();
        Ctrl::EventLoop();
    });
}
