/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s):
 * Henry Sobotka <sobotka@axess.com> Jan. 2000 review and update
 * Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 03/23/2000       IBM Corp.      Fixed InvertRect.
 * 05/08/2000       IBM Corp.      Fix for trying to us an already freed mGammaTable.
 * 05/31/2000       IBM Corp.      Fix background color on PS printers.
 *
 */

//#define PROFILE_GSTATE // be noisy about graphicsstate-usage
#define GSTATE_CACHESIZE 50

// ToDo: Unicode text draw-er
#include "nsGfxDefs.h"

#include "nsRenderingContextOS2.h"
#include "nsIComponentManager.h"
#include "nsDeviceContextOS2.h"
#include "nsFontMetricsOS2.h"
#include "nsIFontMetrics.h"
#include "nsTransform2D.h"
#include "nsRegionOS2.h"
#include "nsGfxCIID.h"
#include "nsString.h"
#include "nsFont.h"
#include "libimg.h"
#include "prprf.h"
#include "nsIRenderingContextOS2.h"


// helper clip region functions - defined at the bottom of this file.
LONG OS2_CombineClipRegion( HPS hps, HRGN hrgnCombine, LONG lMode);
HRGN OS2_CopyClipRegion( HPS hps);
#define OS2_SetClipRegion(hps,hrgn) OS2_CombineClipRegion(hps, hrgn, CRGN_COPY)

// Use these instead of native GpiSave/RestorePS because: need to store ----
// more information, and need to be able to push from onscreen & pop onto
// offscreen.  Potentially.
struct GraphicsState
{
   GraphicsState() { Construct(); }
  ~GraphicsState() { Destruct(); }

   void Construct();
   void Destruct();

   nsTransform2D   mMatrix;
   HRGN            mClipRegion;
   nscolor         mColor;
   nsIFontMetrics *mFontMetrics;
   nsLineStyle     mLineStyle;

   GraphicsState  *mNext;
};

void GraphicsState::Construct()
{
   mClipRegion = 0;
   mColor = NS_RGB( 0, 0, 0);
   mFontMetrics = nsnull;
   mLineStyle = nsLineStyle_kSolid;
   mNext = nsnull;
}

void GraphicsState::Destruct()
{
   if( mClipRegion)
   {
      printf( "oops, leaked a region from rc-gs\n");
      mClipRegion = 0;
   }
   NS_IF_RELEASE( mFontMetrics);
}

class GraphicsStateCache
{
   GraphicsState *mFirst;
   UINT           mSize;
#ifdef PROFILE_GSTATE
   UINT           mDeleted;
   UINT           mCount;
   UINT           mPeak;
#endif

 public:
   GraphicsStateCache() : mFirst(nsnull), mSize(0)
#ifdef PROFILE_GSTATE
                         , mDeleted(0), mCount(0), mPeak(0)
#endif
   {}

  ~GraphicsStateCache()
   {
#ifdef PROFILE_GSTATE
      printf( "---- Graphics-State Stats -----\n");
      printf( "  GStates requested:       %d\n", mCount);
      printf( "  Actual GStates created:  %d\n", mPeak);
      double d = mCount ? (double)(mCount - mPeak) / (double)mCount : 0;
      printf( "  Gstates recycled:        %d (%d%%)\n", mCount - mPeak, (int)(d*100.0));
      UINT i = mSize+mDeleted;
      printf( "  Cached+Deleted:          %d\n", i);
      if( i != mPeak)
         printf( "  WARNING: GStates leaked: %d\n", mPeak - i);
      printf( "------------------------------\n\n");
#endif

      // Clear up the cache
      GraphicsState *pTemp, *pNext = mFirst;
      while( pNext)
      {
         pTemp = pNext->mNext;
         delete pNext;
         pNext = pTemp;
      }
   }

   GraphicsState *NewState()
   {
      GraphicsState *state = nsnull;
      if( mFirst)
      {
         state = mFirst;
         mFirst = mFirst->mNext;
         state->Construct();
         mSize--;
      }
      else
      {
         state = new GraphicsState;
#ifdef PROFILE_GSTATE
         mPeak++;
#endif
      }
#ifdef PROFILE_GSTATE
      mCount++;
#endif
      return state;
   }

   void DisposeState( GraphicsState *aState)
   {
      if( GSTATE_CACHESIZE == mSize)
      {
         delete aState;
#ifdef PROFILE_GSTATE
         mDeleted++;
#endif
      }
      else
      {
         aState->Destruct();
         aState->mNext = mFirst;
         mFirst = aState;
         mSize++;
      }
   }

} GStateCache; // XXX make this less static, I suppose

// Rendering context -------------------------------------------------------

NS_IMPL_ADDREF(nsRenderingContextOS2)
NS_IMPL_RELEASE(nsRenderingContextOS2)

nsresult
nsRenderingContextOS2::QueryInterface( REFNSIID aIID, void **aInstancePtr)
{
   if( !aInstancePtr)
      return NS_ERROR_NULL_POINTER;

   if( aIID.Equals( nsIRenderingContext::GetIID()))
      *aInstancePtr = (void *) (nsIRenderingContext*) this;
   else if( aIID.Equals( ((nsIRenderingContext*)this)->GetIID()))
      *aInstancePtr = (void *) (nsIRenderingContext*)this;
   else if( aIID.Equals( nsIRenderingContextOS2::GetIID()))
      *aInstancePtr = (void *) (nsIRenderingContextOS2*) this;
   else if( aIID.Equals( ((nsIRenderingContextOS2 *)this)->GetIID()))
      *aInstancePtr = (void *) (nsIRenderingContextOS2*) this;

   if( !*aInstancePtr)
      return NS_NOINTERFACE;

   NS_ADDREF_THIS();

   return NS_OK;
}

// Init-term stuff ---------------------------------------------------------

nsRenderingContextOS2::nsRenderingContextOS2()
{
   NS_INIT_REFCNT();

   mContext = nsnull;
   mSurface = nsnull;
   mPS = 0;
   mMainSurface = nsnull;
   mColor = NS_RGB( 0, 0, 0);
   mP2T = 1.0f;
   mStateStack = nsnull;
   mFontMetrics = nsnull;
   mLineStyle = nsLineStyle_kSolid;
   mPreservedInitialClipRegion = PR_FALSE;
   mPaletteMode = PR_FALSE;

   // Need to enforce setting color values for first time. Make cached values different from current ones.
   mCurrFontMetrics = mFontMetrics + 1;
   mCurrTextColor   = mColor + 1;
   mCurrLineColor   = mColor + 1;
   mCurrLineStyle   = (nsLineStyle)((int)mLineStyle + 1);
   mCurrFillColor   = mColor + 1;
}

