/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsRenderingContext.h"
#include "nsBoundingMetrics.h"
#include "nsRegion.h"

// XXXTodo: rename FORM_TWIPS to FROM_APPUNITS
#define FROM_TWIPS(_x)  ((gfxFloat)((_x)/(mP2A)))
#define FROM_TWIPS_INT(_x)  (NSToIntRound((gfxFloat)((_x)/(mP2A))))
#define TO_TWIPS(_x)    ((nscoord)((_x)*(mP2A)))
#define GFX_RECT_FROM_TWIPS_RECT(_r)   (gfxRect(FROM_TWIPS((_r).x), FROM_TWIPS((_r).y), FROM_TWIPS((_r).width), FROM_TWIPS((_r).height)))

// Hard limit substring lengths to 8000 characters ... this lets us statically
// size the cluster buffer array in FindSafeLength
#define MAX_GFX_TEXT_BUF_SIZE 8000

static PRInt32 FindSafeLength(const PRUnichar *aString, PRUint32 aLength,
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

static PRInt32 FindSafeLength(const char *aString, PRUint32 aLength,
                              PRUint32 aMaxChunkLength)
{
    // Since it's ASCII, we don't need to worry about clusters or RTL
    return NS_MIN(aLength, aMaxChunkLength);
}

//////////////////////////////////////////////////////////////////////
//// nsRenderingContext

void
nsRenderingContext::Init(nsDeviceContext* aContext,
                         gfxASurface *aThebesSurface)
{
    Init(aContext, new gfxContext(aThebesSurface));
}

void
nsRenderingContext::Init(nsDeviceContext* aContext,
                         gfxContext *aThebesContext)
{
    mDeviceContext = aContext;
    mThebes = aThebesContext;

    mThebes->SetLineWidth(1.0);
    mP2A = mDeviceContext->AppUnitsPerDevPixel();
}

//
// graphics state
//

void
nsRenderingContext::PushState()
{
    mThebes->Save();
}

void
nsRenderingContext::PopState()
{
    mThebes->Restore();
}

void
nsRenderingContext::IntersectClip(const nsRect& aRect)
{
    mThebes->NewPath();
    gfxRect clipRect(GFX_RECT_FROM_TWIPS_RECT(aRect));
    if (mThebes->UserToDevicePixelSnapped(clipRect, true)) {
        gfxMatrix mat(mThebes->CurrentMatrix());
        mat.Invert();
        clipRect = mat.Transform(clipRect);
        mThebes->Rectangle(clipRect);
    } else {
        mThebes->Rectangle(clipRect);
    }

    mThebes->Clip();
}

void
nsRenderingContext::SetClip(const nsIntRegion& aRegion)
{
    // Region is in device coords, no transformation.  This should
    // only be called when there is no transform in place, when we we
    // just start painting a widget. The region is set by the platform
    // paint routine.  Therefore, there is no option to intersect with
    // an existing clip.

    gfxMatrix mat = mThebes->CurrentMatrix();
    mThebes->IdentityMatrix();

    mThebes->ResetClip();

    mThebes->NewPath();
    nsIntRegionRectIterator iter(aRegion);
    const nsIntRect* rect;
    while ((rect = iter.Next())) {
        mThebes->Rectangle(gfxRect(rect->x, rect->y, rect->width, rect->height),
                           true);
    }
    mThebes->Clip();
    mThebes->SetMatrix(mat);
}

void
nsRenderingContext::SetLineStyle(nsLineStyle aLineStyle)
{
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
}


void
nsRenderingContext::SetColor(nscolor aColor)
{
    /* This sets the color assuming the sRGB color space, since that's
     * what all CSS colors are defined to be in by the spec.
     */
    mThebes->SetColor(gfxRGBA(aColor));
}

void
nsRenderingContext::Translate(const nsPoint& aPt)
{
    mThebes->Translate(gfxPoint(FROM_TWIPS(aPt.x), FROM_TWIPS(aPt.y)));
}

void
nsRenderingContext::Scale(float aSx, float aSy)
{
    mThebes->Scale(aSx, aSy);
}

//
// shapes
//

void
nsRenderingContext::DrawLine(const nsPoint& aStartPt, const nsPoint& aEndPt)
{
    DrawLine(aStartPt.x, aStartPt.y, aEndPt.x, aEndPt.y);
}

void
nsRenderingContext::DrawLine(nscoord aX0, nscoord aY0,
                             nscoord aX1, nscoord aY1)
{
    gfxPoint p0 = gfxPoint(FROM_TWIPS(aX0), FROM_TWIPS(aY0));
    gfxPoint p1 = gfxPoint(FROM_TWIPS(aX1), FROM_TWIPS(aY1));

    // we can't draw thick lines with gfx, so we always assume we want
    // pixel-aligned lines if the rendering context is at 1.0 scale
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
}

void
nsRenderingContext::DrawRect(const nsRect& aRect)
{
    mThebes->NewPath();
    mThebes->Rectangle(GFX_RECT_FROM_TWIPS_RECT(aRect), true);
    mThebes->Stroke();
}

void
nsRenderingContext::DrawRect(nscoord aX, nscoord aY,
                             nscoord aWidth, nscoord aHeight)
{
    DrawRect(nsRect(aX, aY, aWidth, aHeight));
}


/* Clamp r to (0,0) (2^23,2^23)
 * these are to be device coordinates.
 *
 * Returns false if the rectangle is completely out of bounds,
 * true otherwise.
 *
 * This function assumes that it will be called with a rectangle being
 * drawn into a surface with an identity transformation matrix; that
 * is, anything above or to the left of (0,0) will be offscreen.
 *
 * First it checks if the rectangle is entirely beyond
 * CAIRO_COORD_MAX; if so, it can't ever appear on the screen --
 * false is returned.
 *
 * Then it shifts any rectangles with x/y < 0 so that x and y are = 0,
 * and adjusts the width and height appropriately.  For example, a
 * rectangle from (0,-5) with dimensions (5,10) will become a
 * rectangle from (0,0) with dimensions (5,5).
 *
 * If after negative x/y adjustment to 0, either the width or height
 * is negative, then the rectangle is completely offscreen, and
 * nothing is drawn -- false is returned.
 *
 * Finally, if x+width or y+height are greater than CAIRO_COORD_MAX,
 * the width and height are clamped such x+width or y+height are equal
 * to CAIRO_COORD_MAX, and true is returned.
 */
#define CAIRO_COORD_MAX (double(0x7fffff))

static bool
ConditionRect(gfxRect& r) {
    // if either x or y is way out of bounds;
    // note that we don't handle negative w/h here
    if (r.X() > CAIRO_COORD_MAX || r.Y() > CAIRO_COORD_MAX)
        return false;

    if (r.X() < 0.0) {
        r.width += r.X();
        if (r.width < 0.0)
            return false;
        r.x = 0.0;
    }

    if (r.XMost() > CAIRO_COORD_MAX) {
        r.width = CAIRO_COORD_MAX - r.X();
    }

    if (r.Y() < 0.0) {
        r.height += r.Y();
        if (r.Height() < 0.0)
            return false;

        r.y = 0.0;
    }

    if (r.YMost() > CAIRO_COORD_MAX) {
        r.height = CAIRO_COORD_MAX - r.Y();
    }
    return true;
}

void
nsRenderingContext::FillRect(const nsRect& aRect)
{
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
            return;

        mThebes->IdentityMatrix();
        mThebes->NewPath();

        mThebes->Rectangle(r, true);
        mThebes->Fill();
        mThebes->SetMatrix(mat);
    }

    mThebes->NewPath();
    mThebes->Rectangle(r, true);
    mThebes->Fill();
}

