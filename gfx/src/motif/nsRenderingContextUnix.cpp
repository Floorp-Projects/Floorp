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

  mGC = (GC)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);
  mDrawable = (Drawable) aWindow->GetNativeData(NS_NATIVE_WIDGET);

#if 0
  mFontCache = mContext->GetFontCache();
#endif 
}

nsresult nsRenderingContextUnix :: Init(nsIDeviceContext* aContext,
					nsDrawingSurface aSurface)
{
  // XXX We are doomed if this is the case .... need to remove this API. In X, you need both
  //         a drawable and gc for rendering
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mGC = (GC)aSurface;
}

nsresult nsRenderingContextUnix :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  return NS_OK;
}

void nsRenderingContextUnix :: Reset()
{
}

nsIDeviceContext * nsRenderingContextUnix :: GetDeviceContext(void)
{
  return mContext;
}

void nsRenderingContextUnix :: PushState()
{
}

void nsRenderingContextUnix :: PopState()
{
}

PRBool nsRenderingContextUnix :: IsVisibleRect(const nsRect& aRect)
{
  return PR_TRUE;
}

void nsRenderingContextUnix :: SetClipRect(const nsRect& aRect, PRBool aIntersect)
{
}

const nsRect& nsRenderingContextUnix :: GetClipRect()
{
  nsRect r(0,0,0,0);
  return r;
}

void nsRenderingContextUnix :: SetColor(nscolor aColor)
{
  XGCValues values ;

  mCurrentColor = aColor ;

  values.foreground = mCurrentColor;
  values.background = mCurrentColor;

  XChangeGC(XtDisplay((Widget)mDrawable),
	    mGC,
	    GCForeground | GCBackground,
	    &values);
}

nscolor nsRenderingContextUnix :: GetColor() const
{
  return mCurrentColor;
}

void nsRenderingContextUnix :: SetFont(const nsFont& aFont)
{
  Font id = ::XLoadFont(XtDisplay((Widget)mDrawable), "fixed");

  XFontStruct * fs = ::XQueryFont(XtDisplay((Widget)mDrawable), id);

  ::XSetFont(XtDisplay((Widget)mDrawable), mGC, id);


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
  return nsnull ;
}

void nsRenderingContextUnix :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
}

void nsRenderingContextUnix :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
}

void nsRenderingContextUnix :: DrawRect(const nsRect& aRect)
{
}

void nsRenderingContextUnix :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
}

void nsRenderingContextUnix :: FillRect(const nsRect& aRect)
{
  FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

void nsRenderingContextUnix :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  ::XFillRectangle(XtDisplay((Widget)mDrawable), 
		   XtWindow((Widget)mDrawable),
		   mGC,
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
}

void nsRenderingContextUnix :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  this->FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

void nsRenderingContextUnix :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
}

void nsRenderingContextUnix :: DrawString(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY,
                                    nscoord aWidth)
{
  // XXX Hack
  ::XLoadFont(XtDisplay((Widget)mDrawable), "fixed");

  ::XDrawString(XtDisplay((Widget)mDrawable), 
		XtWindow((Widget)mDrawable),
		mGC,
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
  DrawString(aString.ToNewCString(), aString.Length(), aX, aY, aWidth);
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
  return NS_OK;
}
