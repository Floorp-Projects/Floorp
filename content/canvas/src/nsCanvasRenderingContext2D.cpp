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

#include "cairo.h"

class nsCanvasRenderingContext2D :
    public nsIDOMCanvasRenderingContext2D,
    public nsICanvasRenderingContextInternal
{
public:
    nsCanvasRenderingContext2D();
    virtual ~nsCanvasRenderingContext2D();

    nsresult Redraw();
    nsresult UpdateImageFrame();
    void SetCairoColor(nscolor c);

    // nsICanvasRenderingContextInternal
    NS_IMETHOD Init (nsIDOMHTMLCanvasElement* aParentCanvas);
    NS_IMETHOD SetTargetImageFrame(gfxIImageFrame* aImageFrame);

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
        STYLE_MAX
    };

    PRBool StyleVariantToColor(nsIVariant* aStyle, PRInt32 aWhichStyle);
    void StyleColorToString(const nscolor& aColor, nsAString& aStr);

    void DirtyAllStyles();
    void ApplyStyle(PRInt32 aWhichStyle);

    // Member vars
    PRInt32 mWidth, mHeight;

    nsCOMPtr<nsIDOMHTMLCanvasElement> mDOMCanvasElement;
    nsCOMPtr<nsICanvasElement> mCanvasElement;

    // image bits
    nsCOMPtr<gfxIImageFrame> mImageFrame;

    PRBool mDirty;

    // our CSS parser, for colors and whatnot
    nsCOMPtr<nsICSSParser> mCSSParser;

    // yay cairo
    cairo_t *mCairo;
    cairo_surface_t *mSurface;
    char *mSurfaceData;

    // style handling
    PRInt32 mLastStyle;
    PRPackedBool mDirtyStyle[STYLE_MAX];
    nscolor mColorStyles[STYLE_MAX];
    nsCOMPtr<nsIDOMCanvasGradient> mGradientStyles[STYLE_MAX];
    nsCOMPtr<nsIDOMCanvasPattern> mPatternStyles[STYLE_MAX];

    // stolen from nsJSUtils
    static PRBool ConvertJSValToUint32(PRUint32* aProp, JSContext* aContext,
                                       jsval aValue);
    static PRBool ConvertJSValToXPCObject(nsISupports** aSupports, REFNSIID aIID,
                                          JSContext* aContext, jsval aValue);
    static PRBool ConvertJSValToDouble(double* aProp, JSContext* aContext,
                                       jsval aValue);

};

NS_IMPL_ADDREF(nsCanvasRenderingContext2D)
NS_IMPL_RELEASE(nsCanvasRenderingContext2D)

NS_INTERFACE_MAP_BEGIN(nsCanvasRenderingContext2D)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCanvasRenderingContext2D)
  NS_INTERFACE_MAP_ENTRY(nsICanvasRenderingContextInternal)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCanvasRenderingContext2D)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CanvasRenderingContext2D)
NS_INTERFACE_MAP_END

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
    : mDirty(PR_TRUE), mCairo(nsnull), mSurface(nsnull), mSurfaceData(nsnull)
{
    mColorStyles[STYLE_STROKE] = NS_RGB(0,0,0);
    mColorStyles[STYLE_FILL] = NS_RGB(255,255,255);

    mLastStyle = (PRInt32) -1;

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
    if (!mDOMCanvasElement)
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
        PRUint32 sz;
        PRUnichar* str = nsnull;

        rv = aStyle->GetAsWStringWithSize(&sz, &str);
        NS_ENSURE_SUCCESS(rv, rv);

        rv = mCSSParser->ParseColorString(nsString(str, sz), nsnull, 0, PR_TRUE, &color);
        nsMemory::Free(str);
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
            mGradientStyles[aWhichStyle] = grad;
            mDirtyStyle[aWhichStyle] = PR_TRUE;
        }

        nsCOMPtr<nsIDOMCanvasPattern> pattern (do_QueryInterface(iface));
        if (pattern) {
            mPatternStyles[aWhichStyle] = pattern;
            mGradientStyles[aWhichStyle] = nsnull;
            mDirtyStyle[aWhichStyle] = PR_TRUE;
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
    }

    if (mGradientStyles[aWhichStyle]) {
    }

    SetCairoColor(mColorStyles[aWhichStyle]);
}

