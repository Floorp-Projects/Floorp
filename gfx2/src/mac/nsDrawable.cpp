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

#include "nsCOMPtr.h"

NS_IMPL_ISUPPORTS1(nsDrawable, nsIDrawable)

nsDrawable::nsDrawable() :
  mBounds(0,0,0,0),
  mDepth(0),

  mLineWidth(0),
  mForegroundColor(0),
  mBackgroundColor(0),
  mClipOrigin(0, 0)
{
  NS_INIT_ISUPPORTS();
}

nsDrawable::~nsDrawable()
{
}

NS_IMETHODIMP nsDrawable::GetSize(gfx_width *aWidth, gfx_height *aHeight)
{
  *aWidth = mBounds.width;
  *aHeight = mBounds.height;
  return NS_OK;
}

NS_IMETHODIMP nsDrawable::GetWidth(gfx_width *aWidth)
{
  *aWidth = mBounds.width;
  return NS_OK;
}

NS_IMETHODIMP nsDrawable::GetHeight(gfx_height *aHeight)
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
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::DrawLine(gfx_coord aX1, gfx_coord aY1,
                                   gfx_coord aX2, gfx_coord aY2)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::DrawRectangle(gfx_coord x, gfx_coord y,
                                        gfx_width width, gfx_height height)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::FillRectangle(gfx_coord x, gfx_coord y,
                                        gfx_width width, gfx_height height)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::DrawEllipse(gfx_coord x, gfx_coord y,
                                      gfx_width width, gfx_height height)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::FillEllipse(gfx_coord x, gfx_coord y,
                                      gfx_width width, gfx_height height)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::DrawArc(gfx_coord x, gfx_coord y,
                                  gfx_width width, gfx_height height,
                                  PRInt32 angle1, PRInt32 angle2)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::FillArc(gfx_coord x, gfx_coord y,
                                  gfx_width width, gfx_height height,
                                  PRInt32 angle1, PRInt32 angle2)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::DrawPolygon(const nsPoint **points,
                                      const PRUint32 npoints)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::FillPolygon(const nsPoint **points,
                                      const PRUint32 npoints)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
                                 gfx_width width, gfx_height height)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::ClearArea(gfx_coord x,
                                    gfx_coord y,
                                    gfx_width width,
                                    gfx_height height)
{
  Rect			macRect;
  GrafPtr 	savePort;

  ::SetRect(&macRect, x, y, width, height);

	::GetPort(&savePort);
	::SetPort(mGrafPtr);
	::EraseRect(&macRect);
	::SetPort(savePort);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsDrawable::Clear()
{
  Rect			macRect;
  GrafPtr 	savePort;

  ::SetRect(&macRect, 0, 0, mBounds.width, mBounds.height);

	::GetPort(&savePort);
	::SetPort(mGrafPtr);
	::EraseRect(&macRect);
	::SetPort(savePort);

  return NS_OK;
}

/* [noscript] void drawImage (in nsIImage aImage, in nsIImage aMask, [const] in nsRect aSrcRect, [const] in nsRect aDestRect); */
NS_IMETHODIMP nsDrawable::DrawImage(nsIImage *aImage, nsIImage *aMask, const nsRect * aSrcRect, const nsRect * aDestRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void drawTile (in nsIImage aImage, in nsIImage aMask, in gfx_coord aX0, in gfx_coord aY0, in gfx_coord aX1, in gfx_coord aY1, in gfx_width width, in gfx_height height); */
NS_IMETHODIMP nsDrawable::DrawTile(nsIImage *aImage,
                                   nsIImage *aMask,
                                   gfx_coord aX0,
                                   gfx_coord aY0,
                                   gfx_coord aX1,
                                   gfx_coord aY1,
                                   gfx_width width,
                                   gfx_height height)
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

/* void getClipRectangle (out gfx_coord x, out gfx_coord y, out gfx_width width, out gfx_height height); */
NS_IMETHODIMP nsDrawable::GetClipRectangle(gfx_coord *x, gfx_coord *y, gfx_width *width, gfx_height *height)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setClipRectangle (in gfx_coord x, in gfx_coord y, in gfx_width width, in gfx_height height); */
NS_IMETHODIMP nsDrawable::SetClipRectangle(gfx_coord x, gfx_coord y, gfx_width width, gfx_height height)
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

/* [noscript] void changeClipRectangle ([const] in nsRect rect, in short clipOperation); */
NS_IMETHODIMP nsDrawable::ChangeClipRectangle(const nsRect * rect, PRInt16 clipOperation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void changeClipRegion (in nsIRegion aRegion, in short clipOperation); */
NS_IMETHODIMP nsDrawable::ChangeClipRegion(nsIRegion *aRegion, PRInt16 clipOperation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}
