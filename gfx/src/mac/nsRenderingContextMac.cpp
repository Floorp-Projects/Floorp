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


//------------------------------------------------------------------------

class GraphicState
{
public:
  GraphicState();
  ~GraphicState();

	void				Init();
	void				Init(nsIWidget* aWindow);
	void				Duplicate(GraphicState* aGS);

protected:
	RgnHandle		DuplicateRgn(RgnHandle aRgn);

public:
  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

public:
  nsTransform2D * 			mTMatrix; 					// transform that all the graphics drawn here will obey

  nsDrawingSurfaceMac   mRenderingSurface;	// main drawing surface, can be a BackBuffer if selected in
	PRInt32               mOffx;
  PRInt32               mOffy;

  RgnHandle							mMainRegion;
  RgnHandle			    		mClipRegion;

  nscolor               mColor;
  PRInt32               mFont;
  nsIFontMetrics * 			mFontMetrics;
	PRInt32               mCurrFontHandle;

  // Mac specific state
  RgnHandle       			mMacOriginRelativeClipRgn;
  RgnHandle							mMacPortRelativeClipRgn;
};

//------------------------------------------------------------------------

GraphicState::GraphicState()
{
	// everything is initialized to 0 through the 'new' operator
}

//------------------------------------------------------------------------

GraphicState::~GraphicState()
{
	if (mTMatrix)
	{
		delete mTMatrix;
		mTMatrix = nsnull;
	}

	if (mMainRegion)
	{
		::DisposeRgn(mMainRegion);
		mMainRegion = nsnull;
	}

	if (mClipRegion)
	{
		::DisposeRgn(mClipRegion);
		mClipRegion = nsnull;
	}

  NS_IF_RELEASE(mFontMetrics);

	if (mMacOriginRelativeClipRgn)
	{
		::DisposeRgn(mMacOriginRelativeClipRgn);
		mMacOriginRelativeClipRgn = nsnull;
	}

	if (mMacPortRelativeClipRgn)
	{
		::DisposeRgn(mMacPortRelativeClipRgn);
		mMacPortRelativeClipRgn = nsnull;
	}
}

//------------------------------------------------------------------------

void GraphicState::Init()
{
  mTMatrix				= new nsTransform2D();

  mRenderingSurface	= nsnull; 
  mOffx						= 0;
  mOffy						= 0;

  mMainRegion			= nsnull;
  mClipRegion			= nsnull;

  mColor 					= NS_RGB(255,255,255);
	mFont						= 0;
  mFontMetrics		= nsnull;
  mCurrFontHandle	= 0;
  
  // Mac specific state
  mMacOriginRelativeClipRgn	= nsnull;
  mMacPortRelativeClipRgn		= nsnull;
}

//------------------------------------------------------------------------

void GraphicState::Init(nsIWidget* aWindow)
{
	if (mTMatrix == nsnull)
		mTMatrix = new nsTransform2D();

  mRenderingSurface	= (nsDrawingSurfaceMac)aWindow->GetNativeData(NS_NATIVE_DISPLAY); 
  mOffx = (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETX);
  mOffy = (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETY);

	RgnHandle widgetRgn = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION);
	mMainRegion			= DuplicateRgn(widgetRgn);
  mClipRegion			= nsnull;

  mColor 					= NS_RGB(255,255,255);
	mFont						= 0;
  mFontMetrics		= nsnull;
  mCurrFontHandle	= 0;
  
  // Mac specific state
	RgnHandle widgetPortRgn = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION_IN_PORT);
	mMacOriginRelativeClipRgn	= DuplicateRgn(widgetPortRgn);
	mMacPortRelativeClipRgn		= DuplicateRgn(widgetPortRgn);
}

//------------------------------------------------------------------------

void GraphicState::Duplicate(GraphicState* aGS)
{
	if (aGS->mTMatrix)
		mTMatrix = new nsTransform2D(aGS->mTMatrix);
	else
		mTMatrix = new nsTransform2D();

	mRenderingSurface = aGS->mRenderingSurface;
	mOffx						= aGS->mOffx;
	mOffy						= aGS->mOffy;

	mMainRegion			= DuplicateRgn(aGS->mMainRegion);
	mClipRegion			= DuplicateRgn(aGS->mClipRegion);

	mColor					= aGS->mColor;
	mFont						= aGS->mFont;
	mFontMetrics		= aGS->mFontMetrics;
	NS_IF_ADDREF(mFontMetrics);

	mCurrFontHandle	= aGS->mCurrFontHandle;

	mMacOriginRelativeClipRgn	= DuplicateRgn(aGS->mMacOriginRelativeClipRgn);
	mMacPortRelativeClipRgn		= DuplicateRgn(aGS->mMacPortRelativeClipRgn);
}


