/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsFontMetricsGTK.h"
#include "nsRenderingContextGTK.h"
#include "nsGdkUtils.h"
#include "nsRegionGTK.h"
#include "nsImageGTK.h"
#include "nsGraphicsStateGTK.h"
#include "nsCompressedCharMap.h"
#include "nsXFont.h"
#include <math.h>
#include "nsGCCache.h"
#include <gtk/gtk.h>
#include "prmem.h"
#include "prenv.h"

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdkwindow.h>
#endif

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
  NS_INIT_ISUPPORTS();

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
  
/* We can't enable fast text measuring (yet) on platforms
 * which force natural alignment of datatypes (see 
 * http://bugzilla.mozilla.org/show_bug.cgi?id=36146#c46) ... ;-(
 */
#ifndef CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT
#if defined(__i386)
#define CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT 1
#endif /* __i386 */
#endif /* !CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT */

  static PRBool enable_fast_measure;
  static PRBool getenv_done = PR_FALSE;

  /* Check for the env vars "MOZILLA_GFX_ENABLE_FAST_MEASURE" and
   * "MOZILLA_GFX_DISABLE_FAST_MEASURE" to enable/disable fast text
   * measuring (for debugging the feature and doing regression tests).
   * This code will be removed one all issues around this new feature have
   * been fixed. */
  if (!getenv_done)
  {
#ifdef CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT
    enable_fast_measure = PR_TRUE;
#else
    enable_fast_measure = PR_FALSE;
#endif /* CPU_DOES_NOT_REQUIRE_NATURAL_ALIGNMENT */

    if (PR_GetEnv("MOZILLA_GFX_ENABLE_FAST_MEASURE"))
    {
      enable_fast_measure = PR_TRUE;
    }

    if (PR_GetEnv("MOZILLA_GFX_DISABLE_FAST_MEASURE"))
    {
      enable_fast_measure = PR_FALSE;
    }
        
    getenv_done = PR_TRUE;
  } 

  if (enable_fast_measure) {
    // We have GetTextDimensions()
    result |= NS_RENDERING_HINT_FAST_MEASURE;
  }

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

  values.foreground.pixel =
    gdk_rgb_xpixel_from_rgb(NS_TO_GDK_RGB(mCurrentColor));
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
 
  nsresult rv = NS_OK;
  nsDrawingSurfaceGTK *surf = new nsDrawingSurfaceGTK();

  if (surf)
  {
    NS_ADDREF(surf);
    UpdateGC();
    rv = surf->Init(mGC, aBounds->width, aBounds->height, aSurfFlags);    
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
Widen8To16AndGetWidth (nsXFont        *xFont,
                       const gchar    *text,
                       gint            text_length)
{
  NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;
  gint rawWidth;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)PR_Malloc(text_length*sizeof(XChar2b));
    if (!p) return(0); // handle malloc failure
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  rawWidth = xFont->TextWidth16(p, uchar_size/2);

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    PR_Free((char*)p);
  }
  return(rawWidth);
}

