/*
 * Include this before ANY vstgui and after you have included JuceHeader and you stand a chance
 *
 * IF YOU ARE READING THIS and you haven't been following the convresation on discord it will
 * look terrible and broken and wierd. That's because right now it is. If you want some context
 * here *please please* chat with @baconpaul in the #surge-development channel on discord or
 * check the pinned 'status' issue on the surge github. This code has all sorts of problems
 * (dependencies out of order, memory leaks, etc...) which I'm working through as I bootstrap
 * our way out of vstgui.
 */

#ifndef SURGE_ESCAPE_FROM_VSTGUI_H
#define SURGE_ESCAPE_FROM_VSTGUI_H

#include <memory>
#include <unordered_set>
#include "DebugHelpers.h"

#if MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

#if MAC
#define DEBUG_EFVG_MEMORY 1
#define DEBUG_EFVG_MEMORY_STACKS 0
#else
#define DEBUG_EFVG_MEMORY 0
#define DEBUG_EFVG_MEMORY_STACKS 0
#endif

#if DEBUG_EFVG_MEMORY
#include <unordered_map>

#if MAC || LINUX
#include <execinfo.h>
#include <cstdio>
#include <cstdlib>
#endif

#endif

#include <JuceHeader.h>

// The layers of unimpl. Really bad (D), standard, and we are OK with it for now (OKUNIMPL);
#define DUNIMPL                                                                                    \
    std::cout << "  - efvg unimplemented : " << __func__ << " at " << __FILE__ << ":" << __LINE__  \
              << std::endl;
#define UNIMPL void(0);
//#define UNIMPL DUNIMPL
#define UNIMPL_STACK DUNIMPL EscapeNS::Internal::printStack(__func__);
#define OKUNIMPL void(0);

typedef int VstKeyCode;

namespace EscapeFromVSTGUI
{
struct JuceVSTGUIEditorAdapterBase
{
    virtual juce::AudioProcessorEditor *getJuceEditor() = 0;
};
} // namespace EscapeFromVSTGUI

namespace EscapeNS
{
namespace Internal
{
inline void printStack(const char *where)
{
#if MAC
    void *callstack[128];
    int i, frames = backtrace(callstack, 128);
    char **strs = backtrace_symbols(callstack, frames);
    std::ostringstream oss;
    oss << "----- " << where << " -----\n";
    for (i = 0; i < frames - 1 && i < 20; ++i)
    {
        oss << strs[i] << "\n";
    }
    free(strs);
    std::cout << oss.str() << std::endl;
#endif
}
#if DEBUG_EFVG_MEMORY
struct DebugAllocRecord
{
    DebugAllocRecord() { record("CONSTRUCT"); }
    void record(const std::string &where)
    {
#if DEBUG_EFVG_MEMORY_STACKS
#if MAC
        void *callstack[128];
        int i, frames = backtrace(callstack, 128);
        char **strs = backtrace_symbols(callstack, frames);
        std::ostringstream oss;
        oss << "----- " << where << " -----\n";
        for (i = 0; i < frames - 1 && i < 20; ++i)
        {
            oss << strs[i] << "\n";
        }
        free(strs);
        records.push_back(oss.str());
#endif
#endif
    }
    int remembers = 0, forgets = 0;
    std::vector<std::string> records;
};
struct FakeRefcount;

inline std::unordered_map<FakeRefcount *, DebugAllocRecord> *getAllocMap()
{
    static std::unordered_map<FakeRefcount *, DebugAllocRecord> map;
    return &map;
}

struct RefActivity
{
    int creates = 0, deletes = 0;
    std::map<std::string, int> createsByType;
};
inline RefActivity *getRefActivity()
{
    static RefActivity r;
    return &r;
}

#endif
struct FakeRefcount
{
    explicit FakeRefcount(bool doDebug = true, bool alwaysLeak = false)
        : doDebug(doDebug), alwaysLeak(alwaysLeak)
    {
#if DEBUG_EFVG_MEMORY
        if (doDebug)
        {
            getAllocMap()->emplace(std::make_pair(this, DebugAllocRecord()));
            getRefActivity()->creates++;
        }
#else
        if (alwaysLeak)
        {
            std::cout << "YOU LEFT ALWAYS LEAK ON SILLY" << std::endl;
        }
#endif
    }
    virtual ~FakeRefcount()
    {
#if DEBUG_EFVG_MEMORY
        if (doDebug)
        {
            getAllocMap()->erase(this);
            getRefActivity()->deletes++;
        }
#endif
    }

