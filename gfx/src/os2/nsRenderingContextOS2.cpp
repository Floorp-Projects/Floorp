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
#include "prprf.h"
#include "nsIRenderingContextOS2.h"


// helper clip region functions - defined at the bottom of this file.
LONG OS2_CombineClipRegion( HPS hps, HRGN hrgnCombine, LONG lMode);
HRGN OS2_CopyClipRegion( HPS hps);
#define OS2_SetClipRegion(hps,hrgn) OS2_CombineClipRegion(hps, hrgn, CRGN_COPY)

BOOL doSetupFontAndTextColor = PR_TRUE;
BOOL doSetupFont = PR_TRUE;

#define FLAG_CLIP_VALID       0x0001
#define FLAG_CLIP_CHANGED     0x0002
#define FLAG_LOCAL_CLIP_VALID 0x0004

#define FLAGS_ALL             (FLAG_CLIP_VALID | FLAG_CLIP_CHANGED | FLAG_LOCAL_CLIP_VALID)

class GraphicsState
{
public:
  GraphicsState();
  GraphicsState(GraphicsState &aState);
  ~GraphicsState();

  GraphicsState   *mNext;
  nsTransform2D   mMatrix;
  nsRect          mLocalClip;
  HRGN            mClipRegion;
  nscolor         mColor;
  nsIFontMetrics  *mFontMetrics;
  PRInt32         mFlags;
  nsLineStyle     mLineStyle;
};

GraphicsState :: GraphicsState()
{
  mNext = nsnull;
  mMatrix.SetToIdentity();  
  mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
  mClipRegion = NULL;
  mColor = NS_RGB(0, 0, 0);
  mFontMetrics = nsnull;
  mFlags = ~FLAGS_ALL;
  mLineStyle = nsLineStyle_kSolid;
}

GraphicsState :: GraphicsState(GraphicsState &aState) :
                               mMatrix(&aState.mMatrix),
                               mLocalClip(aState.mLocalClip)
{
  mNext = &aState;
  mClipRegion = NULL;
  mColor = NS_RGB(0, 0, 0);
  mFontMetrics = nsnull;
  mFlags = ~FLAGS_ALL;
  mLineStyle = aState.mLineStyle;
}

GraphicsState :: ~GraphicsState()
{
  if (NULL != mClipRegion)
  {
    printf( "oops, leaked a region from rc-gs\n");
    mClipRegion = NULL;
  }
}

// Rendering context -------------------------------------------------------

// Init-term stuff ---------------------------------------------------------

nsRenderingContextOS2::nsRenderingContextOS2()
{
   NS_INIT_REFCNT();

   mContext = nsnull;
   mSurface = nsnull;
   mPS = 0;
   mMainSurface = nsnull;
   mColor = NS_RGB( 0, 0, 0);
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

  mStateCache = new nsVoidArray();

  //create an initial GraphicsState

  PushState();

  mP2T = 1.0f;
}

nsRenderingContextOS2::~nsRenderingContextOS2()
{
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mFontMetrics);

  //destroy the initial GraphicsState
  PRBool clipState;
  PopState (clipState);

  if (nsnull != mStateCache)
  {
    PRInt32 cnt = mStateCache->Count();

    while (--cnt >= 0)
    {
      GraphicsState *state = (GraphicsState *)mStateCache->ElementAt(cnt);
      if (state->mClipRegion) {
         GFX (::GpiDestroyRegion (mPS, state->mClipRegion), FALSE);
         state->mClipRegion = 0;
      }
      mStateCache->RemoveElementAt(cnt);

      if (nsnull != state)
        delete state;
    }

    delete mStateCache;
    mStateCache = nsnull;
  }

   // Release surfaces and the palette
   NS_IF_RELEASE(mMainSurface);
   NS_IF_RELEASE(mSurface);
}

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

NS_IMPL_ADDREF(nsRenderingContextOS2)
NS_IMPL_RELEASE(nsRenderingContextOS2)

NS_IMETHODIMP
nsRenderingContextOS2::Init( nsIDeviceContext *aContext,
                             nsIWidget *aWindow)
{
   mContext = aContext;
   NS_IF_ADDREF(mContext);

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

   return CommonInit();
}

NS_IMETHODIMP
nsRenderingContextOS2::Init( nsIDeviceContext *aContext,
                             nsDrawingSurface aSurface)
{
   mContext = aContext;
   NS_IF_ADDREF(mContext);

   // Add a couple of references to the onscreen (or print, more likely)
   mSurface = (nsDrawingSurfaceOS2 *) aSurface;

  if (nsnull != mSurface)
  {
    mPS = mSurface->GetPS ();
    NS_ADDREF(mSurface);

    mMainSurface = mSurface;
    NS_ADDREF(mMainSurface);
  }

   return CommonInit();
}