nsRenderingContextOS2::~nsRenderingContextOS2()
{
   NS_IF_RELEASE(mContext);

   //destroy the initial GraphicsState
   if (mPreservedInitialClipRegion == PR_TRUE)
   {
      PRBool clipState;
      PopState (clipState);
   }

   // clear state stack
   GraphicsState *pTemp, *pNext = mStateStack;
   while( pNext)
   {
      if( pNext->mClipRegion)
      {
         GFX (::GpiDestroyRegion (mPS, pNext->mClipRegion), FALSE);
         pNext->mClipRegion = 0;
      }
      pTemp = pNext->mNext;
      GStateCache.DisposeState( pNext);
      pNext = pTemp;
   }

   // Release surfaces and the palette
   NS_IF_RELEASE(mMainSurface);
   NS_IF_RELEASE(mSurface);
   NS_IF_RELEASE(mFontMetrics);
}

nsresult nsRenderingContextOS2::SetupPS (void)
{
   // If this is a palette device, then select and realize the palette
   nsPaletteInfo palInfo;
   mContext->GetPaletteInfo(palInfo);

   LONG BlackColor, WhiteColor;

   if (palInfo.isPaletteDevice && palInfo.palette)
   {
      ULONG cclr;
 
      // Select the palette in the background
      GFX (::GpiSelectPalette (mPS, (HPAL)palInfo.palette), PAL_ERROR);

      if (mDCOwner)
         GFX (::WinRealizePalette((HWND)mDCOwner->GetNativeData(NS_NATIVE_WINDOW), mPS, &cclr), PAL_ERROR);

      BlackColor = GFX (::GpiQueryColorIndex (mPS, 0, MK_RGB (0x00, 0x00, 0x00)), GPI_ALTERROR);    // CLR_BLACK;
      WhiteColor = GFX (::GpiQueryColorIndex (mPS, 0, MK_RGB (0xFF, 0xFF, 0xFF)), GPI_ALTERROR);    // CLR_WHITE;

      mPaletteMode = PR_TRUE;
   }
   else
   {
      GFX (::GpiCreateLogColorTable (mPS, 0, LCOLF_RGB, 0, 0, 0), FALSE);

      BlackColor = MK_RGB (0x00, 0x00, 0x00);
      WhiteColor = MK_RGB (0xFF, 0xFF, 0xFF);

      mPaletteMode = PR_FALSE;
   }

   // Set image foreground and background colors. These are used in transparent images for blitting 1-bit masks.
   // To invert colors on ROP_SRCAND we map 1 to black and 0 to white
   IMAGEBUNDLE ib;
   ib.lColor     = BlackColor;           // map 1 in mask to 0x000000 (black) in destination
   ib.lBackColor = WhiteColor;           // map 0 in mask to 0xFFFFFF (white) in destination
   ib.usMixMode  = FM_OVERPAINT;
   ib.usBackMixMode = BM_OVERPAINT;
   GFX (::GpiSetAttrs (mPS, PRIM_IMAGE, IBB_COLOR | IBB_BACK_COLOR | IBB_MIX_MODE | IBB_BACK_MIX_MODE, 0, (PBUNDLE)&ib), FALSE);

   // Need to enforce setting color values for first time. Make cached values different from current ones.
   mCurrFontMetrics = mFontMetrics + 1;
   mCurrTextColor   = mColor + 1;
   mCurrLineColor   = mColor + 1;
   mCurrLineStyle   = (nsLineStyle)((int)mLineStyle + 1);
   mCurrFillColor   = mColor + 1;

   return NS_OK;
}

nsresult nsRenderingContextOS2::Init( nsIDeviceContext *aContext,
                                      nsIWidget *aWindow)
{
   mContext = aContext;
   NS_ADDREF(mContext);

   // Create & remember an on-screen surface
   nsWindowSurface *surf = new nsWindowSurface;
   if (!surf)
     return NS_ERROR_OUT_OF_MEMORY;

   surf->Init(aWindow);

   mSurface = surf;
   mPS = mSurface->GetPS ();
   NS_ADDREF(mSurface);

   mDCOwner = aWindow;
   NS_IF_ADDREF(mDCOwner);

   // Grab another reference to the onscreen for later uniformity
   mMainSurface = mSurface;
   NS_ADDREF(mMainSurface);

   PushState();
   mPreservedInitialClipRegion = PR_TRUE;

   return CommonInit();
}

nsresult nsRenderingContextOS2::Init( nsIDeviceContext *aContext,
                                      nsDrawingSurface aSurface)
{
   mContext = aContext;
   NS_IF_ADDREF(mContext);

   // Add a couple of references to the onscreen (or print, more likely)
   mSurface = (nsDrawingSurfaceOS2 *) aSurface;
   mPS = mSurface->GetPS ();
   NS_ADDREF(mSurface);
   mMainSurface = mSurface;
   NS_ADDREF(mMainSurface);

   PushState();
   mPreservedInitialClipRegion = PR_TRUE;

   return CommonInit();
}

// Presentation space page units (& so world coords) are PU_PELS.
// We have a matrix, mTMatrix, which converts from the passed in app units
// to pels.  Note there is *no* guarantee that app units == twips.
nsresult nsRenderingContextOS2::CommonInit()
{
   mContext->GetGammaTable(mGammaTable);
   float app2dev = 0;
   mContext->GetAppUnitsToDevUnits( app2dev);
   mTMatrix.AddScale( app2dev, app2dev);
   mContext->GetDevUnitsToAppUnits( mP2T);

   return SetupPS ();
}

// PS & drawing surface management -----------------------------------------

nsresult nsRenderingContextOS2::SelectOffScreenDrawingSurface( nsDrawingSurface aSurface)
{
   if (aSurface != mSurface)
   {
      if( aSurface)
      {
         NS_IF_RELEASE(mSurface);
         mSurface = (nsDrawingSurfaceOS2 *) aSurface;
         mPS = mSurface->GetPS ();

         SetupPS ();
      }
      else // deselect current offscreen...
      {
         NS_IF_RELEASE(mSurface);
         mSurface = mMainSurface;
         mPS = mSurface->GetPS ();

         SetupPS ();
      }

      NS_ADDREF(mSurface);
   }

   return NS_OK;
}

// !! This is a bit dodgy; nsDrawingSurface *really* needs to be XP-comified
// !! properly...
nsresult nsRenderingContextOS2::GetDrawingSurface( nsDrawingSurface *aSurface)
{
   if( !aSurface)
      return NS_ERROR_NULL_POINTER;

#if 0
   NS_IF_RELEASE( *aSurface);
#endif

   *aSurface = (void*)((nsDrawingSurfaceOS2 *) mSurface);
   // !! probably don't want to addref this 'cos client can't release it...

   return NS_OK;
}