    virtual void remember()
    {
        refCount++;
#if DEBUG_EFVG_MEMORY
        if (doDebug)
        {
            (*getAllocMap())[this].remembers++;
            (*getAllocMap())[this].record("Remember");
        }
#endif
    }
    virtual void forget()
    {
        refCount--;
#if DEBUG_EFVG_MEMORY
        if (doDebug)
        {
            (*getAllocMap())[this].forgets++;
            (*getAllocMap())[this].record("Forget");
        }
#endif
        if (refCount == 0)
        {
            if (!alwaysLeak)
            {
#if DEBUG_EFVG_MEMORY
                getRefActivity()->createsByType[typeid(*this).name()]++;
#endif
                delete this;
            }
            else
            {
                std::cout << "Skipping a destroy: ptr=" << this << "id=" << typeid(*this).name()
                          << std::endl;
            }
        }
        else if (refCount < 0)
        {
            std::cout << "REFCOUNT NEGATIVE" << std::endl;
        }
    }
    int64_t refCount = 0;
    bool doDebug = true, alwaysLeak = false;
};

#if DEBUG_EFVG_MEMORY

inline void showMemoryOutstanding()
{
    std::cout << "EscapeFromVSTGUI Memory Check:\n   - Total activity : Create="
              << getRefActivity()->creates << " Delete=" << getRefActivity()->deletes << std::endl;
    for (auto &p : getRefActivity()->createsByType)
    {
        std::cout << "  | " << p.first << " -> " << p.second << std::endl;
    }
    for (auto &p : *(getAllocMap()))
    {
        if (p.first->doDebug)
        {
            std::cout << "   - Still here! ptr=" << p.first << " refct=" << p.first->refCount
                      << " remembers=" << p.second.remembers << " forgets=" << p.second.forgets
                      << " " << typeid(*(p.first)).name() << std::endl;
            for (auto &r : p.second.records)
                std::cout << r << "\n" << std::endl;
        }
    }
}
#endif

inline std::unordered_set<FakeRefcount *> *getForgetMeLater()
{
    static std::unordered_set<FakeRefcount *> fml;
    return &fml;
}

inline void enqueueForget(FakeRefcount *c) { getForgetMeLater()->insert(c); }

} // namespace Internal

inline void efvgIdle()
{
    for (auto v : *(Internal::getForgetMeLater()))
        v->forget();
    Internal::getForgetMeLater()->clear();
}

#if MAC
inline CFBundleRef getBundleRef() { return nullptr; }
#endif

typedef juce::AudioProcessorEditor EditorType;
typedef const char *UTF8String; // I know I know

struct CFrame;
struct CBitmap;
typedef juce::Colour CColor;

typedef float CCoord;
struct CPoint
{
    CPoint() : x(0), y(0) {}
    CPoint(float x, float y) : x(x), y(y) {}
    CPoint(const juce::Point<float> &p) : x(p.x), y(p.y) {}
    CPoint(const juce::Point<int> &p) : x(p.x), y(p.y) {}
    float x, y;

    bool operator==(const CPoint &that) const { return (x == that.x && y == that.y); }
    bool operator!=(const CPoint &that) const { return !(*this == that); }
    CPoint operator-(const CPoint &that) const { return {x - that.x, y - that.y}; }
    CPoint offset(float dx, float dy)
    {
        x += dx;
        y += dy;
        return *this;
    }