void
nsRenderingContext::FillRect(nscoord aX, nscoord aY,
                             nscoord aWidth, nscoord aHeight)
{
    FillRect(nsRect(aX, aY, aWidth, aHeight));
}

void
nsRenderingContext::InvertRect(const nsRect& aRect)
{
    gfxContext::GraphicsOperator lastOp = mThebes->CurrentOperator();

    mThebes->SetOperator(gfxContext::OPERATOR_XOR);
    FillRect(aRect);
    mThebes->SetOperator(lastOp);
}

void
nsRenderingContext::DrawEllipse(nscoord aX, nscoord aY,
                                nscoord aWidth, nscoord aHeight)
{
    mThebes->NewPath();
    mThebes->Ellipse(gfxPoint(FROM_TWIPS(aX) + FROM_TWIPS(aWidth)/2.0,
                              FROM_TWIPS(aY) + FROM_TWIPS(aHeight)/2.0),
                     gfxSize(FROM_TWIPS(aWidth),
                             FROM_TWIPS(aHeight)));
    mThebes->Stroke();
}

void
nsRenderingContext::FillEllipse(const nsRect& aRect)
{
    FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

void
nsRenderingContext::FillEllipse(nscoord aX, nscoord aY,
                                nscoord aWidth, nscoord aHeight)
{
    mThebes->NewPath();
    mThebes->Ellipse(gfxPoint(FROM_TWIPS(aX) + FROM_TWIPS(aWidth)/2.0,
                              FROM_TWIPS(aY) + FROM_TWIPS(aHeight)/2.0),
                     gfxSize(FROM_TWIPS(aWidth),
                             FROM_TWIPS(aHeight)));
    mThebes->Fill();
}

void
nsRenderingContext::FillPolygon(const nsPoint twPoints[], PRInt32 aNumPoints)
{
    if (aNumPoints == 0)
        return;

    nsAutoArrayPtr<gfxPoint> pxPoints(new gfxPoint[aNumPoints]);

    for (int i = 0; i < aNumPoints; i++) {
        pxPoints[i].x = FROM_TWIPS(twPoints[i].x);
        pxPoints[i].y = FROM_TWIPS(twPoints[i].y);
    }

    mThebes->NewPath();
    mThebes->Polygon(pxPoints, aNumPoints);
    mThebes->Fill();
}

//
// text
//

void
nsRenderingContext::SetTextRunRTL(bool aIsRTL)
{
    mFontMetrics->SetTextRunRTL(aIsRTL);
}

void
nsRenderingContext::SetFont(nsFontMetrics *aFontMetrics)
{
    mFontMetrics = aFontMetrics;
}

PRInt32
nsRenderingContext::GetMaxChunkLength()
{
    if (!mFontMetrics)
        return 1;
    return NS_MIN(mFontMetrics->GetMaxStringLength(), MAX_GFX_TEXT_BUF_SIZE);
}

nscoord
nsRenderingContext::GetWidth(char aC)
{
    if (aC == ' ' && mFontMetrics) {
        return mFontMetrics->SpaceWidth();
    }

    return GetWidth(&aC, 1);
}

nscoord
nsRenderingContext::GetWidth(PRUnichar aC)
{
    return GetWidth(&aC, 1);
}

nscoord
nsRenderingContext::GetWidth(const nsString& aString)
{
    return GetWidth(aString.get(), aString.Length());
}

nscoord
nsRenderingContext::GetWidth(const char* aString)
{
    return GetWidth(aString, strlen(aString));
}

nscoord
nsRenderingContext::GetWidth(const char* aString, PRUint32 aLength)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    nscoord width = 0;
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
        width += mFontMetrics->GetWidth(aString, len, this);
        aLength -= len;
        aString += len;
    }
    return width;
}

