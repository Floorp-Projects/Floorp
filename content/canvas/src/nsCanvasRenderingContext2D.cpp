/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include "prmem.h"

#include "nsIServiceManager.h"

#include "nsContentUtils.h"

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIVariant.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIDOMHTMLCanvasElement.h"
#include "nsICanvasElement.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIImage.h"
#include "nsIFrame.h"
#include "nsDOMError.h"
#include "nsIScriptError.h"

#include "nsICSSParser.h"
#include "nsICSSStyleRule.h"
#include "nsInspectorCSSUtils.h"
#include "nsStyleSet.h"

#include "nsPrintfCString.h"

#include "nsReadableUtils.h"

#include "nsColor.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsGfxCIID.h"
#include "nsIScriptSecurityManager.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIXPConnect.h"
#include "jsapi.h"
#include "jsnum.h"

#include "nsTArray.h"

#include "imgIEncoder.h"

#include "gfxContext.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "gfxFont.h"
#include "gfxTextRunCache.h"
#include "gfxBlur.h"

#include "nsFrameManager.h"

#include "nsBidiPresUtils.h"

#ifdef MOZ_MEDIA
#include "nsHTMLVideoElement.h"
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#define M_PI_2		1.57079632679489661923
#endif

static PRBool CheckSaneSubrectSize (PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h, PRInt32 realWidth, PRInt32 realHeight);

/* Float validation stuff */

#define VALIDATE(_f)  if (!JSDOUBLE_IS_FINITE(_f)) return PR_FALSE

/* These must take doubles as args, because JSDOUBLE_IS_FINITE expects
 * to take the address of its argument; we can't cast/convert in the
 * macro.
 */

static PRBool FloatValidate (double f1) {
    VALIDATE(f1);
    return PR_TRUE;
}

static PRBool FloatValidate (double f1, double f2) {
    VALIDATE(f1); VALIDATE(f2);
    return PR_TRUE;
}

static PRBool FloatValidate (double f1, double f2, double f3) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3);
    return PR_TRUE;
}

static PRBool FloatValidate (double f1, double f2, double f3, double f4) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3); VALIDATE(f4);
    return PR_TRUE;
}

static PRBool FloatValidate (double f1, double f2, double f3, double f4, double f5) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3); VALIDATE(f4); VALIDATE(f5);
    return PR_TRUE;
}

static PRBool FloatValidate (double f1, double f2, double f3, double f4, double f5, double f6) {
    VALIDATE(f1); VALIDATE(f2); VALIDATE(f3); VALIDATE(f4); VALIDATE(f5); VALIDATE(f6);
    return PR_TRUE;
}

#undef VALIDATE

/**
 ** nsCanvasGradient
 **/
#define NS_CANVASGRADIENT_PRIVATE_IID \
    { 0x491d39d8, 0x4058, 0x42bd, { 0xac, 0x76, 0x70, 0xd5, 0x62, 0x7f, 0x02, 0x10 } }
class nsCanvasGradient : public nsIDOMCanvasGradient
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASGRADIENT_PRIVATE_IID)

    nsCanvasGradient(gfxPattern* pat, nsICSSParser* cssparser)
        : mPattern(pat), mCSSParser(cssparser)
    {
    }

    void Apply(gfxContext* ctx) {
        ctx->SetPattern(mPattern);
    }

    /* nsIDOMCanvasGradient */
    NS_IMETHOD AddColorStop (float offset,
                             const nsAString& colorstr)
    {
        nscolor color;

        if (!FloatValidate(offset))
            return NS_ERROR_DOM_SYNTAX_ERR;

        if (offset < 0.0 || offset > 1.0)
            return NS_ERROR_DOM_INDEX_SIZE_ERR;

        nsresult rv = mCSSParser->ParseColorString(nsString(colorstr), nsnull, 0, &color);
        if (NS_FAILED(rv))
            return NS_ERROR_DOM_SYNTAX_ERR;

        mPattern->AddColorStop(offset, gfxRGBA(color));

        return NS_OK;
    }

    NS_DECL_ISUPPORTS

protected:
    nsRefPtr<gfxPattern> mPattern;
    nsCOMPtr<nsICSSParser> mCSSParser;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCanvasGradient, NS_CANVASGRADIENT_PRIVATE_IID)

NS_IMPL_ADDREF(nsCanvasGradient)
NS_IMPL_RELEASE(nsCanvasGradient)

NS_INTERFACE_MAP_BEGIN(nsCanvasGradient)
  NS_INTERFACE_MAP_ENTRY(nsCanvasGradient)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasGradient)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasGradient)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/**
 ** nsCanvasPattern
 **/
#define NS_CANVASPATTERN_PRIVATE_IID \
    { 0xb85c6c8a, 0x0624, 0x4530, { 0xb8, 0xee, 0xff, 0xdf, 0x42, 0xe8, 0x21, 0x6d } }
class nsCanvasPattern : public nsIDOMCanvasPattern
{
public:
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASPATTERN_PRIVATE_IID)

    nsCanvasPattern(gfxPattern* pat,
                    nsIPrincipal* principalForSecurityCheck,
                    PRBool forceWriteOnly)
        : mPattern(pat),
          mPrincipal(principalForSecurityCheck),
          mForceWriteOnly(forceWriteOnly)
    {
    }

    void Apply(gfxContext* ctx) {
        ctx->SetPattern(mPattern);
    }
    
    nsIPrincipal* Principal() { return mPrincipal; }
    PRBool GetForceWriteOnly() { return mForceWriteOnly; }

    NS_DECL_ISUPPORTS

protected:
    nsRefPtr<gfxPattern> mPattern;
    nsCOMPtr<nsIPrincipal> mPrincipal;
    PRPackedBool mForceWriteOnly;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsCanvasPattern, NS_CANVASPATTERN_PRIVATE_IID)

NS_IMPL_ADDREF(nsCanvasPattern)
NS_IMPL_RELEASE(nsCanvasPattern)

NS_INTERFACE_MAP_BEGIN(nsCanvasPattern)
  NS_INTERFACE_MAP_ENTRY(nsCanvasPattern)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasPattern)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasPattern)
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

NS_INTERFACE_MAP_BEGIN(nsTextMetrics)
  NS_INTERFACE_MAP_ENTRY(nsTextMetrics)
  NS_INTERFACE_MAP_ENTRY(nsIDOMTextMetrics)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(TextMetrics)
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
    NS_IMETHOD SetCanvasElement(nsICanvasElement* aParentCanvas);
    NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
    NS_IMETHOD Render(gfxContext *ctx);
    NS_IMETHOD GetInputStream(const char* aMimeType,
                              const PRUnichar* aEncoderOptions,
                              nsIInputStream **aStream);
    NS_IMETHOD GetThebesSurface(gfxASurface **surface);
    NS_IMETHOD SetIsOpaque(PRBool isOpaque);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMCanvasRenderingContext2D interface
    NS_DECL_NSIDOMCANVASRENDERINGCONTEXT2D

    enum Style {
        STYLE_STROKE = 0,
        STYLE_FILL,
        STYLE_SHADOW,
        STYLE_MAX
    };

protected:
    // destroy thebes/image stuff, in preparation for possibly recreating
    void Destroy();

    // Some helpers.  Doesn't modify acolor on failure.
    nsresult SetStyleFromVariant(nsIVariant* aStyle, Style aWhichStyle);
    void StyleColorToString(const nscolor& aColor, nsAString& aStr);

    void DirtyAllStyles();
    /**
     * applies the given style as the current source. If the given style is
     * a solid color, aUseGlobalAlpha indicates whether to multiply the alpha
     * by global alpha, and is ignored otherwise.
     */
    void ApplyStyle(Style aWhichStyle, PRBool aUseGlobalAlpha = PR_TRUE);
    
    // If aPrincipal is not subsumed by this canvas element, then
    // we make the canvas write-only so bad guys can't extract the pixel
    // data.  If forceWriteOnly is set, we force write only to be set
    // and ignore aPrincipal.  (This is used for when the original data came
    // from a <canvas> that had write-only set.)
    void DoDrawImageSecurityCheck(nsIPrincipal* aPrincipal,
                                  PRBool forceWriteOnly);

    // Member vars
    PRInt32 mWidth, mHeight;
    PRPackedBool mValid;
    PRPackedBool mOpaque;

    // the canvas element informs us when it's going away,
    // so these are not nsCOMPtrs
    nsICanvasElement* mCanvasElement;

    // our CSS parser, for colors and whatnot
    nsCOMPtr<nsICSSParser> mCSSParser;

    // yay thebes
    nsRefPtr<gfxContext> mThebes;
    nsRefPtr<gfxASurface> mSurface;

    PRUint32 mSaveCount;

    /**
     * Flag to avoid duplicate calls to InvalidateFrame. Set to true whenever
     * Redraw is called, reset to false when Render is called.
     */
    PRBool mIsFrameInvalid;

    /**
     * Returns true iff the the given operator should affect areas of the
     * destination where the source is transparent. Among other things, this
     * implies that a fully transparent source would still affect the canvas.
     */
    PRBool OperatorAffectsUncoveredAreas(gfxContext::GraphicsOperator op) const
    {
        return PR_FALSE;
        // XXX certain operators cause 2d.composite.uncovered.* tests to fail
#if 0
        return op == gfxContext::OPERATOR_IN ||
               op == gfxContext::OPERATOR_OUT ||
               op == gfxContext::OPERATOR_DEST_IN ||
               op == gfxContext::OPERATOR_DEST_ATOP ||
               op == gfxContext::OPERATOR_SOURCE;
#endif
    }

    /**
     * Returns true iff a shadow should be drawn along with a
     * drawing operation.
     */
    PRBool NeedToDrawShadow()
    {
        ContextState& state = CurrentState();

        // special case the default values as a "don't draw shadows" mode
        PRBool doDraw = state.colorStyles[STYLE_SHADOW] != 0 ||
                        state.shadowOffset.x != 0 ||
                        state.shadowOffset.y != 0;
        PRBool isColor = CurrentState().StyleIsColor(STYLE_SHADOW);

        // if not using one of the cooky operators, can avoid drawing a shadow
        // if the color is fully transparent
        return (doDraw || !isColor) && (!isColor ||
               NS_GET_A(state.colorStyles[STYLE_SHADOW]) != 0 ||
               OperatorAffectsUncoveredAreas(mThebes->CurrentOperator()));
    }

    /**
     * Checks the current state to determine if an intermediate surface would
     * be necessary to complete a drawing operation. Does not check the
     * condition pertaining to global alpha and patterns since that does not
     * pertain to all drawing operations.
     */
    PRBool NeedToUseIntermediateSurface()
    {
        // certain operators always need an intermediate surface, except
        // with quartz since quartz does compositing differently than cairo
        return mThebes->OriginalSurface()->GetType() != gfxASurface::SurfaceTypeQuartz &&
               OperatorAffectsUncoveredAreas(mThebes->CurrentOperator());

        // XXX there are other unhandled cases but they should be investigated
        // first to ensure we aren't using an intermediate surface unecessarily
    }

    /**
     * Returns true iff the current source is such that global alpha would not
     * be handled correctly without the use of an intermediate surface.
     */
    PRBool NeedIntermediateSurfaceToHandleGlobalAlpha(Style aWhichStyle)
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
     */
    nsresult DrawPath(Style style);

    /**
     * Draws a rectangle in the given style; used by FillRect and StrokeRect.
     */
    nsresult DrawRect(const gfxRect& rect, Style style);

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

    gfxFontGroup *GetCurrentFontStyle();

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
    PRPackedBool mDirtyStyle[STYLE_MAX];

    // state stack handling
    class ContextState {
    public:
        ContextState() : shadowOffset(0.0, 0.0),
                         globalAlpha(1.0),             
                         shadowBlur(0.0),
                         textAlign(TEXT_ALIGN_START),
                         textBaseline(TEXT_BASELINE_ALPHABETIC) { }

        ContextState(const ContextState& other)
            : shadowOffset(other.shadowOffset),
              globalAlpha(other.globalAlpha),
              shadowBlur(other.shadowBlur),
              font(other.font),
              fontGroup(other.fontGroup),
              textAlign(other.textAlign),
              textBaseline(other.textBaseline)
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
        inline PRBool StyleIsColor(Style whichStyle) const
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
    };

    nsTArray<ContextState> mStyleStack;

    inline ContextState& CurrentState() {
        return mStyleStack[mSaveCount];
    }

    // stolen from nsJSUtils
    static PRBool ConvertJSValToUint32(PRUint32* aProp, JSContext* aContext,
                                       jsval aValue);
    static PRBool ConvertJSValToXPCObject(nsISupports** aSupports, REFNSIID aIID,
                                          JSContext* aContext, jsval aValue);
    static PRBool ConvertJSValToDouble(double* aProp, JSContext* aContext,
                                       jsval aValue);

    // thebes helpers
    nsresult ThebesSurfaceFromElement(nsIDOMElement *imgElt,
                                      PRBool forceCopy,
                                      gfxASurface **aSurface,
                                      PRInt32 *widthOut, PRInt32 *heightOut,
                                      nsIPrincipal **prinOut,
                                      PRBool *forceWriteOnlyOut);

    // other helpers
    void GetAppUnitsValues(PRUint32 *perDevPixel, PRUint32 *perCSSPixel) {
        // If we don't have a canvas element, we just return something generic.
        PRUint32 devPixel = 60;
        PRUint32 cssPixel = 60;

        nsCOMPtr<nsINode> elem = do_QueryInterface(mCanvasElement);
        if (elem) {
            nsIDocument *doc = elem->GetOwnerDoc();
            if (!doc) goto FINISH;
            nsIPresShell *ps = doc->GetPrimaryShell();
            if (!ps) goto FINISH;
            nsPresContext *pc = ps->GetPresContext();
            if (!pc) goto FINISH;
            devPixel = pc->AppUnitsPerDevPixel();
            cssPixel = pc->AppUnitsPerCSSPixel();
        }

      FINISH:
        if (perDevPixel)
            *perDevPixel = devPixel;
        if (perCSSPixel)
            *perCSSPixel = cssPixel;
    }

    friend struct nsCanvasBidiProcessor;
};