    operator juce::Point<float>() const { return juce::Point<float>(x, y); }
    operator juce::Point<int>() const { return juce::Point<int>(x, y); }
};

struct CRect
{
    double top = 0, bottom = 0, left = 0, right = 0;
    CRect() = default;
    CRect(juce::Rectangle<int> rect)
    {
        left = rect.getX();
        top = rect.getY();
        right = left + rect.getWidth();
        bottom = top + rect.getHeight();
    }
    CRect(const CPoint &corner, const CPoint &size)
    {
        top = corner.y;
        left = corner.x;
        right = left + size.x;
        bottom = top + size.y;
    }
    CRect(float left, float top, float right, float bottom)
    {
        this->left = left;
        this->top = top;
        this->right = right;
        this->bottom = bottom;
    }

    inline CPoint getSize() const { return CPoint(right - left, bottom - top); }
    inline CPoint getTopLeft() const { return CPoint(left, top); }
    inline CPoint getBottomLeft() const { return CPoint(left, bottom); }
    inline CPoint getTopRight() const { return CPoint(right, top); }
    inline CPoint getBottomRight() const { return CPoint(right, bottom); }

    inline float getWidth() const { return right - left; }
    inline float getHeight() const { return bottom - top; }

    CRect centerInside(const CRect &r)
    {
        UNIMPL;
        return r;
    }
    bool rectOverlap(const CRect &rect)
    {
        return right >= rect.left && left <= rect.right && bottom >= rect.top && top <= rect.bottom;
    }
    inline void setHeight(float h) { bottom = top + h; }
    inline void setWidth(float h) { right = left + h; }

    inline CRect inset(float dx, float dy)
    {
        left += dx;
        right -= dx;
        top += dy;
        bottom -= dy;
        return *this;
    }
    inline CRect offset(float x, float y)
    {
        top += y;
        bottom += y;
        left += x;
        right += x;
        return *this;
    }
    inline CRect moveTo(CPoint p) { return moveTo(p.x, p.y); }
    inline CRect moveTo(float x, float y)
    {
        auto dx = x - left;
        auto dy = y - top;
        top += dy;
        left += dx;
        bottom += dy;
        right += dx;
        return *this;
    }
    inline bool pointInside(const CPoint &where) const { return asJuceFloatRect().contains(where); }
    inline CPoint getCenter()
    {
        auto p = asJuceFloatRect().getCentre();
        return p;
    }
    inline CRect extend(float dx, float dy) { return inset(-dx, -dy); }
    inline CRect bound(CRect &rect)
    {
        if (left < rect.left)
            left = rect.left;
        if (top < rect.top)
            top = rect.top;
        if (right > rect.right)
            right = rect.right;
        if (bottom > rect.bottom)
            bottom = rect.bottom;
        if (bottom < top)
            bottom = top;
        if (right < left)
            right = left;
        return *this;
    }