// The idea here is to create an offscreen surface for blitting with.
// I can't find any way for people to resize the bitmap created here,
// so I guess this gets called quite often.
//
// I'm reliably told that 'aBounds' is in device units, and that the
// position oughtn't to be ignored, but for all intents & purposes can be.
//
nsresult nsRenderingContextOS2::CreateDrawingSurface( nsRect *aBounds,
                             PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
   nsresult rc = NS_ERROR_FAILURE;

   nsOffscreenSurface *surf = new nsOffscreenSurface;

   if (!surf)
     return NS_ERROR_OUT_OF_MEMORY;

   rc = surf->Init( mMainSurface->GetPS (), aBounds->width, aBounds->height, aSurfFlags);

   if(NS_SUCCEEDED(rc))
   {
      NS_ADDREF(surf);
      aSurface = (void*)((nsDrawingSurfaceOS2 *) surf);
   }
   else
      delete surf;

   return rc;
}

NS_IMETHODIMP nsRenderingContextOS2::CreateDrawingSurface(HPS aPS, nsDrawingSurface &aSurface, nsIWidget *aWidget)
{
  nsWindowSurface *surf = new nsWindowSurface();

  if (nsnull != surf)
  {
    NS_ADDREF(surf);
    surf->Init(aPS, aWidget);
  }

  aSurface = (nsDrawingSurface)surf;

  return NS_OK;
}

nsresult nsRenderingContextOS2::DestroyDrawingSurface( nsDrawingSurface aDS)
{
   nsDrawingSurfaceOS2 *surf = (nsDrawingSurfaceOS2 *) aDS;
   nsresult rc = NS_ERROR_NULL_POINTER;

   // If the surface being destroyed is the one we're currently using for
   // offscreen, then lose our reference to it & hook back to the onscreen.
   if( surf && surf == mSurface)
   {
      NS_RELEASE(mSurface);    // ref. from SelectOffscreen
      mSurface = mMainSurface;
      mPS = mSurface->GetPS ();
      NS_ADDREF(mSurface);
   }

   if( surf)
   {
      NS_RELEASE(surf);        // ref. from CreateSurface
      rc = NS_OK;
   }

   return rc;
}


NS_IMETHODIMP nsRenderingContextOS2::LockDrawingSurface( PRInt32 aX, PRInt32 aY,
                                       PRUint32 aWidth, PRUint32 aHeight,
                                       void **aBits,
                                       PRInt32 *aStride, PRInt32 *aWidthBytes,
                                       PRUint32 aFlags)
{
  PushState();

  return mSurface->Lock( aX, aY, aWidth, aHeight, aBits,
                         aStride, aWidthBytes, aFlags);
}

NS_IMETHODIMP nsRenderingContextOS2::UnlockDrawingSurface()
{
  mSurface->Unlock();

  PRBool clipstate;
  PopState (clipstate);

  return NS_OK;
}

// State stack -------------------------------------------------------------
nsresult nsRenderingContextOS2::PushState()
{
   GraphicsState *state = GStateCache.NewState();

   // copy matrix into state
   state->mMatrix.SetMatrix( &mTMatrix);
   // copy color & font
   state->mColor = mColor;
   state->mFontMetrics = mFontMetrics;
   state->mLineStyle = mLineStyle;

   // add a new ref to the fontmetrics
   NS_IF_ADDREF( mFontMetrics);

   // clip region: get current & copy it.
   state->mClipRegion = OS2_CopyClipRegion( mPS);

   // push state onto stack
   state->mNext = mStateStack;
   mStateStack = state;
   return NS_OK;
}

// RC here is true if clip region is now empty.
nsresult nsRenderingContextOS2::PopState( PRBool &aClipEmpty)
{
   PRBool rc = PR_TRUE;

   NS_ASSERTION( mStateStack, "state underflow");
   if( !mStateStack) return NS_ERROR_FAILURE;

   GraphicsState *state = mStateStack;
   mStateStack = state->mNext;

   // update xform matrix
   mTMatrix.SetMatrix( &state->mMatrix);

   // color & font
   SetColor( state->mColor);
   SetLineStyle( state->mLineStyle);
   NS_IF_RELEASE(mFontMetrics);
   mFontMetrics = state->mFontMetrics;
   state->mFontMetrics = nsnull;

   // Clip region
   OS2_SetClipRegion( mPS, state->mClipRegion);

   if( state->mClipRegion != 0)
   {
      state->mClipRegion = 0;
      rc = PR_FALSE;
   }

   GStateCache.DisposeState( state);

   aClipEmpty = rc;

   return NS_OK;
}

nsresult nsRenderingContextOS2::Reset()
{
   // okay, what's this supposed to do?  Empty the state stack?
   return NS_OK;
}

// Clip region management --------------------------------------------------

nsresult nsRenderingContextOS2::IsVisibleRect( const nsRect &aRect,
                                               PRBool &aIsVisible)
{
   nsRect trect( aRect);
   mTMatrix.TransformCoord( &trect.x, &trect.y,
                            &trect.width, &trect.height);
   RECTL rcl;
   mSurface->NS2PM_ININ (trect, rcl);

   LONG rc = GFX (::GpiRectVisible( mPS, &rcl), RVIS_ERROR);

   aIsVisible = (rc == RVIS_PARTIAL || rc == RVIS_VISIBLE) ? PR_TRUE : PR_FALSE;


   return NS_OK;
}

// Return PR_TRUE if clip region is now empty
nsresult nsRenderingContextOS2::SetClipRect( const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
   nsRect trect = aRect;
   mTMatrix.TransformCoord( &trect.x, &trect.y, &trect.width, &trect.height);
   long lrc = RGN_ERROR;

   if( trect.width == 0 || trect.height == 0)
   {
      // Mozilla assumes that clip region with zero width or height does not produce any output - everything is clipped.
      // GPI does not support clip regions with zero width or height. We cheat by creating 1x1 clip region far outside
      // of real drawing region limits.

      if( aCombine == nsClipCombine_kIntersect || aCombine == nsClipCombine_kReplace)
      {
         RECTL rcl;
         rcl.xLeft   = -1000;
         rcl.xRight  = -999;
         rcl.yBottom = -1000;
         rcl.yTop    = -999;

         HRGN hrgn = GFX (::GpiCreateRegion( mPS, 1, &rcl), RGN_ERROR);
         OS2_SetClipRegion (mPS, hrgn);

         lrc = PR_TRUE;      // Should pretend that clipping region is empty
      } else
      {
         // Clipping region is already correct. Just need to obtain it's complexity
         POINTL Offset = { 0, 0 };

         lrc = GFX (::GpiOffsetClipRegion (mPS, &Offset), RGN_ERROR);
      }
   }
   else
   {
      RECTL rcl;

      switch( aCombine)
      {
         case nsClipCombine_kIntersect:
         case nsClipCombine_kSubtract:
            mSurface->NS2PM_ININ (trect, rcl);
            if( aCombine == nsClipCombine_kIntersect)
               lrc = GFX (::GpiIntersectClipRectangle( mPS, &rcl), RGN_ERROR);
            else
               lrc = GFX (::GpiExcludeClipRectangle( mPS, &rcl), RGN_ERROR);
            break;

         case nsClipCombine_kUnion:
         case nsClipCombine_kReplace:
         {
            // need to create a new region & fiddle with it
            mSurface->NS2PM_INEX (trect, rcl);
            HRGN hrgn = GFX (::GpiCreateRegion( mPS, 1, &rcl), RGN_ERROR);
            if( hrgn && aCombine == nsClipCombine_kReplace)
               lrc = OS2_SetClipRegion( mPS, hrgn);
            else if( hrgn)
               lrc = OS2_CombineClipRegion( mPS, hrgn, CRGN_OR);
            break;
         }
         default:
            // compiler informational...
            NS_ASSERTION( 0, "illegal clip combination");
            break;
      }
   }

   aClipEmpty = (lrc == RGN_NULL) ? PR_TRUE : PR_FALSE;

   return NS_OK;
}