NS_IMPL_ADDREF(nsCanvasRenderingContext2D)
NS_IMPL_RELEASE(nsCanvasRenderingContext2D)

NS_INTERFACE_MAP_BEGIN(nsCanvasRenderingContext2D)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasRenderingContext2D)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCanvasRenderingContext2D)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasRenderingContext2D)
NS_INTERFACE_MAP_END

/**
 ** CanvasRenderingContext2D impl
 **/

nsresult
NS_NewCanvasRenderingContext2D(nsIDOMCanvasRenderingContext2D** aResult)
{
    nsRefPtr<nsIDOMCanvasRenderingContext2D> ctx = new nsCanvasRenderingContext2D();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    *aResult = ctx.forget().get();
    return NS_OK;
}

nsCanvasRenderingContext2D::nsCanvasRenderingContext2D()
    : mValid(PR_FALSE), mOpaque(PR_FALSE), mCanvasElement(nsnull),
      mSaveCount(0), mIsFrameInvalid(PR_FALSE), mStyleStack(20)
{
}

nsCanvasRenderingContext2D::~nsCanvasRenderingContext2D()
{
    Destroy();
}

void
nsCanvasRenderingContext2D::Destroy()
{
    mSurface = nsnull;
    mThebes = nsnull;
    mValid = PR_FALSE;
    mIsFrameInvalid = PR_FALSE;
}