    operator juce::Rectangle<float>() const
    {
        return juce::Rectangle<float>(left, top, getWidth(), getHeight());
    }
    juce::Rectangle<float> asJuceFloatRect() const
    {
        return static_cast<juce::Rectangle<float>>(*this);
    }
    juce::Rectangle<int> asJuceIntRect() const
    {
        juce::Rectangle<float> f = *this;
        return f.toNearestIntEdges();
    }
};

struct CGradient : public Internal::FakeRefcount
{
    std::unique_ptr<juce::ColourGradient> grad;
    CGradient() { grad = std::make_unique<juce::ColourGradient>(); }
    struct ColorStopMap
    {
    };
    static CGradient *create(const ColorStopMap &m)
    {
        auto res = new CGradient();
        res->remember();
        return res;
    };
    void addColorStop(float pos, const CColor &c) { grad->addColour(pos, c); }
};

enum CMouseEventResult
{
    kMouseEventNotImplemented = 0,
    kMouseEventHandled,
    kMouseEventNotHandled,
    kMouseDownEventHandledButDontNeedMovedOrUpEvents,
    kMouseMoveEventHandledButDontNeedMoreEvents
};

enum CHoriTxtAlign
{
    kLeftText = 0,
    kCenterText,
    kRightText
};

enum CButton
{
    /** left mouse button */
    kLButton = juce::ModifierKeys::leftButtonModifier,
    /** middle mouse button */
    kMButton = juce::ModifierKeys::middleButtonModifier,
    /** right mouse button */
    kRButton = juce::ModifierKeys::rightButtonModifier,
    /** shift modifier */
    kShift = juce::ModifierKeys::shiftModifier,
    /** control modifier (Command Key on Mac OS X and Control Key on Windows) */
    kControl = juce::ModifierKeys::ctrlModifier,
    /** alt modifier */
    kAlt = juce::ModifierKeys::altModifier,
    /** apple modifier (Mac OS X only. Is the Control key) */
    kApple = 1U << 29,
    /** 4th mouse button */
    kButton4 = 1U << 30,
    /** 5th mouse button */
    kButton5 = 1U << 31,
    /** mouse button is double click */
    kDoubleClick = 1U << 22,
    /** system mouse wheel setting is inverted (Only valid for onMouseWheel methods). The distance
       value is already transformed back to non inverted. But for scroll views we need to know if we
       need to invert it back. */
    kMouseWheelInverted = 1U << 11,
    /** event caused by touch gesture instead of mouse */
    kTouch = 1U << 12
};

// https://docs.juce.com/master/classModifierKeys.html#ae15cb452a97164e1b857086a1405942f
struct CButtonState
{
    int state = juce::ModifierKeys::noModifiers;
    CButtonState()
    {
        auto ms = juce::ModifierKeys::currentModifiers;
        state = ms.getRawFlags();
    }
    CButtonState(CButton &b) { state = (int)b; }
    CButtonState(int b) { state = b; }
    CButtonState(const juce::ModifierKeys &m) { state = m.getRawFlags(); }
    operator int() const { return state; }
    bool isRightButton() const { return (state & kRButton); }
    bool isTouch() const { return (state & kTouch); }
    int getButtonState() const { return state; }
};

struct CDrawContext
{
    juce::Graphics &g;
    explicit CDrawContext(juce::Graphics &g) : g(g) {}
};

struct CResourceDescription
{
    enum Type : uint32_t
    {
        kIntegerType = 0
    };
    CResourceDescription(int id)
    {
        u.id = id;
        u.type = kIntegerType;
    }
    Type type;
    struct U
    {
        int32_t id = 0;
        Type type = kIntegerType;
    } u;
};

struct CBitmap : public Internal::FakeRefcount
{
    CBitmap(const CResourceDescription &d) : desc(d), Internal::FakeRefcount(false, false) {}
    virtual ~CBitmap()
    {
        // std::cout << " Deleting bitmap with " << desc.u.id << std::endl;
    }

    virtual void draw(CDrawContext *dc, const CRect &r, const CPoint &off = CPoint(),
                      float alpha = 1.0)
    {
        juce::Graphics::ScopedSaveState gs(dc->g);
        auto tl = r.asJuceIntRect().getTopLeft();

        auto t = juce::AffineTransform().translated(tl.x, tl.y).translated(-off.x, -off.y);
        dc->g.reduceClipRegion(r.asJuceIntRect());
        drawable->draw(dc->g, alpha, t);
    }

    // The entire memory management of these happens through SurgeBitmaps so deal
    // with it there
    void remember() override {}
    void forget() override {}

    CResourceDescription desc;
    std::unique_ptr<juce::Drawable> drawable;
};

struct CViewBase;

struct OnRemovedHandler
{
    virtual void onRemoved() = 0;
};

template <typename T> struct juceCViewConnector : public T, public OnRemovedHandler
{
    juceCViewConnector() : T() {}
    ~juceCViewConnector();
    void setViewCompanion(CViewBase *v);
    void onRemoved() override;

    CViewBase *viewCompanion = nullptr;
    void paint(juce::Graphics &g) override;

    CMouseEventResult traverseMouseParents(
        const juce::MouseEvent &e,
        std::function<CMouseEventResult(CViewBase *, const CPoint &, const CButtonState &)> vf,
        std::function<void(const juce::MouseEvent &)> jf);

    void mouseDown(const juce::MouseEvent &e) override;
    void mouseUp(const juce::MouseEvent &e) override;
    void mouseEnter(const juce::MouseEvent &e) override;
    void mouseExit(const juce::MouseEvent &e) override;
    void mouseMove(const juce::MouseEvent &e) override;
    void mouseDrag(const juce::MouseEvent &e) override;
    void mouseDoubleClick(const juce::MouseEvent &e) override;
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override;
    void mouseMagnify(const juce::MouseEvent &event, float scaleFactor) override;
    bool supressMoveAndUp = false;
};

struct CView;

// Clena this up obviously

struct BaseViewFunctions
{
    virtual void invalid() = 0;
    virtual CRect getControlViewSize() = 0;
    virtual CCoord getControlHeight() = 0;
};

struct CMouseWheelAxis
{
};

struct CViewBase : public Internal::FakeRefcount, public BaseViewFunctions
{
    CViewBase(const CRect &size) : size(size), ma(size) {}
    virtual ~CViewBase(){
        // Here we probably have to make sure that the juce component is removed
    };
    virtual juce::Component *juceComponent() = 0;

