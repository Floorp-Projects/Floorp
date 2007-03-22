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

#ifdef MOZILLA_1_8_BRANCH
#include "nsIScriptGlobalObject.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#endif

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

#include "nsPrintfCString.h"

#include "nsReadableUtils.h"

#include "nsColor.h"
#include "nsTransform2D.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsIBlender.h"
#include "nsGfxCIID.h"
#include "nsIDrawingSurface.h"
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

#include "cairo.h"
#include "imgIEncoder.h"
#ifdef MOZILLA_1_8_BRANCH
#define imgIEncoder imgIEncoder_MOZILLA_1_8_BRANCH
#endif

#ifdef MOZ_CAIRO_GFX
#include "gfxContext.h"
#include "gfxASurface.h"
#include "gfxPlatform.h"

#include "nsDisplayList.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsFrameManager.h"
#include "nsRegion.h"
#endif

#ifdef XP_WIN
#include "cairo-win32.h"

#ifdef MOZILLA_1_8_BRANCH
extern "C" {
cairo_surface_t *
_cairo_win32_surface_create_dib (cairo_format_t format,
                                 int	        width,
                                 int	        height);
}
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#define M_PI_2		1.57079632679489661923
#endif
#endif

#ifdef XP_OS2
#define INCL_WINWINDOWMGR
#define INCL_GPIBITMAPS
#include <os2.h>
#include "nsDrawingSurfaceOS2.h"
#include "cairo-os2.h"
#endif

#ifdef MOZ_WIDGET_GTK2
#include "cairo-xlib.h"
#include "cairo-xlib-xrender.h"
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#endif

#ifdef XP_MACOSX
#include <Quickdraw.h>
#include <CGContext.h>

/* This has to be 0 on machines less than 10.4 (which are always PPC);
 * otherwise the older CG gets confused if we pass in
 * kCGBitmapByteOrder32Big since it doesn't understand it.
 */
#ifdef __BIG_ENDIAN__
#define CG_BITMAP_BYTE_ORDER_FLAG 0
#else    /* Little endian. */
/* x86, and will be a 10.4u SDK; ByteOrder32Host will be defined */
#define CG_BITMAP_BYTE_ORDER_FLAG kCGBitmapByteOrder32Host
#endif

#ifndef MOZ_CAIRO_GFX
#include "nsDrawingSurfaceMac.h"
#endif

#endif

static NS_DEFINE_IID(kBlenderCID, NS_BLENDER_CID);

/* Maximum depth of save() which has style information saved */
#define STYLE_STACK_DEPTH 50
#define STYLE_CURRENT_STACK ((mSaveCount<STYLE_STACK_DEPTH)?mSaveCount:STYLE_STACK_DEPTH-1)

static PRBool CheckSaneImageSize (PRInt32 width, PRInt32 height);
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
#ifdef MOZILLA_1_8_BRANCH
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_CANVASGRADIENT_PRIVATE_IID)
#else
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASGRADIENT_PRIVATE_IID)
#endif

    nsCanvasGradient(cairo_pattern_t *cpat, nsICSSParser *cssparser)
        : mPattern(cpat), mCSSParser(cssparser)
    {
    }

    ~nsCanvasGradient() {
        if (mPattern)
            cairo_pattern_destroy(mPattern);
    }

    void Apply(cairo_t *cairo) {
        cairo_set_source(cairo, mPattern);
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

        nsresult rv = mCSSParser->ParseColorString(nsString(colorstr), nsnull, 0, PR_TRUE, &color);
        if (NS_FAILED(rv))
            return NS_ERROR_DOM_SYNTAX_ERR;

        cairo_pattern_add_color_stop_rgba (mPattern, (double) offset,
                                           NS_GET_R(color) / 255.0,
                                           NS_GET_G(color) / 255.0,
                                           NS_GET_B(color) / 255.0,
                                           NS_GET_A(color) / 255.0);
        return NS_OK;
    }

    NS_DECL_ISUPPORTS

protected:
    cairo_pattern_t *mPattern;
    nsCOMPtr<nsICSSParser> mCSSParser;
};

#ifndef MOZILLA_1_8_BRANCH
NS_DEFINE_STATIC_IID_ACCESSOR(nsCanvasGradient, NS_CANVASGRADIENT_PRIVATE_IID)
#endif

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
#ifdef MOZILLA_1_8_BRANCH
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_CANVASPATTERN_PRIVATE_IID)
#else
    NS_DECLARE_STATIC_IID_ACCESSOR(NS_CANVASPATTERN_PRIVATE_IID)
#endif

    nsCanvasPattern(cairo_pattern_t *cpat, PRUint8 *dataToFree,
                    nsIURI* URIForSecurityCheck, PRBool forceWriteOnly)
        : mPattern(cpat), mData(dataToFree), mURI(URIForSecurityCheck), mForceWriteOnly(forceWriteOnly)
    { }

    ~nsCanvasPattern() {
        if (mPattern)
            cairo_pattern_destroy(mPattern);
        if (mData)
            nsMemory::Free(mData);
    }

    void Apply(cairo_t *cairo) {
        cairo_set_source(cairo, mPattern);
    }
    
    nsIURI* GetURI() { return mURI; }
    PRBool GetForceWriteOnly() { return mForceWriteOnly; }

    NS_DECL_ISUPPORTS

protected:
    cairo_pattern_t *mPattern;
    PRUint8 *mData;
    nsCOMPtr<nsIURI> mURI;
    PRPackedBool mForceWriteOnly;
};

#ifndef MOZILLA_1_8_BRANCH
NS_DEFINE_STATIC_IID_ACCESSOR(nsCanvasPattern, NS_CANVASPATTERN_PRIVATE_IID)
#endif

NS_IMPL_ADDREF(nsCanvasPattern)
NS_IMPL_RELEASE(nsCanvasPattern)

NS_INTERFACE_MAP_BEGIN(nsCanvasPattern)
  NS_INTERFACE_MAP_ENTRY(nsCanvasPattern)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasPattern)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasPattern)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

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
    void SetCairoColor(nscolor c);

    // nsICanvasRenderingContextInternal
    NS_IMETHOD SetCanvasElement(nsICanvasElement* aParentCanvas);
    NS_IMETHOD SetDimensions(PRInt32 width, PRInt32 height);
    NS_IMETHOD Render(nsIRenderingContext *rc);
    NS_IMETHOD RenderToSurface(cairo_surface_t *surf);
    NS_IMETHOD GetInputStream(const nsACString& aMimeType,
                              const nsAString& aEncoderOptions,
                              nsIInputStream **aStream);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMCanvasRenderingContext2D interface
    NS_DECL_NSIDOMCANVASRENDERINGCONTEXT2D

protected:
    // destroy cairo/image stuff, in preparation for possibly recreating
    void Destroy();

    nsIFrame *GetCanvasLayoutFrame();

    // Some helpers.  Doesn't modify acolor on failure.
    enum {
        STYLE_STROKE = 0,
        STYLE_FILL,
        STYLE_SHADOW
        //STYLE_MAX
    };

    // VC6 sucks
#define STYLE_MAX 3

    nsresult SetStyleFromVariant(nsIVariant* aStyle, PRInt32 aWhichStyle);
    void StyleColorToString(const nscolor& aColor, nsAString& aStr);

    void DirtyAllStyles();
    void ApplyStyle(PRInt32 aWhichStyle);
    
    // If aURI has a different origin than the current script, then
    // we make the canvas write-only so bad guys can't extract the pixel
    // data.  If forceWriteOnly is set, we force write only to be set
    // and ignore aURI.  (This is used for when the original data came
    // from a <canvas> that had write-only set.)
    void DoDrawImageSecurityCheck(nsIURI* aURI, PRBool forceWriteOnly);

    // Member vars
    PRInt32 mWidth, mHeight;

    // the canvas element informs us when it's going away,
    // so these are not nsCOMPtrs
    nsICanvasElement* mCanvasElement;

    // our CSS parser, for colors and whatnot
    nsCOMPtr<nsICSSParser> mCSSParser;

    // yay cairo
#ifdef MOZ_CAIRO_GFX
    nsRefPtr<gfxContext> mThebesContext;
    nsRefPtr<gfxASurface> mThebesSurface;
#endif

    PRUint32 mSaveCount;
    cairo_t *mCairo;
    cairo_surface_t *mSurface;

    // only non-null if mSurface is an image surface
    PRUint8 *mImageSurfaceData;

    // style handling
    PRInt32 mLastStyle;
    PRPackedBool mDirtyStyle[STYLE_MAX];

    // state stack handling
    class ContextState {
    public:
        ContextState() : globalAlpha(1.0) { }

        ContextState(const ContextState& other)
            : globalAlpha(other.globalAlpha)
        {
            for (int i = 0; i < STYLE_MAX; i++) {
                colorStyles[i] = other.colorStyles[i];
                gradientStyles[i] = other.gradientStyles[i];
                patternStyles[i] = other.patternStyles[i];
            }
        }

        inline void SetColorStyle(int whichStyle, nscolor color) {
            colorStyles[whichStyle] = color;
            gradientStyles[whichStyle] = nsnull;
            patternStyles[whichStyle] = nsnull;
        }

        inline void SetPatternStyle(int whichStyle, nsCanvasPattern* pat) {
            gradientStyles[whichStyle] = nsnull;
            patternStyles[whichStyle] = pat;
        }

        inline void SetGradientStyle(int whichStyle, nsCanvasGradient* grad) {
            gradientStyles[whichStyle] = grad;
            patternStyles[whichStyle] = nsnull;
        }

        float globalAlpha;
        nscolor colorStyles[STYLE_MAX];
        nsCOMPtr<nsCanvasGradient> gradientStyles[STYLE_MAX];
        nsCOMPtr<nsCanvasPattern> patternStyles[STYLE_MAX];
    };

    nsTArray<ContextState> mStyleStack;

    inline ContextState& CurrentState() {
        return mStyleStack[mSaveCount];
    }

