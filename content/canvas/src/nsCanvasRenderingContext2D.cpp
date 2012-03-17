/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Vladimir Vukicevic <vladimir@pobox.com>
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rob Arnold <tellrob@gmail.com>
 *   Eric Butler <zantifon@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "base/basictypes.h"

#include "nsIDOMXULElement.h"

#include "prmem.h"
#include "prenv.h"

#include "nsIServiceManager.h"
#include "nsMathUtils.h"

#include "nsContentUtils.h"

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsHTMLCanvasElement.h"
#include "nsSVGEffects.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIVariant.h"

#include "nsIInterfaceRequestorUtils.h"
#include "nsIFrame.h"
#include "nsDOMError.h"
#include "nsIScriptError.h"

#include "nsCSSParser.h"
#include "mozilla/css/StyleRule.h"
#include "mozilla/css/Declaration.h"
#include "nsComputedDOMStyle.h"
#include "nsStyleSet.h"

#include "nsPrintfCString.h"

#include "nsReadableUtils.h"

#include "nsColor.h"
#include "nsGfxCIID.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDocShell.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIXPConnect.h"
#include "nsDisplayList.h"

#include "nsTArray.h"

#include "imgIEncoder.h"

#include "gfxContext.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "gfxFont.h"
#include "gfxBlur.h"
#include "gfxUtils.h"

#include "nsFrameManager.h"
#include "nsFrameLoader.h"
#include "nsBidi.h"
#include "nsBidiPresUtils.h"
#include "Layers.h"
#include "CanvasUtils.h"
#include "nsIMemoryReporter.h"
#include "nsStyleUtil.h"
#include "CanvasImageCache.h"
#include "CheckedInt.h"

#include <algorithm>

#include "jsapi.h"
#include "jstypedarray.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ImageData.h"
#include "mozilla/dom/PBrowserParent.h"
#include "mozilla/ipc/DocumentRendererParent.h"
#include "mozilla/ipc/PDocumentRendererParent.h"

// windows.h (included by chromium code) defines this, in its infinite wisdom
#undef DrawText

using namespace mozilla;
using namespace mozilla::CanvasUtils;
using namespace mozilla::dom;
using namespace mozilla::ipc;
using namespace mozilla::layers;

static float kDefaultFontSize = 10.0;
static NS_NAMED_LITERAL_STRING(kDefaultFontName, "sans-serif");
static NS_NAMED_LITERAL_STRING(kDefaultFontStyle, "10px sans-serif");

/* Memory reporter stuff */
static nsIMemoryReporter *gCanvasMemoryReporter = nsnull;
static PRInt64 gCanvasMemoryUsed = 0;

static PRInt64 GetCanvasMemoryUsed() {
    return gCanvasMemoryUsed;
}

// This is KIND_OTHER because it's not always clear where in memory the pixels of
// a canvas are stored.  Furthermore, this memory will be tracked by the
// underlying surface implementations.  See bug 655638 for details.
NS_MEMORY_REPORTER_IMPLEMENT(CanvasMemory,
    "canvas-2d-pixel-bytes",
    KIND_OTHER,
    UNITS_BYTES,
    GetCanvasMemoryUsed,
    "Memory used by 2D canvases. Each canvas requires (width * height * 4) "
    "bytes.")

static void
CopyContext(gfxContext* dest, gfxContext* src)
{
    dest->Multiply(src->CurrentMatrix());

    nsRefPtr<gfxPath> path = src->CopyPath();
    dest->NewPath();
    dest->AppendPath(path);

    nsRefPtr<gfxPattern> pattern = src->GetPattern();
    dest->SetPattern(pattern);

    dest->SetLineWidth(src->CurrentLineWidth());
    dest->SetLineCap(src->CurrentLineCap());
    dest->SetLineJoin(src->CurrentLineJoin());
    dest->SetMiterLimit(src->CurrentMiterLimit());
    dest->SetFillRule(src->CurrentFillRule());

    dest->SetAntialiasMode(src->CurrentAntialiasMode());

    AutoFallibleTArray<gfxFloat, 10> dashes;
    double dashOffset;
    if (src->CurrentDash(dashes, &dashOffset)) {
        dest->SetDash(dashes.Elements(), dashes.Length(), dashOffset);
    }
}

/**
 ** nsCanvasGradient
 **/
#define NS_CANVASGRADIENT_PRIVATE_IID \
    { 0x491d39d8, 0x4058, 0x42bd, { 0xac, 0x76, 0x70, 0xd5, 0x62, 0x7f, 0x02, 0x10 } }
class nsCanvasGradient MOZ_FINAL : public nsIDOMCanvasGradient
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASGRADIENT_PRIVATE_IID)

    nsCanvasGradient(gfxPattern* pat)
        : mPattern(pat)
    {
    }

    gfxPattern* GetPattern() {
        return mPattern;
    }

    /* nsIDOMCanvasGradient */
    NS_IMETHOD AddColorStop (float offset,
                             const nsAString& colorstr)
    {
        if (!FloatValidate(offset) || offset < 0.0 || offset > 1.0)
            return NS_ERROR_DOM_INDEX_SIZE_ERR;

        nscolor color;
        nsCSSParser parser;
        nsresult rv = parser.ParseColorString(nsString(colorstr),
                                              nsnull, 0, &color);
        if (NS_FAILED(rv))
            return NS_ERROR_DOM_SYNTAX_ERR;

        mPattern->AddColorStop(offset, gfxRGBA(color));

        return NS_OK;
    }

    NS_DECL_ISUPPORTS

protected:
    nsRefPtr<gfxPattern> mPattern;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCanvasGradient, NS_CANVASGRADIENT_PRIVATE_IID)

NS_IMPL_ADDREF(nsCanvasGradient)
NS_IMPL_RELEASE(nsCanvasGradient)

DOMCI_DATA(CanvasGradient, nsCanvasGradient)

NS_INTERFACE_MAP_BEGIN(nsCanvasGradient)
  NS_INTERFACE_MAP_ENTRY(nsCanvasGradient)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasGradient)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CanvasGradient)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/**
 ** nsCanvasPattern
 **/
#define NS_CANVASPATTERN_PRIVATE_IID \
    { 0xb85c6c8a, 0x0624, 0x4530, { 0xb8, 0xee, 0xff, 0xdf, 0x42, 0xe8, 0x21, 0x6d } }
class nsCanvasPattern MOZ_FINAL : public nsIDOMCanvasPattern
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASPATTERN_PRIVATE_IID)

    nsCanvasPattern(gfxPattern* pat,
                    nsIPrincipal* principalForSecurityCheck,
                    bool forceWriteOnly,
                    bool CORSUsed)
        : mPattern(pat),
          mPrincipal(principalForSecurityCheck),
          mForceWriteOnly(forceWriteOnly),
          mCORSUsed(CORSUsed)
    {
    }

    gfxPattern* GetPattern() const {
        return mPattern;
    }

    nsIPrincipal* Principal() const { return mPrincipal; }
    bool GetForceWriteOnly() const { return mForceWriteOnly; }
    bool GetCORSUsed() const { return mCORSUsed; }

    NS_DECL_ISUPPORTS

protected:
    nsRefPtr<gfxPattern> mPattern;
    nsCOMPtr<nsIPrincipal> mPrincipal;
    const bool mForceWriteOnly;
    const bool mCORSUsed;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCanvasPattern, NS_CANVASPATTERN_PRIVATE_IID)

NS_IMPL_ADDREF(nsCanvasPattern)
NS_IMPL_RELEASE(nsCanvasPattern)

DOMCI_DATA(CanvasPattern, nsCanvasPattern)

NS_INTERFACE_MAP_BEGIN(nsCanvasPattern)
  NS_INTERFACE_MAP_ENTRY(nsCanvasPattern)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasPattern)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CanvasPattern)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/**
 ** nsTextMetrics
 **/
#define NS_TEXTMETRICS_PRIVATE_IID \
    { 0xc5b1c2f9, 0xcb4f, 0x4394, { 0xaf, 0xe0, 0xc6, 0x59, 0x33, 0x80, 0x8b, 0xf3 } }
class nsTextMetrics : public nsIDOMTextMetrics
{
public:
    nsTextMetrics(float w) : width(w) { }

    virtual ~nsTextMetrics() { }

    NS_DECLARE_STATIC_IID_ACCESSOR(NS_TEXTMETRICS_PRIVATE_IID)

    NS_IMETHOD GetWidth(float* w) {
        *w = width;
        return NS_OK;
    }

    NS_DECL_ISUPPORTS

private:
    float width;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsTextMetrics, NS_TEXTMETRICS_PRIVATE_IID)

NS_IMPL_ADDREF(nsTextMetrics)
NS_IMPL_RELEASE(nsTextMetrics)

DOMCI_DATA(TextMetrics, nsTextMetrics)

NS_INTERFACE_MAP_BEGIN(nsTextMetrics)
  NS_INTERFACE_MAP_ENTRY(nsTextMetrics)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTextMetrics)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(TextMetrics)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

struct nsCanvasBidiProcessor;

/**
 ** nsCanvasRenderingContext2D
 **/
class nsCanvasRenderingContext2D :
    public nsIDOMCanvasRenderingContext2D,
    public nsICanvasRenderingContextInternal
{
public:
    nsCanvasRenderingContext2D();
    virtual ~nsCanvasRenderingContext2D();

    nsresult Redraw();

    // nsICanvasRenderingContextInternal
    NS_IMETHOD SetCanvasElement(nsHTMLCanvasElement* aParentCanvas);
    NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
    void Initialize(nsIDocShell *shell, PRInt32 width, PRInt32 height);
    NS_IMETHOD InitializeWithSurface(nsIDocShell *shell, gfxASurface *surface, PRInt32 width, PRInt32 height);
    bool EnsureSurface();
    NS_IMETHOD Render(gfxContext *ctx, gfxPattern::GraphicsFilter aFilter);
    NS_IMETHOD GetInputStream(const char* aMimeType,
                              const PRUnichar* aEncoderOptions,
                              nsIInputStream **aStream);
    NS_IMETHOD GetThebesSurface(gfxASurface **surface);
    mozilla::TemporaryRef<mozilla::gfx::SourceSurface> GetSurfaceSnapshot()
        { return nsnull; }

    NS_IMETHOD SetIsOpaque(bool isOpaque);
    NS_IMETHOD Reset();
    virtual already_AddRefed<CanvasLayer> GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                                         CanvasLayer *aOldLayer,
                                                         LayerManager *aManager);
    virtual bool ShouldForceInactiveLayer(LayerManager *aManager);
    virtual void MarkContextClean();
    NS_IMETHOD SetIsIPC(bool isIPC);
    // this rect is in canvas device space
    NS_IMETHOD Redraw(const gfxRect &r);
    // this rect is in mThebes's current user space
    NS_IMETHOD RedrawUser(const gfxRect &r);

    // nsISupports interface + CC
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS

    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsCanvasRenderingContext2D, nsIDOMCanvasRenderingContext2D)

    // nsIDOMCanvasRenderingContext2D interface
    NS_DECL_NSIDOMCANVASRENDERINGCONTEXT2D

    enum Style {
        STYLE_STROKE = 0,
        STYLE_FILL,
        STYLE_SHADOW,
        STYLE_MAX
    };

    class PathAutoSaveRestore
    {
    public:
        PathAutoSaveRestore(nsCanvasRenderingContext2D* aCtx) :
          mContext(aCtx->mThebes)
        {
            if (aCtx->mHasPath) {
                mPath = mContext->CopyPath();
            }
        }
        ~PathAutoSaveRestore()
        {
            mContext->NewPath();
            if (mPath) {
                mContext->AppendPath(mPath);
            }
        }
    private:
        gfxContext *mContext;
        nsRefPtr<gfxPath> mPath;
    };
    friend class PathAutoSaveRestore;

