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
#include "nsFontMetricsGTK.h"
#include "nsRenderingContextGTK.h"
#include "nsRegionGTK.h"
#include "nsGfxCIID.h"
#include <math.h>

#define NS_TO_GDK_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

#define NSRECT_TO_GDKRECT(ns,gdk) \
  PR_BEGIN_MACRO \
  gdk.x = ns.x; \
  gdk.y = ns.y; \
  gdk.width = ns.width; \
  gdk.height = ns.height; \
  PR_END_MACRO

static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

class GraphicsState
{
public:
  GraphicsState();
  virtual ~GraphicsState();

  nsTransform2D  *mMatrix;
  nsRegionGTK    *mClipRegion;
  nscolor         mColor;
  nsLineStyle     mLineStyle;
  nsIFontMetrics *mFontMetrics;
};

GraphicsState::GraphicsState()
{
  mMatrix = nsnull;
  mClipRegion = nsnull;
  mColor = NS_RGB(0, 0, 0);
  mLineStyle = nsLineStyle_kSolid;
  mFontMetrics = nsnull;
}

GraphicsState::~GraphicsState()
{
  NS_IF_RELEASE(mClipRegion);
  NS_IF_RELEASE(mFontMetrics);
}


nsRenderingContextGTK::nsRenderingContextGTK()
{
  NS_INIT_REFCNT();

  mFontMetrics = nsnull;
  mContext = nsnull;
  mSurface = nsnull;
  mOffscreenSurface = nsnull;
  mCurrentColor = 0;
  mCurrentLineStyle = nsLineStyle_kSolid;
  mCurrentFont = nsnull;
  mTMatrix = nsnull;
  mP2T = 1.0f;
  mStateCache = new nsVoidArray();
  mClipRegion = nsnull;
  mDrawStringBuf = nsnull;

  PushState();
}

nsRenderingContextGTK::~nsRenderingContextGTK()
{
  // Destroy the State Machine
  if (mStateCache)
  {
    PRInt32 cnt = mStateCache->Count();

    while (--cnt >= 0)
    {
      PRBool  clipstate;
      PopState(clipstate);
    }

    delete mStateCache;
    mStateCache = nsnull;
  }

  if (mTMatrix)
    delete mTMatrix;
  NS_IF_RELEASE(mClipRegion);
  NS_IF_RELEASE(mOffscreenSurface);
  NS_IF_RELEASE(mFontMetrics);
  NS_IF_RELEASE(mContext);

  if (nsnull != mDrawStringBuf) {
    delete [] mDrawStringBuf;
  }
}

NS_IMPL_QUERY_INTERFACE(nsRenderingContextGTK, kRenderingContextIID)
NS_IMPL_ADDREF(nsRenderingContextGTK)
NS_IMPL_RELEASE(nsRenderingContextGTK)

NS_IMETHODIMP nsRenderingContextGTK::Init(nsIDeviceContext* aContext,
                                          nsIWidget *aWindow)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