//------------------------------------------------------------------------

RgnHandle GraphicState::DuplicateRgn(RgnHandle aRgn)
{
	RgnHandle dupRgn = nsnull;
	if (aRgn)
	{
		dupRgn = ::NewRgn();
		if (dupRgn)
			::CopyRgn(aRgn, dupRgn);
	}
	return dupRgn;
}


//------------------------------------------------------------------------

#pragma mark -

static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

nsRenderingContextMac::nsRenderingContextMac()
{
  NS_INIT_REFCNT();

  mP2T							= 1.0f;
  mContext					= nsnull ;
  mOriginalSurface	= nsnull;
  mFrontBuffer			= nsnull;
  mBackBuffer				= nsnull;
 	mCurrentBuffer		= nsnull;
 
	// create graphic state
	mGS = new GraphicState();
	mGS->Init();

  // create graphic state array 
  mGSArray = new nsVoidArray();
  
  // add graphic state to the array
  mGSArray->AppendElement(mGS);
}


//------------------------------------------------------------------------

nsRenderingContextMac::~nsRenderingContextMac()
{
	// destroy the graphic state array
	if (mGSArray)
	{
	  PRInt32 cnt = mGSArray->Count();
	  while (--cnt >= 0)
		{
	    GraphicState* gs = (GraphicState*)mGSArray->ElementAt(cnt);
	    mGSArray->RemoveElementAt(cnt);

	    if (gs)
	      delete gs;
		}
	  delete mGSArray;
	  mGSArray = nsnull;
	}

	// restore stuff
  NS_IF_RELEASE(mContext);
	::SetPort(mOriginalSurface);
	::SetOrigin(0,0); 		// setting to 0,0 doesn't really reset the state properly
}


NS_IMPL_QUERY_INTERFACE(nsRenderingContextMac, kRenderingContextIID);
NS_IMPL_ADDREF(nsRenderingContextMac);
NS_IMPL_RELEASE(nsRenderingContextMac);


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::Init(nsIDeviceContext* aContext, nsIWidget* aWindow)
{
  if (nsnull == aWindow->GetNativeData(NS_NATIVE_WINDOW))
    return NS_ERROR_NOT_INITIALIZED;

  mContext = aContext;
  NS_IF_ADDREF(mContext);

	// use widget to initialize graphic state
	mGS->Init(aWindow);

	// init rendering context data
	nsDrawingSurfaceMac widgetSurface = mGS->mRenderingSurface;
	mOriginalSurface	= widgetSurface;
	mFrontBuffer			= widgetSurface;
	mCurrentBuffer		= widgetSurface;
	
	mMacScreenPortRelativeRect = (** mGS->mMacPortRelativeClipRgn).rgnBBox;
  
	// use graphic state to initialize QuickDraw environment
  ::SetPort(mGS->mRenderingSurface);

	PRInt32	offx = mGS->mOffx;
	PRInt32 offy = mGS->mOffy;
  ::SetOrigin(-offx,-offy);
  ::OffsetRgn(mGS->mMainRegion, -offx, -offy);
  ::OffsetRgn(mGS->mMacOriginRelativeClipRgn, -offx, -offy);

	::SetClip(mGS->mMainRegion);

	::PenNormal();
	::TextMode(srcOr);

  return (CommonInit());
}

//------------------------------------------------------------------------


// should only be called for an offscreen drawing surface, without an offset or clip region
NS_IMETHODIMP nsRenderingContextMac::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);

	nsDrawingSurfaceMac drawingSurface = (nsDrawingSurfaceMac)aSurface;
	mOriginalSurface	= drawingSurface;
	mBackBuffer				= drawingSurface;
	mCurrentBuffer		= drawingSurface;

  return (CommonInit());
}

//------------------------------------------------------------------------


NS_IMETHODIMP nsRenderingContextMac::CommonInit()
{
  ((nsDeviceContextMac *)mContext)->SetDrawingSurface(mCurrentBuffer);
  //((nsDeviceContextMac *)mContext)->InstallColormap();

  mContext->GetDevUnitsToAppUnits(mP2T);

  if (mGS->mTMatrix)
  {
	  float app2dev;
	  mContext->GetAppUnitsToDevUnits(app2dev);
  	mGS->mTMatrix->AddScale(app2dev, app2dev);
  }

  SetColor(mGS->mColor);

  return NS_OK;
}