    CRect getControlViewSize() override { return getViewSize(); }
    CCoord getControlHeight() override { return getHeight(); }

    virtual void draw(CDrawContext *dc){};
    CRect getViewSize()
    {
        // FIXME - this should reajju just use the cast-operator
        auto b = juceComponent()->getBounds();
        return CRect(CPoint(b.getX(), b.getY()),
                     CPoint(juceComponent()->getWidth(), juceComponent()->getHeight()));
    }

    CPoint getTopLeft()
    {
        auto b = juceComponent()->getBounds();
        return CPoint(b.getX(), b.getY());
    }

    void setViewSize(const CRect &r) { juceComponent()->setBounds(r.asJuceIntRect()); }
    float getHeight() { return juceComponent()->getHeight(); }
    float getWidth() { return juceComponent()->getWidth(); }

    virtual bool magnify(CPoint &where, float amount) { return false; }

    virtual CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual CMouseEventResult onMouseUp(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual CMouseEventResult onMouseEntered(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual CMouseEventResult onMouseExited(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual CMouseEventResult onMouseMoved(CPoint &where, const CButtonState &buttons)
    {
        return kMouseEventNotHandled;
    }

    virtual bool supportsJuceNativeWheel() { return false; }

    virtual void mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel) {}

    virtual bool onWheel(const CPoint &where, const float &distance, const CButtonState &buttons)
    {
        return false;
    }

    virtual bool onWheel(const CPoint &where, const CMouseWheelAxis &, const float &distance,
                         const CButtonState &buttons)
    {
        return false;
    }

    void invalid() override { juceComponent()->repaint(); }

    CRect ma;
    CRect getMouseableArea() { return ma; }
    void setMouseableArea(const CRect &r) { ma = r; }
    void setMouseEnabled(bool b) { OKUNIMPL; }
    CPoint localToFrame(CPoint &w)
    {
        OKUNIMPL;
        return w;
    }

    void setDirty(bool b = true)
    {
        if (b)
            invalid();
    }
    bool isDirty()
    {
        OKUNIMPL;
        return true;
    }
    void setVisible(bool b)
    {
        if (juceComponent())
            juceComponent()->setVisible(b);
    }
    bool isVisible()
    {
        if (juceComponent())
            return juceComponent()->isVisible();
        return false;
    }
    void setSize(const CRect &s) { UNIMPL; }
    void setSize(float w, float h) { juceComponent()->setBounds(0, 0, w, h); }
    CRect size;

    CView *parentView = nullptr;
    CView *getParentView() { return parentView; }

    CFrame *frame = nullptr;
    CFrame *getFrame();
};

struct CView : public CViewBase
{
    CView(const CRect &size) : CViewBase(size) {}
    ~CView() {}

    virtual void onAdded() {}
    virtual void efvg_resolveDeferredAdds(){};

    std::unique_ptr<juceCViewConnector<juce::Component>> comp;
    juce::Component *juceComponent() override
    {
        if (!comp)
        {
            comp = std::make_unique<juceCViewConnector<juce::Component>>();
            comp->setBounds(size.asJuceIntRect());
            comp->setViewCompanion(this);
        }
        return comp.get();
    }

    EscapeFromVSTGUI::JuceVSTGUIEditorAdapterBase *ed = nullptr; // FIX ME obviously
};

#define GSPAIR(x, sT, gT, gTV)                                                                     \
    gT m_##x = gTV;                                                                                \
    virtual void set##x(sT v) { m_##x = v; }                                                       \
    virtual gT get##x() const { return m_##x; }

#define COLPAIR(x)                                                                                 \
    CColor m_##x;                                                                                  \
    virtual void set##x(const CColor &v) { m_##x = v; }                                            \
    virtual CColor get##x() const { return m_##x; }

inline CFrame *CViewBase::getFrame()
{
    if (!frame)
    {
        if (parentView)
        {
            return parentView->getFrame();
        }
    }
    return frame;
}

struct CViewContainer : public CView
{
    CViewContainer(const CRect &size) : CView(size) {}
    ~CViewContainer()
    {
        for (auto e : views)
        {
            e->forget();
        }
        views.clear();
    }
    bool transparent = false;
    void setTransparency(bool b) { transparent = b; }

    CBitmap *bg = nullptr;
    void setBackground(CBitmap *b)
    {
        b->remember();
        if (bg)
            bg->forget();
        bg = b;
    }
    CColor bgcol;
    void setBackgroundColor(const CColor &c) { bgcol = c; }
    int getNbViews() { return views.size(); }
    CView *getView(int i)
    {
        if (i >= 0 && i < views.size())
            return views[i];
        return nullptr;
    }

    void onAdded() override {}
    void addView(CView *v)
    {
        if (!v)
            return;

        v->remember();
        v->ed = ed;
        v->frame = frame;
        v->parentView = this;
        auto jc = v->juceComponent();
        if (jc && ed)
        {
            juceComponent()->addAndMakeVisible(*jc);
            v->efvg_resolveDeferredAdds();
            v->onAdded();
        }
        else
        {
            deferredAdds.push_back(v);
        }
        views.push_back(v);
    }

    std::unordered_set<std::unique_ptr<juce::Component>> rawAddedComponents;
    virtual void takeOwnership(std::unique_ptr<juce::Component> c)
    {
        rawAddedComponents.insert(std::move(c));
    }

    void removeViewInternals(CView *v, bool forgetChildren = true)
    {
        auto cvc = dynamic_cast<CViewContainer *>(v);
        if (cvc)
        {
            cvc->removeAll(forgetChildren);
        }

        juceComponent()->removeChildComponent(v->juceComponent());
        auto orh = dynamic_cast<OnRemovedHandler *>(v->juceComponent());
        if (orh)
        {
            orh->onRemoved();
        }
        v->parentView = nullptr;
    }
    void removeView(CView *v, bool doForget = true)
    {
        auto pos = std::find(views.begin(), views.end(), v);
        if (pos == views.end())
        {
            std::cout << "WIERD ERROR";
            jassert(false);
            return;
        }
        removeViewInternals(v, doForget);
        views.erase(pos);
        if (doForget)
            v->forget();
        invalid();
    }

    CView *getViewAt(const CPoint &p)
    {
        UNIMPL;
        return nullptr;
    }
    bool isChild(CView *v)
    {
        UNIMPL;
        return true;
    }
    CView *getFocusView()
    {
        // UNIMPL;
        // FIXME!!
        return nullptr;
    }
    void removeAll(bool b = true)
    {
        for (auto &j : rawAddedComponents)
        {
            juceComponent()->removeChildComponent(j.get());
        }
        rawAddedComponents.clear();

        for (auto e : views)
        {
            removeViewInternals(e, b);
            if (b)
            {
                e->forget();
            }
        }
        views.clear();
    }
    void draw(CDrawContext *dc) override
    {
        if (bg)
        {
            bg->draw(dc, getViewSize());
        }
        else if (!transparent)
        {
            dc->g.fillAll(bgcol);
        }
        else
        {
        }
    }

    CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons) override
    {
        return kMouseEventNotHandled;
    }
    void efvg_resolveDeferredAdds() override
    {
        for (auto v : deferredAdds)
        {
            v->ed = ed;
            juceComponent()->addAndMakeVisible(v->juceComponent());
            v->efvg_resolveDeferredAdds();
        }
        deferredAdds.clear();

        for (auto v : deferredJuceAdds)
        {
            juceComponent()->addAndMakeVisible(v);
        }
        deferredJuceAdds.clear();
    }

    std::vector<CView *> deferredAdds;
    std::vector<juce::Component *> deferredJuceAdds;
    std::vector<CView *> views;
};

struct CFrame : public CViewContainer
{
    CFrame(CRect &rect, EscapeFromVSTGUI::JuceVSTGUIEditorAdapterBase *ed) : CViewContainer(rect)
    {
        this->ed = ed;
        this->frame = this;

        juceComponent()->setBounds(rect.asJuceIntRect());
        ed->getJuceEditor()->addAndMakeVisible(juceComponent());
    }
    ~CFrame() = default;