protected:
    nsresult GetImageDataArray(JSContext* aCx, int32_t aX, int32_t aY,
                               uint32_t aWidth, uint32_t aHeight,
                               JSObject** aRetval);

    /**
     * The number of living nsCanvasRenderingContexts.  When this goes down to
     * 0, we free the premultiply and unpremultiply tables, if they exist.
     */
    static PRUint32 sNumLivingContexts;

    /**
     * Lookup table used to speed up GetImageData().
     */
    static PRUint8 (*sUnpremultiplyTable)[256];

    /**
     * Lookup table used to speed up PutImageData().
     */
    static PRUint8 (*sPremultiplyTable)[256];

    // Some helpers.  Doesn't modify acolor on failure.
    nsresult SetStyleFromStringOrInterface(const nsAString& aStr, nsISupports *aInterface, Style aWhichStyle);
    nsresult GetStyleAsStringOrInterface(nsAString& aStr, nsISupports **aInterface, PRInt32 *aType, Style aWhichStyle);

    void StyleColorToString(const nscolor& aColor, nsAString& aStr);

    void DirtyAllStyles();
    /**
     * applies the given style as the current source. If the given style is
     * a solid color, aUseGlobalAlpha indicates whether to multiply the alpha
     * by global alpha, and is ignored otherwise.
     */
    void ApplyStyle(Style aWhichStyle, bool aUseGlobalAlpha = true);

    /**
     * Creates the unpremultiply lookup table, if it doesn't exist.
     */
    void EnsureUnpremultiplyTable();

    /**
     * Creates the premultiply lookup table, if it doesn't exist.
     */
    void EnsurePremultiplyTable();

    /**
     * Returns the image format this canvas should be allocated using. Takes
     * into account mOpaque, platform requirements, etc.
     */
    gfxASurface::gfxImageFormat GetImageFormat() const;

    // Member vars
    PRInt32 mWidth, mHeight;
    bool mValid;
    bool mZero;
    bool mOpaque;
    bool mResetLayer;
    bool mIPC;

    // the canvas element we're a context of
    nsCOMPtr<nsIDOMHTMLCanvasElement> mCanvasElement;
    nsHTMLCanvasElement *HTMLCanvasElement() {
        return static_cast<nsHTMLCanvasElement*>(mCanvasElement.get());
    }

    // Initialize the Thebes rendering context
    void CreateThebes();

    // If mCanvasElement is not provided, then a docshell is
    nsCOMPtr<nsIDocShell> mDocShell;

    // our drawing surfaces, contexts, and layers
    nsRefPtr<gfxContext> mThebes;
    nsRefPtr<gfxASurface> mSurface;
    bool mSurfaceCreated;

    PRUint32 mSaveCount;

    /**
     * Flag to avoid duplicate calls to InvalidateFrame. Set to true whenever
     * Redraw is called, reset to false when Render is called.
     */
    bool mIsEntireFrameInvalid;
    /**
     * When this is set, the first call to Redraw(gfxRect) should set
     * mIsEntireFrameInvalid since we expect it will be followed by
     * many more Redraw calls.
     */
    bool mPredictManyRedrawCalls;
    /**
     * This is set whenever there's a nonempty path set by the API user.
     */
    bool mHasPath;

    /**
     * Number of times we've invalidated before calling redraw
     */
    PRUint32 mInvalidateCount;
    static const PRUint32 kCanvasMaxInvalidateCount = 100;

    /**
     * Returns true iff the the given operator should affect areas of the
     * destination where the source is transparent. Among other things, this
     * implies that a fully transparent source would still affect the canvas.
     */
    bool OperatorAffectsUncoveredAreas(gfxContext::GraphicsOperator op) const
    {
        return op == gfxContext::OPERATOR_IN ||
               op == gfxContext::OPERATOR_OUT ||
               op == gfxContext::OPERATOR_DEST_IN ||
               op == gfxContext::OPERATOR_DEST_ATOP;
    }

    /**
     * Returns true iff a shadow should be drawn along with a
     * drawing operation.
     */
    bool NeedToDrawShadow()
    {
        ContextState& state = CurrentState();

        // The spec says we should not draw shadows when the alpha value is 0,
        // regardless of the operator being used.
        return state.StyleIsColor(STYLE_SHADOW) &&
               NS_GET_A(state.colorStyles[STYLE_SHADOW]) > 0 &&
               (state.shadowOffset != gfxPoint(0, 0) || state.shadowBlur != 0);
    }

    /**
     * Checks the current state to determine if an intermediate surface would
     * be necessary to complete a drawing operation. Does not check the
     * condition pertaining to global alpha and patterns since that does not
     * pertain to all drawing operations.
     */
    bool NeedToUseIntermediateSurface()
    {
        if (!mThebes) {
            // Haven't created a surface yet, default is OVER.
            return OperatorAffectsUncoveredAreas(gfxContext::OPERATOR_OVER);
        }
     
        // certain operators always need an intermediate surface, except
        // with quartz since quartz does compositing differently than cairo
        return OperatorAffectsUncoveredAreas(mThebes->CurrentOperator());

        // XXX there are other unhandled cases but they should be investigated
        // first to ensure we aren't using an intermediate surface unecessarily
    }

    /**
     * If the current operator is "source" then clear the destination before we
     * draw into it, to simulate the effect of an unbounded source operator.
     */
    void ClearSurfaceForUnboundedSource()
    {
        if (!mThebes) {
            // Haven't created a surface yet, default is OVER.
            return;
        }

        gfxContext::GraphicsOperator current = mThebes->CurrentOperator();
        if (current != gfxContext::OPERATOR_SOURCE)
            return;
        mThebes->SetOperator(gfxContext::OPERATOR_CLEAR);
        // It doesn't really matter what the source is here, since Paint
        // isn't bounded by the source and the mask covers the entire clip
        // region.
        mThebes->Paint();
        mThebes->SetOperator(current);
    }

    /**
     * Returns true iff the current source is such that global alpha would not
     * be handled correctly without the use of an intermediate surface.
     */
    bool NeedIntermediateSurfaceToHandleGlobalAlpha(Style aWhichStyle)
    {
        return CurrentState().globalAlpha != 1.0 && !CurrentState().StyleIsColor(aWhichStyle);
    }

    /**
     * Initializes the drawing of a shadow onto the canvas. The returned context
     * should have the shadow shape drawn onto it, and then ShadowFinalize
     * should be called. The return value is null if an error occurs.
     * @param extents The extents of the shadow object, in device space.
     * @param blur A newly contructed gfxAlphaBoxBlur, made with the default
     *  constructor and left uninitialized.
     * @remark The lifetime of the return value is tied to the lifetime of
     *  the gfxAlphaBoxBlur, so it does not need to be ref counted.
     */
    gfxContext* ShadowInitialize(const gfxRect& extents, gfxAlphaBoxBlur& blur);

    /**
     * Completes a shadow drawing operation.
     * @param blur The gfxAlphaBoxBlur that was passed to ShadowInitialize.
     */
    void ShadowFinalize(gfxAlphaBoxBlur& blur);

    /**
     * Draws the current path in the given style. Takes care of
     * any shadow drawing and will use intermediate surfaces as needed.
     *
     * If dirtyRect is given, it will contain the user-space dirty
     * rectangle of the draw operation.
     */
    nsresult DrawPath(Style style, gfxRect *dirtyRect = nsnull);

    /**
     * Draws a rectangle in the given style; used by FillRect and StrokeRect.
     */
    nsresult DrawRect(const gfxRect& rect, Style style);

    /**
     * Gets the pres shell from either the canvas element or the doc shell
     */
    nsIPresShell *GetPresShell() {
      nsCOMPtr<nsIContent> content = do_QueryObject(mCanvasElement);
      if (content) {
        return content->OwnerDoc()->GetShell();
      }
      if (mDocShell) {
        nsCOMPtr<nsIPresShell> shell;
        mDocShell->GetPresShell(getter_AddRefs(shell));
        return shell.get();
      }
      return nsnull;
    }

    // text
    enum TextAlign {
        TEXT_ALIGN_START,
        TEXT_ALIGN_END,
        TEXT_ALIGN_LEFT,
        TEXT_ALIGN_RIGHT,
        TEXT_ALIGN_CENTER
    };

    enum TextBaseline {
        TEXT_BASELINE_TOP,
        TEXT_BASELINE_HANGING,
        TEXT_BASELINE_MIDDLE,
        TEXT_BASELINE_ALPHABETIC,
        TEXT_BASELINE_IDEOGRAPHIC,
        TEXT_BASELINE_BOTTOM
    };

    gfxFontGroup* GetCurrentFontStyle();
    gfxTextRun* MakeTextRun(const PRUnichar* aText,
                            PRUint32         aLength,
                            PRUint32         aAppUnitsPerDevUnit,
                            PRUint32         aFlags);

    enum TextDrawOperation {
        TEXT_DRAW_OPERATION_FILL,
        TEXT_DRAW_OPERATION_STROKE,
        TEXT_DRAW_OPERATION_MEASURE
    };

    /*
     * Implementation of the fillText, strokeText, and measure functions with
     * the operation abstracted to a flag.
     */
    nsresult DrawOrMeasureText(const nsAString& text,
                               float x,
                               float y,
                               float maxWidth,
                               TextDrawOperation op,
                               float* aWidth);

    // style handling
    /*
     * The previous set style. Is equal to STYLE_MAX when there is no valid
     * previous style.
     */
    Style mLastStyle;
    bool mDirtyStyle[STYLE_MAX];

    // state stack handling
    class ContextState {
    public:
        ContextState() : shadowOffset(0.0, 0.0),
                         globalAlpha(1.0),
                         shadowBlur(0.0),
                         textAlign(TEXT_ALIGN_START),
                         textBaseline(TEXT_BASELINE_ALPHABETIC),
                         imageSmoothingEnabled(true)
        { }

        ContextState(const ContextState& other)
            : shadowOffset(other.shadowOffset),
              globalAlpha(other.globalAlpha),
              shadowBlur(other.shadowBlur),
              font(other.font),
              fontGroup(other.fontGroup),
              textAlign(other.textAlign),
              textBaseline(other.textBaseline),
              imageSmoothingEnabled(other.imageSmoothingEnabled)
        {
            for (int i = 0; i < STYLE_MAX; i++) {
                colorStyles[i] = other.colorStyles[i];
                gradientStyles[i] = other.gradientStyles[i];
                patternStyles[i] = other.patternStyles[i];
            }
        }

        inline void SetColorStyle(Style whichStyle, nscolor color) {
            colorStyles[whichStyle] = color;
            gradientStyles[whichStyle] = nsnull;
            patternStyles[whichStyle] = nsnull;
        }

        inline void SetPatternStyle(Style whichStyle, nsCanvasPattern* pat) {
            gradientStyles[whichStyle] = nsnull;
            patternStyles[whichStyle] = pat;
        }

        inline void SetGradientStyle(Style whichStyle, nsCanvasGradient* grad) {
            gradientStyles[whichStyle] = grad;
            patternStyles[whichStyle] = nsnull;
        }

        /**
         * returns true iff the given style is a solid color.
         */
        inline bool StyleIsColor(Style whichStyle) const
        {
            return !(patternStyles[whichStyle] ||
                     gradientStyles[whichStyle]);
        }

        gfxPoint shadowOffset;
        float globalAlpha;
        float shadowBlur;

        nsString font;
        nsRefPtr<gfxFontGroup> fontGroup;
        TextAlign textAlign;
        TextBaseline textBaseline;

        nscolor colorStyles[STYLE_MAX];
        nsCOMPtr<nsCanvasGradient> gradientStyles[STYLE_MAX];
        nsCOMPtr<nsCanvasPattern> patternStyles[STYLE_MAX];

        bool imageSmoothingEnabled;
    };

    nsTArray<ContextState> mStyleStack;

    inline ContextState& CurrentState() {
        return mStyleStack[mSaveCount];
    }

    // other helpers
    void GetAppUnitsValues(PRUint32 *perDevPixel, PRUint32 *perCSSPixel) {
        // If we don't have a canvas element, we just return something generic.
        PRUint32 devPixel = 60;
        PRUint32 cssPixel = 60;

        nsIPresShell *ps = GetPresShell();
        nsPresContext *pc;

        if (!ps) goto FINISH;
        pc = ps->GetPresContext();
        if (!pc) goto FINISH;
        devPixel = pc->AppUnitsPerDevPixel();
        cssPixel = pc->AppUnitsPerCSSPixel();

      FINISH:
        if (perDevPixel)
            *perDevPixel = devPixel;
        if (perCSSPixel)
            *perCSSPixel = cssPixel;
    }

    friend struct nsCanvasBidiProcessor;
};

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCanvasRenderingContext2D)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCanvasRenderingContext2D)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsCanvasRenderingContext2D)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsCanvasRenderingContext2D)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCanvasElement)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsCanvasRenderingContext2D)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCanvasElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(CanvasRenderingContext2D, nsCanvasRenderingContext2D)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCanvasRenderingContext2D)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasRenderingContext2D)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCanvasRenderingContext2D)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CanvasRenderingContext2D)
NS_INTERFACE_MAP_END

/**
 ** CanvasRenderingContext2D impl
 **/


// Initialize our static variables.
PRUint32 nsCanvasRenderingContext2D::sNumLivingContexts = 0;
PRUint8 (*nsCanvasRenderingContext2D::sUnpremultiplyTable)[256] = nsnull;
PRUint8 (*nsCanvasRenderingContext2D::sPremultiplyTable)[256] = nsnull;

nsresult
NS_NewCanvasRenderingContext2DThebes(nsIDOMCanvasRenderingContext2D** aResult)
{
    nsRefPtr<nsIDOMCanvasRenderingContext2D> ctx = new nsCanvasRenderingContext2D();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = ctx.forget().get();
    return NS_OK;
}

nsCanvasRenderingContext2D::nsCanvasRenderingContext2D()
    : mValid(false), mZero(false), mOpaque(false), mResetLayer(true)
    , mIPC(false)
    , mCanvasElement(nsnull)
    , mSaveCount(0), mIsEntireFrameInvalid(false)
    , mPredictManyRedrawCalls(false), mHasPath(false), mInvalidateCount(0)
    , mLastStyle(STYLE_MAX), mStyleStack(20)
{
    sNumLivingContexts++;
}

nsCanvasRenderingContext2D::~nsCanvasRenderingContext2D()
{
    Reset();
    sNumLivingContexts--;
    if (!sNumLivingContexts) {
        delete[] sUnpremultiplyTable;
        delete[] sPremultiplyTable;
        sUnpremultiplyTable = nsnull;
        sPremultiplyTable = nsnull;
    }
}

nsresult
nsCanvasRenderingContext2D::Reset()
{
    if (mCanvasElement) {
        HTMLCanvasElement()->InvalidateCanvas();
    }

    // only do this for non-docshell created contexts,
    // since those are the ones that we created a surface for
    if (mValid && !mDocShell && mSurface)
        gCanvasMemoryUsed -= mWidth * mHeight * 4;

    mSurface = nsnull;
    mThebes = nsnull;
    mValid = false;
    mIsEntireFrameInvalid = false;
    mPredictManyRedrawCalls = false;
    return NS_OK;
}

nsresult
nsCanvasRenderingContext2D::SetStyleFromStringOrInterface(const nsAString& aStr,
                                                          nsISupports *aInterface,
                                                          Style aWhichStyle)
{
    nsresult rv;
    nscolor color;

    if (!aStr.IsVoid()) {
        nsIDocument* document = mCanvasElement ?
                                HTMLCanvasElement()->OwnerDoc() : nsnull;

        // Pass the CSS Loader object to the parser, to allow parser error
        // reports to include the outer window ID.
        nsCSSParser parser(document ? document->CSSLoader() : nsnull);
        rv = parser.ParseColorString(aStr, nsnull, 0, &color);
        if (NS_FAILED(rv)) {
            // Error reporting happens inside the CSS parser
            return NS_OK;
        }

        CurrentState().SetColorStyle(aWhichStyle, color);

        mDirtyStyle[aWhichStyle] = true;
        return NS_OK;
    }

    if (aInterface) {
        nsCOMPtr<nsCanvasGradient> grad(do_QueryInterface(aInterface));
        if (grad) {
            CurrentState().SetGradientStyle(aWhichStyle, grad);
            mDirtyStyle[aWhichStyle] = true;
            return NS_OK;
        }

        nsCOMPtr<nsCanvasPattern> pattern(do_QueryInterface(aInterface));
        if (pattern) {
            CurrentState().SetPatternStyle(aWhichStyle, pattern);
            mDirtyStyle[aWhichStyle] = true;
            return NS_OK;
        }
    }

    nsContentUtils::ReportToConsole(
        nsIScriptError::warningFlag,
        "Canvas",
        mCanvasElement ? HTMLCanvasElement()->OwnerDoc() : nsnull,
        nsContentUtils::eDOM_PROPERTIES,
        "UnexpectedCanvasVariantStyle");

    return NS_OK;
}

nsresult
nsCanvasRenderingContext2D::GetStyleAsStringOrInterface(nsAString& aStr,
                                                        nsISupports **aInterface,
                                                        PRInt32 *aType,
                                                        Style aWhichStyle)
{
    if (CurrentState().patternStyles[aWhichStyle]) {
        aStr.SetIsVoid(true);
        NS_ADDREF(*aInterface = CurrentState().patternStyles[aWhichStyle]);
        *aType = CMG_STYLE_PATTERN;
    } else if (CurrentState().gradientStyles[aWhichStyle]) {
        aStr.SetIsVoid(true);
        NS_ADDREF(*aInterface = CurrentState().gradientStyles[aWhichStyle]);
        *aType = CMG_STYLE_GRADIENT;
    } else {
        StyleColorToString(CurrentState().colorStyles[aWhichStyle], aStr);
        *aInterface = nsnull;
        *aType = CMG_STYLE_STRING;
    }

    return NS_OK;
}

void
nsCanvasRenderingContext2D::StyleColorToString(const nscolor& aColor, nsAString& aStr)
{
    // We can't reuse the normal CSS color stringification code,
    // because the spec calls for a different algorithm for canvas.
    if (NS_GET_A(aColor) == 255) {
        CopyUTF8toUTF16(nsPrintfCString(100, "#%02x%02x%02x",
                                        NS_GET_R(aColor),
                                        NS_GET_G(aColor),
                                        NS_GET_B(aColor)),
                        aStr);
    } else {
        CopyUTF8toUTF16(nsPrintfCString(100, "rgba(%d, %d, %d, ",
                                        NS_GET_R(aColor),
                                        NS_GET_G(aColor),
                                        NS_GET_B(aColor)),
                        aStr);
        aStr.AppendFloat(nsStyleUtil::ColorComponentToFloat(NS_GET_A(aColor)));
        aStr.Append(')');
    }
}

void
nsCanvasRenderingContext2D::DirtyAllStyles()
{
    for (int i = 0; i < STYLE_MAX; i++) {
        mDirtyStyle[i] = true;
    }
}

void
nsCanvasRenderingContext2D::ApplyStyle(Style aWhichStyle,
                                       bool aUseGlobalAlpha)
{
    if (mLastStyle == aWhichStyle &&
        !mDirtyStyle[aWhichStyle] &&
        aUseGlobalAlpha)
    {
        // nothing to do, this is already the set style
        return;
    }

    if (!EnsureSurface()) {
      return;
    }

    // if not using global alpha, don't optimize with dirty bit
    if (aUseGlobalAlpha)
        mDirtyStyle[aWhichStyle] = false;
    mLastStyle = aWhichStyle;

    nsCanvasPattern* pattern = CurrentState().patternStyles[aWhichStyle];
    if (pattern) {
        if (mCanvasElement)
            CanvasUtils::DoDrawImageSecurityCheck(HTMLCanvasElement(),
                                                  pattern->Principal(),
                                                  pattern->GetForceWriteOnly(),
                                                  pattern->GetCORSUsed());

        gfxPattern* gpat = pattern->GetPattern();

        if (CurrentState().imageSmoothingEnabled)
            gpat->SetFilter(gfxPattern::FILTER_GOOD);
        else
            gpat->SetFilter(gfxPattern::FILTER_NEAREST);

        mThebes->SetPattern(gpat);
        return;
    }

    if (CurrentState().gradientStyles[aWhichStyle]) {
        gfxPattern* gpat = CurrentState().gradientStyles[aWhichStyle]->GetPattern();
        mThebes->SetPattern(gpat);
        return;
    }

    gfxRGBA color(CurrentState().colorStyles[aWhichStyle]);
    if (aUseGlobalAlpha)
        color.a *= CurrentState().globalAlpha;

    mThebes->SetColor(color);
}

