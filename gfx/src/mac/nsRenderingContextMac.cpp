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
 
/* 
 String

translated to a number via  getfnum()

TextFont(number)
TextSize
TextFace  -- may not play well with new style  
DrawString();  DrawText for cstrings
*/

#include "nsRenderingContextMac.h"
#include "nsDeviceContextMac.h"
#include "nsFontMetricsMac.h"

#include <math.h>
#include "nspr.h"
#include <QDOffscreen.h>
#include <windows.h>
#include "nsRegionMac.h"
#include "nsGfxCIID.h"
#include <Fonts.h>

//#define NO_CLIP


class GraphicsState
{
public:
  GraphicsState();
  ~GraphicsState();

  nscolor               mColor; // Current Color
  
  nsTransform2D       * mTMatrix; // transform that all the graphics drawn here will obey
  nsDrawingSurfaceMac   mRenderingSurface; // main drawing surface,Can be a BackBuffer if Selected in
  
  RgnHandle			    mClipRegion;
  
  nsRect				mLocalClip;
  RgnHandle				mMainRegion;
  PRInt32               mCurrFontHandle;
  PRInt32               mOffx;
  PRInt32               mOffy;
  
  GrafPtr		      * mCurPort;		// port set up on the stack
  
  nsIFontMetrics      * mFontMetrics;
  PRInt32               mFont;
  
  // Mac specific state info.
  RgnHandle       mMacOriginRelativeClipRgn;
  RgnHandle       mMacPortRelativeClipRgn;
  
  PRInt32         mMacPortRelativeX;
  PRInt32         mMacPortRelativeY;
};

//------------------------------------------------------------------------

GraphicsState :: GraphicsState()
{
  mColor = NS_RGB(0, 0, 0);
  mTMatrix = nsnull;  
  mRenderingSurface = nsnull;
  mClipRegion = nsnull;
  mLocalClip.x = mLocalClip.y = mLocalClip.width = mLocalClip.height = 0;
  mMainRegion = nsnull;
  mCurrFontHandle = 0;
  mOffx = 0;
  mOffy = 0;
  
  mCurPort = nsnull;
  
  mFontMetrics = nsnull;
  mFont = 0;
  
  // Mac specific state info.
  mMacOriginRelativeClipRgn = nsnull;
  mMacPortRelativeClipRgn = nsnull; 
  
  mMacPortRelativeX = 0;
  mMacPortRelativeY = 0;
}

//------------------------------------------------------------------------

GraphicsState :: ~GraphicsState()
{
	mFont = 0;
}

static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

//------------------------------------------------------------------------

nsRenderingContextMac :: nsRenderingContextMac()
{
  NS_INIT_REFCNT();

  mP2T = 1.0f;
  mContext = nsnull ;
  mOriginalSurface = nsnull;
  mFrontBuffer = nsnull;
  
  mCurStatePtr = new GraphicsState();
  
  mCurStatePtr->mFontMetrics = nsnull ;

  mCurStatePtr->mRenderingSurface = nsnull ; 
       
  mCurStatePtr->mColor = NS_RGB(255,255,255);
  mCurStatePtr->mTMatrix = new nsTransform2D();
  
  mCurStatePtr->mClipRegion = nsnull;
  mCurStatePtr->mCurrFontHandle = 0;
  mCurStatePtr->mOffx = 0;
  mCurStatePtr->mOffy = 0;
  mCurStatePtr->mMainRegion = nsnull;
  
  // Mac Specific state
  mCurStatePtr->mMacOriginRelativeClipRgn = nsnull;
  mCurStatePtr->mMacPortRelativeClipRgn = nsnull;
  
  mCurStatePtr->mMacPortRelativeX = 0;
  mCurStatePtr->mMacPortRelativeY = 0;

  // Create a new state stack
  mStateCache = new nsVoidArray();
  
  // Add this as the first element of the state cache
  mStateCache->AppendElement( mCurStatePtr );
}

//------------------------------------------------------------------------

nsRenderingContextMac :: ~nsRenderingContextMac()
{

  if(mCurStatePtr->mRenderingSurface)
  {
  	::SetPort(mOriginalSurface);
  	::SetOrigin(0,0); // Setting to 0,0 doesn't really reset the state properly
  }

  if (mCurStatePtr->mClipRegion) 
  {
    ::DisposeRgn(mCurStatePtr->mClipRegion);
    mCurStatePtr->mClipRegion = nsnull;
  }

  if (mCurStatePtr->mMacOriginRelativeClipRgn) 
  {
    ::DisposeRgn( mCurStatePtr->mMacOriginRelativeClipRgn );
    mCurStatePtr->mMacOriginRelativeClipRgn = nsnull;
  }
  
  if ( mCurStatePtr->mMacPortRelativeClipRgn ) 
  {
    ::DisposeRgn( mCurStatePtr->mMacPortRelativeClipRgn );
    mCurStatePtr->mMacPortRelativeClipRgn = nsnull;
  }
 
  if ( mCurStatePtr->mMainRegion )
  {
    ::DisposeRgn( mCurStatePtr->mMainRegion );
    mCurStatePtr->mMainRegion = nsnull;
  }

  mCurStatePtr->mTMatrix = nsnull;

  NS_IF_RELEASE(mCurStatePtr->mFontMetrics);
  NS_IF_RELEASE(mContext);
  
  // Destroy the State Machine
  if (nsnull != mStateCache)
  	{
    PRInt32 cnt = mStateCache->Count();

    while (--cnt >= 0)
    	{
      GraphicsState *state = (GraphicsState *)mStateCache->ElementAt(cnt);
      mStateCache->RemoveElementAt(cnt);

      if (nsnull != state)
        delete state;
    	}
    delete mStateCache;
    mStateCache = nsnull;
  	}
}

