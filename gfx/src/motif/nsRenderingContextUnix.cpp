/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsRenderingContextUnix.h"
#include <math.h>

typedef unsigned char BYTE;

#define RGB(r,g,b) ((unsigned long) (((BYTE) (r) | ((unsigned long) ((BYTE) (g)) <<8)) | (((unsigned long)(BYTE)(b)) << 16)))

static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

nsRenderingContextUnix :: nsRenderingContextUnix()
{
  NS_INIT_REFCNT();

  mFontCache = nsnull ;
  mFontMetrics = nsnull ;
  mContext = nsnull ;
  mRenderingSurface = nsnull ;
  mOffscreenSurface = nsnull ;
}

nsRenderingContextUnix :: ~nsRenderingContextUnix()
{
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mFontCache);
  NS_IF_RELEASE(mFontMetrics);
}

NS_IMPL_QUERY_INTERFACE(nsRenderingContextUnix, kRenderingContextIID)
NS_IMPL_ADDREF(nsRenderingContextUnix)
NS_IMPL_RELEASE(nsRenderingContextUnix)

nsresult nsRenderingContextUnix :: Init(nsIDeviceContext* aContext,
					nsIWidget *aWindow)
{

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mRenderingSurface = new nsDrawingSurfaceUnix();

  mRenderingSurface->display =  XtDisplay((Widget)aWindow->GetNativeData(NS_NATIVE_WIDGET));
  mRenderingSurface->drawable = (Drawable)aWindow->GetNativeData(NS_NATIVE_WINDOW);
  mRenderingSurface->gc       = (GC)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);

  ((nsDeviceContextUnix *)aContext)->SetDrawingSurface(mRenderingSurface);
  ((nsDeviceContextUnix *)aContext)->InstallColormap();

#if 0
  mFontCache = mContext->GetFontCache();
#endif 
}

nsresult nsRenderingContextUnix :: Init(nsIDeviceContext* aContext,
					nsDrawingSurface aSurface)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mRenderingSurface = (nsDrawingSurfaceUnix *) aSurface;
}

nsresult nsRenderingContextUnix :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  mOffscreenSurface = mRenderingSurface;
  mRenderingSurface = (nsDrawingSurfaceUnix *) aSurface;
  return NS_OK;
}

void nsRenderingContextUnix :: Reset()
{
}

nsIDeviceContext * nsRenderingContextUnix :: GetDeviceContext(void)
{
  return mContext;
}

void nsRenderingContextUnix :: PushState(void)
{
}

void nsRenderingContextUnix :: PopState(void)
{
}

PRBool nsRenderingContextUnix :: IsVisibleRect(const nsRect& aRect)
{
  return PR_TRUE;
}

void nsRenderingContextUnix :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine)
{
  nsRect  trect = aRect;

	mTMatrix->TransformCoord(&trect.x, &trect.y,
                           &trect.width, &trect.height);

  //how we combine the new rect with the previous?

  if (aCombine == nsClipCombine_kIntersect)
  {
  }
  else if (aCombine == nsClipCombine_kUnion)
  {
  }
  else if (aCombine == nsClipCombine_kSubtract)
  {
  }
  else if (aCombine == nsClipCombine_kReplace)
  {
  }
  else
    NS_ASSERTION(FALSE, "illegal clip combination");
}

PRBool nsRenderingContextUnix :: GetClipRect(nsRect &aRect)
{
  return PR_FALSE;
}

void nsRenderingContextUnix :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine)
{
  //XXX wow, needs to do something.
}

void nsRenderingContextUnix :: GetClipRegion(nsIRegion **aRegion)
{
  //XXX wow, needs to do something.
}

void nsRenderingContextUnix :: SetColor(nscolor aColor)
{
  XGCValues values ;

  mCurrentColor = aColor ;

  values.foreground = mCurrentColor;
  values.background = mCurrentColor;

  XChangeGC(mRenderingSurface->display,
	    mRenderingSurface->gc,
	    GCForeground | GCBackground,
	    &values);
}

nscolor nsRenderingContextUnix :: GetColor() const
{
  return mCurrentColor;
}

void nsRenderingContextUnix :: SetFont(const nsFont& aFont)
{
  Font id = ::XLoadFont(mRenderingSurface->display, "fixed");

  XFontStruct * fs = ::XQueryFont(mRenderingSurface->display, id);

  ::XSetFont(mRenderingSurface->display, mRenderingSurface->gc, id);


#if 0
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = mFontCache->GetMetricsFor(aFont);
#endif

}

const nsFont& nsRenderingContextUnix :: GetFont()
{
  return mFontMetrics->GetFont();
}

nsIFontMetrics* nsRenderingContextUnix :: GetFontMetrics()
{
  return mFontMetrics;
}

// add the passed in translation to the current translation
void nsRenderingContextUnix :: Translate(nscoord aX, nscoord aY)
{
}

