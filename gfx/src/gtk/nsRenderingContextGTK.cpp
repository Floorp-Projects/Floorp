/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tomi Leppikangas <tomi.leppikangas@oulu.fi>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsFixedSizeAllocator.h"
#include "nsRenderingContextGTK.h"
#include "nsRegionGTK.h"
#include "nsImageGTK.h"
#include "nsGraphicsStateGTK.h"
#include "nsCompressedCharMap.h"
#include <math.h>
#include "nsGCCache.h"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include "prmem.h"
#include "prenv.h"

#include "nsIFontMetricsGTK.h"
#include "nsDeviceContextGTK.h"
#include "nsFontMetricsUtils.h"

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdkwindow.h>
#endif

NS_IMPL_ISUPPORTS1(nsRenderingContextGTK, nsIRenderingContext)

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

#define NSRECT_TO_GDKRECT(ns,gdk) \
  PR_BEGIN_MACRO \
  gdk.x = ns.x; \
  gdk.y = ns.y; \
  gdk.width = ns.width; \
  gdk.height = ns.height; \
  PR_END_MACRO

static nsGCCache *gcCache = nsnull;
static nsFixedSizeAllocator *gStatePool = nsnull;

nsRenderingContextGTK::nsRenderingContextGTK()
{
  mFontMetrics = nsnull;
  mContext = nsnull;
  mSurface = nsnull;
  mOffscreenSurface = nsnull;
  mCurrentColor = NS_RGB(255, 255, 255);  // set it to white
  mCurrentLineStyle = nsLineStyle_kSolid;
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
    PopState();

  if (mTranMatrix) {
    if (gStatePool) {
      mTranMatrix->~nsTransform2D();
      gStatePool->Free(mTranMatrix, sizeof(nsTransform2D));
    } else {
      delete mTranMatrix;
    }
  }
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
  delete gStatePool;
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
#ifdef MOZ_WIDGET_GTK2
      gdk_drawable_set_colormap(win, gdk_rgb_get_colormap());
#endif
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
  mOffscreenSurface = mSurface;

  return (CommonInit());
}