nscoord
nsRenderingContext::GetWidth(const PRUnichar *aString, PRUint32 aLength)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    nscoord width = 0;
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
        width += mFontMetrics->GetWidth(aString, len, this);
        aLength -= len;
        aString += len;
    }
    return width;
}

nsBoundingMetrics
nsRenderingContext::GetBoundingMetrics(const PRUnichar* aString,
                                       PRUint32 aLength)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
    // Assign directly in the first iteration. This ensures that
    // negative ascent/descent can be returned and the left bearing
    // is properly initialized.
    nsBoundingMetrics totalMetrics
        = mFontMetrics->GetBoundingMetrics(aString, len, this);
    aLength -= len;
    aString += len;

    while (aLength > 0) {
        len = FindSafeLength(aString, aLength, maxChunkLength);
        nsBoundingMetrics metrics
            = mFontMetrics->GetBoundingMetrics(aString, len, this);
        totalMetrics += metrics;
        aLength -= len;
        aString += len;
    }
    return totalMetrics;
}

void
nsRenderingContext::DrawString(const char *aString, PRUint32 aLength,
                               nscoord aX, nscoord aY)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    while (aLength > 0) {
        PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
        mFontMetrics->DrawString(aString, len, aX, aY, this);
        aLength -= len;

        if (aLength > 0) {
            nscoord width = mFontMetrics->GetWidth(aString, len, this);
            aX += width;
            aString += len;
        }
    }
}

void
nsRenderingContext::DrawString(const nsString& aString, nscoord aX, nscoord aY)
{
    DrawString(aString.get(), aString.Length(), aX, aY);
}

void
nsRenderingContext::DrawString(const PRUnichar *aString, PRUint32 aLength,
                               nscoord aX, nscoord aY)
{
    PRUint32 maxChunkLength = GetMaxChunkLength();
    if (aLength <= maxChunkLength) {
        mFontMetrics->DrawString(aString, aLength, aX, aY, this, this);
        return;
    }

    bool isRTL = mFontMetrics->GetTextRunRTL();

    // If we're drawing right to left, we must start at the end.
    if (isRTL) {
        aX += GetWidth(aString, aLength);
    }

    while (aLength > 0) {
        PRInt32 len = FindSafeLength(aString, aLength, maxChunkLength);
        nscoord width = mFontMetrics->GetWidth(aString, len, this);
        if (isRTL) {
            aX -= width;
        }
        mFontMetrics->DrawString(aString, len, aX, aY, this, this);
        if (!isRTL) {
            aX += width;
        }
        aLength -= len;
        aString += len;
    }
}
