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

#include <gtk/gtk.h>
#include "nsRenderingContextGTK.h"
#include <math.h>

typedef unsigned char BYTE;

#define RGB(r,g,b) ((unsigned long) (((BYTE) (r) | ((unsigned long) ((BYTE) (g)) <<8)) | (((unsigned long)(BYTE)(b)) << 16)))

#define NSRECT_TO_GDKRECT(ns,gdk) \
  PR_BEGIN_MACRO \
  gdk.x = ns.x; \
  gdk.y = ns.y; \
  gdk.width = ns.width; \
  gdk.height = ns.height; \
  PR_END_MACRO

static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

class GraphicsState {
 public:
   GraphicsState(GraphicsState &gs) { 
    gc = gdk_gc_new(NULL);
    gdk_gc_copy(gs.gc, gc);
    if (gs.clipRegion) {
      GdkRegion *leak = gdk_region_new();
      clipRegion = gdk_regions_union(leak, gs.clipRegion);
      gdk_region_destroy(leak);
    } else
      clipRegion = NULL;
    
    mCurrentColor = gs.mCurrentColor;
  }

  ~GraphicsState() { 
    gdk_gc_unref(gc);
    if (clipRegion)
      gdk_region_destroy(clipRegion);
  }

  GdkGC *gc;
  GdkRegion *clipRegion;
  nsRect clipRect;
  nscolor mCurrentColor;
};

nsRenderingContextGTK :: nsRenderingContextGTK()
{
  NS_INIT_REFCNT();

  mFontMetrics = nsnull ;
  mContext = nsnull ;
  mRenderingSurface = nsnull ;
  mOffscreenSurface = nsnull ;
  mTMatrix = nsnull;
}

nsRenderingContextGTK :: ~nsRenderingContextGTK()
{
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mFontMetrics);
}

NS_IMPL_QUERY_INTERFACE(nsRenderingContextGTK, kRenderingContextIID)
NS_IMPL_ADDREF(nsRenderingContextGTK)
NS_IMPL_RELEASE(nsRenderingContextGTK)

NS_IMETHODIMP nsRenderingContextGTK::Init(nsIDeviceContext* aContext,
                                          nsIWidget *aWindow)
{

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mRenderingSurface = new nsDrawingSurfaceGTK();

  mRenderingSurface->drawable = (GdkDrawable *)aWindow->GetNativeData(NS_NATIVE_WINDOW);
  mRenderingSurface->gc       = (GdkGC *)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);
  
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::Init(nsIDeviceContext* aContext,
                                          nsDrawingSurface aSurface)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mRenderingSurface = (nsDrawingSurfaceGTK *) aSurface;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetHints(PRUint32& aResult)
{
}