NS_IMPL_QUERY_INTERFACE(nsRenderingContextMac, kRenderingContextIID);
NS_IMPL_ADDREF(nsRenderingContextMac);
NS_IMPL_RELEASE(nsRenderingContextMac);

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: Init(nsIDeviceContext* aContext,nsIWidget *aWindow)
{
PRInt32	offx,offy;

  if (nsnull == aWindow->GetNativeData(NS_NATIVE_WINDOW))
    return NS_ERROR_NOT_INITIALIZED;

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mCurStatePtr->mMacPortRelativeX = offx = (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETX);
  mCurStatePtr->mMacPortRelativeY = offy = (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETY);

  mCurStatePtr->mRenderingSurface = (nsDrawingSurfaceMac)aWindow->GetNativeData(NS_NATIVE_DISPLAY);
  mFrontBuffer = mCurStatePtr->mRenderingSurface;
  
  if(!mFrontBuffer)
    mFrontBuffer = nsnull;
  
  RgnHandle widgetRgn = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION);
  if (mCurStatePtr->mMainRegion == nsnull)
    mCurStatePtr->mMainRegion = ::NewRgn();
  if (mCurStatePtr->mMainRegion)
    ::CopyRgn(widgetRgn, mCurStatePtr->mMainRegion);

  // Setup mMacPortRelativeClipRgn and mMacOriginRelativeClipRgn
  widgetRgn = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION_IN_PORT);
  
  if (mCurStatePtr->mMacOriginRelativeClipRgn == nsnull)
    mCurStatePtr->mMacOriginRelativeClipRgn = ::NewRgn();
  if (mCurStatePtr->mMacOriginRelativeClipRgn)
    ::CopyRgn(widgetRgn, mCurStatePtr->mMacOriginRelativeClipRgn);
    
  if (mCurStatePtr->mMacPortRelativeClipRgn == nsnull)
    mCurStatePtr->mMacPortRelativeClipRgn = ::NewRgn();
  if (mCurStatePtr->mMacPortRelativeClipRgn)
    ::CopyRgn(widgetRgn, mCurStatePtr->mMacPortRelativeClipRgn); 
  
  mOriginalSurface = mCurStatePtr->mRenderingSurface;    // we need to know when we set back
  ::SetPort(mCurStatePtr->mRenderingSurface);
  ::SetOrigin(-offx,-offy);
  //if(((**mMacOriginRelativeClipRgn).rgnBBox.left - offx) < 0)
  //{
  // 	offx = 0;
  //}
  //if(((**mMacOriginRelativeClipRgn).rgnBBox.top - offy) < 0)
  //{
  //	offy = 0;
  //}
  ::OffsetRgn(mCurStatePtr->mMainRegion, -offx, -offy);
  //::OffsetRgn(mMainRegion, offx, offy);
  
  ::OffsetRgn(mCurStatePtr->mMacOriginRelativeClipRgn, -offx, -offy);
  //::OffsetRgn(mMacOriginRelativeClipRgn, offx, offy);

  mCurStatePtr->mOffx = -offx;
  mCurStatePtr->mOffy = -offy;

  // TODO - cps - Push the state to overcome design bugs
  //PushState();

  return (CommonInit());
}

//------------------------------------------------------------------------


// this drawing surface init should only be called for an offscreen drawing surface, without and offset or clip region
NS_IMETHODIMP nsRenderingContextMac :: Init(nsIDeviceContext* aContext,
					nsDrawingSurface aSurface)
{

  mContext = aContext;
  NS_IF_ADDREF(mContext);

  mCurStatePtr->mRenderingSurface = (nsDrawingSurfaceMac) aSurface;
  
  return (CommonInit());
}

//------------------------------------------------------------------------