NS_IMETHODIMP nsRenderingContextGTK::CommonInit()
{
  mP2T = mContext->DevUnitsToAppUnits();
  float app2dev;
  app2dev = mContext->AppUnitsToDevUnits();
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

  // see if the font metrics has anything more to offer
  result |= NS_FontMetricsGetHints();

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
  PopState();

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
  if (!gStatePool) {
    gStatePool = new nsFixedSizeAllocator();
    size_t sizes[] = {sizeof(nsGraphicsState), sizeof(nsTransform2D)};
    if (gStatePool)
      gStatePool->Init("GTKStatePool", sizes, sizeof(sizes)/sizeof(size_t),
                       sizeof(nsGraphicsState)*64);
  }

  nsGraphicsState *state = nsnull;
  if (gStatePool) {
    void *space = gStatePool->Alloc(sizeof(nsGraphicsState));
    if (space)
      state = ::new(space) nsGraphicsState;
  } else {
    state = new nsGraphicsState;
  }

  // Push into this state object, add to vector
  if (!state)
    return NS_ERROR_FAILURE;

  state->mMatrix = mTranMatrix;

  if (gStatePool) {
    void *space = gStatePool->Alloc(sizeof(nsTransform2D));
    if (mTranMatrix)
      mTranMatrix = ::new(space) nsTransform2D(mTranMatrix);
    else
      mTranMatrix = ::new(space) nsTransform2D();
  } else {
    if (mTranMatrix)
      mTranMatrix = ::new nsTransform2D(mTranMatrix);
    else
      mTranMatrix = ::new nsTransform2D();
  }

  // set state to mClipRegion.. SetClip{Rect,Region}() will do copy-on-write stuff
  state->mClipRegion = mClipRegion;

  NS_IF_ADDREF(mFontMetrics);
  state->mFontMetrics = mFontMetrics;

  state->mColor = mCurrentColor;
  state->mLineStyle = mCurrentLineStyle;

  mStateCache.AppendElement(state);
  
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextGTK::PopState(void)
{
  PRUint32 cnt = mStateCache.Count();
  nsGraphicsState * state;

  if (cnt > 0) {
    state = (nsGraphicsState *)mStateCache.ElementAt(cnt - 1);
    mStateCache.RemoveElementAt(cnt - 1);

    // Assign all local attributes from the state object just popped
    if (state->mMatrix) {
      if (mTranMatrix) {
        if (gStatePool) {
          mTranMatrix->~nsTransform2D();
          gStatePool->Free(mTranMatrix, sizeof(nsTransform2D));
        } else {
          delete mTranMatrix;
        }
      }
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
    if (gStatePool) {
      state->~nsGraphicsState();
      gStatePool->Free(state, sizeof(nsGraphicsState));
    } else {
      delete state;
    }
  }

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
// #define TRACE_SET_CLIP
#endif

#ifdef TRACE_SET_CLIP
static char *
nsClipCombine_to_string(nsClipCombine aCombine)
{
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
                                                 nsClipCombine aCombine)
{
  nsRect trect = aRect;
  mTranMatrix->TransformCoord(&trect.x, &trect.y,
                              &trect.width, &trect.height);
  SetClipRectInPixels(trect, aCombine);
  return NS_OK;
}

void nsRenderingContextGTK::SetClipRectInPixels(const nsRect& aRect,
                                                nsClipCombine aCombine)
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

#ifdef TRACE_SET_CLIP
  printf("nsRenderingContextGTK::SetClipRect(%s)\n",
         nsClipCombine_to_string(aCombine));
#endif // TRACE_SET_CLIP

  switch(aCombine)
  {
    case nsClipCombine_kIntersect:
      mClipRegion->Intersect(aRect.x,aRect.y,aRect.width,aRect.height);
      break;
    case nsClipCombine_kUnion:
      mClipRegion->Union(aRect.x,aRect.y,aRect.width,aRect.height);
      break;
    case nsClipCombine_kSubtract:
      mClipRegion->Subtract(aRect.x,aRect.y,aRect.width,aRect.height);
      break;
    case nsClipCombine_kReplace:
      mClipRegion->SetTo(aRect.x,aRect.y,aRect.width,aRect.height);
      break;
  }
#if 0
  nscolor color = mCurrentColor;
  SetColor(NS_RGB(255,   0,   0));
  FillRect(aRect);
  SetColor(color);
#endif
}

void nsRenderingContextGTK::UpdateGC()
{
  GdkGCValues values;
  GdkGCValuesMask valuesMask;

  if (mGC)
    gdk_gc_unref(mGC);

  memset(&values, 0, sizeof(GdkGCValues));

  values.foreground.pixel =
    gdk_rgb_xpixel_from_rgb(NS_TO_GDK_RGB(mCurrentColor));
  valuesMask = GDK_GC_FOREGROUND;

#ifdef MOZ_ENABLE_COREXFONTS
  if (mFontMetrics) {
    GdkFont *font = mFontMetrics->GetCurrentGDKFont();
    if (font) {
      valuesMask = GdkGCValuesMask(valuesMask | GDK_GC_FONT);
      values.font = font;
    }
  }
#endif

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
                                                   nsClipCombine aCombine)
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

NS_IMETHODIMP nsRenderingContextGTK::SetFont(const nsFont& aFont, nsIAtom* aLangGroup)
{
  nsCOMPtr<nsIFontMetrics> newMetrics;
  nsresult rv = mContext->GetMetricsFor(aFont, aLangGroup, *getter_AddRefs(newMetrics));
  if (NS_SUCCEEDED(rv)) {
    rv = SetFont(newMetrics);
  }
  return rv;
}

NS_IMETHODIMP nsRenderingContextGTK::SetFont(nsIFontMetrics *aFontMetrics)
{
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = NS_REINTERPRET_CAST(nsIFontMetricsGTK *, aFontMetrics);
  NS_IF_ADDREF(mFontMetrics);

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

NS_IMETHODIMP nsRenderingContextGTK::CreateDrawingSurface(const nsRect &aBounds,
                                                          PRUint32 aSurfFlags,
                                                          nsDrawingSurface &aSurface)
{
  if (nsnull == mSurface) {
    aSurface = nsnull;
    return NS_ERROR_FAILURE;
  }

  g_return_val_if_fail ((aBounds.width > 0) && (aBounds.height > 0), NS_ERROR_FAILURE);
 
  nsresult rv = NS_OK;
  nsDrawingSurfaceGTK *surf = new nsDrawingSurfaceGTK();

  if (surf)
  {
    NS_ADDREF(surf);
    PushState();
    mClipRegion = nsnull;
    UpdateGC();
    rv = surf->Init(mGC, aBounds.width, aBounds.height, aSurfFlags);
    PopState();
  } else {
    rv = NS_ERROR_FAILURE;
  }

  aSurface = (nsDrawingSurface)surf;

  return rv;
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

  mFunction = GDK_XOR;

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

  if (w < 16 || h < 16) {
    /* Fix for bug 91816 ("bullets are not displayed correctly on certain text zooms")
     * De-uglify bullets on some X servers:
     * 1st: Draw... */
    ::gdk_draw_arc(mSurface->GetDrawable(), mGC, FALSE,
                   x, y, w, h,
                   0, 360 * 64);
    /*  ...then fill. */
  }
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

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(char aC, nscoord &aWidth)
{
    // Check for the very common case of trying to get the width of a single
    // space.
  if ((aC == ' ') && (nsnull != mFontMetrics)) {
    return mFontMetrics->GetSpaceWidth(aWidth);
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
    return NS_OK;
  }

  g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

  return mFontMetrics->GetWidth(aString, aLength, aWidth, this);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetWidth(const PRUnichar* aString, PRUint32 aLength,
                                nscoord& aWidth, PRInt32* aFontID)
{
  if (0 == aLength) {
    aWidth = 0;
    return NS_OK;
  }

  g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

  return mFontMetrics->GetWidth(aString, aLength, aWidth, aFontID, this);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetTextDimensions(const char* aString, PRUint32 aLength,
                                         nsTextDimensions& aDimensions)
{
  mFontMetrics->GetMaxAscent(aDimensions.ascent);
  mFontMetrics->GetMaxDescent(aDimensions.descent);
  return GetWidth(aString, aLength, aDimensions.width);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetTextDimensions(const PRUnichar* aString,
                                         PRUint32 aLength,
                                         nsTextDimensions& aDimensions, 
                                         PRInt32* aFontID)
{
  return mFontMetrics->GetTextDimensions(aString, aLength, aDimensions,
                                         aFontID, this);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetTextDimensions(const char*       aString,
                                         PRInt32           aLength,
                                         PRInt32           aAvailWidth,
                                         PRInt32*          aBreaks,
                                         PRInt32           aNumBreaks,
                                         nsTextDimensions& aDimensions,
                                         PRInt32&          aNumCharsFit,
                                         nsTextDimensions& aLastWordDimensions,
                                         PRInt32*          aFontID)
{
  return mFontMetrics->GetTextDimensions(aString, aLength, aAvailWidth,
                                         aBreaks, aNumBreaks, aDimensions,
                                         aNumCharsFit,
                                         aLastWordDimensions, aFontID,
                                         this);
}
NS_IMETHODIMP
nsRenderingContextGTK::GetTextDimensions(const PRUnichar*  aString,
                                         PRInt32           aLength,
                                         PRInt32           aAvailWidth,
                                         PRInt32*          aBreaks,
                                         PRInt32           aNumBreaks,
                                         nsTextDimensions& aDimensions,
                                         PRInt32&          aNumCharsFit,
                                         nsTextDimensions& aLastWordDimensions,
                                         PRInt32*          aFontID)
{
  return mFontMetrics->GetTextDimensions(aString, aLength, aAvailWidth,
                                         aBreaks, aNumBreaks, aDimensions,
                                         aNumCharsFit,
                                         aLastWordDimensions, aFontID,
                                         this);
}

NS_IMETHODIMP
nsRenderingContextGTK::DrawString(const char *aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  const nscoord* aSpacing)
{
  return mFontMetrics->DrawString(aString, aLength, aX, aY, aSpacing,
                                  this, mSurface);
}

NS_IMETHODIMP
nsRenderingContextGTK::DrawString(const PRUnichar* aString, PRUint32 aLength,
                                  nscoord aX, nscoord aY,
                                  PRInt32 aFontID,
                                  const nscoord* aSpacing)
{
  return mFontMetrics->DrawString(aString, aLength, aX, aY, aFontID,
                                  aSpacing, this, mSurface);
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
  {
    NS_ENSURE_TRUE(mOffscreenSurface != nsnull, NS_ERROR_FAILURE);
    destsurf = mOffscreenSurface;
  }

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

NS_IMETHODIMP
nsRenderingContextGTK::GetBoundingMetrics(const char*        aString, 
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics)
{
  return mFontMetrics->GetBoundingMetrics(aString, aLength, aBoundingMetrics,
                                          this);
}

NS_IMETHODIMP
nsRenderingContextGTK::GetBoundingMetrics(const PRUnichar*   aString, 
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics,
                                          PRInt32*           aFontID)
{
  return mFontMetrics->GetBoundingMetrics(aString, aLength, aBoundingMetrics,
                                          aFontID, this);
}

#endif /* MOZ_MATHML */

NS_IMETHODIMP nsRenderingContextGTK::DrawImage(imgIContainer *aImage, const nsRect & aSrcRect, const nsRect & aDestRect)
{
  UpdateGC();
  return nsRenderingContextImpl::DrawImage(aImage, aSrcRect, aDestRect);
}

NS_IMETHODIMP nsRenderingContextGTK::GetBackbuffer(const nsRect &aRequestedSize, const nsRect &aMaxSize, nsDrawingSurface &aBackbuffer)
{
  // Do not cache the backbuffer. On GTK it is more efficient to allocate
  // the backbuffer as needed and it doesn't cause a performance hit. @see bug 95952
  return AllocateBackbuffer(aRequestedSize, aMaxSize, aBackbuffer, PR_FALSE);
}
 
NS_IMETHODIMP nsRenderingContextGTK::ReleaseBackbuffer(void) {
  // Do not cache the backbuffer. On GTK it is more efficient to allocate
  // the backbuffer as needed and it doesn't cause a performance hit. @see bug 95952
  return DestroyCachedBackbuffer();
}
