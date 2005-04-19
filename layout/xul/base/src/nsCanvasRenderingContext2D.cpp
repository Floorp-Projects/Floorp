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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "nsICanvasRenderingContext2D.h"
#include "nsICanvasRenderingContext.h"

#include "nsCanvasFrame.h"
#include "nsBoxLayoutState.h"

#include "imgIRequest.h"
#include "imgIContainer.h"
#include "gfxIImageFrame.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIImageLoadingContent.h"

#include "nsPrintfCString.h"

#include "nsReadableUtils.h"

#include <cairo.h>

class nsCanvasRenderingContext2D :
    public nsICanvasRenderingContext2D,
    public nsICanvasRenderingContext
{
public:
    nsCanvasRenderingContext2D();
    virtual ~nsCanvasRenderingContext2D();

    nsresult Redraw();
    void SetCairoColor(nscolor c);

    // nsICanvasRenderingContext
    virtual nsresult Init(nsCanvasFrame* aCanvasFrame, nsPresContext* aPresContext);
    virtual nsresult Paint(nsPresContext*       aPresContext,
                           nsIRenderingContext& aRenderingContext,
                           const nsRect&        aDirtyRect,
                           nsFramePaintLayer    aWhichLayer,
                           PRUint32             aFlags);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsICanvasRenderingContext2D interface
    NS_DECL_NSICANVASRENDERINGCONTEXT2D
protected:
    PRUint32 mWidth, mHeight;
    nsPresContext* mPresContext;
    nsCanvasFrame* mCanvasFrame;
    float mPixelsToTwips;

    nscolor mStrokeColor;
    nscolor mFillColor;

    // image bits
    nsCOMPtr<imgIContainer> mImageContainer;
    nsCOMPtr<gfxIImageFrame> mImageFrame;
    PRUint8 *mImageData;
    PRUint32 mImageDataStride;
    PRUint8 *mAlphaData;
    PRUint32 mAlphaDataStride;

    PRBool mDirty;

    // yay cairo
    cairo_t *mCairo;
    cairo_surface_t *mSurface;
    char *mSurfaceData;
};