#pragma mark -

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: PushState(void)
{
	// create a GS
  GraphicState * state = new GraphicState();

	// copy the current GS into it
	state->Duplicate(mGS);

	// put the new GS at the end of the array
  mGSArray->AppendElement(state);

  // and make it the current GS
  mGS = state;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: PopState(PRBool &aClipEmpty)
{
  PRBool bEmpty = PR_FALSE;

  PRUint32 cnt = mGSArray->Count();
  if (cnt > 1) 
  {
		nscolor previousColor = mGS->mColor;

    // remove the current GS object from the array and delete it
    mGSArray->RemoveElementAt(cnt - 1);
    delete mGS;

    // get the last GS from the array and make it the current GS
    GraphicState* state = (GraphicState *)mGSArray->ElementAt(cnt - 2);
    mGS = state;
    
		// restore the QuickDraw environment
    ::SetOrigin(-mGS->mOffx, -mGS->mOffy);
    	
    ::SetClip(mGS->mMacOriginRelativeClipRgn);

		if (mGS->mColor != previousColor)
			SetColor(mGS->mColor);

		if (mGS->mFontMetrics)
			SetFont(mGS->mFontMetrics);

		if (mGS->mMacOriginRelativeClipRgn)
			if (::EmptyRgn(mGS->mMacOriginRelativeClipRgn))
      	bEmpty = PR_TRUE;
	}

  aClipEmpty = bEmpty;
  return NS_OK;
}

#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{  
  if (aSurface == nsnull)
  {
  	// set to screen port
    mCurrentBuffer = mGS->mRenderingSurface = mFrontBuffer;
	}
  else
  { 
  	// cps - This isn't as simple as just setting the surface... all the 
  	// surface relative parameters must be set as well. This means a 
  	// rendering context push.

   mMacScreenPortRelativeRect = (**mGS->mMacPortRelativeClipRgn).rgnBBox;
  
  	// This push is balaced by a pop in CopyOffScreenBits. This is a hack.
  	PushState();

  	mCurrentBuffer = mGS->mRenderingSurface = (nsDrawingSurfaceMac) aSurface;
  	
	::SetOrigin(0,0);
	
	::SetRectRgn( mGS->mMacPortRelativeClipRgn,
		((CGrafPtr)aSurface)->portRect.left,
		((CGrafPtr)aSurface)->portRect.top,
		((CGrafPtr)aSurface)->portRect.right,
		((CGrafPtr)aSurface)->portRect.bottom );
	
	::SetRectRgn( mGS->mMacOriginRelativeClipRgn, 
		0,
		0,
		((CGrafPtr)aSurface)->portRect.right - ((CGrafPtr)aSurface)->portRect.left,
		((CGrafPtr)aSurface)->portRect.bottom - ((CGrafPtr)aSurface)->portRect.top );
	
		mGS->mOffx = 0;
		mGS->mOffy = 0;
  }

	return NS_OK;
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
    destport = mGS->mRenderingSurface;
    NS_ASSERTION(!(nsnull == destport), "no back buffer");
  }
  else
    destport = mFrontBuffer;

  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mGS->mTMatrix->TransformCoord(&x, &y);

  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mGS->mTMatrix->TransformCoord(&drect.x, &drect.y, &drect.width, &drect.height);

  //::SetRect(&srcrect, x, y, x + drect.width, y + drect.height);
  //::SetRect(&dstrect, drect.x, drect.y, drect.x + drect.width, drect.y + drect.height);
  ::SetRect(&srcrect, x, y, drect.width, drect.height);
  ::SetRect(
    &dstrect, 
    mMacScreenPortRelativeRect.left, 
    mMacScreenPortRelativeRect.top, 
    mMacScreenPortRelativeRect.left + drect.x + drect.width, 
    mMacScreenPortRelativeRect.top + drect.y + drect.height);
  

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
	mCurrentBuffer = mGS->mRenderingSurface = mFrontBuffer;		// point back at the front surface
		
  //if (mRenderingSurface == (GrafPtr)theoff)
    //mRenderingSurface = nsnull;

  //DisposeRgn(mMainRegion);
  //mMainRegion = nsnull;

  return NS_OK;
}


