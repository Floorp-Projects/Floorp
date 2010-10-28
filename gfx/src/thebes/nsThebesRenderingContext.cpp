/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@pavlov.net>
 *   Vladimir Vukicevic <vladimir@pobox.com>
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

#include "nsThebesRenderingContext.h"
#include "nsThebesDeviceContext.h"

#include "nsString.h"
#include "nsTransform2D.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsGfxCIID.h"

#include "nsThebesRegion.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gfxPlatform.h"

#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#include "cairo-win32.h"
#endif

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

//////////////////////////////////////////////////////////////////////

// XXXTodo: rename FORM_TWIPS to FROM_APPUNITS
#define FROM_TWIPS(_x)  ((gfxFloat)((_x)/(mP2A)))
#define FROM_TWIPS_INT(_x)  (NSToIntRound((gfxFloat)((_x)/(mP2A))))
#define TO_TWIPS(_x)    ((nscoord)((_x)*(mP2A)))
#define GFX_RECT_FROM_TWIPS_RECT(_r)   (gfxRect(FROM_TWIPS((_r).x), FROM_TWIPS((_r).y), FROM_TWIPS((_r).width), FROM_TWIPS((_r).height)))

//////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsThebesRenderingContext, nsIRenderingContext)

// Hard limit substring lengths to 8000 characters ... this lets us statically
// size the cluster buffer array in FindSafeLength
#define MAX_GFX_TEXT_BUF_SIZE 8000
static PRInt32 GetMaxChunkLength(nsThebesRenderingContext* aContext)
{
    PRInt32 len = aContext->GetMaxStringLength();
    return PR_MIN(len, MAX_GFX_TEXT_BUF_SIZE);
}

static PRInt32 FindSafeLength(nsThebesRenderingContext* aContext,
                              const PRUnichar *aString, PRUint32 aLength,
                              PRUint32 aMaxChunkLength)
{
    if (aLength <= aMaxChunkLength)
        return aLength;

    PRInt32 len = aMaxChunkLength;

    // Ensure that we don't break inside a surrogate pair
    while (len > 0 && NS_IS_LOW_SURROGATE(aString[len])) {
        len--;
    }
    if (len == 0) {
        // We don't want our caller to go into an infinite loop, so don't
        // return zero. It's hard to imagine how we could actually get here
        // unless there are languages that allow clusters of arbitrary size.
        // If there are and someone feeds us a 500+ character cluster, too
        // bad.
        return aMaxChunkLength;
    }
    return len;
}

static PRInt32 FindSafeLength(nsThebesRenderingContext* aContext,
                              const char *aString, PRUint32 aLength,
                              PRUint32 aMaxChunkLength)
{
    // Since it's ASCII, we don't need to worry about clusters or RTL
    return PR_MIN(aLength, aMaxChunkLength);
}

nsThebesRenderingContext::nsThebesRenderingContext()
  : mLineStyle(nsLineStyle_kNone)
  , mColor(NS_RGB(0,0,0))
{
}

nsThebesRenderingContext::~nsThebesRenderingContext()
{
}

//////////////////////////////////////////////////////////////////////
//// nsIRenderingContext

NS_IMETHODIMP
nsThebesRenderingContext::Init(nsIDeviceContext* aContext, gfxASurface *aThebesSurface)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::Init ctx %p thebesSurface %p\n", this, aContext, aThebesSurface));

    mDeviceContext = aContext;
    mWidget = nsnull;

    mThebes = new gfxContext(aThebesSurface);

    return (CommonInit());
}

NS_IMETHODIMP
nsThebesRenderingContext::Init(nsIDeviceContext* aContext, gfxContext *aThebesContext)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::Init ctx %p thebesContext %p\n", this, aContext, aThebesContext));

    mDeviceContext = aContext;
    mWidget = nsnull;

    mThebes = aThebesContext;

    return (CommonInit());
}

NS_IMETHODIMP
nsThebesRenderingContext::Init(nsIDeviceContext* aContext, nsIWidget *aWidget)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::Init ctx %p widget %p\n", this, aContext, aWidget));

    mDeviceContext = aContext;
    mWidget = aWidget;

    mThebes = new gfxContext(aWidget->GetThebesSurface());

    //mThebes->SetColor(gfxRGBA(0.9, 0.0, 0.0, 0.3));
    //mThebes->Paint();

    //mThebes->Translate(gfxPoint(300,0));
    //mThebes->Rotate(M_PI/4);

    return (CommonInit());
}

NS_IMETHODIMP
nsThebesRenderingContext::CommonInit(void)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::CommonInit\n", this));

    mThebes->SetLineWidth(1.0);

    mP2A = mDeviceContext->AppUnitsPerDevPixel();

    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetDeviceContext(nsIDeviceContext *& aDeviceContext)
{
    aDeviceContext = mDeviceContext;
    NS_IF_ADDREF(aDeviceContext);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::PushTranslation(PushedTranslation* aState)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::PushTranslation\n", this));

    // XXX this is slow!
    PushState();
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::PopTranslation(PushedTranslation* aState)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::PopTranslation\n", this));

    // XXX this is slow!
    PopState();
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::SetTranslation(nscoord aX, nscoord aY)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::SetTranslation %d %d\n", this, aX, aY));

    gfxMatrix newMat(mThebes->CurrentMatrix());
    newMat.x0 = aX;
    newMat.y0 = aY;
    mThebes->SetMatrix(newMat);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::PushState()
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::PushState\n", this));

    mThebes->Save();
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::PopState()
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::PopState\n", this));

    mThebes->Restore();
    return NS_OK;
}