// add the passed in scale to the current scale
void nsRenderingContextUnix :: Scale(float aSx, float aSy)
{
}

nsTransform2D * nsRenderingContextUnix :: GetCurrentTransform()
{
  return nsnull ;
}

nsDrawingSurface nsRenderingContextUnix :: CreateDrawingSurface(nsRect *aBounds)
{
  // Must make sure this code never gets called when nsRenderingSurface is nsnull
  PRUint32 depth = DefaultDepth(mRenderingSurface->display,
				DefaultScreen(mRenderingSurface->display));

  Pixmap p = ::XCreatePixmap(mRenderingSurface->display,
			     mRenderingSurface->drawable,
			     aBounds->width, aBounds->height, depth);

  nsDrawingSurfaceUnix * surface = new nsDrawingSurfaceUnix();

  surface->drawable = p ;
  surface->display  = mRenderingSurface->display;
  surface->gc       = mRenderingSurface->gc;

  return ((nsDrawingSurface)surface);
}

void nsRenderingContextUnix :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
  nsDrawingSurfaceUnix * surface = (nsDrawingSurfaceUnix *) aDS;

  // XXX - Could this be a GC? If so, store the type of surface in nsDrawingSurfaceUnix
  ::XFreePixmap(surface->display, surface->drawable);

  delete aDS;
}

void nsRenderingContextUnix :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  ::XDrawLine(mRenderingSurface->display, 
	      mRenderingSurface->drawable,
	      mRenderingSurface->gc,
	      aX0, aX1, aY0, aY1);
}

void nsRenderingContextUnix :: DrawRect(const nsRect& aRect)
{
  DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

void nsRenderingContextUnix :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  ::XDrawRectangle(mRenderingSurface->display, 
		   mRenderingSurface->drawable,
		   mRenderingSurface->gc,
		   aX, aY, aWidth, aHeight);
}

void nsRenderingContextUnix :: FillRect(const nsRect& aRect)
{
  FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

void nsRenderingContextUnix :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  ::XFillRectangle(mRenderingSurface->display, 
		   mRenderingSurface->drawable,
		   mRenderingSurface->gc,
		   aX, aY,
		   aWidth, aHeight);
}


void nsRenderingContextUnix::DrawPolygon(nsPoint aPoints[], PRInt32 aNumPoints)
{
}

void nsRenderingContextUnix::FillPolygon(nsPoint aPoints[], PRInt32 aNumPoints)
{
}

void nsRenderingContextUnix :: DrawEllipse(const nsRect& aRect)
{
  DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

void nsRenderingContextUnix :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
}

void nsRenderingContextUnix :: FillEllipse(const nsRect& aRect)
{
  FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

void nsRenderingContextUnix :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
}

void nsRenderingContextUnix :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  this->DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

void nsRenderingContextUnix :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  ::XDrawArc(mRenderingSurface->display, 
	     mRenderingSurface->drawable,
	     mRenderingSurface->gc,
	     aX, aY, aWidth, aHeight, aStartAngle, aEndAngle);
}

void nsRenderingContextUnix :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  this->FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

void nsRenderingContextUnix :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  ::XFillArc(mRenderingSurface->display, 
	     mRenderingSurface->drawable,
	     mRenderingSurface->gc,
	     aX, aY, aWidth, aHeight, aStartAngle, aEndAngle);
}

void nsRenderingContextUnix :: DrawString(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY,
                                    nscoord aWidth)
{
  // XXX Hack
  ::XLoadFont(mRenderingSurface->display, "fixed");

  ::XDrawString(mRenderingSurface->display, 
		mRenderingSurface->drawable,
		mRenderingSurface->gc,
		aX, aY, aString, aWidth);
}

void nsRenderingContextUnix :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, nscoord aWidth)
{
}

void nsRenderingContextUnix :: DrawString(const nsString& aString,
                                         nscoord aX, nscoord aY, nscoord aWidth)
{
  // XXX Leak - How to Print UniChar  
  if (aString.Length() > 0) {
    char * buf ;
    
    buf = (char *) malloc(sizeof(char) * (aString.Length()+1));

    buf[aString.Length()] = '\0';

    aString.ToCString(buf, aString.Length());

    DrawString(buf, aString.Length(), aX, aY, aWidth);

    free(buf);
  }
}

void nsRenderingContextUnix :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
}

void nsRenderingContextUnix :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
}

void nsRenderingContextUnix :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
}

void nsRenderingContextUnix :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
}

nsresult nsRenderingContextUnix :: CopyOffScreenBits(nsRect &aBounds)
{
  ::XCopyArea(mRenderingSurface->display, 
	      mRenderingSurface->drawable,
	      mOffscreenSurface->drawable,
	      mOffscreenSurface->gc,
	      aBounds.x, aBounds.y, aBounds.width, aBounds.height, 0, 0);

  return NS_OK;
}