NS_IMETHODIMP nsRenderingContextGTK::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  mOffscreenSurface = mRenderingSurface;
  mRenderingSurface = (nsDrawingSurfaceGTK *) aSurface;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetDeviceContext(nsIDeviceContext *&aContext)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::PushState(void)
{
#if 0
  GraphicsState state = new GraphicsState(mStates->data);
  mStates = g_list_prepend(mStates, state);
#endif
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::PopState(PRBool &aClipEmpty)
{
  GraphicsState *state = (GraphicsState *)mStates->data;
  GList *tempList = mStates;
  mStates = mStates->next;
  g_list_free_1(tempList);
  delete state;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::IsVisibleRect(const nsRect& aRect,
                                                   PRBool &aClipEmpty)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::SetClipRect(const nsRect& aRect,
                                                 nsClipCombine aCombine,
                                                 PRBool &aClipEmpty)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  aRect = ((GraphicsState *)mStates->data)->clipRect;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::SetClipRegion(const nsIRegion& aRegion,
                                                   nsClipCombine aCombine,
                                                   PRBool &aClipEmpt)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetClipRegion(nsIRegion **aRegion)
{
}

NS_IMETHODIMP nsRenderingContextGTK::SetColor(nscolor aColor)
{
#if 0
  GraphicsState * state = ((GraphicsState *)mStates->data);
  GdkColor color;
  state->mCurrentColor = aColor;

  color.red = NS_GET_R(aColor);
  color.blue = NS_GET_B(aColor);
  color.green = NS_GET_G(aColor);
  gdk_color_alloc(&color);
  gdk_gc_set_foreground(state->gc, &color);
  /* no need to set background? */
#endif
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetColor(nscolor &aColor) const
{
#if 0
  GraphicsState state = ((GraphicsState *)mStates->data);
  return state->mCurrentColor;
#endif
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::SetFont(const nsFont& aFont)
{
#if 0
  if (mFontMetrics)
    delete mFontMetrics;
  mFontMetrics = new nsFontMetricsGTK();
  mFontMetrics.Init(aFont);
#endif
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::SetFont(nsIFontMetrics *aFontMetrics)
{
}

NS_IMETHODIMP nsRenderingContextGTK::SetLineStyle(nsLineStyle aLineStyle)
{
}

NS_IMETHODIMP nsRenderingContextGTK::GetLineStyle(nsLineStyle &aLineStyle)
{
}

NS_IMETHODIMP nsRenderingContextGTK::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextGTK::Translate(nscoord aX, nscoord aY)
{
#if 0
  mTMatrix->Translate((float)aX, (float)aY);
#endif
  return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextGTK::Scale(float aSx, float aSy)
{
#if 0
	mTMatrix->AddScale(aSx, aSy);
#endif
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetCurrentTransform(nsTransform2D *&aTransform)
{
#if 0
  return mTMatrix;
#endif
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::CreateDrawingSurface(nsRect *aBounds,
                                                          PRUint32 aSurfFlags,
                                                          nsDrawingSurface &aSurface)
{

  GdkPixmap *pixmap = gdk_pixmap_new(mRenderingSurface->drawable, aBounds->width,
                                     aBounds->height, -1);

  nsDrawingSurfaceGTK * surface = new nsDrawingSurfaceGTK();

  surface->drawable = pixmap ;
  surface->gc       = mRenderingSurface->gc;

//  return ((nsDrawingSurface)surface);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DestroyDrawingSurface(nsDrawingSurface aDS)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawRect(const nsRect& aRect)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
}

NS_IMETHODIMP nsRenderingContextGTK::FillRect(const nsRect& aRect)
{
}

NS_IMETHODIMP nsRenderingContextGTK::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
}

NS_IMETHODIMP nsRenderingContextGTK::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawEllipse(const nsRect& aRect)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
}

NS_IMETHODIMP nsRenderingContextGTK::FillEllipse(const nsRect& aRect)
{
}

NS_IMETHODIMP nsRenderingContextGTK::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawArc(const nsRect& aRect,
                   float aStartAngle, float aEndAngle)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawArc(nscoord aX, nscoord aY,
                                             nscoord aWidth, nscoord aHeight,
                                             float aStartAngle, float aEndAngle)
{
}

NS_IMETHODIMP nsRenderingContextGTK::FillArc(const nsRect& aRect,
                                             float aStartAngle, float aEndAngle)
{
}


NS_IMETHODIMP nsRenderingContextGTK::FillArc(nscoord aX, nscoord aY,
                                             nscoord aWidth, nscoord aHeight,
                                             float aStartAngle, float aEndAngle)
{
}

NS_IMETHODIMP nsRenderingContextGTK::GetWidth(char aC, nscoord &aWidth)
{
}
NS_IMETHODIMP nsRenderingContextGTK::GetWidth(PRUnichar aC, nscoord &aWidth)
{
}
NS_IMETHODIMP nsRenderingContextGTK::GetWidth(const nsString& aString, nscoord &aWidth)
{
}
NS_IMETHODIMP nsRenderingContextGTK::GetWidth(const char *aString, nscoord &aWidth)
{
}
NS_IMETHODIMP nsRenderingContextGTK::GetWidth(const char *aString,
                                              PRUint32 aLength, nscoord &aWidth)
{
}
NS_IMETHODIMP nsRenderingContextGTK::GetWidth(const PRUnichar *aString,
                                              PRUint32 aLength, nscoord &aWidth)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawString(const char *aString, PRUint32 aLength,
                                                nscoord aX, nscoord aY,
                                                nscoord aWidth,
                                                const nscoord* aSpacing)
{
}
NS_IMETHODIMP nsRenderingContextGTK::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                                nscoord aX, nscoord aY,
                                                nscoord aWidth,
                                                const nscoord* aSpacing)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawString(const nsString& aString, 
                                                nscoord aX, nscoord aY,
                                                nscoord aWidth,
                                                const nscoord* aSpacing)
{
}

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
}
NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                     nscoord aWidth, nscoord aHeight)
{
}
NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
}
NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
}

NS_IMETHODIMP nsRenderingContextGTK::CopyOffScreenBits(nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                               const nsRect &aDestBounds, PRUint32 aCopyFlags)
{
}

  //locals
NS_IMETHODIMP nsRenderingContextGTK::CommonInit()
{
}
NS_IMETHODIMP nsRenderingContextGTK::SetClipRectInPixels(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
}


#if 0
void nsRenderingContextGTK :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
  nsDrawingSurfaceGTK * surface = (nsDrawingSurfaceGTK *) aDS;

  gdk_pixmap_unref(surface->drawable);

  delete aDS;
}

void nsRenderingContextGTK :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  mTMatrix->TransformCoord(&aX0, &aY0);
  mTMatrix->TransformCoord(&aX1, &aY1);
  gdk_draw_line(mRenderingSurface->drawable, mRenderingSurface->gc, aX0, aX1, aY0, aY1);
}

void nsRenderingContextGTK :: DrawRect(const nsRect& aRect)
{
  DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

void nsRenderingContextGTK :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
  gdk_draw_rect(mRenderingSurface->drawable, mRenderingSurface->gc, FALSE, aX, aY, aWidth, aHeight);
}

void nsRenderingContextGTK :: FillRect(const nsRect& aRect)
{
  FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

void nsRenderingContextGTK :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
  gdk_draw_rect(mRenderingSurface->drawable, mRenderingSurface->gc, TRUE, aX, aY, aWidth, aHeight);
}