nsresult
nsCanvasRenderingContext2D::Redraw()
{
    if (mIsEntireFrameInvalid)
        return NS_OK;
    mIsEntireFrameInvalid = true;

    if (!mCanvasElement) {
        NS_ASSERTION(mDocShell, "Redraw with no canvas element or docshell!");
        return NS_OK;
    }

    nsSVGEffects::InvalidateDirectRenderingObservers(HTMLCanvasElement());

    HTMLCanvasElement()->InvalidateCanvasContent(nsnull);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Redraw(const gfxRect& r)
{
    ++mInvalidateCount;

    if (mIsEntireFrameInvalid)
        return NS_OK;

    if (mPredictManyRedrawCalls ||
        mInvalidateCount > kCanvasMaxInvalidateCount) {
        return Redraw();
    }

    if (!mCanvasElement) {
        NS_ASSERTION(mDocShell, "Redraw with no canvas element or docshell!");
        return NS_OK;
    }

    nsSVGEffects::InvalidateDirectRenderingObservers(HTMLCanvasElement());

    HTMLCanvasElement()->InvalidateCanvasContent(&r);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::RedrawUser(const gfxRect& r)
{
    if (mIsEntireFrameInvalid) {
        ++mInvalidateCount;
        return NS_OK;
    }

    return Redraw(mThebes->UserToDevice(r));
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetDimensions(PRInt32 width, PRInt32 height)
{
    Initialize(NULL, width, height);
    return NS_OK;
}

void
nsCanvasRenderingContext2D::Initialize(nsIDocShell *docShell, PRInt32 width, PRInt32 height) 
{
    Reset();

    NS_ASSERTION(!docShell ^ !mCanvasElement, "Cannot set both docshell and canvas element");
    mDocShell = docShell;

    mWidth = width;
    mHeight = height;

    mResetLayer = true;
    mValid = true;
    mSurfaceCreated = false;

    // set up the initial canvas defaults
    mStyleStack.Clear();
    mSaveCount = 0;

    ContextState *state = mStyleStack.AppendElement();
    state->globalAlpha = 1.0;

    state->colorStyles[STYLE_FILL] = NS_RGB(0,0,0);
    state->colorStyles[STYLE_STROKE] = NS_RGB(0,0,0);
    state->colorStyles[STYLE_SHADOW] = NS_RGBA(0,0,0,0);
    DirtyAllStyles();

    // always force a redraw, because if the surface dimensions were reset
    // then the surface became cleared, and we need to redraw everything.
    Redraw();

    return;
}

void
nsCanvasRenderingContext2D::CreateThebes()
{
    mThebes = new gfxContext(mSurface);
    mSurfaceCreated = true;
    
    mThebes->SetOperator(gfxContext::OPERATOR_CLEAR);
    mThebes->NewPath();
    mThebes->Rectangle(gfxRect(0, 0, mWidth, mHeight));
    mThebes->Fill();

    mThebes->SetLineWidth(1.0);
    mThebes->SetOperator(gfxContext::OPERATOR_OVER);
    mThebes->SetMiterLimit(10.0);
    mThebes->SetLineCap(gfxContext::LINE_CAP_BUTT);
    mThebes->SetLineJoin(gfxContext::LINE_JOIN_MITER);

    mThebes->NewPath();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::InitializeWithSurface(nsIDocShell *docShell, 
                                                  gfxASurface *surface, 
                                                  PRInt32 width, 
                                                  PRInt32 height)
{
    Initialize(docShell, width, height);
    
    mSurface = surface;
    CreateThebes();
    return mValid ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

bool
nsCanvasRenderingContext2D::EnsureSurface()
{
    if (!mValid) {
        return false;
    }

    if (mSurface && mThebes && mSurfaceCreated) {
        if (mSurface->CairoStatus()) {
            return false;
        }
        return true;
    }
    
    nsRefPtr<gfxASurface> surface;

    // Check that the dimensions are sane
    if (gfxASurface::CheckSurfaceSize(gfxIntSize(mWidth, mHeight), 0xffff)) {
        // Zero sized surfaces have problems, so just use a 1 by 1.
        if (mHeight == 0 || mWidth == 0) {
            mZero = true;
            mHeight = 1;
            mWidth = 1;
        } else {
            mZero = false;
        }

        gfxASurface::gfxImageFormat format = GetImageFormat();

        if (!PR_GetEnv("MOZ_CANVAS_IMAGE_SURFACE")) {
            nsCOMPtr<nsIContent> content = do_QueryObject(mCanvasElement);
            nsIDocument* ownerDoc = nsnull;
            if (content)
                ownerDoc = content->OwnerDoc();
            nsRefPtr<LayerManager> layerManager = nsnull;

            if (ownerDoc)
              layerManager =
                nsContentUtils::PersistentLayerManagerForDocument(ownerDoc);

            if (layerManager) {
              surface = layerManager->CreateOptimalSurface(gfxIntSize(mWidth, mHeight), format);
            } else {
              surface = gfxPlatform::GetPlatform()->
                CreateOffscreenSurface(gfxIntSize(mWidth, mHeight), gfxASurface::ContentFromFormat(format));
            }
        }

        if (!surface || surface->CairoStatus()) {
            // If we couldn't create a surface of the type we want, fall back
            // to an image surface. This lets us handle surface sizes that
            // the underlying cairo backend might not handle.
            surface = new gfxImageSurface(gfxIntSize(mWidth, mHeight), format);
            if (!surface || surface->CairoStatus()) {
                surface = nsnull;
            }
        }
    }
    if (surface) {
        if (gCanvasMemoryReporter == nsnull) {
            gCanvasMemoryReporter = new NS_MEMORY_REPORTER_NAME(CanvasMemory);
            NS_RegisterMemoryReporter(gCanvasMemoryReporter);
        }

        gCanvasMemoryUsed += mWidth * mHeight * 4;
        JSContext* context = nsContentUtils::GetCurrentJSContext();
        if (context) {
            JS_updateMallocCounter(context, mWidth * mHeight * 4);
        }
    } else {
        return false;
    }

    mSurface = surface;
    CreateThebes();

    if (mSurface->CairoStatus()) {
        return false;
    }
    return true;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetIsOpaque(bool isOpaque)
{
    if (isOpaque == mOpaque)
        return NS_OK;

    mOpaque = isOpaque;

    if (mValid) {
        /* If we've already been created, let SetDimensions take care of
         * recreating our surface
         */
        return SetDimensions(mWidth, mHeight);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetIsIPC(bool isIPC)
{
    if (isIPC == mIPC)
        return NS_OK;

    mIPC = isIPC;

    if (mValid) {
        /* If we've already been created, let SetDimensions take care of
         * recreating our surface
         */
        return SetDimensions(mWidth, mHeight);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Render(gfxContext *ctx, gfxPattern::GraphicsFilter aFilter)
{
    nsresult rv = NS_OK;

    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxPattern> pat = new gfxPattern(mSurface);

    pat->SetFilter(aFilter);
    pat->SetExtend(gfxPattern::EXTEND_PAD);

    gfxContext::GraphicsOperator op = ctx->CurrentOperator();
    if (mOpaque)
        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);

    // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
    // pixel alignment for this stuff!
    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
    ctx->Fill();

    if (mOpaque)
        ctx->SetOperator(op);

    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetInputStream(const char *aMimeType,
                                           const PRUnichar *aEncoderOptions,
                                           nsIInputStream **aStream)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    nsresult rv;
    const char encoderPrefix[] = "@mozilla.org/image/encoder;2?type=";
    nsAutoArrayPtr<char> conid(new (std::nothrow) char[strlen(encoderPrefix) + strlen(aMimeType) + 1]);

    if (!conid)
        return NS_ERROR_OUT_OF_MEMORY;

    strcpy(conid, encoderPrefix);
    strcat(conid, aMimeType);

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(conid);
    if (!encoder)
        return NS_ERROR_FAILURE;

    nsAutoArrayPtr<PRUint8> imageBuffer(new (std::nothrow) PRUint8[mWidth * mHeight * 4]);
    if (!imageBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<gfxImageSurface> imgsurf = new gfxImageSurface(imageBuffer.get(),
                                                            gfxIntSize(mWidth, mHeight),
                                                            mWidth * 4,
                                                            gfxASurface::ImageFormatARGB32);

    if (!imgsurf || imgsurf->CairoStatus())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxContext> ctx = new gfxContext(imgsurf);

    if (!ctx || ctx->HasError())
        return NS_ERROR_FAILURE;

    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->SetSource(mSurface, gfxPoint(0, 0));
    ctx->Paint();

    rv = encoder->InitFromData(imageBuffer.get(),
                               mWidth * mHeight * 4, mWidth, mHeight, mWidth * 4,
                               imgIEncoder::INPUT_FORMAT_HOSTARGB,
                               nsDependentString(aEncoderOptions));
    NS_ENSURE_SUCCESS(rv, rv);

    return CallQueryInterface(encoder, aStream);
}

gfxASurface::gfxImageFormat
nsCanvasRenderingContext2D::GetImageFormat() const
{
    gfxASurface::gfxImageFormat format = gfxASurface::ImageFormatARGB32;

    if (mOpaque)
        format = gfxASurface::ImageFormatRGB24;

    return format;
}

//
// nsCanvasRenderingContext2D impl
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetCanvasElement(nsHTMLCanvasElement* aCanvasElement)
{
    mCanvasElement = aCanvasElement;

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetCanvas(nsIDOMHTMLCanvasElement **canvas)
{
    NS_IF_ADDREF(*canvas = mCanvasElement);

    return NS_OK;
}

//
// state
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::Save()
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    ContextState state = CurrentState();
    mStyleStack.AppendElement(state);
    mThebes->Save();
    mSaveCount++;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Restore()
{
    if (!EnsureSurface()) 
        return NS_ERROR_FAILURE;

    if (mSaveCount == 0)
        return NS_OK;

    mStyleStack.RemoveElementAt(mSaveCount);
    mThebes->Restore();

    mLastStyle = STYLE_MAX;
    DirtyAllStyles();

    mSaveCount--;
    return NS_OK;
}

//
// transformations
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::Scale(float x, float y)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(x,y))
        return NS_OK;

    mThebes->Scale(x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Rotate(float angle)
{
    if (!EnsureSurface()) 
        return NS_ERROR_FAILURE;

    if (!FloatValidate(angle))
        return NS_OK;

    mThebes->Rotate(angle);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Translate(float x, float y)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(x,y))
        return NS_OK;

    mThebes->Translate(gfxPoint(x, y));
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(m11,m12,m21,m22,dx,dy))
        return NS_OK;

    gfxMatrix matrix(m11, m12, m21, m22, dx, dy);
    mThebes->Multiply(matrix);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(m11,m12,m21,m22,dx,dy))
        return NS_OK;

    gfxMatrix matrix(m11, m12, m21, m22, dx, dy);
    mThebes->SetMatrix(matrix);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetMozCurrentTransform(JSContext* cx,
                                                   const jsval& matrix)
{
    nsresult rv;
    gfxMatrix newCTM;
    
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!JSValToMatrix(cx, matrix, &newCTM, &rv)) {
        return rv;
    }

    mThebes->SetMatrix(newCTM);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMozCurrentTransform(JSContext* cx,
                                                   jsval* matrix)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    return MatrixToJSVal(mThebes->CurrentMatrix(), cx, matrix);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetMozCurrentTransformInverse(JSContext* cx,
                                                          const jsval& matrix)
{
    nsresult rv;
    gfxMatrix newCTMInverse;
    
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!JSValToMatrix(cx, matrix, &newCTMInverse, &rv)) {
        return rv;
    }

    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    if (!newCTMInverse.IsSingular()) {
        mThebes->SetMatrix(newCTMInverse.Invert());
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMozCurrentTransformInverse(JSContext* cx,
                                                          jsval* matrix)
{
    gfxMatrix ctm = mThebes->CurrentMatrix();

    if (!mThebes->CurrentMatrix().IsSingular()) {
        ctm.Invert();
    } else {
        double NaN = JSVAL_TO_DOUBLE(JS_GetNaNValue(cx));
        ctm = gfxMatrix(NaN, NaN, NaN, NaN, NaN, NaN);
    }

    return MatrixToJSVal(ctm, cx, matrix);
}

//
// colors
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetGlobalAlpha(float aGlobalAlpha)
{
    if (!FloatValidate(aGlobalAlpha) || aGlobalAlpha < 0.0 || aGlobalAlpha > 1.0)
        return NS_OK;

    CurrentState().globalAlpha = aGlobalAlpha;
    DirtyAllStyles();

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetGlobalAlpha(float *aGlobalAlpha)
{
    *aGlobalAlpha = CurrentState().globalAlpha;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetStrokeStyle(nsIVariant *aValue)
{
    if (!aValue)
        return NS_ERROR_FAILURE;

    nsString str;

    nsresult rv;
    PRUint16 vtype;
    rv = aValue->GetDataType(&vtype);
    NS_ENSURE_SUCCESS(rv, rv);

    if (vtype == nsIDataType::VTYPE_INTERFACE ||
        vtype == nsIDataType::VTYPE_INTERFACE_IS)
    {
        nsIID *iid;
        nsCOMPtr<nsISupports> sup;
        rv = aValue->GetAsInterface(&iid, getter_AddRefs(sup));
        NS_ENSURE_SUCCESS(rv, rv);
        if (iid)
            NS_Free(iid);

        str.SetIsVoid(true);
        return SetStrokeStyle_multi(str, sup);
    }

    rv = aValue->GetAsAString(str);
    NS_ENSURE_SUCCESS(rv, rv);

    return SetStrokeStyle_multi(str, nsnull);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetStrokeStyle(nsIVariant **aResult)
{
    nsCOMPtr<nsIWritableVariant> wv = do_CreateInstance(NS_VARIANT_CONTRACTID);

    nsCOMPtr<nsISupports> sup;
    nsString str;
    PRInt32 t;
    nsresult rv = GetStrokeStyle_multi(str, getter_AddRefs(sup), &t);
    NS_ENSURE_SUCCESS(rv, rv);

    if (t == CMG_STYLE_STRING) {
        rv = wv->SetAsAString(str);
    } else if (t == CMG_STYLE_PATTERN) {
        rv = wv->SetAsInterface(NS_GET_IID(nsIDOMCanvasPattern),
                                sup);
    } else if (t == CMG_STYLE_GRADIENT) {
        rv = wv->SetAsInterface(NS_GET_IID(nsIDOMCanvasGradient),
                                sup);
    } else {
        NS_ERROR("Unknown type from GetStroke/FillStyle_multi!");
        return NS_ERROR_FAILURE;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    NS_IF_ADDREF(*aResult = wv.get());
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetFillStyle(nsIVariant *aValue)
{
    if (!aValue)
        return NS_ERROR_FAILURE;

    nsString str;
    nsresult rv;
    PRUint16 vtype;
    rv = aValue->GetDataType(&vtype);
    NS_ENSURE_SUCCESS(rv, rv);

    if (vtype == nsIDataType::VTYPE_INTERFACE ||
        vtype == nsIDataType::VTYPE_INTERFACE_IS)
    {
        nsIID *iid;
        nsCOMPtr<nsISupports> sup;
        rv = aValue->GetAsInterface(&iid, getter_AddRefs(sup));
        NS_ENSURE_SUCCESS(rv, rv);

        str.SetIsVoid(true);
        return SetFillStyle_multi(str, sup);
    }

    rv = aValue->GetAsAString(str);
    NS_ENSURE_SUCCESS(rv, rv);

    return SetFillStyle_multi(str, nsnull);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetFillStyle(nsIVariant **aResult)
{
    nsCOMPtr<nsIWritableVariant> wv = do_CreateInstance(NS_VARIANT_CONTRACTID);

    nsCOMPtr<nsISupports> sup;
    nsString str;
    PRInt32 t;
    nsresult rv = GetFillStyle_multi(str, getter_AddRefs(sup), &t);
    NS_ENSURE_SUCCESS(rv, rv);

    if (t == CMG_STYLE_STRING) {
        rv = wv->SetAsAString(str);
    } else if (t == CMG_STYLE_PATTERN) {
        rv = wv->SetAsInterface(NS_GET_IID(nsIDOMCanvasPattern),
                                sup);
    } else if (t == CMG_STYLE_GRADIENT) {
        rv = wv->SetAsInterface(NS_GET_IID(nsIDOMCanvasGradient),
                                sup);
    } else {
        NS_ERROR("Unknown type from GetStroke/FillStyle_multi!");
        return NS_ERROR_FAILURE;
    }
    NS_ENSURE_SUCCESS(rv, rv);

    NS_IF_ADDREF(*aResult = wv.get());
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetStrokeStyle_multi(const nsAString& aStr, nsISupports *aInterface)
{
    return SetStyleFromStringOrInterface(aStr, aInterface, STYLE_STROKE);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetStrokeStyle_multi(nsAString& aStr, nsISupports **aInterface, PRInt32 *aType)
{
    return GetStyleAsStringOrInterface(aStr, aInterface, aType, STYLE_STROKE);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetFillStyle_multi(const nsAString& aStr, nsISupports *aInterface)
{
    return SetStyleFromStringOrInterface(aStr, aInterface, STYLE_FILL);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetFillStyle_multi(nsAString& aStr, nsISupports **aInterface, PRInt32 *aType)
{
    return GetStyleAsStringOrInterface(aStr, aInterface, aType, STYLE_FILL);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetMozFillRule(const nsAString& aString)
{
    gfxContext::FillRule rule;
    
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (aString.EqualsLiteral("evenodd"))
        rule = gfxContext::FILL_RULE_EVEN_ODD;
    else if (aString.EqualsLiteral("nonzero"))
        rule = gfxContext::FILL_RULE_WINDING;
    else
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_OK;

    mThebes->SetFillRule(rule);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMozFillRule(nsAString& aString)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    switch (mThebes->CurrentFillRule()) {
    case gfxContext::FILL_RULE_WINDING:
        aString.AssignLiteral("nonzero"); break;
    case gfxContext::FILL_RULE_EVEN_ODD:
        aString.AssignLiteral("evenodd"); break;
    }
    return NS_OK;
}

//
// gradients and patterns
//
NS_IMETHODIMP
nsCanvasRenderingContext2D::CreateLinearGradient(float x0, float y0, float x1, float y1,
                                                 nsIDOMCanvasGradient **_retval)
{
    if (!FloatValidate(x0,y0,x1,y1))
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

    nsRefPtr<gfxPattern> gradpat = new gfxPattern(x0, y0, x1, y1);
    if (!gradpat)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<nsIDOMCanvasGradient> grad = new nsCanvasGradient(gradpat);
    if (!grad)
        return NS_ERROR_OUT_OF_MEMORY;

    *_retval = grad.forget().get();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::CreateRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1,
                                                 nsIDOMCanvasGradient **_retval)
{
    if (!FloatValidate(x0,y0,r0,x1,y1,r1))
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;

    if (r0 < 0.0 || r1 < 0.0)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    nsRefPtr<gfxPattern> gradpat = new gfxPattern(x0, y0, r0, x1, y1, r1);
    if (!gradpat)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<nsIDOMCanvasGradient> grad = new nsCanvasGradient(gradpat);
    if (!grad)
        return NS_ERROR_OUT_OF_MEMORY;

    *_retval = grad.forget().get();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::CreatePattern(nsIDOMHTMLElement *image,
                                          const nsAString& repeat,
                                          nsIDOMCanvasPattern **_retval)
{
    nsCOMPtr<nsIContent> content = do_QueryInterface(image);
    if (!content) {
        return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
    }

    gfxPattern::GraphicsExtend extend;
    if (repeat.IsEmpty() || repeat.EqualsLiteral("repeat")) {
        extend = gfxPattern::EXTEND_REPEAT;
    } else if (repeat.EqualsLiteral("repeat-x")) {
        // XX
        extend = gfxPattern::EXTEND_REPEAT;
    } else if (repeat.EqualsLiteral("repeat-y")) {
        // XX
        extend = gfxPattern::EXTEND_REPEAT;
    } else if (repeat.EqualsLiteral("no-repeat")) {
        extend = gfxPattern::EXTEND_NONE;
    } else {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    nsHTMLCanvasElement* canvas = nsHTMLCanvasElement::FromContent(content);
    if (canvas) {
        nsIntSize size = canvas->GetSize();
        if (size.width == 0 || size.height == 0) {
            return NS_ERROR_DOM_INVALID_STATE_ERR;
        }
    }

    // The canvas spec says that createPattern should use the first frame
    // of animated images
    nsLayoutUtils::SurfaceFromElementResult res =
        nsLayoutUtils::SurfaceFromElement(content->AsElement(),
            nsLayoutUtils::SFE_WANT_FIRST_FRAME | nsLayoutUtils::SFE_WANT_NEW_SURFACE);
    if (!res.mSurface)
        return NS_ERROR_NOT_AVAILABLE;

    nsRefPtr<gfxPattern> thebespat = new gfxPattern(res.mSurface);

    thebespat->SetExtend(extend);

    nsRefPtr<nsCanvasPattern> pat = new nsCanvasPattern(thebespat, res.mPrincipal,
                                                        res.mIsWriteOnly,
                                                        res.mCORSUsed);
    *_retval = pat.forget().get();
    return NS_OK;
}

//
// shadows
//
NS_IMETHODIMP
nsCanvasRenderingContext2D::SetShadowOffsetX(float x)
{
    if (!FloatValidate(x))
        return NS_OK;

    CurrentState().shadowOffset.x = x;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowOffsetX(float *x)
{
    *x = static_cast<float>(CurrentState().shadowOffset.x);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetShadowOffsetY(float y)
{
    if (!FloatValidate(y))
        return NS_OK;

    CurrentState().shadowOffset.y = y;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowOffsetY(float *y)
{
    *y = static_cast<float>(CurrentState().shadowOffset.y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetShadowBlur(float blur)
{
    if (!FloatValidate(blur) || blur < 0.0)
        return NS_OK;

    CurrentState().shadowBlur = blur;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowBlur(float *blur)
{
    *blur = CurrentState().shadowBlur;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetShadowColor(const nsAString& colorstr)
{
    nsIDocument* document = mCanvasElement ?
                            HTMLCanvasElement()->OwnerDoc() : nsnull;

    // Pass the CSS Loader object to the parser, to allow parser error reports
    // to include the outer window ID.
    nsCSSParser parser(document ? document->CSSLoader() : nsnull);
    nscolor color;
    nsresult rv = parser.ParseColorString(colorstr, nsnull, 0, &color);
    if (NS_FAILED(rv)) {
        // Error reporting happens inside the CSS parser
        return NS_OK;
    }

    CurrentState().SetColorStyle(STYLE_SHADOW, color);

    mDirtyStyle[STYLE_SHADOW] = true;

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowColor(nsAString& color)
{
    StyleColorToString(CurrentState().colorStyles[STYLE_SHADOW], color);

    return NS_OK;
}

static const gfxFloat SIGMA_MAX = 100;

gfxContext*
nsCanvasRenderingContext2D::ShadowInitialize(const gfxRect& extents, gfxAlphaBoxBlur& blur)
{
    gfxIntSize blurRadius;

    float shadowBlur = CurrentState().shadowBlur;
    gfxFloat sigma = shadowBlur / 2;
    // limit to avoid overly huge temp images
    if (sigma > SIGMA_MAX)
        sigma = SIGMA_MAX;
    blurRadius = gfxAlphaBoxBlur::CalculateBlurRadius(gfxPoint(sigma, sigma));

    // calculate extents
    gfxRect drawExtents = extents;

    // intersect with clip to avoid making overly huge temp images
    gfxMatrix matrix = mThebes->CurrentMatrix();
    mThebes->IdentityMatrix();
    gfxRect clipExtents = mThebes->GetClipExtents();
    mThebes->SetMatrix(matrix);
    // outset by the blur radius so that blurs can leak onto the canvas even
    // when the shape is outside the clipping area
    clipExtents.Inflate(blurRadius.width, blurRadius.height);
    drawExtents = drawExtents.Intersect(clipExtents - CurrentState().shadowOffset);

    gfxContext* ctx = blur.Init(drawExtents, gfxIntSize(0,0), blurRadius, nsnull, nsnull);

    if (!ctx)
        return nsnull;

    return ctx;
}

void
nsCanvasRenderingContext2D::ShadowFinalize(gfxAlphaBoxBlur& blur)
{
    if (!EnsureSurface())
        return;

    ApplyStyle(STYLE_SHADOW);
    // canvas matrix was already applied, don't apply it twice, but do
    // apply the shadow offset
    gfxMatrix matrix = mThebes->CurrentMatrix();
    mThebes->IdentityMatrix();
    mThebes->Translate(CurrentState().shadowOffset);

    blur.Paint(mThebes);
    mThebes->SetMatrix(matrix);
}

nsresult
nsCanvasRenderingContext2D::DrawPath(Style style, gfxRect *dirtyRect)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    bool doUseIntermediateSurface = false;
    
    if (mSurface->GetType() == gfxASurface::SurfaceTypeD2D) {
      if (style != STYLE_FILL) {
        // D2D does all operators correctly even if transparent areas of SOURCE
        // affect dest. We need to use an intermediate surface for STROKE because
        // we can't clip to the actual stroke shape easily, but prefer a geometric
        // clip over an intermediate surface for a FILL.
        doUseIntermediateSurface = NeedIntermediateSurfaceToHandleGlobalAlpha(style);
      }
    } else {
      /*
       * Need an intermediate surface when:
       * - globalAlpha != 1 and gradients/patterns are used (need to paint_with_alpha)
       * - certain operators are used
       */
      doUseIntermediateSurface = NeedToUseIntermediateSurface() ||
                                 NeedIntermediateSurfaceToHandleGlobalAlpha(style);
    }

    bool doDrawShadow = NeedToDrawShadow();

    // Clear the surface if we need to simulate unbounded SOURCE operator
    ClearSurfaceForUnboundedSource();

    if (doDrawShadow) {
        gfxMatrix matrix = mThebes->CurrentMatrix();
        mThebes->IdentityMatrix();

        // calculate extents of path
        gfxRect drawExtents;
        if (style == STYLE_FILL)
            drawExtents = mThebes->GetUserFillExtent();
        else // STYLE_STROKE
            drawExtents = mThebes->GetUserStrokeExtent();

        mThebes->SetMatrix(matrix);

        gfxAlphaBoxBlur blur;

        // no need for a ref here, the blur owns the context
        gfxContext* ctx = ShadowInitialize(drawExtents, blur);
        if (ctx) {
            ApplyStyle(style, false);
            CopyContext(ctx, mThebes);
            ctx->SetOperator(gfxContext::OPERATOR_SOURCE);

            if (style == STYLE_FILL)
                ctx->Fill();
            else
                ctx->Stroke();

            ShadowFinalize(blur);
        }
    }

    if (doUseIntermediateSurface) {
        nsRefPtr<gfxPath> path = mThebes->CopyPath();
        // if the path didn't copy correctly then we can't restore it, so bail
        if (!path)
            return NS_ERROR_FAILURE;

        // draw onto a pushed group
        mThebes->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

        // XXX for some reason clipping messes up the path when push/popping
        // copying the path seems to fix it, for unknown reasons
        mThebes->NewPath();
        mThebes->AppendPath(path);

        // don't want operators to be applied twice,
        if (mSurface->GetType() != gfxASurface::SurfaceTypeD2D) {
            mThebes->SetOperator(gfxContext::OPERATOR_SOURCE);
        } else {
            // In the case of D2D OPERATOR_OVER is much faster. So we can just
            // use that since it's the same as SOURCE for a transparent
            // destinations. It would be nice if cairo backends could make this
            // optimization internally but I see no very good way of doing this.
            mThebes->SetOperator(gfxContext::OPERATOR_OVER);
        }
    }

    ApplyStyle(style);

    if (style == STYLE_FILL) {
        if (!doUseIntermediateSurface &&
            CurrentState().globalAlpha != 1.0 &&
            !CurrentState().StyleIsColor(style))
        {
            mThebes->Clip();
            mThebes->Paint(CurrentState().globalAlpha);
        } else {
            mThebes->Fill();
        }
    } else
        mThebes->Stroke();

    // XXX do some more work to calculate the extents of shadows
    // XXX handle stroke extents
    if (dirtyRect && style == STYLE_FILL && !doDrawShadow) {
        *dirtyRect = mThebes->GetUserPathExtent();
    }

    if (doUseIntermediateSurface) {
        mThebes->PopGroupToSource();
        DirtyAllStyles();

        mThebes->Paint(CurrentState().StyleIsColor(style) ? 1.0 : CurrentState().globalAlpha);
    }

    if (dirtyRect) {
        if (style != STYLE_FILL || doDrawShadow) {
            // just use the clip extents
            *dirtyRect = mThebes->GetClipExtents();
        }
    }

    return NS_OK;
}

//
// rects
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::ClearRect(float x, float y, float w, float h)
{
    if (!mSurfaceCreated)
        return NS_OK;

    if (!FloatValidate(x,y,w,h))
        return NS_OK;

    PathAutoSaveRestore pathSR(this);
    gfxContextAutoSaveRestore autoSR(mThebes);

    mThebes->SetOperator(gfxContext::OPERATOR_CLEAR);
    mThebes->NewPath();
    mThebes->Rectangle(gfxRect(x, y, w, h));
    mThebes->Fill();

    return RedrawUser(mThebes->GetUserPathExtent());
}

nsresult
nsCanvasRenderingContext2D::DrawRect(const gfxRect& rect, Style style)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(rect.X(), rect.Y(), rect.Width(), rect.Height()))
        return NS_OK;

    PathAutoSaveRestore pathSR(this);

    mThebes->NewPath();
    mThebes->Rectangle(rect);

    gfxRect dirty;
    nsresult rv = DrawPath(style, &dirty);
    if (NS_FAILED(rv))
        return rv;

    return RedrawUser(dirty);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::FillRect(float x, float y, float w, float h)
{
    return DrawRect(gfxRect(x, y, w, h), STYLE_FILL);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::StrokeRect(float x, float y, float w, float h)
{
    if (w == 0.f && h == 0.f) {
        return NS_OK;
    }
    return DrawRect(gfxRect(x, y, w, h), STYLE_STROKE);
}

//
// path bits
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::BeginPath()
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    mHasPath = false;
    mThebes->NewPath();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::ClosePath()
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    mThebes->ClosePath();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Fill()
{
    gfxRect dirty;
    nsresult rv = DrawPath(STYLE_FILL, &dirty);
    if (NS_FAILED(rv))
        return rv;
    return RedrawUser(dirty);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Stroke()
{
    gfxRect dirty;
    nsresult rv = DrawPath(STYLE_STROKE, &dirty);
    if (NS_FAILED(rv))
        return rv;
    return RedrawUser(dirty);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Clip()
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    mThebes->Clip();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::MoveTo(float x, float y)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(x,y))
        return NS_OK;

    mHasPath = true;
    mThebes->MoveTo(gfxPoint(x, y));
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::LineTo(float x, float y)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(x,y))
        return NS_OK;

    mHasPath = true;
    mThebes->LineTo(gfxPoint(x, y));
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::QuadraticCurveTo(float cpx, float cpy, float x, float y)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(cpx,cpy,x,y))
        return NS_OK;

    // we will always have a current point, since beginPath forces
    // a moveto(0,0)
    gfxPoint c = mThebes->CurrentPoint();
    gfxPoint p(x,y);
    gfxPoint cp(cpx, cpy);

    mHasPath = true;
    mThebes->CurveTo((c+cp*2)/3.0, (p+cp*2)/3.0, p);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::BezierCurveTo(float cp1x, float cp1y,
                                          float cp2x, float cp2y,
                                          float x, float y)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(cp1x,cp1y,cp2x,cp2y,x,y))
        return NS_OK;

    mHasPath = true;
    mThebes->CurveTo(gfxPoint(cp1x, cp1y),
                     gfxPoint(cp2x, cp2y),
                     gfxPoint(x, y));

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::ArcTo(float x1, float y1, float x2, float y2, float radius)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(x1,y1,x2,y2,radius))
        return NS_OK;

    if (radius < 0)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    mHasPath = true;

    gfxPoint p0 = mThebes->CurrentPoint();

    double dir, a2, b2, c2, cosx, sinx, d, anx, any, bnx, bny, x3, y3, x4, y4, cx, cy, angle0, angle1;
    bool anticlockwise;

    if ((x1 == p0.x && y1 == p0.y) || (x1 == x2 && y1 == y2) || radius == 0) {
        mThebes->LineTo(gfxPoint(x1, y1));
        return NS_OK;
    }

    dir = (x2-x1)*(p0.y-y1) + (y2-y1)*(x1-p0.x);
    if (dir == 0) {
        mThebes->LineTo(gfxPoint(x1, y1));
        return NS_OK;
    }

    a2 = (p0.x-x1)*(p0.x-x1) + (p0.y-y1)*(p0.y-y1);
    b2 = (x1-x2)*(x1-x2) + (y1-y2)*(y1-y2);
    c2 = (p0.x-x2)*(p0.x-x2) + (p0.y-y2)*(p0.y-y2);
    cosx = (a2+b2-c2)/(2*sqrt(a2*b2));

    sinx = sqrt(1 - cosx*cosx);
    d = radius / ((1 - cosx) / sinx);

    anx = (x1-p0.x) / sqrt(a2);
    any = (y1-p0.y) / sqrt(a2);
    bnx = (x1-x2) / sqrt(b2);
    bny = (y1-y2) / sqrt(b2);
    x3 = x1 - anx*d;
    y3 = y1 - any*d;
    x4 = x1 - bnx*d;
    y4 = y1 - bny*d;
    anticlockwise = (dir < 0);
    cx = x3 + any*radius*(anticlockwise ? 1 : -1);
    cy = y3 - anx*radius*(anticlockwise ? 1 : -1);
    angle0 = atan2((y3-cy), (x3-cx));
    angle1 = atan2((y4-cy), (x4-cx));

    mThebes->LineTo(gfxPoint(x3, y3));

    if (anticlockwise)
        mThebes->NegativeArc(gfxPoint(cx, cy), radius, angle0, angle1);
    else
        mThebes->Arc(gfxPoint(cx, cy), radius, angle0, angle1);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Arc(float x, float y, float r, float startAngle, float endAngle, bool ccw)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(x,y,r,startAngle,endAngle))
        return NS_OK;

    if (r < 0.0)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    gfxPoint p(x,y);

    mHasPath = true;
    if (ccw)
        mThebes->NegativeArc(p, r, startAngle, endAngle);
    else
        mThebes->Arc(p, r, startAngle, endAngle);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Rect(float x, float y, float w, float h)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(x,y,w,h))
        return NS_OK;

    mHasPath = true;
    mThebes->Rectangle(gfxRect(x, y, w, h));
    return NS_OK;
}

//
// text
//

/**
 * Helper function for SetFont that creates a style rule for the given font.
 * @param aFont The CSS font string
 * @param aNode The canvas element
 * @param aResult Pointer in which to place the new style rule.
 * @remark Assumes all pointer arguments are non-null.
 */
static nsresult
CreateFontStyleRule(const nsAString& aFont,
                    nsINode* aNode,
                    css::StyleRule** aResult)
{
    nsRefPtr<css::StyleRule> rule;
    bool changed;

    nsIPrincipal* principal = aNode->NodePrincipal();
    nsIDocument* document = aNode->OwnerDoc();

    nsIURI* docURL = document->GetDocumentURI();
    nsIURI* baseURL = document->GetDocBaseURI();

    // Pass the CSS Loader object to the parser, to allow parser error reports
    // to include the outer window ID.
    nsCSSParser parser(document->CSSLoader());

    nsresult rv = parser.ParseStyleAttribute(EmptyString(), docURL, baseURL,
                                             principal, getter_AddRefs(rule));
    if (NS_FAILED(rv))
        return rv;

    rv = parser.ParseProperty(eCSSProperty_font, aFont, docURL, baseURL,
                              principal, rule->GetDeclaration(), &changed,
                              false);
    if (NS_FAILED(rv))
        return rv;

    rv = parser.ParseProperty(eCSSProperty_line_height,
                              NS_LITERAL_STRING("normal"), docURL, baseURL,
                              principal, rule->GetDeclaration(), &changed,
                              false);
    if (NS_FAILED(rv))
        return rv;

    rule->RuleMatched();

    rule.forget(aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetFont(const nsAString& font)
{
    nsresult rv;

    /*
     * If font is defined with relative units (e.g. ems) and the parent
     * style context changes in between calls, setting the font to the
     * same value as previous could result in a different computed value,
     * so we cannot have the optimization where we check if the new font
     * string is equal to the old one.
     */

    nsCOMPtr<nsIContent> content = do_QueryInterface(mCanvasElement);
    if (!content && !mDocShell) {
        NS_WARNING("Canvas element must be an nsIContent and non-null or a docshell must be provided");
        return NS_ERROR_FAILURE;
    }

    nsIPresShell* presShell = GetPresShell();
    if (!presShell)
      return NS_ERROR_FAILURE;
    nsIDocument* document = presShell->GetDocument();

    nsCOMArray<nsIStyleRule> rules;

    nsRefPtr<css::StyleRule> rule;
    rv = CreateFontStyleRule(font, document, getter_AddRefs(rule));
    if (NS_FAILED(rv))
        return rv;

    css::Declaration *declaration = rule->GetDeclaration();
    // The easiest way to see whether we got a syntax error or whether
    // we got 'inherit' or 'initial' is to look at font-size-adjust,
    // which the shorthand resets to either 'none' or
    // '-moz-system-font'.
    // We know the declaration is not !important, so we can use
    // GetNormalBlock().
    const nsCSSValue *fsaVal =
      declaration->GetNormalBlock()->ValueFor(eCSSProperty_font_size_adjust);
    if (!fsaVal || (fsaVal->GetUnit() != eCSSUnit_None &&
                    fsaVal->GetUnit() != eCSSUnit_System_Font)) {
        // We got an all-property value or a syntax error.  The spec says
        // this value must be ignored.
        return NS_OK;
    }

    rules.AppendObject(rule);

    nsStyleSet* styleSet = presShell->StyleSet();

    // have to get a parent style context for inherit-like relative
    // values (2em, bolder, etc.)
    nsRefPtr<nsStyleContext> parentContext;

    if (content && content->IsInDoc()) {
        // inherit from the canvas element
        parentContext = nsComputedDOMStyle::GetStyleContextForElement(
                content->AsElement(),
                nsnull,
                presShell);
    } else {
        // otherwise inherit from default
        nsRefPtr<css::StyleRule> parentRule;
        rv = CreateFontStyleRule(kDefaultFontStyle,
                                 document,
                                 getter_AddRefs(parentRule));
        if (NS_FAILED(rv))
            return rv;
        nsCOMArray<nsIStyleRule> parentRules;
        parentRules.AppendObject(parentRule);
        parentContext = styleSet->ResolveStyleForRules(nsnull, parentRules);
    }

    if (!parentContext)
        return NS_ERROR_FAILURE;

    nsRefPtr<nsStyleContext> sc =
        styleSet->ResolveStyleForRules(parentContext, rules);
    if (!sc)
        return NS_ERROR_FAILURE;
    const nsStyleFont* fontStyle = sc->GetStyleFont();

    NS_ASSERTION(fontStyle, "Could not obtain font style");

    nsIAtom* language = sc->GetStyleFont()->mLanguage;
    if (!language) {
        language = presShell->GetPresContext()->GetLanguageFromCharset();
    }

    // use CSS pixels instead of dev pixels to avoid being affected by page zoom
    const PRUint32 aupcp = nsPresContext::AppUnitsPerCSSPixel();
    // un-zoom the font size to avoid being affected by text-only zoom
    //
    // Purposely ignore the font size that respects the user's minimum
    // font preference (fontStyle->mFont.size) in favor of the
    // computed size (fontStyle->mSize).  See
    // https://bugzilla.mozilla.org/show_bug.cgi?id=698652.
    const nscoord fontSize = nsStyleFont::UnZoomText(parentContext->PresContext(), fontStyle->mSize);

    bool printerFont = (presShell->GetPresContext()->Type() == nsPresContext::eContext_PrintPreview ||
                          presShell->GetPresContext()->Type() == nsPresContext::eContext_Print);

    gfxFontStyle style(fontStyle->mFont.style,
                       fontStyle->mFont.weight,
                       fontStyle->mFont.stretch,
                       NSAppUnitsToFloatPixels(fontSize, float(aupcp)),
                       language,
                       fontStyle->mFont.sizeAdjust,
                       fontStyle->mFont.systemFont,
                       printerFont,
                       fontStyle->mFont.featureSettings,
                       fontStyle->mFont.languageOverride);

    CurrentState().fontGroup =
        gfxPlatform::GetPlatform()->CreateFontGroup(fontStyle->mFont.name,
                                                    &style,
                                                    presShell->GetPresContext()->GetUserFontSet());
    NS_ASSERTION(CurrentState().fontGroup, "Could not get font group");

    // The font getter is required to be reserialized based on what we
    // parsed (including having line-height removed).  (Older drafts of
    // the spec required font sizes be converted to pixels, but that no
    // longer seems to be required.)
    declaration->GetValue(eCSSProperty_font, CurrentState().font);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetFont(nsAString& font)
{
    /* will initilize the value if not set, else does nothing */
    GetCurrentFontStyle();

    font = CurrentState().font;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetTextAlign(const nsAString& ta)
{
    if (ta.EqualsLiteral("start"))
        CurrentState().textAlign = TEXT_ALIGN_START;
    else if (ta.EqualsLiteral("end"))
        CurrentState().textAlign = TEXT_ALIGN_END;
    else if (ta.EqualsLiteral("left"))
        CurrentState().textAlign = TEXT_ALIGN_LEFT;
    else if (ta.EqualsLiteral("right"))
        CurrentState().textAlign = TEXT_ALIGN_RIGHT;
    else if (ta.EqualsLiteral("center"))
        CurrentState().textAlign = TEXT_ALIGN_CENTER;

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetTextAlign(nsAString& ta)
{
    switch (CurrentState().textAlign)
    {
    case TEXT_ALIGN_START:
        ta.AssignLiteral("start");
        break;
    case TEXT_ALIGN_END:
        ta.AssignLiteral("end");
        break;
    case TEXT_ALIGN_LEFT:
        ta.AssignLiteral("left");
        break;
    case TEXT_ALIGN_RIGHT:
        ta.AssignLiteral("right");
        break;
    case TEXT_ALIGN_CENTER:
        ta.AssignLiteral("center");
        break;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetTextBaseline(const nsAString& tb)
{
    if (tb.EqualsLiteral("top"))
        CurrentState().textBaseline = TEXT_BASELINE_TOP;
    else if (tb.EqualsLiteral("hanging"))
        CurrentState().textBaseline = TEXT_BASELINE_HANGING;
    else if (tb.EqualsLiteral("middle"))
        CurrentState().textBaseline = TEXT_BASELINE_MIDDLE;
    else if (tb.EqualsLiteral("alphabetic"))
        CurrentState().textBaseline = TEXT_BASELINE_ALPHABETIC;
    else if (tb.EqualsLiteral("ideographic"))
        CurrentState().textBaseline = TEXT_BASELINE_IDEOGRAPHIC;
    else if (tb.EqualsLiteral("bottom"))
        CurrentState().textBaseline = TEXT_BASELINE_BOTTOM;

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetTextBaseline(nsAString& tb)
{
    switch (CurrentState().textBaseline)
    {
    case TEXT_BASELINE_TOP:
        tb.AssignLiteral("top");
        break;
    case TEXT_BASELINE_HANGING:
        tb.AssignLiteral("hanging");
        break;
    case TEXT_BASELINE_MIDDLE:
        tb.AssignLiteral("middle");
        break;
    case TEXT_BASELINE_ALPHABETIC:
        tb.AssignLiteral("alphabetic");
        break;
    case TEXT_BASELINE_IDEOGRAPHIC:
        tb.AssignLiteral("ideographic");
        break;
    case TEXT_BASELINE_BOTTOM:
        tb.AssignLiteral("bottom");
        break;
    }

    return NS_OK;
}

/*
 * Helper function that replaces the whitespace characters in a string
 * with U+0020 SPACE. The whitespace characters are defined as U+0020 SPACE,
 * U+0009 CHARACTER TABULATION (tab), U+000A LINE FEED (LF), U+000B LINE
 * TABULATION, U+000C FORM FEED (FF), and U+000D CARRIAGE RETURN (CR).
 * @param str The string whose whitespace characters to replace.
 */
static inline void
TextReplaceWhitespaceCharacters(nsAutoString& str)
{
    str.ReplaceChar("\x09\x0A\x0B\x0C\x0D", PRUnichar(' '));
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::FillText(const nsAString& text, float x, float y, float maxWidth)
{
    return DrawOrMeasureText(text, x, y, maxWidth, TEXT_DRAW_OPERATION_FILL, nsnull);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::StrokeText(const nsAString& text, float x, float y, float maxWidth)
{
    return DrawOrMeasureText(text, x, y, maxWidth, TEXT_DRAW_OPERATION_STROKE, nsnull);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::MeasureText(const nsAString& rawText,
                                        nsIDOMTextMetrics** _retval)
{
    float width;

    nsresult rv = DrawOrMeasureText(rawText, 0, 0, 0, TEXT_DRAW_OPERATION_MEASURE, &width);

    if (NS_FAILED(rv))
        return rv;

    nsRefPtr<nsIDOMTextMetrics> textMetrics = new nsTextMetrics(width);
    if (!textMetrics.get())
        return NS_ERROR_OUT_OF_MEMORY;

    *_retval = textMetrics.forget().get();

    return NS_OK;
}

/**
 * Used for nsBidiPresUtils::ProcessText
 */
struct NS_STACK_CLASS nsCanvasBidiProcessor : public nsBidiPresUtils::BidiProcessor
{
    virtual void SetText(const PRUnichar* text, PRInt32 length, nsBidiDirection direction)
    {
        mTextRun = mFontgrp->MakeTextRun(text,
                                         length,
                                         mThebes,
                                         mAppUnitsPerDevPixel,
                                         direction==NSBIDI_RTL ? gfxTextRunFactory::TEXT_IS_RTL : 0);
    }

    virtual nscoord GetWidth()
    {
        gfxTextRun::Metrics textRunMetrics = mTextRun->MeasureText(0,
                                                                   mTextRun->GetLength(),
                                                                   mDoMeasureBoundingBox ?
                                                                       gfxFont::TIGHT_INK_EXTENTS :
                                                                       gfxFont::LOOSE_INK_EXTENTS,
                                                                   mThebes,
                                                                   nsnull);

        // this only measures the height; the total width is gotten from the
        // the return value of ProcessText.
        if (mDoMeasureBoundingBox) {
            textRunMetrics.mBoundingBox.Scale(1.0 / mAppUnitsPerDevPixel);
            mBoundingBox = mBoundingBox.Union(textRunMetrics.mBoundingBox);
        }

        return NSToCoordRound(textRunMetrics.mAdvanceWidth);
    }

    virtual void DrawText(nscoord xOffset, nscoord width)
    {
        gfxPoint point = mPt;
        point.x += xOffset;

        // offset is given in terms of left side of string
        if (mTextRun->IsRightToLeft()) {
            // Bug 581092 - don't use rounded pixel width to advance to
            // right-hand end of run, because this will cause different
            // glyph positioning for LTR vs RTL drawing of the same
            // glyph string on OS X and DWrite where textrun widths may
            // involve fractional pixels.
            gfxTextRun::Metrics textRunMetrics =
                mTextRun->MeasureText(0,
                                      mTextRun->GetLength(),
                                      mDoMeasureBoundingBox ?
                                          gfxFont::TIGHT_INK_EXTENTS :
                                          gfxFont::LOOSE_INK_EXTENTS,
                                      mThebes,
                                      nsnull);
            point.x += textRunMetrics.mAdvanceWidth;
            // old code was:
            //   point.x += width * mAppUnitsPerDevPixel;
            // TODO: restore this if/when we move to fractional coords
            // throughout the text layout process
        }

        mTextRun->Draw(mThebes,
                       point,
                       mOp == nsCanvasRenderingContext2D::TEXT_DRAW_OPERATION_STROKE ?
                                    gfxFont::GLYPH_STROKE : gfxFont::GLYPH_FILL,
                       0,
                       mTextRun->GetLength(),
                       nsnull,
                       nsnull,
                       nsnull);
    }

    // current text run
    nsAutoPtr<gfxTextRun> mTextRun;

    // pointer to the context, may not be the canvas's context
    // if an intermediate surface is being used
    gfxContext* mThebes;

    // position of the left side of the string, alphabetic baseline
    gfxPoint mPt;

    // current font
    gfxFontGroup* mFontgrp;

    // dev pixel conversion factor
    PRUint32 mAppUnitsPerDevPixel;

    // operation (fill or stroke)
    nsCanvasRenderingContext2D::TextDrawOperation mOp;

    // union of bounding boxes of all runs, needed for shadows
    gfxRect mBoundingBox;

    // true iff the bounding box should be measured
    bool mDoMeasureBoundingBox;
};

nsresult
nsCanvasRenderingContext2D::DrawOrMeasureText(const nsAString& aRawText,
                                              float aX,
                                              float aY,
                                              float aMaxWidth,
                                              TextDrawOperation aOp,
                                              float* aWidth)
{
    nsresult rv;

    if (!FloatValidate(aX, aY, aMaxWidth))
        return NS_ERROR_DOM_SYNTAX_ERR;

    // spec isn't clear on what should happen if aMaxWidth <= 0, so
    // treat it as an invalid argument
    // technically, 0 should be an invalid value as well, but 0 is the default
    // arg, and there is no way to tell if the default was used
    if (aMaxWidth < 0)
        return NS_ERROR_INVALID_ARG;

    nsCOMPtr<nsIContent> content = do_QueryInterface(mCanvasElement);
    if (!content && !mDocShell) {
        NS_WARNING("Canvas element must be an nsIContent and non-null or a docshell must be provided");
        return NS_ERROR_FAILURE;
    }

    nsIPresShell* presShell = GetPresShell();
    if (!presShell)
        return NS_ERROR_FAILURE;

    nsIDocument* document = presShell->GetDocument();

    // replace all the whitespace characters with U+0020 SPACE
    nsAutoString textToDraw(aRawText);
    TextReplaceWhitespaceCharacters(textToDraw);

    // for now, default to ltr if not in doc
    bool isRTL = false;

    if (content && content->IsInDoc()) {
        // try to find the closest context
        nsRefPtr<nsStyleContext> canvasStyle =
            nsComputedDOMStyle::GetStyleContextForElement(content->AsElement(),
                                                          nsnull,
                                                          presShell);
        if (!canvasStyle)
            return NS_ERROR_FAILURE;
        isRTL = canvasStyle->GetStyleVisibility()->mDirection ==
            NS_STYLE_DIRECTION_RTL;
    } else {
      isRTL = GET_BIDI_OPTION_DIRECTION(document->GetBidiOptions()) == IBMBIDI_TEXTDIRECTION_RTL;
    }

    Style style = aOp == TEXT_DRAW_OPERATION_FILL ? STYLE_FILL : STYLE_STROKE;

    bool doDrawShadow = NeedToDrawShadow();
    bool doUseIntermediateSurface = NeedToUseIntermediateSurface()
        || NeedIntermediateSurfaceToHandleGlobalAlpha(style);

    // Clear the surface if we need to simulate unbounded SOURCE operator
    ClearSurfaceForUnboundedSource();

    nsCanvasBidiProcessor processor;

    GetAppUnitsValues(&processor.mAppUnitsPerDevPixel, NULL);
    processor.mPt = gfxPoint(aX, aY);
    nsRefPtr<nsRenderingContext> ctx;
    if (mThebes) {
        processor.mThebes = mThebes;
    } else {
        ctx = presShell->GetReferenceRenderingContext();
        processor.mThebes = ctx->ThebesContext(); 
    }
    processor.mOp = aOp;
    processor.mBoundingBox = gfxRect(0, 0, 0, 0);
    processor.mDoMeasureBoundingBox = doDrawShadow || !mIsEntireFrameInvalid;

    processor.mFontgrp = GetCurrentFontStyle();
    NS_ASSERTION(processor.mFontgrp, "font group is null");

    nscoord totalWidthCoord;

    // calls bidi algo twice since it needs the full text width and the
    // bounding boxes before rendering anything
    nsBidi bidiEngine;
    rv = nsBidiPresUtils::ProcessText(textToDraw.get(),
                                      textToDraw.Length(),
                                      isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                      presShell->GetPresContext(),
                                      processor,
                                      nsBidiPresUtils::MODE_MEASURE,
                                      nsnull,
                                      0,
                                      &totalWidthCoord,
                                      &bidiEngine);
    if (NS_FAILED(rv))
        return rv;

    float totalWidth = float(totalWidthCoord) / processor.mAppUnitsPerDevPixel;
    if (aWidth)
        *aWidth = totalWidth;

    // if only measuring, don't need to do any more work
    if (aOp==TEXT_DRAW_OPERATION_MEASURE)
        return NS_OK;

    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    processor.mThebes = mThebes;

    // offset pt.x based on text align
    gfxFloat anchorX;

    if (CurrentState().textAlign == TEXT_ALIGN_CENTER)
        anchorX = .5;
    else if (CurrentState().textAlign == TEXT_ALIGN_LEFT ||
             (!isRTL && CurrentState().textAlign == TEXT_ALIGN_START) ||
             (isRTL && CurrentState().textAlign == TEXT_ALIGN_END))
        anchorX = 0;
    else
        anchorX = 1;

    processor.mPt.x -= anchorX * totalWidth;

    // offset pt.y based on text baseline
    NS_ASSERTION(processor.mFontgrp->FontListLength()>0, "font group contains no fonts");
    const gfxFont::Metrics& fontMetrics = processor.mFontgrp->GetFontAt(0)->GetMetrics();

    gfxFloat anchorY;

    switch (CurrentState().textBaseline)
    {
    case TEXT_BASELINE_HANGING:
        // fall through; best we can do with the information available
    case TEXT_BASELINE_TOP:
        anchorY = fontMetrics.emAscent;
        break;
        break;
    case TEXT_BASELINE_MIDDLE:
        anchorY = (fontMetrics.emAscent - fontMetrics.emDescent) * .5f;
        break;
    case TEXT_BASELINE_IDEOGRAPHIC:
        // fall through; best we can do with the information available
    case TEXT_BASELINE_ALPHABETIC:
        anchorY = 0;
        break;
    case TEXT_BASELINE_BOTTOM:
        anchorY = -fontMetrics.emDescent;
        break;
    }

    processor.mPt.y += anchorY;

    // correct bounding box to get it to be the correct size/position
    processor.mBoundingBox.width = totalWidth;
    processor.mBoundingBox.MoveBy(processor.mPt);

    processor.mPt.x *= processor.mAppUnitsPerDevPixel;
    processor.mPt.y *= processor.mAppUnitsPerDevPixel;

    // if text is over aMaxWidth, then scale the text horizontally such that its
    // width is precisely aMaxWidth
    gfxContextAutoSaveRestore autoSR;
    if (aMaxWidth > 0 && totalWidth > aMaxWidth) {
        autoSR.SetContext(mThebes);
        // translate the anchor point to 0, then scale and translate back
        gfxPoint trans(aX, 0);
        mThebes->Translate(trans);
        mThebes->Scale(aMaxWidth/totalWidth, 1);
        mThebes->Translate(-trans);
    }

    // save the previous bounding box
    gfxRect boundingBox = processor.mBoundingBox;

    // don't ever need to measure the bounding box twice
    processor.mDoMeasureBoundingBox = false;

    if (doDrawShadow) {
        // for some reason the box is too tight, probably rounding error
        processor.mBoundingBox.Inflate(2.0);

        // this is unnecessarily big is max-width scaling is involved, but it
        // will still produce correct output
        gfxRect drawExtents = mThebes->UserToDevice(processor.mBoundingBox);
        gfxAlphaBoxBlur blur;

        gfxContext* ctx = ShadowInitialize(drawExtents, blur);

        if (ctx) {
            ApplyStyle(style, false);
            CopyContext(ctx, mThebes);
            ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
            processor.mThebes = ctx;

            rv = nsBidiPresUtils::ProcessText(textToDraw.get(),
                                              textToDraw.Length(),
                                              isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                              presShell->GetPresContext(),
                                              processor,
                                              nsBidiPresUtils::MODE_DRAW,
                                              nsnull,
                                              0,
                                              nsnull,
                                              &bidiEngine);
            if (NS_FAILED(rv))
                return rv;

            ShadowFinalize(blur);
        }

        processor.mThebes = mThebes;
    }

    gfxContextPathAutoSaveRestore pathSR(mThebes, false);

    if (doUseIntermediateSurface) {
        mThebes->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

        // don't want operators to be applied twice
        mThebes->SetOperator(gfxContext::OPERATOR_SOURCE);
    }

    ApplyStyle(style);

    rv = nsBidiPresUtils::ProcessText(textToDraw.get(),
                                      textToDraw.Length(),
                                      isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                      presShell->GetPresContext(),
                                      processor,
                                      nsBidiPresUtils::MODE_DRAW,
                                      nsnull,
                                      0,
                                      nsnull,
                                      &bidiEngine);

    // this needs to be restored before function can return
    if (doUseIntermediateSurface) {
        mThebes->PopGroupToSource();
        DirtyAllStyles();
    }

    if (NS_FAILED(rv))
        return rv;

    if (doUseIntermediateSurface)
        mThebes->Paint(CurrentState().StyleIsColor(style) ? 1.0 : CurrentState().globalAlpha);

    if (aOp == nsCanvasRenderingContext2D::TEXT_DRAW_OPERATION_FILL && !doDrawShadow)
        return RedrawUser(boundingBox);

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetMozTextStyle(const nsAString& textStyle)
{
    // font and mozTextStyle are the same value
    return SetFont(textStyle);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMozTextStyle(nsAString& textStyle)
{
    // font and mozTextStyle are the same value
    return GetFont(textStyle);
}

gfxFontGroup*
nsCanvasRenderingContext2D::GetCurrentFontStyle()
{
    // use lazy initilization for the font group since it's rather expensive
    if(!CurrentState().fontGroup) {
        nsresult rv = SetMozTextStyle(kDefaultFontStyle);
        if (NS_FAILED(rv)) {
            gfxFontStyle style;
            style.size = kDefaultFontSize;
            CurrentState().fontGroup =
                gfxPlatform::GetPlatform()->CreateFontGroup(kDefaultFontName,
                                                            &style,
                                                            nsnull);
            if (CurrentState().fontGroup) {
                CurrentState().font = kDefaultFontStyle;
                rv = NS_OK;
            } else {
                rv = NS_ERROR_OUT_OF_MEMORY;
            }
        }
            
        NS_ASSERTION(NS_SUCCEEDED(rv), "Default canvas font is invalid");
    }

    return CurrentState().fontGroup;
}

gfxTextRun*
nsCanvasRenderingContext2D::MakeTextRun(const PRUnichar* aText,
                                        PRUint32         aLength,
                                        PRUint32         aAppUnitsPerDevUnit,
                                        PRUint32         aFlags)
{
    gfxFontGroup* currentFontStyle = GetCurrentFontStyle();
    if (!currentFontStyle)
        return nsnull;
    return currentFontStyle->MakeTextRun(aText, aLength,
                                         mThebes, aAppUnitsPerDevUnit, aFlags);
}


//
// line caps/joins
//
NS_IMETHODIMP
nsCanvasRenderingContext2D::SetLineWidth(float width)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(width) || width <= 0.0)
        return NS_OK;

    mThebes->SetLineWidth(width);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetLineWidth(float *width)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;
 
    gfxFloat d = mThebes->CurrentLineWidth();
    *width = static_cast<float>(d);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetLineCap(const nsAString& capstyle)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    gfxContext::GraphicsLineCap cap;

    if (capstyle.EqualsLiteral("butt"))
        cap = gfxContext::LINE_CAP_BUTT;
    else if (capstyle.EqualsLiteral("round"))
        cap = gfxContext::LINE_CAP_ROUND;
    else if (capstyle.EqualsLiteral("square"))
        cap = gfxContext::LINE_CAP_SQUARE;
    else
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_OK;

    mThebes->SetLineCap(cap);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetLineCap(nsAString& capstyle)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    gfxContext::GraphicsLineCap cap = mThebes->CurrentLineCap();

    if (cap == gfxContext::LINE_CAP_BUTT)
        capstyle.AssignLiteral("butt");
    else if (cap == gfxContext::LINE_CAP_ROUND)
        capstyle.AssignLiteral("round");
    else if (cap == gfxContext::LINE_CAP_SQUARE)
        capstyle.AssignLiteral("square");
    else
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetLineJoin(const nsAString& joinstyle)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    gfxContext::GraphicsLineJoin j;

    if (joinstyle.EqualsLiteral("round"))
        j = gfxContext::LINE_JOIN_ROUND;
    else if (joinstyle.EqualsLiteral("bevel"))
        j = gfxContext::LINE_JOIN_BEVEL;
    else if (joinstyle.EqualsLiteral("miter"))
        j = gfxContext::LINE_JOIN_MITER;
    else
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_OK;

    mThebes->SetLineJoin(j);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetLineJoin(nsAString& joinstyle)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    gfxContext::GraphicsLineJoin j = mThebes->CurrentLineJoin();

    if (j == gfxContext::LINE_JOIN_ROUND)
        joinstyle.AssignLiteral("round");
    else if (j == gfxContext::LINE_JOIN_BEVEL)
        joinstyle.AssignLiteral("bevel");
    else if (j == gfxContext::LINE_JOIN_MITER)
        joinstyle.AssignLiteral("miter");
    else
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetMiterLimit(float miter)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(miter) || miter <= 0.0)
        return NS_OK;

    mThebes->SetMiterLimit(miter);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMiterLimit(float *miter)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    gfxFloat d = mThebes->CurrentMiterLimit();
    *miter = static_cast<float>(d);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetMozDash(JSContext *cx, const jsval& patternArray)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    AutoFallibleTArray<gfxFloat, 10> dashes;
    nsresult rv = JSValToDashArray(cx, patternArray, dashes);
    if (NS_SUCCEEDED(rv)) {
        mThebes->SetDash(dashes.Elements(), dashes.Length(),
                         mThebes->CurrentDashOffset());
    }
    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMozDash(JSContext* cx, jsval* dashArray)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    AutoFallibleTArray<gfxFloat, 10> dashes;
    if (!mThebes->CurrentDash(dashes, nsnull)) {
        dashes.SetLength(0);
    }
    return DashArrayToJSVal(dashes, cx, dashArray);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetMozDashOffset(float offset)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(offset)) {
        return NS_ERROR_ILLEGAL_VALUE;
    }

    AutoFallibleTArray<gfxFloat, 10> dashes;
    if (!mThebes->CurrentDash(dashes, nsnull)) {
        // Either no dash is set or the cairo call failed.  Either
        // way, eat the error.

        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_OK;
    }
    NS_ABORT_IF_FALSE(dashes.Length() > 0,
                      "CurrentDash() should have returned false");

    mThebes->SetDash(dashes.Elements(), dashes.Length(),
                     gfxFloat(offset));

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMozDashOffset(float* offset)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    *offset = float(mThebes->CurrentDashOffset());
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::IsPointInPath(float x, float y, bool *retVal)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (!FloatValidate(x,y)) {
        *retVal = false;
        return NS_OK;
    }

    gfxPoint pt(x, y);
    *retVal = mThebes->PointInFill(mThebes->DeviceToUser(pt));
    return NS_OK;
}

//
// image
//

// drawImage(in HTMLImageElement image, in float dx, in float dy);
//   -- render image from 0,0 at dx,dy top-left coords
// drawImage(in HTMLImageElement image, in float dx, in float dy, in float sw, in float sh);
//   -- render image from 0,0 at dx,dy top-left coords clipping it to sw,sh
// drawImage(in HTMLImageElement image, in float sx, in float sy, in float sw, in float sh, in float dx, in float dy, in float dw, in float dh);
//   -- render the region defined by (sx,sy,sw,wh) in image-local space into the region (dx,dy,dw,dh) on the canvas

NS_IMETHODIMP
nsCanvasRenderingContext2D::DrawImage(nsIDOMElement *imgElt, float a1,
                                      float a2, float a3, float a4, float a5,
                                      float a6, float a7, float a8,
                                      PRUint8 optional_argc)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIContent> content = do_QueryInterface(imgElt);
    if (!content) {
        return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
    }

    nsHTMLCanvasElement* canvas = nsHTMLCanvasElement::FromContent(content);
    if (canvas) {
        nsIntSize size = canvas->GetSize();
        if (size.width == 0 || size.height == 0) {
            return NS_ERROR_DOM_INVALID_STATE_ERR;
        }
    }

    gfxMatrix matrix;
    nsRefPtr<gfxPattern> pattern;
    gfxIntSize imgSize;
    nsRefPtr<gfxASurface> imgsurf =
      CanvasImageCache::Lookup(imgElt, HTMLCanvasElement(), &imgSize);

    if (!imgsurf) {
        // The canvas spec says that drawImage should draw the first frame
        // of animated images
        PRUint32 sfeFlags = nsLayoutUtils::SFE_WANT_FIRST_FRAME;
        nsLayoutUtils::SurfaceFromElementResult res =
            nsLayoutUtils::SurfaceFromElement(content->AsElement(), sfeFlags);
        if (!res.mSurface) {
            // Spec says to silently do nothing if the element is still loading.
            return res.mIsStillLoading ? NS_OK : NS_ERROR_NOT_AVAILABLE;
        }

        // Force a copy if we're using drawImage with our destination
        // as a source to work around some Cairo self-copy semantics issues.
        if (res.mSurface == mSurface) {
            sfeFlags |= nsLayoutUtils::SFE_WANT_NEW_SURFACE;
            res = nsLayoutUtils::SurfaceFromElement(content->AsElement(),
                                                    sfeFlags);
            if (!res.mSurface)
                return NS_ERROR_NOT_AVAILABLE;
        }

        imgsurf = res.mSurface.forget();
        imgSize = res.mSize;

        if (mCanvasElement) {
            CanvasUtils::DoDrawImageSecurityCheck(HTMLCanvasElement(),
                                                  res.mPrincipal,
                                                  res.mIsWriteOnly,
                                                  res.mCORSUsed);
        }

        if (res.mImageRequest) {
            CanvasImageCache::NotifyDrawImage(imgElt, HTMLCanvasElement(),
                                              res.mImageRequest, imgsurf, imgSize);
        }
    }

    double sx,sy,sw,sh;
    double dx,dy,dw,dh;
    if (optional_argc == 0) {
        dx = a1;
        dy = a2;
        sx = sy = 0.0;
        dw = sw = (double) imgSize.width;
        dh = sh = (double) imgSize.height;
    } else if (optional_argc == 2) {
        dx = a1;
        dy = a2;
        dw = a3;
        dh = a4;
        sx = sy = 0.0;
        sw = (double) imgSize.width;
        sh = (double) imgSize.height;
    } else if (optional_argc == 6) {
        sx = a1;
        sy = a2;
        sw = a3;
        sh = a4;
        dx = a5;
        dy = a6;
        dw = a7;
        dh = a8;
    } else {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_INVALID_ARG;
    }

    if (dw == 0.0 || dh == 0.0) {
        // not really failure, but nothing to do --
        // and noone likes a divide-by-zero
        return NS_OK;
    }

    if (!FloatValidate(sx, sy, sw, sh) || !FloatValidate(dx, dy, dw, dh)) {
        return NS_OK;
    }

    // check args
    if (sx < 0.0 || sy < 0.0 ||
        sw < 0.0 || sw > (double) imgSize.width ||
        sh < 0.0 || sh > (double) imgSize.height ||
        dw < 0.0 || dh < 0.0)
    {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    matrix.Translate(gfxPoint(sx, sy));
    matrix.Scale(sw/dw, sh/dh);

    pattern = new gfxPattern(imgsurf);
    pattern->SetMatrix(matrix);
    pattern->SetExtend(gfxPattern::EXTEND_PAD);

    if (CurrentState().imageSmoothingEnabled)
        pattern->SetFilter(gfxPattern::FILTER_GOOD);
    else
        pattern->SetFilter(gfxPattern::FILTER_NEAREST);

    PathAutoSaveRestore pathSR(this);

    // Clear the surface if we need to simulate unbounded SOURCE operator
    ClearSurfaceForUnboundedSource();

    {
        gfxContextMatrixAutoSaveRestore autoMatrixSR(mThebes);

        mThebes->Translate(gfxPoint(dx, dy));

        gfxRect clip(0, 0, dw, dh);

        if (NeedToDrawShadow()) {
            gfxRect drawExtents = mThebes->UserToDevice(clip);
            gfxAlphaBoxBlur blur;

            gfxContext* ctx = ShadowInitialize(drawExtents, blur);

            if (ctx) {
                CopyContext(ctx, mThebes);
                ctx->SetPattern(pattern);
                ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
                ctx->Clip(clip);
                ctx->Paint();

                ShadowFinalize(blur);
            }
        }

        mThebes->SetPattern(pattern);
        DirtyAllStyles();

        bool doUseIntermediateSurface = NeedToUseIntermediateSurface();
        if (doUseIntermediateSurface) {
            gfxContextAutoSaveRestore autoSR(mThebes);

            // draw onto a pushed group
            mThebes->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
            mThebes->Clip(clip);

            // don't want operators to be applied twice
            mThebes->SetOperator(gfxContext::OPERATOR_SOURCE);

            mThebes->Paint();
            mThebes->PopGroupToSource();
            mThebes->Paint(CurrentState().globalAlpha);
        } else if (CurrentState().globalAlpha == 1.0f &&
                   mThebes->CurrentOperator() == gfxContext::OPERATOR_OVER) {
            /* Direct2D isn't very good at clipping so use Fill() when we can */
            mThebes->NewPath();
            mThebes->Rectangle(clip);
            mThebes->Fill();
        } else {
            gfxContextAutoSaveRestore autoSR(mThebes);

            /* we need to use to clip instead of fill for globalAlpha */
            mThebes->Clip(clip);
            mThebes->Paint(CurrentState().globalAlpha);
        }

        RedrawUser(clip);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetGlobalCompositeOperation(const nsAString& op)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    gfxContext::GraphicsOperator thebes_op;

#define CANVAS_OP_TO_THEBES_OP(cvsop,thebesop) \
    if (op.EqualsLiteral(cvsop))   \
        thebes_op = gfxContext::OPERATOR_##thebesop;

    CANVAS_OP_TO_THEBES_OP("copy", SOURCE)
    else CANVAS_OP_TO_THEBES_OP("destination-atop", DEST_ATOP)
    else CANVAS_OP_TO_THEBES_OP("destination-in", DEST_IN)
    else CANVAS_OP_TO_THEBES_OP("destination-out", DEST_OUT)
    else CANVAS_OP_TO_THEBES_OP("destination-over", DEST_OVER)
    else CANVAS_OP_TO_THEBES_OP("lighter", ADD)
    else CANVAS_OP_TO_THEBES_OP("source-atop", ATOP)
    else CANVAS_OP_TO_THEBES_OP("source-in", IN)
    else CANVAS_OP_TO_THEBES_OP("source-out", OUT)
    else CANVAS_OP_TO_THEBES_OP("source-over", OVER)
    else CANVAS_OP_TO_THEBES_OP("xor", XOR)
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    else return NS_OK;

#undef CANVAS_OP_TO_THEBES_OP

    mThebes->SetOperator(thebes_op);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetGlobalCompositeOperation(nsAString& op)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    gfxContext::GraphicsOperator thebes_op = mThebes->CurrentOperator();

#define CANVAS_OP_TO_THEBES_OP(cvsop,thebesop) \
    if (thebes_op == gfxContext::OPERATOR_##thebesop) \
        op.AssignLiteral(cvsop);

    CANVAS_OP_TO_THEBES_OP("copy", SOURCE)
    else CANVAS_OP_TO_THEBES_OP("destination-atop", DEST_ATOP)
    else CANVAS_OP_TO_THEBES_OP("destination-in", DEST_IN)
    else CANVAS_OP_TO_THEBES_OP("destination-out", DEST_OUT)
    else CANVAS_OP_TO_THEBES_OP("destination-over", DEST_OVER)
    else CANVAS_OP_TO_THEBES_OP("lighter", ADD)
    else CANVAS_OP_TO_THEBES_OP("source-atop", ATOP)
    else CANVAS_OP_TO_THEBES_OP("source-in", IN)
    else CANVAS_OP_TO_THEBES_OP("source-out", OUT)
    else CANVAS_OP_TO_THEBES_OP("source-over", OVER)
    else CANVAS_OP_TO_THEBES_OP("xor", XOR)
    else return NS_ERROR_FAILURE;

#undef CANVAS_OP_TO_THEBES_OP

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::DrawWindow(nsIDOMWindow* aWindow, float aX, float aY,
                                       float aW, float aH,
                                       const nsAString& aBGColor,
                                       PRUint32 flags)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    NS_ENSURE_ARG(aWindow != nsnull);

    // protect against too-large surfaces that will cause allocation
    // or overflow issues
    if (!gfxASurface::CheckSurfaceSize(gfxIntSize(PRInt32(aW), PRInt32(aH)),
                                       0xffff))
        return NS_ERROR_FAILURE;

    // We can't allow web apps to call this until we fix at least the
    // following potential security issues:
    // -- rendering cross-domain IFRAMEs and then extracting the results
    // -- rendering the user's theme and then extracting the results
    // -- rendering native anonymous content (e.g., file input paths;
    // scrollbars should be allowed)
    if (!nsContentUtils::IsCallerTrustedForRead()) {
      // not permitted to use DrawWindow
      // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    // Flush layout updates
    if (!(flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH))
        nsContentUtils::FlushLayoutForTree(aWindow);

    nsRefPtr<nsPresContext> presContext;
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aWindow);
    if (win) {
        nsIDocShell* docshell = win->GetDocShell();
        if (docshell) {
            docshell->GetPresContext(getter_AddRefs(presContext));
        }
    }
    if (!presContext)
        return NS_ERROR_FAILURE;

    nscolor bgColor;

    nsIDocument* elementDoc = mCanvasElement ?
                              HTMLCanvasElement()->OwnerDoc() : nsnull;

    // Pass the CSS Loader object to the parser, to allow parser error reports
    // to include the outer window ID.
    nsCSSParser parser(elementDoc ? elementDoc->CSSLoader() : nsnull);
    nsresult rv = parser.ParseColorString(PromiseFlatString(aBGColor),
                                          nsnull, 0, &bgColor);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIPresShell* presShell = presContext->PresShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    nsRect r(nsPresContext::CSSPixelsToAppUnits(aX),
             nsPresContext::CSSPixelsToAppUnits(aY),
             nsPresContext::CSSPixelsToAppUnits(aW),
             nsPresContext::CSSPixelsToAppUnits(aH));
    PRUint32 renderDocFlags = (nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                               nsIPresShell::RENDER_DOCUMENT_RELATIVE);
    if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_CARET) {
        renderDocFlags |= nsIPresShell::RENDER_CARET;
    }
    if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_VIEW) {
        renderDocFlags &= ~(nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING |
                            nsIPresShell::RENDER_DOCUMENT_RELATIVE);
    }
    if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_USE_WIDGET_LAYERS) {
        renderDocFlags |= nsIPresShell::RENDER_USE_WIDGET_LAYERS;
    }
    if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_ASYNC_DECODE_IMAGES) {
        renderDocFlags |= nsIPresShell::RENDER_ASYNC_DECODE_IMAGES;
    }

    rv = presShell->RenderDocument(r, renderDocFlags, bgColor, mThebes);

    // get rid of the pattern surface ref, just in case
    mThebes->SetColor(gfxRGBA(1,1,1,1));
    DirtyAllStyles();

    // note that aX and aY are coordinates in the document that
    // we're drawing; aX and aY are drawn to 0,0 in current user
    // space.
    RedrawUser(gfxRect(0, 0, aW, aH));

    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::AsyncDrawXULElement(nsIDOMXULElement* aElem, float aX, float aY,
                                                float aW, float aH,
                                                const nsAString& aBGColor,
                                                PRUint32 flags)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    NS_ENSURE_ARG(aElem != nsnull);

    // We can't allow web apps to call this until we fix at least the
    // following potential security issues:
    // -- rendering cross-domain IFRAMEs and then extracting the results
    // -- rendering the user's theme and then extracting the results
    // -- rendering native anonymous content (e.g., file input paths;
    // scrollbars should be allowed)
    if (!nsContentUtils::IsCallerTrustedForRead()) {
        // not permitted to use DrawWindow
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsCOMPtr<nsIFrameLoaderOwner> loaderOwner = do_QueryInterface(aElem);
    if (!loaderOwner)
        return NS_ERROR_FAILURE;

    nsRefPtr<nsFrameLoader> frameloader = loaderOwner->GetFrameLoader();
    if (!frameloader)
        return NS_ERROR_FAILURE;

    PBrowserParent *child = frameloader->GetRemoteBrowser();
    if (!child) {
        nsCOMPtr<nsIDOMWindow> window =
            do_GetInterface(frameloader->GetExistingDocShell());
        if (!window)
            return NS_ERROR_FAILURE;

        return DrawWindow(window, aX, aY, aW, aH, aBGColor, flags);
    }

    // protect against too-large surfaces that will cause allocation
    // or overflow issues
    if (!gfxASurface::CheckSurfaceSize(gfxIntSize(aW, aH), 0xffff))
        return NS_ERROR_FAILURE;

    bool flush =
        (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DO_NOT_FLUSH) == 0;

    PRUint32 renderDocFlags = nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
    if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_CARET) {
        renderDocFlags |= nsIPresShell::RENDER_CARET;
    }
    if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_VIEW) {
        renderDocFlags &= ~nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
    }

    nsRect rect(nsPresContext::CSSPixelsToAppUnits(aX),
                nsPresContext::CSSPixelsToAppUnits(aY),
                nsPresContext::CSSPixelsToAppUnits(aW),
                nsPresContext::CSSPixelsToAppUnits(aH));
    if (mIPC) {
        PDocumentRendererParent *pdocrender =
            child->SendPDocumentRendererConstructor(rect,
                                                    mThebes->CurrentMatrix(),
                                                    nsString(aBGColor),
                                                    renderDocFlags, flush,
                                                    nsIntSize(mWidth, mHeight));
        if (!pdocrender)
            return NS_ERROR_FAILURE;

        DocumentRendererParent *docrender =
            static_cast<DocumentRendererParent *>(pdocrender);

        docrender->SetCanvasContext(this, mThebes);
    }

    return NS_OK;
}

//
// device pixel getting/setting
//

void
nsCanvasRenderingContext2D::EnsureUnpremultiplyTable() {
  if (sUnpremultiplyTable)
    return;

  // Infallably alloc the unpremultiply table.
  sUnpremultiplyTable = new PRUint8[256][256];

  // It's important that the array be indexed first by alpha and then by rgb
  // value.  When we unpremultiply a pixel, we're guaranteed to do three
  // lookups with the same alpha; indexing by alpha first makes it likely that
  // those three lookups will be close to one another in memory, thus
  // increasing the chance of a cache hit.

  // a == 0 case
  for (PRUint32 c = 0; c <= 255; c++) {
    sUnpremultiplyTable[0][c] = c;
  }

  for (int a = 1; a <= 255; a++) {
    for (int c = 0; c <= 255; c++) {
      sUnpremultiplyTable[a][c] = (PRUint8)((c * 255) / a);
    }
  }
}


NS_IMETHODIMP
nsCanvasRenderingContext2D::GetImageData(double aSx, double aSy,
                                         double aSw, double aSh,
                                         JSContext* aCx,
                                         nsIDOMImageData** aRetval)
{
    if (!mCanvasElement && !mDocShell) {
        NS_ERROR("No canvas element and no docshell in GetImageData!!!");
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    // Check only if we have a canvas element; if we were created with a docshell,
    // then it's special internal use.
    if (mCanvasElement &&
        HTMLCanvasElement()->IsWriteOnly() &&
        !nsContentUtils::IsCallerTrustedForRead()) {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    if (!EnsureSurface()) {
        return NS_ERROR_FAILURE;
    }

    if (!NS_finite(aSx) || !NS_finite(aSy) ||
        !NS_finite(aSw) || !NS_finite(aSh)) {
        return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    }

    if (!aSw || !aSh) {
        return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    int32_t x = JS_DoubleToInt32(aSx);
    int32_t y = JS_DoubleToInt32(aSy);
    int32_t wi = JS_DoubleToInt32(aSw);
    int32_t hi = JS_DoubleToInt32(aSh);

    // Handle negative width and height by flipping the rectangle over in the
    // relevant direction.
    uint32_t w, h;
    if (aSw < 0) {
        w = -wi;
        x -= w;
    } else {
        w = wi;
    }
    if (aSh < 0) {
        h = -hi;
        y -= h;
    } else {
        h = hi;
    }

    if (w == 0) {
        w = 1;
    }
    if (h == 0) {
        h = 1;
    }

    JSObject* array;
    nsresult rv = GetImageDataArray(aCx, x, y, w, h, &array);
    NS_ENSURE_SUCCESS(rv, rv);
    MOZ_ASSERT(array);

    nsRefPtr<ImageData> imageData = new ImageData(w, h, *array);
    imageData.forget(aRetval);
    return NS_OK;
}

nsresult
nsCanvasRenderingContext2D::GetImageDataArray(JSContext* aCx,
                                              int32_t aX,
                                              int32_t aY,
                                              uint32_t aWidth,
                                              uint32_t aHeight,
                                              JSObject** aRetval)
{
    MOZ_ASSERT(aWidth && aHeight);

    CheckedInt<uint32_t> len = CheckedInt<uint32_t>(aWidth) * aHeight * 4;
    if (!len.valid()) {
        return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    CheckedInt<int32_t> rightMost = CheckedInt<int32_t>(aX) + aWidth;
    CheckedInt<int32_t> bottomMost = CheckedInt<int32_t>(aY) + aHeight;

    if (!rightMost.valid() || !bottomMost.valid()) {
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    JSObject* darray =
      js_CreateTypedArray(aCx, js::TypedArray::TYPE_UINT8_CLAMPED, len.value());
    if (!darray) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    uint8_t* data = static_cast<uint8_t*>(JS_GetTypedArrayData(darray));

    /* Copy the surface contents to the buffer */
    nsRefPtr<gfxImageSurface> tmpsurf =
        new gfxImageSurface(data,
                            gfxIntSize(aWidth, aHeight),
                            aWidth * 4,
                            gfxASurface::ImageFormatARGB32);

    if (tmpsurf->CairoStatus())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxContext> tmpctx = new gfxContext(tmpsurf);

    if (tmpctx->HasError())
        return NS_ERROR_FAILURE;

    if (!mZero) {
        gfxRect srcRect(0, 0, mWidth, mHeight);
        gfxRect destRect(aX, aY, aWidth, aHeight);

        bool finishedPainting = false;
        // In the common case, we want to avoid the Rectangle call.
        if (!srcRect.Contains(destRect)) {
            // If the requested area is entirely outside the canvas, we're done.
            gfxRect tmp = srcRect.Intersect(destRect);
            finishedPainting = tmp.IsEmpty();

            // Set clipping region if necessary.
            if (!finishedPainting) {
                tmpctx->Rectangle(tmp);
            }
        }

        if (!finishedPainting) {
            tmpctx->SetOperator(gfxContext::OPERATOR_SOURCE);
            tmpctx->SetSource(mSurface, gfxPoint(-aX, -aY));
            tmpctx->Paint();
        }
    }

    // make sure sUnpremultiplyTable has been created
    EnsureUnpremultiplyTable();

    // NOTE! dst is the same as src, and this relies on reading
    // from src and advancing that ptr before writing to dst.
    uint8_t *src = data;
    uint8_t *dst = data;

    for (uint32_t j = 0; j < aHeight; ++j) {
        for (uint32_t i = 0; i < aWidth; ++i) {
            // XXX Is there some useful swizzle MMX we can use here?
#ifdef IS_LITTLE_ENDIAN
            PRUint8 b = *src++;
            PRUint8 g = *src++;
            PRUint8 r = *src++;
            PRUint8 a = *src++;
#else
            PRUint8 a = *src++;
            PRUint8 r = *src++;
            PRUint8 g = *src++;
            PRUint8 b = *src++;
#endif
            // Convert to non-premultiplied color
            *dst++ = sUnpremultiplyTable[a][r];
            *dst++ = sUnpremultiplyTable[a][g];
            *dst++ = sUnpremultiplyTable[a][b];
            *dst++ = a;
        }
    }

    *aRetval = darray;
    return NS_OK;
}

void
nsCanvasRenderingContext2D::EnsurePremultiplyTable() {
  if (sPremultiplyTable)
    return;

  // Infallably alloc the premultiply table.
  sPremultiplyTable = new PRUint8[256][256];

  // Like the unpremultiply table, it's important that we index the premultiply
  // table with the alpha value as the first index to ensure good cache
  // performance.

  for (int a = 0; a <= 255; a++) {
    for (int c = 0; c <= 255; c++) {
      sPremultiplyTable[a][c] = (a * c + 254) / 255;
    }
  }
}

// void putImageData (in ImageData d, in float x, in float y);
NS_IMETHODIMP
nsCanvasRenderingContext2D::PutImageData()
{
    /* Should never be called -- PutImageData_explicit is the QS entry point */
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::PutImageData_explicit(PRInt32 x, PRInt32 y, PRUint32 w, PRUint32 h,
                                                  unsigned char *aData, PRUint32 aDataLen,
                                                  bool hasDirtyRect, PRInt32 dirtyX, PRInt32 dirtyY,
                                                  PRInt32 dirtyWidth, PRInt32 dirtyHeight)
{
    if (!EnsureSurface())
        return NS_ERROR_FAILURE;

    if (w == 0 || h == 0)
        return NS_ERROR_DOM_SYNTAX_ERR;

    gfxRect dirtyRect;
    gfxRect imageDataRect(0, 0, w, h);

    if (hasDirtyRect) {
        // fix up negative dimensions
        if (dirtyWidth < 0) {
            NS_ENSURE_TRUE(dirtyWidth != INT_MIN, NS_ERROR_DOM_INDEX_SIZE_ERR);

            CheckedInt32 checkedDirtyX = CheckedInt32(dirtyX) + dirtyWidth;

            if (!checkedDirtyX.valid())
                return NS_ERROR_DOM_INDEX_SIZE_ERR;

            dirtyX = checkedDirtyX.value();
            dirtyWidth = -(int32)dirtyWidth;
        }

        if (dirtyHeight < 0) {
            NS_ENSURE_TRUE(dirtyHeight != INT_MIN, NS_ERROR_DOM_INDEX_SIZE_ERR);

            CheckedInt32 checkedDirtyY = CheckedInt32(dirtyY) + dirtyHeight;

            if (!checkedDirtyY.valid())
                return NS_ERROR_DOM_INDEX_SIZE_ERR;

            dirtyY = checkedDirtyY.value();
            dirtyHeight = -(int32)dirtyHeight;
        }

        // bound the dirty rect within the imageData rectangle
        dirtyRect = imageDataRect.Intersect(gfxRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight));

        if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0)
            return NS_OK;
    } else {
        dirtyRect = imageDataRect;
    }

    dirtyRect.MoveBy(gfxPoint(x, y));
    dirtyRect = gfxRect(0, 0, mWidth, mHeight).Intersect(dirtyRect);

    if (dirtyRect.Width() <= 0 || dirtyRect.Height() <= 0)
        return NS_OK;

    PRUint32 len = w * h * 4;
    if (aDataLen != len)
        return NS_ERROR_DOM_SYNTAX_ERR;

    nsRefPtr<gfxImageSurface> imgsurf = new gfxImageSurface(gfxIntSize(w, h),
                                                            gfxASurface::ImageFormatARGB32);
    if (!imgsurf || imgsurf->CairoStatus())
        return NS_ERROR_FAILURE;

    // ensure premultiply table has been created
    EnsurePremultiplyTable();

    PRUint8 *src = aData;
    PRUint8 *dst = imgsurf->Data();

    for (PRUint32 j = 0; j < h; j++) {
        for (PRUint32 i = 0; i < w; i++) {
            PRUint8 r = *src++;
            PRUint8 g = *src++;
            PRUint8 b = *src++;
            PRUint8 a = *src++;

            // Convert to premultiplied color (losslessly if the input came from getImageData)
#ifdef IS_LITTLE_ENDIAN
            *dst++ = sPremultiplyTable[a][b];
            *dst++ = sPremultiplyTable[a][g];
            *dst++ = sPremultiplyTable[a][r];
            *dst++ = a;
#else
            *dst++ = a;
            *dst++ = sPremultiplyTable[a][r];
            *dst++ = sPremultiplyTable[a][g];
            *dst++ = sPremultiplyTable[a][b];
#endif
        }
    }

    PathAutoSaveRestore pathSR(this);
    gfxContextAutoSaveRestore autoSR(mThebes);

    // ignore clipping region, as per spec
    mThebes->ResetClip();

    mThebes->IdentityMatrix();
    mThebes->NewPath();
    mThebes->Rectangle(dirtyRect);
    mThebes->SetSource(imgsurf, gfxPoint(x, y));
    mThebes->SetOperator(gfxContext::OPERATOR_SOURCE);
    mThebes->Fill();

    return Redraw(dirtyRect);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetThebesSurface(gfxASurface **surface)
{
    if (!EnsureSurface()) {
        *surface = nsnull;
        return NS_ERROR_NOT_AVAILABLE;
    }

    *surface = mSurface.get();
    NS_ADDREF(*surface);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::CreateImageData()
{
    /* Should never be called; handled entirely in the quickstub */
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMozImageSmoothingEnabled(bool *retVal)
{
    *retVal = CurrentState().imageSmoothingEnabled;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetMozImageSmoothingEnabled(bool val)
{
    if (val != CurrentState().imageSmoothingEnabled) {
        CurrentState().imageSmoothingEnabled = val;
        DirtyAllStyles();
    }

    return NS_OK;
}

static PRUint8 g2DContextLayerUserData;

class CanvasRenderingContext2DUserData : public LayerUserData {
public:
  CanvasRenderingContext2DUserData(nsHTMLCanvasElement *aContent)
    : mContent(aContent) {}
  static void DidTransactionCallback(void* aData)
  {
    static_cast<CanvasRenderingContext2DUserData*>(aData)->mContent->MarkContextClean();
  }

private:
  nsRefPtr<nsHTMLCanvasElement> mContent;
};

already_AddRefed<CanvasLayer>
nsCanvasRenderingContext2D::GetCanvasLayer(nsDisplayListBuilder* aBuilder,
                                           CanvasLayer *aOldLayer,
                                           LayerManager *aManager)
{
    if (!EnsureSurface()) 
        return nsnull;

    if (!mResetLayer && aOldLayer &&
        aOldLayer->HasUserData(&g2DContextLayerUserData)) {
        NS_ADDREF(aOldLayer);
        return aOldLayer;
    }

    nsRefPtr<CanvasLayer> canvasLayer = aManager->CreateCanvasLayer();
    if (!canvasLayer) {
        NS_WARNING("CreateCanvasLayer returned null!");
        return nsnull;
    }
    CanvasRenderingContext2DUserData *userData = nsnull;
    if (aBuilder->IsPaintingToWindow()) {
      // Make the layer tell us whenever a transaction finishes (including
      // the current transaction), so we can clear our invalidation state and
      // start invalidating again. We need to do this for the layer that is
      // being painted to a window (there shouldn't be more than one at a time,
      // and if there is, flushing the invalidation state more often than
      // necessary is harmless).

      // The layer will be destroyed when we tear down the presentation
      // (at the latest), at which time this userData will be destroyed,
      // releasing the reference to the element.
      // The userData will receive DidTransactionCallbacks, which flush the
      // the invalidation state to indicate that the canvas is up to date.
      userData = new CanvasRenderingContext2DUserData(HTMLCanvasElement());
      canvasLayer->SetDidTransactionCallback(
              CanvasRenderingContext2DUserData::DidTransactionCallback, userData);
    }
    canvasLayer->SetUserData(&g2DContextLayerUserData, userData);

    CanvasLayer::Data data;

    data.mSurface = mSurface.get();
    data.mSize = nsIntSize(mWidth, mHeight);

    canvasLayer->Initialize(data);
    PRUint32 flags = mOpaque ? Layer::CONTENT_OPAQUE : 0;
    canvasLayer->SetContentFlags(flags);
    canvasLayer->Updated();

    mResetLayer = false;

    return canvasLayer.forget();
}

bool
nsCanvasRenderingContext2D::ShouldForceInactiveLayer(LayerManager *aManager)
{
    return !aManager->CanUseCanvasLayerForSize(gfxIntSize(mWidth, mHeight));
}

void
nsCanvasRenderingContext2D::MarkContextClean()
{
    if (mInvalidateCount > 0) {
        mPredictManyRedrawCalls = mInvalidateCount > kCanvasMaxInvalidateCount;
    }
    mIsEntireFrameInvalid = false;
    mInvalidateCount = 0;
}