//  ::gdk_rgb_init();

  mSurface = new nsDrawingSurfaceGTK();

  if (mSurface)
  {
#ifndef NS_GTK_REF
    // we want to ref the window here so that we can unref in the drawing surface.
    // otherwise, we can not unref and that causes windows that are created in the
    // drawing surface not to be freed.
    GdkDrawable *win = (GdkDrawable*)gdk_window_ref((GdkWindow *)aWindow->GetNativeData(NS_NATIVE_WINDOW));
#else
    GdkDrawable *win = (GdkDrawable *)aWindow->GetNativeData(NS_NATIVE_WINDOW);
#endif
    GdkGC *gc = (GdkGC *)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);
    mSurface->Init(win,gc);

    mOffscreenSurface = mSurface;

    NS_ADDREF(mSurface);
  }
  return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextGTK::Init(nsIDeviceContext* aContext,
                                          nsDrawingSurface aSurface)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mSurface = (nsDrawingSurfaceGTK *) aSurface;
  NS_ADDREF(mSurface);

  return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextGTK::CommonInit()
{
  gint x, y, w, h, d;
  gdk_window_get_geometry(mSurface->GetDrawable(), &x, &y, &w, &h, &d);
  
  mClipRegion = new nsRegionGTK();
  mClipRegion->Init();
  mClipRegion->SetTo(0, 0, w, h);

  mContext->GetDevUnitsToAppUnits(mP2T);
  float app2dev;
  mContext->GetAppUnitsToDevUnits(app2dev);
  mTMatrix->AddScale(app2dev, app2dev);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetHints(PRUint32& aResult)
{
  PRUint32 result = 0;

  // Most X servers implement 8 bit text rendering alot faster than
  // XChar2b rendering. In addition, we can avoid the PRUnichar to
  // XChar2b conversion. So we set this bit...
  result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;

  // XXX see if we are rendering to the local display or to a remote
  // dispaly and set the NS_RENDERING_HINT_REMOTE_RENDERING accordingly

  aResult = result;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  PushState();

  return mSurface->Lock(aX, aY, aWidth, aHeight,
  			aBits, aStride, aWidthBytes, aFlags);
}

NS_IMETHODIMP nsRenderingContextGTK::UnlockDrawingSurface(void)
{
  PRBool  clipstate;
  PopState(clipstate);

  mSurface->Unlock();
  
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{
  if (nsnull == aSurface)
    mSurface = mOffscreenSurface;
  else
    mSurface = (nsDrawingSurfaceGTK *)aSurface;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetDrawingSurface(nsDrawingSurface *aSurface)
{
  *aSurface = mSurface;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::Reset()
{
  g_print("nsRenderingContextGTK::Reset() called\n");
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::PushState(void)
{
  GraphicsState *state = new GraphicsState();

  // Push into this state object, add to vector
  state->mMatrix = mTMatrix;

  if (nsnull == mTMatrix)
    mTMatrix = new nsTransform2D();
  else
    mTMatrix = new nsTransform2D(mTMatrix);

  if (mClipRegion)
    {
      NS_IF_ADDREF(mClipRegion);
      state->mClipRegion = mClipRegion;
      
      mClipRegion = new nsRegionGTK();
      mClipRegion->Init();
      mClipRegion->SetTo(state->mClipRegion);
    }

  NS_IF_ADDREF(mFontMetrics);
  state->mFontMetrics = mFontMetrics;

  state->mColor = mCurrentColor;
  state->mLineStyle = mCurrentLineStyle;

  mStateCache->AppendElement(state);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::PopState(PRBool &aClipEmpty)
{
  PRUint32 cnt = mStateCache->Count();
  GraphicsState * state;

  if (cnt > 0) {
    state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);

    // Assign all local attributes from the state object just popped
    if (mTMatrix)
      delete mTMatrix;
    mTMatrix = state->mMatrix;

    NS_RELEASE(mClipRegion);

// restore everything
    mClipRegion = state->mClipRegion;
    mFontMetrics = state->mFontMetrics;

    if (mSurface && mClipRegion)
    {
      GdkRegion *rgn;
      mClipRegion->GetNativeRegion((void*&)rgn);
      ::gdk_gc_set_clip_region (mSurface->GetGC(), rgn);
    }

    if (state->mColor != mCurrentColor)
      SetColor(state->mColor);

    if (state->mLineStyle != mCurrentLineStyle)
      SetLineStyle(state->mLineStyle);

    // Delete this graphics state object
    delete state;
  }

  if (mClipRegion)
    aClipEmpty = mClipRegion->IsEmpty();
  else
    aClipEmpty = PR_TRUE;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::IsVisibleRect(const nsRect& aRect,
                                                   PRBool &aVisible)
{
  aVisible = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  PRInt32 x, y, w, h;
  if (!mClipRegion->IsEmpty())
  {
    mClipRegion->GetBoundingBox(&x,&y,&w,&h);
    aRect.SetRect(x,y,w,h);
    aClipValid = PR_TRUE;
  } else {
    aRect.SetRect(0,0,0,0);
    aClipValid = PR_FALSE;
  }

  return NS_OK;
}

#ifdef DEBUG
#undef TRACE_SET_CLIP
#endif

#ifdef TRACE_SET_CLIP
static char *
nsClipCombine_to_string(nsClipCombine aCombine)
{
#ifdef TRACE_SET_CLIP
  printf("nsRenderingContextGTK::SetClipRect(x=%d,y=%d,w=%d,h=%d,%s)\n",
         trect.x,
         trect.y,
         trect.width,
         trect.height,
         nsClipCombine_to_string(aCombine));
#endif // TRACE_SET_CLIP

  switch(aCombine)
    {
      case nsClipCombine_kIntersect:
        return "nsClipCombine_kIntersect";
        break;

      case nsClipCombine_kUnion:
        return "nsClipCombine_kUnion";
        break;

      case nsClipCombine_kSubtract:
        return "nsClipCombine_kSubtract";
        break;

      case nsClipCombine_kReplace:
        return "nsClipCombine_kReplace";
        break;
    }

  return "something got screwed";
}
#endif // TRACE_SET_CLIP

NS_IMETHODIMP nsRenderingContextGTK::SetClipRect(const nsRect& aRect,
                                                 nsClipCombine aCombine,
                                                 PRBool &aClipEmpty)
{
  nsRect trect = aRect;
  GdkRegion *rgn;

#ifdef TRACE_SET_CLIP
  printf("nsRenderingContextGTK::SetClipRect(%s)\n",
         nsClipCombine_to_string(aCombine));
#endif // TRACE_SET_CLIP

  mTMatrix->TransformCoord(&trect.x, &trect.y,
                           &trect.width, &trect.height);

  switch(aCombine)
  {
    case nsClipCombine_kIntersect:
      mClipRegion->Intersect(trect.x,trect.y,trect.width,trect.height);
      break;
    case nsClipCombine_kUnion:
      mClipRegion->Union(trect.x,trect.y,trect.width,trect.height);
      break;
    case nsClipCombine_kSubtract:
      mClipRegion->Subtract(trect.x,trect.y,trect.width,trect.height);
      break;
    case nsClipCombine_kReplace:
      mClipRegion->SetTo(trect.x,trect.y,trect.width,trect.height);
      break;
  }

  aClipEmpty = mClipRegion->IsEmpty();

  mClipRegion->GetNativeRegion((void*&)rgn);
  gdk_gc_set_clip_region(mSurface->GetGC(),rgn);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::SetClipRegion(const nsIRegion& aRegion,
                                                   nsClipCombine aCombine,
                                                   PRBool &aClipEmpty)
{
  GdkRegion *rgn;
 
  switch(aCombine)
  {
    case nsClipCombine_kIntersect:
      mClipRegion->Intersect(aRegion);
      break;
    case nsClipCombine_kUnion:
      mClipRegion->Union(aRegion);
      break;
    case nsClipCombine_kSubtract:
      mClipRegion->Subtract(aRegion);
      break;
    case nsClipCombine_kReplace:
      mClipRegion->SetTo(aRegion);
      break;
  }

  aClipEmpty = mClipRegion->IsEmpty();
  mClipRegion->GetNativeRegion((void*&)rgn);
  gdk_gc_set_clip_region(mSurface->GetGC(),rgn);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetClipRegion(nsIRegion **aRegion)
{
  nsresult  rv = NS_OK;

  NS_ASSERTION(!(nsnull == aRegion), "no region ptr");

  if (nsnull == *aRegion)
  {
    nsRegionGTK *rgn = new nsRegionGTK();

    if (nsnull != rgn)
    {
      NS_ADDREF(rgn);

      rv = rgn->Init();

      if (NS_OK == rv)
        *aRegion = rgn;
      else
        NS_RELEASE(rgn);
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (rv == NS_OK)
    (*aRegion)->SetTo(*mClipRegion);

  return rv;
}

NS_IMETHODIMP nsRenderingContextGTK::SetColor(nscolor aColor)
{
  if (nsnull == mContext)  
    return NS_ERROR_FAILURE;
      
  mCurrentColor = aColor;

  ::gdk_rgb_gc_set_foreground(mSurface->GetGC(), NS_TO_GDK_RGB(mCurrentColor));
  
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetColor(nscolor &aColor) const
{
  aColor = mCurrentColor;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::SetFont(const nsFont& aFont)
{
  NS_IF_RELEASE(mFontMetrics);
  mContext->GetMetricsFor(aFont, mFontMetrics);
  return SetFont(mFontMetrics);
}

NS_IMETHODIMP nsRenderingContextGTK::SetFont(nsIFontMetrics *aFontMetrics)
{
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

  if (mFontMetrics)
  {
    nsFontHandle  fontHandle;
    mFontMetrics->GetFontHandle(fontHandle);
    mCurrentFont = (GdkFont *)fontHandle;

    gdk_gc_set_font(mSurface->GetGC(),
                    mCurrentFont);
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::SetLineStyle(nsLineStyle aLineStyle)
{
  if (aLineStyle != mCurrentLineStyle)
  {
    switch(aLineStyle)
    { 
      case nsLineStyle_kSolid:
        ::gdk_gc_set_line_attributes(mSurface->GetGC(),
                          1, GDK_LINE_SOLID, (GdkCapStyle)0, (GdkJoinStyle)0);
        break;

      case nsLineStyle_kDashed: {
        static char dashed[2] = {4,4};

        ::gdk_gc_set_dashes(mSurface->GetGC(), 
                     0, dashed, 2);
        } break;

      case nsLineStyle_kDotted: {
        static char dotted[2] = {3,1};

        ::gdk_gc_set_dashes(mSurface->GetGC(), 
                     0, dotted, 2);
         }break;

      default:
        break;

    }
    
    mCurrentLineStyle = aLineStyle ;
  }

  return NS_OK;

}

NS_IMETHODIMP nsRenderingContextGTK::GetLineStyle(nsLineStyle &aLineStyle)
{
  aLineStyle = mCurrentLineStyle;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;
  return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextGTK::Translate(nscoord aX, nscoord aY)
{
  mTMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextGTK::Scale(float aSx, float aSy)
{
  mTMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mTMatrix;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::CreateDrawingSurface(nsRect *aBounds,
                                                          PRUint32 aSurfFlags,
                                                          nsDrawingSurface &aSurface)
{
  if (nsnull == mSurface) {
    aSurface = nsnull;
    return NS_ERROR_FAILURE;
  }
 
  g_return_val_if_fail (aBounds != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail ((aBounds->width > 0) && (aBounds->height > 0), NS_ERROR_FAILURE);
 
  nsDrawingSurfaceGTK *surf = new nsDrawingSurfaceGTK();

  if (surf)
  {
    NS_ADDREF(surf);
    surf->Init(mSurface->GetGC(), aBounds->width, aBounds->height, aSurfFlags);
  }

  aSurface = (nsDrawingSurface)surf;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DestroyDrawingSurface(nsDrawingSurface aDS)
{
  nsDrawingSurfaceGTK *surf = (nsDrawingSurfaceGTK *) aDS;

  g_return_val_if_fail ((surf != NULL), NS_ERROR_FAILURE);

  NS_IF_RELEASE(surf);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  mTMatrix->TransformCoord(&aX0,&aY0);
  mTMatrix->TransformCoord(&aX1,&aY1);

  if (aY0 != aY1) {
    aY1--;
  }
  if (aX0 != aX1) {
    aX1--;
  }
  ::gdk_draw_line(mSurface->GetDrawable(),
                  mSurface->GetGC(),
                  aX0, aY0, aX1, aY1);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PRInt32 i;

  g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  GdkPoint *pts = new GdkPoint[aNumPoints];
	for (i = 0; i < aNumPoints; i++)
  {
    nsPoint p = aPoints[i];
    mTMatrix->TransformCoord(&p.x,&p.y);
    pts[i].x = p.x;
    pts[i].y = p.y;
  }

  ::gdk_draw_lines(mSurface->GetDrawable(),
                   mSurface->GetGC(),
                   pts, aNumPoints);

  delete[] pts;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawRect(const nsRect& aRect)
{
  return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  if (nsnull == mTMatrix || nsnull == mSurface) {
    return NS_ERROR_FAILURE;
  }

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  g_return_val_if_fail ((mSurface->GetDrawable() != NULL) ||
                        (mSurface->GetGC() != NULL), NS_ERROR_FAILURE);

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  ::gdk_draw_rectangle(mSurface->GetDrawable(), mSurface->GetGC(),
                       FALSE,
                       x, y, w - 1, h - 1);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::FillRect(const nsRect& aRect)
{
  return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextGTK::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  if (nsnull == mTMatrix || nsnull == mSurface) {
    return NS_ERROR_FAILURE;
  }

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  //  gdk_gc_set_clip_region(mSurface->GetGC(), nsnull);

  ::gdk_draw_rectangle(mSurface->GetDrawable(), mSurface->GetGC(),
                       TRUE,
                       x, y, w, h);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  GdkPoint *pts = new GdkPoint[aNumPoints];
	for (PRInt32 i = 0; i < aNumPoints; i++)
  {
    nsPoint p = aPoints[i];
		mTMatrix->TransformCoord(&p.x,&p.y);
		pts[i].x = p.x;
    pts[i].y = p.y;
	}
  ::gdk_draw_polygon(mSurface->GetDrawable(), mSurface->GetGC(), FALSE, pts, aNumPoints);

  delete[] pts;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  GdkPoint *pts = new GdkPoint[aNumPoints];
	for (PRInt32 i = 0; i < aNumPoints; i++)
  {
    nsPoint p = aPoints[i];
		mTMatrix->TransformCoord(&p.x,&p.y);
		pts[i].x = p.x;
    pts[i].y = p.y;
	}
  ::gdk_draw_polygon(mSurface->GetDrawable(), mSurface->GetGC(), TRUE, pts, aNumPoints);

  delete[] pts;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawEllipse(const nsRect& aRect)
{
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  ::gdk_draw_arc(mSurface->GetDrawable(), mSurface->GetGC(), FALSE,
                 x, y, w, h,
                 0, 360 * 64);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::FillEllipse(const nsRect& aRect)
{
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextGTK::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  ::gdk_draw_arc(mSurface->GetDrawable(), mSurface->GetGC(), TRUE,
                 x, y, w, h,
                 0, 360 * 64);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawArc(const nsRect& aRect,
                                             float aStartAngle, float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawArc(nscoord aX, nscoord aY,
                                             nscoord aWidth, nscoord aHeight,
                                             float aStartAngle, float aEndAngle)
{
  g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  ::gdk_draw_arc(mSurface->GetDrawable(), mSurface->GetGC(), FALSE,
                 x, y, w, h,
                 NSToIntRound(aStartAngle * 64.0f),
                 NSToIntRound(aEndAngle * 64.0f));

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::FillArc(const nsRect& aRect,
                                             float aStartAngle, float aEndAngle)
{
  return FillArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}


NS_IMETHODIMP nsRenderingContextGTK::FillArc(nscoord aX, nscoord aY,
                                             nscoord aWidth, nscoord aHeight,
                                             float aStartAngle, float aEndAngle)
{
  g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTMatrix->TransformCoord(&x,&y,&w,&h);

  ::gdk_draw_arc(mSurface->GetDrawable(), mSurface->GetGC(), TRUE,
                 x, y, w, h,
                 NSToIntRound(aStartAngle * 64.0f),
                 NSToIntRound(aEndAngle * 64.0f));

  return NS_OK;
}

#ifdef FONT_SWITCHING

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(char aC, nscoord &aWidth)
{
  return GetWidth(&aC, 1, aWidth);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(PRUnichar aC, nscoord& aWidth,
                                PRInt32* aFontID)
{
  return GetWidth(&aC, 1, aWidth, aFontID);
}

#else /* FONT_SWITCHING */

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(char aC, nscoord &aWidth)
{
  gint rawWidth = gdk_char_width(mCurrentFont, aC); 
  aWidth = NSToCoordRound(rawWidth * mP2T);
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(PRUnichar aC, nscoord& aWidth,
                                PRInt32* aFontID)
{
  gint rawWidth = gdk_char_width_wc(mCurrentFont, (GdkWChar)aC); 
  aWidth = NSToCoordRound(rawWidth * mP2T);
  if (nsnull != aFontID)
    *aFontID = 0;
  return NS_OK;
}

#endif /* FONT_SWITCHING */

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const nsString& aString,
                                nscoord& aWidth, PRInt32* aFontID)
{
  return GetWidth(aString.GetUnicode(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

#ifdef FONT_SWITCHING

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const char* aString, PRUint32 aLength,
                                nscoord& aWidth)
{
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);
    gint rawWidth = gdk_text_width (mCurrentFont, aString, aLength);
    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                                nscoord& aWidth, PRInt32* aFontID)
{
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

    nsFontMetricsGTK* metrics = (nsFontMetricsGTK*) mFontMetrics;
    nsFontGTK* prevFont = nsnull;
    gint rawWidth = 0;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontGTK* currFont = nsnull;
      nsFontGTK** font = metrics->mLoadedFonts;
      nsFontGTK** end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (FONT_HAS_GLYPH((*font)->mMap, c)) {
	  currFont = *font;
	  goto FoundFont; // for speed -- avoid "if" statement
	}
	font++;
      }
      currFont = metrics->FindFont(c);
FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
	if (currFont != prevFont) {
          rawWidth += nsFontMetricsGTK::GetWidth(prevFont, &aString[start],
	    i - start);
	  prevFont = currFont;
	  start = i;
	}
      }
      else {
        prevFont = currFont;
	start = i;
      }
    }

    if (prevFont) {
      rawWidth += nsFontMetricsGTK::GetWidth(prevFont, &aString[start],
        i - start);
    }

    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}

#else /* FONT_SWITCHING */

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const char* aString, PRUint32 aLength,
                                nscoord& aWidth)
{
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);
    gint rawWidth = gdk_text_width (mCurrentFont, aString, aLength);
    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                                nscoord& aWidth, PRInt32* aFontID)
{
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

    // Make the temporary buffer larger if needed.
    if (nsnull == mDrawStringBuf) {
      mDrawStringBuf = new GdkWChar[aLength];
      mDrawStringSize = aLength;
    }
    else {
      if (mDrawStringSize < aLength) {
        delete [] mDrawStringBuf;
        mDrawStringBuf = new GdkWChar[aLength];
        mDrawStringSize = aLength;
      }
    }

    // Translate the unicode data into GdkWChar's
    GdkWChar* xc = mDrawStringBuf;
    GdkWChar* end = xc + aLength;
    while (xc < end) {
      *xc++ = (GdkWChar) *aString++;
    }

    gint rawWidth = gdk_text_width_wc(mCurrentFont, mDrawStringBuf, aLength);
    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}

#endif /* FONT_SWITCHING */

NS_IMETHODIMP
nsRenderingContextGTK::DrawString(const char *aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  const nscoord* aSpacing)
{
  if (0 != aLength) {
    g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

    nscoord x = aX;
    nscoord y = aY;

    // Substract xFontStruct ascent since drawing specifies baseline
    if (mFontMetrics) {
      mFontMetrics->GetMaxAscent(y);
      y += aY;
    }

    if (nsnull != aSpacing) {
      // Render the string, one character at a time...
      const char* end = aString + aLength;
      while (aString < end) {
        char ch = *aString++;
        nscoord xx = x;
        nscoord yy = y;
        mTMatrix->TransformCoord(&xx, &yy);
        ::gdk_draw_text(mSurface->GetDrawable(), mCurrentFont,
                        mSurface->GetGC(),
                        xx, yy, &ch, 1);
        x += *aSpacing++;
      }
    }
    else {
      mTMatrix->TransformCoord(&x, &y);
      ::gdk_draw_text (mSurface->GetDrawable(), mCurrentFont,
                       mSurface->GetGC(),
                       x, y, aString, aLength);
    }
  }

#if 0
  //this is no longer to be done by this API, but another
  //will take it's place that will need this code again. MMP
  if (mFontMetrics)
  {
    const nsFont *font;
    mFontMetrics->GetFont(font);
    PRUint8 deco = font->decorations;

    if (deco & NS_FONT_DECORATION_OVERLINE)
      DrawLine(aX, aY, aX + aWidth, aY);

    if (deco & NS_FONT_DECORATION_UNDERLINE)
    {
      nscoord ascent,descent;

      mFontMetrics->GetMaxAscent(ascent);
      mFontMetrics->GetMaxDescent(descent);

      DrawLine(aX, aY + ascent + (descent >> 1),
               aX + aWidth, aY + ascent + (descent >> 1));
    }

    if (deco & NS_FONT_DECORATION_LINE_THROUGH)
    {
      nscoord height;

	  mFontMetrics->GetHeight(height);

      DrawLine(aX, aY + (height >> 1), aX + aWidth, aY + (height >> 1));
    }
  }
#endif

  return NS_OK;
}

#ifdef FONT_SWITCHING

NS_IMETHODIMP
nsRenderingContextGTK::DrawString(const PRUnichar* aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  PRInt32 aFontID,
                                  const nscoord* aSpacing)
{
  if (aLength && mFontMetrics) {
    g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

    nscoord x = aX;
    nscoord y;

    // Substract xFontStruct ascent since drawing specifies baseline
    mFontMetrics->GetMaxAscent(y);
    y += aY;
    aY = y;

    mTMatrix->TransformCoord(&x, &y);

    nsFontMetricsGTK* metrics = (nsFontMetricsGTK*) mFontMetrics;
    nsFontGTK* prevFont = nsnull;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontGTK* currFont = nsnull;
      nsFontGTK** font = metrics->mLoadedFonts;
      nsFontGTK** end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (FONT_HAS_GLYPH((*font)->mMap, c)) {
	  currFont = *font;
	  goto FoundFont; // for speed -- avoid "if" statement
	}
	font++;
      }
      currFont = metrics->FindFont(c);
FoundFont:
      // XXX avoid this test by duplicating code -- erik
      if (prevFont) {
	if (currFont != prevFont) {
	  if (aSpacing) {
	    const PRUnichar* str = &aString[start];
	    const PRUnichar* end = &aString[i];
	    while (str < end) {
	      x = aX;
	      y = aY;
              mTMatrix->TransformCoord(&x, &y);
              nsFontMetricsGTK::DrawString(mSurface, prevFont, x, y, str, 1);
	      aX += *aSpacing++;
	      str++;
	    }
	  }
	  else {
            nsFontMetricsGTK::DrawString(mSurface, prevFont, x, y,
	      &aString[start], i - start);
            x += nsFontMetricsGTK::GetWidth(prevFont, &aString[start],
	      i - start);
	  }
	  prevFont = currFont;
	  start = i;
	}
      }
      else {
        prevFont = currFont;
	start = i;
      }
    }

    if (prevFont) {
      if (aSpacing) {
	const PRUnichar* str = &aString[start];
	const PRUnichar* end = &aString[i];
	while (str < end) {
	  x = aX;
	  y = aY;
          mTMatrix->TransformCoord(&x, &y);
          nsFontMetricsGTK::DrawString(mSurface, prevFont, x, y, str, 1);
	  aX += *aSpacing++;
	  str++;
	}
      }
      else {
        nsFontMetricsGTK::DrawString(mSurface, prevFont, x, y, &aString[start],
	  i - start);
      }
    }
  }

  return NS_OK;
}

#else /* FONT_SWITCHING */

NS_IMETHODIMP
nsRenderingContextGTK::DrawString(const PRUnichar* aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  PRInt32 aFontID,
                                  const nscoord* aSpacing)
{
  if (0 != aLength) {
    g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

    nscoord x = aX;
    nscoord y = aY;

    // Substract xFontStruct ascent since drawing specifies baseline
    if (mFontMetrics) {
      mFontMetrics->GetMaxAscent(y);
      y += aY;
    }

    if (nsnull != aSpacing) {
      // Render the string, one character at a time...
      const PRUnichar* end = aString + aLength;
      while (aString < end) {
        GdkWChar ch = (GdkWChar) *aString++;
        nscoord xx = x;
        nscoord yy = y;
        mTMatrix->TransformCoord(&xx, &yy);
        ::gdk_draw_text_wc(mSurface->GetDrawable(), mCurrentFont,
                           mSurface->GetGC(),
                           xx, yy, &ch, 1);
        x += *aSpacing++;
      }
    }
    else {
      // Make the temporary buffer larger if needed.
      if (nsnull == mDrawStringBuf) {
        mDrawStringBuf = new GdkWChar[aLength];
        mDrawStringSize = aLength;
      }
      else {
        if (mDrawStringSize < aLength) {
          delete [] mDrawStringBuf;
          mDrawStringBuf = new GdkWChar[aLength];
          mDrawStringSize = aLength;
        }
      }

      // Translate the unicode data into GdkWChar's
      GdkWChar* xc = mDrawStringBuf;
      GdkWChar* end = xc + aLength;
      while (xc < end) {
        *xc++ = (GdkWChar) *aString++;
      }

      mTMatrix->TransformCoord(&x, &y);
      ::gdk_draw_text_wc (mSurface->GetDrawable(), mCurrentFont,
                          mSurface->GetGC(),
                          x, y, mDrawStringBuf, aLength);
    }
  }
  return NS_OK;
}

#endif /* FONT_SWITCHING */

NS_IMETHODIMP
nsRenderingContextGTK::DrawString(const nsString& aString,
                                  nscoord aX, nscoord aY,
                                  PRInt32 aFontID,
                                  const nscoord* aSpacing)
{
  return DrawString(aString.GetUnicode(), aString.Length(),
                    aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  nscoord width,height;
  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());

  return DrawImage(aImage,aX,aY,width,height);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                     nscoord aWidth, nscoord aHeight)
{
  nsRect	tr;

  tr.x = aX;
  tr.y = aY;
  tr.width = aWidth;
  tr.height = aHeight;

  return DrawImage(aImage,tr);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  nsRect	tr;

  tr = aRect;
  mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);

  return aImage->Draw(*this,mSurface,tr.x,tr.y,tr.width,tr.height);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  nsRect	sr,dr;

  sr = aSRect;
  mTMatrix ->TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);

  dr = aDRect;
  mTMatrix->TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);

  return aImage->Draw(*this,mSurface,sr.x,sr.y,sr.width,sr.height,
                      dr.x,dr.y,dr.width,dr.height);
}

NS_IMETHODIMP
nsRenderingContextGTK::CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                         const nsRect &aDestBounds,
                                         PRUint32 aCopyFlags)
{
  PRInt32               x = aSrcX;
  PRInt32               y = aSrcY;
  nsRect                drect = aDestBounds;
  nsDrawingSurfaceGTK  *destsurf;

  g_return_val_if_fail(aSrcSurf != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mTMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

#if 0
  printf("nsRenderingContextGTK::CopyOffScreenBits()\nflags=\n");

  if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
    printf("NS_COPYBITS_USE_SOURCE_CLIP_REGION\n");

  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    printf("NS_COPYBITS_XFORM_SOURCE_VALUES\n");

  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    printf("NS_COPYBITS_XFORM_DEST_VALUES\n");

  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
    printf("NS_COPYBITS_TO_BACK_BUFFER\n");

  printf("\n");
#endif

  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
  {
    NS_ASSERTION(!(nsnull == mSurface), "no back buffer");
    destsurf = mSurface;
  }
  else
    destsurf = mOffscreenSurface;

  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mTMatrix->TransformCoord(&x, &y);

  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mTMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

  //XXX flags are unused. that would seem to mean that there is
  //inefficiency somewhere... MMP

  ::gdk_draw_pixmap(destsurf->GetDrawable(),
                    ((nsDrawingSurfaceGTK *)aSrcSurf)->GetGC(),
                    ((nsDrawingSurfaceGTK *)aSrcSurf)->GetDrawable(),
                    x, y,
                    drect.x, drect.y,
                    drect.width, drect.height);

  return NS_OK;
}