//
// clipping
//

NS_IMETHODIMP
nsThebesRenderingContext::SetClipRect(const nsRect& aRect,
                                      nsClipCombine aCombine)
{
    //return NS_OK;
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::SetClipRect [%d,%d,%d,%d] %d\n", this, aRect.x, aRect.y, aRect.width, aRect.height, aCombine));

    if (aCombine == nsClipCombine_kReplace) {
        mThebes->ResetClip();
    } else if (aCombine != nsClipCombine_kIntersect) {
        NS_WARNING("Unexpected usage of SetClipRect");
    }

    mThebes->NewPath();
    gfxRect clipRect(GFX_RECT_FROM_TWIPS_RECT(aRect));
    if (mThebes->UserToDevicePixelSnapped(clipRect, PR_TRUE)) {
        gfxMatrix mat(mThebes->CurrentMatrix());
        mThebes->IdentityMatrix();
        mThebes->Rectangle(clipRect);
        mThebes->SetMatrix(mat);
    } else {
        mThebes->Rectangle(clipRect);
    }

    mThebes->Clip();

    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::SetClipRegion(const nsIntRegion& aRegion,
                                        nsClipCombine aCombine)
{
    // Region is in device coords, no transformation.
    // This should only be called when there is no transform in place, when we
    // we just start painting a widget. The region is set by the platform paint
    // routine.
    NS_ASSERTION(aCombine == nsClipCombine_kReplace,
                 "Unexpected usage of SetClipRegion");

    gfxMatrix mat = mThebes->CurrentMatrix();
    mThebes->IdentityMatrix();

    mThebes->ResetClip();
    
    mThebes->NewPath();
    nsIntRegionRectIterator iter(aRegion);
    const nsIntRect* rect;
    while ((rect = iter.Next())) {
        mThebes->Rectangle(gfxRect(rect->x, rect->y, rect->width, rect->height),
                           PR_TRUE);
    }
    mThebes->Clip();

    mThebes->SetMatrix(mat);

    return NS_OK;
}

//
// other junk
//

NS_IMETHODIMP
nsThebesRenderingContext::SetLineStyle(nsLineStyle aLineStyle)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::SetLineStyle %d\n", this, aLineStyle));
    switch (aLineStyle) {
        case nsLineStyle_kSolid:
            mThebes->SetDash(gfxContext::gfxLineSolid);
            break;
        case nsLineStyle_kDashed:
            mThebes->SetDash(gfxContext::gfxLineDashed);
            break;
        case nsLineStyle_kDotted:
            mThebes->SetDash(gfxContext::gfxLineDotted);
            break;
        case nsLineStyle_kNone:
        default:
            // nothing uses kNone
            NS_ERROR("SetLineStyle: Invalid line style");
            break;
    }

    mLineStyle = aLineStyle;
    return NS_OK;
}


NS_IMETHODIMP
nsThebesRenderingContext::SetColor(nscolor aColor)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::SetColor 0x%08x\n", this, aColor));
    /* This sets the color assuming the sRGB color space, since that's what all
     * CSS colors are defined to be in by the spec.
     */
    mThebes->SetColor(gfxRGBA(aColor));

    mColor = aColor;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetColor(nscolor &aColor) const
{
    aColor = mColor;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::Translate(nscoord aX, nscoord aY)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::Translate %d %d\n", this, aX, aY));
    mThebes->Translate (gfxPoint(FROM_TWIPS(aX), FROM_TWIPS(aY)));
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::Scale(float aSx, float aSy)
{
    // as far as I can tell, noone actually calls this
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::Scale %f %f\n", this, aSx, aSy));
    mThebes->Scale (aSx, aSy);
    return NS_OK;
}

void
nsThebesRenderingContext::UpdateTempTransformMatrix()
{
    //PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::UpdateTempTransformMatrix\n", this));

    /*****
     * Thebes matrix layout:   gfx matrix layout:
     * | xx yx 0 |            | m00 m01  0 |
     * | xy yy 0 |            | m10 m11  0 |
     * | x0 y0 1 |            | m20 m21  1 |
     *****/

    const gfxMatrix& ctm = mThebes->CurrentMatrix();
    NS_ASSERTION(ctm.yx == 0 && ctm.xy == 0, "Can't represent Thebes matrix to Gfx");
    mTempTransform.SetToTranslate(TO_TWIPS(ctm.x0), TO_TWIPS(ctm.y0));
    mTempTransform.AddScale(ctm.xx, ctm.yy);
}

nsTransform2D&
nsThebesRenderingContext::CurrentTransform()
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::CurrentTransform\n", this));
    UpdateTempTransformMatrix();
    return mTempTransform;
}

