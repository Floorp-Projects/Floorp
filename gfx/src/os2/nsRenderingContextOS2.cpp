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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henry Sobotka <sobotka@axess.com> Jan. 2000 review and update
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
 * ***** END LICENSE BLOCK *****
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

// ToDo: Unicode text draw-er
#include "nsGfxDefs.h"

#include "nsRenderingContextOS2.h"
#include "nsFontMetricsOS2.h"
#include "nsRegionOS2.h"
#include "nsDeviceContextOS2.h"
#include "prprf.h"
#include "nsGfxCIID.h"
#include "nsUnicharUtils.h"


// helper clip region functions - defined at the bottom of this file.
LONG OS2_CombineClipRegion( HPS hps, HRGN hrgnCombine, LONG lMode);
HRGN OS2_CopyClipRegion( HPS hps);
#define OS2_SetClipRegion(hps,hrgn) OS2_CombineClipRegion(hps, hrgn, CRGN_COPY)

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
  mContext = nsnull;
  mSurface = nsnull;
  mPS = 0;
  mMainSurface = nsnull;
  mColor = NS_RGB( 0, 0, 0);
  mFontMetrics = nsnull;
  mLineStyle = nsLineStyle_kSolid;
  mPreservedInitialClipRegion = PR_FALSE;
  mPaletteMode = PR_FALSE;
  mCurrFontOS2 = nsnull;

  mStateCache = new nsVoidArray();
  mRightToLeftText = PR_FALSE;

  //create an initial GraphicsState

  PushState();

  mP2T = 1.0f;
}