    void draw(CDrawContext *dc) override { CViewContainer::draw(dc); }

    void invalid() override
    {
        for (auto v : views)
            v->invalid();
    }
    void invalidRect(const CRect &r) { invalid(); }
    void setDirty(bool b = true) { invalid(); }

    void open(void *parent, int) { OKUNIMPL; }
    void close() { removeAll(); }

    COLPAIR(FocusColor);

    void localToFrame(CRect &r) { UNIMPL; }
    void localToFrame(CPoint &p) { UNIMPL; };
    void setZoom(float z) { OKUNIMPL; }
    void getPosition(float &x, float &y)
    {
        auto b = juceComponent()->getScreenPosition();
        x = b.x;
        y = b.y;
    }
    void getCurrentMouseLocation(CPoint &w)
    {
        auto pq = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
        auto b = juceComponent()->getLocalPoint(nullptr, pq);
        w.x = b.x;
        w.y = b.y;
    }
    CButtonState getCurrentMouseButtons() { return CButtonState(); }
};

struct CControlValueInterface
{
    virtual uint32_t getTag() const = 0;
    virtual float getValue() const = 0;
    virtual void setValue(float) = 0;
    virtual void valueChanged() = 0;

    BaseViewFunctions *asBaseViewFunctions()
    {
        auto r = dynamic_cast<BaseViewFunctions *>(this);
        if (r)
            return r;
        jassert(false);
        return nullptr;
    }
};

struct IControlListener
{
    virtual void valueChanged(CControlValueInterface *p) = 0;
    virtual int32_t controlModifierClicked(CControlValueInterface *p, CButtonState s)
    {
        return false;
    }