/****
 **** XXXXXX
 ****
 **** On other gfx implementations, the transform returned by this
 **** has a built in twips to pixels ratio.  That is, you pass in
 **** twips to any nsTransform2D TransformCoord method, and you
 **** get back pixels.  This makes no sense.  We don't do this.
 **** This in turn breaks SVG and <object>; those should just be
 **** fixed to not use this!
 ****/

NS_IMETHODIMP
nsThebesRenderingContext::GetCurrentTransform(nsTransform2D *&aTransform)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::GetCurrentTransform\n", this));
    UpdateTempTransformMatrix();
    aTransform = &mTempTransform;
    return NS_OK;
}

void
nsThebesRenderingContext::TransformCoord (nscoord *aX, nscoord *aY)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::TransformCoord\n", this));

    gfxPoint pt(FROM_TWIPS(*aX), FROM_TWIPS(*aY));

    pt = mThebes->UserToDevice (pt);

    *aX = TO_TWIPS(pt.x);
    *aY = TO_TWIPS(pt.y);
}

NS_IMETHODIMP
nsThebesRenderingContext::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::DrawLine %d %d %d %d\n", this, aX0, aY0, aX1, aY1));

    gfxPoint p0 = gfxPoint(FROM_TWIPS(aX0), FROM_TWIPS(aY0));
    gfxPoint p1 = gfxPoint(FROM_TWIPS(aX1), FROM_TWIPS(aY1));

    // we can't draw thick lines with gfx, so we always assume we want pixel-aligned
    // lines if the rendering context is at 1.0 scale
    gfxMatrix savedMatrix = mThebes->CurrentMatrix();
    if (!savedMatrix.HasNonTranslation()) {
        p0 = mThebes->UserToDevice(p0);
        p1 = mThebes->UserToDevice(p1);

        p0.Round();
        p1.Round();

        mThebes->IdentityMatrix();

        mThebes->NewPath();

        // snap straight lines
        if (p0.x == p1.x) {
            mThebes->Line(p0 + gfxPoint(0.5, 0),
                          p1 + gfxPoint(0.5, 0));
        } else if (p0.y == p1.y) {
            mThebes->Line(p0 + gfxPoint(0, 0.5),
                          p1 + gfxPoint(0, 0.5));
        } else {
            mThebes->Line(p0, p1);
        }

        mThebes->Stroke();

        mThebes->SetMatrix(savedMatrix);
    } else {
        mThebes->NewPath();
        mThebes->Line(p0, p1);
        mThebes->Stroke();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::DrawRect(const nsRect& aRect)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::DrawRect [%d,%d,%d,%d]\n", this, aRect.x, aRect.y, aRect.width, aRect.height));

    mThebes->NewPath();
    mThebes->Rectangle(GFX_RECT_FROM_TWIPS_RECT(aRect), PR_TRUE);
    mThebes->Stroke();

    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    DrawRect(nsRect(aX, aY, aWidth, aHeight));
    return NS_OK;
}


/* Clamp r to (0,0) (2^23,2^23)
 * these are to be device coordinates.
 *
 * Returns PR_FALSE if the rectangle is completely out of bounds,
 * PR_TRUE otherwise.
 *
 * This function assumes that it will be called with a rectangle being
 * drawn into a surface with an identity transformation matrix; that
 * is, anything above or to the left of (0,0) will be offscreen.
 *
 * First it checks if the rectangle is entirely beyond
 * CAIRO_COORD_MAX; if so, it can't ever appear on the screen --
 * PR_FALSE is returned.
 *
 * Then it shifts any rectangles with x/y < 0 so that x and y are = 0,
 * and adjusts the width and height appropriately.  For example, a
 * rectangle from (0,-5) with dimensions (5,10) will become a
 * rectangle from (0,0) with dimensions (5,5).
 *
 * If after negative x/y adjustment to 0, either the width or height
 * is negative, then the rectangle is completely offscreen, and
 * nothing is drawn -- PR_FALSE is returned.
 *
 * Finally, if x+width or y+height are greater than CAIRO_COORD_MAX,
 * the width and height are clamped such x+width or y+height are equal
 * to CAIRO_COORD_MAX, and PR_TRUE is returned.
 */
#define CAIRO_COORD_MAX (double(0x7fffff))

static PRBool
ConditionRect(gfxRect& r) {
    // if either x or y is way out of bounds;
    // note that we don't handle negative w/h here
    if (r.pos.x > CAIRO_COORD_MAX || r.pos.y > CAIRO_COORD_MAX)
        return PR_FALSE;

    if (r.pos.x < 0.0) {
        r.size.width += r.pos.x;
        if (r.size.width < 0.0)
            return PR_FALSE;
        r.pos.x = 0.0;
    }

    if (r.pos.x + r.size.width > CAIRO_COORD_MAX) {
        r.size.width = CAIRO_COORD_MAX - r.pos.x;
    }

    if (r.pos.y < 0.0) {
        r.size.height += r.pos.y;
        if (r.size.height < 0.0)
            return PR_FALSE;

        r.pos.y = 0.0;
    }

    if (r.pos.y + r.size.height > CAIRO_COORD_MAX) {
        r.size.height = CAIRO_COORD_MAX - r.pos.y;
    }
    return PR_TRUE;
}