nsRenderingContextOS2::~nsRenderingContextOS2()
{
  NS_IF_RELEASE(mContext);
  NS_IF_RELEASE(mFontMetrics);

  //destroy the initial GraphicsState
  PopState ();

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

nsresult nsRenderingContextOS2::SetupPS(void)
{
   LONG BlackColor, WhiteColor;

   // If this is a palette device, then set transparent colors
   if (((nsDeviceContextOS2*)mContext)->IsPaletteDevice())
   {
      BlackColor = GFX (::GpiQueryColorIndex(mPS, 0, MK_RGB (0x00, 0x00, 0x00)), GPI_ALTERROR);    // CLR_BLACK;
      WhiteColor = GFX (::GpiQueryColorIndex(mPS, 0, MK_RGB (0xFF, 0xFF, 0xFF)), GPI_ALTERROR);    // CLR_WHITE;

      mPaletteMode = PR_TRUE;
   }
   else
   {
      GFX (::GpiCreateLogColorTable(mPS, 0, LCOLF_RGB, 0, 0, 0), FALSE);

      BlackColor = MK_RGB(0x00, 0x00, 0x00);
      WhiteColor = MK_RGB(0xFF, 0xFF, 0xFF);

      mPaletteMode = PR_FALSE;
   }

   // Set image foreground and background colors. These are used in transparent images for blitting 1-bit masks.
   // To invert colors on ROP_SRCAND we map 1 to black and 0 to white
   IMAGEBUNDLE ib;
   ib.lColor     = BlackColor;           // map 1 in mask to 0x000000 (black) in destination
   ib.lBackColor = WhiteColor;           // map 0 in mask to 0xFFFFFF (white) in destination
   ib.usMixMode  = FM_OVERPAINT;
   ib.usBackMixMode = BM_OVERPAINT;
   GFX (::GpiSetAttrs(mPS, PRIM_IMAGE, IBB_COLOR | IBB_BACK_COLOR | IBB_MIX_MODE | IBB_BACK_MIX_MODE, 0, (PBUNDLE)&ib), FALSE);

   return NS_OK;
}

// Presentation space page units (& so world coords) are PU_PELS.
// We have a matrix, mTMatrix, which converts from the passed in app units
// to pels.  Note there is *no* guarantee that app units == twips.
nsresult nsRenderingContextOS2::CommonInit()
{
   float app2dev;

   app2dev = mContext->AppUnitsToDevUnits();
   mTranMatrix->AddScale( app2dev, app2dev);
   mP2T = mContext->DevUnitsToAppUnits();

   return SetupPS();
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

  PopState();

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

         SetupPS();
      }
      else // deselect current offscreen...
      {
         NS_IF_RELEASE(mSurface);
         mSurface = mMainSurface;
         mPS = mSurface->GetPS ();

         SetupPS();
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
   *aSurface = mSurface;
   return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextOS2::GetHints(PRUint32& aResult)
{
  PRUint32 result = 0;
  
  result |= NS_RENDERING_HINT_FAST_MEASURE;

  aResult = result;
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::Reset()
{
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

NS_IMETHODIMP nsRenderingContextOS2 :: PopState(void)
{
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
NS_IMETHODIMP nsRenderingContextOS2::SetClipRect( const nsRect& aRect, nsClipCombine aCombine)
{
  nsRect  trect = aRect;
  RECTL   rcl;

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
    }
    else if( aCombine == nsClipCombine_kUnion || aCombine == nsClipCombine_kSubtract)
    {
      PushClipState();

      // Clipping region is already correct. Just need to obtain it's complexity
      POINTL Offset = { 0, 0 };

      GFX (::GpiOffsetClipRegion (mPS, &Offset), RGN_ERROR);
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
      GFX (::GpiIntersectClipRectangle(mPS, &rcl), RGN_ERROR);
    }
    else if (aCombine == nsClipCombine_kUnion)
    {
      PushClipState();

      mSurface->NS2PM_INEX (trect, rcl);
      HRGN hrgn = GFX (::GpiCreateRegion(mPS, 1, &rcl), RGN_ERROR);

      if( hrgn )
        OS2_CombineClipRegion(mPS, hrgn, CRGN_OR);
    }
    else if (aCombine == nsClipCombine_kSubtract)
    {
      PushClipState();

      mSurface->NS2PM_ININ (trect, rcl);
      GFX (::GpiExcludeClipRectangle(mPS, &rcl), RGN_ERROR);
    }
    else if (aCombine == nsClipCombine_kReplace)
    {
      PushClipState();

      mSurface->NS2PM_INEX (trect, rcl);
      HRGN hrgn = GFX (::GpiCreateRegion(mPS, 1, &rcl), RGN_ERROR);

      if( hrgn )
	OS2_SetClipRegion(mPS, hrgn);
    }
    else
      NS_ASSERTION(PR_FALSE, "illegal clip combination");
  }

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
NS_IMETHODIMP nsRenderingContextOS2::SetClipRegion( const nsIRegion &aRegion, nsClipCombine aCombine)
{
   nsRegionOS2 *pRegion = (nsRegionOS2 *) &aRegion;
   PRUint32     ulHeight = mSurface->GetHeight ();

   HRGN hrgn = pRegion->GetHRGN( ulHeight, mPS);
   int cmode;

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
    OS2_CombineClipRegion( mPS, hrgn, cmode);
  }
  else
    return PR_FALSE;

  return NS_OK;
}

/**
 * Fills in |aRegion| with a copy of the current clip region.
 */
NS_IMETHODIMP nsRenderingContextOS2::CopyClipRegion(nsIRegion &aRegion)
{
#if 0
  HRGN hr = OS2_CopyClipRegion(mPS);

  if (hr == HRGN_ERROR)
    return NS_ERROR_FAILURE;

  ((nsRegionOS2 *)&aRegion)->mRegion = hr;

  return NS_OK;
#else
  NS_ASSERTION( 0, "nsRenderingContextOS2::CopyClipRegion() not implemented" );
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
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

NS_IMETHODIMP nsRenderingContextOS2::SetFont( const nsFont &aFont, nsIAtom* aLangGroup)
{
  mCurrFontOS2 = nsnull; // owned & released by mFontMetrics
  NS_IF_RELEASE(mFontMetrics);
  mContext->GetMetricsFor(aFont, aLangGroup, mFontMetrics);

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::SetFont( nsIFontMetrics *aFontMetrics)
{
  mCurrFontOS2 = nsnull; // owned & released by mFontMetrics
  NS_IF_RELEASE(mFontMetrics);
  mFontMetrics = aFontMetrics;
  NS_IF_ADDREF(mFontMetrics);

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
NS_IMETHODIMP nsRenderingContextOS2::CreateDrawingSurface(const nsRect& aBounds,
                             PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
   nsresult rc = NS_ERROR_FAILURE;

   nsOffscreenSurface *surf = new nsOffscreenSurface;

   if (!surf)
     return NS_ERROR_OUT_OF_MEMORY;

   rc = surf->Init( mMainSurface->GetPS (), aBounds.width, aBounds.height, aSurfFlags);

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
   LONG gcolor = MK_RGB (NS_GET_R (mColor),
                         NS_GET_G (mColor),
                         NS_GET_B (mColor));

   return (mPaletteMode) ? GFX (::GpiQueryColorIndex (mPS, 0, gcolor), GPI_ALTERROR) :
                           gcolor ;
}

void nsRenderingContextOS2::SetupLineColorAndStyle (void)
{
   
   LINEBUNDLE lineBundle;
   lineBundle.lColor = GetGPIColor ();

   GFX (::GpiSetAttrs (mPS, PRIM_LINE, LBB_COLOR, 0, (PBUNDLE)&lineBundle), FALSE);
   
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
   
}

void nsRenderingContextOS2::SetupFillColor (void)
{
   
   AREABUNDLE areaBundle;
   areaBundle.lColor = GetGPIColor ();

   GFX (::GpiSetAttrs (mPS, PRIM_AREA, ABB_COLOR, 0, (PBUNDLE)&areaBundle), FALSE);

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
   PMDrawPoly( aPoints, aNumPoints, PR_TRUE );
   return NS_OK;
}

 // bDoTransform defaults to PR_TRUE
void nsRenderingContextOS2::PMDrawPoly( const nsPoint aPoints[], PRInt32 aNumPoints, PRBool bFilled, PRBool bDoTransform)
{
   if( aNumPoints > 1)
   {
      // Xform coords
      POINTL  aptls[ 20];
      PPOINTL pts = aptls;

      if( aNumPoints > 20)
         pts = new POINTL[aNumPoints];

      PPOINTL pp = pts;
      const nsPoint *np = &aPoints[0];

      for( PRInt32 i = 0; i < aNumPoints; i++, pp++, np++)
      {
         pp->x = np->x;
         pp->y = np->y;
         if( bDoTransform )
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
   GFX (::GpiSetMix (mPS, FM_INVERT), FALSE);
   PMDrawRect(tr, FALSE);
   GFX (::GpiSetMix (mPS, CurMix), FALSE);
   return NS_OK;
}

void nsRenderingContextOS2::PMDrawRect( nsRect &rect, BOOL fill)
{
   mTranMatrix->TransformCoord( &rect.x, &rect.y, &rect.width, &rect.height);

   // only draw line if it has a non-zero height and width
   if ( !rect.width || !rect.height )
      return;

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
    // Check for the very common case of trying to get the width of a single
    // space.
    if ((1 == aLength) && (aString[0] == ' '))
    {
      return mFontMetrics->GetSpaceWidth(aWidth);
    }

    SetupFontAndColor();
    nscoord pxWidth = mCurrFontOS2->GetWidth(mPS, aString, aLength);
    aWidth = NSToCoordRound(float(pxWidth) * mP2T);

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

struct GetWidthData {
  HPS                   mPS;      // IN
  nsDrawingSurfaceOS2*  mSurface; // IN
  nsFontOS2*            mFont;    // IN/OUT (running)
  LONG                  mWidth;   // IN/OUT (running, accumulated width so far)
};

static PRBool PR_CALLBACK
do_GetWidth(const nsFontSwitch* aFontSwitch,
            const PRUnichar*    aSubstring,
            PRUint32            aSubstringLength,
            void*               aData)
{
  nsFontOS2* font = aFontSwitch->mFont;

  GetWidthData* data = (GetWidthData*)aData;
  if (data->mFont != font) {
    // the desired font is not the current font in the PS
    data->mFont = font;
    data->mSurface->SelectFont(data->mFont);
  }
  data->mWidth += font->GetWidth(data->mPS, aSubstring, aSubstringLength);
  return PR_TRUE; // don't stop till the end
}

NS_IMETHODIMP nsRenderingContextOS2::GetWidth( const PRUnichar *aString,
                                               PRUint32 aLength,
                                               nscoord &aWidth,
                                               PRInt32 *aFontID)
{
  if (!mFontMetrics)
    return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsOS2* metrics = (nsFontMetricsOS2*)mFontMetrics;
  GetWidthData data = {mPS, mSurface, mCurrFontOS2, 0};

  metrics->ResolveForwards(mPS, aString, aLength, do_GetWidth, &data);
  aWidth = NSToCoordRound(float(data.mWidth) * mP2T);

  if (data.mFont != mCurrFontOS2) {
    // If the font was changed along the way, restore our font
    mSurface->SelectFont(mCurrFontOS2);
  }

  if (aFontID)
    *aFontID = 0;

  return NS_OK;
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
  NS_PRECONDITION(aBreaks[aNumBreaks - 1] == aLength, "invalid break array");

  if (nsnull != mFontMetrics) {
    // Setup the font and foreground color
    SetupFontAndColor();

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
      nscoord pxWidth, twWidth = 0;
      if ((1 == numChars) && (aString[start] == ' ')) {
        mFontMetrics->GetSpaceWidth(twWidth);
      } 
      else if (numChars > 0) {
        pxWidth = mCurrFontOS2->GetWidth(mPS, &aString[start], numChars);
        twWidth = NSToCoordRound(float(pxWidth) * mP2T);
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
          else if (numChars > 0) {
            pxWidth = mCurrFontOS2->GetWidth(mPS, &aString[start], numChars);
            twWidth = NSToCoordRound(float(pxWidth) * mP2T);
          }

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
  HPS         mPS;                // IN
  nsDrawingSurfaceOS2*  mSurface; // IN
  nsFontOS2*  mFont;              // IN/OUT (running)
  float       mP2T;               // IN
  PRInt32     mAvailWidth;        // IN
  PRInt32*    mBreaks;            // IN
  PRInt32     mNumBreaks;         // IN
  nscoord     mSpaceWidth;        // IN
  nscoord     mAveCharWidth;      // IN
  PRInt32     mEstimatedNumChars; // IN (running -- to handle the edge case of one word)

  PRInt32     mNumCharsFit;  // IN/OUT -- accumulated number of chars that fit so far
  nscoord     mWidth;        // IN/OUT -- accumulated width so far

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
do_BreakGetTextDimensions(const nsFontSwitch* aFontSwitch,
                          const PRUnichar*    aSubstring,
                          PRUint32            aSubstringLength,
                          void*               aData)
{
  nsFontOS2* font = aFontSwitch->mFont;

  // Make sure the font is selected
  BreakGetTextDimensionsData* data = (BreakGetTextDimensionsData*)aData;
  if (data->mFont != font) {
    // the desired font is not the current font in the PS
    data->mFont = font;
    data->mSurface->SelectFont(data->mFont);
  }

   // set mMaxAscent & mMaxDescent if not already set in nsFontOS2 struct
  if (font->mMaxAscent == 0)
  {
    FONTMETRICS fm;
    GFX (::GpiQueryFontMetrics(data->mPS, sizeof (fm), &fm), FALSE);
    
    font->mMaxAscent  = NSToCoordRound( (fm.lMaxAscender-1) * data->mP2T );
    font->mMaxDescent = NSToCoordRound( (fm.lMaxDescender+1) * data->mP2T );
  }

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
      while (breakIndex + 1 < data->mNumBreaks &&
             data->mBreaks[breakIndex + 1] <= estimatedBreakOffset) {
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
      pxWidth = font->GetWidth(data->mPS, &pstr[start], numChars);
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
          pxWidth = font->GetWidth(data->mPS, &pstr[start], numChars);
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
    data->mFonts->AppendElement(font);
    data->mOffsets->AppendElement((void*)&pstr[numCharsFit]);
  }

  if (allDone) {
    // stop now
    return PR_FALSE;
  }

  return PR_TRUE; // don't stop if we still need to measure more characters
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
  if (!mFontMetrics)
    return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsOS2* metrics = (nsFontMetricsOS2*)mFontMetrics;

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

  BreakGetTextDimensionsData data = {mPS, mSurface, mCurrFontOS2, mP2T,
                                     aAvailWidth, aBreaks, aNumBreaks,
                                     spaceWidth, aveCharWidth, 0, 0, 0, -1, 0,
                                     &fonts, &offsets};

  metrics->ResolveForwards(mPS, aString, aLength, do_BreakGetTextDimensions, &data);

  if (data.mFont != mCurrFontOS2) {
    // If the font was changed along the way, restore our font
    mSurface->SelectFont(mCurrFontOS2);
  }

  if (aFontID)
    *aFontID = 0;

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
  nsFontOS2* font = (nsFontOS2*)fonts[0];
  NS_ASSERTION(font, "internal error in do_BreakGetTextDimensions");
  aDimensions.ascent = font->mMaxAscent;
  aDimensions.descent = font->mMaxDescent;

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
    font = (nsFontOS2*)fonts[currFont];
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
        font = (nsFontOS2*)fonts[currFont];
        nextOffset = (PRUnichar*)offsets[++currFont];
      } 
    }

    // see if the last word intersects with the current font
    // (we are testing for 'nextOffset-1 >= lastWord' since the
    // current font ends at nextOffset-1)
    if (nextOffset > lastWord) {
      if (aLastWordDimensions.ascent < font->mMaxAscent) {
        aLastWordDimensions.ascent = font->mMaxAscent;
      }
      if (aLastWordDimensions.descent < font->mMaxDescent) {
        aLastWordDimensions.descent = font->mMaxDescent;
      }
    }

    // see if we have not reached the last word yet
    if (pstr < lastWord) {
      if (aDimensions.ascent < font->mMaxAscent) {
        aDimensions.ascent = font->mMaxAscent;
      }
      if (aDimensions.descent < font->mMaxDescent) {
        aDimensions.descent = font->mMaxDescent;
      }
    }

    // advance to where the next font starts
    pstr = nextOffset;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsRenderingContextOS2::GetTextDimensions(const char*       aString,
                                         PRUint32          aLength,
                                         nsTextDimensions& aDimensions)
{
  GetWidth(aString, aLength, aDimensions.width);
  if (mFontMetrics)
  {
    mFontMetrics->GetMaxAscent(aDimensions.ascent);
    mFontMetrics->GetMaxDescent(aDimensions.descent);
  }
  return NS_OK;
}

struct GetTextDimensionsData {
  HPS                   mPS;      // IN
  float                 mP2T;     // IN
  nsDrawingSurfaceOS2*  mSurface; // IN
  nsFontOS2*            mFont;    // IN/OUT (running)
  LONG                  mWidth;   // IN/OUT (running)
  nscoord               mAscent;  // IN/OUT (running)
  nscoord               mDescent; // IN/OUT (running)
};

static PRBool PR_CALLBACK
do_GetTextDimensions(const nsFontSwitch* aFontSwitch,
                     const PRUnichar*    aSubstring,
                     PRUint32            aSubstringLength,
                     void*               aData)
{
  nsFontOS2* font = aFontSwitch->mFont;
  
  GetTextDimensionsData* data = (GetTextDimensionsData*)aData;
  if (data->mFont != font) {
    // the desired font is not the current font in the PS
    data->mFont = font;
    data->mSurface->SelectFont(data->mFont);
  }

  data->mWidth += font->GetWidth(data->mPS, aSubstring, aSubstringLength);

   // set mMaxAscent & mMaxDescent if not already set in nsFontOS2 struct
  if (font->mMaxAscent == 0) {
    FONTMETRICS fm;
    GFX (::GpiQueryFontMetrics(data->mPS, sizeof (fm), &fm), FALSE);
    font->mMaxAscent  = NSToCoordRound( (fm.lMaxAscender-1) * data->mP2T );
    font->mMaxDescent = NSToCoordRound( (fm.lMaxDescender+1) * data->mP2T );
  }

  if (data->mAscent < font->mMaxAscent) {
    data->mAscent = font->mMaxAscent;
  }
  if (data->mDescent < font->mMaxDescent) {
    data->mDescent = font->mMaxDescent;
  }

  return PR_TRUE; // don't stop till the end
}

NS_IMETHODIMP
nsRenderingContextOS2::GetTextDimensions(const PRUnichar*  aString,
                                         PRUint32          aLength,
                                         nsTextDimensions& aDimensions,
                                         PRInt32*          aFontID)
{
  aDimensions.Clear();

  if (!mFontMetrics)
    return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsOS2* metrics = (nsFontMetricsOS2*)mFontMetrics;
  GetTextDimensionsData data = {mPS, mP2T, mSurface, mCurrFontOS2, 0, 0, 0};

  metrics->ResolveForwards(mPS, aString, aLength, do_GetTextDimensions, &data);
  aDimensions.width = NSToCoordRound(float(data.mWidth) * mP2T);
  aDimensions.ascent = data.mAscent;
  aDimensions.descent = data.mDescent;

  if (data.mFont != mCurrFontOS2) {
    // If the font was changed along the way, restore our font
    mSurface->SelectFont(mCurrFontOS2);
  }

  if (aFontID) *aFontID = 0;

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString(const char *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  const nscoord* aSpacing)
{
  NS_PRECONDITION(mFontMetrics,"Something is wrong somewhere");

  PRInt32 x = aX;
  PRInt32 y = aY;

  SetupFontAndColor();

  INT dxMem[500];
  INT* dx0 = NULL;
  if (aSpacing) {
    dx0 = dxMem;
    if (aLength > 500) {
      dx0 = new INT[aLength];
    }
    mTranMatrix->ScaleXCoords(aSpacing, aLength, dx0);
  }
  mTranMatrix->TransformCoord(&x, &y);

  mCurrFontOS2->DrawString(mPS, mSurface, x, y, aString, aLength, dx0);

  if (dx0 && (dx0 != dxMem)) {
    delete [] dx0;
  }

  return NS_OK;
}

struct DrawStringData {
  HPS                   mPS;        // IN
  nsDrawingSurfaceOS2*  mSurface;   // IN
  nsFontOS2*            mFont;      // IN/OUT (running)
  nsTransform2D*        mTranMatrix;// IN
  nscoord               mX;         // IN/OUT (running)
  nscoord               mY;         // IN
  const nscoord*        mSpacing;   // IN
  nscoord               mMaxLength; // IN (length of the full string)
  nscoord               mLength;    // IN/OUT (running, current length already rendered)
};

static PRBool PR_CALLBACK
do_DrawString(const nsFontSwitch* aFontSwitch,
              const PRUnichar*    aSubstring,
              PRUint32            aSubstringLength,
              void*               aData)
{
  nsFontOS2* font = aFontSwitch->mFont;

  PRInt32 x, y;
  DrawStringData* data = (DrawStringData*)aData;
  if (data->mFont != font) {
    // the desired font is not the current font in the PS
    data->mFont = font;
    data->mSurface->SelectFont(data->mFont);
  }

  data->mLength += aSubstringLength;
  if (data->mSpacing) {
    // XXX Fix path to use a twips transform in the DC and use the
    // spacing values directly and let windows deal with the sub-pixel
    // positioning.

    // Slow, but accurate rendering
    const PRUnichar* str = aSubstring;
    const PRUnichar* end = aSubstring + aSubstringLength;
    while (str < end) {
      // XXX can shave some cycles by inlining a version of transform
      // coord where y is constant and transformed once
      x = data->mX;
      y = data->mY;
      data->mTranMatrix->TransformCoord(&x, &y);
      if (IS_HIGH_SURROGATE(*str) && 
          ((str+1)<end) && 
          IS_LOW_SURROGATE(*(str+1))) 
      {
        // special case for surrogate pair
        font->DrawString(data->mPS, data->mSurface, x, y, str, 2);
        // we need to advance data->mX and str twice
        data->mX += *data->mSpacing++;
        ++str;
      } else {
        font->DrawString(data->mPS, data->mSurface, x, y, str, 1);
      }
      data->mX += *data->mSpacing++;
      ++str;
    }
  }
  else {
    font->DrawString(data->mPS, data->mSurface, data->mX, data->mY, aSubstring,
                     aSubstringLength);
    // be ready if there is more to come
    if (data->mLength < data->mMaxLength) {
      data->mX += font->GetWidth(data->mPS, aSubstring, aSubstringLength);
    }
  }
  return PR_TRUE; // don't stop till the end
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  if (!mFontMetrics)
    return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsOS2* metrics = (nsFontMetricsOS2*)mFontMetrics;
  DrawStringData data = {mPS, mSurface, mCurrFontOS2, mTranMatrix, aX, aY,
                         aSpacing, aLength, 0};
  if (!aSpacing) { // @see do_DrawString for the spacing case
    mTranMatrix->TransformCoord(&data.mX, &data.mY);
  }

  if (mRightToLeftText) {
    metrics->ResolveBackwards(mPS, aString, aLength, do_DrawString, &data);
  }
  else
  {
    metrics->ResolveForwards(mPS, aString, aLength, do_DrawString, &data);
  }

  if (data.mFont != mCurrFontOS2) {
    // If the font was changed along the way, restore our font
    mSurface->SelectFont(mCurrFontOS2);
  }

  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2 :: DrawString(const nsString& aString,
                                                  nscoord aX, nscoord aY,
                                                  PRInt32 aFontID,
                                                  const nscoord* aSpacing)
{
  return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}

#ifdef MOZ_MATHML
NS_IMETHODIMP 
nsRenderingContextOS2::GetBoundingMetrics(const char*        aString,
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics)
{
#if 0
  NS_PRECONDITION(mFontMetrics,"Something is wrong somewhere");

  aBoundingMetrics.Clear();
  if (!mFontMetrics) return NS_ERROR_FAILURE;

  SetupFontAndColor();

  // set glyph transform matrix to identity
  MAT2 mat2;
  FIXED zero, one;
  zero.fract = 0; one.fract = 0;
  zero.value = 0; one.value = 1; 
  mat2.eM12 = mat2.eM21 = zero; 
  mat2.eM11 = mat2.eM22 = one; 

  // measure the string
  nscoord descent;
  GLYPHMETRICS gm;
  DWORD len = GetGlyphOutline(mDC, aString[0], GGO_METRICS, &gm, 0, nsnull, &mat2);
  if (GDI_ERROR == len) {
    return NS_ERROR_UNEXPECTED;
  }
  // flip sign of descent for cross-platform compatibility
  descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
  aBoundingMetrics.leftBearing = gm.gmptGlyphOrigin.x;
  aBoundingMetrics.rightBearing = gm.gmptGlyphOrigin.x + gm.gmBlackBoxX;
  aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
  aBoundingMetrics.descent = gm.gmptGlyphOrigin.y - gm.gmBlackBoxY;
  aBoundingMetrics.width = gm.gmCellIncX;

  if (1 < aLength) {
    // loop over each glyph to get the ascent and descent
    PRUint32 i;
    for (i = 1; i < aLength; i++) {
      len = GetGlyphOutline(mDC, aString[i], GGO_METRICS, &gm, 0, nsnull, &mat2);
      if (GDI_ERROR == len) {
        return NS_ERROR_UNEXPECTED;
      }
      // flip sign of descent for cross-platform compatibility
      descent = -(nscoord(gm.gmptGlyphOrigin.y) - nscoord(gm.gmBlackBoxY));
      if (aBoundingMetrics.ascent < gm.gmptGlyphOrigin.y)
        aBoundingMetrics.ascent = gm.gmptGlyphOrigin.y;
      if (aBoundingMetrics.descent > descent)
        aBoundingMetrics.descent = descent;
    }
    // get the final rightBearing and width. Possible kerning is taken into account.
    SIZE size;
    ::GetTextExtentPoint32(mDC, aString, aLength, &size);
    aBoundingMetrics.width = size.cx;
    aBoundingMetrics.rightBearing = size.cx - gm.gmCellIncX + gm.gmBlackBoxX;
  }

  // convert to app units
  aBoundingMetrics.leftBearing = NSToCoordRound(float(aBoundingMetrics.leftBearing) * mP2T);
  aBoundingMetrics.rightBearing = NSToCoordRound(float(aBoundingMetrics.rightBearing) * mP2T);
  aBoundingMetrics.width = NSToCoordRound(float(aBoundingMetrics.width) * mP2T);
  aBoundingMetrics.ascent = NSToCoordRound(float(aBoundingMetrics.ascent) * mP2T);
  aBoundingMetrics.descent = NSToCoordRound(float(aBoundingMetrics.descent) * mP2T);

  return NS_OK;
#endif
  return NS_ERROR_FAILURE;
}

#if 0
struct GetBoundingMetricsData {
  HDC                mDC;              // IN
  HFONT              mFont;            // IN/OUT (running)
  nsBoundingMetrics* mBoundingMetrics; // IN/OUT (running)
  PRBool             mFirstTime;       // IN/OUT (set once)
  nsresult           mStatus;          // OUT
};

static PRBool PR_CALLBACK
do_GetBoundingMetrics(const nsFontSwitch* aFontSwitch,
                      const PRUnichar*    aSubstring,
                      PRUint32            aSubstringLength,
                      void*               aData)
{
  nsFontWin* fontWin = aFontSwitch->mFontWin;

  GetBoundingMetricsData* data = (GetBoundingMetricsData*)aData;
  if (data->mFont != fontWin->mFont) {
    data->mFont = fontWin->mFont;
    ::SelectObject(data->mDC, data->mFont);
  }

  nsBoundingMetrics rawbm;
  data->mStatus = fontWin->GetBoundingMetrics(data->mDC, aSubstring, aSubstringLength, rawbm);
  if (NS_FAILED(data->mStatus)) {
    return PR_FALSE; // stop now
  }

  if (data->mFirstTime) {
    data->mFirstTime = PR_FALSE;
    *data->mBoundingMetrics = rawbm;
  }
  else {
    *data->mBoundingMetrics += rawbm;
  }

  return PR_TRUE; // don't stop till the end
}
#endif

NS_IMETHODIMP
nsRenderingContextOS2::GetBoundingMetrics(const PRUnichar*   aString,
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics,
                                          PRInt32*           aFontID)
{
#if 0
  aBoundingMetrics.Clear();
  if (!mFontMetrics) return NS_ERROR_FAILURE;

  SetupFontAndColor();

  nsFontMetricsWin* metrics = (nsFontMetricsWin*)mFontMetrics;
  GetBoundingMetricsData data = {mDC, mCurrFont, &aBoundingMetrics, PR_TRUE, NS_OK};

  nsresult rv = metrics->ResolveForwards(mDC, aString, aLength, do_GetBoundingMetrics, &data);
  if (NS_SUCCEEDED(rv)) {
    rv = data.mStatus;
  }

  // convert to app units
  aBoundingMetrics.leftBearing = NSToCoordRound(float(aBoundingMetrics.leftBearing) * mP2T);
  aBoundingMetrics.rightBearing = NSToCoordRound(float(aBoundingMetrics.rightBearing) * mP2T);
  aBoundingMetrics.width = NSToCoordRound(float(aBoundingMetrics.width) * mP2T);
  aBoundingMetrics.ascent = NSToCoordRound(float(aBoundingMetrics.ascent) * mP2T);
  aBoundingMetrics.descent = NSToCoordRound(float(aBoundingMetrics.descent) * mP2T);

  if (mCurrFont != data.mFont) {
    // If the font was changed along the way, restore our font
    ::SelectObject(mDC, mCurrFont);
  }

  if (aFontID) *aFontID = 0;

  return rv;
#endif
  return NS_ERROR_FAILURE;
}
#endif // MOZ_MATHML

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
      DestPS   = mMainSurface->GetPS();
   }

   PRUint32 DestHeight   = DestSurf->GetHeight();
   PRUint32 SourceHeight = SourceSurf->GetHeight();


   if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
   {
      HRGN SourceClipRegion = OS2_CopyClipRegion(SourceSurf->GetPS());

      // Currently clip region is in offscreen drawing surface coordinate system.
      // If offscreen surface have different height than destination surface,
      // then clipping region should be shifted down by amount of this difference.

      if ( (SourceClipRegion) && (SourceHeight != DestHeight) )
      {
         POINTL Offset = { 0, -(SourceHeight - DestHeight) };

         GFX (::GpiOffsetRegion(DestPS, SourceClipRegion, &Offset), FALSE);
      }

      OS2_SetClipRegion(DestPS, SourceClipRegion);
   }


   nsRect drect(aDestBounds);

   if( aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
      mTranMatrix->TransformCoord( &aSrcX, &aSrcY);

   if( aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
      mTranMatrix->TransformCoord( &drect.x, &drect.y,
                               &drect.width, &drect.height);

   // Note rects for GpiBitBlt are in-ex.

   POINTL Points [3] = { {drect.x, DestHeight - drect.y - drect.height},    // TLL
                         {drect.x + drect.width, DestHeight - drect.y},     // TUR
                         {aSrcX, SourceHeight - aSrcY - drect.height} };    // SLL


   GFX (::GpiBitBlt(DestPS, SourceSurf->GetPS(), 3, Points, ROP_SRCCOPY, BBO_OR), GPI_ERROR);

   return NS_OK;
}

NS_IMETHODIMP nsRenderingContextOS2::RetrieveCurrentNativeGraphicData(PRUint32* ngd)
{
  if(ngd != nsnull)
    *ngd = (PRUint32)mPS;
  return NS_OK;
}

void nsRenderingContextOS2::SetupFontAndColor(void)
{
  if (mFontMetrics) {
    // select font
    nsFontHandle fontHandle;
    mFontMetrics->GetFontHandle(fontHandle);
    if (!mCurrFontOS2 || mCurrFontOS2 != fontHandle) {
      mCurrFontOS2 = (nsFontOS2*)fontHandle;
      mSurface->SelectFont(mCurrFontOS2);
    }
  }

  // XXX - should we be always setting the color?
  CHARBUNDLE cBundle;
  cBundle.lColor = GetGPIColor();
  cBundle.usMixMode = FM_OVERPAINT;
  cBundle.usBackMixMode = BM_LEAVEALONE;

  GFX (::GpiSetAttrs(mPS, PRIM_CHAR,
                     CBB_COLOR | CBB_MIX_MODE | CBB_BACK_MIX_MODE,
                     0, &cBundle),
       FALSE);
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
      // Attempt to fix region leak
      if( tstate->mClipRegion )
      {
         GpiDestroyRegion( mPS, tstate->mClipRegion );
      }

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
