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

#include "nsIServiceManager.h"

#include "nsContentUtils.h"

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

#include "nsICSSParser.h"

#include "nsPrintfCString.h"

#include "nsReadableUtils.h"

#include "nsColor.h"
#include "nsIRenderingContext.h"
#include "nsIBlender.h"
#include "nsGfxCIID.h"
#include "nsIDrawingSurface.h"
#include "nsIScriptSecurityManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsPresContext.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIDOMWindow.h"

#include "cairo.h"
#include "imgIEncoder.h"

static NS_DEFINE_IID(kBlenderCID, NS_BLENDER_CID);

/**
 ** nsCanvasGradient
 **/
#define NS_CANVASGRADIENT_PRIVATE_IID \
    { 0x491d39d8, 0x4058, 0x42bd, { 0xac, 0x76, 0x70, 0xd5, 0x62, 0x7f, 0x02, 0x10 } }
class nsCanvasGradient : public nsIDOMCanvasGradient
{
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_CANVASGRADIENT_PRIVATE_IID)

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

        nsresult rv = mCSSParser->ParseColorString(nsString(colorstr), nsnull, 0, PR_TRUE, &color);
        if (NS_FAILED(rv))
            return PR_FALSE;

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
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_CANVASPATTERN_PRIVATE_IID)

    nsCanvasPattern(cairo_pattern_t *cpat, PRUint8 *dataToFree)
        : mPattern(cpat), mData(dataToFree)
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

    NS_DECL_ISUPPORTS

protected:
    cairo_pattern_t *mPattern;
    PRUint8 *mData;
};

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
    NS_IMETHOD SetTargetImageFrame(gfxIImageFrame* aImageFrame);
    NS_IMETHOD GetInputStream(const nsACString& aMimeType,
                              const nsAString& aEncoderOptions,
                              nsIInputStream **aStream);
    NS_IMETHOD UpdateImageFrame();

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
        STYLE_SHADOW,
        STYLE_MAX
    };

    PRBool StyleVariantToColor(nsIVariant* aStyle, PRInt32 aWhichStyle);
    void StyleColorToString(const nscolor& aColor, nsAString& aStr);

    void DirtyAllStyles();
    void ApplyStyle(PRInt32 aWhichStyle);

    // Member vars
    PRInt32 mWidth, mHeight;

    // the canvas element informs us when its going away,
    // so these are not nsCOMPtrs
    nsICanvasElement* mCanvasElement;

    // image bits
    nsCOMPtr<gfxIImageFrame> mImageFrame;

    PRBool mDirty;

    // our CSS parser, for colors and whatnot
    nsCOMPtr<nsICSSParser> mCSSParser;

    // yay cairo
    PRUint32 mSaveCount;
    cairo_t *mCairo;
    cairo_surface_t *mSurface;
    unsigned char *mSurfaceData;
    float mGlobalAlpha;

    // style handling
    PRInt32 mLastStyle;
    PRPackedBool mDirtyStyle[STYLE_MAX];
    nscolor mColorStyles[STYLE_MAX];
    nsCOMPtr<nsCanvasGradient> mGradientStyles[STYLE_MAX];
    nsCOMPtr<nsCanvasPattern> mPatternStyles[STYLE_MAX];

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
                                     PRInt32 *widthOut, PRInt32 *heightOut);

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
      mDirty(PR_TRUE), mSaveCount(0), mCairo(nsnull), mSurface(nsnull), mSurfaceData(nsnull)
{
    mColorStyles[STYLE_STROKE] = NS_RGB(0,0,0);
    mColorStyles[STYLE_FILL] = NS_RGB(0,0,0);
    mColorStyles[STYLE_SHADOW] = NS_RGBA(0,0,0,0);

    mLastStyle = (PRInt32) -1;
    mGlobalAlpha = 1.0f;

    DirtyAllStyles();
}

nsCanvasRenderingContext2D::~nsCanvasRenderingContext2D()
{
    Destroy();

    mImageFrame = nsnull;
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

    if (mSurfaceData) {
        nsMemory::Free(mSurfaceData);
        mSurfaceData = nsnull;
    }
}