NS_IMETHODIMP
nsThebesRenderingContext::FillRect(const nsRect& aRect)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::FillRect [%d,%d,%d,%d]\n", this, aRect.x, aRect.y, aRect.width, aRect.height));

    gfxRect r(GFX_RECT_FROM_TWIPS_RECT(aRect));

    /* Clamp coordinates to work around a design bug in cairo */
    nscoord bigval = (nscoord)(CAIRO_COORD_MAX*mP2A);
    if (aRect.width > bigval ||
        aRect.height > bigval ||
        aRect.x < -bigval ||
        aRect.x > bigval ||
        aRect.y < -bigval ||
        aRect.y > bigval)
    {
        gfxMatrix mat = mThebes->CurrentMatrix();

        r = mat.Transform(r);

        if (!ConditionRect(r))
            return NS_OK;

        mThebes->IdentityMatrix();
        mThebes->NewPath();

        PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::FillRect conditioned to [%f,%f,%f,%f]\n", this, r.pos.x, r.pos.y, r.size.width, r.size.height));

        mThebes->Rectangle(r, PR_TRUE);
        mThebes->Fill();
        mThebes->SetMatrix(mat);

        return NS_OK;
    }

    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::FillRect raw [%f,%f,%f,%f]\n", this, r.pos.x, r.pos.y, r.size.width, r.size.height));

    mThebes->NewPath();
    mThebes->Rectangle(r, PR_TRUE);
    mThebes->Fill();

    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    FillRect(nsRect(aX, aY, aWidth, aHeight));
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::InvertRect(const nsRect& aRect)
{
    gfxContext::GraphicsOperator lastOp = mThebes->CurrentOperator();

    mThebes->SetOperator(gfxContext::OPERATOR_XOR);
    nsresult rv = FillRect(aRect);
    mThebes->SetOperator(lastOp);

    return rv;
}

NS_IMETHODIMP
nsThebesRenderingContext::InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    return InvertRect(nsRect(aX, aY, aWidth, aHeight));
}

NS_IMETHODIMP
nsThebesRenderingContext::DrawEllipse(const nsRect& aRect)
{
    return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsThebesRenderingContext::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::DrawEllipse [%d,%d,%d,%d]\n", this, aX, aY, aWidth, aHeight));

    mThebes->NewPath();
    mThebes->Ellipse(gfxPoint(FROM_TWIPS(aX) + FROM_TWIPS(aWidth)/2.0,
                              FROM_TWIPS(aY) + FROM_TWIPS(aHeight)/2.0),
                     gfxSize(FROM_TWIPS(aWidth),
                             FROM_TWIPS(aHeight)));
    mThebes->Stroke();

    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::FillEllipse(const nsRect& aRect)
{
    return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsThebesRenderingContext::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::FillEllipse [%d,%d,%d,%d]\n", this, aX, aY, aWidth, aHeight));

    mThebes->NewPath();
    mThebes->Ellipse(gfxPoint(FROM_TWIPS(aX) + FROM_TWIPS(aWidth)/2.0,
                              FROM_TWIPS(aY) + FROM_TWIPS(aHeight)/2.0),
                     gfxSize(FROM_TWIPS(aWidth),
                             FROM_TWIPS(aHeight)));
    mThebes->Fill();

    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::FillPolygon(const nsPoint twPoints[], PRInt32 aNumPoints)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::FillPolygon %d\n", this, aNumPoints));

    if (aNumPoints == 0)
        return NS_OK;

    if (aNumPoints == 4) {
    }

    nsAutoArrayPtr<gfxPoint> pxPoints(new gfxPoint[aNumPoints]);

    for (int i = 0; i < aNumPoints; i++) {
        pxPoints[i].x = FROM_TWIPS(twPoints[i].x);
        pxPoints[i].y = FROM_TWIPS(twPoints[i].y);
    }

    mThebes->NewPath();
    mThebes->Polygon(pxPoints, aNumPoints);
    mThebes->Fill();

    return NS_OK;
}

void*
nsThebesRenderingContext::GetNativeGraphicData(GraphicDataType aType)
{
    if (aType == NATIVE_GDK_DRAWABLE)
    {
        if (mWidget)
            return mWidget->GetNativeData(NS_NATIVE_WIDGET);
    }
    if (aType == NATIVE_THEBES_CONTEXT)
        return mThebes;
    if (aType == NATIVE_CAIRO_CONTEXT)
        return mThebes->GetCairo();
#ifdef XP_WIN
    if (aType == NATIVE_WINDOWS_DC) {
        nsRefPtr<gfxASurface> surf(mThebes->CurrentSurface());
        if (!surf || surf->CairoStatus())
            return nsnull;
        return static_cast<gfxWindowsSurface*>(static_cast<gfxASurface*>(surf.get()))->GetDC();
    }
#endif
#ifdef XP_OS2
    if (aType == NATIVE_OS2_PS) {
        nsRefPtr<gfxASurface> surf(mThebes->CurrentSurface());
        if (!surf || surf->CairoStatus())
            return nsnull;
        return (void*)(static_cast<gfxOS2Surface*>(static_cast<gfxASurface*>(surf.get()))->GetPS());
    }
#endif

    return nsnull;
}

NS_IMETHODIMP
nsThebesRenderingContext::PushFilter(const nsRect& twRect, PRBool aAreaIsOpaque, float aOpacity)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG,
           ("## %p nsTRC::PushFilter [%d,%d,%d,%d] isOpaque: %d opacity: %f\n",
            this, twRect.x, twRect.y, twRect.width, twRect.height,
            aAreaIsOpaque, aOpacity));

    mOpacityArray.AppendElement(aOpacity);

    mThebes->Save();
    mThebes->Clip(GFX_RECT_FROM_TWIPS_RECT(twRect));
    mThebes->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);

    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::PopFilter()
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::PopFilter\n"));

    if (mOpacityArray.Length() > 0) {
        float f = mOpacityArray[mOpacityArray.Length()-1];
        mOpacityArray.RemoveElementAt(mOpacityArray.Length()-1);

        mThebes->PopGroupToSource();

        if (f < 0.0) {
            mThebes->SetOperator(gfxContext::OPERATOR_SOURCE);
            mThebes->Paint();
        } else {
            mThebes->SetOperator(gfxContext::OPERATOR_OVER);
            mThebes->Paint(f);
        }

        mThebes->Restore();
    }


    return NS_OK;
}