// rc is whether there is a cliprect to return
// !! Potential problems here: I think we ought to return the rect
// !! transformed by the inverse of our xformation matrix.  Which,
// !! frankly, I can't be bothered to write the code for - it ought
// !! to be a method in nsTransform2D.
nsresult nsRenderingContextOS2::GetClipRect( nsRect &aRect, PRBool &aHasLocalClip)
{
   RECTL rcl;
   long rc = GFX (::GpiQueryClipBox (mPS, &rcl), RGN_ERROR);

   PRBool brc = PR_FALSE;

   if( rc != RGN_NULL && rc != RGN_ERROR)
   {
      mSurface->PM2NS_ININ (rcl, aRect);
      brc = PR_TRUE;
   }

   aHasLocalClip = brc;

   return NS_OK;
}

// Return PR_TRUE if clip region is now empty
nsresult nsRenderingContextOS2::SetClipRegion( const nsIRegion &aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
   nsRegionOS2 *pRegion = (nsRegionOS2 *) &aRegion;
   PRUint32     ulHeight = mSurface->GetHeight ();

   HRGN hrgn = pRegion->GetHRGN( ulHeight, mPS);
   long cmode = 0;

   switch( aCombine)
   {
      case nsClipCombine_kIntersect:
         cmode = CRGN_AND;
         break;
      case nsClipCombine_kUnion:
         cmode = CRGN_OR;
         break;
      case nsClipCombine_kSubtract:
         cmode = CRGN_DIFF;
         break;
      case nsClipCombine_kReplace:
         cmode = CRGN_COPY;
         break;
      default:
         // Compiler informational...
         NS_ASSERTION( 0, "illegal clip combination");
         break;
   }

   long lrc = OS2_CombineClipRegion( mPS, hrgn, cmode);

   aClipEmpty = (lrc == RGN_NULL) ? PR_TRUE : PR_FALSE;

   return NS_OK;
}

// Somewhat dubious & rather expensive
nsresult nsRenderingContextOS2::GetClipRegion( nsIRegion **aRegion)
{
   if( !aRegion)
      return NS_ERROR_NULL_POINTER;

   *aRegion = 0;

   nsRegionOS2 *pRegion = new nsRegionOS2;
   if (!pRegion)
     return NS_ERROR_OUT_OF_MEMORY;
   NS_ADDREF(pRegion);

   // Get current clip region
   HRGN hrgnClip = 0;

   GFX (::GpiSetClipRegion (mPS, 0, &hrgnClip), RGN_ERROR);
   
   if( hrgnClip && hrgnClip != HRGN_ERROR)
   {
      // There was a clip region, so get it & init.
      HRGN hrgnDummy = 0;
      PRUint32 ulHeight = mSurface->GetHeight ();

      pRegion->Init( hrgnClip, ulHeight, mPS);
      GFX (::GpiSetClipRegion (mPS, hrgnClip, &hrgnDummy), RGN_ERROR);
   }
   else
      pRegion->Init();

   *aRegion = pRegion;

   return NS_OK;
}

/**
 * Fills in |aRegion| with a copy of the current clip region.
 */