nsresult
nsCanvasRenderingContext2D::Redraw()
{
    mDirty = PR_TRUE;
    nsresult rv = UpdateImageFrame();
    if (NS_FAILED(rv)) {
        NS_WARNING("Canvas UpdateImageFrame filed");
        return NS_ERROR_FAILURE;
    }

    nsIFrame *frame = GetCanvasLayoutFrame();
    if (frame) {
        nsRect r = frame->GetRect();
        r.x = r.y = 0;
        frame->Invalidate(r, PR_TRUE);
    }

    return NS_OK;
}

void
nsCanvasRenderingContext2D::SetCairoColor(nscolor c)
{
    double r = double(NS_GET_R(c) / 255.0);
    double g = double(NS_GET_G(c) / 255.0);
    double b = double(NS_GET_B(c) / 255.0);
    double a = double(NS_GET_A(c) / 255.0);

    cairo_set_rgb_color (mCairo, r, g, b);
    cairo_set_alpha (mCairo, a);
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetTargetImageFrame(gfxIImageFrame* aImageFrame)
{
    // clean up old cairo bits
    Destroy();

    aImageFrame->GetWidth(&mWidth);
    aImageFrame->GetHeight(&mHeight);

    mCairo = cairo_create();
    mSurfaceData = (char *) nsMemory::Alloc(mWidth * mHeight * 4);
    mSurface = cairo_surface_create_for_image (mSurfaceData,
                                               CAIRO_FORMAT_ARGB32,
                                               mWidth,
                                               mHeight,
                                               mWidth * 4);
    cairo_set_target_surface (mCairo, mSurface);

    mImageFrame = aImageFrame;

    return NS_OK;
}

nsresult
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

        for (PRUint32 j = 0; j < (PRUint32) mHeight; j++) {
            PRUint8 *inrow = (PRUint8*)(mSurfaceData + (mWidth * 4 * j));
#ifdef XP_WIN
            // On windows, RGB_A8 is really "BGR with Y axis flipped"
            PRUint8 *outrowrgb = rgbBits + (rgbStride * (mHeight - j - 1));
            PRUint8 *outrowalpha = alphaBits + (alphaStride * (mHeight - j - 1));
#else
            PRUint8 *outrowrgb = rgbBits + (rgbStride * j);
            PRUint8 *outrowalpha = alphaBits + (alphaStride * j);
#endif
            for (PRUint32 i = 0; i < (PRUint32) mWidth; i++) {
                PRUint8 b = *inrow++;
                PRUint8 g = *inrow++;
                PRUint8 r = *inrow++;
                PRUint8 a = *inrow++;

                *outrowalpha++ = a;

                *outrowrgb++ = r;
                *outrowrgb++ = g;
                *outrowrgb++ = b;
#ifdef XP_MACOSX
                // On the mac, RGB_A8 is really RGBX_A8
                *outrowrgb++ = 0;
#endif
            }
        }

        rv = mImageFrame->UnlockAlphaData();
        rv |= mImageFrame->UnlockImageData();
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIImage> img(do_GetInterface(mImageFrame));
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
nsCanvasRenderingContext2D::Init(nsIDOMHTMLCanvasElement* aCanvasElement)
{
    mDOMCanvasElement = aCanvasElement;
    mCanvasElement = do_QueryInterface(mDOMCanvasElement);

    // set up our css parser
    if (!mCSSParser) {
        mCSSParser = do_CreateInstance("@mozilla.org/content/css-parser;1");
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetCanvas(nsIDOMHTMLCanvasElement **canvas)
{
    NS_IF_ADDREF(*canvas = mDOMCanvasElement);
    return NS_OK;
}

//
// state
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::Save()
{
    cairo_save (mCairo);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Restore()
{
    cairo_restore (mCairo);
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
nsCanvasRenderingContext2D::SetGlobalAlpha(float globalAlpha)
{
    cairo_set_alpha (mCairo, globalAlpha);
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetGlobalAlpha(float *globalAlpha)
{
    double d = cairo_current_alpha(mCairo);
    *globalAlpha = (float) d;
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

    nsString styleStr;
    StyleColorToString(mColorStyles[STYLE_STROKE], styleStr);

    nsCOMPtr<nsIWritableVariant> var = do_CreateInstance("@mozilla.org/variant;1");
    if (!var)
        return NS_ERROR_FAILURE;

    rv = var->SetWritable(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = var->SetAsDOMString(styleStr);
    NS_ENSURE_SUCCESS(rv, rv);

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

    nsString styleStr;
    StyleColorToString(mColorStyles[STYLE_FILL], styleStr);

    nsCOMPtr<nsIWritableVariant> var = do_CreateInstance("@mozilla.org/variant;1");
    if (!var)
        return NS_ERROR_FAILURE;

    rv = var->SetWritable(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = var->SetAsDOMString(styleStr);
    NS_ENSURE_SUCCESS(rv, rv);

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
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::CreateRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1,
                                                 nsIDOMCanvasGradient **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::CreatePattern(nsIDOMHTMLImageElement *image,
                                          const nsAString& repetition,
                                          nsIDOMCanvasPattern **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
    return NS_OK;
}

//
// rects
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::ClearRect(float x, float y, float w, float h)
{
    nsIFrame *fr = GetCanvasLayoutFrame();

    if (fr) {
        if (fr->GetStyleBackground()->IsTransparent())
            cairo_set_rgb_color(mCairo, 0.0f, 0.0f, 0.0f);
        else
            SetCairoColor(fr->GetStyleBackground()->mBackgroundColor);
    } else {
        cairo_set_rgb_color(mCairo, 1.0f, 1.0f, 1.0f);
    }

    DirtyAllStyles();

    cairo_set_alpha (mCairo, 0.0);
    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);
    cairo_fill (mCairo);

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::FillRect(float x, float y, float w, float h)
{
    ApplyStyle(STYLE_FILL);

    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);
    cairo_fill (mCairo);

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::StrokeRect(float x, float y, float w, float h)
{
    ApplyStyle(STYLE_STROKE);

    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);
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
nsCanvasRenderingContext2D::Arc(float x, float y, float r, float startAngle, float endAngle, int clockwise)
{
    if (clockwise)
        cairo_arc (mCairo, x, y, r, startAngle, endAngle);
    else
        cairo_arc_negative (mCairo, x, y, r, startAngle, endAngle);
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
    double d = cairo_current_line_width(mCairo);
    *width = (float) d;
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetLineCap(const nsAString& capstyle)
{
    cairo_line_cap_t cap;

    if (capstyle.EqualsLiteral("round"))
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
    cairo_line_cap_t cap = cairo_current_line_cap(mCairo);

    if (cap == CAIRO_LINE_CAP_ROUND)
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
    cairo_line_join_t j = cairo_current_line_join(mCairo);

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
    double d = cairo_current_miter_limit(mCairo);
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

    nsCOMPtr<nsIDOMHTMLImageElement> imgElt;
    if (!ConvertJSValToXPCObject(getter_AddRefs(imgElt),
                                 NS_GET_IID(nsIDOMHTMLImageElement),
                                 ctx, argv[0]))
        return NS_ERROR_DOM_TYPE_MISMATCH_ERR;

    // before we do anything more, make sure that this image is something
    // that we can actually render
    nsCOMPtr<nsIImageLoadingContent> imageLoader = do_QueryInterface(imgElt);
    if (!imageLoader)
        return NS_ERROR_NOT_AVAILABLE;
    nsCOMPtr<imgIRequest> imgRequest;
    rv = imageLoader->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST,
                                 getter_AddRefs(imgRequest));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!imgRequest)
        return NS_ERROR_NOT_AVAILABLE;

    nsCOMPtr<imgIContainer> imgContainer;
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

    // if we're at this point, we have an image container
    // and we can actually get at the image data

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
        GET_ARG(&sw, argv[3]);
        GET_ARG(&sh, argv[4]);
        sx = sy = 0.0;
        dw = sw;
        dh = sh;
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
    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

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

    if (format == gfxIFormats::RGB || format == gfxIFormats::BGR) {
        useBGR = (format & 1);

        for (PRUint32 j = 0; j < (PRUint32) imgHeight; j++) {
            PRUint8 *inrowrgb;

            if (topToBottom) {
                inrowrgb = inPixBits + (inPixStride * j);
            } else {
                inrowrgb = inPixBits + (inPixStride * (imgHeight - j - 1));
            }

            for (PRUint32 i = 0; i < (PRUint32) imgWidth; i++) {
                // rgb
                if (useBGR) {
                    *outData++ = *inrowrgb++;
                    *outData++ = *inrowrgb++;
                    *outData++ = *inrowrgb++;
                } else {
                    PRUint8 r = *inrowrgb++;
                    PRUint8 g = *inrowrgb++;
                    PRUint8 b = *inrowrgb++;
                    *outData++ = b;
                    *outData++ = g;
                    *outData++ = r;
                }
#ifdef XP_MACOSX
                inrowrgb++;
#endif

                // alpha
                *outData++ = 255;
            }
        }
        rv = NS_OK;
    } else if (format == gfxIFormats::RGB_A1 || format == gfxIFormats::BGR_A1) {
        useBGR = (format & 1);
        for (PRUint32 j = 0; j < (PRUint32) imgHeight; j++) {
            PRUint8 *inrowrgb;
            PRUint8 *inrowalpha;

            if (topToBottom) {
                inrowrgb = inPixBits + (inPixStride * j);
                inrowalpha = inAlphaBits + (inAlphaStride * j);
            } else {
                inrowrgb = inPixBits + (inPixStride * (imgHeight - j - 1));
                inrowalpha = inAlphaBits + (inAlphaStride * (imgHeight - j - 1));
            }

            for (PRUint32 i = 0; i < (PRUint32) imgWidth; i++) {
                // rgb
                if (useBGR) {
                    *outData++ = *inrowrgb++;
                    *outData++ = *inrowrgb++;
                    *outData++ = *inrowrgb++;
                } else {
                    PRUint8 r = *inrowrgb++;
                    PRUint8 g = *inrowrgb++;
                    PRUint8 b = *inrowrgb++;
                    *outData++ = b;
                    *outData++ = g;
                    *outData++ = r;
                }
#ifdef XP_MACOSX
                inrowrgb++;
#endif

                // alpha
                PRInt32 bit = i % 8;
                PRInt32 byte = i / 8;

                PRUint8 a = (inrowalpha[byte] >> bit) & 1;
                if (a) a = 255;

                *outData++ = a;
            }
        }
        rv = NS_OK;
    } else if (format == gfxIFormats::RGB_A8 || format == gfxIFormats::RGB_A8) {
        useBGR = (format & 1);
        for (PRUint32 j = 0; j < (PRUint32) imgHeight; j++) {
            PRUint8 *inrowrgb;
            PRUint8 *inrowalpha;

            if (topToBottom) {
                inrowrgb = inPixBits + (inPixStride * j);
                inrowalpha = inAlphaBits + (inAlphaStride * j);
            } else {
                inrowrgb = inPixBits + (inPixStride * (imgHeight - j - 1));
                inrowalpha = inAlphaBits + (inAlphaStride * (imgHeight - j - 1));
            }

            for (PRUint32 i = 0; i < (PRUint32) imgWidth; i++) {
                // rgb
                if (useBGR) {
                    *outData++ = *inrowrgb++;
                    *outData++ = *inrowrgb++;
                    *outData++ = *inrowrgb++;
                } else {
                    PRUint8 r = *inrowrgb++;
                    PRUint8 g = *inrowrgb++;
                    PRUint8 b = *inrowrgb++;
                    *outData++ = b;
                    *outData++ = g;
                    *outData++ = r;
                }
#ifdef XP_MACOSX
                inrowrgb++;
#endif

                // alpha
                *outData++ = *inrowalpha++;
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
        cairo_image_surface_create_for_data((char *)cairoImgData, CAIRO_FORMAT_ARGB32,
                                            imgWidth, imgHeight, imgWidth*4);

    cairo_matrix_t *surfMat = cairo_matrix_create();

    cairo_matrix_translate(surfMat, -sx, -sy);
    cairo_matrix_scale(surfMat, sw/dw, sh/dh);
    cairo_surface_set_matrix(imgSurf, surfMat);
    cairo_matrix_destroy(surfMat);

    cairo_save(mCairo);
    cairo_translate(mCairo, dx, dy);
    cairo_show_surface(mCairo, imgSurf, (int) dw, (int) dh);
    cairo_restore(mCairo);

    nsMemory::Free(cairoImgData);
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

    CANVAS_OP_TO_CAIRO_OP("clear", CLEAR)
    else CANVAS_OP_TO_CAIRO_OP("copy", SRC)
    else CANVAS_OP_TO_CAIRO_OP("darker", SATURATE)  // XXX
    else CANVAS_OP_TO_CAIRO_OP("destination-atop", ATOP_REVERSE)
    else CANVAS_OP_TO_CAIRO_OP("destination-in", IN_REVERSE)
    else CANVAS_OP_TO_CAIRO_OP("destination-out", OUT_REVERSE)
    else CANVAS_OP_TO_CAIRO_OP("destination-over", OVER_REVERSE)
    else CANVAS_OP_TO_CAIRO_OP("lighter", SATURATE)
    else CANVAS_OP_TO_CAIRO_OP("source-atop", ATOP)
    else CANVAS_OP_TO_CAIRO_OP("source-in", IN)
    else CANVAS_OP_TO_CAIRO_OP("source-out", OUT)
    else CANVAS_OP_TO_CAIRO_OP("source-over", OVER)
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
    cairo_operator_t cairo_op = cairo_current_operator(mCairo);

#define CANVAS_OP_TO_CAIRO_OP(cvsop,cairoop) \
    if (cairo_op == CAIRO_OPERATOR_##cairoop) \
        op.AssignLiteral(cvsop);

    CANVAS_OP_TO_CAIRO_OP("clear", CLEAR)
    else CANVAS_OP_TO_CAIRO_OP("copy", SRC)
    else CANVAS_OP_TO_CAIRO_OP("darker", SATURATE)  // XXX
    else CANVAS_OP_TO_CAIRO_OP("destination-atop", ATOP_REVERSE)
    else CANVAS_OP_TO_CAIRO_OP("destination-in", IN_REVERSE)
    else CANVAS_OP_TO_CAIRO_OP("destination-out", OUT_REVERSE)
    else CANVAS_OP_TO_CAIRO_OP("destination-over", OVER_REVERSE)
    else CANVAS_OP_TO_CAIRO_OP("lighter", SATURATE)
    else CANVAS_OP_TO_CAIRO_OP("source-atop", ATOP)
    else CANVAS_OP_TO_CAIRO_OP("source-in", IN)
    else CANVAS_OP_TO_CAIRO_OP("source-out", OUT)
    else CANVAS_OP_TO_CAIRO_OP("source-over", OVER)
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