PRBool
nsCanvasRenderingContext2D::StyleVariantToColor(nsIVariant* aStyle, PRInt32 aWhichStyle)
{
    nsresult rv;
    nscolor color;

    PRUint16 paramType;
    rv = aStyle->GetDataType(&paramType);
    NS_ENSURE_SUCCESS(rv, rv);

    if (paramType == nsIDataType::VTYPE_DOMSTRING) {
        nsString str;
        rv = aStyle->GetAsDOMString(str);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mCSSParser->ParseColorString(str, nsnull, 0, PR_TRUE, &color);
        if (NS_FAILED(rv))
            return PR_FALSE;

        mColorStyles[aWhichStyle] = color;
        mPatternStyles[aWhichStyle] = nsnull;
        mGradientStyles[aWhichStyle] = nsnull;

        mDirtyStyle[aWhichStyle] = PR_TRUE;

        return PR_TRUE;
    } else if (paramType == nsIDataType::VTYPE_WSTRING_SIZE_IS) {
        nsAutoString str;

        rv = aStyle->GetAsAString(str);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mCSSParser->ParseColorString(str, nsnull, 0, PR_TRUE, &color);
        if (NS_FAILED(rv))
            return PR_FALSE;

        mColorStyles[aWhichStyle] = color;
        mPatternStyles[aWhichStyle] = nsnull;
        mGradientStyles[aWhichStyle] = nsnull;

        mDirtyStyle[aWhichStyle] = PR_TRUE;
        return PR_TRUE;
    } else if (paramType == nsIDataType::VTYPE_INTERFACE ||
               paramType == nsIDataType::VTYPE_INTERFACE_IS)
    {
        nsID *iid;
        nsCOMPtr<nsISupports> iface;
        rv = aStyle->GetAsInterface(&iid, getter_AddRefs(iface));

        nsCOMPtr<nsIDOMCanvasGradient> grad (do_QueryInterface(iface));
        if (grad) {
            mPatternStyles[aWhichStyle] = nsnull;
            mGradientStyles[aWhichStyle] = do_QueryInterface(iface);
            mDirtyStyle[aWhichStyle] = PR_TRUE;

            if (!mGradientStyles[aWhichStyle])
                return PR_FALSE;

            return PR_TRUE;
        }

        nsCOMPtr<nsIDOMCanvasPattern> pattern (do_QueryInterface(iface));
        if (pattern) {
            mPatternStyles[aWhichStyle] = do_QueryInterface(iface);
            mGradientStyles[aWhichStyle] = nsnull;
            mDirtyStyle[aWhichStyle] = PR_TRUE;

            if (!mPatternStyles[aWhichStyle])
                return PR_FALSE;

            return PR_TRUE;
        }
    }

    return PR_FALSE;
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
        CopyUTF8toUTF16(nsPrintfCString(100, "rgb(%d,%d,%d,%0.2f)",
                                        NS_GET_R(aColor),
                                        NS_GET_G(aColor),
                                        NS_GET_B(aColor),
                                        NS_GET_A(aColor) / 255.0f),
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

    if (mPatternStyles[aWhichStyle]) {
        mPatternStyles[aWhichStyle]->Apply(mCairo);
        return;
    }

    if (mGradientStyles[aWhichStyle]) {
        mGradientStyles[aWhichStyle]->Apply(mCairo);
        return;
    }

    SetCairoColor(mColorStyles[aWhichStyle]);
}

nsresult
nsCanvasRenderingContext2D::Redraw()
{
    mDirty = PR_TRUE;

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
    double a = double(NS_GET_A(c) / 255.0) * mGlobalAlpha;

    cairo_set_source_rgba (mCairo, r, g, b, a);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetTargetImageFrame(gfxIImageFrame* aImageFrame)
{
    // clean up old cairo bits
    Destroy();

    aImageFrame->GetWidth(&mWidth);
    aImageFrame->GetHeight(&mHeight);

    mSurfaceData = (unsigned char *) nsMemory::Alloc(mWidth * mHeight * 4);
    mSurface = cairo_image_surface_create_for_data (mSurfaceData,
                                                    CAIRO_FORMAT_ARGB32,
                                                    mWidth,
                                                    mHeight,
                                                    mWidth * 4);
    mCairo = cairo_create(mSurface);

    // set up the initial canvas defaults
    cairo_set_line_width (mCairo, 1.0);
    cairo_set_operator (mCairo, CAIRO_OPERATOR_OVER);
    cairo_set_miter_limit(mCairo, 10.0);
    cairo_set_line_cap(mCairo, CAIRO_LINE_CAP_BUTT);
    cairo_set_line_join(mCairo, CAIRO_LINE_JOIN_MITER);

    mImageFrame = aImageFrame;

    return ClearRect (0, 0, mWidth, mHeight);
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

    encoder->InitFromData(mSurfaceData,
                          mWidth * mHeight * 4, mWidth, mHeight, mWidth * 4,
                          imgIEncoder::INPUT_FORMAT_HOSTARGB,
                          aEncoderOptions);

    return CallQueryInterface(encoder, aStream);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::UpdateImageFrame()
{
    nsresult rv;

    if (!mImageFrame)
        return NS_OK;

    if (!mSurfaceData)
        return NS_ERROR_FAILURE;

    if (mDirty) {
        PRUint8 *alphaBits, *rgbBits;
        PRUint32 alphaLen, rgbLen;
        PRUint32 alphaStride, rgbStride;

        rv = mImageFrame->LockImageData();
        if (NS_FAILED(rv)) {
            return rv;
        }

        rv = mImageFrame->LockAlphaData();
        if (NS_FAILED(rv)) {
            mImageFrame->UnlockImageData();
            return rv;
        }

        rv = mImageFrame->GetAlphaBytesPerRow(&alphaStride);
        rv |= mImageFrame->GetAlphaData(&alphaBits, &alphaLen);
        rv |= mImageFrame->GetImageBytesPerRow(&rgbStride);
        rv |= mImageFrame->GetImageData(&rgbBits, &rgbLen);
        if (NS_FAILED(rv)) {
            mImageFrame->UnlockImageData();
            mImageFrame->UnlockAlphaData();
            return rv;
        }

        nsCOMPtr<nsIImage> img(do_GetInterface(mImageFrame));
        PRBool topToBottom = img->GetIsRowOrderTopToBottom();

        for (PRUint32 j = 0; j < (PRUint32) mHeight; j++) {
            PRUint8 *inrow = (PRUint8*)(mSurfaceData + (mWidth * 4 * j));

            PRUint32 rowIndex;
            if (topToBottom)
                rowIndex = j;
            else
                rowIndex = mHeight - j - 1;

            PRUint8 *outrowrgb = rgbBits + (rgbStride * rowIndex);
            PRUint8 *outrowalpha = alphaBits + (alphaStride * rowIndex);

            for (PRUint32 i = 0; i < (PRUint32) mWidth; i++) {
#ifdef IS_LITTLE_ENDIAN
                PRUint8 b = *inrow++;
                PRUint8 g = *inrow++;
                PRUint8 r = *inrow++;
                PRUint8 a = *inrow++;
#else
                PRUint8 a = *inrow++;
                PRUint8 r = *inrow++;
                PRUint8 g = *inrow++;
                PRUint8 b = *inrow++;
#endif
                // now recover the real bgra from the cairo
                // premultiplied values
                if (a == 0) {
                    // can't do much for us if we're at 0
                    b = g = r = 0;
                } else {
                    // the (a/2) factor is a bias similar to one cairo applies
                    // when premultiplying
                    b = (b * 255 + a / 2) / a;
                    g = (g * 255 + a / 2) / a;
                    r = (r * 255 + a / 2) / a;
                }

                *outrowalpha++ = a;

#ifdef XP_MACOSX
                // On the mac, RGB_A8 is really RGBX_A8
                *outrowrgb++ = 0;
#endif

#ifdef XP_WIN
                // On windows, RGB_A8 is really BGR_A8.
                // in fact, BGR_A8 is also BGR_A8.
                *outrowrgb++ = b;
                *outrowrgb++ = g;
                *outrowrgb++ = r;
#else
                *outrowrgb++ = r;
                *outrowrgb++ = g;
                *outrowrgb++ = b;
#endif
            }
        }

        rv = mImageFrame->UnlockAlphaData();
        rv |= mImageFrame->UnlockImageData();
        if (NS_FAILED(rv))
            return rv;

        nsRect r(0, 0, mWidth, mHeight);
        img->ImageUpdated(nsnull, nsImageUpdateFlags_kBitsChanged, &r);

        mDirty = PR_FALSE;
    }

    return NS_OK;
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
    mSaveCount++;
    cairo_save (mCairo);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Restore()
{
    if (mSaveCount > 0) {
        mSaveCount--;
        cairo_restore (mCairo);
    }
    return NS_OK;
}

//
// transformations
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::Scale(float x, float y)
{
    cairo_scale (mCairo, x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Rotate(float angle)
{
    cairo_rotate (mCairo, angle);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Translate(float x, float y)
{
    cairo_translate (mCairo, x, y);
    return NS_OK;
}

//
// colors
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetGlobalAlpha(float aGlobalAlpha)
{
    mGlobalAlpha = aGlobalAlpha;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetGlobalAlpha(float *aGlobalAlpha)
{
    *aGlobalAlpha = mGlobalAlpha;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetStrokeStyle(nsIVariant* aStyle)
{
    if (StyleVariantToColor(aStyle, STYLE_STROKE))
        return NS_OK;

    return NS_ERROR_INVALID_ARG;
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

    if (mPatternStyles[STYLE_STROKE]) {
        rv = var->SetAsISupports(mPatternStyles[STYLE_STROKE]);
        NS_ENSURE_SUCCESS(rv, rv);
    } else if (mGradientStyles[STYLE_STROKE]) {
        rv = var->SetAsISupports(mGradientStyles[STYLE_STROKE]);
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        nsString styleStr;
        StyleColorToString(mColorStyles[STYLE_STROKE], styleStr);

        rv = var->SetAsDOMString(styleStr);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    NS_ADDREF(*aStyle = var);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetFillStyle(nsIVariant* aStyle)
{
    if (StyleVariantToColor(aStyle, STYLE_FILL))
        return NS_OK;

    return NS_ERROR_INVALID_ARG;
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

    if (mPatternStyles[STYLE_FILL]) {
        rv = var->SetAsISupports(mPatternStyles[STYLE_FILL]);
        NS_ENSURE_SUCCESS(rv, rv);
    } else if (mGradientStyles[STYLE_FILL]) {
        rv = var->SetAsISupports(mGradientStyles[STYLE_FILL]);
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        nsString styleStr;
        StyleColorToString(mColorStyles[STYLE_FILL], styleStr);

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
nsCanvasRenderingContext2D::CreatePattern(nsIDOMHTMLImageElement *image,
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
        return NS_ERROR_DOM_SYNTAX_ERR;
    }

    cairo_surface_t *imgSurf = nsnull;
    PRUint8 *imgData = nsnull;
    PRInt32 imgWidth, imgHeight;
    rv = CairoSurfaceFromElement(image, &imgSurf, &imgData, &imgWidth, &imgHeight);
    if (NS_FAILED(rv))
        return rv;

    cairo_pattern_t *cairopat = cairo_pattern_create_for_surface(imgSurf);
    cairo_surface_destroy(imgSurf);

    cairo_pattern_set_extend (cairopat, extend);

    nsCanvasPattern *pat = new nsCanvasPattern(cairopat, imgData);
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
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetShadowColor(nsAString& color)
{
    StyleColorToString(mColorStyles[STYLE_SHADOW], color);
    return NS_OK;
}

//
// rects
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::ClearRect(float x, float y, float w, float h)
{
    cairo_save (mCairo);
    cairo_set_operator (mCairo, CAIRO_OPERATOR_CLEAR);
    cairo_set_source_rgba (mCairo, 0.0, 0.0, 0.0, 0.0);
    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);
    cairo_fill (mCairo);
    cairo_restore (mCairo);

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::FillRect(float x, float y, float w, float h)
{
    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);

    ApplyStyle(STYLE_FILL);
    cairo_fill (mCairo);

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::StrokeRect(float x, float y, float w, float h)
{
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
    cairo_fill(mCairo);
    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Stroke()
{
    ApplyStyle(STYLE_STROKE);
    cairo_stroke(mCairo);
    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Clip()
{
    cairo_clip(mCairo);
    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::MoveTo(float x, float y)
{
    cairo_move_to(mCairo, x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::LineTo(float x, float y)
{
    cairo_line_to(mCairo, x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::QuadraticCurveTo(float cpx, float cpy, float x, float y)
{
    cairo_curve_to(mCairo, cpx, cpy, cpx, cpy, x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::BezierCurveTo(float cp1x, float cp1y,
                                          float cp2x, float cp2y,
                                          float x, float y)
{
    cairo_curve_to(mCairo, cp1x, cp1y, cp2x, cp2y, x, y);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::ArcTo(float x1, float y1, float x2, float y2, float radius)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Arc(float x, float y, float r, float startAngle, float endAngle, int ccw)
{
    if (ccw)
        cairo_arc_negative (mCairo, x, y, r, startAngle, endAngle);
    else
        cairo_arc (mCairo, x, y, r, startAngle, endAngle);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Rect(float x, float y, float w, float h)
{
    cairo_rectangle (mCairo, x, y, w, h);
    return NS_OK;
}


//
// line caps/joins
//
NS_IMETHODIMP
nsCanvasRenderingContext2D::SetLineWidth(float width)
{
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

    {
        nsCOMPtr<nsIDOMHTMLImageElement> image = do_QueryInterface(imgElt);
        nsCOMPtr<nsIDOMHTMLCanvasElement> canvas = do_QueryInterface(imgElt);
        if (!image && !canvas)
            return NS_ERROR_DOM_TYPE_MISMATCH_ERR;
    }

    cairo_surface_t *imgSurf = nsnull;
    PRUint8 *imgData = nsnull;
    PRInt32 imgWidth, imgHeight;
    rv = CairoSurfaceFromElement(imgElt, &imgSurf, &imgData, &imgWidth, &imgHeight);
    if (NS_FAILED(rv))
        return rv;

#define GET_ARG(dest,whicharg) \
    if (!ConvertJSValToDouble(dest, ctx, whicharg)) return NS_ERROR_INVALID_ARG

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
        return NS_ERROR_INVALID_ARG;
    }
#undef GET_ARG

    // check args
    if (sx < 0.0 || sy < 0.0 ||
        sw < 0.0 || sw > (double) imgWidth ||
        sh < 0.0 || sh > (double) imgHeight ||
        dw < 0.0 || dh < 0.0)
    {
        return NS_ERROR_DOM_INDEX_SIZE_ERR;
    }

    cairo_matrix_t surfMat;
    cairo_matrix_init_translate(&surfMat, sx, sy);
    cairo_matrix_scale(&surfMat, sw/dw, sh/dh);
    cairo_pattern_t* pat = cairo_pattern_create_for_surface(imgSurf);
    cairo_pattern_set_matrix(pat, &surfMat);

    cairo_save(mCairo);
    cairo_translate(mCairo, dx, dy);
    cairo_rectangle(mCairo, 0, 0, dw, dh);
    cairo_set_source(mCairo, pat);
    cairo_clip(mCairo);
    cairo_paint_with_alpha(mCairo, mGlobalAlpha);
    cairo_restore(mCairo);

    cairo_pattern_destroy(pat);

    nsMemory::Free(imgData);
    cairo_surface_destroy(imgSurf);

    return Redraw();
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
    else CANVAS_OP_TO_CAIRO_OP("source-over", SOURCE)
    else CANVAS_OP_TO_CAIRO_OP("xor", XOR)
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
    else CANVAS_OP_TO_CAIRO_OP("source-over", SOURCE)
    else CANVAS_OP_TO_CAIRO_OP("xor", XOR)
    else CANVAS_OP_TO_CAIRO_OP("over", OVER)
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
                                                    PRInt32 *widthOut, PRInt32 *heightOut)
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
            return NS_ERROR_NOT_AVAILABLE;
        
        rv = imgRequest->GetImage(getter_AddRefs(imgContainer));
        NS_ENSURE_SUCCESS(rv, rv);
    } else {
        // maybe a canvas
        nsCOMPtr<nsICanvasElement> canvas = do_QueryInterface(imgElt);
        if (canvas) {
            // Ensure the canvas is up to date
            canvas->UpdateImageFrame();
            canvas->GetCanvasImageContainer(getter_AddRefs(imgContainer));
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
                // pull out alpha to premultiply
                PRInt32 bit = i % 8;
                PRInt32 byte = i / 8;

                PRUint8 a = (inrowalpha[byte] >> bit) & 1;

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

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::DrawWindow(nsIDOMWindow* aWindow, PRInt32 aX, PRInt32 aY,
                                       PRInt32 aW, PRInt32 aH, 
                                       const nsAString& aBGColor)
{
    NS_ENSURE_ARG(aWindow != nsnull);

    // We can't allow web apps to call this until we fix at least the
    // following potential security issues:
    // -- rendering cross-domain IFRAMEs and then extracting the results
    // -- rendering the user's theme and then extracting the results
    // -- rendering native anonymous content (e.g., file input paths;
    // scrollbars should be allowed)
    nsCOMPtr<nsIScriptSecurityManager> ssm =
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID);
    if (!ssm)
        return NS_ERROR_FAILURE;

    PRBool isTrusted = PR_FALSE;
    PRBool isChrome = PR_FALSE;
    PRBool hasCap = PR_FALSE;

    // The secman really should handle UniversalXPConnect case, since that
    // should include UniversalBrowserRead... doesn't right now, though.
    if ((NS_SUCCEEDED(ssm->SubjectPrincipalIsSystem(&isChrome)) && isChrome) ||
        (NS_SUCCEEDED(ssm->IsCapabilityEnabled("UniversalBrowserRead", &hasCap)) && hasCap) ||
        (NS_SUCCEEDED(ssm->IsCapabilityEnabled("UniversalXPConnect", &hasCap)) && hasCap))
    {
        isTrusted = PR_TRUE;
    }

    if (!isTrusted) {
        // not permitted to use DrawWindow
        return NS_ERROR_DOM_SECURITY_ERR;
    }

    nsCOMPtr<nsPresContext> presContext;
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
    if (sgo) {
        nsIDocShell* docshell = sgo->GetDocShell();
        if (docshell) {
            docshell->GetPresContext(getter_AddRefs(presContext));
        }
    }
    if (!presContext)
        return NS_ERROR_FAILURE;

    // Dig down past the viewport scroll stuff
    nsIViewManager* vm = presContext->GetViewManager();
    nsIView* view;
    vm->GetRootView(view);
    NS_ASSERTION(view, "Must have root view!");

    nscolor bgColor;
    nsresult rv = mCSSParser->ParseColorString(PromiseFlatString(aBGColor),
                                               nsnull, 0, PR_TRUE, &bgColor);
    NS_ENSURE_SUCCESS(rv, rv);
    
    float p2t = presContext->PixelsToTwips();
    nsRect r(aX, aY, aW, aH);
    r.ScaleRoundOut(p2t);

    nsCOMPtr<nsIRenderingContext> blackCtx;
    rv = vm->RenderOffscreen(view, r, PR_FALSE, PR_TRUE,
                             NS_ComposeColors(NS_RGB(0, 0, 0), bgColor),
                             getter_AddRefs(blackCtx));
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
    rv = vm->RenderOffscreen(view, r, PR_FALSE, PR_TRUE,
                             NS_ComposeColors(NS_RGB(255, 255, 255), bgColor),
                             getter_AddRefs(whiteCtx));
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
    return rv;
}

nsresult
nsCanvasRenderingContext2D::DrawNativeSurfaces(nsIDrawingSurface* aBlackSurface,
                                               nsIDrawingSurface* aWhiteSurface,
                                               const nsIntSize& aSurfaceSize,
                                               nsIRenderingContext* aBlackContext)
{
    if (!mImageFrame) {
        NS_ERROR("Must have image frame already");
        return NS_ERROR_FAILURE;
    }
    
    // Acquire alpha values
    nsAutoArrayPtr<PRUint8> alphas;
    nsresult rv;
    if (aWhiteSurface) {
        // There is transparency. Use the blender to recover alphas.
        nsCOMPtr<nsIBlender> blender = do_CreateInstance(kBlenderCID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        nsIDeviceContext* dc = nsnull;
        aBlackContext->GetDeviceContext(dc);
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

    // Convert the data
    PRUint8* dest = tmpBuf;
    PRInt32 index = 0;
    for (PRInt32 i = 0; i < aSurfaceSize.height; ++i) {
        PRUint8* src = data + i*rowSpan;
        for (PRInt32 j = 0; j < aSurfaceSize.width; ++j) {
            /* v is the pixel value */
#ifdef IS_BIG_ENDIAN
            PRUint32 v = (src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3];
            v >>= (32 - 8*bytesPerPix);
#else
            PRUint32 v = src[0] | (src[1] << 8) | (src[2] << 16) | (src[3] << 24);
#endif
            // Note that because aBlackSurface is the image rendered
            // onto black, the channel values we get here have
            // effectively been premultipled by the alpha value.
            dest[BLUE_BYTE] = (PRUint8)(((v & format.mBlueMask) >> format.mBlueShift)
                                << (8 - format.mBlueCount));
            dest[GREEN_BYTE] = (PRUint8)(((v & format.mGreenMask) >> format.mGreenShift)
                                << (8 - format.mGreenCount));
            dest[RED_BYTE] = (PRUint8)(((v & format.mRedMask) >> format.mRedShift)
                                << (8 - format.mRedCount));
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
    cairo_paint_with_alpha(mCairo, mGlobalAlpha);
    
    cairo_surface_destroy(tmpSurf);
    aBlackSurface->Unlock();
    return Redraw();
}