//
// text junk
//
NS_IMETHODIMP
nsThebesRenderingContext::SetRightToLeftText(PRBool aIsRTL)
{
    return mFontMetrics->SetRightToLeftText(aIsRTL);
}

NS_IMETHODIMP
nsThebesRenderingContext::GetRightToLeftText(PRBool* aIsRTL)
{
    *aIsRTL = mFontMetrics->GetRightToLeftText();
    return NS_OK;
}

void
nsThebesRenderingContext::SetTextRunRTL(PRBool aIsRTL)
{
    mFontMetrics->SetTextRunRTL(aIsRTL);
}

NS_IMETHODIMP
nsThebesRenderingContext::SetFont(const nsFont& aFont, nsIAtom* aLanguage,
                                  gfxUserFontSet *aUserFontSet)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::SetFont %p\n", this, &aFont));

    nsCOMPtr<nsIFontMetrics> newMetrics;
    mDeviceContext->GetMetricsFor(aFont, aLanguage, aUserFontSet,
                                  *getter_AddRefs(newMetrics));
    mFontMetrics = reinterpret_cast<nsIThebesFontMetrics*>(newMetrics.get());
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::SetFont(const nsFont& aFont,
                                  gfxUserFontSet *aUserFontSet)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::SetFont %p\n", this, &aFont));

    nsCOMPtr<nsIFontMetrics> newMetrics;
    mDeviceContext->GetMetricsFor(aFont, nsnull, aUserFontSet,
                                  *getter_AddRefs(newMetrics));
    mFontMetrics = reinterpret_cast<nsIThebesFontMetrics*>(newMetrics.get());
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::SetFont(nsIFontMetrics *aFontMetrics)
{
    PR_LOG(gThebesGFXLog, PR_LOG_DEBUG, ("## %p nsTRC::SetFont[Metrics] %p\n", this, aFontMetrics));

    mFontMetrics = static_cast<nsIThebesFontMetrics*>(aFontMetrics);
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
    aFontMetrics = mFontMetrics;
    NS_IF_ADDREF(aFontMetrics);
    return NS_OK;
}

PRInt32
nsThebesRenderingContext::GetMaxStringLength()
{
    if (!mFontMetrics)
        return 1;
    return mFontMetrics->GetMaxStringLength();
}

NS_IMETHODIMP
nsThebesRenderingContext::GetWidth(char aC, nscoord &aWidth)
{
    if (aC == ' ' && mFontMetrics)
        return mFontMetrics->GetSpaceWidth(aWidth);

    return GetWidth(&aC, 1, aWidth);
}

NS_IMETHODIMP
nsThebesRenderingContext::GetWidth(PRUnichar aC, nscoord &aWidth, PRInt32 *aFontID)
{
    return GetWidth(&aC, 1, aWidth, aFontID);
}