#pragma mark -

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
    if (nsnull != mGS->mClipRegion) 
    {
      tregion = ::NewRgn();
      ::SectRgn(theregion,mGS->mClipRegion,tregion);
      ::DisposeRgn(theregion);
      ::DisposeRgn(mGS->mClipRegion);
      mGS->mClipRegion = tregion;
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
    if (nsnull != mGS->mClipRegion) 
	{
	  tregion = ::NewRgn();
	  ::UnionRgn(theregion, mGS->mClipRegion, tregion);
	  ::DisposeRgn(mGS->mClipRegion);
	  ::DisposeRgn(theregion);
	  mGS->mClipRegion = tregion;
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
    if (nsnull != mGS->mClipRegion) 
    {
      tregion = ::NewRgn();
	  ::DiffRgn(mGS->mClipRegion, theregion, tregion);
      ::DisposeRgn(mGS->mClipRegion);
      ::DisposeRgn(theregion);
      mGS->mClipRegion = tregion;
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
    if (nsnull != mGS->mClipRegion)
      ::DisposeRgn(mGS->mClipRegion);

    mGS->mClipRegion = theregion;
    
    
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

	StartDraw();
  if (nsnull == mGS->mMacOriginRelativeClipRgn) 
  {
    bEmpty = PR_TRUE;
    // clip to the size of the window
    ::ClipRect(&mGS->mRenderingSurface->portRect);
  } 
  else 
  {
    // set the clipping area of this windowptr
    ::SetClip(mGS->mMacOriginRelativeClipRgn);
	}
	EndDraw();

  aClipEmpty = bEmpty;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
  nsRect  trect = aRect;

  mGS->mTMatrix->TransformCoord(&trect.x, &trect.y,&trect.width, &trect.height);

  return SetClipRectInPixels(trect, aCombine, aClipEmpty);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
  Rect	cliprect;

  if (mGS->mClipRegion != nsnull) 
  {
  	cliprect = (**mGS->mClipRegion).rgnBBox;
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
  //Rect	    mrect;
  //RgnHandle	mregion;

  nsRegionMac *pRegion = (nsRegionMac *)&aRegion;
  
  //pRegion->GetNativeRegion(&mregion);
  //mrect = (**mregion).rgnBBox;
  pRegion->GetBoundingBox(&rect.x, &rect.y, &rect.width, &rect.height);
  
  //rect.x = mrect.left;
  //rect.y = mrect.top;
  //rect.width = mrect.right-mrect.left;
  //rect.height = mrect.bottom-mrect.top;

  SetClipRectInPixels(rect, aCombine, aClipEmpty);

  if (::EmptyRgn(mGS->mClipRegion) == PR_TRUE)
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
	StartDraw();

	#define COLOR8TOCOLOR16(color8)	 ((color8 << 8) | color8)

  RGBColor	thecolor;
	thecolor.red = COLOR8TOCOLOR16(NS_GET_R(aColor));
	thecolor.green = COLOR8TOCOLOR16(NS_GET_G(aColor));
	thecolor.blue = COLOR8TOCOLOR16(NS_GET_B(aColor));
	::RGBForeColor(&thecolor);
  mGS->mColor = aColor ;
 
	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetColor(nscolor &aColor) const
{
  aColor = mGS->mColor;
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
	NS_IF_RELEASE(mGS->mFontMetrics);

	if (mContext)
		mContext->GetMetricsFor(aFont, mGS->mFontMetrics);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetFont(nsIFontMetrics *aFontMetrics)
{
	NS_IF_RELEASE(mGS->mFontMetrics);
	mGS->mFontMetrics = aFontMetrics;
	NS_IF_ADDREF(mGS->mFontMetrics);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
  NS_IF_ADDREF(mGS->mFontMetrics);
  aFontMetrics = mGS->mFontMetrics;
  return NS_OK;
}

//------------------------------------------------------------------------

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextMac :: Translate(nscoord aX, nscoord aY)
{
  mGS->mTMatrix->AddTranslation((float)aX,(float)aY);
  return NS_OK;
}

//------------------------------------------------------------------------

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextMac :: Scale(float aSx, float aSy)
{
  mGS->mTMatrix->AddScale(aSx, aSy);
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetCurrentTransform(nsTransform2D *&aTransform)
{
  aTransform = mGS->mTMatrix;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
	StartDraw();

  mGS->mTMatrix->TransformCoord(&aX0,&aY0);
  mGS->mTMatrix->TransformCoord(&aX1,&aY1);
	::MoveTo(aX0, aY0);
	::LineTo(aX1, aY1);

	EndDraw();
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
	StartDraw();
	
  nscoord x,y,w,h;
  Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
	::SetRect(&therect,x,y,x+w,y+h);
	::FrameRect(&therect);

	EndDraw();
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
	StartDraw();

  nscoord  x,y,w,h;
  Rect     therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  // TODO - cps - must debug and fix this 
  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);

	::SetRect(&therect,x,y,x+w,y+h);
	::PaintRect(&therect);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	StartDraw();

  PRUint32   i;
  PolyHandle thepoly;
  PRInt32    x,y;

  thepoly = ::OpenPoly();
	
  x = aPoints[0].x;
  y = aPoints[0].y;
  mGS->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
  ::MoveTo(x,y);

  for (i = 1; i < aNumPoints; i++)
  {
    x = aPoints[i].x;
    y = aPoints[i].y;
		
		mGS->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
	}

	ClosePoly();
	
	::FramePoly(thepoly);

	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	StartDraw();

  PRUint32   i;
  PolyHandle thepoly;
  PRInt32    x,y;

  thepoly = ::OpenPoly();
	
  x = aPoints[0].x;
  y = aPoints[0].y;
  mGS->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
  ::MoveTo(x,y);

  for (i = 1; i < aNumPoints; i++)
  {
    x = aPoints[i].x;
    y = aPoints[i].y;
		mGS->mTMatrix->TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
	}

	::ClosePoly();
	
	::PaintPoly(thepoly);

	EndDraw();
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
	StartDraw();

  nscoord x,y,w,h;
  Rect    therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,x+h);
  ::FrameOval(&therect);

	EndDraw();
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
	StartDraw();

	nscoord x,y,w,h;
  Rect    therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;

  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,x+h);
  ::PaintOval(&therect);

	EndDraw();
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
	StartDraw();

  nscoord x,y,w,h;
  Rect    therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  
  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,x+h);
  ::FrameArc(&therect,aStartAngle,aEndAngle);

	EndDraw();
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
	StartDraw();

  nscoord x,y,w,h;
  Rect		therect;

  x = aX;
  y = aY;
  w = aWidth;
  h = aHeight;
  
  mGS->mTMatrix->TransformCoord(&x,&y,&w,&h);
  ::SetRect(&therect,x,y,x+w,x+h);
  ::PaintArc(&therect,aStartAngle,aEndAngle);

	EndDraw();
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
	StartDraw();

	// set native font and attributes
  nsFont *font;
  mGS->mFontMetrics->GetFont(font);
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
	  mGS->mFontMetrics->GetMaxAdvance(aAdvance);
			aWidth += aAdvance;
			break;
	}

	EndDraw();
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
	StartDraw();

	PRInt32 x = aX;
	PRInt32 y = aY;

  if (mGS->mFontMetrics)
  {
		// set native font and attributes
    nsFont *font;
    mGS->mFontMetrics->GetFont(font);
		nsFontMetricsMac::SetFont(*font, mContext);

		// substract ascent since drawing specifies baseline
		nscoord ascent = 0;
		mGS->mFontMetrics->GetMaxAscent(ascent);
		y += ascent;
	}

  mGS->mTMatrix->TransformCoord(&x,&y);

	::MoveTo(x,y);
	::DrawText(aString,0,aLength);

  if (mGS->mFontMetrics)
	{
		const nsFont* font;
    mGS->mFontMetrics->GetFont(font);
		PRUint8 deco = font->decorations;

		if (deco & NS_FONT_DECORATION_OVERLINE)
			DrawLine(aX, aY, aX + aWidth, aY);

		if (deco & NS_FONT_DECORATION_UNDERLINE)
		{
			nscoord ascent = 0;
			nscoord descent = 0;
	  	mGS->mFontMetrics->GetMaxAscent(ascent);
	  	mGS->mFontMetrics->GetMaxDescent(descent);

			DrawLine(aX, aY + ascent + (descent >> 1),
						aX + aWidth, aY + ascent + (descent >> 1));
		}

		if (deco & NS_FONT_DECORATION_LINE_THROUGH)
		{
			nscoord height = 0;
	 		mGS->mFontMetrics->GetHeight(height);

			DrawLine(aX, aY + (height >> 1), aX + aWidth, aY + (height >> 1));
		}
	}

	EndDraw();
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
	StartDraw();

  nsRect sr = aSRect;
  nsRect dr = aDRect;
  mGS->mTMatrix->TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);
  mGS->mTMatrix->TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);
  
  nsresult result =  aImage->Draw(*this,mGS->mRenderingSurface,sr.x,sr.y,sr.width,sr.height,
                      dr.x,dr.y,dr.width,dr.height);

	EndDraw();
	return result;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawImage(nsIImage *aImage, const nsRect& aRect)
{
	StartDraw();

  nsRect tr = aRect;
  mGS->mTMatrix->TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  
	nsresult result = aImage->Draw(*this,mGS->mRenderingSurface,tr.x,tr.y,tr.width,tr.height);

	EndDraw();
	return result;
}