#ifdef MOZ_WIDGET_GTK2
    Pixmap mSurfacePixmap;
#endif

    // stolen from nsJSUtils
    static PRBool ConvertJSValToUint32(PRUint32* aProp, JSContext* aContext,
                                       jsval aValue);
    static PRBool ConvertJSValToXPCObject(nsISupports** aSupports, REFNSIID aIID,
                                          JSContext* aContext, jsval aValue);
    static PRBool ConvertJSValToDouble(double* aProp, JSContext* aContext,
                                       jsval aValue);

    // cairo helpers
    nsresult CairoSurfaceFromElement(nsIDOMElement *imgElt,
                                     cairo_surface_t **aCairoSurface,
                                     PRUint8 **imgDataOut,
                                     PRInt32 *widthOut, PRInt32 *heightOut,
                                     nsIURI **uriOut, PRBool *forceWriteOnlyOut);

    nsresult DrawNativeSurfaces(nsIDrawingSurface* aBlackSurface,
                                nsIDrawingSurface* aWhiteSurface,
                                const nsIntSize& aSurfaceSize,
                                nsIRenderingContext* aBlackContext);
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
    nsIDOMCanvasRenderingContext2D* ctx = new nsCanvasRenderingContext2D();
    if (!ctx)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult = ctx);
    return NS_OK;
}

nsCanvasRenderingContext2D::nsCanvasRenderingContext2D()
    : mCanvasElement(nsnull),
      mSaveCount(0), mCairo(nsnull), mSurface(nsnull), mImageSurfaceData(nsnull), mStyleStack(20)
{
#ifdef MOZ_WIDGET_GTK2
    mSurfacePixmap = None;
#endif
}

nsCanvasRenderingContext2D::~nsCanvasRenderingContext2D()
{
    Destroy();
}

nsIFrame*
nsCanvasRenderingContext2D::GetCanvasLayoutFrame()
{
    if (!mCanvasElement)
        return nsnull;

    nsIFrame *fr = nsnull;
    mCanvasElement->GetPrimaryCanvasFrame(&fr);
    return fr;
}

void
nsCanvasRenderingContext2D::Destroy()
{
    if (mCairo) {
        cairo_destroy(mCairo);
        mCairo = nsnull;
    }

    if (mSurface) {
        cairo_surface_destroy(mSurface);
        mSurface = nsnull;
    }

#ifdef MOZ_WIDGET_GTK2
    if (mSurfacePixmap != None) {
        XFreePixmap(GDK_DISPLAY(), mSurfacePixmap);
        mSurfacePixmap = None;
    }
#endif

    if (mImageSurfaceData) {
        PR_Free (mImageSurfaceData);
        mImageSurfaceData = nsnull;
    }
}

nsresult
nsCanvasRenderingContext2D::SetStyleFromVariant(nsIVariant* aStyle, PRInt32 aWhichStyle)
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

        rv = mCSSParser->ParseColorString(str, nsnull, 0, PR_TRUE, &color);
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
nsCanvasRenderingContext2D::DoDrawImageSecurityCheck(nsIURI* aURI, PRBool forceWriteOnly)
{
    if (mCanvasElement->IsWriteOnly())
        return;

    // If we explicitly set WriteOnly just do it and get out
    if (forceWriteOnly) {
        mCanvasElement->SetWriteOnly();
        return;
    }

    // Further security checks require a URI. If we don't have one the image
    // must be another canvas and we inherit the forceWriteOnly state
    if (!aURI)
        return;

    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();

#ifdef MOZILLA_1_8_BRANCH
    nsCOMPtr<nsIDOMNode> elem = do_QueryInterface(mCanvasElement);
    if (elem && ssm) {
        nsCOMPtr<nsIPrincipal> elemPrincipal;
        nsCOMPtr<nsIPrincipal> uriPrincipal;
        nsCOMPtr<nsIDocument> elemDocument;
        nsContentUtils::GetDocumentAndPrincipal(elem, getter_AddRefs(elemDocument), getter_AddRefs(elemPrincipal));
        ssm->GetCodebasePrincipal(aURI, getter_AddRefs(uriPrincipal));

        if (uriPrincipal && elemPrincipal) {
            nsresult rv =
                ssm->CheckSameOriginPrincipal(elemPrincipal, uriPrincipal);
            if (NS_SUCCEEDED(rv)) {
                // Same origin
                return;
            }
        }
    }
#else
    nsCOMPtr<nsINode> elem = do_QueryInterface(mCanvasElement);
    if (elem && ssm) {
        nsCOMPtr<nsIPrincipal> uriPrincipal;
        ssm->GetCodebasePrincipal(aURI, getter_AddRefs(uriPrincipal));

        if (uriPrincipal) {
            nsresult rv = ssm->CheckSameOriginPrincipal(elem->NodePrincipal(),
                                                        uriPrincipal);
            if (NS_SUCCEEDED(rv)) {
                // Same origin
                return;
            }
        }
    }
#endif

    mCanvasElement->SetWriteOnly();
}

void
nsCanvasRenderingContext2D::ApplyStyle(PRInt32 aWhichStyle)
{
    if (mLastStyle == aWhichStyle &&
        !mDirtyStyle[aWhichStyle])
    {
        // nothing to do, this is already the set style
        return;
    }

    mDirtyStyle[aWhichStyle] = PR_FALSE;
    mLastStyle = aWhichStyle;

    nsCanvasPattern* pattern = CurrentState().patternStyles[aWhichStyle];
    if (pattern) {
        DoDrawImageSecurityCheck(pattern->GetURI(), pattern->GetForceWriteOnly());
        pattern->Apply(mCairo);
        return;
    }

    if (CurrentState().gradientStyles[aWhichStyle]) {
        CurrentState().gradientStyles[aWhichStyle]->Apply(mCairo);
        return;
    }

    SetCairoColor(CurrentState().colorStyles[aWhichStyle]);
}

nsresult
nsCanvasRenderingContext2D::Redraw()
{
    nsIFrame *frame = GetCanvasLayoutFrame();
    if (frame) {
        nsRect r = frame->GetRect();
        r.x = r.y = 0;
        frame->Invalidate(r, PR_FALSE);
    }

    return NS_OK;
}

void
nsCanvasRenderingContext2D::SetCairoColor(nscolor c)
{
    double r = double(NS_GET_R(c) / 255.0);
    double g = double(NS_GET_G(c) / 255.0);
    double b = double(NS_GET_B(c) / 255.0);
    double a = double(NS_GET_A(c) / 255.0) * CurrentState().globalAlpha;

    cairo_set_source_rgba (mCairo, r, g, b, a);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetDimensions(PRInt32 width, PRInt32 height)
{
    Destroy();

    // Check that the dimensions are sane
    if (!CheckSaneImageSize(width, height))
        return NS_ERROR_FAILURE;

    mWidth = width;
    mHeight = height;

#ifdef MOZ_CAIRO_GFX
    mThebesSurface = gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(width, height), gfxASurface::ImageFormatARGB32);
    mThebesContext = new gfxContext(mThebesSurface);

    mSurface = mThebesSurface->CairoSurface();
    cairo_surface_reference(mSurface);
    mCairo = mThebesContext->GetCairo();
    cairo_reference(mCairo);
#else
    // non-cairo gfx
#ifdef XP_WIN
#ifndef MOZILLA_1_8_BRANCH
    mSurface = cairo_win32_surface_create_with_dib (CAIRO_FORMAT_ARGB32,
                                                    mWidth, mHeight);
#else
    mSurface = _cairo_win32_surface_create_dib (CAIRO_FORMAT_ARGB32,
                                                mWidth, mHeight);
#endif
#elif MOZ_WIDGET_GTK2
    // On most current X servers, using the software-only surface
    // actually provides a much smoother and faster display.
    // However, we provide MOZ_CANVAS_USE_RENDER for whomever wants to
    // go that route.
    if (getenv("MOZ_CANVAS_USE_RENDER")) {
        XRenderPictFormat *fmt = XRenderFindStandardFormat (GDK_DISPLAY(),
                                                            PictStandardARGB32);
        if (fmt) {
            int npfmts = 0;
            XPixmapFormatValues *pfmts = XListPixmapFormats(GDK_DISPLAY(), &npfmts);
            for (int i = 0; i < npfmts; i++) {
                if (pfmts[i].depth == 32) {
                    npfmts = -1;
                    break;
                }
            }
            XFree(pfmts);

            if (npfmts == -1) {
                mSurfacePixmap = XCreatePixmap (GDK_DISPLAY(),
                                                DefaultRootWindow(GDK_DISPLAY()),
                                                width, height, 32);
                mSurface = cairo_xlib_surface_create_with_xrender_format
                    (GDK_DISPLAY(), mSurfacePixmap, DefaultScreenOfDisplay(GDK_DISPLAY()),
                     fmt, mWidth, mHeight);
            }
        }
    }
#endif

    // fall back to image surface
    if (!mSurface) {
        mImageSurfaceData = (PRUint8*) PR_Malloc (mWidth * mHeight * 4);
        if (!mImageSurfaceData)
            return NS_ERROR_OUT_OF_MEMORY;

        mSurface = cairo_image_surface_create_for_data (mImageSurfaceData,
                                                        CAIRO_FORMAT_ARGB32,
                                                        mWidth, mHeight,
                                                        mWidth * 4);
    }

    mCairo = cairo_create(mSurface);
#endif

    // set up the initial canvas defaults
    mStyleStack.Clear();
    mSaveCount = 0;

    ContextState *state = mStyleStack.AppendElement();
    state->globalAlpha = 1.0;
    for (int i = 0; i < STYLE_MAX; i++)
        state->colorStyles[i] = NS_RGB(0,0,0);
    mLastStyle = -1;

    DirtyAllStyles();

    cairo_set_operator(mCairo, CAIRO_OPERATOR_CLEAR);
    cairo_new_path(mCairo);
    cairo_rectangle(mCairo, 0, 0, mWidth, mHeight);
    cairo_fill(mCairo);

    cairo_set_line_width(mCairo, 1.0);
    cairo_set_operator(mCairo, CAIRO_OPERATOR_OVER);
    cairo_set_miter_limit(mCairo, 10.0);
    cairo_set_line_cap(mCairo, CAIRO_LINE_CAP_BUTT);
    cairo_set_line_join(mCairo, CAIRO_LINE_JOIN_MITER);

    cairo_new_path(mCairo);

    return NS_OK;
}
 