NS_IMETHODIMP
nsThebesRenderingContext::GetWidth(const nsString& aString, nscoord &aWidth,
                                   PRInt32 *aFontID)
{
    return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsThebesRenderingContext::GetWidth(const char* aString, nscoord& aWidth)
{
    return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsThebesRenderingContext::DrawString(const nsString& aString, nscoord aX, nscoord aY,
                                     PRInt32 aFontID, const nscoord* aSpacing)
{
    return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP
nsThebesRenderingContext::GetWidth(const char* aString,
                                   PRUint32 aLength,
                                   nscoord& aWidth)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    aWidth = 0;
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
        nscoord width;
        nsresult rv = GetWidthInternal(aString, len, width);
        if (NS_FAILED(rv))
            return rv;
        aWidth += width;
        aLength -= len;
        aString += len;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetWidth(const PRUnichar *aString,
                                   PRUint32 aLength,
                                   nscoord &aWidth,
                                   PRInt32 *aFontID)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    aWidth = 0;

    if (aFontID) {
        *aFontID = 0;
    }

    while (aLength > 0) {
        PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
        nscoord width;
        nsresult rv = GetWidthInternal(aString, len, width);
        if (NS_FAILED(rv))
            return rv;
        aWidth += width;
        aLength -= len;
        aString += len;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetTextDimensions(const char* aString,
                                            PRUint32 aLength,
                                            nsTextDimensions& aDimensions)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    if (aLength <= maxChunkLength)
        return GetTextDimensionsInternal(aString, aLength, aDimensions);

    PRBool firstIteration = PR_TRUE;
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
        nsTextDimensions dimensions;
        nsresult rv = GetTextDimensionsInternal(aString, len, dimensions);
        if (NS_FAILED(rv))
            return rv;
        if (firstIteration) {
            // Instead of combining with a Clear()ed nsTextDimensions, we
            // assign directly in the first iteration. This ensures that
            // negative ascent/ descent can be returned.
            aDimensions = dimensions;
        } else {
            aDimensions.Combine(dimensions);
        }
        aLength -= len;
        aString += len;
        firstIteration = PR_FALSE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetTextDimensions(const PRUnichar* aString,
                                            PRUint32 aLength,
                                            nsTextDimensions& aDimensions,
                                            PRInt32* aFontID)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    if (aLength <= maxChunkLength)
        return GetTextDimensionsInternal(aString, aLength, aDimensions);

    if (aFontID) {
        *aFontID = nsnull;
    }

    PRBool firstIteration = PR_TRUE;
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
        nsTextDimensions dimensions;
        nsresult rv = GetTextDimensionsInternal(aString, len, dimensions);
        if (NS_FAILED(rv))
            return rv;
        if (firstIteration) {
            // Instead of combining with a Clear()ed nsTextDimensions, we
            // assign directly in the first iteration. This ensures that
            // negative ascent/ descent can be returned.
            aDimensions = dimensions;
        } else {
            aDimensions.Combine(dimensions);
        }
        aLength -= len;
        aString += len;
        firstIteration = PR_FALSE;
    }
    return NS_OK;
}

#if defined(_WIN32) || defined(XP_OS2) || defined(MOZ_X11) || defined(XP_BEOS)
NS_IMETHODIMP
nsThebesRenderingContext::GetTextDimensions(const char*       aString,
                                            PRInt32           aLength,
                                            PRInt32           aAvailWidth,
                                            PRInt32*          aBreaks,
                                            PRInt32           aNumBreaks,
                                            nsTextDimensions& aDimensions,
                                            PRInt32&          aNumCharsFit,
                                            nsTextDimensions& aLastWordDimensions,
                                            PRInt32*          aFontID)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    if (aLength <= PRInt32(maxChunkLength))
        return GetTextDimensionsInternal(aString, aLength, aAvailWidth, aBreaks, aNumBreaks,
                                         aDimensions, aNumCharsFit, aLastWordDimensions, aFontID);

    if (aFontID) {
        *aFontID = 0;
    }

    // Do a naive implementation based on 3-arg GetTextDimensions
    PRInt32 x = 0;
    PRInt32 wordCount;
    for (wordCount = 0; wordCount < aNumBreaks; ++wordCount) {
        PRInt32 lastBreak = wordCount > 0 ? aBreaks[wordCount - 1] : 0;
        nsTextDimensions dimensions;

        NS_ASSERTION(aBreaks[wordCount] > lastBreak, "Breaks must be monotonically increasing");
        NS_ASSERTION(aBreaks[wordCount] <= aLength, "Breaks can't exceed string length");

         // Call safe method

        nsresult rv =
            GetTextDimensions(aString + lastBreak, aBreaks[wordCount] - lastBreak,
                            dimensions);
        if (NS_FAILED(rv))
            return rv;
        x += dimensions.width;
        // The first word always "fits"
        if (x > aAvailWidth && wordCount > 0)
            break;
        // aDimensions ascent/descent should exclude the last word (unless there
        // is only one word) so we let it run one word behind
        if (wordCount == 0) {
            aDimensions = dimensions;
        } else {
            aDimensions.Combine(aLastWordDimensions);
        }
        aNumCharsFit = aBreaks[wordCount];
        aLastWordDimensions = dimensions;
    }
    // aDimensions width should include all the text
    aDimensions.width = x;
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetTextDimensions(const PRUnichar*  aString,
                                            PRInt32           aLength,
                                            PRInt32           aAvailWidth,
                                            PRInt32*          aBreaks,
                                            PRInt32           aNumBreaks,
                                            nsTextDimensions& aDimensions,
                                            PRInt32&          aNumCharsFit,
                                            nsTextDimensions& aLastWordDimensions,
                                            PRInt32*          aFontID)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    if (aLength <= PRInt32(maxChunkLength))
        return GetTextDimensionsInternal(aString, aLength, aAvailWidth, aBreaks, aNumBreaks,
                                     aDimensions, aNumCharsFit, aLastWordDimensions, aFontID);

    if (aFontID) {
        *aFontID = 0;
    }

    // Do a naive implementation based on 3-arg GetTextDimensions
    PRInt32 x = 0;
    PRInt32 wordCount;
    for (wordCount = 0; wordCount < aNumBreaks; ++wordCount) {
        PRInt32 lastBreak = wordCount > 0 ? aBreaks[wordCount - 1] : 0;

        NS_ASSERTION(aBreaks[wordCount] > lastBreak, "Breaks must be monotonically increasing");
        NS_ASSERTION(aBreaks[wordCount] <= aLength, "Breaks can't exceed string length");

        nsTextDimensions dimensions;
        // Call safe method
        nsresult rv =
            GetTextDimensions(aString + lastBreak, aBreaks[wordCount] - lastBreak,
                        dimensions);
        if (NS_FAILED(rv))
            return rv;
        x += dimensions.width;
        // The first word always "fits"
        if (x > aAvailWidth && wordCount > 0)
            break;
        // aDimensions ascent/descent should exclude the last word (unless there
        // is only one word) so we let it run one word behind
        if (wordCount == 0) {
            aDimensions = dimensions;
        } else {
            aDimensions.Combine(aLastWordDimensions);
        }
        aNumCharsFit = aBreaks[wordCount];
        aLastWordDimensions = dimensions;
    }
    // aDimensions width should include all the text
    aDimensions.width = x;
    return NS_OK;
}
#endif

#ifdef MOZ_MATHML
NS_IMETHODIMP
nsThebesRenderingContext::GetBoundingMetrics(const char*        aString,
                                             PRUint32           aLength,
                                             nsBoundingMetrics& aBoundingMetrics)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    if (aLength <= maxChunkLength)
        return GetBoundingMetricsInternal(aString, aLength, aBoundingMetrics);

    PRBool firstIteration = PR_TRUE;
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
        nsBoundingMetrics metrics;
        nsresult rv = GetBoundingMetricsInternal(aString, len, metrics);
        if (NS_FAILED(rv))
            return rv;
        if (firstIteration) {
            // Instead of combining with a Clear()ed nsBoundingMetrics, we
            // assign directly in the first iteration. This ensures that
            // negative ascent/ descent can be returned and the left bearing
            // is properly initialized.
            aBoundingMetrics = metrics;
        } else {
            aBoundingMetrics += metrics;
        }
        aLength -= len;
        aString += len;
        firstIteration = PR_FALSE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetBoundingMetrics(const PRUnichar*   aString,
                                             PRUint32           aLength,
                                             nsBoundingMetrics& aBoundingMetrics,
                                             PRInt32*           aFontID)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    if (aLength <= maxChunkLength)
        return GetBoundingMetricsInternal(aString, aLength, aBoundingMetrics, aFontID);

    if (aFontID) {
        *aFontID = 0;
    }

    PRBool firstIteration = PR_TRUE;
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
        nsBoundingMetrics metrics;
        nsresult rv = GetBoundingMetricsInternal(aString, len, metrics);
        if (NS_FAILED(rv))
            return rv;
        if (firstIteration) {
            // Instead of combining with a Clear()ed nsBoundingMetrics, we
            // assign directly in the first iteration. This ensures that
            // negative ascent/ descent can be returned and the left bearing
            // is properly initialized.
            aBoundingMetrics = metrics;
        } else {
            aBoundingMetrics += metrics;
        }
        aLength -= len;
        aString += len;
        firstIteration = PR_FALSE;
    }
    return NS_OK;
}
#endif