// creation
nsresult
NS_NewCanvasRenderingContext2D(nsICanvasRenderingContext2D** aResult)
{
    *aResult = new nsCanvasRenderingContext2D;
    if (!*aResult)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ISUPPORTS2(nsCanvasRenderingContext2D, nsICanvasRenderingContext2D, nsICanvasRenderingContext)

static PRBool
ColorStringToColor (const nsAString& style, nscolor &color)
{
    nsCAutoString str;
    str.Assign(NS_ConvertUTF16toUTF8(style));

    if (str.IsEmpty())
        return PR_FALSE;

    int slen = str.Length();

    if (str[0] == '#') {
        unsigned int shift = 0;
        color = 0;
        if (slen == 4) {
            for (int i = 0; i < 3; i++) {
                if (str[i+1] >= '0' && str[i+1] <= '9') {
                    color |= ((str[i+1] - '0') * 0x10) << shift;
                } else if (tolower(str[i+1]) >= 'a' && tolower(str[i+1]) <= 'f') {
                    color |= (((tolower(str[i+1]) - 'a') + 9) * 0x10) << shift;
                } else {
                    return PR_FALSE;
                }
                shift += 8;
            }
            return PR_TRUE;
        } else if (slen == 7) {
            char *ss = nsnull;
            unsigned long l;
            l = strtoul (nsPromiseFlatCString(str).get()+1, &ss, 16);
            if (*ss != 0) {
                return PR_FALSE;
            }
            color = (nscolor) l;
            return PR_TRUE;
        } else {
            return PR_FALSE;
        }
    }

    if (StringBeginsWith(style, NS_LITERAL_STRING("rgb("))) {
        // ...
    }

    if (NS_ColorNameToRGB(style, &color))
        return PR_TRUE;

    return PR_FALSE;
}

nsCanvasRenderingContext2D::nsCanvasRenderingContext2D()
    : mPresContext(nsnull), mDirty(PR_TRUE), mCairo(nsnull), mSurface(nsnull), mSurfaceData(nsnull)
{
}

nsCanvasRenderingContext2D::~nsCanvasRenderingContext2D()
{
    nsMemory::Free(mSurfaceData);

    cairo_surface_destroy(mSurface);
    cairo_destroy(mCairo);
    mImageFrame = nsnull;
    mImageContainer = nsnull;

    nsMemory::Free(mImageData);
    nsMemory::Free(mAlphaData);
}

nsresult
nsCanvasRenderingContext2D::Redraw()
{
    mDirty = PR_TRUE;
    nsBoxLayoutState state(mPresContext);
    return mCanvasFrame->Redraw(state);
}

void
nsCanvasRenderingContext2D::SetCairoColor(nscolor c)
{
    double r = double(NS_GET_R(c) / 255.0);
    double g = double(NS_GET_G(c) / 255.0);
    double b = double(NS_GET_B(c) / 255.0);

//    fprintf (stderr, "::SetCairoColor r: %g g: %g b: %g a: %g\n", r, g, b, a);
    cairo_set_rgb_color (mCairo, r, g, b);
}

nsresult
nsCanvasRenderingContext2D::Init(nsCanvasFrame* aCanvasFrame, nsPresContext* aPresContext)
{
    nsresult rv;

    fprintf (stderr, "++ nsCanvasRenderingContext2D::Init!\n");

    mPresContext = aPresContext;
    mCanvasFrame = aCanvasFrame;
    mPixelsToTwips = mPresContext->PixelsToTwips();

    nsRect frameRect;
    mCanvasFrame->GetClientRect(frameRect);

    mWidth = NSToIntRound(float(frameRect.width) * (1.0f / mPixelsToTwips));
    mHeight = NSToIntRound(float(frameRect.height) * (1.0f / mPixelsToTwips));

    fprintf (stderr, "++ frame rect: %d %d %d %d (canvas: %d %d)\n", frameRect.x, frameRect.y, frameRect.width, frameRect.height, mWidth, mHeight);
    fprintf (stderr, "pixelstotwips factor: %g\n", mPixelsToTwips);

    // set up cairo
    mCairo = cairo_create();
    mSurfaceData = (char *) nsMemory::Alloc(mWidth * mHeight * 4);
    mSurface = cairo_surface_create_for_image (mSurfaceData,
                                               CAIRO_FORMAT_ARGB32,
                                               mWidth,
                                               mHeight,
                                               mWidth * 4);
    cairo_set_target_surface (mCairo, mSurface);

    // set up libpr0n
    mImageContainer = do_CreateInstance("@mozilla.org/image/container;1");
    if (!mImageContainer)
        return NS_ERROR_FAILURE;

    rv = mImageContainer->Init(mWidth, mHeight, nsnull);
    if (NS_FAILED(rv)) return rv;

    mImageFrame = do_CreateInstance("@mozilla.org/gfx/image/frame;2");
    if (!mImageFrame)
        return NS_ERROR_FAILURE;

    rv = mImageFrame->Init(0, 0, mWidth, mHeight, gfxIFormats::RGB_A8, 24);
    if (NS_FAILED(rv)) return rv;

    rv = mImageContainer->AppendFrame(mImageFrame);
    if (NS_FAILED(rv)) return rv;

    rv = mImageFrame->GetImageBytesPerRow(&mImageDataStride);
    rv |= mImageFrame->GetAlphaBytesPerRow(&mAlphaDataStride);

    mImageData = (PRUint8 *) nsMemory::Alloc(mHeight * mImageDataStride);
    mAlphaData = (PRUint8 *) nsMemory::Alloc(mHeight * mAlphaDataStride);

    if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult
nsCanvasRenderingContext2D::Paint(nsPresContext*       aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect&        aDirtyRect,
                                  nsFramePaintLayer    aWhichLayer,
                                  PRUint32             aFlags)
{
    nsresult rv;

    if (!mSurfaceData)
        return NS_OK;

    PRBool isVisible;
    if (NS_FAILED(mCanvasFrame->IsVisibleForPainting (aPresContext, aRenderingContext, PR_TRUE, &isVisible)) && !isVisible)
        return NS_OK;

    PRBool paintingSuppressed = PR_FALSE;
    if (NS_SUCCEEDED(aPresContext->PresShell()->IsPaintingSuppressed(&paintingSuppressed)) && paintingSuppressed)
        return NS_OK;

    if (aWhichLayer == NS_FRAME_PAINT_LAYER_BACKGROUND) {
        if (mDirty) {
            for (PRUint32 j = 0; j < mHeight; j++) {
                PRUint8 *inrow = (PRUint8*)(mSurfaceData + (mWidth * 4 * j));
                PRUint8 *outrowimage = mImageData + (mImageDataStride * j);
                PRUint8 *outrowalpha = mAlphaData + (mAlphaDataStride * j);

                for (PRUint32 i = 0; i < mWidth; i++) {
                    PRUint8 b = *inrow++;
                    PRUint8 g = *inrow++;
                    PRUint8 r = *inrow++;
                    PRUint8 a = *inrow++;

                    *outrowalpha++ = a;
                    *outrowimage++ = r;
                    *outrowimage++ = g;
                    *outrowimage++ = b;
                }
            }

            rv = mImageFrame->SetAlphaData(mAlphaData, mHeight * mAlphaDataStride, 0);
            if (NS_FAILED(rv)) {
                fprintf (stderr, "SetAlphaData: 0x%08x\n", rv);
                return rv;
            }

            rv = mImageFrame->SetImageData(mImageData, mHeight * mImageDataStride, 0);
            if (NS_FAILED(rv)) {
                fprintf (stderr, "SetImageData: 0x%08x\n", rv);
                return rv;
            }

            mDirty = PR_FALSE;
        }

        nsPoint dstpt = mCanvasFrame->GetPosition();
        nsRect src(0, 0, NSIntPixelsToTwips(mWidth, mPixelsToTwips), NSIntPixelsToTwips(mHeight, mPixelsToTwips));
        nsRect dst(dstpt.x, dstpt.y, src.width, src.height);
        return aRenderingContext.DrawImage(mImageContainer, src, dst);
    }

    return NS_OK;
}

//
// nsCanvasRenderingContext2D impl
//

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetCanvas(nsIBoxObject **canvas)
{
    //NS_IF_ADDREF(*canvas = mCanvasFrame);
    return NS_ERROR_NOT_IMPLEMENTED;
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
nsCanvasRenderingContext2D::SetStrokeStyle(const nsAString& style)
{
    if (ColorStringToColor(style, mStrokeColor))
        return NS_OK;

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetStrokeStyle(nsAString& style)
{
    style.Assign(NS_ConvertUTF8toUTF16(nsPrintfCString(100, "#%08x", mStrokeColor)));
    return NS_OK;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::SetFillStyle(const nsAString& style)
{
    if (ColorStringToColor(style, mFillColor))
        return NS_OK;

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::GetFillStyle(nsAString& style)
{
    style.Assign(NS_ConvertUTF8toUTF16(nsPrintfCString(100, "#%08x", mFillColor)));
    return NS_OK;
}

//
// gradients and patterns
//
NS_IMETHODIMP
nsCanvasRenderingContext2D::CreateLinearGradient(float x0, float y0, float x1, float y1,
                                                 nsICanvasGradient **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::CreateRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1,
                                                 nsICanvasGradient **_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::CreatePattern(nsIDOMHTMLImageElement *image,
                                          const nsAString& repetition,
                                          nsICanvasPattern **_retval)
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
    if (mCanvasFrame->GetStyleBackground()->IsTransparent())
        SetCairoColor(mPresContext->DefaultBackgroundColor());
    else
        SetCairoColor(mCanvasFrame->GetStyleBackground()->mBackgroundColor);

    cairo_set_alpha (mCairo, 0.0);
    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);
    cairo_fill (mCairo);

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::FillRect(float x, float y, float w, float h)
{
    SetCairoColor(mFillColor);

    cairo_new_path (mCairo);
    cairo_rectangle (mCairo, x, y, w, h);
    cairo_fill (mCairo);

    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::StrokeRect(float x, float y, float w, float h)
{
    SetCairoColor(mStrokeColor);

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
    SetCairoColor(mFillColor);
    cairo_fill(mCairo);
    return Redraw();
}

NS_IMETHODIMP
nsCanvasRenderingContext2D::Stroke()
{
    SetCairoColor(mStrokeColor);
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

NS_IMETHODIMP
nsCanvasRenderingContext2D::DrawImage()
{
#if 0
    nsCOMPtr<nsIImageLoadingContent> contentImage(aImage);
    nsCOMPtr<imgIRequest> request;
    contentImage->GetRequest(nsIImageLoadingContent::CURRENT_REQUEST, getter_AddRefs(request));
    
    nsCOMPtr<imgIContainer> img;
    request->GetImage(getter_AddRefs(img));

    nsCOMPtr<gfxIImageFrame> frame;
    img->GetCurrentFrame(getter_AddRefs(frame));

    PRInt32 width, height;

    frame->GetWidth(&width);
    frame->GetHeight(&height);

    /* gotta get some data, allocate it and convert it to ARGB32 */
    PRUint8 *data = (PRUint8*)nsMemory::Alloc(height * width * 4);

    gfx_format format;
    frame->GetFormat(&format);

    frame->LockAlphaData();
    frame->LockImageData();

    PRUint32 alphaBytesPerRow, imageBytesPerRow;
    frame->GetImageBytesPerRow(&imageBytesPerRow);
    frame->GetAlphaBytesPerRow(&alphaBytesPerRow);
    
    PRUint8 *imageData;
    PRUint8 *alphaData;
    PRUint32 len;
    frame->GetImageData(&imageData, &len);
    frame->GetAlphaData(&alphaData, &len); /* its ok for this to fail since we'll check before poking alphaData below */

    /* if we're on mac, we need to deal with RGBx (or xRGB) instead of just RGB/BGR */

    for (PRUint32 j = 0; j < height; j++) {
        PRUint8 *outimage = data;
        PRUint8 *inrowimage = imageData + (imageBytesPerRow * j);
        

        if (format == gfxIFormats::RGB || format == gfxIFormats::BGR) {        
            for (PRUint32 i = 0; i < width; i++) {
                PRUint8 r = *inrowimage++;
                PRUint8 g = *inrowimage++;
                PRUint8 b = *inrowimage++;
                *outimage++ = 255;
                *outimage++ = r;
                *outimage++ = g;
                *outimage++ = b;
            }
        }
        if (format == gfxIFormats::RGB_A1 || format == gfxIFormats::BGR_A1) {
            PRUint8 *inrowalpha = alphaData + (alphaBytesPerRow * j);
        
            for (PRUint32 i = 0; i < width; i++) {
                PRUint8 a = inrowalpha[i >> 3];                
                PRUint8 r = *inrowimage++;
                PRUint8 g = *inrowimage++;
                PRUint8 b = *inrowimage++;
                *outimage++ = a;
                *outimage++ = r;
                *outimage++ = g;
                *outimage++ = b;
            }
        }
        if (format == gfxIFormats::RGB_A8 || format == gfxIFormats::BGR_A8) {
            PRUint8 *inrowalpha = alphaData + (alphaBytesPerRow * j);
        
            for (PRUint32 i = 0; i < width; i++) {
                PRUint8 a = *inrowalpha++;
                PRUint8 r = *inrowimage++;
                PRUint8 g = *inrowimage++;
                PRUint8 b = *inrowimage++;
                *outimage++ = a;
                *outimage++ = r;
                *outimage++ = g;
                *outimage++ = b;
            }
        }

    }
    
    frame->UnlockAlphaData();
    frame->UnlockImageData();

    /* save so we can set the composite operator and then reset it */
    cairo_save(mCairo);

    cairo_surface_t *surface;
    surface = cairo_image_surface_create_for_data((char *)data, CAIRO_FORMAT_ARGB32,
                                                  width, height, width*4);

    cairo_pattern_t *pattern = cairo_pattern_create_for_surface(surface);

//    SetCompositeOperation(composite);
    cairo_rectangle(mCairo, x, y, w, h);
    cairo_set_pattern(mCairo, pattern);
    cairo_fill(mCairo);

    /* restore */
    cairo_restore(mCairo);

    cairo_pattern_destroy(pattern);
    nsMemory::Free(data);
    cairo_surface_destroy(surface);

    return NS_OK;
#endif
    return NS_ERROR_NOT_IMPLEMENTED;
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