/* static */ void
nsRenderingContextGTK::Widen8To16AndDraw (GdkDrawable *drawable,
                                          nsXFont     *xFont,
                                          GdkGC       *gc,
                                          gint         x,
                                          gint         y,
                                          const gchar *text,
                                          gint         text_length)
{
  NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
  XChar2b buf[WIDEN_8_TO_16_BUF_SIZE];
  XChar2b *p = buf;
  int uchar_size;

  if (text_length > WIDEN_8_TO_16_BUF_SIZE) {
    p = (XChar2b*)PR_Malloc(text_length*sizeof(XChar2b));
    if (!p) return; // handle malloc failure
  }

  uchar_size = Widen8To16AndMove(text, text_length, p);
  xFont->DrawText16(drawable, gc, x, y, p, uchar_size/2);

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
  }
  else {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mCurrentFont != NULL, NS_ERROR_FAILURE);
    gint rawWidth;
    nsXFont *xFont = mCurrentFont->GetXFont();
    if (mCurrentFont->IsFreeTypeFont()) {
      PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
      // need to fix this for long strings
      PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
      // convert 7 bit data to unicode
      // this function is only supposed to be called for ascii data
      for (PRUint32 i=0; i<len; i++) {
        unichars[i] = (PRUnichar)((unsigned char)aString[i]);
      }
      rawWidth = mCurrentFont->GetWidth(unichars, len);
    }
    else if (!mCurrentFont->GetXFontIs10646()) {
      NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
      // 8 bit data with an 8 bit font
      rawWidth = xFont->TextWidth8(aString, aLength);
    }
    else {
      NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
      // we have 8 bit data but a 16 bit font
      rawWidth = Widen8To16AndGetWidth (mCurrentFont->GetXFont(), aString, aLength);
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
  NS_PRECONDITION(aBreaks[aNumBreaks - 1] == aLength, "invalid break array");

  if (nsnull != mFontMetrics) {
    // If we need to back up this state represents the last place we could
    // break. We can use this to avoid remeasuring text
    PRInt32 prevBreakState_BreakIndex = -1; // not known (hasn't been computed)
    nscoord prevBreakState_Width = 0; // accumulated width to this point

    // Initialize OUT parameters
    mFontMetrics->GetMaxAscent(aLastWordDimensions.ascent);
    mFontMetrics->GetMaxDescent(aLastWordDimensions.descent);
    aLastWordDimensions.width = -1;
    aNumCharsFit = 0;

    // Iterate each character in the string and determine which font to use
    nscoord width = 0;
    PRInt32 start = 0;
    nscoord aveCharWidth;
    mFontMetrics->GetAveCharWidth(aveCharWidth);

    while (start < aLength) {
      // Estimate how many characters will fit. Do that by diving the available
      // space by the average character width. Make sure the estimated number
      // of characters is at least 1
      PRInt32 estimatedNumChars = 0;
      if (aveCharWidth > 0) {
        estimatedNumChars = (aAvailWidth - width) / aveCharWidth;
      }
      if (estimatedNumChars < 1) {
        estimatedNumChars = 1;
      }

      // Find the nearest break offset
      PRInt32 estimatedBreakOffset = start + estimatedNumChars;
      PRInt32 breakIndex;
      nscoord numChars;

      // Find the nearest place to break that is less than or equal to
      // the estimated break offset
      if (aLength <= estimatedBreakOffset) {
        // All the characters should fit
        numChars = aLength - start;
        breakIndex = aNumBreaks - 1;
      } 
      else {
        breakIndex = prevBreakState_BreakIndex;
        while (((breakIndex + 1) < aNumBreaks) &&
               (aBreaks[breakIndex + 1] <= estimatedBreakOffset)) {
          ++breakIndex;
        }
        if (breakIndex == prevBreakState_BreakIndex) {
          ++breakIndex; // make sure we advanced past the previous break index
        }
        numChars = aBreaks[breakIndex] - start;
      }

      // Measure the text
      nscoord twWidth = 0;
      if ((1 == numChars) && (aString[start] == ' ')) {
        mFontMetrics->GetSpaceWidth(twWidth);
      } 
      else if (numChars > 0)
        GetWidth( &aString[start], numChars, twWidth);

      // See if the text fits
      PRBool  textFits = (twWidth + width) <= aAvailWidth;

      // If the text fits then update the width and the number of
      // characters that fit
      if (textFits) {
        aNumCharsFit += numChars;
        width += twWidth;
        start += numChars;

        // This is a good spot to back up to if we need to so remember
        // this state
        prevBreakState_BreakIndex = breakIndex;
        prevBreakState_Width = width;
      }
      else {
        // See if we can just back up to the previous saved state and not
        // have to measure any text
        if (prevBreakState_BreakIndex > 0) {
          // If the previous break index is just before the current break index
          // then we can use it
          if (prevBreakState_BreakIndex == (breakIndex - 1)) {
            aNumCharsFit = aBreaks[prevBreakState_BreakIndex];
            width = prevBreakState_Width;
            break;
          }
        }

        // We can't just revert to the previous break state
        if (0 == breakIndex) {
          // There's no place to back up to, so even though the text doesn't fit
          // return it anyway
          aNumCharsFit += numChars;
          width += twWidth;
          break;
        }

        // Repeatedly back up until we get to where the text fits or we're all
        // the way back to the first word
        width += twWidth;
        while ((breakIndex >= 1) && (width > aAvailWidth)) {
          twWidth = 0;
          start = aBreaks[breakIndex - 1];
          numChars = aBreaks[breakIndex] - start;
          
          if ((1 == numChars) && (aString[start] == ' ')) {
            mFontMetrics->GetSpaceWidth(twWidth);
          } 
          else if (numChars > 0)
            GetWidth( &aString[start], numChars, twWidth);

          width -= twWidth;
          aNumCharsFit = start;
          breakIndex--;
        }
        break;
      }
    }

    aDimensions.width = width;
    mFontMetrics->GetMaxAscent(aDimensions.ascent);
    mFontMetrics->GetMaxDescent(aDimensions.descent);

    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

struct BreakGetTextDimensionsData {
  float    mP2T;               // IN
  PRInt32  mAvailWidth;        // IN
  PRInt32* mBreaks;            // IN
  PRInt32  mNumBreaks;         // IN
  nscoord  mSpaceWidth;        // IN
  nscoord  mAveCharWidth;      // IN
  PRInt32  mEstimatedNumChars; // IN (running -- to handle the edge case of one word)

  PRInt32  mNumCharsFit;  // IN/OUT -- accumulated number of chars that fit so far
  nscoord  mWidth;        // IN/OUT -- accumulated width so far

  // If we need to back up, this state represents the last place
  // we could break. We can use this to avoid remeasuring text
  PRInt32 mPrevBreakState_BreakIndex; // IN/OUT, initialized as -1, i.e., not yet computed
  nscoord mPrevBreakState_Width;      // IN/OUT, initialized as  0

  // Remember the fonts that we use so that we can deal with
  // line-breaking in-between fonts later. mOffsets[0] is also used
  // to initialize the current offset from where to start measuring
  nsVoidArray* mFonts;   // OUT
  nsVoidArray* mOffsets; // IN/OUT
};

static PRBool PR_CALLBACK
do_BreakGetTextDimensions(const nsFontSwitchGTK *aFontSwitch,
                          const PRUnichar*       aSubstring,
                          PRUint32               aSubstringLength,
                          void*                  aData)
{
  nsFontGTK* fontGTK = aFontSwitch->mFontGTK;

  // Make sure the font is selected
  BreakGetTextDimensionsData* data = (BreakGetTextDimensionsData*)aData;

  // Our current state relative to the _full_ string...
  // This allows emulation of the previous code...
  const PRUnichar* pstr = (const PRUnichar*)data->mOffsets->ElementAt(0);
  PRInt32 numCharsFit = data->mNumCharsFit;
  nscoord width = data->mWidth;
  PRInt32 start = (PRInt32)(aSubstring - pstr);
  PRInt32 i = start + aSubstringLength;
  PRBool allDone = PR_FALSE;

  while (start < i) {
    // Estimate how many characters will fit. Do that by dividing the
    // available space by the average character width
    PRInt32 estimatedNumChars = data->mEstimatedNumChars;
    if (!estimatedNumChars && data->mAveCharWidth > 0) {
      estimatedNumChars = (data->mAvailWidth - width) / data->mAveCharWidth;
    }
    // Make sure the estimated number of characters is at least 1
    if (estimatedNumChars < 1) {
      estimatedNumChars = 1;
    }

    // Find the nearest break offset
    PRInt32 estimatedBreakOffset = start + estimatedNumChars;
    PRInt32 breakIndex = -1; // not yet computed
    PRBool  inMiddleOfSegment = PR_FALSE;
    nscoord numChars;

    // Avoid scanning the break array in the case where we think all
    // the text should fit
    if (i <= estimatedBreakOffset) {
      // Everything should fit
      numChars = i - start;
    }
    else {
      // Find the nearest place to break that is less than or equal to
      // the estimated break offset
      breakIndex = data->mPrevBreakState_BreakIndex;
      while (data->mBreaks[breakIndex + 1] <= estimatedBreakOffset) {
        ++breakIndex;
      }

      if (breakIndex == -1)
        breakIndex = 0;

      // We found a place to break that is before the estimated break
      // offset. Where we break depends on whether the text crosses a
      // segment boundary
      if (start < data->mBreaks[breakIndex]) {
        // The text crosses at least one segment boundary so measure to the
        // break point just before the estimated break offset
        numChars = PR_MIN(data->mBreaks[breakIndex] - start, (PRInt32)aSubstringLength);
      } 
      else {
        // See whether there is another segment boundary between this one
        // and the end of the text
        if ((breakIndex < (data->mNumBreaks - 1)) && (data->mBreaks[breakIndex] < i)) {
          ++breakIndex;
          numChars = PR_MIN(data->mBreaks[breakIndex] - start, (PRInt32)aSubstringLength);
        }
        else {
          NS_ASSERTION(i != data->mBreaks[breakIndex], "don't expect to be at segment boundary");

          // The text is all within the same segment
          numChars = i - start;

          // Remember we're in the middle of a segment and not between
          // two segments
          inMiddleOfSegment = PR_TRUE;
        }
      }
    }

    // Measure the text
    nscoord twWidth, pxWidth;
    if ((1 == numChars) && (pstr[start] == ' ')) {
      twWidth = data->mSpaceWidth;
    }
    else {
      pxWidth = fontGTK->GetWidth(&pstr[start], numChars);
      twWidth = NSToCoordRound(float(pxWidth) * data->mP2T);
    }

    // See if the text fits
    PRBool textFits = (twWidth + width) <= data->mAvailWidth;

    // If the text fits then update the width and the number of
    // characters that fit
    if (textFits) {
      numCharsFit += numChars;
      width += twWidth;

      // If we computed the break index and we're not in the middle
      // of a segment then this is a spot that we can back up to if
      // we need to, so remember this state
      if ((breakIndex != -1) && !inMiddleOfSegment) {
        data->mPrevBreakState_BreakIndex = breakIndex;
        data->mPrevBreakState_Width = width;
      }
    }
    else {
      // The text didn't fit. If we're out of room then we're all done
      allDone = PR_TRUE;

      // See if we can just back up to the previous saved state and not
      // have to measure any text
      if (data->mPrevBreakState_BreakIndex != -1) {
        PRBool canBackup;

        // If we're in the middle of a word then the break index
        // must be the same if we can use it. If we're at a segment
        // boundary, then if the saved state is for the previous
        // break index then we can use it
        if (inMiddleOfSegment) {
          canBackup = data->mPrevBreakState_BreakIndex == breakIndex;
        } else {
          canBackup = data->mPrevBreakState_BreakIndex == (breakIndex - 1);
        }

        if (canBackup) {
          numCharsFit = data->mBreaks[data->mPrevBreakState_BreakIndex];
          width = data->mPrevBreakState_Width;
          break;
        }
      }

      // We can't just revert to the previous break state. Find the break
      // index just before the end of the text
      i = start + numChars;
      if (breakIndex == -1) {
        breakIndex = 0;
        if (data->mBreaks[breakIndex] < i) {
          while ((breakIndex + 1 < data->mNumBreaks) && (data->mBreaks[breakIndex + 1] < i)) {
            ++breakIndex;
          }
        }
      }

      if ((0 == breakIndex) && (i <= data->mBreaks[0])) {
        // There's no place to back up to, so even though the text doesn't fit
        // return it anyway
        numCharsFit += numChars;
        width += twWidth;

        // Edge case of one word: it could be that we just measured a fragment of the
        // first word and its remainder involves other fonts, so we want to keep going
        // until we at least measure the entire first word
        if (numCharsFit < data->mBreaks[0]) {
          allDone = PR_FALSE;         
          // From now on we don't care anymore what is the _real_ estimated
          // number of characters that fits. Rather, we have no where to break
          // and have to measure one word fully, but the real estimate is less
          // than that one word. However, since the other bits of code rely on
          // what is in "data->mEstimatedNumChars", we want to override
          // "data->mEstimatedNumChars" and pass in what _has_ to be measured
          // so that it is transparent to the other bits that depend on it.
          data->mEstimatedNumChars = data->mBreaks[0] - numCharsFit;
          start += numChars;
        }

        break;
      }

      // Repeatedly back up until we get to where the text fits or we're
      // all the way back to the first word
      width += twWidth;
      while ((breakIndex >= 0) && (width > data->mAvailWidth)) {
        twWidth = 0;
        start = data->mBreaks[breakIndex];
        numChars = i - start;
        if ((1 == numChars) && (pstr[start] == ' ')) {
          twWidth = data->mSpaceWidth;
        }
        else if (numChars > 0) {
          pxWidth = fontGTK->GetWidth(&pstr[start], numChars);
          twWidth = NSToCoordRound(float(pxWidth) * data->mP2T);
        }

        width -= twWidth;
        numCharsFit = start;
        --breakIndex;
        i = start;
      }
    }

    start += numChars;
  }

#ifdef DEBUG_rbs
  NS_ASSERTION(allDone || start == i, "internal error");
  NS_ASSERTION(allDone || data->mNumCharsFit != numCharsFit, "internal error");
#endif /* DEBUG_rbs */

  if (data->mNumCharsFit != numCharsFit) {
    // some text was actually retained
    data->mWidth = width;
    data->mNumCharsFit = numCharsFit;
    data->mFonts->AppendElement(fontGTK);
    data->mOffsets->AppendElement((void*)&pstr[numCharsFit]);
  }

  if (allDone) {
    // stop now
    return PR_FALSE;
  }

  return PR_TRUE; // don't stop if we still need to measure more characters
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
  if (!mFontMetrics)
    return NS_ERROR_FAILURE;

  nsFontMetricsGTK* metrics = (nsFontMetricsGTK*)mFontMetrics;

  nscoord spaceWidth, aveCharWidth;
  metrics->GetSpaceWidth(spaceWidth);
  metrics->GetAveCharWidth(aveCharWidth);

  // Note: aBreaks[] is supplied to us so that the first word is located
  // at aString[0 .. aBreaks[0]-1] and more generally, the k-th word is
  // located at aString[aBreaks[k-1] .. aBreaks[k]-1]. Whitespace can
  // be included and each of them counts as a word in its own right.

  // Upon completion of glyph resolution, characters that can be
  // represented with fonts[i] are at offsets[i] .. offsets[i+1]-1

  nsAutoVoidArray fonts, offsets;
  offsets.AppendElement((void*)aString);

  BreakGetTextDimensionsData data = { mP2T, aAvailWidth, aBreaks, aNumBreaks,
    spaceWidth, aveCharWidth, 0, 0, 0, -1, 0, &fonts, &offsets 
  };

  metrics->ResolveForwards(aString, aLength, do_BreakGetTextDimensions, &data);

  if (aFontID) *aFontID = 0;

  aNumCharsFit = data.mNumCharsFit;
  aDimensions.width = data.mWidth;

  ///////////////////
  // Post-processing for the ascent and descent:
  //
  // The width of the last word is included in the final width, but its
  // ascent and descent are kept aside for the moment. The problem is that
  // line-breaking may occur _before_ the last word, and we don't want its
  // ascent and descent to interfere. We can re-measure the last word and
  // substract its width later. However, we need a special care for the ascent
  // and descent at the break-point. The idea is to keep the ascent and descent
  // of the last word separate, and let layout consider them later when it has
  // determined that line-breaking doesn't occur before the last word.
  //
  // Therefore, there are two things to do:
  // 1. Determine the ascent and descent up to where line-breaking may occur.
  // 2. Determine the ascent and descent of the remainder.
  //    For efficiency however, it is okay to bail out early if there is only
  //    one font (in this case, the height of the last word has no special
  //    effect on the total height).

  // aLastWordDimensions.width should be set to -1 to reply that we don't
  // know the width of the last word since we measure multiple words
  aLastWordDimensions.Clear();
  aLastWordDimensions.width = -1;

  PRInt32 count = fonts.Count();
  if (!count)
    return NS_OK;
  nsFontGTK* fontGTK = (nsFontGTK*)fonts[0];
  NS_ASSERTION(fontGTK, "internal error in do_BreakGetTextDimensions");
  aDimensions.ascent = fontGTK->mMaxAscent;
  aDimensions.descent = fontGTK->mMaxDescent;

  // fast path - normal case, quick return if there is only one font
  if (count == 1)
    return NS_OK;

  // get the last break index.
  // If there is only one word, we end up with lastBreakIndex = 0. We don't
  // need to worry about aLastWordDimensions in this case too. But if we didn't
  // return earlier, it would mean that the unique word needs several fonts
  // and we will still have to loop over the fonts to return the final height
  PRInt32 lastBreakIndex = 0;
  while (aBreaks[lastBreakIndex] < aNumCharsFit)
    ++lastBreakIndex;

  const PRUnichar* lastWord = (lastBreakIndex > 0) 
    ? aString + aBreaks[lastBreakIndex-1]
    : aString + aNumCharsFit; // let it point outside to play nice with the loop

  // now get the desired ascent and descent information... this is however
  // a very fast loop of the order of the number of additional fonts

  PRInt32 currFont = 0;
  const PRUnichar* pstr = aString;
  const PRUnichar* last = aString + aNumCharsFit;

  while (pstr < last) {
    fontGTK = (nsFontGTK*)fonts[currFont];
    PRUnichar* nextOffset = (PRUnichar*)offsets[++currFont]; 

    // For consistent word-wrapping, we are going to handle the whitespace
    // character with special care because a whitespace character can come
    // from a font different from that of the previous word. If 'x', 'y', 'z',
    // are Unicode points that require different fonts, we want 'xyz <br>'
    // and 'xyz<br>' to have the same height because it gives a more stable
    // rendering, especially when the window is resized at the edge of the word.
    // If we don't do this, a 'tall' trailing whitespace, i.e., if the whitespace
    // happens to come from a font with a bigger ascent and/or descent than all
    // current fonts on the line, this can cause the next lines to be shifted
    // down when the window is slowly resized to fit that whitespace.
    if (*pstr == ' ') {
      // skip pass the whitespace to ignore the height that it may contribute
      ++pstr;
      // get out if we reached the end
      if (pstr == last) {
        break;
      }
      // switch to the next font if we just passed the current font 
      if (pstr == nextOffset) {
        fontGTK = (nsFontGTK*)fonts[currFont];
        nextOffset = (PRUnichar*)offsets[++currFont];
      } 
    }

    // see if the last word intersects with the current font
    // (we are testing for 'nextOffset-1 >= lastWord' since the
    // current font ends at nextOffset-1)
    if (nextOffset > lastWord) {
      if (aLastWordDimensions.ascent < fontGTK->mMaxAscent) {
        aLastWordDimensions.ascent = fontGTK->mMaxAscent;
      }
      if (aLastWordDimensions.descent < fontGTK->mMaxDescent) {
        aLastWordDimensions.descent = fontGTK->mMaxDescent;
      }
    }

    // see if we have not reached the last word yet
    if (pstr < lastWord) {
      if (aDimensions.ascent < fontGTK->mMaxAscent) {
        aDimensions.ascent = fontGTK->mMaxAscent;
      }
      if (aDimensions.descent < fontGTK->mMaxDescent) {
        aDimensions.descent = fontGTK->mMaxDescent;
      }
    }

    // advance to where the next font starts
    pstr = nextOffset;
  }

  return NS_OK;
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
nsRenderingContextGTK::GetTextDimensions(const PRUnichar* aString, PRUint32 aLength,
                                         nsTextDimensions& aDimensions, PRInt32* aFontID)
{
  aDimensions.Clear();
  if (0 < aLength) {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);

    nsFontMetricsGTK* metrics = (nsFontMetricsGTK*) mFontMetrics;

    g_return_val_if_fail(metrics != NULL, NS_ERROR_FAILURE);

    nsFontGTK* prevFont = nsnull;
    gint rawWidth = 0, rawAscent = 0, rawDescent = 0;
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
          if (rawAscent < prevFont->mMaxAscent)
            rawAscent = prevFont->mMaxAscent;
          if (rawDescent < prevFont->mMaxDescent)
            rawDescent = prevFont->mMaxDescent;
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
      if (rawAscent < prevFont->mMaxAscent)
        rawAscent = prevFont->mMaxAscent;
      if (rawDescent < prevFont->mMaxDescent)
        rawDescent = prevFont->mMaxDescent;
    }

    aDimensions.width = NSToCoordRound(rawWidth * mP2T);
    aDimensions.ascent = NSToCoordRound(rawAscent * mP2T);
    aDimensions.descent = NSToCoordRound(rawDescent * mP2T);
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
  nsresult res = NS_OK;

  if (0 != aLength) {
    g_return_val_if_fail(mTranMatrix != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mSurface != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mCurrentFont != NULL, NS_ERROR_FAILURE);

    nscoord x = aX;
    nscoord y = aY;

    UpdateGC();

    nsXFont *xFont = mCurrentFont->GetXFont();
    if (nsnull != aSpacing) {
      // Render the string, one character at a time...
      const char* end = aString + aLength;
      while (aString < end) {
        char ch = *aString++;
        nscoord xx = x;
        nscoord yy = y;
        mTranMatrix->TransformCoord(&xx, &yy);
        if (mCurrentFont->IsFreeTypeFont()) {
          PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
          // need to fix this for long strings
          PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
          // convert 7 bit data to unicode
          // this function is only supposed to be called for ascii data
          for (PRUint32 i=0; i<len; i++) {
            unichars[i] = (PRUnichar)((unsigned char)aString[i]);
          }
          res = mCurrentFont->DrawString(this, mSurface, xx, yy,
                                         unichars, len);
        }
        else if (!mCurrentFont->GetXFontIs10646()) {
          // 8 bit data with an 8 bit font
          NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
          xFont->DrawText8(mSurface->GetDrawable(), mGC, xx, yy, &ch, 1);
        }
        else {
          // we have 8 bit data but a 16 bit font
          NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
          Widen8To16AndDraw(mSurface->GetDrawable(), xFont, mGC,
                                                xx, yy, &ch, 1);
        }
        x += *aSpacing++;
      }
    }
    else {
      mTranMatrix->TransformCoord(&x, &y);
      if (mCurrentFont->IsFreeTypeFont()) {
        PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
        // need to fix this for long strings
        PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
        // convert 7 bit data to unicode
        // this function is only supposed to be called for ascii data
        for (PRUint32 i=0; i<len; i++) {
          unichars[i] = (PRUnichar)((unsigned char)aString[i]);
        }
        res = mCurrentFont->DrawString(this, mSurface, x, y,
                                       unichars, len);
      }
      else if (!mCurrentFont->GetXFontIs10646()) { // keep 8 bit path fast
        // 8 bit data with an 8 bit font
        NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
        xFont->DrawText8(mSurface->GetDrawable(), mGC, x, y, aString, aLength);
        }
      else {
        // we have 8 bit data but a 16 bit font
        NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
        Widen8To16AndDraw(mSurface->GetDrawable(), xFont, mGC,
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

  return res;
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
    nscoord y = aY;

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

void
Widen8To16AndGetTextExtents (nsXFont *xFont,  
                        const gchar *text,
                        gint         text_length,
                        gint        *lbearing,
                        gint        *rbearing,
                        gint        *width,
                        gint        *ascent,
                        gint        *descent)
{
  NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
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
  xFont->TextExtents16(p, uchar_size/2,
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
  nsresult res = NS_OK;
  aBoundingMetrics.Clear();
  if (aString && 0 < aLength) {
    g_return_val_if_fail(aString != NULL, NS_ERROR_FAILURE);
    g_return_val_if_fail(mCurrentFont != NULL, NS_ERROR_FAILURE);
    nsXFont *xFont = mCurrentFont->GetXFont();
    if (mCurrentFont->IsFreeTypeFont()) {
      PRUnichar unichars[WIDEN_8_TO_16_BUF_SIZE];
      // need to fix this for long strings
      PRUint32 len = PR_MIN(aLength, WIDEN_8_TO_16_BUF_SIZE);
      // convert 7 bit data to unicode
      // this function is only supposed to be called for ascii data
      for (PRUint32 i=0; i<len; i++) {
        unichars[i] = (PRUnichar)((unsigned char)aString[i]);
      }
      res = mCurrentFont->GetBoundingMetrics(unichars, len,
                                            aBoundingMetrics);
    }
    else if (!mCurrentFont->GetXFontIs10646()) {
        // 8 bit data with an 8 bit font
        NS_ASSERTION(xFont->IsSingleByte(),"wrong string/font size");
        xFont->TextExtents8(aString, aLength,
                                &aBoundingMetrics.leftBearing, 
                                &aBoundingMetrics.rightBearing, 
                                &aBoundingMetrics.width, 
                                &aBoundingMetrics.ascent, 
                                &aBoundingMetrics.descent);
    }
    else {
        // we have 8 bit data but a 16 bit font
        NS_ASSERTION(!xFont->IsSingleByte(),"wrong string/font size");
        Widen8To16AndGetTextExtents (mCurrentFont->GetXFont(), aString, aLength,
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

  return res;

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