NS_IMETHODIMP
nsThebesRenderingContext::DrawString(const char *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   const nscoord* aSpacing)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
        nsresult rv = DrawStringInternal(aString, len, aX, aY);
        if (NS_FAILED(rv))
            return rv;
        aLength -= len;

        if (aLength > 0) {
            nscoord width;
            rv = GetWidthInternal(aString, len, width);
            if (NS_FAILED(rv))
                return rv;
            aX += width;
            aString += len;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsThebesRenderingContext::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                   nscoord aX, nscoord aY,
                                   PRInt32 aFontID,
                                   const nscoord* aSpacing)
{
    PRUint32 maxChunkLength = GetMaxChunkLength(this);
    if (aLength <= maxChunkLength) {
        return DrawStringInternal(aString, aLength, aX, aY, aFontID, aSpacing);
    }

    PRBool isRTL = PR_FALSE;
    GetRightToLeftText(&isRTL);

    if (isRTL) {
        nscoord totalWidth = 0;
        if (aSpacing) {
            for (PRUint32 i = 0; i < aLength; ++i) {
                totalWidth += aSpacing[i];
            }
        } else {
            nsresult rv = GetWidth(aString, aLength, totalWidth);
            if (NS_FAILED(rv))
                return rv;
        }
        aX += totalWidth;
    }

    while (aLength > 0) {
        PRInt32 len = FindSafeLength(this, aString, aLength, maxChunkLength);
        nscoord width = 0;
        if (aSpacing) {
            for (PRInt32 i = 0; i < len; ++i) {
                width += aSpacing[i];
            }
        } else {
            nsresult rv = GetWidthInternal(aString, len, width);
            if (NS_FAILED(rv))
                return rv;
        }

        if (isRTL) {
            aX -= width;
        }
        nsresult rv = DrawStringInternal(aString, len, aX, aY, aFontID, aSpacing);
        if (NS_FAILED(rv))
            return rv;
        aLength -= len;
        if (!isRTL) {
            aX += width;
        }
        aString += len;
        if (aSpacing) {
            aSpacing += len;
        }
    }
    return NS_OK;
}