nsresult nsRenderingContextOS2::CopyClipRegion(nsIRegion &aRegion)
{
  HRGN hr = OS2_CopyClipRegion(mPS);

  if (hr == HRGN_ERROR)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

// Setters & getters -------------------------------------------------------

nsresult nsRenderingContextOS2::GetDeviceContext( nsIDeviceContext *&aDeviceContext)
{
// NS_IF_RELEASE(aDeviceContext);
   aDeviceContext = mContext;
   NS_IF_ADDREF( mContext);
   return NS_OK;
}

nsresult nsRenderingContextOS2::SetColor( nscolor aColor)
{
   mColor = aColor;
   return NS_OK;
}

nsresult nsRenderingContextOS2::GetColor( nscolor &aColor) const
{
   aColor = mColor;
   return NS_OK;
}

nsresult nsRenderingContextOS2::SetLineStyle( nsLineStyle aLineStyle)
{
   mLineStyle = aLineStyle;
   return NS_OK;
}

nsresult nsRenderingContextOS2::GetLineStyle( nsLineStyle &aLineStyle)
{
   aLineStyle = mLineStyle;
   return NS_OK;
}

nsresult nsRenderingContextOS2::SetFont( const nsFont &aFont)
{
   NS_IF_RELEASE( mFontMetrics);
   mContext->GetMetricsFor( aFont, mFontMetrics);
   return NS_OK;
}

nsresult nsRenderingContextOS2::SetFont( nsIFontMetrics *aFontMetrics)
{
   NS_IF_RELEASE( mFontMetrics);
   mFontMetrics = aFontMetrics;
   NS_IF_ADDREF( mFontMetrics);
   return NS_OK;
}

nsresult nsRenderingContextOS2::GetFontMetrics( nsIFontMetrics*& aFontMetrics)
{
// Commented out because clients misuse the interface.
//
// NS_IF_RELEASE(aFontMetrics);
   aFontMetrics = mFontMetrics;
   NS_IF_ADDREF( mFontMetrics);
   return NS_OK;
}

// add the passed in translation to the current translation
nsresult nsRenderingContextOS2::Translate( nscoord aX, nscoord aY)
{
   mTMatrix.AddTranslation( (float) aX, (float) aY);
   return NS_OK;
}

// add the passed in scale to the current scale
nsresult nsRenderingContextOS2::Scale( float aSx, float aSy)
{
   mTMatrix.AddScale(aSx, aSy);
   return NS_OK;
}

nsresult nsRenderingContextOS2::GetCurrentTransform( nsTransform2D *&aTransform)
{
/* JSK0126 - Assign mTMatrix into aTransform, since aTransform is not a valid object yet */
   aTransform = &mTMatrix;

   /*
   NS_PRECONDITION(aTransform,"Null transform ptr");
   if( !aTransform)
      return NS_ERROR_NULL_POINTER;
   aTransform->SetMatrix( &mTMatrix);
   */
   return NS_OK;
}


// Drawing methods ---------------------------------------------------------

LONG nsRenderingContextOS2::GetGPIColor (void)
{
   LONG gcolor = MK_RGB (mGammaTable [NS_GET_R (mColor)],
                         mGammaTable [NS_GET_G (mColor)],
                         mGammaTable [NS_GET_B (mColor)]);

   return (mPaletteMode) ? GFX (::GpiQueryColorIndex (mPS, 0, gcolor), GPI_ALTERROR) :
                           gcolor ;
}

void nsRenderingContextOS2::SetupLineColorAndStyle (void)
{
   if (mColor != mCurrLineColor)
   {
      LINEBUNDLE lineBundle;
      lineBundle.lColor = GetGPIColor ();

      GFX (::GpiSetAttrs (mPS, PRIM_LINE, LBB_COLOR, 0, (PBUNDLE)&lineBundle), FALSE);

      mCurrLineColor = mColor;
   }

   if (mLineStyle != mCurrLineStyle)
   {
      long ltype = 0;
      switch( mLineStyle)
      {
         case nsLineStyle_kNone:   ltype = LINETYPE_INVISIBLE; break;
         case nsLineStyle_kSolid:  ltype = LINETYPE_SOLID; break;
         case nsLineStyle_kDashed: ltype = LINETYPE_SHORTDASH; break;
         case nsLineStyle_kDotted: ltype = LINETYPE_DOT; break;
         default:
            NS_ASSERTION(0, "Unexpected line style");
            break;
      }
      GFX (::GpiSetLineType (mPS, ltype), FALSE);
      mCurrLineStyle = mLineStyle;
   }
}

void nsRenderingContextOS2::SetupFillColor (void)
{
   if (mColor != mCurrFillColor)
   {
      AREABUNDLE areaBundle;
      areaBundle.lColor = GetGPIColor ();

      GFX (::GpiSetAttrs (mPS, PRIM_AREA, ABB_COLOR, 0, (PBUNDLE)&areaBundle), FALSE);

      mCurrFillColor = mColor;
   }
}

void nsRenderingContextOS2::SetupFontAndTextColor (void)
{
   if (mFontMetrics != mCurrFontMetrics)
   {
      // select font
      mCurrFontMetrics = mFontMetrics;
      if( mCurrFontMetrics)
         mSurface->SelectFont( mCurrFontMetrics);
   }

   if (mColor != mCurrTextColor)
   {
      CHARBUNDLE cBundle;
      cBundle.lColor = GetGPIColor ();
      cBundle.usMixMode = FM_OVERPAINT;
      cBundle.usBackMixMode = BM_LEAVEALONE;

      GFX (::GpiSetAttrs (mPS, PRIM_CHAR,
                          CBB_COLOR | CBB_MIX_MODE | CBB_BACK_MIX_MODE,
                          0, &cBundle),
           FALSE);

      mCurrTextColor = mColor;
   }
}

nsresult nsRenderingContextOS2::DrawLine( nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
   mTMatrix.TransformCoord( &aX0, &aY0);
   mTMatrix.TransformCoord( &aX1, &aY1);

   POINTL ptls[] = { { (long) aX0, (long) aY0 },
                     { (long) aX1, (long) aY1 } };
   mSurface->NS2PM (ptls, 2);

   if (ptls[0].x > ptls[1].x)
      ptls[0].x--;
   else if (ptls[1].x > ptls[0].x)
      ptls[1].x--;

   if (ptls[0].y < ptls[1].y)
      ptls[0].y++;
   else if (ptls[1].y < ptls[0].y)
      ptls[1].y++;

   SetupLineColorAndStyle ();

   GFX (::GpiMove (mPS, ptls), FALSE);
   GFX (::GpiLine (mPS, ptls + 1), GPI_ERROR);

   return NS_OK;
}

nsresult nsRenderingContextOS2::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
   PMDrawPoly( aPoints, aNumPoints, PR_FALSE);
   return NS_OK;
}

nsresult nsRenderingContextOS2::DrawPolygon( const nsPoint aPoints[], PRInt32 aNumPoints)
{
   PMDrawPoly( aPoints, aNumPoints, PR_FALSE);
   return NS_OK;
}

nsresult nsRenderingContextOS2::FillPolygon( const nsPoint aPoints[], PRInt32 aNumPoints)
{
   PMDrawPoly( aPoints, aNumPoints, PR_TRUE);
   return NS_OK;
}

void nsRenderingContextOS2::PMDrawPoly( const nsPoint aPoints[], PRInt32 aNumPoints, PRBool bFilled)
{
   if( aNumPoints > 1)
   {
      // Xform coords
      POINTL  aptls[ 20];
      PPOINTL pts = aptls;

      if( aNumPoints > 20)
         pts = new POINTL [ aNumPoints];

      PPOINTL pp = pts;
      const nsPoint *np = &aPoints[0];

      for( PRInt32 i = 0; i < aNumPoints; i++, pp++, np++)
      {
         pp->x = np->x;
         pp->y = np->y;
         mTMatrix.TransformCoord( (int*)&pp->x, (int*)&pp->y);
      }

      // go to os2
      mSurface->NS2PM (pts, aNumPoints);

      // We draw closed pgons using polyline to avoid filling it.  This works
      // because the API to this class specifies that the last point must
      // be the same as the first one...

      GFX (::GpiMove (mPS, pts), FALSE);

      if( bFilled == PR_TRUE)
      {
         POLYGON pgon = { aNumPoints - 1, pts + 1 };
         //IBM-AKR changed from boundary and inclusive to be noboundary and 
         //        exclusive to fix bug with text fields, buttons, etc. borders 
         //        being 1 pel too thick.  Bug 56853
         SetupFillColor ();
         GFX (::GpiPolygons (mPS, 1, &pgon, POLYGON_NOBOUNDARY, POLYGON_EXCL), GPI_ERROR);
      }
      else
      {
         SetupLineColorAndStyle ();
         GFX (::GpiPolyLine (mPS, aNumPoints - 1, pts + 1), GPI_ERROR);
      }

      if( aNumPoints > 20)
         delete [] pts;
   }
}

nsresult nsRenderingContextOS2::DrawRect( const nsRect& aRect)
{
   nsRect tr = aRect;
   PMDrawRect( tr, FALSE);
   return NS_OK;
}

nsresult nsRenderingContextOS2::DrawRect( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
   nsRect tr( aX, aY, aWidth, aHeight);
   PMDrawRect( tr, FALSE);
   return NS_OK;
}

nsresult nsRenderingContextOS2::FillRect( const nsRect& aRect)
{
   nsRect tr = aRect;
   PMDrawRect( tr, TRUE);
   return NS_OK;
}