NS_IMETHODIMP
nsCanvasRenderingContext2D::Render(nsIRenderingContext *rc)
{
    nsresult rv = NS_OK;

#ifdef MOZ_CAIRO_GFX

    gfxContext* ctx = (gfxContext*) rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);
    nsRefPtr<gfxPattern> pat = new gfxPattern(mThebesSurface);

    // XXX I don't want to use PixelSnapped here, but layout doesn't guarantee
    // pixel alignment for this stuff!
    ctx->NewPath();
    ctx->PixelSnappedRectangleAndSetPattern(gfxRect(0, 0, mWidth, mHeight), pat);
    ctx->Fill();

#else

    // non-Thebes; this becomes exciting
    cairo_surface_t *dest = nsnull;
    cairo_t *dest_cr = nsnull;

#ifdef XP_WIN
    void *ptr = nsnull;
#ifdef MOZILLA_1_8_BRANCH
    rv = rc->RetrieveCurrentNativeGraphicData(&ptr);
    if (NS_FAILED(rv) || !ptr)
        return NS_ERROR_FAILURE;
#else
    ptr = rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_WINDOWS_DC);
#endif
    HDC dc = (HDC) ptr;

    dest = cairo_win32_surface_create (dc);
    dest_cr = cairo_create (dest);
#endif

#ifdef XP_OS2
    void *ptr = nsnull;
#ifdef MOZILLA_1_8_BRANCH
    rv = rc->RetrieveCurrentNativeGraphicData(&ptr);
    if (NS_FAILED(rv) || !ptr)
        return NS_ERROR_FAILURE;
#else
    /* OS/2 also uses NATIVE_WINDOWS_DC to get a native OS/2 PS */
    ptr = rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_WINDOWS_DC);
#endif

    HPS hps = (HPS)ptr;
    nsDrawingSurfaceOS2 *surface; /* to get the dimensions from this */
    PRUint32 width, height;
    rc->GetDrawingSurface((nsIDrawingSurface**)&surface);
    surface->GetDimensions(&width, &height);

    dest = cairo_os2_surface_create(hps, width, height);
    cairo_surface_mark_dirty(dest); // needed on OS/2 for initialization
    dest_cr = cairo_create(dest);
#endif

#ifdef MOZ_WIDGET_GTK2
    GdkDrawable *gdkdraw = nsnull;
#ifdef MOZILLA_1_8_BRANCH
    rv = rc->RetrieveCurrentNativeGraphicData((void**) &gdkdraw);
    if (NS_FAILED(rv) || !gdkdraw)
        return NS_ERROR_FAILURE;
#else
    gdkdraw = (GdkDrawable*) rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_GDK_DRAWABLE);
    if (!gdkdraw)
        return NS_ERROR_FAILURE;
#endif

    gint w, h;
    gdk_drawable_get_size (gdkdraw, &w, &h);
    dest = cairo_xlib_surface_create (GDK_DRAWABLE_XDISPLAY(gdkdraw),
                                      GDK_DRAWABLE_XID(gdkdraw),
                                      GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(gdkdraw)),
                                      w, h);
    dest_cr = cairo_create (dest);
#endif

    nsTransform2D *tx = nsnull;
    rc->GetCurrentTransform(tx);

    nsCOMPtr<nsIDeviceContext> dctx;
    rc->GetDeviceContext(*getter_AddRefs(dctx));

    // Until we can use the quartz2 surface, mac will be different,
    // since we'll use CG to render.
#ifndef XP_MACOSX

    float x0 = 0.0, y0 = 0.0;
    float sx = 1.0, sy = 1.0;

    if (tx->GetType() & MG_2DTRANSLATION) {
        tx->Transform(&x0, &y0);
    }

    if (tx->GetType() & MG_2DSCALE) {
        sx = sy = dctx->DevUnitsToTwips();
        tx->TransformNoXLate(&sx, &sy);
    }

    cairo_translate (dest_cr, NSToIntRound(x0), NSToIntRound(y0));
    if (sx != 1.0 || sy != 1.0)
        cairo_scale (dest_cr, sx, sy);

    cairo_rectangle (dest_cr, 0, 0, mWidth, mHeight);
    cairo_clip (dest_cr);

    cairo_set_source_surface (dest_cr, mSurface, 0, 0);
    cairo_paint (dest_cr);

    if (dest_cr)
        cairo_destroy (dest_cr);
    if (dest)
        cairo_surface_destroy (dest);

#else

    // OSX path
    nsIDrawingSurface *ds = nsnull;
    rc->GetDrawingSurface(&ds);
    if (!ds)
        return NS_ERROR_FAILURE;

    nsDrawingSurfaceMac *macds = NS_STATIC_CAST(nsDrawingSurfaceMac*, ds);
    CGContextRef cgc = macds->StartQuartzDrawing();

    CGDataProviderRef dataProvider;
    CGImageRef img;

    dataProvider = CGDataProviderCreateWithData (NULL, mImageSurfaceData,
                                                 mWidth * mHeight * 4,
                                                 NULL);
    CGColorSpaceRef rgb = CGColorSpaceCreateDeviceRGB();
    img = CGImageCreate (mWidth, mHeight, 8, 32, mWidth * 4, rgb,
                         (CGImageAlphaInfo)(kCGImageAlphaPremultipliedFirst | CG_BITMAP_BYTE_ORDER_FLAG),
                         dataProvider, NULL, false, kCGRenderingIntentDefault);
    CGColorSpaceRelease (rgb);
    CGDataProviderRelease (dataProvider);

    float x0 = 0.0, y0 = 0.0;
    float sx = 1.0, sy = 1.0;
    if (tx->GetType() & MG_2DTRANSLATION) {
        tx->Transform(&x0, &y0);
    }

    if (tx->GetType() & MG_2DSCALE) {
        float p2t = dctx->DevUnitsToTwips();
        sx = p2t, sy = p2t;
        tx->TransformNoXLate(&sx, &sy);
    }

    CGContextTranslateCTM (cgc, NSToIntRound(x0), NSToIntRound(y0));
    if (sx != 1.0 || sy != 1.0)
        CGContextScaleCTM (cgc, sx, sy);

    // flip, so that the image gets drawn correct-side up
    CGContextScaleCTM (cgc, 1.0, -1.0);
    CGContextDrawImage (cgc, CGRectMake(0, -mHeight, mWidth, mHeight), img);

    CGImageRelease (img);

    macds->EndQuartzDrawing(cgc);

    rv = NS_OK;
#endif
#endif

    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::RenderToSurface(cairo_surface_t *surf)
{
    cairo_t *cr = cairo_create (surf);
    cairo_set_source_surface (cr, mSurface, 0, 0);
    cairo_paint (cr);
    cairo_destroy (cr);

    return NS_OK;
}
 