nsresult
nsThebesRenderingContext::GetWidthInternal(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
#ifdef DISABLE_TEXT
    aWidth = (8 * aLength);
    return NS_OK;
#endif

    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    return mFontMetrics->GetWidth(aString, aLength, aWidth, this);
}

nsresult
nsThebesRenderingContext::GetWidthInternal(const PRUnichar *aString, PRUint32 aLength,
                                           nscoord &aWidth, PRInt32 *aFontID)
{
#ifdef DISABLE_TEXT
    aWidth = (8 * aLength);
    return NS_OK;
#endif

    if (aLength == 0) {
        aWidth = 0;
        return NS_OK;
    }

    return mFontMetrics->GetWidth(aString, aLength, aWidth, aFontID, this);
}

nsresult
nsThebesRenderingContext::GetTextDimensionsInternal(const char* aString, PRUint32 aLength,
                                                    nsTextDimensions& aDimensions)
{
    mFontMetrics->GetMaxAscent(aDimensions.ascent);
    mFontMetrics->GetMaxDescent(aDimensions.descent);
    return GetWidth(aString, aLength, aDimensions.width);
}

nsresult
nsThebesRenderingContext::GetTextDimensionsInternal(const PRUnichar* aString,
                                                    PRUint32 aLength,
                                                    nsTextDimensions& aDimensions,
                                                    PRInt32* aFontID)
{
    mFontMetrics->GetMaxAscent(aDimensions.ascent);
    mFontMetrics->GetMaxDescent(aDimensions.descent);
    return GetWidth(aString, aLength, aDimensions.width, aFontID);
}

#if defined(_WIN32) || defined(XP_OS2) || defined(MOZ_X11) || defined(XP_BEOS) || defined(XP_MACOSX) || defined (MOZ_DFB)
nsresult
nsThebesRenderingContext::GetTextDimensionsInternal(const char*       aString,
                                                    PRInt32           aLength,
                                                    PRInt32           aAvailWidth,
                                                    PRInt32*          aBreaks,
                                                    PRInt32           aNumBreaks,
                                                    nsTextDimensions& aDimensions,
                                                    PRInt32&          aNumCharsFit,
                                                    nsTextDimensions& aLastWordDimensions,
                                                    PRInt32*          aFontID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsThebesRenderingContext::GetTextDimensionsInternal(const PRUnichar*  aString,
                                                    PRInt32           aLength,
                                                    PRInt32           aAvailWidth,
                                                    PRInt32*          aBreaks,
                                                    PRInt32           aNumBreaks,
                                                    nsTextDimensions& aDimensions,
                                                    PRInt32&          aNumCharsFit,
                                                    nsTextDimensions& aLastWordDimensions,
                                                    PRInt32*          aFontID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

#ifdef MOZ_MATHML
nsresult 
nsThebesRenderingContext::GetBoundingMetricsInternal(const char*        aString,
                                                     PRUint32           aLength,
                                                     nsBoundingMetrics& aBoundingMetrics)
{
    return mFontMetrics->GetBoundingMetrics(aString, aLength, this, aBoundingMetrics);
}

nsresult
nsThebesRenderingContext::GetBoundingMetricsInternal(const PRUnichar*   aString,
                                                     PRUint32           aLength,
                                                     nsBoundingMetrics& aBoundingMetrics,
                                                     PRInt32*           aFontID)
{
    return mFontMetrics->GetBoundingMetrics(aString, aLength, this, aBoundingMetrics);
}
#endif // MOZ_MATHML

nsresult
nsThebesRenderingContext::DrawStringInternal(const char *aString, PRUint32 aLength,
                                             nscoord aX, nscoord aY,
                                             const nscoord* aSpacing)
{
#ifdef DISABLE_TEXT
    return NS_OK;
#endif

    return mFontMetrics->DrawString(aString, aLength, aX, aY, aSpacing,
                                    this);
}

nsresult
nsThebesRenderingContext::DrawStringInternal(const PRUnichar *aString, PRUint32 aLength,
                                             nscoord aX, nscoord aY,
                                             PRInt32 aFontID,
                                             const nscoord* aSpacing)
{
#ifdef DISABLE_TEXT
    return NS_OK;
#endif

    return mFontMetrics->DrawString(aString, aLength, aX, aY, aFontID,
                                    aSpacing, this);
}

PRInt32
nsThebesRenderingContext::GetPosition(const PRUnichar *aText,
                                      PRUint32 aLength,
                                      nsPoint aPt)
{
  return -1;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetRangeWidth(const PRUnichar *aText,
                                        PRUint32 aLength,
                                        PRUint32 aStart,
                                        PRUint32 aEnd,
                                        PRUint32 &aWidth)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThebesRenderingContext::GetRangeWidth(const char *aText,
                                        PRUint32 aLength,
                                        PRUint32 aStart,
                                        PRUint32 aEnd,
                                        PRUint32 &aWidth)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsThebesRenderingContext::RenderEPS(const nsRect& aRect, FILE *aDataFile)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