nsresult nsRenderingContextOS2::FillRect( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
   nsRect tr( aX, aY, aWidth, aHeight);
   PMDrawRect( tr, TRUE);
   return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextOS2 :: InvertRect(const nsRect& aRect)
{
   return InvertRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP
nsRenderingContextOS2 :: InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
   nsRect tr(aX, aY, aWidth, aHeight);
   LONG CurMix = GFX (::GpiQueryMix (mPS), FM_ERROR);
   GFX (::GpiSetMix (mPS, FM_XOR), FALSE);
   PMDrawRect(tr, FALSE);
   GFX (::GpiSetMix (mPS, CurMix), FALSE);
   return NS_OK;
}

void nsRenderingContextOS2::PMDrawRect( nsRect &rect, BOOL fill)
{
   mTMatrix.TransformCoord( &rect.x, &rect.y, &rect.width, &rect.height);

   RECTL rcl;
   mSurface->NS2PM_ININ (rect, rcl);

   GFX (::GpiMove (mPS, (PPOINTL) &rcl), FALSE);

   if (rcl.xLeft == rcl.xRight || rcl.yTop == rcl.yBottom)
   {
      SetupLineColorAndStyle ();
      GFX (::GpiLine (mPS, ((PPOINTL)&rcl) + 1), GPI_ERROR);
   }
   else 
   {
      long lOps;

      if (fill)
      {
         lOps = DRO_FILL;
         SetupFillColor ();
      } else
      {
         lOps = DRO_OUTLINE;
         SetupLineColorAndStyle ();
      }

      GFX (::GpiBox (mPS, lOps, ((PPOINTL)&rcl) + 1, 0, 0), GPI_ERROR);
   }
}

// Arc-drawing methods, all proxy on to PMDrawArc
nsresult nsRenderingContextOS2::DrawEllipse( const nsRect& aRect)
{
   nsRect tRect( aRect);
   PMDrawArc( tRect, PR_FALSE, PR_TRUE);
   return NS_OK;
}

nsresult nsRenderingContextOS2::DrawEllipse( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
   nsRect tRect( aX, aY, aWidth, aHeight);
   PMDrawArc( tRect, PR_FALSE, PR_TRUE);
   return NS_OK;
}

nsresult nsRenderingContextOS2::FillEllipse( const nsRect& aRect)
{
   nsRect tRect( aRect);
   PMDrawArc( tRect, PR_TRUE, PR_TRUE);
   return NS_OK;
}

nsresult nsRenderingContextOS2::FillEllipse( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
   nsRect tRect( aX, aY, aWidth, aHeight);
   PMDrawArc( tRect, PR_TRUE, PR_TRUE);
   return NS_OK;
}

nsresult nsRenderingContextOS2::DrawArc( const nsRect& aRect,
                                         float aStartAngle, float aEndAngle)
{
   nsRect tRect( aRect);
   PMDrawArc( tRect, PR_FALSE, PR_FALSE, aStartAngle, aEndAngle);
   return NS_OK;
}

nsresult nsRenderingContextOS2::DrawArc( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                         float aStartAngle, float aEndAngle)
{
   nsRect tRect( aX, aY, aWidth, aHeight);
   PMDrawArc( tRect, PR_FALSE, PR_FALSE, aStartAngle, aEndAngle);
   return NS_OK;
}

nsresult nsRenderingContextOS2::FillArc( const nsRect& aRect,
                                         float aStartAngle, float aEndAngle)
{
   nsRect tRect( aRect);
   PMDrawArc( tRect, PR_TRUE, PR_FALSE, aStartAngle, aEndAngle);
   return NS_OK;
}

nsresult nsRenderingContextOS2::FillArc( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                         float aStartAngle, float aEndAngle)
{
   nsRect tRect( aX, aY, aWidth, aHeight);
   PMDrawArc( tRect, PR_TRUE, PR_FALSE, aStartAngle, aEndAngle);
   return NS_OK;
}

void nsRenderingContextOS2::PMDrawArc( nsRect &rect, PRBool bFilled, PRBool bFull,
                                       float start, float end)
{
   // convert coords
   mTMatrix.TransformCoord( &rect.x, &rect.y, &rect.width, &rect.height);

   RECTL rcl;
   mSurface->NS2PM_ININ (rect, rcl);

   // set arc params.
   long lWidth = rect.width / 2;
   long lHeight = rect.height / 2;
   ARCPARAMS arcparams = { lWidth, lHeight, 0, 0 };
   GFX (::GpiSetArcParams (mPS, &arcparams), FALSE);

   // move to center
   rcl.xLeft += lWidth;
   rcl.yBottom += lHeight;
   GFX (::GpiMove (mPS, (PPOINTL)&rcl), FALSE);

   if (bFilled)
      SetupFillColor ();
   else
      SetupLineColorAndStyle ();


   if (bFull)
   {
      long lOps = (bFilled) ? DRO_FILL : DRO_OUTLINE;

      // draw ellipse
      GFX (::GpiFullArc (mPS, lOps, MAKEFIXED(1,0)), GPI_ERROR);
   }
   else
   {
      FIXED StartAngle = (FIXED)(start * 65536.0) % MAKEFIXED (360, 0);
      FIXED EndAngle   = (FIXED)(end * 65536.0) % MAKEFIXED (360, 0);
      FIXED SweepAngle = EndAngle - StartAngle;

      if (SweepAngle < 0) SweepAngle += MAKEFIXED (360, 0);

      // draw an arc or a pie
      if (bFilled)
      {
         GFX (::GpiBeginArea (mPS, BA_NOBOUNDARY), FALSE);
         GFX (::GpiPartialArc (mPS, (PPOINTL)&rcl, MAKEFIXED(1,0), StartAngle, SweepAngle), GPI_ERROR);
         GFX (::GpiEndArea (mPS), GPI_ERROR);
      }
      else
      {
         // draw an invisible partialarc to get to the start of the arc.
         long lLineType = GFX (::GpiQueryLineType (mPS), LINETYPE_ERROR);
         GFX (::GpiSetLineType (mPS, LINETYPE_INVISIBLE), FALSE);
         GFX (::GpiPartialArc (mPS, (PPOINTL)&rcl, MAKEFIXED(1,0), StartAngle, MAKEFIXED (0,0)), GPI_ERROR);
         // now draw a real arc
         GFX (::GpiSetLineType (mPS, lLineType), FALSE);
         GFX (::GpiPartialArc (mPS, (PPOINTL)&rcl, MAKEFIXED(1,0), StartAngle, SweepAngle), GPI_ERROR);
      }
   }
}

NS_IMETHODIMP nsRenderingContextOS2::GetHints(PRUint32& aResult)
{
  PRUint32 result = 0;

  aResult = result;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2 :: GetWidth(char ch, nscoord& aWidth)
{
  char buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

NS_IMETHODIMP nsRenderingContextOS2 :: GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
{
  PRUnichar buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextOS2 :: GetWidth(const char* aString, nscoord& aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

NS_IMETHODIMP nsRenderingContextOS2 :: GetWidth(const char* aString,
                                                PRUint32 aLength,
                                                nscoord& aWidth)
{
  if (nsnull != mFontMetrics)
  {
    // Check for the very common case of trying to get the width of a single
    // space.
    if ((1 == aLength) && (aString[0] == ' '))
    {
      nsFontMetricsOS2* fontMetricsOS2 = (nsFontMetricsOS2*)mFontMetrics;
      return fontMetricsOS2->GetSpaceWidth(aWidth);
    }

    SIZEL size;

    SetupFontAndTextColor ();
    ::GetTextExtentPoint32(mPS, aString, aLength, &size);
    aWidth = NSToCoordRound(float(size.cx) * mP2T);

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsRenderingContextOS2::GetWidth(const char *aString,
                                PRInt32     aLength,
                                PRInt32     aAvailWidth,
                                PRInt32*    aBreaks,
                                PRInt32     aNumBreaks,
                                nscoord&    aWidth,
                                PRInt32&    aNumCharsFit,
                                PRInt32*    aFontID = nsnull)
{
  NS_PRECONDITION(aBreaks[aNumBreaks - 1] == aLength, "invalid break array");

  if (nsnull != mFontMetrics) {
    // If we need to back up this state represents the last place we could
    // break. We can use this to avoid remeasuring text
    struct PrevBreakState {
      PRInt32   mBreakIndex;
      nscoord   mWidth;    // accumulated width to this point

      PrevBreakState() {
        mBreakIndex = -1;  // not known (hasn't been computed)
        mWidth = 0;
      }
    };

    // Initialize OUT parameter
    aNumCharsFit = 0;

    // Setup the font and foreground color
    SetupFontAndTextColor ();

    // Iterate each character in the string and determine which font to use
    nsFontMetricsOS2* metrics = (nsFontMetricsOS2*)mFontMetrics;
    PrevBreakState    prevBreakState;
    nscoord           width = 0;
    PRInt32           start = 0;
    nscoord           aveCharWidth;
    metrics->GetAveCharWidth(aveCharWidth);

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
      if (aLength < estimatedBreakOffset) {
        // All the characters should fit
        numChars = aLength - start;
        breakIndex = aNumBreaks - 1;

      } else {
        breakIndex = prevBreakState.mBreakIndex;
        while (((breakIndex + 1) < aNumBreaks) &&
               (aBreaks[breakIndex + 1] <= estimatedBreakOffset)) {
          breakIndex++;
        }
        if (breakIndex == prevBreakState.mBreakIndex) {
          breakIndex++; // make sure we advanced past the previous break index
        }
        numChars = aBreaks[breakIndex] - start;
      }

      // Measure the text
      nscoord twWidth;
      if ((1 == numChars) && (aString[start] == ' ')) {
        metrics->GetSpaceWidth(twWidth);

      } else {
        SIZEL size;
        ::GetTextExtentPoint32(mPS, &aString[start], numChars, &size);
        twWidth = NSToCoordRound(float(size.cx) * mP2T);
      }

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
        prevBreakState.mBreakIndex = breakIndex;
        prevBreakState.mWidth = width;

      } else {
        // See if we can just back up to the previous saved state and not
        // have to measure any text
        if (prevBreakState.mBreakIndex > 0) {
          // If the previous break index is just before the current break index
          // then we can use it
          if (prevBreakState.mBreakIndex == (breakIndex - 1)) {
            aNumCharsFit = aBreaks[prevBreakState.mBreakIndex];
            width = prevBreakState.mWidth;
            break;
          }
        }
          
        // We can't just revert to the previous break state
        if (0 == breakIndex) {
          // There's no place to back up to so even though the text doesn't fit
          // return it anyway
          aNumCharsFit += numChars;
          width += twWidth;
          break;
        }

        // Repeatedly back up until we get to where the text fits or we're all
        // the way back to the first word
        width += twWidth;
        while ((breakIndex >= 1) && (width > aAvailWidth)) {
          start = aBreaks[breakIndex - 1];
          numChars = aBreaks[breakIndex] - start;
          
          if ((1 == numChars) && (aString[start] == ' ')) {
            metrics->GetSpaceWidth(twWidth);

          } else {
            SIZEL size;
            ::GetTextExtentPoint32(mPS, &aString[start], numChars, &size);
            twWidth = NSToCoordRound(float(size.cx) * mP2T);
          }

          width -= twWidth;
          aNumCharsFit = start;
          breakIndex--;
        }
        break;
      }
    }

    aWidth = width;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsRenderingContextOS2::GetWidth( const nsString &aString,
                                               nscoord &aWidth,
                                               PRInt32 *aFontID)
{
   return GetWidth( aString.GetUnicode(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextOS2::GetWidth( const PRUnichar *aString,
                                               PRUint32 aLength,
                                               nscoord &aWidth,
                                               PRInt32 *aFontID)
{
  nsresult temp;
  char buf[1024];

  int newLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage, aString, aLength, buf, sizeof(buf));
  temp = GetWidth( buf, newLength, aWidth);
  return temp;
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString(const char *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  const nscoord* aSpacing)
{
  NS_PRECONDITION(mFontMetrics,"Something is wrong somewhere");

  // Take care of ascent and specifies the drawing on the baseline
  nscoord ascent;
  mFontMetrics->GetMaxAscent(ascent);
  aY += ascent;

  PRInt32 x = aX;
  PRInt32 y = aY;

  SetupFontAndTextColor ();

  INT dxMem[500];
  INT* dx0;
  if (nsnull != aSpacing) {
    dx0 = dxMem;
    if (aLength > 500) {
      dx0 = new INT[aLength];
    }
    mTMatrix.ScaleXCoords(aSpacing, aLength, dx0);
  }
  mTMatrix.TransformCoord(&x, &y);

  POINTL ptl = { x, y };
  mSurface->NS2PM (&ptl, 1);

  ::ExtTextOut(mPS, ptl.x, ptl.y, 0, NULL, aString, aLength, aSpacing ? dx0 : NULL);

  if ((nsnull != aSpacing) && (dx0 != dxMem)) {
    delete [] dx0;
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  char buf[1024];

  int newLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage, aString, aLength, buf, sizeof(buf));

  return DrawString( buf, newLength, aX, aY, aSpacing);
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString(const nsString& aString,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  return DrawString(aString.GetUnicode(), aString.Length(), aX, aY, aFontID, aSpacing);
}


// Image drawing: just proxy on to the image object, so no worries yet.
nsresult nsRenderingContextOS2::DrawImage( nsIImage *aImage, nscoord aX, nscoord aY)
{
   nscoord width, height;

   width = NSToCoordRound( mP2T * aImage->GetWidth());
   height = NSToCoordRound( mP2T * aImage->GetHeight());

   return this->DrawImage( aImage, aX, aY, width, height);
}

nsresult nsRenderingContextOS2::DrawImage( nsIImage *aImage, nscoord aX, nscoord aY,
                                           nscoord aWidth, nscoord aHeight)
{
   nsRect  tr;

   tr.x = aX;
   tr.y = aY;
   tr.width = aWidth;
   tr.height = aHeight;

   return this->DrawImage( aImage, tr);
}

nsresult nsRenderingContextOS2::DrawImage( nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
   nsRect sr,dr;

   sr = aSRect;
   mTMatrix.TransformCoord( &sr.x, &sr.y, &sr.width, &sr.height);

   dr = aDRect;
   mTMatrix.TransformCoord( &dr.x, &dr.y, &dr.width, &dr.height);

   return aImage->Draw( *this, mSurface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);
}

nsresult nsRenderingContextOS2::DrawImage( nsIImage *aImage, const nsRect& aRect)
{
   nsRect tr( aRect);
   mTMatrix.TransformCoord( &tr.x, &tr.y, &tr.width, &tr.height);

   return aImage->Draw( *this, mSurface, tr.x, tr.y, tr.width, tr.height);
}

/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
NS_IMETHODIMP
nsRenderingContextOS2::DrawTile(nsIImage *aImage,nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,
                                                    nscoord aWidth,nscoord aHeight)
{
  nscoord     orgX,orgY,orgWidth,orgHeight;
  PRBool      didtile = FALSE;

  // convert output platform, but no translation.. just scale
  orgX = aX0;
  orgY = aY0;
  orgWidth = aX1 - aX0;
  orgHeight = aY1 - aY0;
  mTMatrix.TransformCoord(&aX0,&aY0,&aWidth,&aHeight);
  mTMatrix.TransformCoord(&orgX,&orgY,&orgWidth,&orgHeight);
  aX1 = aX0 + orgWidth;
  aY1 = aY0 + orgHeight;

//  if ( PR_TRUE==CanTile(aWidth,aHeight) ) {    
//    didtile = ((nsImageOS2*)aImage)->PatBltTile(*this,mSurface,aX0,aY0,aX1,aY1,aWidth,aHeight);
//  }
      
//  if (PR_FALSE ==didtile){
    // rely on the slower tiler supported in nsRenderingContextWin.. don't have 
    // to use xplatform which is really slow (slowest is the only one that supports transparency
    didtile = ((nsImageOS2*)aImage)->DrawTile(*this,mSurface,aX0,aY0,aX1,aY1,aWidth,aHeight);
//  }

  return NS_OK;
}


nsresult nsRenderingContextOS2::CopyOffScreenBits(
                     nsDrawingSurface aSrcSurf, PRInt32 aSrcX, PRInt32 aSrcY,
                     const nsRect &aDestBounds, PRUint32 aCopyFlags)
{
   NS_ASSERTION( aSrcSurf && mSurface && mMainSurface, "bad surfaces");

   nsDrawingSurfaceOS2 *SourceSurf = (nsDrawingSurfaceOS2 *) aSrcSurf;
   nsDrawingSurfaceOS2 *DestSurf;
   HPS DestPS;

   if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
   {
      DestSurf = mSurface;
      DestPS   = mPS;
   } else
   {
      DestSurf = mMainSurface;
      DestPS   = mMainSurface->GetPS ();
   }

   PRUint32 DestHeight   = DestSurf->GetHeight ();
   PRUint32 SourceHeight = SourceSurf->GetHeight ();


   if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
   {
      HRGN SourceClipRegion = OS2_CopyClipRegion (SourceSurf->GetPS ());

      // Currently clip region is in offscreen drawing surface coordinate system.
      // If offscreen surface have different height than destination surface,
      // then clipping region should be shifted down by amount of this difference.

      if ( (SourceClipRegion) && (SourceHeight != DestHeight) )
      {
         POINTL Offset = { 0, -(SourceHeight - DestHeight) };

         GFX (::GpiOffsetRegion (DestPS, SourceClipRegion, &Offset), FALSE);
      }

      OS2_SetClipRegion (DestPS, SourceClipRegion);
   }


   nsRect drect( aDestBounds);

   if( aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
      mTMatrix.TransformCoord( &aSrcX, &aSrcY);

   if( aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
      mTMatrix.TransformCoord( &drect.x, &drect.y,
                               &drect.width, &drect.height);

   // Note rects for GpiBitBlt are in-ex.

   POINTL Points [3] = { drect.x, DestHeight - drect.y - drect.height,    // TLL
                         drect.x + drect.width, DestHeight - drect.y,     // TUR
                         aSrcX, SourceHeight - aSrcY - drect.height };    // SLL


   GFX (::GpiBitBlt (DestPS, SourceSurf->GetPS (), 3, Points, ROP_SRCCOPY, BBO_OR), GPI_ERROR);

   return NS_OK;
}

nsresult nsRenderingContextOS2::RetrieveCurrentNativeGraphicData(PRUint32* ngd)
{
  if(ngd != nsnull)
    *ngd = (PRUint32)mPS;
  return NS_OK;
}

// Clip region helper functions --------------------------------------------

/* hrgnCombine becomes the clip region.  Any current clip region is       */
/* dealt with.  hrgnCombine may be NULLHANDLE.                            */
/* Return value is lComplexity.                                           */
/* lMode should be CRGN_*                                                 */
LONG OS2_CombineClipRegion( HPS hps, HRGN hrgnCombine, LONG lMode)
{
   if (!hps) return RGN_ERROR;

   HRGN hrgnClip = 0;
   LONG rc = RGN_NULL;

   GFX (::GpiSetClipRegion (hps, 0, &hrgnClip), RGN_ERROR); // Get the current clip region and deselect it

   if (hrgnClip && hrgnClip != HRGN_ERROR)
   {
      if (lMode != CRGN_COPY)    // If necessarry combine with previous clip region
         GFX (::GpiCombineRegion (hps, hrgnCombine, hrgnClip, hrgnCombine, lMode), RGN_ERROR);
      
      GFX (::GpiDestroyRegion (hps, hrgnClip), FALSE);
   }

   if (hrgnCombine)
      rc = GFX (::GpiSetClipRegion (hps, hrgnCombine, NULL), RGN_ERROR);  // Set new clip region

   return rc;
}

/* Return value is HRGN_ */
HRGN OS2_CopyClipRegion( HPS hps)
{
  if (!hps) return HRGN_ERROR;

  HRGN hrgn = 0, hrgnClip;

  // Get current clip region
  GFX (::GpiSetClipRegion (hps, 0, &hrgnClip), RGN_ERROR);

  if (hrgnClip && hrgnClip != HRGN_ERROR)
  {
     hrgn = GFX (::GpiCreateRegion (hps, 0, NULL), RGN_ERROR);     // Create empty region and combine with current
     GFX (::GpiCombineRegion (hps, hrgn, hrgnClip, 0, CRGN_COPY), RGN_ERROR);
     GFX (::GpiSetClipRegion (hps, hrgnClip, NULL), RGN_ERROR);    // restore current clip region
  }

  return hrgn;
}

// Keep coordinate within 32-bit limits
NS_IMETHODIMP nsRenderingContextOS2::ConditionRect(nscoord &x, nscoord &y, nscoord &w, nscoord &h)
{
  if (y < -134217728)
    y = -134217728;

  if (y + h > 134217727)
    h  = 134217727 - y;

  if (x < -134217728)
    x = -134217728;

  if (x + w > 134217727)
    w  = 134217727 - x;

  return NS_OK;
}