NS_IMETHODIMP nsRenderingContextMac :: CommonInit()
{
  //((nsDeviceContextMac *)mContext)->SetDrawingSurface(mRenderingSurface);
  ((nsDeviceContextMac *)mContext)->SetDrawingSurface(mCurStatePtr->mRenderingSurface);
  //((nsDeviceContextMac *)mContext)->InstallColormap();

  mContext->GetDevUnitsToAppUnits(mP2T);
  float app2dev;
  mContext->GetAppUnitsToDevUnits(app2dev);
  if(mCurStatePtr->mTMatrix)
  	mCurStatePtr->mTMatrix->AddScale(app2dev, app2dev);
  	
  SetColor(mCurStatePtr->mColor);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{  
  if (nsnull == aSurface)
    mCurStatePtr->mRenderingSurface = mFrontBuffer; // Set to screen port
  else
  { 
  	// cps - This isn't as simple as just setting the surface... all the 
  	// surface relative parameters must be set as well. This means a 
  	// rendering context push.
  	
  	// This push is balaced by a pop in CopyOffScreenBits. This is a hack.
  	PushState();
  	
  	mCurStatePtr->mRenderingSurface = (nsDrawingSurfaceMac) aSurface;
  	
	::SetOrigin(0,0);
	
	::SetRectRgn( mCurStatePtr->mMacPortRelativeClipRgn,
		((CGrafPtr)aSurface)->portRect.left,
		((CGrafPtr)aSurface)->portRect.top,
		((CGrafPtr)aSurface)->portRect.right,
		((CGrafPtr)aSurface)->portRect.bottom );
	
	::SetRectRgn( mCurStatePtr->mMacOriginRelativeClipRgn, 
		0,
		0,
		((CGrafPtr)aSurface)->portRect.right - ((CGrafPtr)aSurface)->portRect.left,
		((CGrafPtr)aSurface)->portRect.bottom - ((CGrafPtr)aSurface)->portRect.top );
	
	mCurStatePtr->mMacPortRelativeX = 0;
	mCurStatePtr->mMacPortRelativeY = 0;
  }

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: Reset(void)
{
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetDeviceContext(nsIDeviceContext *&aContext)
{
  NS_IF_ADDREF(mContext);
  aContext = mContext;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: PushState(void)
{
  nsRect 	rect;
  Rect		mac_rect;

  GraphicsState * state = new GraphicsState();

  // Push into this state object, add to vector
  state->mTMatrix = mCurStatePtr->mTMatrix;
  if (nsnull == mCurStatePtr->mTMatrix)
    mCurStatePtr->mTMatrix = new nsTransform2D();
  else
    mCurStatePtr->mTMatrix = new nsTransform2D(mCurStatePtr->mTMatrix);

  mStateCache->AppendElement(state);

  state->mColor = mCurStatePtr->mColor;

  state->mRenderingSurface = mCurStatePtr->mRenderingSurface;

  PRBool clipState;
  GetClipRect(state->mLocalClip, clipState);

  state->mClipRegion = mCurStatePtr->mClipRegion;
  if (nsnull != state->mClipRegion) 
  	{
    mCurStatePtr->mClipRegion = ::NewRgn();
    
    mac_rect.left = state->mLocalClip.x;
    mac_rect.top = state->mLocalClip.y;
    mac_rect.right = state->mLocalClip.x + state->mLocalClip.width;
    mac_rect.bottom = state->mLocalClip.y + state->mLocalClip.height;
    
    ::SetRectRgn (
      mCurStatePtr->mClipRegion,
      state->mLocalClip.x,
      state->mLocalClip.y,
      state->mLocalClip.x + state->mLocalClip.width,
      state->mLocalClip.y + state->mLocalClip.height );
  	}
  	
  state->mMainRegion = mCurStatePtr->mMainRegion;
  if(nsnull != state->mMainRegion)
  {
    mCurStatePtr->mMainRegion = ::NewRgn();
    ::CopyRgn(state->mMainRegion, mCurStatePtr->mMainRegion);
  }
  
  state->mCurrFontHandle = mCurStatePtr->mCurrFontHandle;

  state->mOffx = mCurStatePtr->mOffx;
  state->mOffy = mCurStatePtr->mOffy;
  
  state->mCurPort = mCurStatePtr->mCurPort;
  state->mFontMetrics = mCurStatePtr->mFontMetrics;
  state->mFont = mCurStatePtr->mFont;
  
  // Make a new copy of the mMacPortRelativeClipRgn if there is one
  state->mMacPortRelativeClipRgn = mCurStatePtr->mMacPortRelativeClipRgn;
  if (nsnull != state->mMacPortRelativeClipRgn) 
    {
    mCurStatePtr->mMacPortRelativeClipRgn = ::NewRgn();
    
	::CopyRgn (state->mMacPortRelativeClipRgn, mCurStatePtr->mMacPortRelativeClipRgn );
  	}
  	
  // Make a new copy of the mMacOriginRelativeClipRgn if there is one
  state->mMacOriginRelativeClipRgn = mCurStatePtr->mMacOriginRelativeClipRgn;
  if (nsnull != state->mMacOriginRelativeClipRgn) 
    {
    mCurStatePtr->mMacOriginRelativeClipRgn = ::NewRgn();
    
	::CopyRgn (state->mMacOriginRelativeClipRgn, mCurStatePtr->mMacOriginRelativeClipRgn );
  	}
  	
  state->mMacPortRelativeX = mCurStatePtr->mMacPortRelativeX;
  state->mMacPortRelativeY = mCurStatePtr->mMacPortRelativeY;
  
  // Set the CurStatePtr to the top state
  mCurStatePtr = state;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: PopState(PRBool &aClipEmpty)
{
  PRBool bEmpty = PR_FALSE;

  PRUint32 cnt = mStateCache->Count();
  GraphicsState * state;

  if (cnt > 1) 
  {
    state = (GraphicsState *)mStateCache->ElementAt(cnt - 2);
    mStateCache->RemoveElementAt(cnt - 1);

    // cps- Instead of doing this BS I should just be calling delete mCurStatePtr
    
    if (mCurStatePtr->mTMatrix)
      delete mCurStatePtr->mTMatrix;

    if (nsnull != mCurStatePtr->mClipRegion)
      ::DisposeRgn(mCurStatePtr->mClipRegion);

    if (nsnull != mCurStatePtr->mMacOriginRelativeClipRgn)
      ::DisposeRgn(mCurStatePtr->mMacOriginRelativeClipRgn);
    
    if( mCurStatePtr->mMacPortRelativeClipRgn )
      ::DisposeRgn( mCurStatePtr->mMacPortRelativeClipRgn );
    
    if (state->mColor != mCurStatePtr->mColor)
      SetColor(state->mColor);
    
    // Delete this graphics state object
    delete mCurStatePtr;
    
    // Set the current state pointer to top of stack
    mCurStatePtr = state;
    
    // Set the appropriate states to their old values
    // Cannot use the region alone as ::SetRectRgn sets only logical regions, ie, area greater than 0
    ::SetOrigin( 
      -mCurStatePtr->mMacPortRelativeX, 
      -mCurStatePtr->mMacPortRelativeY );
    	
    ::SetClip( mCurStatePtr->mMacOriginRelativeClipRgn );
    
    if (nsnull != mCurStatePtr->mMacOriginRelativeClipRgn && ::EmptyRgn(mCurStatePtr->mMacOriginRelativeClipRgn) == PR_TRUE)
      bEmpty = PR_TRUE;
  	}

  aClipEmpty = bEmpty;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
  aVisible = PR_TRUE;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetClipRectInPixels(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  PRBool 		bEmpty = PR_FALSE;
  nsRect  	trect = aRect;
  RgnHandle	theregion,tregion;

  theregion = ::NewRgn();
  SetRectRgn(theregion,trect.x,trect.y,trect.x+trect.width,trect.y+trect.height);

  if (aCombine == nsClipCombine_kIntersect)
  	{
    if (nsnull != mCurStatePtr->mClipRegion) 
    {
      tregion = ::NewRgn();
      ::SectRgn(theregion,mCurStatePtr->mClipRegion,tregion);
      ::DisposeRgn(theregion);
      ::DisposeRgn(mCurStatePtr->mClipRegion);
      mCurStatePtr->mClipRegion = tregion;
    } 
    
    //if (nsnull != mMacOriginRelativeClipRgn) 
    //{
    //  tregion = ::NewRgn();
    //  ::SectRgn(theregion,mMacOriginRelativeClipRgn,tregion);
    //  ::DisposeRgn(theregion);
    //  ::DisposeRgn(mMacOriginRelativeClipRgn);
    //  mMacOriginRelativeClipRgn = tregion;
    //} 
  }
  else if (aCombine == nsClipCombine_kUnion)
  {
    if (nsnull != mCurStatePtr->mClipRegion) 
	{
	  tregion = ::NewRgn();
	  ::UnionRgn(theregion, mCurStatePtr->mClipRegion, tregion);
	  ::DisposeRgn(mCurStatePtr->mClipRegion);
	  ::DisposeRgn(theregion);
	  mCurStatePtr->mClipRegion = tregion;
	} 
	
    //if (nsnull != mMacOriginRelativeClipRgn) 
	//{
	//  tregion = ::NewRgn();
	//  ::UnionRgn(theregion, mMacOriginRelativeClipRgn, tregion);
	//  ::DisposeRgn(mMacOriginRelativeClipRgn);
	//  ::DisposeRgn(theregion);
	//  mMacOriginRelativeClipRgn = tregion;
	//} 
  }
  else if (aCombine == nsClipCombine_kSubtract)
  {
    if (nsnull != mCurStatePtr->mClipRegion) 
    {
      tregion = ::NewRgn();
	  ::DiffRgn(mCurStatePtr->mClipRegion, theregion, tregion);
      ::DisposeRgn(mCurStatePtr->mClipRegion);
      ::DisposeRgn(theregion);
      mCurStatePtr->mClipRegion = tregion;
    } 
    
    //if (nsnull != mMacOriginRelativeClipRgn) 
    //{
    //  tregion = ::NewRgn();
	//  ::DiffRgn(mMacOriginRelativeClipRgn, theregion, tregion);
    //  ::DisposeRgn(mMacOriginRelativeClipRgn);
    //  ::DisposeRgn(theregion);
    //  mMacOriginRelativeClipRgn = tregion;
    //} 
  }
  else if (aCombine == nsClipCombine_kReplace)
  {
    if (nsnull != mCurStatePtr->mClipRegion)
      ::DisposeRgn(mCurStatePtr->mClipRegion);

    mCurStatePtr->mClipRegion = theregion;
    
    
    //if (nsnull != mMacOriginRelativeClipRgn)
    //  ::DisposeRgn(mMacOriginRelativeClipRgn);

    //mMacOriginRelativeClipRgn = theregion;
  			}
  		else
    		NS_ASSERTION(PR_FALSE, "illegal clip combination");
  /*
  if (nsnull == mClipRegion)
  	{
    bEmpty = PR_TRUE;
    // clip to the size of the window
    GrafPtr oldPort = NULL;
 	::GetPort( & oldPort );
    ::SetPort(mRenderingSurface);
    ::ClipRect(&mRenderingSurface->portRect);
    ::SetPort( oldPort );
  } 
  else 
  {
    // set the clipping area of this windowptr
 	GrafPtr oldPort = NULL;
 	::GetPort( & oldPort );
    ::SetPort(mRenderingSurface);
    ::SetClip(mClipRegion);
    ::SetPort( oldPort );
  }
  */
  if (nsnull == mCurStatePtr->mMacOriginRelativeClipRgn) 
  {
    bEmpty = PR_TRUE;
    // clip to the size of the window
    GrafPtr oldPort = NULL;
 	::GetPort( & oldPort );
    ::SetPort(mCurStatePtr->mRenderingSurface);
    ::ClipRect(&mCurStatePtr->mRenderingSurface->portRect);
    ::SetPort( oldPort );
  } 
  else 
  {
    // set the clipping area of this windowptr
 	GrafPtr oldPort = NULL;
 	::GetPort( & oldPort );
    ::SetPort(mCurStatePtr->mRenderingSurface);
    ::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
    ::SetPort( oldPort );
  	}

  aClipEmpty = bEmpty;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  nsRect  trect = aRect;

  mCurStatePtr->mTMatrix->TransformCoord(&trect.x, &trect.y,&trect.width, &trect.height);

  return SetClipRectInPixels(trect, aCombine, aClipEmpty);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  Rect	cliprect;

  if (mCurStatePtr->mClipRegion != nsnull) 
  {
  	cliprect = (**mCurStatePtr->mClipRegion).rgnBBox;
    aRect.SetRect(cliprect.left, cliprect.top, cliprect.right-cliprect.left, cliprect.bottom-cliprect.top);
    aClipValid = PR_TRUE;
 	} 
 	else 
	{
    aRect.SetRect(0,0,0,0);
    aClipValid = PR_FALSE;
 	}

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  nsRect    rect;
  Rect	    mrect;
  RgnHandle	mregion;

  nsRegionMac *pRegion = (nsRegionMac *)&aRegion;
  
  mregion = pRegion->GetRegion();
  mrect = (**mregion).rgnBBox;
  
  rect.x = mrect.left;
  rect.y = mrect.top;
  rect.width = mrect.right-mrect.left;
  rect.height = mrect.bottom-mrect.top;

  SetClipRectInPixels(rect, aCombine, aClipEmpty);

  if (::EmptyRgn(mCurStatePtr->mClipRegion) == PR_TRUE)
    aClipEmpty = PR_TRUE;
  else
    aClipEmpty = PR_FALSE;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetClipRegion(nsIRegion **aRegion)
{
  nsIRegion * pRegion;

  static NS_DEFINE_IID(kCRegionCID, NS_REGION_CID);
  static NS_DEFINE_IID(kIRegionIID, NS_IREGION_IID);

  nsresult rv = nsRepository::CreateInstance(kCRegionCID,nsnull,  kIRegionIID, (void **)aRegion);

  if (NS_OK == rv)
 	{
    nsRect rect;
    PRBool clipState;
    pRegion = (nsIRegion *)&aRegion;
    pRegion->Init();
    GetClipRect(rect, clipState);
    pRegion->Union(rect.x,rect.y,rect.width,rect.height);
 	}

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetColor(nscolor aColor)
{
  RGBColor	thecolor;
  GrafPtr	curport;

	#define COLOR8TOCOLOR16(color8)	 (color8 == 0xFF ? 0xFFFF : (color8 << 8))

	::GetPort(&curport);
	::SetPort(mCurStatePtr->mRenderingSurface);
	thecolor.red = COLOR8TOCOLOR16(NS_GET_R(aColor));
	thecolor.green = COLOR8TOCOLOR16(NS_GET_G(aColor));
	thecolor.blue = COLOR8TOCOLOR16(NS_GET_B(aColor));
	::RGBForeColor(&thecolor);
  mCurStatePtr->mColor = aColor ;
  ::SetPort(curport);
 
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetColor(nscolor &aColor) const
{
  aColor = mCurStatePtr->mColor;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetLineStyle(nsLineStyle aLineStyle)
{
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetLineStyle(nsLineStyle &aLineStyle)
{
  return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetFont(const nsFont& aFont)
{
	NS_IF_RELEASE(mCurStatePtr->mFontMetrics);

	if (mContext)
		mContext->GetMetricsFor(aFont, mCurStatePtr->mFontMetrics);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetFont(nsIFontMetrics *aFontMetrics)
{
	NS_IF_RELEASE(mCurStatePtr->mFontMetrics);
	mCurStatePtr->mFontMetrics = aFontMetrics;
	NS_IF_ADDREF(mCurStatePtr->mFontMetrics);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  NS_IF_ADDREF(mCurStatePtr->mFontMetrics);
  aFontMetrics = mCurStatePtr->mFontMetrics;
  return NS_OK;
}

//------------------------------------------------------------------------

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextMac :: Translate(nscoord aX, nscoord aY)
{
  mCurStatePtr->mTMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

//------------------------------------------------------------------------

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextMac :: Scale(float aSx, float aSy)
{
  mCurStatePtr->mTMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mCurStatePtr->mTMatrix;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  PRUint32	depth;
  GWorldPtr	theoff;
  Rect      bounds;
  QDErr     myerr;

  // Must make sure this code never gets called when nsRenderingSurface is nsnull
  if(mContext)
  mContext->GetDepth(depth);

  if(aBounds!=nsnull)
  {
  	::SetRect(
  		&bounds,
  		0,
  		0,
  		aBounds->width,
  		aBounds->height);
  }
  else
  	::SetRect(&bounds,0,0,2,2);

  myerr = ::NewGWorld(&theoff,depth,&bounds,0,0,0);
  ::LockPixels( ::GetGWorldPixMap(theoff) );
  
  aSurface = theoff;  

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DestroyDrawingSurface(nsDrawingSurface aDS)
{
  GWorldPtr	theoff;

	if(aDS)
	{
		theoff = (GWorldPtr)aDS;
		
		//if( theoff == mCurrentOffscreenRenderingSurface )
		//{
			::UnlockPixels( ::GetGWorldPixMap(theoff) );
		//	mCurrentOffscreenRenderingSurface == nsnull;
		//}
		
		//if(theoff == mPreviousOffscreenRenderingSurface)
		//    mPreviousOffscreenRenderingSurface = nsnull;
		
		::DisposeGWorld(theoff);
	}
	
	theoff = nsnull;
	mCurStatePtr->mRenderingSurface = mFrontBuffer;		// point back at the front surface
		
  //if (mRenderingSurface == (GrafPtr)theoff)
    //mRenderingSurface = nsnull;

  //DisposeRgn(mMainRegion);
  //mMainRegion = nsnull;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
  mCurStatePtr->mTMatrix->TransformCoord(&aX0,&aY0);
  mCurStatePtr->mTMatrix->TransformCoord(&aX1,&aY1);

  GrafPtr oldPort;
  ::GetPort(&oldPort);
  
	::SetPort(mCurStatePtr->mRenderingSurface);
	//::SetClip(mMainRegion);
	//::SetOrigin( 
    //	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
    //	-(**mMacPortRelativeClipRgn).rgnBBox.top );
    ::SetOrigin( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );
    	
	::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
	::MoveTo(aX0, aY0);
	::LineTo(aX1, aY1);

  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawRect(const nsRect& aRect)
{
  return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  nscoord x,y,w,h;
  Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  GrafPtr oldPort;
  ::GetPort(&oldPort);
	::SetPort(mCurStatePtr->mRenderingSurface);
	//::SetClip(mMainRegion);
	//::SetOrigin( 
    //	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
    //	-(**mMacPortRelativeClipRgn).rgnBBox.top );
    ::SetOrigin( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );
    	
	::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
	
    mCurStatePtr->mTMatrix->TransformCoord(&x,&y,&w,&h);
	::SetRect(&therect,x,y,x+w,y+h);
	::FrameRect(&therect);

  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillRect(const nsRect& aRect)
{
	return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  nscoord  x,y,w,h;
  Rect     therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  // TODO - cps - must debug and fix this 
  mCurStatePtr->mTMatrix->TransformCoord(&x,&y,&w,&h);

  GrafPtr oldPort;
  ::GetPort(&oldPort);
	::SetPort(mCurStatePtr->mRenderingSurface);
	//::SetClip(mMainRegion);
	//::SetOrigin( 
    //	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
    //	-(**mMacPortRelativeClipRgn).rgnBBox.top );
    ::SetOrigin( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );
    	
	::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
	::SetRect(&therect,x,y,x+w,y+h);
	::PaintRect(&therect);
  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PRUint32   i;
  PolyHandle thepoly;
  PRInt32    x,y;

  GrafPtr oldPort;
  ::GetPort(&oldPort);
  
  ::SetPort(mCurStatePtr->mRenderingSurface); 
  thepoly = ::OpenPoly();
	
  x = aPoints[0].x;
  y = aPoints[0].y;
  mCurStatePtr->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
  ::MoveTo(x,y);

  for (i = 1; i < aNumPoints; i++)
  {
    x = aPoints[i].x;
    y = aPoints[i].y;
		
	mCurStatePtr->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
	}

	ClosePoly();
	
	::FramePoly(thepoly);

  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
  PRUint32   i;
  PolyHandle thepoly;
  PRInt32    x,y;

  GrafPtr oldPort;
  ::GetPort(&oldPort);
  
  ::SetPort(mCurStatePtr->mRenderingSurface);
  //::SetClip(mMainRegion);
  //::SetOrigin( 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.top );
  ::SetOrigin( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );  
    	
  ::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
  thepoly = ::OpenPoly();
	
  x = aPoints[0].x;
  y = aPoints[0].y;
  mCurStatePtr->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
  ::MoveTo(x,y);

  for (i = 1; i < aNumPoints; i++)
  {
    x = aPoints[i].x;
    y = aPoints[i].y;
	mCurStatePtr->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
	}

	::ClosePoly();
	
	::PaintPoly(thepoly);

  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawEllipse(const nsRect& aRect)
{
  return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  nscoord x,y,w,h;
  Rect    therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  GrafPtr oldPort;
  ::GetPort(&oldPort);
  
  ::SetPort(mCurStatePtr->mRenderingSurface);
  //::SetClip(mMainRegion);
  //::SetOrigin( 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.top );
  ::SetOrigin ( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );  
    	
  ::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
  mCurStatePtr->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,x+h);
  ::FrameOval(&therect);

  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillEllipse(const nsRect& aRect)
{
  return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
nscoord x,y,w,h;
  Rect    therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  GrafPtr oldPort;
  ::GetPort(&oldPort);
  
  ::SetPort(mCurStatePtr->mRenderingSurface);
  //::SetClip(mMainRegion);
  //::SetOrigin( 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.top );
  ::SetOrigin( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );  
    	
  ::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
  mCurStatePtr->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,x+h);
  ::PaintOval(&therect);

  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  nscoord x,y,w,h;
  Rect    therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  GrafPtr oldPort;
  ::GetPort(&oldPort);
  
  ::SetPort(mCurStatePtr->mRenderingSurface);
  //::SetClip(mMainRegion);
  //::SetOrigin( 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.top );
  ::SetOrigin( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );  
    	
  ::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
  
  mCurStatePtr->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,x+h);
  ::FrameArc(&therect,aStartAngle,aEndAngle);

  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
  return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
  nscoord x,y,w,h;
  Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  GrafPtr oldPort;
  ::GetPort(&oldPort);
  
  ::SetPort(mCurStatePtr->mRenderingSurface);
  //::SetClip(mMainRegion);
  //::SetOrigin( 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.top );
  ::SetOrigin( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );  
    	
  ::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
  
  mCurStatePtr->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,x+h);
  ::PaintArc(&therect,aStartAngle,aEndAngle);

  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(char ch, nscoord &aWidth)
{
  char buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(PRUnichar ch, nscoord &aWidth)
{
  PRUnichar buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(const nsString& aString, nscoord &aWidth)
{
  return GetWidth(aString.GetUnicode(), aString.Length(), aWidth);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(const char *aString, nscoord &aWidth)
{
  return GetWidth(aString, strlen(aString), aWidth);
}

//------------------------------------------------------------------------

NS_IMETHODIMP
nsRenderingContextMac :: GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
	// set native font and attributes
  
  GrafPtr oldPort;
  ::GetPort(&oldPort);
  ::SetPort(mCurStatePtr->mRenderingSurface);
  
  nsFont *font;
  mCurStatePtr->mFontMetrics->GetFont(font);
	nsFontMetricsMac::SetFont(*font, mContext);

	// measure text
	short textWidth = ::TextWidth(aString, 0, aLength);
	aWidth = NSToCoordRound(float(textWidth) * mP2T);

	// add a bit for italic
	switch (font->style)
	{
		case NS_FONT_STYLE_ITALIC:
		case NS_FONT_STYLE_OBLIQUE:
			nscoord aAdvance;
	  mCurStatePtr->mFontMetrics->GetMaxAdvance(aAdvance);
			aWidth += aAdvance;
			break;
	}

  ::SetPort(oldPort);

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth)
{
	nsString nsStr;
	nsStr.SetString(aString, aLength);
	char* cStr = nsStr.ToNewCString();
  GetWidth(cStr, aLength, aWidth);
	delete[] cStr;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const char *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY,
                                         nscoord aWidth,
                                         const nscoord* aSpacing)
{
	PRInt32 x = aX;
	PRInt32 y = aY;

  GrafPtr oldPort;
  ::GetPort(&oldPort);
  
  ::SetPort(mCurStatePtr->mRenderingSurface);
  //::SetClip(mMainRegion);
  //::SetOrigin( 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
  //  	-(**mMacPortRelativeClipRgn).rgnBBox.top );
  ::SetOrigin( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );  
    	
  ::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);

  if (mCurStatePtr->mFontMetrics)
  {
	// set native font and attributes
    nsFont *font;
    mCurStatePtr->mFontMetrics->GetFont(font);
		nsFontMetricsMac::SetFont(*font, mContext);

		// substract ascent since drawing specifies baseline
		nscoord ascent = 0;
	mCurStatePtr->mFontMetrics->GetMaxAscent(ascent);
		y += ascent;
	}

  mCurStatePtr->mTMatrix->TransformCoord(&x,&y);

	::MoveTo(x,y);
	::DrawText(aString,0,aLength);

  if (mCurStatePtr->mFontMetrics)
	{
		const nsFont* font;
    mCurStatePtr->mFontMetrics->GetFont(font);
		PRUint8 deco = font->decorations;

		if (deco & NS_FONT_DECORATION_OVERLINE)
			DrawLine(aX, aY, aX + aWidth, aY);

		if (deco & NS_FONT_DECORATION_UNDERLINE)
		{
			nscoord ascent = 0;
			nscoord descent = 0;
	  mCurStatePtr->mFontMetrics->GetMaxAscent(ascent);
	  mCurStatePtr->mFontMetrics->GetMaxDescent(descent);

			DrawLine(aX, aY + ascent + (descent >> 1),
						aX + aWidth, aY + ascent + (descent >> 1));
		}

		if (deco & NS_FONT_DECORATION_LINE_THROUGH)
		{
			nscoord height = 0;
	  mCurStatePtr->mFontMetrics->GetHeight(height);

			DrawLine(aX, aY + (height >> 1), aX + aWidth, aY + (height >> 1));
		}
	}

  ::SetPort(oldPort);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, nscoord aWidth,
                                         const nscoord* aSpacing)
{
	nsString nsStr;
	nsStr.SetString(aString, aLength);
	char* cStr = nsStr.ToNewCString();

	nsresult rv = DrawString(cStr, aLength, aX, aY, aWidth,aSpacing);

	delete[] cStr;

  return rv;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const nsString& aString,
                                         nscoord aX, nscoord aY, nscoord aWidth,
                                         const nscoord* aSpacing)
{
	char* cStr = aString.ToNewCString();
	nsresult rv = DrawString(cStr, aString.Length(), aX, aY, aWidth, aSpacing);

	delete[] cStr;

  return rv;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
  nscoord width,height;

  width = NSToCoordRound(mP2T * aImage->GetWidth());
  height = NSToCoordRound(mP2T * aImage->GetHeight());
  
  return DrawImage(aImage,aX,aY,width,height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
                                        nscoord aWidth, nscoord aHeight) 
{
  nsRect	tr;

  tr.x = aX;
  tr.y = aY;
  tr.width = aWidth;
  tr.height = aHeight;

  return DrawImage(aImage,tr);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
  nsRect	sr,dr;
  
  sr = aSRect;
  mCurStatePtr->mTMatrix ->TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);

  dr = aDRect;
  mCurStatePtr->mTMatrix->TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);
  
  return aImage->Draw(*this,mCurStatePtr->mRenderingSurface,sr.x,sr.y,sr.width,sr.height,
                      dr.x,dr.y,dr.width,dr.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
  nsRect	tr;

  tr = aRect;
  mCurStatePtr->mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  
  if (aImage != nsnull) 
	{
    GrafPtr oldPort;
    ::GetPort(&oldPort);
    
	::SetPort(mCurStatePtr->mRenderingSurface);
	//::SetClip(mMainRegion);
	//::SetOrigin( 
    //	-(**mMacPortRelativeClipRgn).rgnBBox.left, 
    //	-(**mMacPortRelativeClipRgn).rgnBBox.top );
    ::SetOrigin( -mCurStatePtr->mMacPortRelativeX, -mCurStatePtr->mMacPortRelativeY );
    	
	::SetClip(mCurStatePtr->mMacOriginRelativeClipRgn);
    nsresult result = aImage->Draw(*this,mCurStatePtr->mRenderingSurface,tr.x,tr.y,tr.width,tr.height);
    ::SetPort(oldPort);
    return result;
 	} 
  else 
 	{
    printf("Image is NULL!\n");
    return NS_ERROR_FAILURE;
 	}
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
// cps - This API does not allow for source rect cropping.
{
  //PixMapPtr           srcpix;
  //PixMapPtr			  destpix;
  RGBColor			  rgbblack = {0x0000,0x0000,0x0000};
  RGBColor			  rgbwhite = {0xFFFF,0xFFFF,0xFFFF};
  Rect                srcrect,dstrect;
  PRInt32             x = aSrcX;
  PRInt32             y = aSrcY;
  nsRect              drect = aDestBounds;
  nsDrawingSurfaceMac destport;
  GrafPtr             savePort;
  RGBColor            saveForeColor;
  RGBColor            saveBackColor;

  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
  {
    destport = mCurStatePtr->mRenderingSurface;
    NS_ASSERTION(!(nsnull == destport), "no back buffer");
  }
  else
    destport = mFrontBuffer;

  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mCurStatePtr->mTMatrix->TransformCoord(&x, &y);

  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mCurStatePtr->mTMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

  //::SetRect(&srcrect, x, y, x + drect.width, y + drect.height);
  //::SetRect(&dstrect, drect.x, drect.y, drect.x + drect.width, drect.y + drect.height);
  ::SetRect(&srcrect, x, y, drect.width, drect.height);
  ::SetRect(&dstrect, drect.x, drect.y, drect.x + drect.width, drect.y + drect.height);

	::GetPort(&savePort);
	::SetPort(destport);

  ::SetClip(destport->visRgn);
  
  if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
  {
  	//::SetEmptyRgn(destport->clipRgn);
	//::CopyRgn(((nsDrawingSurfaceMac)aSrcSurf)->clipRgn, destport->clipRgn);
  }

  
  // Don't want to do this just yet
  //destpix = *((CGrafPtr)destport)->portPixMap;
  //destpix = ((GrafPtr)destport)->portBits;
  
  //PixMapHandle offscreenPM = ::GetGWorldPixMap((GWorldPtr)aSrcSurf);
  //if (offscreenPM)
  {
    //srcpix = (PixMapPtr)*offscreenPM;

		::GetForeColor(&saveForeColor);
		::GetBackColor(&saveBackColor);
		::RGBForeColor(&rgbblack);				//¥TODO: we should use MySetColor() from Dogbert: it's much faster
		::RGBBackColor(&rgbwhite);
		
	::CopyBits(
	  &((GrafPtr)aSrcSurf)->portBits,
	  &((GrafPtr)destport)->portBits,
	  &srcrect,
	  &dstrect,
	  ditherCopy,
	  0L);


		::RGBForeColor(&saveForeColor);		//¥TODO: we should use MySetColor() from Dogbert: it's much faster
		::RGBBackColor(&saveBackColor);
		}
	::SetPort(savePort);
		
  // HACK - cps - hack to balance the state push that occurs when the drawing surface is selected in.
  // This hack assumes that all drawing happens in the offscreen and CopyOffscreenBits is called only once
  // directly before destroying the offscreen drawing surface.
  if ( ! (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER) )
  {
  	PRBool bEmpty;
    PopState(bEmpty);
  }
  
  return NS_OK;
}
