/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsFontMetricsGTK.h"
#include "nsRenderingContextGTK.h"
#include "nsRegionGTK.h"
#include "nsImageGTK.h"
#include "nsGraphicsStateGTK.h"
#include "nsCompressedCharMap.h"
#include <math.h>
#include "nsGCCache.h"
#include <gtk/gtk.h>
#include "prmem.h"

#define NS_TO_GDK_RGB(ns) (ns & 0xff) << 16 | (ns & 0xff00) | ((ns >> 16) & 0xff)

NS_IMPL_THREADSAFE_ISUPPORTS1(nsRenderingContextGTK, nsIRenderingContext)

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

#define NSRECT_TO_GDKRECT(ns,gdk) \
  PR_BEGIN_MACRO \
  gdk.x = ns.x; \
  gdk.y = ns.y; \
  gdk.width = ns.width; \
  gdk.height = ns.height; \
  PR_END_MACRO

static nsGCCache *gcCache = nsnull;

nsRenderingContextGTK::nsRenderingContextGTK()
{
  NS_INIT_REFCNT();

  mFontMetrics = nsnull;
  mContext = nsnull;
  mSurface = nsnull;
  mOffscreenSurface = nsnull;
  mCurrentColor = NS_RGB(255, 255, 255);  // set it to white
  mCurrentLineStyle = nsLineStyle_kSolid;
  mCurrentFont = nsnull;
  mTranMatrix = nsnull;
  mP2T = 1.0f;
  mClipRegion = nsnull;
  mDrawStringBuf = nsnull;
  mGC = nsnull;

  mFunction = GDK_COPY;

  PushState();
}

nsRenderingContextGTK::~nsRenderingContextGTK()
{
  // Destroy the State Machine
  PRInt32 cnt = mStateCache.Count();

  while (--cnt >= 0)
  {
    PRBool  clipstate;
    PopState(clipstate);
  }

  if (mTranMatrix)
    delete mTranMatrix;
  NS_IF_RELEASE(mOffscreenSurface);
  NS_IF_RELEASE(mFontMetrics);
  NS_IF_RELEASE(mContext);

  if (nsnull != mDrawStringBuf) {
    delete [] mDrawStringBuf;
  }

  if (nsnull != mGC) {
    gdk_gc_unref(mGC);
  }
}