void nsRenderingContextGTK::DrawPolygon(nsPoint aPoints[], PRInt32 aNumPoints)
{
  GdkPoint *pts = new GdkPoint[aNumPoints];
	for (PRInt32 i = 0; i < aNumPoints; i++)
  {
    nsPoint p = aPoints[i];
		mTMatrix->TransformCoord(&p.x,&p.y);
		pts[i]->x = p.x;
    pts[i]->y = p.y;
	}
  gdk_draw_polygon(mRenderingSurface->drawable, mRenderingSurface->gc, FALSE, pts, aNumPoints);
}

void nsRenderingContextGTK::FillPolygon(nsPoint aPoints[], PRInt32 aNumPoints)
{
  GdkPoint *pts = new GdkPoint[aNumPoints];
	for (PRInt32 i = 0; i < aNumPoints; i++)
  {
    nsPoint p = aPoints[i];
		mTMatrix->TransformCoord(&p.x,&p.y);
		pts[i]->x = p.x;
    pts[i]->y = p.y;
	}
  /* XXXshaver common code! */
  gdk_draw_polygon(mRenderingSurface->drawable, mRenderingSurface->gc, TRUE, pts, aNumPoints);
}

void nsRenderingContextGTK :: DrawEllipse(const nsRect& aRect)
{
  DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

void nsRenderingContextGTK :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
  gdk_draw_arc(mRenderingSurface->drawable, mRenderingSurface->gc, FALSE, aX, aY, aWidth, aHeight,
               0, 360 * 64);
}

void nsRenderingContextGTK :: FillEllipse(const nsRect& aRect)
{
  FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

void nsRenderingContextGTK :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
  gdk_draw_arc(mRenderingSurface->drawable, mRenderingSurface->gc, TRUE, aX, aY, aWidth, aHeight,
               0, 360 * 64);
}

void nsRenderingContextGTK :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  this->DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

void nsRenderingContextGTK :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
  gdk_draw_arc(mRenderingSurface->drawable, mRenderingSurface->gc, FALSE, aX, aY, aWidth, aHeight,
               aStartAngle * 64, aEndAngle * 64);
}

void nsRenderingContextGTK :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  this->FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

void nsRenderingContextGTK :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
	mTMatrix->TransformCoord(&aX,&aY,&aWidth,&aHeight);
  gdk_draw_arc(mRenderingSurface->drawable, mRenderingSurface->gc, TRUE, aX, aY, aWidth, aHeight,
               aStartAngle * 64, aEndAngle * 64);
}

void nsRenderingContextGTK :: DrawString(const char *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY,
                                    nscoord aWidth)
{
  mTMatrix->TransformCoord(&aX, &aY);
  gdk_draw_string(mRenderingSurface->drawable, NULL, mRenderingSurface->gc, aX, aY);
}

void nsRenderingContextGTK :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, nscoord aWidth)
{
  PR_ASSERT(0);
}

void nsRenderingContextGTK :: DrawString(const nsString& aString,
                                         nscoord aX, nscoord aY, nscoord aWidth)
{
  mTMatrix->TransformCoord(&aX, &aY);
  if (aString.Length() > 0) {
    char * buf ;
    
    buf = (char *) malloc(sizeof(char) * (aString.Length()+1));

    buf[aString.Length()] = '\0';

    aString.ToCString(buf, aString.Length());

    gdk_draw_string(mRenderingSurface->drawable, NULL, mRenderingSurface->gc,
                    aX, aY, buf);

    free(buf);
  }
}

void nsRenderingContextGTK :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  nscoord width, height;

  width = NS_TO_INT_ROUND(mP2T * aImage->GetWidth());
  height = NS_TO_INT_ROUND(mP2T * aImage->GetHeight());

  this->DrawImage(aImage, aX, aY, width, height);
}

void nsRenderingContextGTK :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
  nsRect  tr;

  tr.x = aX;
  tr.y = aY;
  tr.width = aWidth;
  tr.height = aHeight;

  this->DrawImage(aImage, tr);
}

void nsRenderingContextGTK :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  nsRect	sr,dr;

	sr = aSRect;
	mTMatrix->TransformCoord(&sr.x, &sr.y, &sr.width, &sr.height);

  dr = aDRect;
	mTMatrix->TransformCoord(&dr.x, &dr.y, &dr.width, &dr.height);

  ((nsImageGTK *)aImage)->Draw(*this, mDC, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);
}

void nsRenderingContextGTK :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  nsRect	tr;

	tr = aRect;
	mTMatrix->TransformCoord(&tr.x, &tr.y, &tr.width, &tr.height);

  ((nsImageGTK *)aImage)->Draw(*this, mDC, tr.x, tr.y, tr.width, tr.height);
}

nsresult nsRenderingContextGTK :: CopyOffScreenBits(nsRect &aBounds)
{
  gdk_window_copy_area(mRenderingSurface->drawable, mRenderingSurface->gc, 
                       aBounds.x, aBounds.y, mOffScreenSurface->drawable,
                       0, 0, aBounds.width, aBounds.height);

  return NS_OK;
}
#endif /* 0 */