NS_IMETHODIMP
nsCanvasRenderingContext2D::GetInputStream(const nsACString& aMimeType,
                                           const nsAString& aEncoderOptions,
                                           nsIInputStream **aStream)
{
    nsCString conid(NS_LITERAL_CSTRING("@mozilla.org/image/encoder;2?type="));
    conid += aMimeType;

    nsCOMPtr<imgIEncoder> encoder = do_CreateInstance(conid.get());
    if (!encoder)
        return NS_ERROR_FAILURE;

    if (mImageSurfaceData) {
        encoder->InitFromData(mImageSurfaceData,
                              mWidth * mHeight * 4, mWidth, mHeight, mWidth * 4,
                              imgIEncoder::INPUT_FORMAT_HOSTARGB,
                              aEncoderOptions);
    } else {
        nsAutoArrayPtr<PRUint8> imageBuffer((PRUint8*) PR_Malloc(sizeof(PRUint8) * mWidth * mHeight * 4));
        if (!imageBuffer)
            return NS_ERROR_FAILURE;

        cairo_surface_t *imgsurf = cairo_image_surface_create_for_data (imageBuffer.get(),
                                                                        CAIRO_FORMAT_ARGB32,
                                                                        mWidth, mHeight, mWidth * 4);
        if (!imgsurf || cairo_surface_status(imgsurf))
            return NS_ERROR_FAILURE;

        cairo_t *cr = cairo_create(imgsurf);
        cairo_surface_destroy (imgsurf);

        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface (cr, mSurface, 0, 0);
        cairo_paint (cr);
        cairo_destroy (cr);

        encoder->InitFromData(imageBuffer.get(),
                              mWidth * mHeight * 4, mWidth, mHeight, mWidth * 4,
                              imgIEncoder::INPUT_FORMAT_HOSTARGB,
                              aEncoderOptions);
    }

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
    cairo_save (mCairo);
    mSaveCount++;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Restore()
{
    if (mSaveCount <= 0)
        return NS_ERROR_DOM_INVALID_STATE_ERR;

    mStyleStack.RemoveElementAt(mSaveCount);
    cairo_restore (mCairo);

    mLastStyle = -1;
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

    cairo_scale (mCairo, x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Rotate(float angle)
{
    if (!FloatValidate(angle))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_rotate (mCairo, angle);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Translate(float x, float y)
{
    if (!FloatValidate(x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_translate (mCairo, x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    if (!FloatValidate(m11,m12,m21,m22,dx,dy))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_matrix_t mat;
    cairo_matrix_init (&mat, m11, m12, m21, m22, dx, dy);
    cairo_transform (mCairo, &mat);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    if (!FloatValidate(m11,m12,m21,m22,dx,dy))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_matrix_t mat;
    cairo_matrix_init (&mat, m11, m12, m21, m22, dx, dy);
    cairo_set_matrix (mCairo, &mat);
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

    NS_ADDREF(*aStyle = var);
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

    NS_ADDREF(*aStyle = var);
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

    cairo_pattern_t *gradpat = nsnull;
    gradpat = cairo_pattern_create_linear ((double) x0, (double) y0, (double) x1, (double) y1);
    nsCanvasGradient *grad = new nsCanvasGradient(gradpat, mCSSParser);
    if (!grad) {
        cairo_pattern_destroy(gradpat);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*_retval = grad);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::CreateRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1,
                                                 nsIDOMCanvasGradient **_retval)
{
    if (!FloatValidate(x0,y0,r0,x1,y1,r1))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_pattern_t *gradpat = nsnull;
    gradpat = cairo_pattern_create_radial ((double) x0, (double) y0, (double) r0,
                                           (double) x1, (double) y1, (double) r1);
    nsCanvasGradient *grad = new nsCanvasGradient(gradpat, mCSSParser);
    if (!grad) {
        cairo_pattern_destroy(gradpat);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*_retval = grad);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::CreatePattern(nsIDOMHTMLElement *image,
                                          const nsAString& repeat,
                                          nsIDOMCanvasPattern **_retval)
{
    nsresult rv;
    cairo_extend_t extend;

    if (repeat.IsEmpty() || repeat.EqualsLiteral("repeat")) {
        extend = CAIRO_EXTEND_REPEAT;
    } else if (repeat.EqualsLiteral("repeat-x")) {
        // XX
        extend = CAIRO_EXTEND_REPEAT;
    } else if (repeat.EqualsLiteral("repeat-y")) {
        // XX
        extend = CAIRO_EXTEND_REPEAT;
    } else if (repeat.EqualsLiteral("no-repeat")) {
        extend = CAIRO_EXTEND_NONE;
    } else {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    cairo_surface_t *imgSurf = nsnull;
    PRUint8 *imgData = nsnull;
    PRInt32 imgWidth, imgHeight;
    nsCOMPtr<nsIURI> uri;
    PRBool forceWriteOnly = PR_FALSE;
    rv = CairoSurfaceFromElement(image, &imgSurf, &imgData,
                                 &imgWidth, &imgHeight, getter_AddRefs(uri), &forceWriteOnly);
    if (NS_FAILED(rv))
        return rv;

    cairo_pattern_t *cairopat = cairo_pattern_create_for_surface(imgSurf);
    cairo_surface_destroy(imgSurf);

    cairo_pattern_set_extend (cairopat, extend);

    nsCanvasPattern *pat = new nsCanvasPattern(cairopat, imgData, uri, forceWriteOnly);
    if (!pat) {
        cairo_pattern_destroy(cairopat);
        nsMemory::Free(imgData);
        return NS_ERROR_OUT_OF_MEMORY;
    }

    NS_ADDREF(*_retval = pat);
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
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowOffsetX(float *x)
{
    *x = 0.0f;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetShadowOffsetY(float y)
{
    if (!FloatValidate(y))
        return NS_ERROR_DOM_SYNTAX_ERR;
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowOffsetY(float *y)
{
    *y = 0.0f;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetShadowBlur(float blur)
{
    if (!FloatValidate(blur))
        return NS_ERROR_DOM_SYNTAX_ERR;
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowBlur(float *blur)
{
    *blur = 0.0f;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetShadowColor(const nsAString& color)
{
    // XXX ERRMSG we need to report an error to developers here! (bug 329026)
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowColor(nsAString& color)
{
    color.SetIsVoid(PR_TRUE);
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

    cairo_save (mCairo);
    cairo_set_operator (mCairo, CAIRO_OPERATOR_CLEAR);
    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);
    cairo_fill (mCairo);
    cairo_restore (mCairo);

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::FillRect(float x, float y, float w, float h)
{
    if (!FloatValidate(x,y,w,h))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);

    ApplyStyle(STYLE_FILL);
    cairo_fill (mCairo);

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::StrokeRect(float x, float y, float w, float h)
{
    if (!FloatValidate(x,y,w,h))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);

    ApplyStyle(STYLE_STROKE);
    cairo_stroke (mCairo);

    return Redraw();
}

//
// path bits
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::BeginPath()
{
    cairo_new_path(mCairo);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::ClosePath()
{
    cairo_close_path(mCairo);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Fill()
{
    ApplyStyle(STYLE_FILL);
    cairo_fill_preserve(mCairo);
    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Stroke()
{
    ApplyStyle(STYLE_STROKE);
    cairo_stroke_preserve(mCairo);
    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Clip()
{
    cairo_clip_preserve(mCairo);
    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::MoveTo(float x, float y)
{
    if (!FloatValidate(x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_move_to(mCairo, x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::LineTo(float x, float y)
{
    if (!FloatValidate(x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_line_to(mCairo, x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::QuadraticCurveTo(float cpx, float cpy, float x, float y)
{
    if (!FloatValidate(cpx,cpy,x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    double cx, cy;

    // we will always have a current point, since beginPath forces
    // a moveto(0,0)
    cairo_get_current_point(mCairo, &cx, &cy);
    cairo_curve_to(mCairo,
                   (cx + cpx * 2.0) / 3.0,
                   (cy + cpy * 2.0) / 3.0,
                   (cpx * 2.0 + x) / 3.0,
                   (cpy * 2.0 + y) / 3.0,
                   x,
                   y);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::BezierCurveTo(float cp1x, float cp1y,
                                          float cp2x, float cp2y,
                                          float x, float y)
{
    if (!FloatValidate(cp1x,cp1y,cp2x,cp2y,x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_curve_to(mCairo, cp1x, cp1y, cp2x, cp2y, x, y);
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
    
    double x0, y0;
    double angle0, angle1, angle2, angled;
    double d0, d2;
    double sin_, cos_;
    double xc, yc, dc;
    int forward;

    cairo_get_current_point(mCairo, &x0, &y0);

    angle0 = atan2 (y0 - y1, x0 - x1); /* angle from (x1,y1) to (x0,y0) */
    angle2 = atan2 (y2 - y1, x2 - x1); /* angle from (x1,y1) to (x2,y2) */
    angle1 = (angle0 + angle2) / 2;    /* angle from (x1,y1) to (xc,yc) */

    angled = angle2 - angle0;          /* the angle (x0,y0)--(x1,y1)--(x2,y2) */

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

    angle0 += M_PI_2; /* angle from (xc,yc) to (x0,y0) */
    angle2 -= M_PI_2; /* angle from (xc,yc) to (x2,y2) */
    angled /= 2;      /* the angle (x0,y0)--(x1,y1)--(xc,yc) */


    /* distance from (x1,y1) to (x0,y0) */
    d0 = sqrt ((x0-x1)*(x0-x1)+(y0-y1)*(y0-y1));
    /* distance from (x2,y2) to (x0,y0) */
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
    xc = x1 + sin(angle1) * dc;
    yc = y1 + cos(angle1) * dc;


    /* the arc operation draws the line from current point (x0,y0)
     * to arc center too. */

    if (forward)
        cairo_arc (mCairo, xc, yc, radius, angle0, angle2);
    else
        cairo_arc_negative (mCairo, xc, yc, radius, angle2, angle0);

    cairo_line_to (mCairo, x2, y2);

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Arc(float x, float y, float r, float startAngle, float endAngle, int ccw)
{
    if (!FloatValidate(x,y,r,startAngle,endAngle))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (ccw)
        cairo_arc_negative (mCairo, x, y, r, startAngle, endAngle);
    else
        cairo_arc (mCairo, x, y, r, startAngle, endAngle);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Rect(float x, float y, float w, float h)
{
    if (!FloatValidate(x,y,w,h))
        return NS_ERROR_DOM_SYNTAX_ERR;

    cairo_rectangle (mCairo, x, y, w, h);
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

    cairo_set_line_width(mCairo, width);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetLineWidth(float *width)
{
    double d = cairo_get_line_width(mCairo);
    *width = (float) d;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetLineCap(const nsAString& capstyle)
{
    cairo_line_cap_t cap;

    if (capstyle.EqualsLiteral("butt"))
        cap = CAIRO_LINE_CAP_BUTT;
    else if (capstyle.EqualsLiteral("round"))
        cap = CAIRO_LINE_CAP_ROUND;
    else if (capstyle.EqualsLiteral("square"))
        cap = CAIRO_LINE_CAP_SQUARE;
    else
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_NOT_IMPLEMENTED;

    cairo_set_line_cap (mCairo, cap);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetLineCap(nsAString& capstyle)
{
    cairo_line_cap_t cap = cairo_get_line_cap(mCairo);

    if (cap == CAIRO_LINE_CAP_BUTT)
        capstyle.AssignLiteral("butt");
    else if (cap == CAIRO_LINE_CAP_ROUND)
        capstyle.AssignLiteral("round");
    else if (cap == CAIRO_LINE_CAP_SQUARE)
        capstyle.AssignLiteral("square");
    else
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetLineJoin(const nsAString& joinstyle)
{
    cairo_line_join_t j;

    if (joinstyle.EqualsLiteral("round"))
        j = CAIRO_LINE_JOIN_ROUND;
    else if (joinstyle.EqualsLiteral("bevel"))
        j = CAIRO_LINE_JOIN_BEVEL;
    else if (joinstyle.EqualsLiteral("miter"))
        j = CAIRO_LINE_JOIN_MITER;
    else
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        return NS_ERROR_NOT_IMPLEMENTED;

    cairo_set_line_join (mCairo, j);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetLineJoin(nsAString& joinstyle)
{
    cairo_line_join_t j = cairo_get_line_join(mCairo);

    if (j == CAIRO_LINE_JOIN_ROUND)
        joinstyle.AssignLiteral("round");
    else if (j == CAIRO_LINE_JOIN_BEVEL)
        joinstyle.AssignLiteral("bevel");
    else if (j == CAIRO_LINE_JOIN_MITER)
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

    cairo_set_miter_limit(mCairo, miter);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetMiterLimit(float *miter)
{
    double d = cairo_get_miter_limit(mCairo);
    *miter = (float) d;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::IsPointInPath(float x, float y, PRBool *retVal)
{
    if (!FloatValidate(x,y))
        return NS_ERROR_DOM_SYNTAX_ERR;

    *retVal = (PRBool) cairo_in_fill(mCairo, x, y);
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

    nsCOMPtr<nsIXPCNativeCallContext> ncc;
    rv = nsContentUtils::XPConnect()->
        GetCurrentNativeCallContext(getter_AddRefs(ncc));
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

    double sx,sy,sw,sh;
    double dx,dy,dw,dh;

    nsCOMPtr<nsIDOMElement> imgElt;
    if (!ConvertJSValToXPCObject(getter_AddRefs(imgElt),
                                 NS_GET_IID(nsIDOMElement),
                                 ctx, argv[0]))
        return NS_ERROR_DOM_TYPE_MISMATCH_ERR;

    cairo_surface_t *imgSurf = nsnull;
    cairo_matrix_t surfMat;
    cairo_pattern_t* pat;
    PRUint8 *imgData = nsnull;
    PRInt32 imgWidth, imgHeight;
    nsCOMPtr<nsIURI> uri;
    PRBool forceWriteOnly = PR_FALSE;
    rv = CairoSurfaceFromElement(imgElt, &imgSurf, &imgData,
                                 &imgWidth, &imgHeight, getter_AddRefs(uri), &forceWriteOnly);
    if (NS_FAILED(rv))
        return rv;
    DoDrawImageSecurityCheck(uri, forceWriteOnly);

#define GET_ARG(dest,whicharg) \
    do { if (!ConvertJSValToDouble(dest, ctx, whicharg)) { rv = NS_ERROR_INVALID_ARG; goto FAIL; } } while (0)

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
        goto FAIL;
    }
#undef GET_ARG

    if (!FloatValidate(sx,sy,sw,sh))
        return NS_ERROR_DOM_SYNTAX_ERR;
    if (!FloatValidate(dx,dy,dw,dh))
        return NS_ERROR_DOM_SYNTAX_ERR;

    // check args
    if (sx < 0.0 || sy < 0.0 ||
        sw < 0.0 || sw > (double) imgWidth ||
        sh < 0.0 || sh > (double) imgHeight ||
        dw < 0.0 || dh < 0.0)
    {
        // XXX ERRMSG we need to report an error to developers here! (bug 329026)
        rv = NS_ERROR_DOM_INDEX_SIZE_ERR;
        goto FAIL;
    }

    cairo_matrix_init_translate(&surfMat, sx, sy);
    cairo_matrix_scale(&surfMat, sw/dw, sh/dh);
    pat = cairo_pattern_create_for_surface(imgSurf);
    cairo_pattern_set_matrix(pat, &surfMat);

    cairo_save(mCairo);
    cairo_translate(mCairo, dx, dy);
    cairo_new_path(mCairo);
    cairo_rectangle(mCairo, 0, 0, dw, dh);
    cairo_set_source(mCairo, pat);
    cairo_clip(mCairo);
    cairo_paint_with_alpha(mCairo, CurrentState().globalAlpha);
    cairo_restore(mCairo);

#if 1
    // XXX cairo bug workaround; force a clip update on mCairo.
    // Otherwise, a pixman clip gets left around somewhere, and pixman
    // (Render) does source clipping as well -- so we end up
    // compositing with an incorrect clip.  This only seems to affect
    // fallback cases, which happen when we have CSS scaling going on.
    // This will blow away the current path, but we already blew it
    // away in this function earlier.
    cairo_new_path(mCairo);
    cairo_rectangle(mCairo, 0, 0, 0, 0);
    cairo_fill(mCairo);
#endif

    cairo_pattern_destroy(pat);

FAIL:
    if (imgData)
        nsMemory::Free(imgData);
    if (imgSurf)
        cairo_surface_destroy(imgSurf);

    if (NS_SUCCEEDED(rv))
        rv = Redraw();

    return rv;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetGlobalCompositeOperation(const nsAString& op)
{
    cairo_operator_t cairo_op;

#define CANVAS_OP_TO_CAIRO_OP(cvsop,cairoop) \
    if (op.EqualsLiteral(cvsop))   \
        cairo_op = CAIRO_OPERATOR_##cairoop;

    // XXX "darker" isn't really correct
    CANVAS_OP_TO_CAIRO_OP("clear", CLEAR)
    else CANVAS_OP_TO_CAIRO_OP("copy", SOURCE)
    else CANVAS_OP_TO_CAIRO_OP("darker", SATURATE)  // XXX
    else CANVAS_OP_TO_CAIRO_OP("destination-atop", DEST_ATOP)
    else CANVAS_OP_TO_CAIRO_OP("destination-in", DEST_IN)
    else CANVAS_OP_TO_CAIRO_OP("destination-out", DEST_OUT)
    else CANVAS_OP_TO_CAIRO_OP("destination-over", DEST_OVER)
    else CANVAS_OP_TO_CAIRO_OP("lighter", ADD)
    else CANVAS_OP_TO_CAIRO_OP("source-atop", ATOP)
    else CANVAS_OP_TO_CAIRO_OP("source-in", IN)
    else CANVAS_OP_TO_CAIRO_OP("source-out", OUT)
    else CANVAS_OP_TO_CAIRO_OP("source-over", OVER)
    else CANVAS_OP_TO_CAIRO_OP("xor", XOR)
    // not part of spec, kept here for compat
    else CANVAS_OP_TO_CAIRO_OP("over", OVER)
    else return NS_ERROR_NOT_IMPLEMENTED;

#undef CANVAS_OP_TO_CAIRO_OP

    cairo_set_operator(mCairo, cairo_op);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetGlobalCompositeOperation(nsAString& op)
{
    cairo_operator_t cairo_op = cairo_get_operator(mCairo);

#define CANVAS_OP_TO_CAIRO_OP(cvsop,cairoop) \
    if (cairo_op == CAIRO_OPERATOR_##cairoop) \
        op.AssignLiteral(cvsop);

    // XXX "darker" isn't really correct
    CANVAS_OP_TO_CAIRO_OP("clear", CLEAR)
    else CANVAS_OP_TO_CAIRO_OP("copy", SOURCE)
    else CANVAS_OP_TO_CAIRO_OP("darker", SATURATE)  // XXX
    else CANVAS_OP_TO_CAIRO_OP("destination-atop", DEST_ATOP)
    else CANVAS_OP_TO_CAIRO_OP("destination-in", DEST_IN)
    else CANVAS_OP_TO_CAIRO_OP("destination-out", DEST_OUT)
    else CANVAS_OP_TO_CAIRO_OP("destination-over", DEST_OVER)
    else CANVAS_OP_TO_CAIRO_OP("lighter", ADD)
    else CANVAS_OP_TO_CAIRO_OP("source-atop", ATOP)
    else CANVAS_OP_TO_CAIRO_OP("source-in", IN)
    else CANVAS_OP_TO_CAIRO_OP("source-out", OUT)
    else CANVAS_OP_TO_CAIRO_OP("source-over", OVER)
    else CANVAS_OP_TO_CAIRO_OP("xor", XOR)
    else return NS_ERROR_FAILURE;

#undef CANVAS_OP_TO_CAIRO_OP

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

/* cairo ARGB32 surfaces are ARGB stored as a packed 32-bit integer; on little-endian
 * platforms, they appear as BGRA bytes in the surface data.  The color values are also
 * stored with premultiplied alpha.
 */

nsresult
nsCanvasRenderingContext2D::CairoSurfaceFromElement(nsIDOMElement *imgElt,
                                                    cairo_surface_t **aCairoSurface,
                                                    PRUint8 **imgData,
                                                    PRInt32 *widthOut, PRInt32 *heightOut,
                                                    nsIURI **uriOut, PRBool *forceWriteOnlyOut)
{
    nsresult rv;

    nsCOMPtr<imgIContainer> imgContainer;

    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgElt);
    if (imageLoader) {
        nsCOMPtr<imgIRequest> imgRequest;
        rv = imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                     getter_AddRefs(imgRequest));
        NS_ENSURE_SUCCESS(rv, rv);
        if (!imgRequest)
            // XXX ERRMSG we need to report an error to developers here! (bug 329026)
            return NS_ERROR_NOT_AVAILABLE;

        nsCOMPtr<nsIURI> uri;
        rv = imageLoader->GetCurrentURI(uriOut);
        NS_ENSURE_SUCCESS(rv, rv);
       
        *forceWriteOnlyOut = PR_FALSE;

        rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        // maybe a canvas
        nsCOMPtr<nsICanvasElement> canvas = do_QueryInterface(imgElt);
        if (canvas) {
            PRUint32 w, h;
            rv = canvas->GetSize(&w, &h);
            NS_ENSURE_SUCCESS(rv, rv);

            cairo_surface_t *surf =
                cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            w, h);
            cairo_t *cr = cairo_create (surf);
            cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
            cairo_paint (cr);
            cairo_destroy (cr);

            rv = canvas->RenderContextsToSurface(surf);
            if (NS_FAILED(rv)) {
                cairo_surface_destroy (surf);
                return rv;
            }

            *aCairoSurface = surf;
            *imgData = nsnull;
            *widthOut = w;
            *heightOut = h;

            *uriOut = nsnull;
            *forceWriteOnlyOut = canvas->IsWriteOnly();

            return NS_OK;
        } else {
            NS_WARNING("No way to get surface from non-canvas, non-imageloader");
            return NS_ERROR_NOT_AVAILABLE;
        }
    }

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

#ifdef MOZ_CAIRO_GFX
    gfxASurface* gfxsurf = nsnull;
    rv = img->GetSurface(&gfxsurf);
    NS_ENSURE_SUCCESS(rv, rv);

    *aCairoSurface = gfxsurf->CairoSurface();
    cairo_surface_reference (*aCairoSurface);
    *imgData = nsnull;
#else
    //
    // We now need to create a cairo_surface with the same data as
    // this image element.
    //

    PRUint8 *cairoImgData = (PRUint8 *)nsMemory::Alloc(imgHeight * imgWidth * 4);
    PRUint8 *outData = cairoImgData;

    gfx_format format;
    rv = frame->GetFormat(&format);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = frame->LockImageData();
    if (img->GetHasAlphaMask())
        rv |= frame->LockAlphaData();
    if (NS_FAILED(rv)) {
        nsMemory::Free(cairoImgData);
        return NS_ERROR_FAILURE;
    }

    PRUint8 *inPixBits, *inAlphaBits = nsnull;
    PRUint32 inPixStride, inAlphaStride = 0;
    inPixBits = img->GetBits();
    inPixStride = img->GetLineStride();
    if (img->GetHasAlphaMask()) {
        inAlphaBits = img->GetAlphaBits();
        inAlphaStride = img->GetAlphaLineStride();
    }

    PRBool topToBottom = img->GetIsRowOrderTopToBottom();
    PRBool useBGR;

    // The gtk backend optimizes away the alpha mask of images
    // with a fully opaque alpha, but doesn't update its format (bug?);
    // you end up with a RGB_A8 image with GetHasAlphaMask() == false.
    // We need to treat that case as RGB.

    if ((format == gfxIFormats::RGB || format == gfxIFormats::BGR) ||
        (!(img->GetHasAlphaMask()) && (format == gfxIFormats::RGB_A8 || format == gfxIFormats::BGR_A8)))
    {
        useBGR = (format & 1);

#ifdef IS_BIG_ENDIAN
        useBGR = !useBGR;
#endif

        for (PRUint32 j = 0; j < (PRUint32) imgHeight; j++) {
            PRUint32 rowIndex;
            if (topToBottom)
                rowIndex = j;
            else
                rowIndex = imgHeight - j - 1;

            PRUint8 *inrowrgb = inPixBits + (inPixStride * rowIndex);

            for (PRUint32 i = 0; i < (PRUint32) imgWidth; i++) {
                // handle rgb data; no alpha to premultiply
#ifdef XP_MACOSX
                // skip extra OSX byte
                inrowrgb++;
#endif
                PRUint8 b = *inrowrgb++;
                PRUint8 g = *inrowrgb++;
                PRUint8 r = *inrowrgb++;

#ifdef IS_BIG_ENDIAN
                // alpha
                *outData++ = 0xff;
#endif

                if (useBGR) {
                    *outData++ = b;
                    *outData++ = g;
                    *outData++ = r;
                } else {
                    *outData++ = r;
                    *outData++ = g;
                    *outData++ = b;
                }

#ifdef IS_LITTLE_ENDIAN
                // alpha
                *outData++ = 0xff;
#endif
            }
        }
        rv = NS_OK;
    } else if (format == gfxIFormats::RGB_A1 || format == gfxIFormats::BGR_A1) {
        useBGR = (format & 1);

#ifdef IS_BIG_ENDIAN
        useBGR = !useBGR;
#endif

        for (PRUint32 j = 0; j < (PRUint32) imgHeight; j++) {
            PRUint32 rowIndex;
            if (topToBottom)
                rowIndex = j;
            else
                rowIndex = imgHeight - j - 1;

            PRUint8 *inrowrgb = inPixBits + (inPixStride * rowIndex);
            PRUint8 *inrowalpha = inAlphaBits + (inAlphaStride * rowIndex);

            for (PRUint32 i = 0; i < (PRUint32) imgWidth; i++) {
                // pull out the bit value into alpha
                PRInt32 bit = i % 8;
                PRInt32 byte = i / 8;

#ifdef IS_LITTLE_ENDIAN
                PRUint8 a = (inrowalpha[byte] >> (7-bit)) & 1;
#else
                PRUint8 a = (inrowalpha[byte] >> bit) & 1;
#endif

#ifdef XP_MACOSX
                // skip extra X8 byte on OSX
                inrowrgb++;
#endif

                // handle rgb data; need to multiply the alpha out,
                // but we short-circuit that here since we know that a
                // can only be 0 or 1
                if (a) {
                    PRUint8 b = *inrowrgb++;
                    PRUint8 g = *inrowrgb++;
                    PRUint8 r = *inrowrgb++;

#ifdef IS_BIG_ENDIAN
                    // alpha
                    *outData++ = 0xff;
#endif

                    if (useBGR) {
                        *outData++ = b;
                        *outData++ = g;
                        *outData++ = r;
                    } else {
                        *outData++ = r;
                        *outData++ = g;
                        *outData++ = b;
                    }

#ifdef IS_LITTLE_ENDIAN
                    // alpha
                    *outData++ = 0xff;
#endif
                } else {
                    // alpha is 0, so we need to write all 0's,
                    // ignoring input color
                    inrowrgb += 3;
                    *outData++ = 0;
                    *outData++ = 0;
                    *outData++ = 0;
                    *outData++ = 0;
                }
            }
        }
        rv = NS_OK;
    } else if (format == gfxIFormats::RGB_A8 || format == gfxIFormats::BGR_A8) {
        useBGR = (format & 1);

#ifdef IS_BIG_ENDIAN
        useBGR = !useBGR;
#endif

        for (PRUint32 j = 0; j < (PRUint32) imgHeight; j++) {
            PRUint32 rowIndex;
            if (topToBottom)
                rowIndex = j;
            else
                rowIndex = imgHeight - j - 1;

            PRUint8 *inrowrgb = inPixBits + (inPixStride * rowIndex);
            PRUint8 *inrowalpha = inAlphaBits + (inAlphaStride * rowIndex);

            for (PRUint32 i = 0; i < (PRUint32) imgWidth; i++) {
                // pull out alpha; we'll need it to premultiply
                PRUint8 a = *inrowalpha++;

                // handle rgb data; we need to fully premultiply
                // with the alpha
#ifdef XP_MACOSX
                // skip extra X8 byte on OSX
                inrowrgb++;
#endif

                // XXX gcc bug: gcc seems to push "r" into a register
                // early, and pretends that it's in that register
                // throughout the 3 macros below.  At the end
                // of the 3rd macro, the correct r value is
                // calculated but never stored anywhere -- the r variable
                // has the value of the low byte of register that it
                // was stuffed into, which has the result of some 
                // intermediate calculation.
                // I've seen this on gcc 3.4.2 x86 (Fedora Core 3)
                // and gcc 3.3 PPC (OS X 10.3)

                //PRUint8 b, g, r;
                //FAST_DIVIDE_BY_255(b, *inrowrgb++ * a - a / 2);
                //FAST_DIVIDE_BY_255(g, *inrowrgb++ * a - a / 2);
                //FAST_DIVIDE_BY_255(r, *inrowrgb++ * a - a / 2);

                PRUint8 b = (*inrowrgb++ * a - a / 2) / 255;
                PRUint8 g = (*inrowrgb++ * a - a / 2) / 255;
                PRUint8 r = (*inrowrgb++ * a - a / 2) / 255;

#ifdef IS_BIG_ENDIAN
                *outData++ = a;
#endif

                if (useBGR) {
                    *outData++ = b;
                    *outData++ = g;
                    *outData++ = r;
                } else {
                    *outData++ = r;
                    *outData++ = g;
                    *outData++ = b;
                }

#ifdef IS_LITTLE_ENDIAN
                *outData++ = a;
#endif
            }
        }
        rv = NS_OK;
    } else {
        rv = NS_ERROR_FAILURE;
    }

    if (img->GetHasAlphaMask())
        frame->UnlockAlphaData();
    frame->UnlockImageData();

    if (NS_FAILED(rv)) {
        nsMemory::Free(cairoImgData);
        return rv;
    }

    cairo_surface_t *imgSurf =
        cairo_image_surface_create_for_data(cairoImgData, CAIRO_FORMAT_ARGB32,
                                            imgWidth, imgHeight, imgWidth*4);

    *aCairoSurface = imgSurf;
    *imgData = cairoImgData;
#endif

    return NS_OK;
}

PRBool
CheckSaneImageSize (PRInt32 width, PRInt32 height)
{
    if (width <= 0 || height <= 0)
        return PR_FALSE;

    /* check to make sure we don't overflow a 32-bit */
    PRInt32 tmp = width * height;
    if (tmp / height != width)
        return PR_FALSE;

    tmp = tmp * 4;
    if (tmp / 4 != width * height)
        return PR_FALSE;

    /* reject over-wide or over-tall images */
    const PRInt32 k64KLimit = 0x0000FFFF;
    if (width > k64KLimit || height > k64KLimit)
        return PR_FALSE;

    return PR_TRUE;
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
    // Note that because FlushPendingNotifications flushes parents, this
    // is O(N^2) in docshell tree depth.  However, the docshell tree is
    // usually pretty shallow.

    nsCOMPtr<nsIDOMDocument> domDoc;
    aWindow->GetDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domDoc);
    if (doc) {
        doc->FlushPendingNotifications(Flush_Layout);
    }

    nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(aWindow);
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
nsCanvasRenderingContext2D::DrawWindow(nsIDOMWindow* aWindow, PRInt32 aX, PRInt32 aY,
                                       PRInt32 aW, PRInt32 aH, 
                                       const nsAString& aBGColor)
{
    NS_ENSURE_ARG(aWindow != nsnull);

    // protect against too-large surfaces that will cause allocation
    // or overflow issues
    if (!CheckSaneImageSize (aW, aH))
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
    FlushLayoutForTree(aWindow);

    nsCOMPtr<nsPresContext> presContext;
#ifdef MOZILLA_1_8_BRANCH
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
    if (sgo) {
        nsIDocShell* docshell = sgo->GetDocShell();
        if (docshell) {
            docshell->GetPresContext(getter_AddRefs(presContext));
        }
    }
#else
    nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aWindow);
    if (win) {
        nsIDocShell* docshell = win->GetDocShell();
        if (docshell) {
            docshell->GetPresContext(getter_AddRefs(presContext));
        }
    }
#endif
    if (!presContext)
        return NS_ERROR_FAILURE;

#ifdef MOZILLA_1_8_BRANCH
    // Dig down past the viewport scroll stuff
    nsIViewManager* vm = presContext->GetViewManager();
    nsIView* view;
    vm->GetRootView(view);
    NS_ASSERTION(view, "Must have root view!");
#endif

    nscolor bgColor;
    nsresult rv = mCSSParser->ParseColorString(PromiseFlatString(aBGColor),
                                               nsnull, 0, PR_TRUE, &bgColor);
    NS_ENSURE_SUCCESS(rv, rv);

#ifndef MOZILLA_1_8_BRANCH    
    nsIPresShell* presShell = presContext->PresShell();
#endif

#ifdef MOZ_CAIRO_GFX
    mThebesContext->Save();
    //mThebesContext->NewPath();
    //mThebesContext->Rectangle(gfxRect(0, 0, aW, aH));
    //mThebesContext->Clip();

    // XXX vlad says this shouldn't both be COLOR_ALPHA but that it is a workaround for some bug
    mThebesContext->PushGroup(NS_GET_A(bgColor) == 0xff ? gfxASurface::CONTENT_COLOR_ALPHA : gfxASurface::CONTENT_COLOR_ALPHA);

    // draw background color
    if (NS_GET_A(bgColor) > 0) {
      mThebesContext->SetColor(gfxRGBA(bgColor));
      mThebesContext->SetOperator(gfxContext::OPERATOR_SOURCE);
      mThebesContext->Paint();
    }

    // we want the window to be composited as a single image using
    // whatever operator was set, so set this to the default OVER;
    // the original operator will be present when we PopGroup
    mThebesContext->SetOperator(gfxContext::OPERATOR_OVER);

    nsIFrame* rootFrame = presShell->FrameManager()->GetRootFrame();
    if (rootFrame) {
        // XXX This shadows the other |r|, above.
        nsRect r(nsPresContext::CSSPixelsToAppUnits(aX),
                 nsPresContext::CSSPixelsToAppUnits(aY),
                 nsPresContext::CSSPixelsToAppUnits(aW),
                 nsPresContext::CSSPixelsToAppUnits(aH));

        nsDisplayListBuilder builder(rootFrame, PR_FALSE, PR_FALSE);
        nsDisplayList list;
        nsIScrollableView* scrollingView = nsnull;       
        presContext->GetViewManager()->GetRootScrollableView(&scrollingView);

        if (scrollingView) {
            nscoord x, y;
            scrollingView->GetScrollPosition(x, y);
            r.MoveBy(-x, -y);
            builder.SetIgnoreScrollFrame(presShell->GetRootScrollFrame());
        }

        rv = rootFrame->BuildDisplayListForStackingContext(&builder, r, &list);      
        if (NS_SUCCEEDED(rv)) {
            float t2p = presContext->AppUnitsPerDevPixel();
            // Ensure that r.x,r.y gets drawn at (0,0)
            mThebesContext->Save();
            mThebesContext->Translate(gfxPoint(-r.x*t2p, -r.y*t2p));
          
            nsIDeviceContext* devCtx = presContext->DeviceContext();
            nsCOMPtr<nsIRenderingContext> rc;
            devCtx->CreateRenderingContextInstance(*getter_AddRefs(rc));
            rc->Init(devCtx, mThebesContext);
            
            nsRegion region(r);
            list.OptimizeVisibility(&builder, &region);
            list.Paint(&builder, rc, r);
            // Flush the list so we don't trigger the IsEmpty-on-destruction assertion
            list.DeleteAll();

            mThebesContext->Restore();
        }
    }

    mThebesContext->PopGroupToSource();
    mThebesContext->Paint();
    mThebesContext->Restore();

    // get rid of the pattern surface ref, just in case
    cairo_set_source_rgba (mCairo, 1, 1, 1, 1);
    DirtyAllStyles();

    Redraw();
#else

    nsCOMPtr<nsIRenderingContext> blackCtx;
#ifdef MOZILLA_1_8_BRANCH
    rv = vm->RenderOffscreen(view, r, PR_FALSE, PR_TRUE,
                             NS_ComposeColors(NS_RGB(0, 0, 0), bgColor),
                             getter_AddRefs(blackCtx));
#else
    rv = presShell->RenderOffscreen(r, PR_FALSE, PR_TRUE,
                                    NS_ComposeColors(NS_RGB(0, 0, 0), bgColor),
                                    getter_AddRefs(blackCtx));
#endif
    NS_ENSURE_SUCCESS(rv, rv);
    
    nsIDrawingSurface* blackSurface;
    blackCtx->GetDrawingSurface(&blackSurface);
    if (!blackSurface)
        return NS_ERROR_FAILURE;
    
    // Render it!
    if (NS_GET_A(bgColor) == 0xFF) {
        // opaque background. Do it the easy way.
        rv = DrawNativeSurfaces(blackSurface, nsnull, nsSize(aW, aH), blackCtx);
        blackCtx->DestroyDrawingSurface(blackSurface);
        return rv;
    }
    
    // transparent background. Do it the hard way. We've drawn onto black,
    // now draw onto white so we can recover the translucency information.
    // But we need to compose our given background color onto black/white
    // to get the real background to use.
    nsCOMPtr<nsIRenderingContext> whiteCtx;
#ifdef MOZILLA_1_8_BRANCH
    rv = vm->RenderOffscreen(view, r, PR_FALSE, PR_TRUE,
                             NS_ComposeColors(NS_RGB(255, 255, 255), bgColor),
                             getter_AddRefs(whiteCtx));
#else
    rv = presShell->RenderOffscreen(r, PR_FALSE, PR_TRUE,
                                    NS_ComposeColors(NS_RGB(255, 255, 255), bgColor),
                                    getter_AddRefs(whiteCtx));
#endif
    if (NS_SUCCEEDED(rv)) {
        nsIDrawingSurface* whiteSurface;
        whiteCtx->GetDrawingSurface(&whiteSurface);
        if (!whiteSurface) {
            rv = NS_ERROR_FAILURE;
        } else {
            rv = DrawNativeSurfaces(blackSurface, whiteSurface, nsSize(aW, aH), blackCtx);
            whiteCtx->DestroyDrawingSurface(whiteSurface);
        }
    }
    
    blackCtx->DestroyDrawingSurface(blackSurface);
#endif

    return rv;
}

/**
 * Given aBits, the number of bits in a color channel, compute a number N
 * such that for values v with aBits bits, floor((N*v)/256) is close to
 * v*255.0/(2^aBits - 1) and in particular we need
 * floor((N*(2^aBits - 1))/256) = 255.
 * We'll just use a table that gives good results :-).
 */
static PRUint32 ComputeScaleFactor(PRUint32 aBits)
{
  static PRUint32 table[9] = {
    0, 255*256, 85*256, 9330, 17*256, 2110, 1038, 515, 256
  };
  
  NS_ASSERTION(aBits <= 8, "more than 8 bits in a color channel not supported");
  NS_ASSERTION(((table[aBits]*((1 << aBits) - 1)) >> 8) == 255,
               "Invalid table entry");
  return table[aBits];
}

nsresult
nsCanvasRenderingContext2D::DrawNativeSurfaces(nsIDrawingSurface* aBlackSurface,
                                               nsIDrawingSurface* aWhiteSurface,
                                               const nsIntSize& aSurfaceSize,
                                               nsIRenderingContext* aBlackContext)
{
    // check if the dimensions are too large;
    // if they are, we may easily overflow malloc later on
    if (!CheckSaneImageSize (aSurfaceSize.width, aSurfaceSize.height))
        return NS_ERROR_FAILURE;

    // Acquire alpha values
    nsAutoArrayPtr<PRUint8> alphas;
    nsresult rv;
    if (aWhiteSurface) {
        // There is transparency. Use the blender to recover alphas.
        nsCOMPtr<nsIBlender> blender = do_CreateInstance(kBlenderCID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsCOMPtr<nsIDeviceContext> dc;
        aBlackContext->GetDeviceContext(*getter_AddRefs(dc));
        rv = blender->Init(dc);
        NS_ENSURE_SUCCESS(rv, rv);
        
        rv = blender->GetAlphas(nsRect(0, 0, aSurfaceSize.width, aSurfaceSize.height),
                                aBlackSurface, aWhiteSurface, getter_Transfers(alphas));
        NS_ENSURE_SUCCESS(rv, rv);
    }

    // We use aBlackSurface to get the image color data
    PRUint8* data;
    PRInt32 rowLen, rowSpan;
    rv = aBlackSurface->Lock(0, 0, aSurfaceSize.width, aSurfaceSize.height,
                             (void**)&data, &rowSpan, &rowLen,
                             NS_LOCK_SURFACE_READ_ONLY);
    if (NS_FAILED(rv))
        return rv;

    // Get info about native surface layout
    PRUint32 bytesPerPix = rowLen/aSurfaceSize.width;
    nsPixelFormat format;
    
#ifndef XP_MACOSX
    rv = aBlackSurface->GetPixelFormat(&format);
    if (NS_FAILED(rv)) {
        aBlackSurface->Unlock();
        return rv;
    }
#else
    // On the mac, GetPixelFormat returns NS_ERROR_NOT_IMPLEMENTED;
    // we fake the pixel format here.  The data that we care about
    // will be in ABGR format, either 8-8-8 or 5-5-5.

    if (bytesPerPix == 4) {
        format.mRedZeroMask   = 0xff;
        format.mGreenZeroMask = 0xff;
        format.mBlueZeroMask  = 0xff;
        format.mAlphaZeroMask = 0;
        
        format.mRedMask   = 0x00ff0000;
        format.mGreenMask = 0x0000ff00;
        format.mBlueMask  = 0x000000ff;
        format.mAlphaMask = 0;
        
        format.mRedCount   = 8;
        format.mGreenCount = 8;
        format.mBlueCount  = 8;
        format.mAlphaCount = 0;
        
        format.mRedShift   = 16;
        format.mGreenShift = 8;
        format.mBlueShift  = 0;
        format.mAlphaShift = 0;
    } else if (bytesPerPix == 2) {
        format.mRedZeroMask   = 0x1f;
        format.mGreenZeroMask = 0x1f;
        format.mBlueZeroMask  = 0x1f;
        format.mAlphaZeroMask = 0;
        
        format.mRedMask   = 0x7C00;
        format.mGreenMask = 0x03E0;
        format.mBlueMask  = 0x001F;
        format.mAlphaMask = 0;
        
        format.mRedCount   = 5;
        format.mGreenCount = 5;
        format.mBlueCount  = 5;
        format.mAlphaCount = 0;
        
        format.mRedShift   = 10;
        format.mGreenShift = 5;
        format.mBlueShift  = 0;
        format.mAlphaShift = 0;
    } else {
        // no clue!
        aBlackSurface->Unlock();
        return NS_ERROR_FAILURE;
    }
    
#endif

    // Create a temporary surface to hold the full-size image in cairo
    // image format.
    nsAutoArrayPtr<PRUint8> tmpBuf(new PRUint8[aSurfaceSize.width*aSurfaceSize.height*4]);
    if (!tmpBuf) {
        aBlackSurface->Unlock();
        return NS_ERROR_OUT_OF_MEMORY;
    }

    cairo_surface_t *tmpSurf =
        cairo_image_surface_create_for_data(tmpBuf.get(),
                                            CAIRO_FORMAT_ARGB32, aSurfaceSize.width, aSurfaceSize.height,
                                            aSurfaceSize.width*4);
    if (!tmpSurf) {
        aBlackSurface->Unlock();
        return NS_ERROR_OUT_OF_MEMORY;
    }

#ifdef IS_BIG_ENDIAN
#define BLUE_BYTE 3
#define GREEN_BYTE 2
#define RED_BYTE 1
#define ALPHA_BYTE 0
#else
#define BLUE_BYTE 0
#define GREEN_BYTE 1
#define RED_BYTE 2
#define ALPHA_BYTE 3
#endif

// Mac surfaces are big endian.
#if defined(IS_BIG_ENDIAN) || defined(XP_MACOSX)
#define NATIVE_SURFACE_IS_BIG_ENDIAN
#endif

// OS/2 needs this painted the other way around
#ifdef XP_OS2
#define NATIVE_SURFACE_IS_VERTICALLY_FLIPPED
#endif

    // Convert the data
    PRUint8* dest = tmpBuf;
    PRInt32 index = 0;
    
    PRUint32 RScale = ComputeScaleFactor(format.mRedCount);
    PRUint32 GScale = ComputeScaleFactor(format.mGreenCount);
    PRUint32 BScale = ComputeScaleFactor(format.mBlueCount);
    
    for (PRInt32 i = 0; i < aSurfaceSize.height; ++i) {
#ifdef NATIVE_SURFACE_IS_VERTICALLY_FLIPPED
        PRUint8* src = data + (aSurfaceSize.height-1 - i)*rowSpan;
#else
        PRUint8* src = data + i*rowSpan;
#endif
        for (PRInt32 j = 0; j < aSurfaceSize.width; ++j) {
            /* v is the pixel value */
#ifdef NATIVE_SURFACE_IS_BIG_ENDIAN
            PRUint32 v = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
            v >>= (32 - 8*bytesPerPix);
#else
            PRUint32 v = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
#endif
            // Note that because aBlackSurface is the image rendered
            // onto black, the channel values we get here have
            // effectively been premultipled by the alpha value.
            dest[BLUE_BYTE] = 
              (PRUint8)((((v & format.mBlueMask) >> format.mBlueShift)*BScale) >> 8);
            dest[GREEN_BYTE] =
              (PRUint8)((((v & format.mGreenMask) >> format.mGreenShift)*GScale) >> 8);
            dest[RED_BYTE] =
              (PRUint8)((((v & format.mRedMask) >> format.mRedShift)*RScale) >> 8);
            dest[ALPHA_BYTE] = alphas ? alphas[index++] : 0xFF;
            src += bytesPerPix;
            dest += 4;
        }
    }

#undef RED_BYTE
#undef GREEN_BYTE
#undef BLUE_BYTE
#undef ALPHA_BYTE

    cairo_set_source_surface(mCairo, tmpSurf, 0, 0);
    cairo_paint_with_alpha(mCairo, CurrentState().globalAlpha);
    
    cairo_surface_destroy(tmpSurf);
    aBlackSurface->Unlock();
    return Redraw();
}

//
// device pixel getting/setting
//

// ImageData getImageData (in float x, in float y, in float width, in float height);
NS_IMETHODIMP
nsCanvasRenderingContext2D::GetImageData()
{
    if (mCanvasElement->IsWriteOnly() && !nsContentUtils::IsCallerTrustedForRead()) {
      // not permitted to use DrawWindow
      // XXX ERRMSG we need to report an error to developers here! (bug 329026)
      return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsCOMPtr<nsIXPCNativeCallContext> ncc;
    nsresult rv = nsContentUtils::XPConnect()->
        GetCurrentNativeCallContext(getter_AddRefs(ncc));
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

    int32 x, y, w, h;
    if (!JS_ConvertArguments (ctx, argc, argv, "jjjj", &x, &y, &w, &h))
        return NS_ERROR_DOM_SYNTAX_ERR;

    if (!CheckSaneSubrectSize (x, y, w, h, mWidth, mHeight))
        return NS_ERROR_DOM_SYNTAX_ERR;

    PRUint8 *surfaceData = mImageSurfaceData;
    nsAutoArrayPtr<PRUint8> allocatedSurfaceData;
    int surfaceDataStride = mWidth * 4;
    int surfaceDataOffset = (surfaceDataStride * y) + (x * 4);

    if (!surfaceData) {
        allocatedSurfaceData = new PRUint8[w * h * 4];
        if (!allocatedSurfaceData)
            return NS_ERROR_OUT_OF_MEMORY;
        surfaceData = allocatedSurfaceData.get();

        cairo_surface_t *tmpsurf = cairo_image_surface_create_for_data (surfaceData,
                                                                        CAIRO_FORMAT_ARGB32,
                                                                        w, h, w*4);
        cairo_t *tmpcr = cairo_create (tmpsurf);
        cairo_set_operator (tmpcr, CAIRO_OPERATOR_SOURCE);
        cairo_set_source_surface (tmpcr, mSurface, -(int)x, -(int)y);
        cairo_paint (tmpcr);
        cairo_destroy (tmpcr);
        cairo_surface_destroy (tmpsurf);

        surfaceDataStride = w * 4;
        surfaceDataOffset = 0;
    }

    PRUint32 len = w * h * 4;
    if (len > (((PRUint32)0xfff00000)/sizeof(jsval)))
        return NS_ERROR_INVALID_ARG;

    nsAutoArrayPtr<jsval> jsvector(new jsval[w * h * 4]);
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

// void putImageData (in ImageData d, in float x, in float y);
NS_IMETHODIMP
nsCanvasRenderingContext2D::PutImageData()
{
    nsresult rv;

    nsCOMPtr<nsIXPCNativeCallContext> ncc;
    rv = nsContentUtils::XPConnect()->
        GetCurrentNativeCallContext(getter_AddRefs(ncc));
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

    JSObject *dataObject;
    int32 x, y;

    if (!JS_ConvertArguments (ctx, argc, argv, "ojj", &dataObject, &x, &y))
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

    nsAutoArrayPtr<PRUint8> imageBuffer(new PRUint8[w * h * 4]);
    cairo_surface_t *imgsurf;
    PRUint8 *imgPtr = imageBuffer.get();
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

    if (mImageSurfaceData) {
        int stride = mWidth*4;
        PRUint8 *dest = mImageSurfaceData + stride*y + x*4;

        for (int32 i = 0; i < y; i++) {
            memcpy(dest, imgPtr + (w*4)*i, w*4);
            dest += stride;
        }
    } else {
        imgsurf = cairo_image_surface_create_for_data (imageBuffer.get(),
                                                       CAIRO_FORMAT_ARGB32,
                                                       w, h, w*4);
        cairo_save (mCairo);
        cairo_identity_matrix (mCairo);
        cairo_translate (mCairo, x, y);
        cairo_new_path (mCairo);
        cairo_rectangle (mCairo, 0, 0, w, h);
        cairo_set_source_surface (mCairo, imgsurf, 0, 0);
        cairo_set_operator (mCairo, CAIRO_OPERATOR_SOURCE);
        cairo_fill (mCairo);
        cairo_restore (mCairo);

        cairo_surface_destroy (imgsurf);
    }

    return Redraw();
}