nsresult nsRenderingContextOS2::SetupPS (void)
{
   LONG BlackColor, WhiteColor;

#ifdef COLOR_256
   // If this is a palette device, then select and realize the palette
   nsPaletteInfo palInfo;
   mContext->GetPaletteInfo(palInfo);

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
#endif
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

// Presentation space page units (& so world coords) are PU_PELS.
// We have a matrix, mTMatrix, which converts from the passed in app units
// to pels.  Note there is *no* guarantee that app units == twips.
nsresult nsRenderingContextOS2::CommonInit()
{
   float app2dev;

   mContext->GetAppUnitsToDevUnits( app2dev);
   mTranMatrix->AddScale( app2dev, app2dev);
   mContext->GetDevUnitsToAppUnits( mP2T);

   mContext->GetGammaTable(mGammaTable);

   return SetupPS ();
}

// PS & drawing surface management -----------------------------------------

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

NS_IMETHODIMP
nsRenderingContextOS2::SelectOffScreenDrawingSurface( nsDrawingSurface aSurface)
{
   if (aSurface != mSurface)
   {
      if(nsnull != aSurface)
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
NS_IMETHODIMP
nsRenderingContextOS2::GetDrawingSurface( nsDrawingSurface *aSurface)
{
   if( !aSurface)
      return NS_ERROR_NULL_POINTER;

   *aSurface = (void*)((nsDrawingSurfaceOS2 *) mSurface);
   return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextOS2::GetHints(PRUint32& aResult)
{
  PRUint32 result = 0;

  aResult = result;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::Reset()
{
   // okay, what's this supposed to do?  Empty the state stack?
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::GetDeviceContext( nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2 :: PushState(void)
{
  PRInt32 cnt = mStateCache->Count();

  if (cnt == 0)
  {
    if (nsnull == mStates)
      mStates = new GraphicsState();
    else
      mStates = new GraphicsState(*mStates);
  }
  else
  {
    GraphicsState *state = (GraphicsState *)mStateCache->ElementAt(cnt - 1);
    mStateCache->RemoveElementAt(cnt - 1);

    state->mNext = mStates;

    //clone state info

    state->mMatrix = mStates->mMatrix;
    state->mLocalClip = mStates->mLocalClip;
// we don't want to NULL this out since we reuse the region
// from state to state. if we NULL it, we need to also delete it,
// which means we'll just re-create it when we push the clip state. MMP
//   state->mClipRegion = OS2_CopyClipRegion( mPS);
    state->mFlags = ~FLAGS_ALL;
    state->mLineStyle = mStates->mLineStyle;

    mStates = state;
  }

  if (nsnull != mStates->mNext)
  {
    mStates->mNext->mColor = mColor;
    mStates->mNext->mFontMetrics = mFontMetrics;
    NS_IF_ADDREF(mStates->mNext->mFontMetrics);
  }

  mTranMatrix = &mStates->mMatrix;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2 :: PopState(PRBool &aClipEmpty)
{
  PRBool  retval = PR_FALSE;

  if (nsnull == mStates)
  {
    NS_ASSERTION(!(nsnull == mStates), "state underflow");
  }
  else
  {
    GraphicsState *oldstate = mStates;

    mStates = mStates->mNext;

    mStateCache->AppendElement(oldstate);

    if (nsnull != mStates)
    {
      mTranMatrix = &mStates->mMatrix;

      GraphicsState *pstate;

      if (oldstate->mFlags & FLAG_CLIP_CHANGED)
      {
        pstate = mStates;

        //the clip rect has changed from state to state, so
        //install the previous clip rect

        while ((nsnull != pstate) && !(pstate->mFlags & FLAG_CLIP_VALID))
          pstate = pstate->mNext;

        if (nsnull != pstate)
        {
          // set copy of pstate->mClipRegion as current clip region
          HRGN hrgn = GFX (::GpiCreateRegion (mPS, 0, NULL), RGN_ERROR);
          GFX (::GpiCombineRegion (mPS, hrgn, pstate->mClipRegion, 0, CRGN_COPY), RGN_ERROR);
          int cliptype = OS2_SetClipRegion (mPS, hrgn);

          if (cliptype == RGN_NULL)
            retval = PR_TRUE;
        }
      }

      oldstate->mFlags &= ~FLAGS_ALL;

      NS_IF_RELEASE(mFontMetrics);
      mFontMetrics = mStates->mFontMetrics;

      mColor = mStates->mColor;

      SetLineStyle(mStates->mLineStyle);
    }
    else
      mTranMatrix = nsnull;
  }

  aClipEmpty = retval;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::IsVisibleRect( const nsRect &aRect,
                                                    PRBool &aIsVisible)
{
   nsRect trect( aRect);
   mTranMatrix->TransformCoord( &trect.x, &trect.y,
                            &trect.width, &trect.height);
   RECTL rcl;
   mSurface->NS2PM_ININ (trect, rcl);

   LONG rc = GFX (::GpiRectVisible( mPS, &rcl), RVIS_ERROR);

   aIsVisible = (rc == RVIS_PARTIAL || rc == RVIS_VISIBLE) ? PR_TRUE : PR_FALSE;


   return NS_OK;
}

// Return PR_TRUE if clip region is now empty
NS_IMETHODIMP nsRenderingContextOS2::SetClipRect( const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  nsRect  trect = aRect;
  RECTL   rcl;
  int     cliptype = RGN_ERROR;

	mTranMatrix->TransformCoord(&trect.x, &trect.y,
                           &trect.width, &trect.height);

  mStates->mLocalClip = aRect;
  mStates->mFlags |= FLAG_LOCAL_CLIP_VALID;

  //how we combine the new rect with the previous?

  if( trect.width == 0 || trect.height == 0)
  {
    // Mozilla assumes that clip region with zero width or height does not produce any output - everything is clipped.
    // GPI does not support clip regions with zero width or height. We cheat by creating 1x1 clip region far outside
    // of real drawing region limits.

    if( aCombine == nsClipCombine_kIntersect || aCombine == nsClipCombine_kReplace)
    {
      PushClipState();

      rcl.xLeft   = -10000;
      rcl.xRight  = -9999;
      rcl.yBottom = -10000;
      rcl.yTop    = -9999;

      HRGN hrgn = GFX (::GpiCreateRegion( mPS, 1, &rcl), RGN_ERROR);
      OS2_SetClipRegion (mPS, hrgn);

      cliptype = RGN_NULL;      // Should pretend that clipping region is empty
    }
    else if( aCombine == nsClipCombine_kUnion || aCombine == nsClipCombine_kSubtract)
    {
      PushClipState();

      // Clipping region is already correct. Just need to obtain it's complexity
      POINTL Offset = { 0, 0 };

      cliptype = GFX (::GpiOffsetClipRegion (mPS, &Offset), RGN_ERROR);
    }
    else
      NS_ASSERTION(PR_FALSE, "illegal clip combination");
  }
  else
  {
    if (aCombine == nsClipCombine_kIntersect)
    {
      PushClipState();

      mSurface->NS2PM_ININ (trect, rcl);
      cliptype = GFX (::GpiIntersectClipRectangle(mPS, &rcl), RGN_ERROR);
    }
    else if (aCombine == nsClipCombine_kUnion)
    {
      PushClipState();

      mSurface->NS2PM_INEX (trect, rcl);
      HRGN hrgn = GFX (::GpiCreateRegion(mPS, 1, &rcl), RGN_ERROR);

      if( hrgn )
        cliptype = OS2_CombineClipRegion(mPS, hrgn, CRGN_OR);
    }
    else if (aCombine == nsClipCombine_kSubtract)
    {
      PushClipState();

      mSurface->NS2PM_ININ (trect, rcl);
      cliptype = GFX (::GpiExcludeClipRectangle(mPS, &rcl), RGN_ERROR);
    }
    else if (aCombine == nsClipCombine_kReplace)
    {
      PushClipState();

      mSurface->NS2PM_INEX (trect, rcl);
      HRGN hrgn = GFX (::GpiCreateRegion(mPS, 1, &rcl), RGN_ERROR);

      if( hrgn )
        cliptype = OS2_SetClipRegion(mPS, hrgn);
    }
    else
      NS_ASSERTION(PR_FALSE, "illegal clip combination");
  }

  if (cliptype == RGN_NULL)
    aClipEmpty = PR_TRUE;
  else
    aClipEmpty = PR_FALSE;

  return NS_OK;
}

// rc is whether there is a cliprect to return
NS_IMETHODIMP nsRenderingContextOS2::GetClipRect( nsRect &aRect, PRBool &aClipValid)
{
  if (mStates->mFlags & FLAG_LOCAL_CLIP_VALID)
  {
    aRect = mStates->mLocalClip;
    aClipValid = PR_TRUE;
  }
  else
    aClipValid = PR_FALSE;

  return NS_OK;
}

// Return PR_TRUE if clip region is now empty
NS_IMETHODIMP nsRenderingContextOS2::SetClipRegion( const nsIRegion &aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
   nsRegionOS2 *pRegion = (nsRegionOS2 *) &aRegion;
   PRUint32     ulHeight = mSurface->GetHeight ();

   HRGN hrgn = pRegion->GetHRGN( ulHeight, mPS);
  int cmode, cliptype;

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

  if (NULL != hrgn)
  {
    mStates->mFlags &= ~FLAG_LOCAL_CLIP_VALID;
    PushClipState();
    cliptype = OS2_CombineClipRegion( mPS, hrgn, cmode);
  }
  else
    return PR_FALSE;

  if (cliptype == RGN_NULL)
    aClipEmpty = PR_TRUE;
  else
    aClipEmpty = PR_FALSE;

   return NS_OK;
}

/**
 * Fills in |aRegion| with a copy of the current clip region.
 */
NS_IMETHODIMP nsRenderingContextOS2::CopyClipRegion(nsIRegion &aRegion)
{
  HRGN hr = OS2_CopyClipRegion(mPS);

  if (hr == HRGN_ERROR)
    return NS_ERROR_FAILURE;

  //((nsRegionOS2 *)&aRegion)->mRegion = hr;

  return NS_OK;
}

// Somewhat dubious & rather expensive
NS_IMETHODIMP nsRenderingContextOS2::GetClipRegion( nsIRegion **aRegion)
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

      pRegion->InitWithHRGN (hrgnClip, ulHeight, mPS);
      GFX (::GpiSetClipRegion (mPS, hrgnClip, &hrgnDummy), RGN_ERROR);
   }
   else
      pRegion->Init();

   *aRegion = pRegion;

   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::SetColor( nscolor aColor)
{
   mColor = aColor;
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::GetColor( nscolor &aColor) const
{
   aColor = mColor;
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::SetLineStyle( nsLineStyle aLineStyle)
{
   mLineStyle = aLineStyle;
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::GetLineStyle( nsLineStyle &aLineStyle)
{
   aLineStyle = mLineStyle;
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::SetFont( const nsFont &aFont)
{
   NS_IF_RELEASE( mFontMetrics);
   mContext->GetMetricsFor( aFont, mFontMetrics);

   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::SetFont( nsIFontMetrics *aFontMetrics)
{
   NS_IF_RELEASE( mFontMetrics);
   mFontMetrics = aFontMetrics;
   NS_IF_ADDREF( mFontMetrics);

   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::GetFontMetrics( nsIFontMetrics*& aFontMetrics)
{
  NS_IF_ADDREF(mFontMetrics);
  aFontMetrics = mFontMetrics;

  return NS_OK;
}

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextOS2::Translate( nscoord aX, nscoord aY)
{
   mTranMatrix->AddTranslation( (float) aX, (float) aY);
   return NS_OK;
}

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextOS2::Scale( float aSx, float aSy)
{
   mTranMatrix->AddScale(aSx, aSy);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::GetCurrentTransform( nsTransform2D *&aTransform)
{
  aTransform = mTranMatrix;
  return NS_OK;
}


// The idea here is to create an offscreen surface for blitting with.
// I can't find any way for people to resize the bitmap created here,
// so I guess this gets called quite often.
//
// I'm reliably told that 'aBounds' is in device units, and that the
// position oughtn't to be ignored, but for all intents & purposes can be.
//
NS_IMETHODIMP nsRenderingContextOS2::CreateDrawingSurface( nsRect *aBounds,
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

NS_IMETHODIMP nsRenderingContextOS2::DestroyDrawingSurface( nsDrawingSurface aDS)
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

NS_IMETHODIMP nsRenderingContextOS2::DrawLine( nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
   mTranMatrix->TransformCoord( &aX0, &aY0);
   mTranMatrix->TransformCoord( &aX1, &aY1);

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

NS_IMETHODIMP nsRenderingContextOS2::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
   PMDrawPoly( aPoints, aNumPoints, PR_FALSE);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::DrawPolygon( const nsPoint aPoints[], PRInt32 aNumPoints)
{
   PMDrawPoly( aPoints, aNumPoints, PR_FALSE);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::FillPolygon( const nsPoint aPoints[], PRInt32 aNumPoints)
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
         mTranMatrix->TransformCoord( (int*)&pp->x, (int*)&pp->y);
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

NS_IMETHODIMP nsRenderingContextOS2::DrawRect( const nsRect& aRect)
{
   nsRect tr = aRect;
   PMDrawRect( tr, FALSE);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::DrawRect( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
   nsRect tr( aX, aY, aWidth, aHeight);
   PMDrawRect( tr, FALSE);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::FillRect( const nsRect& aRect)
{
   nsRect tr = aRect;
   PMDrawRect( tr, TRUE);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::FillRect( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
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
   mTranMatrix->TransformCoord( &rect.x, &rect.y, &rect.width, &rect.height);

   RECTL rcl;
   mSurface->NS2PM_ININ (rect, rcl);

   GFX (::GpiMove (mPS, (PPOINTL) &rcl), FALSE);

   if (rcl.xLeft == rcl.xRight || rcl.yTop == rcl.yBottom)
   {
       // only draw line if it has a non-zero height and width
      if( rect.width && rect.height )
      {
         SetupLineColorAndStyle ();
         GFX (::GpiLine (mPS, ((PPOINTL)&rcl) + 1), GPI_ERROR);
      }
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
NS_IMETHODIMP nsRenderingContextOS2::DrawEllipse( const nsRect& aRect)
{
   nsRect tRect( aRect);
   PMDrawArc( tRect, PR_FALSE, PR_TRUE);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::DrawEllipse( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
   nsRect tRect( aX, aY, aWidth, aHeight);
   PMDrawArc( tRect, PR_FALSE, PR_TRUE);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::FillEllipse( const nsRect& aRect)
{
   nsRect tRect( aRect);
   PMDrawArc( tRect, PR_TRUE, PR_TRUE);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::FillEllipse( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
   nsRect tRect( aX, aY, aWidth, aHeight);
   PMDrawArc( tRect, PR_TRUE, PR_TRUE);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::DrawArc( const nsRect& aRect,
                                         float aStartAngle, float aEndAngle)
{
   nsRect tRect( aRect);
   PMDrawArc( tRect, PR_FALSE, PR_FALSE, aStartAngle, aEndAngle);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::DrawArc( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                         float aStartAngle, float aEndAngle)
{
   nsRect tRect( aX, aY, aWidth, aHeight);
   PMDrawArc( tRect, PR_FALSE, PR_FALSE, aStartAngle, aEndAngle);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::FillArc( const nsRect& aRect,
                                         float aStartAngle, float aEndAngle)
{
   nsRect tRect( aRect);
   PMDrawArc( tRect, PR_TRUE, PR_FALSE, aStartAngle, aEndAngle);
   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::FillArc( nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
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
   mTranMatrix->TransformCoord( &rect.x, &rect.y, &rect.width, &rect.height);

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
    if( !aLength )
    {
      aWidth = 0;
      return NS_OK;
    }

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

NS_IMETHODIMP nsRenderingContextOS2::GetWidth( const nsString &aString,
                                               nscoord &aWidth,
                                               PRInt32 *aFontID)
{
   return GetWidth( aString.get(), aString.Length(), aWidth, aFontID);
}

NS_IMETHODIMP nsRenderingContextOS2::GetWidth( const PRUnichar *aString,
                                               PRUint32 aLength,
                                               nscoord &aWidth,
                                               PRInt32 *aFontID)
{
  nsresult temp;
  char buf[1024];

  PRUnichar* pstr = (PRUnichar *)(const PRUnichar *)aString;

  PRUint32 start = 0;
  nscoord tWidth;
  BOOL createdFont = FALSE;
  LONG oldLcid;
  int convertedLength = 0;
  nsresult rv;

  aWidth = 0;

  if (((nsFontMetricsOS2*)mFontMetrics)->mCodePage == 1252)
  {
    for (PRUint32 i = 0; i < aLength; i++)
    {
      PRUnichar c = pstr[i];
      if (c > 0x00FF)
      {
        if (!createdFont)
        {
           // find the font specified for Unicode characters and use it to
           // display any characters above 0x00FF
          rv = ((nsFontMetricsOS2*)mFontMetrics)->SetUnicodeFont( mPS, 1 );

          if( NS_FAILED(rv) )
            continue;         // failed to load unicode font; use 'normal' font
          else
            createdFont = TRUE;
        }

         // Calculate width of the preceding non-unicode characters
        if( i-start > 0 )
        {
          convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage,
                                      &pstr[start], i-start, buf, sizeof(buf));
          GetWidth((const char*)buf,convertedLength,tWidth);
          aWidth += tWidth;
        }

         // Group unicode chars together
        PRUint32 end = i+1;
        while( end < aLength && pstr[end] > 0x00FF )
          end++;

         // change to use unicode font
        LONG oldLcid = GpiQueryCharSet(mPS);
        GFX (::GpiSetCharSet (mPS, 1), FALSE);
        doSetupFontAndTextColor = PR_FALSE;

        convertedLength = WideCharToMultiByte( 1208, &pstr[i], end-i, buf, sizeof(buf));
        GetWidth((const char*)buf,convertedLength,tWidth);
        aWidth+=tWidth;

        doSetupFontAndTextColor = PR_TRUE;
        GFX (::GpiSetCharSet (mPS, oldLcid), FALSE);
        start = end;
        i = end - 1;   /* get incremented by for loop */
      }
    } /*endfor*/
  }
  else
  {
    for (PRUint32 i = 0; i < aLength; i++)
    {
      PRUnichar c = pstr[i];
      if ((c >= 0x0080) && (c <= 0x00FF))
      {
        if (!createdFont)
        {
          nsFontHandle fh = nsnull;
          if (mFontMetrics)
            mFontMetrics->GetFontHandle(fh);
          nsFontHandleOS2 *pHandle = (nsFontHandleOS2 *) fh;
          FATTRS fattrs = pHandle->fattrs;
          fattrs.usCodePage = 1252;
          GFX (::GpiCreateLogFont (mPS, 0, 1, &fattrs), GPI_ERROR);
          createdFont = TRUE;
        }

        if( i-start > 0 )
        {
          convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage, &pstr[start], i-start, buf, sizeof(buf));
          GetWidth((const char*)buf,convertedLength,tWidth);
          aWidth += tWidth;
        }

         // Group chars together
        PRUint32 end = i+1;           
        while( end < aLength && pstr[end] >= 0x0080 && pstr[end] <= 0x00FF )
          end++;

        LONG oldLcid = GpiQueryCharSet(mPS);
        GFX (::GpiSetCharSet (mPS, 1), FALSE);
        doSetupFontAndTextColor = PR_FALSE;

        convertedLength = WideCharToMultiByte( 1252, &pstr[i], end-i, buf, sizeof(buf));
        GetWidth((const char*)buf,convertedLength,tWidth);
        aWidth+=tWidth;

        doSetupFontAndTextColor = PR_TRUE;
        GFX (::GpiSetCharSet (mPS, oldLcid), FALSE);
        start = end;
        i = end - 1;   /* get incremented by for loop */
      }
    }
  }

  if (createdFont)
  {
    GFX (::GpiDeleteSetId(mPS, 1), FALSE);
    createdFont = FALSE;
  }

  if( aLength-start > 0 )
  {
    convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage, &pstr[start], aLength-start, buf, sizeof(buf));
    temp = GetWidth( buf, convertedLength, tWidth);
    aWidth+= tWidth;
  }

  return temp;
}

NS_IMETHODIMP
nsRenderingContextOS2::GetTextDimensions(const char*       aString,
                                         PRInt32           aLength,
                                         PRInt32           aAvailWidth,
                                         PRInt32*          aBreaks,
                                         PRInt32           aNumBreaks,
                                         nsTextDimensions& aDimensions,
                                         PRInt32&          aNumCharsFit,
                                         nsTextDimensions& aLastWordDimensions,
                                         PRInt32*          aFontID)
{
}

NS_IMETHODIMP
nsRenderingContextOS2::GetTextDimensions(const PRUnichar*  aString,
                                         PRInt32           aLength,
                                         PRInt32           aAvailWidth,
                                         PRInt32*          aBreaks,
                                         PRInt32           aNumBreaks,
                                         nsTextDimensions& aDimensions,
                                         PRInt32&          aNumCharsFit,
                                         nsTextDimensions& aLastWordDimensions,
                                         PRInt32*          aFontID)
{
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRenderingContextOS2::GetTextDimensions(const char*       aString,
                                         PRUint32          aLength,
                                         nsTextDimensions& aDimensions)
{
  if (mFontMetrics) {
    mFontMetrics->GetMaxAscent(aDimensions.ascent);
    mFontMetrics->GetMaxDescent(aDimensions.descent);
  }
  return GetWidth(aString, aLength, aDimensions.width);
}

NS_IMETHODIMP
nsRenderingContextOS2::GetTextDimensions(const PRUnichar*  aString,
                                         PRUint32          aLength,
                                         nsTextDimensions& aDimensions,
                                         PRInt32*          aFontID)
{
  if (mFontMetrics) {
    mFontMetrics->GetMaxAscent(aDimensions.ascent);
    mFontMetrics->GetMaxDescent(aDimensions.descent);
  }
  return GetWidth(aString, aLength, aDimensions.width, aFontID);
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString(const char *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  const nscoord* aSpacing)
{
  nscoord y = 0;
  if (mFontMetrics) {
   mFontMetrics->GetMaxAscent(y);
  }
//  return DrawString2(aString, aLength, aX, aY + y, aSpacing);
  return DrawString2(aString, aLength, aX, aY, aSpacing);
}

NS_IMETHODIMP
nsRenderingContextOS2 :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                    nscoord aX, nscoord aY, PRInt32 aFontID,
                                    const nscoord* aSpacing)
{
  nscoord y = 0;
  if (mFontMetrics) {
    mFontMetrics->GetMaxAscent(y);
  }
  return DrawString2(aString, aLength, aX, aY + y, aFontID, aSpacing);
//  return DrawString2(aString, aLength, aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString(const nsString& aString,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString2(const char *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  const nscoord* aSpacing)
{
  NS_PRECONDITION(mFontMetrics,"Something is wrong somewhere");

  // Take care of ascent and specifies the drawing on the baseline
//  nscoord ascent;
//  mFontMetrics->GetMaxAscent(ascent);
//  aY -= ascent;

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
    mTranMatrix->ScaleXCoords(aSpacing, aLength, dx0);
  }
  mTranMatrix->TransformCoord(&x, &y);

  POINTL ptl = { x, y };
  mSurface->NS2PM (&ptl, 1);

  ::ExtTextOut(mPS, ptl.x, ptl.y, 0, NULL, aString, aLength, aSpacing ? dx0 : NULL);

  if ((nsnull != aSpacing) && (dx0 != dxMem)) {
    delete [] dx0;
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString2(const PRUnichar *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  char buf[1024];

  PRUnichar* pstr = (PRUnichar *)(const PRUnichar *)aString;

  PRInt32 x = aX;
  PRInt32 y = aY;

  PRUint32 start = 0;
  nscoord tWidth;
  BOOL createdFont = FALSE;
  LONG oldLcid;
  int convertedLength = 0;

  nsresult rv = NS_OK;

  if (((nsFontMetricsOS2*)mFontMetrics)->mCodePage == 1252)
  {
    for (PRUint32 i = 0; i < aLength; i++)
    {
      PRUnichar c = pstr[i];
      if (c > 0x00FF)
      {
        if (!createdFont)
        {
          rv = ((nsFontMetricsOS2*)mFontMetrics)->SetUnicodeFont( mPS, 1 );

          if( NS_FAILED(rv) )
            continue;         // failed to load unicode font; use 'normal' font
          else
            createdFont = TRUE;
        }

        if( i-start > 0 )
        {
          if( aSpacing )
          {
            convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage,
                                        &pstr[start], i-start, buf, sizeof(buf));
            char* bufptr = buf;                                

            while( start < i )
            {
              DrawString( bufptr, 1, x, y, NULL );
              x += *aSpacing;
              bufptr++;
              aSpacing++;
              start++;
            }
          }
          else
          {
            convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage,
                                        &pstr[start], i-start, buf, sizeof(buf));
            DrawString( buf, convertedLength, x, y, NULL );

            GetWidth( buf, convertedLength, tWidth );
            x += tWidth;
          }
        }

         // Group unicode chars together
        PRUint32 end = i+1;
        while( end < aLength && pstr[end] > 0x00FF )
          end++;

        LONG oldLcid = GpiQueryCharSet(mPS);
        GFX (::GpiSetCharSet (mPS, 1), FALSE);
        doSetupFont = PR_FALSE;

        if( aSpacing )
        {
          while( i < end )
          {
            convertedLength = WideCharToMultiByte( 1208, &pstr[i], 1, buf, sizeof(buf));
            rv = DrawString( buf, convertedLength, x, y, NULL );
            x += *aSpacing;
            aSpacing++;
            i++;
          }
        }
        else
        {
          convertedLength = WideCharToMultiByte( 1208, &pstr[i], end-i, buf, sizeof(buf));
          rv = DrawString( buf, convertedLength, x, y, NULL );
          GetWidth( buf, convertedLength, tWidth );
          x += tWidth;
        }

        doSetupFont = PR_TRUE;
        GFX (::GpiSetCharSet (mPS, oldLcid), FALSE);
        start = end;
        i = end - 1;
      }
    } /* endfor */
  }                                              
  else
  {
    for (PRUint32 i = 0; i < aLength; i++)
    {
      PRUnichar c = pstr[i];
      if ((c >= 0x0080) && (c <= 0x00FF))
      {
        if (!createdFont)
        {
          nsFontHandle fh = nsnull;
          if (mFontMetrics)
            mFontMetrics->GetFontHandle(fh);
          nsFontHandleOS2 *pHandle = (nsFontHandleOS2 *) fh;
          FATTRS fattrs = pHandle->fattrs;
          fattrs.usCodePage = 1252;
          GFX (::GpiCreateLogFont (mPS, 0, 1, &fattrs), GPI_ERROR);
          createdFont = TRUE;
        }

        if( i-start > 0 )
        {
          if( aSpacing )
          {
            while( start < i )
            {
              convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage,
                                          &pstr[start], 1, buf, sizeof(buf));
              DrawString( buf, convertedLength, x, y, NULL);
              x += *aSpacing;
              aSpacing++;
              start++;
            }
          }
          else
          {
            convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage,
                                        &pstr[start], i-start, buf, sizeof(buf));
            DrawString( buf, convertedLength, x, y, aSpacing);

            GetWidth( buf, convertedLength, tWidth );
            x += tWidth;
          }
        }

         // Group chars together
        PRUint32 end = i+1;
        while( end < aLength && pstr[end] >= 0x0080 && pstr[end] <= 0x00FF )
          end++;

        LONG oldLcid = GpiQueryCharSet(mPS);
        GFX (::GpiSetCharSet (mPS, 1), FALSE);
        doSetupFont = PR_FALSE;

        if( aSpacing )
        {
          while( i < end )
          {
            convertedLength = WideCharToMultiByte( 1252, &pstr[i], 1, buf, sizeof(buf));
            rv = DrawString( buf, convertedLength, x, y, NULL );
            x += *aSpacing;
            aSpacing++;
            i++;
          }
        }
        else
        {
          convertedLength = WideCharToMultiByte( 1252, &pstr[i], end-i, buf, sizeof(buf));
          rv = DrawString( buf, convertedLength, x, y, NULL );
          GetWidth( buf, convertedLength, tWidth );
          x += tWidth;
        }

        doSetupFont = PR_TRUE;
        GFX (::GpiSetCharSet (mPS, oldLcid), FALSE);
        start = end;
        i = end - 1;
      }
    } /* endfor */
  }

  if (createdFont)
  {
    GpiDeleteSetId(mPS, 1);
    createdFont = FALSE;
  }

  if( aLength-start > 0 )
  {
    if( aSpacing )
    {
       // In codepage 1252, we are guaranteed coming out of the for loop above
       // that each char coming out of the WideCharToMultibyte translation
       // has a one-to-one correspondence to a unicode character (i.e. the
       // length of the unicode text passed in to WideCharToMultibyte is equal
       // to convertedLength.  Therefore, we do one big conversion, and then
       // cycle through the buffer to position each char.
       // This is not necessarily true for other codepages, such as those
       // supporting DBCS, where one unicode char may map to 2 or 3 chars.  In
       // these cases, we have to do the translation a character at a time.
      if (((nsFontMetricsOS2*)mFontMetrics)->mCodePage == 1252)
      {
        convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage,
                                &pstr[start], aLength-start, buf, sizeof(buf));
        char* bufptr = buf;                                
        while( start < aLength )
        {
          DrawString( bufptr, 1, x, y, NULL );
          x += *aSpacing;
          bufptr++;
          aSpacing++;
          start++;
        }
      }
      else
      {
        while( start < aLength )
        {
          convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage,
                                      &pstr[start], 1, buf, sizeof(buf));
          DrawString( buf, convertedLength, x, y, NULL );
          x += *aSpacing;
          aSpacing++;
          start++;
        }
      }
    }
    else
    {
      convertedLength = WideCharToMultiByte( ((nsFontMetricsOS2*)mFontMetrics)->mCodePage,
                                &pstr[start], aLength-start, buf, sizeof(buf));
      rv = DrawString( buf, convertedLength, x, y, NULL );
    }
  }

  return rv;
}

// Image drawing: just proxy on to the image object, so no worries yet.
NS_IMETHODIMP nsRenderingContextOS2::DrawImage( nsIImage *aImage, nscoord aX, nscoord aY)
{
   nscoord width, height;

   width = NSToCoordRound( mP2T * aImage->GetWidth());
   height = NSToCoordRound( mP2T * aImage->GetHeight());

   return this->DrawImage( aImage, aX, aY, width, height);
}

NS_IMETHODIMP nsRenderingContextOS2::DrawImage( nsIImage *aImage, nscoord aX, nscoord aY,
                                           nscoord aWidth, nscoord aHeight)
{
   nsRect  tr;

   tr.x = aX;
   tr.y = aY;
   tr.width = aWidth;
   tr.height = aHeight;

   return this->DrawImage( aImage, tr);
}

NS_IMETHODIMP nsRenderingContextOS2::DrawImage( nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
   nsRect sr,dr;

   sr = aSRect;
   mTranMatrix->TransformCoord( &sr.x, &sr.y, &sr.width, &sr.height);
   sr.x = aSRect.x;
   sr.y = aSRect.y;
   mTranMatrix->TransformNoXLateCoord(&sr.x, &sr.y);

   dr = aDRect;
   mTranMatrix->TransformCoord( &dr.x, &dr.y, &dr.width, &dr.height);

   return aImage->Draw( *this, mSurface, sr.x, sr.y, sr.width, sr.height, dr.x, dr.y, dr.width, dr.height);
}

NS_IMETHODIMP nsRenderingContextOS2::DrawImage( nsIImage *aImage, const nsRect& aRect)
{
   nsRect tr( aRect);
   mTranMatrix->TransformCoord( &tr.x, &tr.y, &tr.width, &tr.height);

   return aImage->Draw( *this, mSurface, tr.x, tr.y, tr.width, tr.height);
}

NS_IMETHODIMP nsRenderingContextOS2::CopyOffScreenBits(
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
      mTranMatrix->TransformCoord( &aSrcX, &aSrcY);

   if( aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
      mTranMatrix->TransformCoord( &drect.x, &drect.y,
                               &drect.width, &drect.height);

   // Note rects for GpiBitBlt are in-ex.

   POINTL Points [3] = { drect.x, DestHeight - drect.y - drect.height,    // TLL
                         drect.x + drect.width, DestHeight - drect.y,     // TUR
                         aSrcX, SourceHeight - aSrcY - drect.height };    // SLL


   GFX (::GpiBitBlt (DestPS, SourceSurf->GetPS (), 3, Points, ROP_SRCCOPY, BBO_OR), GPI_ERROR);

   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::RetrieveCurrentNativeGraphicData(PRUint32* ngd)
{
  if(ngd != nsnull)
    *ngd = (PRUint32)mPS;
  return NS_OK;
}

void nsRenderingContextOS2::SetupFontAndTextColor (void)
{
   if (!doSetupFontAndTextColor)
      return;

   if( doSetupFont && mFontMetrics != mCurrFontMetrics )
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

void nsRenderingContextOS2::PushClipState(void)
{
  if (!(mStates->mFlags & FLAG_CLIP_CHANGED))
  {
    GraphicsState *tstate = mStates->mNext;

    //we have never set a clip on this state before, so
    //remember the current clip state in the next state on the
    //stack. kind of wacky, but avoids selecting stuff in the DC
    //all the damned time.

    if (nsnull != tstate)
    {
      tstate->mClipRegion = OS2_CopyClipRegion( mPS );

      if (tstate->mClipRegion != 0)
        tstate->mFlags |= FLAG_CLIP_VALID;
      else
        tstate->mFlags &= ~FLAG_CLIP_VALID;
    }
  
    mStates->mFlags |= FLAG_CLIP_CHANGED;
  }
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