    virtual void controlBeginEdit(CControlValueInterface *control){};
    virtual void controlEndEdit(CControlValueInterface *control){};
};

struct IKeyboardHook
{
    virtual int32_t onKeyDown(const VstKeyCode &code, CFrame *frame) = 0;
    virtual int32_t onKeyUp(const VstKeyCode &code, CFrame *frame) = 0;
};

template <typename T> inline juceCViewConnector<T>::~juceCViewConnector() {}

template <typename T> inline void juceCViewConnector<T>::onRemoved()
{
    // Internal::enqueueForget(viewCompanion);
    viewCompanion = nullptr;
}
template <typename T> inline void juceCViewConnector<T>::setViewCompanion(CViewBase *v)
{
    viewCompanion = v;
    // viewCompanion->remember();
}

template <typename T> inline void juceCViewConnector<T>::paint(juce::Graphics &g)
{
    if (viewCompanion)
    {
        auto dc = std::make_unique<CDrawContext>(g);
        auto b = T::getBounds();
        auto t = juce::AffineTransform().translated(-b.getX(), -b.getY());
        juce::Graphics::ScopedSaveState gs(g);
        g.addTransform(t);
        viewCompanion->draw(dc.get());
    }
    T::paint(g);
}

template <typename T>
inline CMouseEventResult juceCViewConnector<T>::traverseMouseParents(
    const juce::MouseEvent &e,
    std::function<CMouseEventResult(CViewBase *, const CPoint &, const CButtonState &)> vf,
    std::function<void(const juce::MouseEvent &)> jf)
{
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);
    CViewBase *curr = viewCompanion;
    auto r = kMouseEventNotHandled;
    while (curr && r == kMouseEventNotHandled)
    {
        r = vf(curr, w, CButtonState(e.mods));
        curr = curr->getParentView();
    }
    if (r == kMouseEventNotHandled)
    {
        jf(e);
    }

