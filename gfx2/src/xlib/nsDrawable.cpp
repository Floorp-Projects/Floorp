/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsDrawable.h"

#include "nsIComponentManager.h"

#include "nsGCCache.h"

#include "nsCOMPtr.h"

nsGCCache *nsDrawable::sGCCache = new nsGCCache();

NS_IMPL_ISUPPORTS1(nsDrawable, nsIDrawable)

nsDrawable::nsDrawable() :
  mDisplay(nsnull),
  mGC(nsnull),
  mBounds(0.0,0.0,0.0,0.0),
  mDepth(0),
  mDrawable(0),

  mLineWidth(0.0),
  mForegroundColor(0),
  mBackgroundColor(0),
  mClipOrigin(0.0, 0.0)
{
  NS_INIT_ISUPPORTS();
}

nsDrawable::~nsDrawable()
{
}

NS_IMETHODIMP nsDrawable::GetSize(gfx_dimension *aWidth, gfx_dimension *aHeight)
{
  *aWidth = mBounds.width;
  *aHeight = mBounds.height;
  return NS_OK;
}

NS_IMETHODIMP nsDrawable::GetWidth(gfx_dimension *aWidth)
{
  *aWidth = mBounds.width;
  return NS_OK;
}

NS_IMETHODIMP nsDrawable::GetHeight(gfx_dimension *aHeight)
{
  *aHeight = mBounds.height;
  return NS_OK;
}

NS_IMETHODIMP nsDrawable::GetDepth(gfx_depth *aDepth)
{
  *aDepth = mDepth;
  return NS_OK;
}