/*static*/ nsresult
nsRenderingContextGTK::Shutdown()
{
  delete gcCache;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::Init(nsIDeviceContext* aContext,
                                          nsIWidget *aWindow)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

//  ::gdk_rgb_init();

  mSurface = new nsDrawingSurfaceGTK();

  if (mSurface)
  {
    if (!aWindow) return NS_ERROR_NULL_POINTER;

    // we want to ref the window here so that we can unref in the drawing surface.
    // otherwise, we can not unref and that causes windows that are created in the
    // drawing surface not to be freed.
    GdkDrawable *win = (GdkDrawable*)aWindow->GetNativeData(NS_NATIVE_WINDOW);
    if (win)
      gdk_window_ref((GdkWindow*)win);
    else
    {
      GtkWidget *w = (GtkWidget *) aWindow->GetNativeData(NS_NATIVE_WIDGET);

      if (!w)
      {
          delete mSurface;
          mSurface = nsnull;
          return NS_ERROR_NULL_POINTER;
      }

      win = gdk_pixmap_new(nsnull,
                           w->allocation.width,
                           w->allocation.height,
                           gdk_rgb_get_visual()->depth);
    }

    GdkGC *gc = (GdkGC *)aWindow->GetNativeData(NS_NATIVE_GRAPHIC);
    mSurface->Init(win,gc);

    mOffscreenSurface = mSurface;

    NS_ADDREF(mSurface);

    // aWindow->GetNativeData() ref'd the gc.
    // only Win32 has a FreeNativeData() method.
    // so do this manually here.
    gdk_gc_unref(gc);
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
  mContext->GetDevUnitsToAppUnits(mP2T);
  float app2dev;
  mContext->GetAppUnitsToDevUnits(app2dev);
  mTranMatrix->AddScale(app2dev, app2dev);

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
#ifdef DEBUG
  g_print("nsRenderingContextGTK::Reset() called\n");
#endif
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}
#if 0
NS_IMETHODIMP nsRenderingContextGTK::PushState(PRInt32 aFlags)
{
  //  Get a new GS
#ifdef USE_GS_POOL
  nsGraphicsState *state = nsGraphicsStatePool::GetNewGS();
#else
  nsGraphicsState *state = new nsGraphicsState;
#endif
  // Push into this state object, add to vector
  if (!state)
    return NS_ERROR_FAILURE;

  if (aFlags & NS_STATE_COLOR) {
    state->mColor = mCurrentColor;
  }

  if (aFlags & NS_STATE_TRANSFORM) {
    state->mMatrix = mTranMatrix;
    if (nsnull == mTranMatrix) {
      mTranMatrix = new nsTransform2D();
    } else {
      mTranMatrix = new nsTransform2D(mTranMatrix);
    }
  }

  if (aFlags & NS_STATE_FONT) {
    NS_IF_ADDREF(mFontMetrics);
    state->mFontMetrics = mFontMetrics;
  }

  if (aFlags & NS_STATE_CLIP) {
    state->mClipRegion = mClipRegion;
  }

  if (aFlags & NS_STATE_LINESTYLE) {
    state->mLineStyle = mCurrentLineStyle;
  }

  mStateCache.AppendElement(state);
  
  return NS_OK;
}
#endif
NS_IMETHODIMP nsRenderingContextGTK::PushState(void)
{
  //  Get a new GS
#ifdef USE_GS_POOL
  nsGraphicsState *state = nsGraphicsStatePool::GetNewGS();
#else
  nsGraphicsState *state = new nsGraphicsState;
#endif
  // Push into this state object, add to vector
  if (!state)
    return NS_ERROR_FAILURE;

  state->mMatrix = mTranMatrix;

  if (nsnull == mTranMatrix)
    mTranMatrix = new nsTransform2D();
  else
    mTranMatrix = new nsTransform2D(mTranMatrix);

  // set state to mClipRegion.. SetClip{Rect,Region}() will do copy-on-write stuff
  state->mClipRegion = mClipRegion;

  NS_IF_ADDREF(mFontMetrics);
  state->mFontMetrics = mFontMetrics;

  state->mColor = mCurrentColor;
  state->mLineStyle = mCurrentLineStyle;

  mStateCache.AppendElement(state);
  
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::PopState(PRBool &aClipEmpty)
{
  PRUint32 cnt = mStateCache.Count();
  nsGraphicsState * state;

  if (cnt > 0) {
    state = (nsGraphicsState *)mStateCache.ElementAt(cnt - 1);
    mStateCache.RemoveElementAt(cnt - 1);

    // Assign all local attributes from the state object just popped
    if (state->mMatrix) {
      if (mTranMatrix)
        delete mTranMatrix;
      mTranMatrix = state->mMatrix;
    }

    mClipRegion = state->mClipRegion;

    if (state->mFontMetrics && (mFontMetrics != state->mFontMetrics))
      SetFont(state->mFontMetrics);

    if (state->mColor != mCurrentColor)
      SetColor(state->mColor);    

    if (state->mLineStyle != mCurrentLineStyle)
      SetLineStyle(state->mLineStyle);

    // Delete this graphics state object
#ifdef USE_GS_POOL
    nsGraphicsStatePool::ReleaseGS(state);
#else
    delete state;
#endif
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
  
  if (!mClipRegion)
    return NS_ERROR_FAILURE;

  if (!mClipRegion->IsEmpty()) {
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



  PRUint32 cnt = mStateCache.Count();
  nsGraphicsState *state = nsnull;

  if (cnt > 0) {
    state = (nsGraphicsState *)mStateCache.ElementAt(cnt - 1);
  }

  if (state) {
    if (state->mClipRegion) {
      if (state->mClipRegion == mClipRegion) {
        nsCOMPtr<nsIRegion> tmpRgn;
        GetClipRegion(getter_AddRefs(tmpRgn));
        mClipRegion = tmpRgn;
      }
    }
  }

  CreateClipRegion();

  nsRect trect = aRect;

#ifdef TRACE_SET_CLIP
  printf("nsRenderingContextGTK::SetClipRect(%s)\n",
         nsClipCombine_to_string(aCombine));
#endif // TRACE_SET_CLIP

  mTranMatrix->TransformCoord(&trect.x, &trect.y,
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
#if 0
  nscolor color = mCurrentColor;
  SetColor(NS_RGB(255,   0,   0));
  FillRect(aRect);
  SetColor(color);
#endif
  aClipEmpty = mClipRegion->IsEmpty();

  return NS_OK;
}

void nsRenderingContextGTK::UpdateGC()
{
  GdkGCValues values;
  GdkGCValuesMask valuesMask;

  if (mGC)
    gdk_gc_unref(mGC);

  memset(&values, 0, sizeof(GdkGCValues));

  values.foreground.pixel = gdk_rgb_xpixel_from_rgb(NS_TO_GDK_RGB(mCurrentColor));
  valuesMask = GDK_GC_FOREGROUND;

  if ((mCurrentFont) && (mCurrentFont->GetGDKFont())) {
    valuesMask = GdkGCValuesMask(valuesMask | GDK_GC_FONT);
    values.font = mCurrentFont->GetGDKFont();
  }

  valuesMask = GdkGCValuesMask(valuesMask | GDK_GC_LINE_STYLE);
  values.line_style = mLineStyle;

  valuesMask = GdkGCValuesMask(valuesMask | GDK_GC_FUNCTION);
  values.function = mFunction;

  GdkRegion *rgn = nsnull;
  if (mClipRegion) {
    mClipRegion->GetNativeRegion((void*&)rgn);
  }

  if (!gcCache) {
    gcCache = new nsGCCache();
    if (!gcCache) return;
  }

  mGC = gcCache->GetGC(mSurface->GetDrawable(),
                       &values,
                       valuesMask,
                       rgn);

  if (mDashes)
    ::XSetDashes(GDK_DISPLAY(), GDK_GC_XGC(mGC),
                 0, mDashList, mDashes);
}

NS_IMETHODIMP nsRenderingContextGTK::SetClipRegion(const nsIRegion& aRegion,
                                                   nsClipCombine aCombine,
                                                   PRBool &aClipEmpty)
{

  PRUint32 cnt = mStateCache.Count();
  nsGraphicsState *state = nsnull;

  if (cnt > 0) {
    state = (nsGraphicsState *)mStateCache.ElementAt(cnt - 1);
  }

  if (state) {
    if (state->mClipRegion) {
      if (state->mClipRegion == mClipRegion) {
        nsCOMPtr<nsIRegion> tmpRgn;
        GetClipRegion(getter_AddRefs(tmpRgn));
        mClipRegion = tmpRgn;
      }
    }
  }

  CreateClipRegion();

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

  return NS_OK;
}

/**
 * Fills in |aRegion| with a copy of the current clip region.
 */
NS_IMETHODIMP nsRenderingContextGTK::CopyClipRegion(nsIRegion &aRegion)
{
  if (!mClipRegion)
    return NS_ERROR_FAILURE;

  aRegion.SetTo(*mClipRegion);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetClipRegion(nsIRegion **aRegion)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (!aRegion || !mClipRegion)
    return NS_ERROR_NULL_POINTER;

  if (mClipRegion) {
    if (*aRegion) { // copy it, they should be using CopyClipRegion 
      (*aRegion)->SetTo(*mClipRegion);
      rv = NS_OK;
    } else {
      nsCOMPtr<nsIRegion> newRegion = do_CreateInstance(kRegionCID, &rv);
      if (NS_SUCCEEDED(rv)) {
        newRegion->Init();
        newRegion->SetTo(*mClipRegion);
        NS_ADDREF(*aRegion = newRegion);
      }
    }
  } else {
#ifdef DEBUG
    printf("null clip region, can't make a valid copy\n");
#endif
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

NS_IMETHODIMP nsRenderingContextGTK::SetColor(nscolor aColor)
{
  if (nsnull == mContext)  
    return NS_ERROR_FAILURE;

  mCurrentColor = aColor;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetColor(nscolor &aColor) const
{
  aColor = mCurrentColor;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::SetFont(const nsFont& aFont)
{
  nsCOMPtr<nsIFontMetrics> newMetrics;
  nsresult rv = mContext->GetMetricsFor(aFont, *getter_AddRefs(newMetrics));
  if (NS_SUCCEEDED(rv)) {
    rv = SetFont(newMetrics);
  }
  return rv;
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
    mCurrentFont = (nsFontGTK*) fontHandle;
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
        {
          mLineStyle = GDK_LINE_SOLID;
          mDashes = 0;
          /*          ::gdk_gc_set_line_attributes(mSurface->GetGC(),
                                       1,
                                       GDK_LINE_SOLID,
                                       (GdkCapStyle)0,
                                       (GdkJoinStyle)0);
          */
        }
        break;

      case nsLineStyle_kDashed:
        {
          mLineStyle = GDK_LINE_ON_OFF_DASH;
          mDashList[0] = mDashList[1] = 4;
          mDashes = 2;

          /*          ::gdk_gc_set_dashes(mSurface->GetGC(), 
                      0, dashed, 2);
          */
        }
        break;

      case nsLineStyle_kDotted:
        {
          mDashList[0] = mDashList[1] = 1;
          mLineStyle = GDK_LINE_ON_OFF_DASH;
          mDashes = 2;

          /*          ::gdk_gc_set_dashes(mSurface->GetGC(), 
                      0, dotted, 2);
          */
        }
        break;

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
  mTranMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextGTK::Scale(float aSx, float aSy)
{
  mTranMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mTranMatrix;
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
    if (!mGC)
      UpdateGC();
    surf->Init(mGC, aBounds->width, aBounds->height, aSurfFlags);
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
  nscoord diffX,diffY;

  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  mTranMatrix->TransformCoord(&aX0,&aY0);
  mTranMatrix->TransformCoord(&aX1,&aY1);

  diffX = aX1-aX0;
  diffY = aY1-aY0;

  if (0!=diffX) {
    diffX = (diffX>0?1:-1);
  }
  if (0!=diffY) {
    diffY = (diffY>0?1:-1);
  }

  UpdateGC();

  ::gdk_draw_line(mSurface->GetDrawable(),
                  mGC,
                  aX0, aY0, aX1-diffX, aY1-diffY);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawStdLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  nscoord diffX,diffY;

  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  diffX = aX1 - aX0;
  diffY = aY1 - aY0;

  if (0!=diffX) {
    diffX = (diffX>0?1:-1);
  }
  if (0!=diffY) {
    diffY = (diffY>0?1:-1);
  }

  UpdateGC();

  ::gdk_draw_line(mSurface->GetDrawable(),mGC,aX0, aY0, aX1-diffX, aY1-diffY);

  return NS_OK;
}


NS_IMETHODIMP nsRenderingContextGTK::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PRInt32 i;

  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  GdkPoint *pts = new GdkPoint[aNumPoints];
	for (i = 0; i < aNumPoints; i++)
  {
    nsPoint p = aPoints[i];
    mTranMatrix->TransformCoord(&p.x,&p.y);
    pts[i].x = p.x;
    pts[i].y = p.y;
#ifdef DEBUG
    printf("(%i,%i)\n", p.x, p.y);
#endif
  }

  UpdateGC();

  ::gdk_draw_lines(mSurface->GetDrawable(),
                   mGC,
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
  if (nsnull == mTranMatrix || nsnull == mSurface) {
    return NS_ERROR_FAILURE;
  }

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  g_return_val_if_fail ((mSurface->GetDrawable() != NULL) ||
                        (mGC != NULL), NS_ERROR_FAILURE);

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  // After the transform, if the numbers are huge, chop them, because
  // they're going to be converted from 32 bit to 16 bit.
  // It's all way off the screen anyway.
  ConditionRect(x,y,w,h);

  // Don't draw empty rectangles; also, w/h are adjusted down by one
  // so that the right number of pixels are drawn.
  if (w && h) {

    UpdateGC();

    ::gdk_draw_rectangle(mSurface->GetDrawable(), mGC,
                         FALSE,
                         x, y,
                         w - 1,
                         h - 1);
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::FillRect(const nsRect& aRect)
{
  return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextGTK::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  if (nsnull == mTranMatrix || nsnull == mSurface) {
    return NS_ERROR_FAILURE;
  }

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  // After the transform, if the numbers are huge, chop them, because
  // they're going to be converted from 32 bit to 16 bit.
  // It's all way off the screen anyway.
  ConditionRect(x,y,w,h);

  UpdateGC();

  ::gdk_draw_rectangle(mSurface->GetDrawable(), mGC,
                       TRUE,
                       x, y, w, h);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::InvertRect(const nsRect& aRect)
{
  return InvertRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextGTK::InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  if (nsnull == mTranMatrix || nsnull == mSurface) {
    return NS_ERROR_FAILURE;
  }

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  // After the transform, if the numbers are huge, chop them, because
  // they're going to be converted from 32 bit to 16 bit.
  // It's all way off the screen anyway.
  ConditionRect(x,y,w,h);

  mFunction = GDK_INVERT;

  UpdateGC();

  // Fill the rect
  ::gdk_draw_rectangle(mSurface->GetDrawable(), mGC,
                       TRUE,
                       x, y, w, h);

  // Back to normal copy drawing mode
  mFunction = GDK_COPY;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  GdkPoint *pts = new GdkPoint[aNumPoints];
	for (PRInt32 i = 0; i < aNumPoints; i++)
  {
    nsPoint p = aPoints[i];
		mTranMatrix->TransformCoord(&p.x,&p.y);
		pts[i].x = p.x;
    pts[i].y = p.y;
	}

  UpdateGC();

  ::gdk_draw_polygon(mSurface->GetDrawable(), mGC, FALSE, pts, aNumPoints);

  delete[] pts;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  GdkPoint *pts = new GdkPoint[aNumPoints];
	for (PRInt32 i = 0; i < aNumPoints; i++)
  {
    nsPoint p = aPoints[i];
		mTranMatrix->TransformCoord(&p.x,&p.y);
		pts[i].x = p.x;
    pts[i].y = p.y;
	}

  UpdateGC();

  ::gdk_draw_polygon(mSurface->GetDrawable(), mGC, TRUE, pts, aNumPoints);

  delete[] pts;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::DrawEllipse(const nsRect& aRect)
{
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  UpdateGC();

  ::gdk_draw_arc(mSurface->GetDrawable(), mGC, FALSE,
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
  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  UpdateGC();

  ::gdk_draw_arc(mSurface->GetDrawable(), mGC, TRUE,
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
  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  UpdateGC();

  ::gdk_draw_arc(mSurface->GetDrawable(), mGC, FALSE,
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
  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);

  nscoord x,y,w,h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x,&y,&w,&h);

  UpdateGC();

  ::gdk_draw_arc(mSurface->GetDrawable(), mGC, TRUE,
                 x, y, w, h,
                 NSToIntRound(aStartAngle * 64.0f),
                 NSToIntRound(aEndAngle * 64.0f));

  return NS_OK;
}

// do the 8 to 16 bit conversion on the stack
// if the data is less than this size
#define WIDEN_8_TO_16_BUF_SIZE 1024

// handle 8 bit data with a 16 bit font
gint
Widen8To16AndMove(const gchar *char_p, 
                  gint char_len, 
                  XChar2b *xchar2b_p)
{
  int i;
  for (i=0; i<char_len; i++) {
    (xchar2b_p)->byte1 = 0;
    (xchar2b_p++)->byte2 = *char_p++;
  }
  return(char_len*2);
}

// handle 8 bit data with a 16 bit font
gint
Widen8To16AndGetWidth (GdkFont        *font,
                       const gchar    *text,
                       gint            text_length)
{
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;
  gint rawWidth;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)PR_Malloc(text_length*sizeof(XChar2b));
    if (!p) return(0); // handle malloc failure
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  rawWidth = gdk_text_width(font, (const gchar *)p, uchar_size);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    PR_Free((char*)p);
  }
  return(rawWidth);
}

/* static */ void
nsRenderingContextGTK::Widen8To16AndDraw (GdkDrawable *drawable,
                                          GdkFont     *font,
                                          GdkGC       *gc,
                                          gint         x,
                                          gint         y,
                                          const gchar *text,
                                          gint         text_length)
{
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)PR_Malloc(text_length*sizeof(XChar2b));
    if (!p) return; // handle malloc failure
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  my_gdk_draw_text(drawable, font, gc, x, y, (const gchar *)p, uchar_size);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    PR_Free((char*)p);
  }
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(char aC, nscoord &aWidth)
{
    // Check for the very common case of trying to get the width of a single
    // space.
  if ((aC == ' ') && (nsnull != mFontMetrics)) {
    nsFontMetricsGTK* fontMetricsGTK = (nsFontMetricsGTK*)mFontMetrics;
    return fontMetricsGTK->GetSpaceWidth(aWidth);
  }
  return GetWidth(&aC, 1, aWidth);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(PRUnichar aC, nscoord& aWidth,
                                PRInt32* aFontID)
{
  return GetWidth(&aC, 1, aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const nsString& aString,
                                nscoord& aWidth, PRInt32* aFontID)
{
  return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const char* aString, PRUint32 aLength,
                                nscoord& aWidth)
{
  if (0 == aLength) {
    aWidth = 0;
  }
  else {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mCurrentFont != NULL, NS_ERROR_FAILURE);
    gint rawWidth;
    if (!mCurrentFont->GetGDKFontIs10646()) {
      // 8 bit data with an 8 bit font
      rawWidth = gdk_text_width (mCurrentFont->GetGDKFont(), aString, aLength);
    }
    else {
      // we have 8 bit data but a 16 bit font
      rawWidth = Widen8To16AndGetWidth (mCurrentFont->GetGDKFont(), aString, aLength);
    }
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

    g_return_val_if_fail(metrics != NULL, NS_ERROR_FAILURE);

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
        if (CCMAP_HAS_CHAR((*font)->mCCMap, c)) {
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
          rawWidth += prevFont->GetWidth(&aString[start], i - start);
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
      rawWidth += prevFont->GetWidth(&aString[start], i - start);
    }

    aWidth = NSToCoordRound(rawWidth * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextGTK::DrawString(const char *aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  const nscoord* aSpacing)
{
  if (0 != aLength) {
    g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mCurrentFont != NULL, NS_ERROR_FAILURE);

    nscoord x = aX;
    nscoord y = aY;

    // Substract xFontStruct ascent since drawing specifies baseline
    if (mFontMetrics) {
      mFontMetrics->GetMaxAscent(y);
      y += aY;
    }

    UpdateGC();

    if (nsnull != aSpacing) {
      // Render the string, one character at a time...
      const char* end = aString + aLength;
      while (aString < end) {
        char ch = *aString++;
        nscoord xx = x;
        nscoord yy = y;
        mTranMatrix->TransformCoord(&xx, &yy);
        if (!mCurrentFont->GetGDKFontIs10646()) {
          // 8 bit data with an 8 bit font
          nsRenderingContextGTK::my_gdk_draw_text(mSurface->GetDrawable(), 
                                                mCurrentFont->GetGDKFont(), mGC,
                                                xx, yy, &ch, 1);
        }
        else {
          // we have 8 bit data but a 16 bit font
          Widen8To16AndDraw(mSurface->GetDrawable(), mCurrentFont->GetGDKFont(), mGC,
                                                xx, yy, &ch, 1);
        }
        x += *aSpacing++;
      }
    }
    else {
      mTranMatrix->TransformCoord(&x, &y);
      if (!mCurrentFont->GetGDKFontIs10646()) { // keep 8 bit path fast
        // 8 bit data with an 8 bit font
        nsRenderingContextGTK::my_gdk_draw_text (mSurface->GetDrawable(),
                                                 mCurrentFont->GetGDKFont(), mGC,
                                                 x, y, aString, aLength);
        }
        else {
          // we have 8 bit data but a 16 bit font
          Widen8To16AndDraw(mSurface->GetDrawable(), mCurrentFont->GetGDKFont(), mGC,
                                               x, y, aString, aLength);
        }
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

NS_IMETHODIMP
nsRenderingContextGTK::DrawString(const PRUnichar* aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  PRInt32 aFontID,
                                  const nscoord* aSpacing)
{
  if (aLength && mFontMetrics) {
    g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

    nscoord x = aX;
    nscoord y;

    // Substract xFontStruct ascent since drawing specifies baseline
    mFontMetrics->GetMaxAscent(y);
    y += aY;
    aY = y;

    mTranMatrix->TransformCoord(&x, &y);

    nsFontMetricsGTK* metrics = (nsFontMetricsGTK*) mFontMetrics;
    nsFontGTK* prevFont = nsnull;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontGTK* currFont = nsnull;
      nsFontGTK** font = metrics->mLoadedFonts;
      nsFontGTK** lastFont = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < lastFont) {
        if (CCMAP_HAS_CHAR((*font)->mCCMap, c)) {
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

            // save off mCurrentFont and set it so that we cache the GC's font correctly
            nsFontGTK *oldFont = mCurrentFont;
            mCurrentFont = prevFont;
            UpdateGC();

            while (str < end) {
              x = aX;
              y = aY;
              mTranMatrix->TransformCoord(&x, &y);
              prevFont->DrawString(this, mSurface, x, y, str, 1);
              aX += *aSpacing++;
              str++;
            }
            mCurrentFont = oldFont;
          }
          else {
            nsFontGTK *oldFont = mCurrentFont;
            mCurrentFont = prevFont;
            UpdateGC();
            x += prevFont->DrawString(this, mSurface, x, y, &aString[start],
                                      i - start);
            mCurrentFont = oldFont;
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

      nsFontGTK *oldFont = mCurrentFont;
      mCurrentFont = prevFont;
      UpdateGC();
    
      if (aSpacing) {
        const PRUnichar* str = &aString[start];
        const PRUnichar* end = &aString[i];
        while (str < end) {
          x = aX;
          y = aY;
          mTranMatrix->TransformCoord(&x, &y);
          prevFont->DrawString(this, mSurface, x, y, str, 1);
          aX += *aSpacing++;
          str++;
        }

      }
      else {
        prevFont->DrawString(this, mSurface, x, y, &aString[start], i - start);
      }

      mCurrentFont = oldFont;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextGTK::DrawString(const nsString& aString,
                                  nscoord aX, nscoord aY,
                                  PRInt32 aFontID,
                                  const nscoord* aSpacing)
{
  return DrawString(aString.get(), aString.Length(),
                    aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage,
                                               nscoord aX, nscoord aY)
{
  nscoord width, height;

  // we have to do this here because we are doing a transform below
  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());

  return DrawImage(aImage, aX, aY, width, height);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  return DrawImage(aImage,
                   aRect.x,
                   aRect.y,
                   aRect.width,
                   aRect.height);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage,
                                               nscoord aX, nscoord aY,
                                               nscoord aWidth, nscoord aHeight)
{
  nscoord x, y, w, h;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mTranMatrix->TransformCoord(&x, &y, &w, &h);

#if 0
  //  gdk_window_clear_area(mSurface->GetDrawable(), x, y, w, h);
  PRInt32 xx, yy, ww, hh;
  mClipRegion->GetBoundingBox(&xx,&yy,&ww,&hh);
  printf("clip bounds: x = %i, y = %i, w = %i, h = %i\n", xx, yy, ww, hh);

  nscolor color = mCurrentColor;
  SetColor(NS_RGB(255,   0,   0));
  FillRect(xx, yy, ww, hh);
  SetColor(color);
#endif

  UpdateGC();

  return aImage->Draw(*this, mSurface,
                      x, y, w, h);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(nsIImage *aImage,
                                               const nsRect& aSRect,
                                               const nsRect& aDRect)
{
  nsRect	sr,dr;

  sr = aSRect;
  mTranMatrix->TransformCoord(&sr.x, &sr.y,
                            &sr.width, &sr.height);
  sr.x -= mTranMatrix->GetXTranslationCoord();
  sr.y -= mTranMatrix->GetYTranslationCoord();

  dr = aDRect;
  mTranMatrix->TransformCoord(&dr.x, &dr.y,
                           &dr.width, &dr.height);

#if 0
  PRInt32 x, y, w, h;
  mClipRegion->GetBoundingBox(&x,&y,&w,&h);
  printf("clip bounds: x = %i, y = %i, w = %i, h = %i\n", x, y, w, h);

  //  gdk_window_clear_area(mSurface->GetDrawable(), sr.x, sr.y, sr.width, sr.height);

  nscolor color = mCurrentColor;
  SetColor(NS_RGB(255,   0,   0));
  FillRect(x, y, w, h);
  SetColor(color);
#endif

  UpdateGC();

  return aImage->Draw(*this, mSurface,
                      sr.x, sr.y,
                      sr.width, sr.height,
                      dr.x, dr.y,
                      dr.width, dr.height);
}

NS_IMETHODIMP
nsRenderingContextGTK::CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                         const nsRect &aDestBounds,
                                         PRUint32 aCopyFlags)
{
  PRInt32               srcX = aSrcX;
  PRInt32               srcY = aSrcY;
  nsRect                drect = aDestBounds;
  nsDrawingSurfaceGTK  *destsurf;

  g_return_val_if_fail(aSrcSurf != NULL, NS_ERROR_FAILURE);
  g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
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
    mTranMatrix->TransformCoord(&srcX, &srcY);

  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mTranMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

#if 0
  // XXX impliment me
  if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
  {
    // we should use the source clip region if this flag is used...
    nsIRegion *region;
    CopyClipRegion();
  }
#endif

  //XXX flags are unused. that would seem to mean that there is
  //inefficiency somewhere... MMP

  // gdk_draw_pixmap and copy_area do the same thing internally.
  // copy_area sounds better

  UpdateGC();

  ::gdk_window_copy_area(destsurf->GetDrawable(),
                         mGC,
                         drect.x, drect.y,
                         ((nsDrawingSurfaceGTK *)aSrcSurf)->GetDrawable(),
                         srcX, srcY,
                         drect.width, drect.height);
                     

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
  return NS_OK;
}

#ifdef MOZ_MATHML

// bstell Sept. 22, 2000: 
// added code to handle displaying 8 bit data using a 16 bit font.
// have not tested the MOZ_MATHML code path

void
Widen8To16AndGetTextExtents (GdkFont     *font,  
                        const gchar *text,
                        gint         text_length,
                        gint        *lbearing,
                        gint        *rbearing,
                        gint        *width,
                        gint        *ascent,
                        gint        *descent)
{
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)PR_Malloc(text_length*sizeof(XChar2b));
    if (!p) { // handle malloc failure
      *lbearing = 0;
      *rbearing = 0;
      *width    = 0;
      *ascent   = 0;
      *descent  = 0;
      return;
    }
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  gdk_text_extents (font, (const char*)p, uchar_size,
                    lbearing, 
                    rbearing, 
                    width, 
                    ascent, 
                    descent);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    PR_Free((char*)p);
  }
}

NS_IMETHODIMP
nsRenderingContextGTK::GetBoundingMetrics(const char*        aString, 
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics)
{
  aBoundingMetrics.Clear();
  if (aString && 0 < aLength) {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mCurrentFont != NULL, NS_ERROR_FAILURE);
    if (!mCurrentFont->GetGDKFontIs10646()) {
        // 8 bit data with an 8 bit font
        gdk_text_extents (mCurrentFont->GetGDKFont(), aString, aLength,
                                &aBoundingMetrics.leftBearing, 
                                &aBoundingMetrics.rightBearing, 
                                &aBoundingMetrics.width, 
                                &aBoundingMetrics.ascent, 
                                &aBoundingMetrics.descent);
    }
    else {
        // we have 8 bit data but a 16 bit font
        Widen8To16AndGetTextExtents (mCurrentFont->GetGDKFont(), aString, aLength,
                          &aBoundingMetrics.leftBearing, 
                          &aBoundingMetrics.rightBearing, 
                          &aBoundingMetrics.width, 
                          &aBoundingMetrics.ascent, 
                          &aBoundingMetrics.descent);
    }

    aBoundingMetrics.leftBearing = NSToCoordRound(aBoundingMetrics.leftBearing * mP2T);
    aBoundingMetrics.rightBearing = NSToCoordRound(aBoundingMetrics.rightBearing * mP2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * mP2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * mP2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * mP2T);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextGTK::GetBoundingMetrics(const PRUnichar*   aString, 
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics,
                                          PRInt32*           aFontID)
{
  aBoundingMetrics.Clear(); 
  if (0 < aLength) {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

    nsFontMetricsGTK* metrics = (nsFontMetricsGTK*) mFontMetrics;
    nsFontGTK* prevFont = nsnull;

    nsBoundingMetrics rawbm;
    PRBool firstTime = PR_TRUE;
    PRUint32 start = 0;
    PRUint32 i;
    for (i = 0; i < aLength; i++) {
      PRUnichar c = aString[i];
      nsFontGTK* currFont = nsnull;
      nsFontGTK** font = metrics->mLoadedFonts;
      nsFontGTK** end = &metrics->mLoadedFonts[metrics->mLoadedFontsCount];
      while (font < end) {
        if (CCMAP_HAS_CHAR((*font)->mCCMap, c)) {
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
          prevFont->GetBoundingMetrics((const PRUnichar*) &aString[start],
                                       i - start, rawbm);
          if (firstTime) {
            firstTime = PR_FALSE;
            aBoundingMetrics = rawbm;
          } 
          else {
            aBoundingMetrics += rawbm;
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
      prevFont->GetBoundingMetrics((const PRUnichar*) &aString[start],
                                   i - start, rawbm);
      if (firstTime) {
        aBoundingMetrics = rawbm;
      }
      else {
        aBoundingMetrics += rawbm;
      }
    }
    // convert to app units
    aBoundingMetrics.leftBearing = NSToCoordRound(aBoundingMetrics.leftBearing * mP2T);
    aBoundingMetrics.rightBearing = NSToCoordRound(aBoundingMetrics.rightBearing * mP2T);
    aBoundingMetrics.width = NSToCoordRound(aBoundingMetrics.width * mP2T);
    aBoundingMetrics.ascent = NSToCoordRound(aBoundingMetrics.ascent * mP2T);
    aBoundingMetrics.descent = NSToCoordRound(aBoundingMetrics.descent * mP2T);
  }
  if (nsnull != aFontID)
    *aFontID = 0;

  return NS_OK;
}
#endif /* MOZ_MATHML */


/* static */ void
nsRenderingContextGTK::my_gdk_draw_text (GdkDrawable *drawable,
                                         GdkFont     *font,
                                         GdkGC       *gc,
                                         gint         x,
                                         gint         y,
                                         const gchar *text,
                                         gint         text_length)
{
  GdkWindowPrivate *drawable_private;
  GdkFontPrivate *font_private;
  GdkGCPrivate *gc_private;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (font != NULL);
  g_return_if_fail (gc != NULL);
  g_return_if_fail (text != NULL);

  drawable_private = (GdkWindowPrivate*) drawable;
  if (drawable_private->destroyed)
    return;
  gc_private = (GdkGCPrivate*) gc;
  font_private = (GdkFontPrivate*) font;

  if (font->type == GDK_FONT_FONT)
  {
    XFontStruct *xfont = (XFontStruct *) font_private->xfont;

    // gdk does this... we don't need it..
    //    XSetFont(drawable_private->xdisplay, gc_private->xgc, xfont->fid);

    if ((xfont->min_byte1 == 0) && (xfont->max_byte1 == 0))
    {
      XDrawString (drawable_private->xdisplay, drawable_private->xwindow,
                   gc_private->xgc, x, y, text, text_length);
    }
    else
    {
      XDrawString16 (drawable_private->xdisplay, drawable_private->xwindow,
                     gc_private->xgc, x, y, (XChar2b *) text, text_length / 2);
    }
  }
  else if (font->type == GDK_FONT_FONTSET)
  {
    XFontSet fontset = (XFontSet) font_private->xfont;
    XmbDrawString (drawable_private->xdisplay, drawable_private->xwindow,
                   fontset, gc_private->xgc, x, y, text, text_length);
  }
  else
    g_error("undefined font type\n");
}



NS_IMETHODIMP nsRenderingContextGTK::DrawImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsPoint * aDestPoint)
{
  UpdateGC();
  return nsRenderingContextImpl::DrawImage(aImage, aSrcRect, aDestPoint);
}

NS_IMETHODIMP nsRenderingContextGTK::DrawScaledImage(imgIContainer *aImage, const nsRect * aSrcRect, const nsRect * aDestRect)
{
  UpdateGC();
  return nsRenderingContextImpl::DrawScaledImage(aImage, aSrcRect, aDestRect);
}