    return r;
}

template <typename T> inline void juceCViewConnector<T>::mouseDown(const juce::MouseEvent &e)
{
    supressMoveAndUp = false;
    auto res = traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseDown(b, c); },
        [this](auto e) { T::mouseDown(e); });
    if (res == kMouseDownEventHandledButDontNeedMovedOrUpEvents)
        supressMoveAndUp = true;
}

template <typename T> inline void juceCViewConnector<T>::mouseUp(const juce::MouseEvent &e)
{
    if (!supressMoveAndUp)
    {
        traverseMouseParents(
            e, [](auto *a, auto b, auto c) { return a->onMouseUp(b, c); },
            [this](auto e) { T::mouseUp(e); });
    }
    supressMoveAndUp = false;
}
template <typename T> inline void juceCViewConnector<T>::mouseMove(const juce::MouseEvent &e)
{
    // Don't supress check here because only DRAG will be called in the down
    traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseMoved(b, c); },
        [this](auto e) { T::mouseMove(e); });
}

template <typename T> inline void juceCViewConnector<T>::mouseDrag(const juce::MouseEvent &e)
{
    if (supressMoveAndUp)
        return;
    traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseMoved(b, c); },
        [this](auto e) { T::mouseDrag(e); });
}

template <typename T> inline void juceCViewConnector<T>::mouseEnter(const juce::MouseEvent &e)
{
    traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseEntered(b, c); },
        [this](auto e) { T::mouseEnter(e); });
}
template <typename T> inline void juceCViewConnector<T>::mouseExit(const juce::MouseEvent &e)
{
    traverseMouseParents(
        e, [](auto *a, auto b, auto c) { return a->onMouseExited(b, c); },
        [this](auto e) { T::mouseExit(e); });
}

template <typename T> inline void juceCViewConnector<T>::mouseDoubleClick(const juce::MouseEvent &e)
{
    OKUNIMPL; // wanna do parent thing
    auto b = T::getBounds().getTopLeft();
    CPoint w(e.x + b.x, e.y + b.y);
    auto bs = CButtonState(e.mods) | kDoubleClick;

    auto r = viewCompanion->onMouseDown(w, CButtonState(bs));
    if (r == kMouseEventNotHandled)
    {
        T::mouseDoubleClick(e);
    }
}
template <typename T>
inline void juceCViewConnector<T>::mouseWheelMove(const juce::MouseEvent &e,
                                                  const juce::MouseWheelDetails &wheel)
{
    jassert(false);
}
template <typename T>
inline void juceCViewConnector<T>::mouseMagnify(const juce::MouseEvent &event, float scaleFactor)
{
    std::cout << "scaleFactor is " << scaleFactor << std::endl;
    auto b = T::getBounds().getTopLeft();
    CPoint w(event.x + b.x, event.y + b.y);
    if (!viewCompanion->magnify(w, scaleFactor))
    {
        T::mouseMagnify(event, scaleFactor);
    }
}

#define CLASS_METHODS(a, b)
} // namespace EscapeNS

namespace VSTGUI = EscapeNS;

namespace EscapeFromVSTGUI
{
struct JuceVSTGUIEditorAdapter : public JuceVSTGUIEditorAdapterBase
{
    JuceVSTGUIEditorAdapter(juce::AudioProcessorEditor *parentEd) : parentEd(parentEd) {}
    virtual ~JuceVSTGUIEditorAdapter() = default;
    juce::AudioProcessorEditor *getJuceEditor() { return parentEd; }

    VSTGUI::CRect rect;
    VSTGUI::CFrame *frame = nullptr;
    VSTGUI::CFrame *getFrame() { return frame; }

    juce::AudioProcessorEditor *parentEd;
};

} // namespace EscapeFromVSTGUI

#endif // SURGE_ESCAPE_FROM_VSTGUI_H