NS_IMETHODIMP nsDrawable::DrawPoint(gfx_coord x, gfx_coord y)
{
  UpdateGC();

  ::XDrawPoint(mDisplay, mDrawable, *mGC,
               GFXCoordToIntRound(x),
               GFXCoordToIntRound(y));

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::DrawLine(gfx_coord aX1, gfx_coord aY1,
                                   gfx_coord aX2, gfx_coord aY2)
{
  UpdateGC();

  ::XDrawLine(mDisplay, mDrawable, *mGC,
              GFXCoordToIntRound(aX1),
              GFXCoordToIntRound(aY1),
              GFXCoordToIntRound(aX2),
              GFXCoordToIntRound(aY2));

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::DrawRectangle(gfx_coord x, gfx_coord y,
                                        gfx_dimension width, gfx_dimension height)
{
  UpdateGC();

  ::XDrawRectangle(mDisplay, mDrawable, *mGC,
                   GFXCoordToIntRound(x),
                   GFXCoordToIntRound(y),
                   GFXCoordToIntRound(width),
                   GFXCoordToIntRound(height));

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::FillRectangle(gfx_coord x, gfx_coord y,
                                        gfx_dimension width, gfx_dimension height)
{
  UpdateGC();

    ::XFillRectangle(mDisplay, mDrawable, *mGC,
                     GFXCoordToIntRound(x),
                     GFXCoordToIntRound(y),
                     GFXCoordToIntRound(width),
                     GFXCoordToIntRound(height));

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::DrawEllipse(gfx_coord x, gfx_coord y,
                                      gfx_dimension width, gfx_dimension height)
{
  UpdateGC();

  ::XDrawArc(mDisplay, mDrawable, *mGC,
             GFXCoordToIntRound(x),
             GFXCoordToIntRound(y),
             GFXCoordToIntRound(width),
             GFXCoordToIntRound(height),
             0, 360 * 64);

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::FillEllipse(gfx_coord x, gfx_coord y,
                                      gfx_dimension width, gfx_dimension height)
{
  UpdateGC();

  ::XFillArc(mDisplay, mDrawable, *mGC,
             GFXCoordToIntRound(x),
             GFXCoordToIntRound(y),
             GFXCoordToIntRound(width),
             GFXCoordToIntRound(height),
             0, 360 * 64);

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::DrawArc(gfx_coord x, gfx_coord y,
                                  gfx_dimension width, gfx_dimension height,
                                  gfx_angle angle1, gfx_angle angle2)
{
  UpdateGC();

    ::XDrawArc(mDisplay, mDrawable, *mGC,
               GFXCoordToIntRound(x),
               GFXCoordToIntRound(y),
               GFXCoordToIntRound(width),
               GFXCoordToIntRound(height),
               GFXAngleToIntRound(angle1),
               GFXAngleToIntRound(angle2));

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::FillArc(gfx_coord x, gfx_coord y,
                                  gfx_dimension width, gfx_dimension height,
                                  gfx_angle angle1, gfx_angle angle2)
{
  UpdateGC();

  ::XFillArc(mDisplay, mDrawable, *mGC,
             GFXCoordToIntRound(x),
             GFXCoordToIntRound(y),
             GFXCoordToIntRound(width),
             GFXCoordToIntRound(height),
             GFXAngleToIntRound(angle1), GFXAngleToIntRound(angle2));

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::DrawPolygon(const nsPoint2 **points,
                                      const PRUint32 npoints)
{
  UpdateGC();

  ::XDrawLines(mDisplay, mDrawable,
               *mGC,
               NS_REINTERPRET_CAST(XPoint*, NS_CONST_CAST(nsPoint2*,*points)),
               npoints,
               CoordModeOrigin);

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::FillPolygon(const nsPoint2 **points,
                                      const PRUint32 npoints)
{
  UpdateGC();

  ::XFillPolygon(mDisplay, mDrawable,
                 *mGC,
                 NS_REINTERPRET_CAST(XPoint*, NS_CONST_CAST(nsPoint2*,*points)),
                 npoints,
                 Complex, CoordModeOrigin);

  return NS_OK;
}

NS_IMETHODIMP nsDrawable::DrawString(const PRUnichar *aText,
                                     PRUint32 aLength,
                                     gfx_coord aX, gfx_coord aY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::DrawCString(const char *aText,
                                      PRUint32 aLength,
                                      gfx_coord aX, gfx_coord aY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::CopyTo(nsIDrawable *aDest,
                                 gfx_coord xsrc, gfx_coord ysrc,
                                 gfx_coord xdest, gfx_coord ydest,
                                 gfx_dimension width, gfx_dimension height)
{
  UpdateGC();

  // XXX should you be able to copy to another nsIDrawable that isn't this real class?
  Drawable dest = NS_STATIC_CAST(nsDrawable*, aDest)->mDrawable;

  ::XCopyArea(mDisplay,
              mDrawable, dest,
              *mGC,
              GFXCoordToIntRound(xsrc),
              GFXCoordToIntRound(ysrc),
              GFXCoordToIntRound(xdest),
              GFXCoordToIntRound(ydest),
              GFXCoordToIntRound(width),
              GFXCoordToIntRound(height));
  
  return NS_OK;
}

NS_IMETHODIMP nsDrawable::ClearArea(gfx_coord x,
                                    gfx_coord y,
                                    gfx_dimension width,
                                    gfx_dimension height)
{
  ::XClearArea(mDisplay, (Window)mDrawable,
               GFXCoordToIntRound(x),
               GFXCoordToIntRound(y),
               GFXCoordToIntRound(width),
               GFXCoordToIntRound(height),
               False);
  return NS_OK;
}

NS_IMETHODIMP nsDrawable::Clear()
{
  ::XClearWindow(mDisplay, (Window)mDrawable);
  return NS_OK;
}

/* [noscript] void drawImage (in nsIImage aImage, [const] in nsRect2 aSrcRect, [const] in nsPoint2 aDestPoint); */
NS_IMETHODIMP nsDrawable::DrawImage(nsIImage *aImage, const nsRect2 * aSrcRect, const nsPoint2 * aDestPoint)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void drawScaledImage (in nsIImage aImage, [const] in nsRect2 aSrcRect, [const] in nsRect2 aDestRect); */
NS_IMETHODIMP nsDrawable::DrawScaledImage(nsIImage *aImage, const nsRect2 * aSrcRect, const nsRect2 * aDestRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void drawTile (in nsIImage aImage, in gfx_coord aXOffset, in gfx_coord aYOffset, [const] in nsRect2 aTargetRect); */
NS_IMETHODIMP nsDrawable::DrawTile(nsIImage *aImage, gfx_coord aXOffset, gfx_coord aYOffset, const nsRect2 * aTargetRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] void drawScaledTile (in nsIImage aImage, in gfx_coord aXOffset, in gfx_coord aYOffset, in gfx_dimension aTileWidth, in gfx_dimension aTileHeight, [const] in nsRect2 aTargetRect); */
NS_IMETHODIMP nsDrawable::DrawScaledTile(nsIImage *aImage, gfx_coord aXOffset, gfx_coord aYOffset, gfx_dimension aTileWidth, gfx_dimension aTileHeight, const nsRect2 * aTargetRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}



/* attribute gfx_coord lineWidth; */
NS_IMETHODIMP nsDrawable::GetLineWidth(gfx_coord *aLineWidth)
{
  *aLineWidth = mLineWidth;
  return NS_OK;
}
NS_IMETHODIMP nsDrawable::SetLineWidth(gfx_coord aLineWidth)
{
  mLineWidth = aLineWidth;
  return NS_OK;
}

/* attribute gfx_color foregroundColor; */
NS_IMETHODIMP nsDrawable::GetForegroundColor(gfx_color *aForegroundColor)
{
  *aForegroundColor = mForegroundColor;
  return NS_OK;
}
NS_IMETHODIMP nsDrawable::SetForegroundColor(gfx_color aForegroundColor)
{
  mForegroundColor = aForegroundColor;
  return NS_OK;
}

/* attribute gfx_color backgroundColor; */
NS_IMETHODIMP nsDrawable::GetBackgroundColor(gfx_color *aBackgroundColor)
{
  *aBackgroundColor = mBackgroundColor;
  return NS_OK;
}
NS_IMETHODIMP nsDrawable::SetBackgroundColor(gfx_color aBackgroundColor)
{
  mBackgroundColor = aBackgroundColor;
  return NS_OK;
}

/* [noscript] void setFont ([const] in nsFont font); */
NS_IMETHODIMP nsDrawable::SetFont(const nsFont * font)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* [noscript] nsFont getFont (); */
NS_IMETHODIMP nsDrawable::GetFont(nsFont * *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsIFontMetrics fontMetrics; */
NS_IMETHODIMP nsDrawable::GetFontMetrics(nsIFontMetrics * *aFont)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDrawable::SetFontMetrics(nsIFontMetrics * aFont)
{
  mFontMetrics = aFont;
  return NS_OK;
}

/* void getClipOrigin (out gfx_coord x, out gfx_coord y); */
NS_IMETHODIMP nsDrawable::GetClipOrigin(gfx_coord *x, gfx_coord *y)
{
  *x = mClipOrigin.x;
  *y = mClipOrigin.y;
  return NS_OK;
}

/* void setClipOrigin (in gfx_coord x, in gfx_coord y); */
NS_IMETHODIMP nsDrawable::SetClipOrigin(gfx_coord x, gfx_coord y)
{
  mClipOrigin.MoveTo(x, y);
  return NS_OK;
}

/* void getClipRectangle (out gfx_coord x, out gfx_coord y, out gfx_dimension width, out gfx_dimension height); */
NS_IMETHODIMP nsDrawable::GetClipRectangle(gfx_coord *x, gfx_coord *y, gfx_dimension *width, gfx_dimension *height)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setClipRectangle (in gfx_coord x, in gfx_coord y, in gfx_dimension width, in gfx_dimension height); */
NS_IMETHODIMP nsDrawable::SetClipRectangle(gfx_coord x, gfx_coord y, gfx_dimension width, gfx_dimension height)
{
  if (!mClipRegion) {
    nsresult rv;
    mClipRegion = do_CreateInstance("mozilla.gfx.region.2", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return mClipRegion->SetToRect(x, y, width, height);
}

/* attribute nsIRegion clipRegion; */
NS_IMETHODIMP nsDrawable::GetClipRegion(nsIRegion * *aClipRegion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP nsDrawable::SetClipRegion(nsIRegion * aClipRegion)
{
  if (!mClipRegion) {
    nsresult rv;
    mClipRegion = do_CreateInstance("mozilla.gfx.region.2", &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return mClipRegion->SetToRegion(aClipRegion);
}

/* [noscript] void changeClipRectangle ([const] in nsRect2 rect, in short clipOperation); */
NS_IMETHODIMP nsDrawable::ChangeClipRectangle(const nsRect2 * rect, PRInt16 clipOperation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void changeClipRegion (in nsIRegion aRegion, in short clipOperation); */
NS_IMETHODIMP nsDrawable::ChangeClipRegion(nsIRegion *aRegion, PRInt16 clipOperation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}




/* local helper methods */
void nsDrawable::UpdateGC()
{
  XGCValues values;
  unsigned long valuesMask;

  if (mGC)
    mGC->Release();

  memset(&values, 0, sizeof(XGCValues));

  // values.foreground.pixel = gdk_rgb_xpixel_from_rgb(NS_TO_GDK_RGB(mCurrentColor));
  values.foreground = mForegroundColor;
  valuesMask = GCForeground;

  values.foreground = mBackgroundColor;
  valuesMask = GCBackground;


#if 0
  if (mCurrentFont) {
    valuesMask |= GCFont;
    values.font = mFont;
  }
#endif

#if 0
  valuesMask |= GCLineStyle;
  values.line_style = mLineStyle;
#endif

#if 0
  valuesMask = GdkGCValuesMask(valuesMask | GDK_GC_FUNCTION);
  values.function = mFunction;
#endif

  valuesMask |= GCLineWidth;
  values.line_width = GFXCoordToIntRound(mLineWidth);

  Region rgn = 0;
#if 0
  if (mClipRegion) {
    rgn = mClipRegion->mRegion;
  }
#endif

  mGC = sGCCache->GetGC(mDisplay,
                        mDrawable,
                        valuesMask,
                        &values,
                        rgn);
}