nsresult
nsCanvasRenderingContext2D::SetStyleFromVariant(nsIVariant* aStyle, Style aWhichStyle)
{
    nsresult rv;
    nscolor color;

    PRUint16 paramType;
    rv = aStyle->GetDataType(&paramType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (paramType == nsIDataType::VTYPE_DOMSTRING ||
        paramType == nsIDataType::VTYPE_WSTRING_SIZE_IS) {
        nsAutoString str;

        if (paramType == nsIDataType::VTYPE_DOMSTRING) {
            rv = aStyle->GetAsDOMString(str);
        } else {
            rv = aStyle->GetAsAString(str);
        }
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mCSSParser->ParseColorString(str, nsnull, 0, &color);
        if (NS_FAILED(rv)) {
            // Error reporting happens inside the CSS parser
            return NS_OK;
        }

        CurrentState().SetColorStyle(aWhichStyle, color);

        mDirtyStyle[aWhichStyle] = PR_TRUE;
        return NS_OK;
    } else if (paramType == nsIDataType::VTYPE_INTERFACE ||
               paramType == nsIDataType::VTYPE_INTERFACE_IS)
    {
        nsID *iid;
        nsCOMPtr<nsISupports> iface;
        rv = aStyle->GetAsInterface(&iid, getter_AddRefs(iface));

        nsCOMPtr<nsCanvasGradient> grad(do_QueryInterface(iface));
        if (grad) {
            CurrentState().SetGradientStyle(aWhichStyle, grad);
            mDirtyStyle[aWhichStyle] = PR_TRUE;
            return NS_OK;
        }

        nsCOMPtr<nsCanvasPattern> pattern(do_QueryInterface(iface));
        if (pattern) {
            CurrentState().SetPatternStyle(aWhichStyle, pattern);
            mDirtyStyle[aWhichStyle] = PR_TRUE;
            return NS_OK;
        }
    }

    nsContentUtils::ReportToConsole(
        nsContentUtils::eDOM_PROPERTIES,
        "UnexpectedCanvasVariantStyle",
        nsnull, 0,
        nsnull,
        EmptyString(), 0, 0,
        nsIScriptError::warningFlag,
        "Canvas");

    return NS_OK;
}

void
nsCanvasRenderingContext2D::StyleColorToString(const nscolor& aColor, nsAString& aStr)
{
    if (NS_GET_A(aColor) == 255) {
        CopyUTF8toUTF16(nsPrintfCString(100, "#%02x%02x%02x",
                                        NS_GET_R(aColor),
                                        NS_GET_G(aColor),
                                        NS_GET_B(aColor)),
                        aStr);
    } else {
        // "%0.5f" in nsPrintfCString would use the locale-specific
        // decimal separator. That's why we have to do this:
        PRUint32 alpha = NS_GET_A(aColor) * 100000 / 255;
        CopyUTF8toUTF16(nsPrintfCString(100, "rgba(%d, %d, %d, 0.%d)",
                                        NS_GET_R(aColor),
                                        NS_GET_G(aColor),
                                        NS_GET_B(aColor),
                                        alpha),
                        aStr);
    }
}

void
nsCanvasRenderingContext2D::DirtyAllStyles()
{
    for (int i = 0; i < STYLE_MAX; i++) {
        mDirtyStyle[i] = PR_TRUE;
    }
}

void
nsCanvasRenderingContext2D::DoDrawImageSecurityCheck(nsIPrincipal* aPrincipal,
                                                     PRBool forceWriteOnly)
{
    // Callers should ensure that mCanvasElement is non-null before calling this
    if (!mCanvasElement) {
        NS_WARNING("DoDrawImageSecurityCheck called without canvas element!");
        return;
    }

    if (mCanvasElement->IsWriteOnly())
        return;

    // If we explicitly set WriteOnly just do it and get out
    if (forceWriteOnly) {
        mCanvasElement->SetWriteOnly();
        return;
    }

    if (aPrincipal == nsnull)
        return;

    nsCOMPtr<nsINode> elem = do_QueryInterface(mCanvasElement);
    if (elem) { // XXXbz How could this actually be null?
        PRBool subsumes;
        nsresult rv =
            elem->NodePrincipal()->Subsumes(aPrincipal, &subsumes);
            
        if (NS_SUCCEEDED(rv) && subsumes) {
            // This canvas has access to that image anyway
            return;
        }
    }

    mCanvasElement->SetWriteOnly();
}

void
nsCanvasRenderingContext2D::ApplyStyle(Style aWhichStyle,
                                       PRBool aUseGlobalAlpha)
{
    if (mLastStyle == aWhichStyle &&
        !mDirtyStyle[aWhichStyle] &&
        aUseGlobalAlpha)
    {
        // nothing to do, this is already the set style
        return;
    }

    // if not using global alpha, don't optimize with dirty bit
    if (aUseGlobalAlpha)
        mDirtyStyle[aWhichStyle] = PR_FALSE;
    mLastStyle = aWhichStyle;

    nsCanvasPattern* pattern = CurrentState().patternStyles[aWhichStyle];
    if (pattern) {
        if (!mCanvasElement)
            return;

        DoDrawImageSecurityCheck(pattern->Principal(),
                                 pattern->GetForceWriteOnly());
        pattern->Apply(mThebes);
        return;
    }

    if (CurrentState().gradientStyles[aWhichStyle]) {
        CurrentState().gradientStyles[aWhichStyle]->Apply(mThebes);
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
    if (!mCanvasElement)
        return NS_OK;

    if (!mIsFrameInvalid) {
        mIsFrameInvalid = PR_TRUE;
        return mCanvasElement->InvalidateFrame();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetDimensions(PRInt32 width, PRInt32 height)
{
    Destroy();

    mWidth = width;
    mHeight = height;

    // Check that the dimensions are sane
    if (gfxASurface::CheckSurfaceSize(gfxIntSize(width, height), 0xffff)) {
        gfxASurface::gfxImageFormat format = gfxASurface::ImageFormatARGB32;
        if (mOpaque)
            format = gfxASurface::ImageFormatRGB24;

        mSurface = gfxPlatform::GetPlatform()->CreateOffscreenSurface
            (gfxIntSize(width, height), format);

        if (mSurface->CairoStatus() == 0) {
            mThebes = new gfxContext(mSurface);
        }
    }

    /* Create dummy surfaces here */
    if (mSurface == nsnull || mSurface->CairoStatus() != 0 ||
        mThebes == nsnull || mThebes->HasError())
    {
        mSurface = new gfxImageSurface(gfxIntSize(1,1), gfxASurface::ImageFormatARGB32);
        mThebes = new gfxContext(mSurface);
    } else {
        mValid = PR_TRUE;
    }

    // set up the initial canvas defaults
    mStyleStack.Clear();
    mSaveCount = 0;

    ContextState *state = mStyleStack.AppendElement();
    state->globalAlpha = 1.0;

    state->colorStyles[STYLE_FILL] = NS_RGB(0,0,0);
    state->colorStyles[STYLE_STROKE] = NS_RGB(0,0,0);
    state->colorStyles[STYLE_SHADOW] = NS_RGBA(0,0,0,0);
    DirtyAllStyles();

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

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetIsOpaque(PRBool isOpaque)
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
nsCanvasRenderingContext2D::Render(gfxContext *ctx)
{
    nsresult rv = NS_OK;

    if (!mValid || !mSurface ||
        mSurface->CairoStatus() ||
        mThebes->HasError())
        return NS_ERROR_FAILURE;

    if (!mSurface)
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxPattern> pat = new gfxPattern(mSurface);

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

    mIsFrameInvalid = PR_FALSE;
    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetInputStream(const char *aMimeType,
                                           const PRUnichar *aEncoderOptions,
                                           nsIInputStream **aStream)
{
    if (!mValid || !mSurface ||
        mSurface->CairoStatus() ||
        mThebes->HasError())
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

//
// nsCanvasRenderingContext2D impl
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetCanvasElement(nsICanvasElement* aCanvasElement)
{
    // don't hold a ref to this!
    mCanvasElement = aCanvasElement;

    // set up our css parser, if necessary
    if (!mCSSParser) {
        mCSSParser = do_CreateInstance("@mozilla.org/content/css-parser;1");
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetCanvas(nsIDOMHTMLCanvasElement **canvas)
{
    if (mCanvasElement == nsnull) {
        *canvas = nsnull;
        return NS_OK;
    }

    return CallQueryInterface(mCanvasElement, canvas);
}

//
// state
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::Save()
{
    ContextState state = CurrentState();
    mStyleStack.AppendElement(state);
    mThebes->Save();
    mSaveCount++;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Restore()
{
    if (mSaveCount == 0)
        return NS_OK;
    if (mSaveCount < 0)
        return NS_ERROR_DOM_INVALID_STATE_ERR;

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
    if (!FloatValidate(x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    mThebes->Scale(x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Rotate(float angle)
{
    if (!FloatValidate(angle))
        return NS_ERROR_DOM_SYNTAX_ERR;

    mThebes->Rotate(angle);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Translate(float x, float y)
{
    if (!FloatValidate(x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    mThebes->Translate(gfxPoint(x, y));
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    if (!FloatValidate(m11,m12,m21,m22,dx,dy))
        return NS_ERROR_DOM_SYNTAX_ERR;

    gfxMatrix matrix(m11, m12, m21, m22, dx, dy);
    mThebes->Multiply(matrix);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    if (!FloatValidate(m11,m12,m21,m22,dx,dy))
        return NS_ERROR_DOM_SYNTAX_ERR;

    gfxMatrix matrix(m11, m12, m21, m22, dx, dy);
    mThebes->SetMatrix(matrix);

    return NS_OK;
}

//
// colors
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetGlobalAlpha(float aGlobalAlpha)
{
    if (!FloatValidate(aGlobalAlpha))
        return NS_ERROR_DOM_SYNTAX_ERR;

    // ignore invalid values, as per spec
    if (aGlobalAlpha < 0.0 || aGlobalAlpha > 1.0)
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
nsCanvasRenderingContext2D::SetStrokeStyle(nsIVariant* aStyle)
{
    return SetStyleFromVariant(aStyle, STYLE_STROKE);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetStrokeStyle(nsIVariant** aStyle)
{
    nsresult rv;

    nsCOMPtr<nsIWritableVariant> var = do_CreateInstance("@mozilla.org/variant;1");
    if (!var)
        return NS_ERROR_FAILURE;
    rv = var->SetWritable(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (CurrentState().patternStyles[STYLE_STROKE]) {
        rv = var->SetAsISupports(CurrentState().patternStyles[STYLE_STROKE]);
        NS_ENSURE_SUCCESS(rv, rv);
    } else if (CurrentState().gradientStyles[STYLE_STROKE]) {
        rv = var->SetAsISupports(CurrentState().gradientStyles[STYLE_STROKE]);
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        nsString styleStr;
        StyleColorToString(CurrentState().colorStyles[STYLE_STROKE], styleStr);

        rv = var->SetAsDOMString(styleStr);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    *aStyle = var.forget().get();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetFillStyle(nsIVariant* aStyle)
{
    return SetStyleFromVariant(aStyle, STYLE_FILL);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetFillStyle(nsIVariant** aStyle)
{
    nsresult rv;

    nsCOMPtr<nsIWritableVariant> var = do_CreateInstance("@mozilla.org/variant;1");
    if (!var)
        return NS_ERROR_FAILURE;
    rv = var->SetWritable(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (CurrentState().patternStyles[STYLE_FILL]) {
        rv = var->SetAsISupports(CurrentState().patternStyles[STYLE_FILL]);
        NS_ENSURE_SUCCESS(rv, rv);
    } else if (CurrentState().gradientStyles[STYLE_FILL]) {
        rv = var->SetAsISupports(CurrentState().gradientStyles[STYLE_FILL]);
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        nsString styleStr;
        StyleColorToString(CurrentState().colorStyles[STYLE_FILL], styleStr);

        rv = var->SetAsDOMString(styleStr);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    *aStyle = var.forget().get();
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
        return NS_ERROR_DOM_SYNTAX_ERR;

    nsRefPtr<gfxPattern> gradpat = new gfxPattern(x0, y0, x1, y1);
    if (!gradpat)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<nsIDOMCanvasGradient> grad = new nsCanvasGradient(gradpat, mCSSParser);
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
        return NS_ERROR_DOM_SYNTAX_ERR;

    nsRefPtr<gfxPattern> gradpat = new gfxPattern(x0, y0, r0, x1, y1, r1);
    if (!gradpat)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<nsIDOMCanvasGradient> grad = new nsCanvasGradient(gradpat, mCSSParser);
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
    nsresult rv;
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

    PRInt32 imgWidth, imgHeight;
    nsCOMPtr<nsIPrincipal> principal;
    PRBool forceWriteOnly = PR_FALSE;
    nsRefPtr<gfxASurface> imgsurf;
    rv = ThebesSurfaceFromElement(image, PR_TRUE,
                                  getter_AddRefs(imgsurf), &imgWidth, &imgHeight,
                                  getter_AddRefs(principal), &forceWriteOnly);
    if (NS_FAILED(rv))
        return rv;

    nsRefPtr<gfxPattern> thebespat = new gfxPattern(imgsurf);

    thebespat->SetExtend(extend);

    nsRefPtr<nsCanvasPattern> pat = new nsCanvasPattern(thebespat, principal,
                                                        forceWriteOnly);
    if (!pat)
        return NS_ERROR_OUT_OF_MEMORY;

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
        return NS_ERROR_DOM_SYNTAX_ERR;
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
        return NS_ERROR_DOM_SYNTAX_ERR;
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
    if (!FloatValidate(blur))
        return NS_ERROR_DOM_SYNTAX_ERR;
    if (blur < 0.0)
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
    nscolor color;

    nsresult rv = mCSSParser->ParseColorString(nsString(colorstr), nsnull, 0, &color);
    if (NS_FAILED(rv)) {
        // Error reporting happens inside the CSS parser
        return NS_OK;
    }

    CurrentState().SetColorStyle(STYLE_SHADOW, color);

    mDirtyStyle[STYLE_SHADOW] = PR_TRUE;

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowColor(nsAString& color)
{
    StyleColorToString(CurrentState().colorStyles[STYLE_SHADOW], color);

    return NS_OK;
}

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
}

static const gfxFloat SIGMA_MAX = 25;

gfxContext*
nsCanvasRenderingContext2D::ShadowInitialize(const gfxRect& extents, gfxAlphaBoxBlur& blur)
{
    gfxIntSize blurRadius;

    gfxFloat sigma = CurrentState().shadowBlur > 8 ? sqrt(CurrentState().shadowBlur) : CurrentState().shadowBlur / 2;
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
    clipExtents.Outset(blurRadius.height, blurRadius.width,
                       blurRadius.height, blurRadius.width);
    drawExtents = drawExtents.Intersect(clipExtents - CurrentState().shadowOffset);

    gfxContext* ctx = blur.Init(drawExtents, blurRadius, nsnull);

    if (!ctx)
        return nsnull;

    return ctx;
}

void
nsCanvasRenderingContext2D::ShadowFinalize(gfxAlphaBoxBlur& blur)
{
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
nsCanvasRenderingContext2D::DrawPath(Style style)
{
    /*
     * Need an intermediate surface when:
     * - globalAlpha != 1 and gradients/patterns are used (need to paint_with_alpha)
     * - certain operators are used and are not on mac (quartz/cairo composite operators don't quite line up)
     */
    PRBool doUseIntermediateSurface = NeedToUseIntermediateSurface() ||
                                      NeedIntermediateSurfaceToHandleGlobalAlpha(style);

    PRBool doDrawShadow = NeedToDrawShadow();

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
            ApplyStyle(style, PR_FALSE);
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

        // don't want operators to be applied twice
        mThebes->SetOperator(gfxContext::OPERATOR_SOURCE);
    }

    ApplyStyle(style);
    if (style == STYLE_FILL)
        mThebes->Fill();
    else
        mThebes->Stroke();

    if (doUseIntermediateSurface) {
        mThebes->PopGroupToSource();
        DirtyAllStyles();

        mThebes->Paint(CurrentState().StyleIsColor(style) ? 1.0 : CurrentState().globalAlpha);
    }

    return NS_OK;
}

//
// rects
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::ClearRect(float x, float y, float w, float h)
{
    if (!FloatValidate(x,y,w,h))
        return NS_ERROR_DOM_SYNTAX_ERR;

    gfxContextPathAutoSaveRestore pathSR(mThebes);
    gfxContextAutoSaveRestore autoSR(mThebes);

    mThebes->SetOperator(gfxContext::OPERATOR_CLEAR);
    mThebes->NewPath();
    mThebes->Rectangle(gfxRect(x, y, w, h));
    mThebes->Fill();

    return Redraw();
}

nsresult
nsCanvasRenderingContext2D::DrawRect(const gfxRect& rect, Style style)
{
    if (!FloatValidate(rect.pos.x, rect.pos.y, rect.size.width, rect.size.height))
        return NS_ERROR_DOM_SYNTAX_ERR;

    gfxContextPathAutoSaveRestore pathSR(mThebes);

    mThebes->NewPath();
    mThebes->Rectangle(rect);

    nsresult rv = DrawPath(style);
    if (NS_FAILED(rv))
        return rv;

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::FillRect(float x, float y, float w, float h)
{
    return DrawRect(gfxRect(x, y, w, h), STYLE_FILL);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::StrokeRect(float x, float y, float w, float h)
{
    return DrawRect(gfxRect(x, y, w, h), STYLE_STROKE);
}

//
// path bits
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::BeginPath()
{
    mThebes->NewPath();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::ClosePath()
{
    mThebes->ClosePath();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Fill()
{
    nsresult rv = DrawPath(STYLE_FILL);
    if (NS_FAILED(rv))
        return rv;
    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Stroke()
{
    nsresult rv = DrawPath(STYLE_STROKE);
    if (NS_FAILED(rv))
        return rv;
    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Clip()
{
    mThebes->Clip();
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::MoveTo(float x, float y)
{
    if (!FloatValidate(x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    mThebes->MoveTo(gfxPoint(x, y));
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::LineTo(float x, float y)
{
    if (!FloatValidate(x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    mThebes->LineTo(gfxPoint(x, y));
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::QuadraticCurveTo(float cpx, float cpy, float x, float y)
{
    if (!FloatValidate(cpx,cpy,x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    // we will always have a current point, since beginPath forces
    // a moveto(0,0)
    gfxPoint c = mThebes->CurrentPoint();
    gfxPoint p(x,y);
    gfxPoint cp(cpx, cpy);

    mThebes->CurveTo((c+cp*2)/3.0, (p+cp*2)/3.0, p);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::BezierCurveTo(float cp1x, float cp1y,
                                          float cp2x, float cp2y,
                                          float x, float y)
{
    if (!FloatValidate(cp1x,cp1y,cp2x,cp2y,x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    mThebes->CurveTo(gfxPoint(cp1x, cp1y),
                     gfxPoint(cp2x, cp2y),
                     gfxPoint(x, y));

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::ArcTo(float x1, float y1, float x2, float y2, float radius)
{
    if (!FloatValidate(x1,y1,x2,y2,radius))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (radius <= 0)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    /* This is an adaptation of the cairo_arc_to patch from Behdad
     * Esfahbod; once that patch is accepted, we should remove this
     * and just call cairo_arc_to() directly.
     */
    
    double angle0, angle1, angle2, angled;
    double d0, d2;
    double sin_, cos_;
    double dc;
    int forward;

    gfxPoint p0 = mThebes->CurrentPoint();

    angle0 = atan2 (p0.y - y1, p0.x - x1); /* angle from (x1,y1) to (p0.x,p0.y) */
    angle2 = atan2 (y2 - y1, x2 - x1); /* angle from (x1,y1) to (x2,y2) */
    angle1 = (angle0 + angle2) / 2;    /* angle from (x1,y1) to (xc,yc) */

    angled = angle2 - angle0;          /* the angle (p0.x,p0.y)--(x1,y1)--(x2,y2) */

    /* Shall we go forward or backward? */
    if (angled > M_PI || (angled < 0 && angled > -M_PI)) {
        angle1 += M_PI;
        angled = 2 * M_PI - angled;
        forward = 1;
    } else {
        double tmp;
        tmp = angle0;
        angle0 = angle2;
        angle2 = tmp;
        forward = 0;
    }

    angle0 += M_PI_2; /* angle from (xc,yc) to (p0.x,p0.y) */
    angle2 -= M_PI_2; /* angle from (xc,yc) to (x2,y2) */
    angled /= 2;      /* the angle (p0.x,p0.y)--(x1,y1)--(xc,yc) */


    /* distance from (x1,y1) to (p0.x,p0.y) */
    d0 = sqrt ((p0.x-x1)*(p0.x-x1)+(p0.y-y1)*(p0.y-y1));
    /* distance from (x2,y2) to (p0.x,p0.y) */
    d2 = sqrt ((x2-x1)*(x2-x1)+(y2-y1)*(y2-y1));

    dc = -1;
    sin_ = sin(angled);
    cos_ = cos(angled);
    if (fabs(cos_) >= 1e-5) { /* the arc may not fit */
        /* min distance of end-points from corner */
        double min_d = d0 < d2 ? d0 : d2;
        /* max radius of an arc that fits */
        double max_r = min_d * sin_ / cos_;

        if (radius > max_r) {
            /* arc with requested radius doesn't fit */
            radius = (float) max_r;
            dc = min_d / cos_; /* distance of (xc,yc) from (x1,y1) */
        }
    }

    if (dc < 0)
        dc = radius / sin_; /* distance of (xc,yc) from (x1,y1) */


    /* find (cx,cy), the center of the arc */
    gfxPoint c(x1 + sin(angle1) * dc, y1 + cos(angle1) * dc);

    /* the arc operation draws the line from current point (p0.x,p0.y)
     * to arc center too. */

    if (forward)
        mThebes->Arc(c, radius, angle0, angle2);
    else
        mThebes->NegativeArc(c, radius, angle2, angle0);

    mThebes->LineTo(gfxPoint(x2, y2));

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Arc(float x, float y, float r, float startAngle, float endAngle, int ccw)
{
    if (!FloatValidate(x,y,r,startAngle,endAngle))
        return NS_ERROR_DOM_SYNTAX_ERR;

    gfxPoint p(x,y);

    if (ccw)
        mThebes->NegativeArc(p, r, startAngle, endAngle);
    else
        mThebes->Arc(p, r, startAngle, endAngle);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Rect(float x, float y, float w, float h)
{
    if (!FloatValidate(x,y,w,h))
        return NS_ERROR_DOM_SYNTAX_ERR;

    mThebes->Rectangle(gfxRect(x, y, w, h));
    return NS_OK;
}

//
// text
//

/**
 * Helper function for SetFont that creates a style rule for the given font.
 * @param aFont The CSS font string
 * @param aCSSParser The CSS parser of the canvas rendering context
 * @param aNode The canvas element
 * @param aResult Pointer in which to place the new style rule.
 * @remark Assumes all pointer arguments are non-null.
 */
static nsresult
CreateFontStyleRule(const nsAString& aFont,
                    nsICSSParser* aCSSParser,
                    nsINode* aNode,
                    nsICSSStyleRule** aResult)
{
    nsresult rv;

    nsCOMPtr<nsICSSStyleRule> rule;
    PRBool changed;

    nsIPrincipal* principal = aNode->NodePrincipal();
    nsIDocument* document = aNode->GetOwnerDoc();

    nsIURI* docURL = document->GetDocumentURI();
    nsIURI* baseURL = document->GetBaseURI();

    rv = aCSSParser->ParseStyleAttribute(
            EmptyString(),
            docURL,
            baseURL,
            principal,
            getter_AddRefs(rule));
    if (NS_FAILED(rv))
        return rv;

    rv = aCSSParser->ParseProperty(eCSSProperty_font,
                                   aFont,
                                   docURL,
                                   baseURL,
                                   principal,
                                   rule->GetDeclaration(),
                                   &changed);
    if (NS_FAILED(rv))
        return rv;

    // set line height to normal, as per spec
    rv = aCSSParser->ParseProperty(eCSSProperty_line_height,
                                   NS_LITERAL_STRING("normal"),
                                   docURL,
                                   baseURL,
                                   principal,
                                   rule->GetDeclaration(),
                                   &changed);
    if (NS_FAILED(rv))
        return rv;

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
    if (!content) {
        NS_WARNING("Canvas element must be an nsIContent and non-null");
        return NS_ERROR_FAILURE;
    }

    nsIDocument* document = content->GetOwnerDoc();

    nsIPresShell* presShell = document->GetPrimaryShell();
    if (!presShell)
        return NS_ERROR_FAILURE;

    nsCString langGroup;
    presShell->GetPresContext()->GetLangGroup()->ToUTF8String(langGroup);

    nsCOMArray<nsIStyleRule> rules;

    nsCOMPtr<nsICSSStyleRule> rule;
    rv = CreateFontStyleRule(font, mCSSParser.get(), content.get(), getter_AddRefs(rule));
    if (NS_FAILED(rv))
        return rv;

    rules.AppendObject(rule);

    nsStyleSet* styleSet = presShell->StyleSet();

    // have to get a parent style context for inherit-like relative
    // values (2em, bolder, etc.)
    nsRefPtr<nsStyleContext> parentContext;

    if (content->IsInDoc()) {
        // inherit from the canvas element
        parentContext = nsInspectorCSSUtils::GetStyleContextForContent(
                content,
                nsnull,
                presShell);
    } else {
        // otherwise inherit from default (10px sans-serif)
        nsCOMPtr<nsICSSStyleRule> parentRule;
        rv = CreateFontStyleRule(NS_LITERAL_STRING("10px sans-serif"),
                                 mCSSParser.get(),
                                 content.get(),
                                 getter_AddRefs(parentRule));
        if (NS_FAILED(rv))
            return rv;
        nsCOMArray<nsIStyleRule> parentRules;
        parentRules.AppendObject(parentRule);
        parentContext = styleSet->ResolveStyleForRules(nsnull, parentRules);
    }

    if (!parentContext)
        return NS_ERROR_FAILURE;

    nsRefPtr<nsStyleContext> sc = styleSet->ResolveStyleForRules(parentContext, rules);
    if (!sc)
        return NS_ERROR_FAILURE;
    const nsStyleFont* fontStyle = sc->GetStyleFont();

    NS_ASSERTION(fontStyle, "Could not obtain font style");

    // use CSS pixels instead of dev pixels to avoid being affected by page zoom
    const PRUint32 aupcp = nsPresContext::AppUnitsPerCSSPixel();
    // un-zoom the font size to avoid being affected by text-only zoom
    const nscoord fontSize = nsStyleFont::UnZoomText(parentContext->PresContext(), fontStyle->mFont.size);

    PRBool printerFont = (presShell->GetPresContext()->Type() == nsPresContext::eContext_PrintPreview ||
                          presShell->GetPresContext()->Type() == nsPresContext::eContext_Print);

    gfxFontStyle style(fontStyle->mFont.style,
                       fontStyle->mFont.weight,
                       NSAppUnitsToFloatPixels(fontSize, aupcp),
                       langGroup,
                       fontStyle->mFont.sizeAdjust,
                       fontStyle->mFont.systemFont,
                       fontStyle->mFont.familyNameQuirks,
                       printerFont);

    CurrentState().fontGroup = gfxPlatform::GetPlatform()->CreateFontGroup(fontStyle->mFont.name, &style, presShell->GetPresContext()->GetUserFontSet());
    NS_ASSERTION(CurrentState().fontGroup, "Could not get font group");
    CurrentState().font = font;
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
    // spec says to not throw error for invalid arg, but do it anyway
    else
        return NS_ERROR_INVALID_ARG;

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
    default:
        NS_ASSERTION(0, "textAlign holds invalid value");
        return NS_ERROR_FAILURE;
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
    // spec says to not throw error for invalid arg, but do it anyway
    else
        return NS_ERROR_INVALID_ARG;
    
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
    default:
        NS_ASSERTION(0, "textBaseline holds invalid value");
        return NS_ERROR_FAILURE;
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
        mTextRun = gfxTextRunCache::MakeTextRun(text,
                                                length,
                                                mFontgrp,
                                                mThebes,
                                                mAppUnitsPerDevPixel,
                                                direction==NSBIDI_RTL ? gfxTextRunFactory::TEXT_IS_RTL : 0);
    }

    virtual nscoord GetWidth()
    {
        gfxTextRun::Metrics textRunMetrics = mTextRun->MeasureText(0,
                                                                   mTextRun->GetLength(),
                                                                   mDoMeasureBoundingBox,
                                                                   mThebes,
                                                                   nsnull);

        // this only measures the height; the total width is gotten from the
        // the return value of ProcessText.
        if (mDoMeasureBoundingBox) {
            textRunMetrics.mBoundingBox.Scale(1.0 / mAppUnitsPerDevPixel);
            mBoundingBox = mBoundingBox.Union(textRunMetrics.mBoundingBox);
        }

        return static_cast<nscoord>(textRunMetrics.mAdvanceWidth/gfxFloat(mAppUnitsPerDevPixel));
    }

    virtual void DrawText(nscoord xOffset, nscoord width)
    {
        gfxPoint point = mPt;
        point.x += xOffset * mAppUnitsPerDevPixel;

        // offset is given in terms of left side of string
        if (mTextRun->IsRightToLeft())
            point.x += width * mAppUnitsPerDevPixel;

        // stroke or fill the text depending on operation
        if (mOp == nsCanvasRenderingContext2D::TEXT_DRAW_OPERATION_STROKE)
            mTextRun->DrawToPath(mThebes,
                                 point,
                                 0,
                                 mTextRun->GetLength(),
                                 nsnull,
                                 nsnull);
        else
            // mOp == TEXT_DRAW_OPERATION_FILL
            mTextRun->Draw(mThebes,
                           point,
                           0,
                           mTextRun->GetLength(),
                           nsnull,
                           nsnull,
                           nsnull);
    }

    // current text run
    gfxTextRunCache::AutoTextRun mTextRun;

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
    PRBool mDoMeasureBoundingBox;
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
    if (!content) {
        NS_WARNING("Canvas element must be an nsIContent and non-null");
        return NS_ERROR_FAILURE;
    }

    nsIDocument* document = content->GetOwnerDoc();

    nsIPresShell* presShell = document->GetPrimaryShell();
    if (!presShell)
        return NS_ERROR_FAILURE;

    nsBidiPresUtils* bidiUtils = presShell->GetPresContext()->GetBidiUtils();
    if (!bidiUtils)
        return NS_ERROR_FAILURE;

    // replace all the whitespace characters with U+0020 SPACE
    nsAutoString textToDraw(aRawText);
    TextReplaceWhitespaceCharacters(textToDraw);

    // for now, default to ltr if not in doc
    PRBool isRTL = PR_FALSE;

    if (content->IsInDoc()) {
        // try to find the closest context
        nsRefPtr<nsStyleContext> canvasStyle =
            nsInspectorCSSUtils::GetStyleContextForContent(content,
                                                           nsnull,
                                                           presShell);
        if (!canvasStyle)
            return NS_ERROR_FAILURE;
        isRTL = canvasStyle->GetStyleVisibility()->mDirection ==
            NS_STYLE_DIRECTION_RTL;
    }

    // don't need to take care of these with stroke since Stroke() does that
    PRBool doDrawShadow = aOp == TEXT_DRAW_OPERATION_FILL && NeedToDrawShadow();
    PRBool doUseIntermediateSurface = aOp == TEXT_DRAW_OPERATION_FILL &&
        (NeedToUseIntermediateSurface() || NeedIntermediateSurfaceToHandleGlobalAlpha(STYLE_FILL));

    nsCanvasBidiProcessor processor;

    GetAppUnitsValues(&processor.mAppUnitsPerDevPixel, NULL);
    processor.mPt = gfxPoint(aX, aY);
    processor.mThebes = mThebes;
    processor.mOp = aOp;
    processor.mBoundingBox = gfxRect(0, 0, 0, 0);
    // need to measure size if using an intermediate surface for drawing
    processor.mDoMeasureBoundingBox = doDrawShadow;

    processor.mFontgrp = GetCurrentFontStyle();
    NS_ASSERTION(processor.mFontgrp, "font group is null");

    nscoord totalWidth;

    // calls bidi algo twice since it needs the full text width and the
    // bounding boxes before rendering anything
    rv = bidiUtils->ProcessText(textToDraw.get(),
                                textToDraw.Length(),
                                isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                presShell->GetPresContext(),
                                processor,
                                nsBidiPresUtils::MODE_MEASURE,
                                nsnull,
                                0,
                                &totalWidth);
    if (NS_FAILED(rv))
        return rv;

    if (aWidth)
        *aWidth = static_cast<float>(totalWidth);

    // if only measuring, don't need to do any more work
    if (aOp==TEXT_DRAW_OPERATION_MEASURE)
        return NS_OK;

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
    case TEXT_BASELINE_TOP:
        anchorY = fontMetrics.emAscent;
        break;
    case TEXT_BASELINE_HANGING:
        anchorY = 0; // currently unavailable
        break;
    case TEXT_BASELINE_MIDDLE:
        anchorY = (fontMetrics.emAscent - fontMetrics.emDescent) * .5f;
        break;
    case TEXT_BASELINE_ALPHABETIC:
        anchorY = 0;
        break;
    case TEXT_BASELINE_IDEOGRAPHIC:
        anchorY = 0; // currently unvailable
        break;
    case TEXT_BASELINE_BOTTOM:
        anchorY = -fontMetrics.emDescent;
        break;
    default:
        NS_ASSERTION(0, "mTextBaseline holds invalid value");
        return NS_ERROR_FAILURE;
    }

    processor.mPt.y += anchorY;

    // correct bounding box to get it to be the correct size/position
    processor.mBoundingBox.size.width = totalWidth;
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

    // don't ever need to measure the bounding box twice
    processor.mDoMeasureBoundingBox = PR_FALSE;

    if (doDrawShadow) {
        // for some reason the box is too tight, probably rounding error
        processor.mBoundingBox.Outset(2.0);

        // this is unnecessarily big is max-width scaling is involved, but it
        // will still produce correct output
        gfxRect drawExtents = mThebes->UserToDevice(processor.mBoundingBox);
        gfxAlphaBoxBlur blur;

        gfxContext* ctx = ShadowInitialize(drawExtents, blur);

        if (ctx) {
            CopyContext(ctx, mThebes);
            ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
            processor.mThebes = ctx;

            rv = bidiUtils->ProcessText(textToDraw.get(),
                                        textToDraw.Length(),
                                        isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                        presShell->GetPresContext(),
                                        processor,
                                        nsBidiPresUtils::MODE_DRAW,
                                        nsnull,
                                        0,
                                        nsnull);
            if (NS_FAILED(rv))
                return rv;

            ShadowFinalize(blur);
        }

        processor.mThebes = mThebes;
    }

    gfxContextPathAutoSaveRestore pathSR(mThebes, PR_FALSE);

    // back up path if stroking
    if (aOp == nsCanvasRenderingContext2D::TEXT_DRAW_OPERATION_STROKE)
        pathSR.Save();
    // doUseIntermediateSurface is mutually exclusive to op == STROKE
    else {
        if (doUseIntermediateSurface) {
            mThebes->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

            // don't want operators to be applied twice
            mThebes->SetOperator(gfxContext::OPERATOR_SOURCE);
        }

        ApplyStyle(STYLE_FILL);
    }

    rv = bidiUtils->ProcessText(textToDraw.get(),
                                textToDraw.Length(),
                                isRTL ? NSBIDI_RTL : NSBIDI_LTR,
                                presShell->GetPresContext(),
                                processor,
                                nsBidiPresUtils::MODE_DRAW,
                                nsnull,
                                0,
                                nsnull);

    // this needs to be restored before function can return
    if (doUseIntermediateSurface) {
        mThebes->PopGroupToSource();
        DirtyAllStyles();
    }

    if (NS_FAILED(rv))
        return rv;

    if (aOp == nsCanvasRenderingContext2D::TEXT_DRAW_OPERATION_STROKE) {
        // DrawPath takes care of all shadows and composite oddities
        rv = DrawPath(STYLE_STROKE);
        if (NS_FAILED(rv))
            return rv;
    } else if (doUseIntermediateSurface)
        mThebes->Paint(CurrentState().StyleIsColor(STYLE_FILL) ? 1.0 : CurrentState().globalAlpha);

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

gfxFontGroup *nsCanvasRenderingContext2D::GetCurrentFontStyle()
{
    // use lazy initilization for the font group since it's rather expensive
    if(!CurrentState().fontGroup) {
#ifdef DEBUG
        nsresult res =
#endif
            SetMozTextStyle(NS_LITERAL_STRING("10px sans-serif"));
        NS_ASSERTION(res == NS_OK, "Default canvas font is invalid");
    }

    return CurrentState().fontGroup;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::MozDrawText(const nsAString& textToDraw)
{
    const PRUnichar* textdata;
    textToDraw.GetData(&textdata);

    PRUint32 textrunflags = 0;

    PRUint32 aupdp;
    GetAppUnitsValues(&aupdp, NULL);

    gfxTextRunCache::AutoTextRun textRun;
    textRun = gfxTextRunCache::MakeTextRun(textdata,
                                           textToDraw.Length(),
                                           GetCurrentFontStyle(),
                                           mThebes,
                                           aupdp,
                                           textrunflags);

    if(!textRun.get())
        return NS_ERROR_FAILURE;

    gfxPoint pt(0.0f,0.0f);

    // Fill color is text color
    ApplyStyle(STYLE_FILL);
    
    textRun->Draw(mThebes,
                  pt,
                  /* offset = */ 0,
                  textToDraw.Length(),
                  nsnull,
                  nsnull,
                  nsnull);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::MozMeasureText(const nsAString& textToMeasure, float *retVal)
{
    nsCOMPtr<nsIDOMTextMetrics> metrics;
    nsresult rv;
    rv = MeasureText(textToMeasure, getter_AddRefs(metrics));
    if (NS_FAILED(rv))
        return rv;
    return metrics->GetWidth(retVal);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::MozPathText(const nsAString& textToPath)
{
    const PRUnichar* textdata;
    textToPath.GetData(&textdata);

    PRUint32 textrunflags = 0;

    PRUint32 aupdp;
    GetAppUnitsValues(&aupdp, NULL);

    gfxTextRunCache::AutoTextRun textRun;
    textRun = gfxTextRunCache::MakeTextRun(textdata,
                                           textToPath.Length(),
                                           GetCurrentFontStyle(),
                                           mThebes,
                                           aupdp,
                                           textrunflags);

    if(!textRun.get())
        return NS_ERROR_FAILURE;

    gfxPoint pt(0.0f,0.0f);

    textRun->DrawToPath(mThebes,
                        pt,
                        /* offset = */ 0,
                        textToPath.Length(),
                        nsnull,
                        nsnull);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::MozTextAlongPath(const nsAString& textToDraw, PRBool stroke)
{
    // Most of this code is copied from its svg equivalent
    nsRefPtr<gfxFlattenedPath> path(mThebes->GetFlattenedPath());

    const PRUnichar* textdata;
    textToDraw.GetData(&textdata);

    PRUint32 textrunflags = 0;

    PRUint32 aupdp;
    GetAppUnitsValues(&aupdp, NULL);

    gfxTextRunCache::AutoTextRun textRun;
    textRun = gfxTextRunCache::MakeTextRun(textdata,
                                           textToDraw.Length(),
                                           GetCurrentFontStyle(),
                                           mThebes,
                                           aupdp,
                                           textrunflags);

    if(!textRun.get())
        return NS_ERROR_FAILURE;

    struct PathChar
    {
        PRBool draw;
        gfxFloat angle;
        gfxPoint pos;
        PathChar() : draw(PR_FALSE), angle(0.0), pos(0.0,0.0) {}
    };

    gfxFloat length = path->GetLength();
    PRUint32 strLength = textToDraw.Length();

    PathChar *cp = new PathChar[strLength];

    if (!cp) {
        return NS_ERROR_OUT_OF_MEMORY;
    }

    gfxPoint position(0.0,0.0);
    gfxFloat x = position.x;
    for (PRUint32 i = 0; i < strLength; i++)
    {
        gfxFloat halfAdvance = textRun->GetAdvanceWidth(i, 1, nsnull) / (2.0 * aupdp);

        // Check for end of path
        if(x + halfAdvance > length)
            break;

        if(x + halfAdvance >= 0)
        {
            cp[i].draw = PR_TRUE;
            gfxPoint pt = path->FindPoint(gfxPoint(x + halfAdvance, position.y), &(cp[i].angle));

            cp[i].pos = pt - gfxPoint(cos(cp[i].angle), sin(cp[i].angle)) * halfAdvance;
        }
        x += 2 * halfAdvance;
    }

    if(stroke)
        ApplyStyle(STYLE_STROKE);
    else
        ApplyStyle(STYLE_FILL);

    for(PRUint32 i = 0; i < strLength; i++)
    {
        // Skip non-visible characters
        if(!cp[i].draw) continue;

        gfxMatrix matrix = mThebes->CurrentMatrix();

        gfxMatrix rot;
        rot.Rotate(cp[i].angle);
        mThebes->Multiply(rot);

        rot.Invert();
        rot.Scale(aupdp,aupdp);
        gfxPoint pt = rot.Transform(cp[i].pos);

        if(stroke) {
            textRun->DrawToPath(mThebes, pt, i, 1, nsnull, nsnull);
        } else {
            textRun->Draw(mThebes, pt, i, 1, nsnull, nsnull, nsnull);
        }
        mThebes->SetMatrix(matrix);
    }

    delete [] cp;

    return NS_OK;
}

//
// line caps/joins
//
NS_IMETHODIMP
nsCanvasRenderingContext2D::SetLineWidth(float width)
{
    if (!FloatValidate(width))
        return NS_ERROR_DOM_SYNTAX_ERR;

    mThebes->SetLineWidth(width);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetLineWidth(float *width)
{
    gfxFloat d = mThebes->CurrentLineWidth();
    *width = static_cast<float>(d);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetLineCap(const nsAString& capstyle)
{
    gfxContext::GraphicsLineCap cap;

    if (capstyle.EqualsLiteral("butt"))
        cap = gfxContext::LINE_CAP_BUTT;
    else if (capstyle.EqualsLiteral("round"))
        cap = gfxContext::LINE_CAP_ROUND;
    else if (capstyle.EqualsLiteral("square"))
        cap = gfxContext::LINE_CAP_SQUARE;
    else
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_NOT_IMPLEMENTED;

    mThebes->SetLineCap(cap);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetLineCap(nsAString& capstyle)
{
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
    gfxContext::GraphicsLineJoin j;

    if (joinstyle.EqualsLiteral("round"))
        j = gfxContext::LINE_JOIN_ROUND;
    else if (joinstyle.EqualsLiteral("bevel"))
        j = gfxContext::LINE_JOIN_BEVEL;
    else if (joinstyle.EqualsLiteral("miter"))
        j = gfxContext::LINE_JOIN_MITER;
    else
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_NOT_IMPLEMENTED;

    mThebes->SetLineJoin(j);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetLineJoin(nsAString& joinstyle)
{
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
    if (!FloatValidate(miter))
        return NS_ERROR_DOM_SYNTAX_ERR;

    mThebes->SetMiterLimit(miter);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMiterLimit(float *miter)
{
    gfxFloat d = mThebes->CurrentMiterLimit();
    *miter = static_cast<float>(d);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::IsPointInPath(float x, float y, PRBool *retVal)
{
    if (!FloatValidate(x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    *retVal = mThebes->PointInFill(gfxPoint(x,y));
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
nsCanvasRenderingContext2D::DrawImage()
{
    nsresult rv;

    // we can't do a security check without a canvas element, so
    // just skip this entirely
    if (!mCanvasElement)
        return NS_ERROR_FAILURE;

    nsAXPCNativeCallContext *ncc = nsnull;
    rv = nsContentUtils::XPConnect()->
        GetCurrentNativeCallContext(&ncc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!ncc)
        return NS_ERROR_FAILURE;

    JSContext *ctx = nsnull;

    rv = ncc->GetJSContext(&ctx);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 argc;
    jsval *argv = nsnull;

    ncc->GetArgc(&argc);
    ncc->GetArgvPtr(&argv);

    // we always need at least an image and a dx,dy
    if (argc < 3)
        return NS_ERROR_INVALID_ARG;

    JSAutoRequest ar(ctx);

    double sx,sy,sw,sh;
    double dx,dy,dw,dh;

    nsCOMPtr<nsIDOMElement> imgElt;
    if (!ConvertJSValToXPCObject(getter_AddRefs(imgElt),
                                 NS_GET_IID(nsIDOMElement),
                                 ctx, argv[0]))
        return NS_ERROR_DOM_TYPE_MISMATCH_ERR;

    PRInt32 imgWidth, imgHeight;
    nsCOMPtr<nsIPrincipal> principal;
    PRBool forceWriteOnly = PR_FALSE;
    gfxMatrix matrix;
    nsRefPtr<gfxPattern> pattern;
    nsRefPtr<gfxPath> path;
    nsRefPtr<gfxASurface> imgsurf;
    rv = ThebesSurfaceFromElement(imgElt, PR_FALSE,
                                  getter_AddRefs(imgsurf), &imgWidth, &imgHeight,
                                  getter_AddRefs(principal), &forceWriteOnly);
    if (NS_FAILED(rv))
        return rv;
    DoDrawImageSecurityCheck(principal, forceWriteOnly);

    gfxContextPathAutoSaveRestore pathSR(mThebes, PR_FALSE);

#define GET_ARG(dest,whicharg) \
    do { if (!ConvertJSValToDouble(dest, ctx, whicharg)) { rv = NS_ERROR_INVALID_ARG; goto FINISH; } } while (0)

    rv = NS_OK;

    if (argc == 3) {
        GET_ARG(&dx, argv[1]);
        GET_ARG(&dy, argv[2]);
        sx = sy = 0.0;
        dw = sw = (double) imgWidth;
        dh = sh = (double) imgHeight;
    } else if (argc == 5) {
        GET_ARG(&dx, argv[1]);
        GET_ARG(&dy, argv[2]);
        GET_ARG(&dw, argv[3]);
        GET_ARG(&dh, argv[4]);
        sx = sy = 0.0;
        sw = (double) imgWidth;
        sh = (double) imgHeight;
    } else if (argc == 9) {
        GET_ARG(&sx, argv[1]);
        GET_ARG(&sy, argv[2]);
        GET_ARG(&sw, argv[3]);
        GET_ARG(&sh, argv[4]);
        GET_ARG(&dx, argv[5]);
        GET_ARG(&dy, argv[6]);
        GET_ARG(&dw, argv[7]);
        GET_ARG(&dh, argv[8]);
    } else {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        rv = NS_ERROR_INVALID_ARG;
        goto FINISH;
    }
#undef GET_ARG

    if (dw == 0.0 || dh == 0.0) {
        rv = NS_OK;
        // not really failure, but nothing to do --
        // and noone likes a divide-by-zero
        goto FINISH;
    }

    if (!FloatValidate(sx,sy,sw,sh) || !FloatValidate(dx,dy,dw,dh)) {
        rv = NS_ERROR_DOM_SYNTAX_ERR;
        goto FINISH;
    }

    // check args
    if (sx < 0.0 || sy < 0.0 ||
        sw < 0.0 || sw > (double) imgWidth ||
        sh < 0.0 || sh > (double) imgHeight ||
        dw < 0.0 || dh < 0.0)
    {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        rv = NS_ERROR_DOM_INDEX_SIZE_ERR;
        goto FINISH;
    }
    
    matrix.Translate(gfxPoint(sx, sy));
    matrix.Scale(sw/dw, sh/dh);

    pattern = new gfxPattern(imgsurf);
    pattern->SetMatrix(matrix);

    pathSR.Save();

    {
        gfxContextAutoSaveRestore autoSR(mThebes);
        mThebes->Translate(gfxPoint(dx, dy));
        mThebes->SetPattern(pattern);

        gfxRect clip(0, 0, dw, dh);

        if (NeedToDrawShadow()) {
            gfxRect drawExtents = mThebes->UserToDevice(clip);
            gfxAlphaBoxBlur blur;

            gfxContext* ctx = ShadowInitialize(drawExtents, blur);

            if (ctx) {
                CopyContext(ctx, mThebes);
                ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
                ctx->Clip(clip);
                ctx->Paint();

                ShadowFinalize(blur);
            }
        }

        PRBool doUseIntermediateSurface = NeedToUseIntermediateSurface();

        mThebes->SetPattern(pattern);
        DirtyAllStyles();

        if (doUseIntermediateSurface) {
            // draw onto a pushed group
            mThebes->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
            mThebes->Clip(clip);

            // don't want operators to be applied twice
            mThebes->SetOperator(gfxContext::OPERATOR_SOURCE);

            mThebes->Paint();
            mThebes->PopGroupToSource();
        } else
            mThebes->Clip(clip);

        mThebes->Paint(CurrentState().globalAlpha);
    }

#if 1
    // XXX cairo bug workaround; force a clip update on mThebes.
    // Otherwise, a pixman clip gets left around somewhere, and pixman
    // (Render) does source clipping as well -- so we end up
    // compositing with an incorrect clip.  This only seems to affect
    // fallback cases, which happen when we have CSS scaling going on.
    // This will blow away the current path, but we already blew it
    // away in this function earlier.
    mThebes->UpdateSurfaceClip();
#endif

FINISH:
    if (NS_SUCCEEDED(rv))
        rv = Redraw();

    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetGlobalCompositeOperation(const nsAString& op)
{
    gfxContext::GraphicsOperator thebes_op;

#define CANVAS_OP_TO_THEBES_OP(cvsop,thebesop) \
    if (op.EqualsLiteral(cvsop))   \
        thebes_op = gfxContext::OPERATOR_##thebesop;

    // XXX "darker" isn't really correct
    CANVAS_OP_TO_THEBES_OP("clear", CLEAR)
    else CANVAS_OP_TO_THEBES_OP("copy", SOURCE)
    else CANVAS_OP_TO_THEBES_OP("darker", SATURATE)  // XXX
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
    // not part of spec, kept here for compat
    else CANVAS_OP_TO_THEBES_OP("over", OVER)
    else return NS_ERROR_NOT_IMPLEMENTED;

#undef CANVAS_OP_TO_THEBES_OP

    mThebes->SetOperator(thebes_op);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetGlobalCompositeOperation(nsAString& op)
{
    gfxContext::GraphicsOperator thebes_op = mThebes->CurrentOperator();

#define CANVAS_OP_TO_THEBES_OP(cvsop,thebesop) \
    if (thebes_op == gfxContext::OPERATOR_##thebesop) \
        op.AssignLiteral(cvsop);

    // XXX "darker" isn't really correct
    CANVAS_OP_TO_THEBES_OP("clear", CLEAR)
    else CANVAS_OP_TO_THEBES_OP("copy", SOURCE)
    else CANVAS_OP_TO_THEBES_OP("darker", SATURATE)  // XXX
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


//
// Utils
//
PRBool
nsCanvasRenderingContext2D::ConvertJSValToUint32(PRUint32* aProp, JSContext* aContext,
                                                 jsval aValue)
{
  uint32 temp;
  if (::JS_ValueToECMAUint32(aContext, aValue, &temp)) {
    *aProp = (PRUint32)temp;
  }
  else {
    ::JS_ReportError(aContext, "Parameter must be an integer");
    return JS_FALSE;
  }

  return JS_TRUE;
}

PRBool
nsCanvasRenderingContext2D::ConvertJSValToDouble(double* aProp, JSContext* aContext,
                                                 jsval aValue)
{
  jsdouble temp;
  if (::JS_ValueToNumber(aContext, aValue, &temp)) {
    *aProp = (jsdouble)temp;
  }
  else {
    ::JS_ReportError(aContext, "Parameter must be a number");
    return JS_FALSE;
  }

  return JS_TRUE;
}

PRBool
nsCanvasRenderingContext2D::ConvertJSValToXPCObject(nsISupports** aSupports, REFNSIID aIID,
                                                    JSContext* aContext, jsval aValue)
{
  *aSupports = nsnull;
  if (JSVAL_IS_NULL(aValue)) {
    return JS_TRUE;
  }

  if (JSVAL_IS_OBJECT(aValue)) {
    // WrapJS does all the work to recycle an existing wrapper and/or do a QI
    nsresult rv = nsContentUtils::XPConnect()->
      WrapJS(aContext, JSVAL_TO_OBJECT(aValue), aIID, (void**)aSupports);

    return NS_SUCCEEDED(rv);
  }

  return JS_FALSE;
}

/* thebes ARGB32 surfaces are ARGB stored as a packed 32-bit integer; on little-endian
 * platforms, they appear as BGRA bytes in the surface data.  The color values are also
 * stored with premultiplied alpha.
 *
 * If forceCopy is FALSE, a surface may be returned that's only valid during the current
 * operation.  If it's TRUE, a copy will always be made that can safely be retained.
 */

nsresult
nsCanvasRenderingContext2D::ThebesSurfaceFromElement(nsIDOMElement *imgElt,
                                                     PRBool forceCopy,
                                                     gfxASurface **aSurface,
                                                     PRInt32 *widthOut,
                                                     PRInt32 *heightOut,
                                                     nsIPrincipal **prinOut,
                                                     PRBool *forceWriteOnlyOut)
{
    nsresult rv;

    nsCOMPtr<nsINode> node = do_QueryInterface(imgElt);

    /* If it's a Canvas, grab its internal surface as necessary */
    nsCOMPtr<nsICanvasElement> canvas = do_QueryInterface(imgElt);
    if (node && canvas) {
        PRUint32 w, h;
        rv = canvas->GetSize(&w, &h);
        NS_ENSURE_SUCCESS(rv, rv);

        nsRefPtr<gfxASurface> sourceSurface;

        if (!forceCopy && canvas->CountContexts() == 1) {
            nsICanvasRenderingContextInternal *srcCanvas = canvas->GetContextAtIndex(0);
            rv = srcCanvas->GetThebesSurface(getter_AddRefs(sourceSurface));
            // force a copy if we couldn't get the surface, or if it's
            // the same as what we have
            if (sourceSurface == mSurface || NS_FAILED(rv))
                sourceSurface = nsnull;
        }

        if (sourceSurface == nsnull) {
            nsRefPtr<gfxASurface> surf =
                gfxPlatform::GetPlatform()->CreateOffscreenSurface
                (gfxIntSize(w, h), gfxASurface::ImageFormatARGB32);
            nsRefPtr<gfxContext> ctx = new gfxContext(surf);
            rv = canvas->RenderContexts(ctx);
            if (NS_FAILED(rv))
                return rv;
            sourceSurface = surf;
        }

        *aSurface = sourceSurface.forget().get();
        *widthOut = w;
        *heightOut = h;

        NS_ADDREF(*prinOut = node->NodePrincipal());
        *forceWriteOnlyOut = canvas->IsWriteOnly();

        return NS_OK;
    }

#ifdef MOZ_MEDIA
    /* Maybe it's <video>? */
    nsCOMPtr<nsIDOMHTMLVideoElement> ve = do_QueryInterface(imgElt);
    if (node && ve) {
        nsHTMLVideoElement *video = static_cast<nsHTMLVideoElement*>(ve.get());

        /* If it doesn't have a principal, just bail */
        nsCOMPtr<nsIPrincipal> principal = video->GetCurrentPrincipal();
        if (!principal)
            return NS_ERROR_DOM_SECURITY_ERR;

        PRUint32 videoWidth, videoHeight;
        rv = video->GetVideoWidth(&videoWidth);
        rv |= video->GetVideoHeight(&videoHeight);
        if (NS_FAILED(rv))
            return NS_ERROR_NOT_AVAILABLE;

        nsRefPtr<gfxASurface> surf =
            gfxPlatform::GetPlatform()->CreateOffscreenSurface
                (gfxIntSize(videoWidth, videoHeight), gfxASurface::ImageFormatARGB32);
        nsRefPtr<gfxContext> ctx = new gfxContext(surf);

        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);

        video->Paint(ctx, gfxRect(0, 0, videoWidth, videoHeight));

        *aSurface = surf.forget().get();
        *widthOut = videoWidth;
        *heightOut = videoHeight;

        *prinOut = principal.forget().get();
        *forceWriteOnlyOut = PR_FALSE;

        return NS_OK;
    }
#endif

    /* Finally, check if it's a normal image */
    nsCOMPtr<imgIContainer> imgContainer;
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgElt);

    if (!imageLoader)
        return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<imgIRequest> imgRequest;
    rv = imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                 getter_AddRefs(imgRequest));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!imgRequest)
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_NOT_AVAILABLE;

    PRUint32 status;
    imgRequest->GetImageStatus(&status);
    if ((status & imgIRequest::STATUS_LOAD_COMPLETE) == 0)
        return NS_ERROR_NOT_AVAILABLE;

    // In case of data: URIs, we want to ignore principals;
    // they should have the originating content's principal,
    // but that's broken at the moment in imgLib.
    nsCOMPtr<nsIURI> uri;
    rv = imgRequest->GetURI(getter_AddRefs(uri));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SECURITY_ERR);

    PRBool isDataURI = PR_FALSE;
    rv = uri->SchemeIs("data", &isDataURI);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_SECURITY_ERR);
    
    // Data URIs are always OK; set the principal
    // to null to indicate that.
    if (isDataURI) {
        *prinOut = nsnull;
    } else {
        rv = imgRequest->GetImagePrincipal(prinOut);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_TRUE(*prinOut, NS_ERROR_DOM_SECURITY_ERR);
    }

    *forceWriteOnlyOut = PR_FALSE;

    rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
    NS_ENSURE_SUCCESS(rv, rv);

    if (!imgContainer)
        return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<gfxIImageFrame> frame;
    rv = imgContainer->GetCurrentFrame(getter_AddRefs(frame));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIImage> img(do_GetInterface(frame));

    PRInt32 imgWidth, imgHeight;
    rv = frame->GetWidth(&imgWidth);
    rv |= frame->GetHeight(&imgHeight);
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    if (widthOut)
        *widthOut = imgWidth;
    if (heightOut)
        *heightOut = imgHeight;

    nsRefPtr<gfxPattern> gfxpattern;
    img->GetPattern(getter_AddRefs(gfxpattern));
    nsRefPtr<gfxASurface> gfxsurf = gfxpattern->GetSurface();

    if (!gfxsurf) {
        gfxsurf = new gfxImageSurface (gfxIntSize(imgWidth, imgHeight), gfxASurface::ImageFormatARGB32);
        nsRefPtr<gfxContext> ctx = new gfxContext(gfxsurf);

        ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
        ctx->SetPattern(gfxpattern);
        ctx->Paint();
    }

    *aSurface = gfxsurf.forget().get();

    return NS_OK;
}

/* Check that the rect [x,y,w,h] is a valid subrect of [0,0,realWidth,realHeight]
 * without overflowing any integers and the like.
 */
PRBool
CheckSaneSubrectSize (PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h, PRInt32 realWidth, PRInt32 realHeight)
{
    if (w <= 0 || h <= 0 || x < 0 || y < 0)
        return PR_FALSE;

    if (x >= realWidth  || w > (realWidth - x) ||
        y >= realHeight || h > (realHeight - y))
        return PR_FALSE;

    return PR_TRUE;
}

static void
FlushLayoutForTree(nsIDOMWindow* aWindow)
{
    nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aWindow);
    if (!piWin)
        return;

    // Note that because FlushPendingNotifications flushes parents, this
    // is O(N^2) in docshell tree depth.  However, the docshell tree is
    // usually pretty shallow.

    nsCOMPtr<nsIDOMDocument> domDoc;
    aWindow->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    if (doc) {
        doc->FlushPendingNotifications(Flush_Layout);
    }

    nsCOMPtr<nsIDocShellTreeNode> node =
        do_QueryInterface(piWin->GetDocShell());
    if (node) {
        PRInt32 i = 0, i_end;
        node->GetChildCount(&i_end);
        for (; i < i_end; ++i) {
            nsCOMPtr<nsIDocShellTreeItem> item;
            node->GetChildAt(i, getter_AddRefs(item));
            nsCOMPtr<nsIDOMWindow> win = do_GetInterface(item);
            if (win) {
                FlushLayoutForTree(win);
            }
        }
    }
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::DrawWindow(nsIDOMWindow* aWindow, float aX, float aY,
                                       float aW, float aH, 
                                       const nsAString& aBGColor,
                                       PRUint32 flags)
{
    NS_ENSURE_ARG(aWindow != nsnull);

    // protect against too-large surfaces that will cause allocation
    // or overflow issues
    if (!gfxASurface::CheckSurfaceSize(gfxIntSize(aW, aH), 0xffff))
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
        FlushLayoutForTree(aWindow);

    nsCOMPtr<nsPresContext> presContext;
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
    nsresult rv = mCSSParser->ParseColorString(PromiseFlatString(aBGColor),
                                               nsnull, 0, &bgColor);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIPresShell* presShell = presContext->PresShell();
    NS_ENSURE_TRUE(presShell, NS_ERROR_FAILURE);

    nsRect r(nsPresContext::CSSPixelsToAppUnits(aX),
             nsPresContext::CSSPixelsToAppUnits(aY),
             nsPresContext::CSSPixelsToAppUnits(aW),
             nsPresContext::CSSPixelsToAppUnits(aH));
    PRUint32 renderDocFlags = nsIPresShell::RENDER_IGNORE_VIEWPORT_SCROLLING;
    if (flags & nsIDOMCanvasRenderingContext2D::DRAWWINDOW_DRAW_CARET) {
        renderDocFlags |= nsIPresShell::RENDER_CARET;
    }
    presShell->RenderDocument(r, renderDocFlags, bgColor, mThebes);

    // get rid of the pattern surface ref, just in case
    mThebes->SetColor(gfxRGBA(1,1,1,1));
    DirtyAllStyles();

    Redraw();

    return rv;
}

//
// device pixel getting/setting
//

// ImageData getImageData (in float x, in float y, in float width, in float height);
NS_IMETHODIMP
nsCanvasRenderingContext2D::GetImageData()
{
    if (!mValid || !mCanvasElement)
        return NS_ERROR_FAILURE;

    if (mCanvasElement->IsWriteOnly() && !nsContentUtils::IsCallerTrustedForRead()) {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsAXPCNativeCallContext *ncc = nsnull;
    nsresult rv = nsContentUtils::XPConnect()->
        GetCurrentNativeCallContext(&ncc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!ncc)
        return NS_ERROR_FAILURE;

    JSContext *ctx = nsnull;

    rv = ncc->GetJSContext(&ctx);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 argc;
    jsval *argv = nsnull;

    ncc->GetArgc(&argc);
    ncc->GetArgvPtr(&argv);

    JSAutoRequest ar(ctx);

    int32 x, y, w, h;
    if (!JS_ConvertArguments (ctx, argc, argv, "jjjj", &x, &y, &w, &h))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (!CheckSaneSubrectSize (x, y, w, h, mWidth, mHeight))
        return NS_ERROR_DOM_SYNTAX_ERR;

    nsAutoArrayPtr<PRUint8> surfaceData (new (std::nothrow) PRUint8[w * h * 4]);
    int surfaceDataStride = w*4;
    int surfaceDataOffset = 0;

    if (!surfaceData)
        return NS_ERROR_OUT_OF_MEMORY;

    nsRefPtr<gfxImageSurface> tmpsurf = new gfxImageSurface(surfaceData,
                                                            gfxIntSize(w, h),
                                                            w * 4,
                                                            gfxASurface::ImageFormatARGB32);
    if (!tmpsurf || tmpsurf->CairoStatus())
        return NS_ERROR_FAILURE;

    nsRefPtr<gfxContext> tmpctx = new gfxContext(tmpsurf);

    if (!tmpctx || tmpctx->HasError())
        return NS_ERROR_FAILURE;

    tmpctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    tmpctx->SetSource(mSurface, gfxPoint(-(int)x, -(int)y));
    tmpctx->Paint();

    tmpctx = nsnull;
    tmpsurf = nsnull;

    PRUint32 len = w * h * 4;
    if (len > (((PRUint32)0xfff00000)/sizeof(jsval)))
        return NS_ERROR_INVALID_ARG;

    nsAutoArrayPtr<jsval> jsvector(new (std::nothrow) jsval[w * h * 4]);
    if (!jsvector)
        return NS_ERROR_OUT_OF_MEMORY;
    jsval *dest = jsvector.get();
    PRUint8 *row;
    for (int j = 0; j < h; j++) {
        row = surfaceData + surfaceDataOffset + (surfaceDataStride * j);
        for (int i = 0; i < w; i++) {
            // XXX Is there some useful swizzle MMX we can use here?
            // I guess we have to INT_TO_JSVAL still
#ifdef IS_LITTLE_ENDIAN
            PRUint8 b = *row++;
            PRUint8 g = *row++;
            PRUint8 r = *row++;
            PRUint8 a = *row++;
#else
            PRUint8 a = *row++;
            PRUint8 r = *row++;
            PRUint8 g = *row++;
            PRUint8 b = *row++;
#endif
            // Convert to non-premultiplied color
            if (a != 0) {
                r = (r * 255) / a;
                g = (g * 255) / a;
                b = (b * 255) / a;
            }

            *dest++ = INT_TO_JSVAL(r);
            *dest++ = INT_TO_JSVAL(g);
            *dest++ = INT_TO_JSVAL(b);
            *dest++ = INT_TO_JSVAL(a);
        }
    }

    JSObject *dataArray = JS_NewArrayObject(ctx, w*h*4, jsvector.get());
    if (!dataArray)
        return NS_ERROR_OUT_OF_MEMORY;

    nsAutoGCRoot arrayGCRoot(&dataArray, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *result = JS_NewObject(ctx, NULL, NULL, NULL);
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsAutoGCRoot resultGCRoot(&result, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_DefineProperty(ctx, result, "width", INT_TO_JSVAL(w), NULL, NULL, 0) ||
        !JS_DefineProperty(ctx, result, "height", INT_TO_JSVAL(h), NULL, NULL, 0) ||
        !JS_DefineProperty(ctx, result, "data", OBJECT_TO_JSVAL(dataArray), NULL, NULL, 0))
        return NS_ERROR_FAILURE;

    jsval *retvalPtr;
    ncc->GetRetValPtr(&retvalPtr);
    *retvalPtr = OBJECT_TO_JSVAL(result);
    ncc->SetReturnValueWasSet(PR_TRUE);

    return NS_OK;
}

extern "C" {
#include "jstypes.h"
JS_FRIEND_API(JSBool)
js_ArrayToJSUint8Buffer(JSContext *cx, JSObject *obj, jsuint offset, jsuint count,
                        JSUint8 *dest);
}

// void putImageData (in ImageData d, in float x, in float y);
NS_IMETHODIMP
nsCanvasRenderingContext2D::PutImageData()
{
    nsresult rv;

    if (!mValid)
        return NS_ERROR_FAILURE;

    nsAXPCNativeCallContext *ncc = nsnull;
    rv = nsContentUtils::XPConnect()->
        GetCurrentNativeCallContext(&ncc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!ncc)
        return NS_ERROR_FAILURE;

    JSContext *ctx = nsnull;

    rv = ncc->GetJSContext(&ctx);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 argc;
    jsval *argv = nsnull;

    ncc->GetArgc(&argc);
    ncc->GetArgvPtr(&argv);

    JSAutoRequest ar(ctx);

    JSObject *dataObject;
    int32 x, y;

    if (!JS_ConvertArguments (ctx, argc, argv, "ojj", &dataObject, &x, &y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (!dataObject)
        return NS_ERROR_DOM_SYNTAX_ERR;

    int32 w, h;
    JSObject *dataArray;
    jsval v;

    if (!JS_GetProperty(ctx, dataObject, "width", &v) ||
        !JS_ValueToInt32(ctx, v, &w))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (!JS_GetProperty(ctx, dataObject, "height", &v) ||
        !JS_ValueToInt32(ctx, v, &h))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (!JS_GetProperty(ctx, dataObject, "data", &v) ||
        !JSVAL_IS_OBJECT(v))
        return NS_ERROR_DOM_SYNTAX_ERR;
    dataArray = JSVAL_TO_OBJECT(v);

    if (!CheckSaneSubrectSize (x, y, w, h, mWidth, mHeight))
        return NS_ERROR_DOM_SYNTAX_ERR;

    jsuint arrayLen;
    if (!JS_IsArrayObject(ctx, dataArray) ||
        !JS_GetArrayLength(ctx, dataArray, &arrayLen) ||
        arrayLen < (jsuint)(w * h * 4))
        return NS_ERROR_DOM_SYNTAX_ERR;

    nsAutoArrayPtr<PRUint8> imageBuffer(new (std::nothrow) PRUint8[w * h * 4]);
    if (!imageBuffer)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUint8 *imgPtr = imageBuffer.get();

    JSBool ok = js_ArrayToJSUint8Buffer(ctx, dataArray, 0, w*h*4, imageBuffer);

    // no fast path? go slow.
    if (!ok) {
        jsval vr, vg, vb, va;
        PRUint8 ir, ig, ib, ia;
        for (int32 j = 0; j < h; j++) {
            for (int32 i = 0; i < w; i++) {
                if (!JS_GetElement(ctx, dataArray, (j*w*4) + i*4 + 0, &vr) ||
                    !JS_GetElement(ctx, dataArray, (j*w*4) + i*4 + 1, &vg) ||
                    !JS_GetElement(ctx, dataArray, (j*w*4) + i*4 + 2, &vb) ||
                    !JS_GetElement(ctx, dataArray, (j*w*4) + i*4 + 3, &va))
                    return NS_ERROR_DOM_SYNTAX_ERR;

                if (JSVAL_IS_INT(vr))         ir = (PRUint8) JSVAL_TO_INT(vr);
                else if (JSVAL_IS_DOUBLE(vr)) ir = (PRUint8) (*JSVAL_TO_DOUBLE(vr));
                else return NS_ERROR_DOM_SYNTAX_ERR;

                if (JSVAL_IS_INT(vg))         ig = (PRUint8) JSVAL_TO_INT(vg);
                else if (JSVAL_IS_DOUBLE(vg)) ig = (PRUint8) (*JSVAL_TO_DOUBLE(vg));
                else return NS_ERROR_DOM_SYNTAX_ERR;

                if (JSVAL_IS_INT(vb))         ib = (PRUint8) JSVAL_TO_INT(vb);
                else if (JSVAL_IS_DOUBLE(vb)) ib = (PRUint8) (*JSVAL_TO_DOUBLE(vb));
                else return NS_ERROR_DOM_SYNTAX_ERR;

                if (JSVAL_IS_INT(va))         ia = (PRUint8) JSVAL_TO_INT(va);
                else if (JSVAL_IS_DOUBLE(va)) ia = (PRUint8) (*JSVAL_TO_DOUBLE(va));
                else return NS_ERROR_DOM_SYNTAX_ERR;

                // Convert to premultiplied color (losslessly if the input came from getImageData)
                ir = (ir*ia + 254) / 255;
                ig = (ig*ia + 254) / 255;
                ib = (ib*ia + 254) / 255;

#ifdef IS_LITTLE_ENDIAN
                *imgPtr++ = ib;
                *imgPtr++ = ig;
                *imgPtr++ = ir;
                *imgPtr++ = ia;
#else
                *imgPtr++ = ia;
                *imgPtr++ = ir;
                *imgPtr++ = ig;
                *imgPtr++ = ib;
#endif
            }
        }
    } else {
        /* Walk through and premultiply and swap rgba */
        /* XXX SSE me */
        PRUint8 ir, ig, ib, ia;
        PRUint8 *ptr = imgPtr;
        for (int32 i = 0; i < w*h; i++) {
#ifdef IS_LITTLE_ENDIAN
            ir = ptr[0];
            ig = ptr[1];
            ib = ptr[2];
            ia = ptr[3];
            ptr[0] = (ib*ia + 254) / 255;
            ptr[1] = (ig*ia + 254) / 255;
            ptr[2] = (ir*ia + 254) / 255;
#else
            ptr[0] = (ptr[0]*ptr[3] + 254) / 255;
            ptr[1] = (ptr[1]*ptr[3] + 254) / 255;
            ptr[2] = (ptr[2]*ptr[3] + 254) / 255;
#endif
            ptr += 4;
        }
    }

    nsRefPtr<gfxImageSurface> imgsurf = new gfxImageSurface(imageBuffer.get(),
                                                            gfxIntSize(w, h),
                                                            w * 4,
                                                            gfxASurface::ImageFormatARGB32);
    if (!imgsurf || imgsurf->CairoStatus())
        return NS_ERROR_FAILURE;

    gfxContextPathAutoSaveRestore pathSR(mThebes);
    gfxContextAutoSaveRestore autoSR(mThebes);

    mThebes->IdentityMatrix();
    mThebes->Translate(gfxPoint(x, y));
    mThebes->NewPath();
    mThebes->Rectangle(gfxRect(0, 0, w, h));
    mThebes->SetSource(imgsurf, gfxPoint(0, 0));
    mThebes->SetOperator(gfxContext::OPERATOR_SOURCE);
    mThebes->Fill();

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetThebesSurface(gfxASurface **surface)
{
    if (!mSurface) {
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
    if (!mValid || !mCanvasElement)
        return NS_ERROR_FAILURE;

    nsAXPCNativeCallContext *ncc = nsnull;
    nsresult rv = nsContentUtils::XPConnect()->
        GetCurrentNativeCallContext(&ncc);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!ncc)
        return NS_ERROR_FAILURE;

    JSContext *ctx = nsnull;

    rv = ncc->GetJSContext(&ctx);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 argc;
    jsval *argv = nsnull;

    ncc->GetArgc(&argc);
    ncc->GetArgvPtr(&argv);

    JSAutoRequest ar(ctx);

    int32 w, h;
    if (!JS_ConvertArguments (ctx, argc, argv, "jj", &w, &h))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (w <= 0 || h <= 0)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    // check for overflow when calculating len
    PRUint32 len0 = w * h;
    if (len0 / w != h)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;
    PRUint32 len = len0 * 4;
    if (len / 4 != len0)
        return NS_ERROR_DOM_INDEX_SIZE_ERR;

    nsAutoArrayPtr<jsval> jsvector(new (std::nothrow) jsval[w * h * 4]);
    if (!jsvector)
        return NS_ERROR_OUT_OF_MEMORY;

    jsval *dest = jsvector.get();
    for (PRUint32 i = 0; i < len; i++)
        *dest++ = JSVAL_ZERO;

    JSObject *dataArray = JS_NewArrayObject(ctx, w*h*4, jsvector.get());
    if (!dataArray)
        return NS_ERROR_OUT_OF_MEMORY;

    nsAutoGCRoot arrayGCRoot(&dataArray, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    JSObject *result = JS_NewObject(ctx, NULL, NULL, NULL);
    if (!result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsAutoGCRoot resultGCRoot(&result, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_DefineProperty(ctx, result, "width", INT_TO_JSVAL(w), NULL, NULL, 0) ||
        !JS_DefineProperty(ctx, result, "height", INT_TO_JSVAL(h), NULL, NULL, 0) ||
        !JS_DefineProperty(ctx, result, "data", OBJECT_TO_JSVAL(dataArray), NULL, NULL, 0))
        return NS_ERROR_FAILURE;

    jsval *retvalPtr;
    ncc->GetRetValPtr(&retvalPtr);
    *retvalPtr = OBJECT_TO_JSVAL(result);
    ncc->SetReturnValueWasSet(PR_TRUE);

    return NS_OK;

}

