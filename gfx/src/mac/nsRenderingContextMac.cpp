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
#include "nsIRegion.h"
#include "nsIEnumerator.h"

#include "nsTransform2D.h"
#include "nsVoidArray.h"
#include "nsGfxCIID.h"

#define USE_ATSUI_HACK

#ifdef USE_ATSUI_HACK
#include <ATSUnicode.h>
#include <FixMath.h>
#include <Gestalt.h>
#endif

// Converts a Latin-1 String to a MacRoman one in place
static void ConvertLatin1ToMacRoman( char* aString)
{
	char* location = aString;
	static char  conversionTable[]={
0xDB, 0x2A,  0xE2, 0xC4, 0xE3, 0xC9,  0xA0, 0xE0,  0xF6, 0xE4,  0x53, 0xDC,  0xCE, 0x2A,  0x2A, 0x2A,
0x2A, 0xD4,  0xD5, 0xD2,  0xD3, 0xA5,  0xD0, 0xD1,  0xF7, 0xAA,  0x73, 0xDD,  0xCF, 0x2A,  0x2A, 0xD9,
0xCA, 0xC1,  0xA2, 0xA3,  0xDB, 0xB4,  0x7C, 0xA4,  0xAC, 0xA9, 0xBB, 0xC7,  0xC2, 0xD0,  0xA8, 0xF8,
0xA1, 0xB1,  0x32, 0x33,  0xAB, 0xB5,  0xA6, 0xE1,  0xFC, 0x31,  0xBC, 0xC8,  0x2A, 0x2A,  0x2A, 0xC0,
0xCB, 0xE7,  0xE5, 0xCC,  0x80, 0x81,  0xAE, 0x82, 0xE9, 0x83,  0xE6, 0xE8,  0xED, 0xEA,  0xEB, 0xEC,
0xDC, 0x84,  0xF1, 0xEE,  0xEF, 0xCD,  0x85, 0x78,  0xAF, 0xF4,  0xF2, 0xF3,  0x86, 0xA0,  0xDE, 0xA7,
0x88, 0x87,  0x89, 0x8B,  0x8A, 0x8C,  0xBE, 0x8D,  0x8F, 0x8E,  0x90, 0x91,  0x93, 0x92,  0x94, 0x95,
0xDD, 0x96,  0x98, 0x97,  0x99, 0x9B,  0x9A, 0xD6,  0xBF, 0x9D,  0x9C, 0x9E,  0x9F, 0xE0,  0xDF, 0xD8
};
	int	ch;
	while ( (ch= (*location)) != 0 )
	{
		if ( ch<0 )
			*location = conversionTable[ *location+128];
		location++;
	}
}
//------------------------------------------------------------------------


class GraphicState
{
public:
  GraphicState();
  ~GraphicState();

	void				Clear();
	void				Init(nsDrawingSurface aSurface);
	void				Init(nsIWidget* aWindow);
	void				Set(nsDrawingSurface aSurface);
	void				Set(nsIWidget* aWindow);
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
};

//------------------------------------------------------------------------

GraphicState::GraphicState()
{
	// everything is initialized to 0 through the 'new' operator
}

//------------------------------------------------------------------------

GraphicState::~GraphicState()
{
	Clear();
}

//------------------------------------------------------------------------

void GraphicState::Clear()
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
}

//------------------------------------------------------------------------

void GraphicState::Init(nsDrawingSurface aSurface)
{
	// delete old values
	Clear();

	// set the new ones
  mTMatrix				= nsnull;

  mRenderingSurface	= (nsDrawingSurfaceMac)aSurface; 
  mOffx						= 0;
  mOffy						= 0;

  mMainRegion			= ::NewRgn();
  ::RectRgn(mMainRegion, &((nsDrawingSurfaceMac)aSurface)->portRect);
  mClipRegion			= DuplicateRgn(mMainRegion);

  mColor 					= NS_RGB(255,255,255);
	mFont						= 0;
  mFontMetrics		= nsnull;
  mCurrFontHandle	= 0;
}

//------------------------------------------------------------------------

void GraphicState::Init(nsIWidget* aWindow)
{
	// delete old values
	Clear();

	// set the new ones
	mTMatrix				= nsnull;

  mRenderingSurface	= (nsDrawingSurfaceMac)aWindow->GetNativeData(NS_NATIVE_DISPLAY); 
  mOffx						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETX);
  mOffy						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETY);

	RgnHandle widgetRgn = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION);
	mMainRegion			= DuplicateRgn(widgetRgn);
  mClipRegion			= DuplicateRgn(widgetRgn);

  mColor 					= NS_RGB(255,255,255);
	mFont						= 0;
  mFontMetrics		= nsnull;
  mCurrFontHandle	= 0;
}

//------------------------------------------------------------------------

void GraphicState::Set(nsDrawingSurface aSurface)
{
  mRenderingSurface	= (nsDrawingSurfaceMac)aSurface; 
  mOffx						= 0;
  mOffy						= 0;

  ::RectRgn(mMainRegion, &((nsDrawingSurfaceMac)aSurface)->portRect);
}

//------------------------------------------------------------------------

void GraphicState::Set(nsIWidget* aWindow)
{
  mRenderingSurface	= (nsDrawingSurfaceMac)aWindow->GetNativeData(NS_NATIVE_DISPLAY); 
  mOffx						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETX);
  mOffy						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETY);

	RgnHandle widgetRgn = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION);
	::CopyRgn(widgetRgn, mMainRegion);
}

//------------------------------------------------------------------------

void GraphicState::Duplicate(GraphicState* aGS)
{
	// delete old values
	Clear();

	// copy new ones
	if (aGS->mTMatrix)
		mTMatrix = new nsTransform2D(aGS->mTMatrix);
	else
		mTMatrix = nsnull;

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
  mCurrentBuffer		= nsnull;

	mGS								= nsnull;
  mGSArray					= new nsVoidArray();
	mSurfaceArray			= new nsVoidArray();
  mGSStack					= new nsVoidArray();
}


//------------------------------------------------------------------------

nsRenderingContextMac::~nsRenderingContextMac()
{
	// delete the surface array
	if (mSurfaceArray)
	{
	  delete mSurfaceArray;
	  mSurfaceArray = nsnull;
	}

	// delete the GS array and its contents
	if (mGSArray)
	{
	  PRInt32 cnt = mGSArray->Count();
	  for (PRInt32 i = 0; i < cnt; i ++)
	  {
	    GraphicState* gs = (GraphicState*)mGSArray->ElementAt(i);
    	if (gs)
    		delete gs;
	  }
	  delete mGSArray;
	  mGSArray = nsnull;
	}


	// delete the stack and its contents
	if (mGSStack)
	{
	  PRInt32 cnt = mGSStack->Count();
	  for (PRInt32 i = 0; i < cnt; i ++)
	  {
	    GraphicState* gs = (GraphicState*)mGSStack->ElementAt(i);
    	if (gs)
    		delete gs;
	  }
	  delete mGSStack;
	  mGSStack = nsnull;
	}

	// restore stuff
  NS_IF_RELEASE(mContext);
  if (mOriginalSurface)
		::SetPort(mOriginalSurface);
	::SetOrigin(0,0); 		//¥TODO? setting to 0,0 may not really reset the state properly
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
 
	// look if that surface is already in the array
  nsDrawingSurfaceMac widgetSurface	= (nsDrawingSurfaceMac)aWindow->GetNativeData(NS_NATIVE_DISPLAY); 
	PRInt32 index = mSurfaceArray->IndexOf((void*)widgetSurface);

	// if not, it needs a graphic state
	GraphicState* gs;
	if (index < 0)
	{
		// create graphic state and use widget to initialize it
		gs = new GraphicState();
		gs->Init(aWindow);

		// add new GS to the arrays
		mSurfaceArray->AppendElement((void*)widgetSurface);
		mGSArray->AppendElement((void*)gs);
	}
	else
	{
		// use widget to upate the graphic state
		gs = (GraphicState*)mGSArray->ElementAt(index);
		gs->Set(aWindow);
	}

	// clip out the children
	RgnHandle myRgn = ::NewRgn();
	RgnHandle childRgn = ::NewRgn();
	::CopyRgn(gs->mMainRegion, myRgn);

	nsIEnumerator* children = aWindow->GetChildren();
  if (children)
	{
    nsIWidget* child;
		children->First();
		do
		{
      if (NS_SUCCEEDED(children->CurrentItem((nsISupports **)&child)))
      {
      	nsRect childRect;
      	Rect macRect;
      	child->GetBounds(childRect);
      	::SetRect(&macRect, childRect.x, childRect.y, childRect.x + childRect.width, childRect.y + childRect.height);
      	::RectRgn(childRgn, &macRect);
      	::DiffRgn(myRgn, childRgn, myRgn);
      }
		}
    while (NS_SUCCEEDED(children->Next()));			
		delete children;
	}
	::CopyRgn(myRgn, gs->mMainRegion);
	::CopyRgn(myRgn, gs->mClipRegion);
	::DisposeRgn(myRgn);
	::DisposeRgn(childRgn);

	// select the surface
	if (mOriginalSurface == nsnull)
		mOriginalSurface = widgetSurface;
	mFrontBuffer = widgetSurface;

	SetQuickDrawEnvironment(gs, clipToMainRegion);

  return (CommonInit());
}

//------------------------------------------------------------------------

// should only be called for an offscreen drawing surface, without an offset or clip region
NS_IMETHODIMP nsRenderingContextMac::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);
 
	// look if that surface is already in the array
  nsDrawingSurfaceMac drawingSurface	= (nsDrawingSurfaceMac)aSurface; 
	PRInt32 index = mSurfaceArray->IndexOf((void*)drawingSurface);

	// if not, it needs a graphic state
	GraphicState* gs;
	if (index < 0)
	{
		// create graphic state and use surface to initialize it
		gs = new GraphicState();
		gs->Init(drawingSurface);

		// add new GS to the arrays
		mSurfaceArray->AppendElement((void*)drawingSurface);
		mGSArray->AppendElement((void*)gs);
	}
	else
	{
		// use surface to upate the graphic state
		gs = (GraphicState*)mGSArray->ElementAt(index);
		gs->Set(drawingSurface);
	}

	// select the surface
	if (mOriginalSurface == nsnull)
		mOriginalSurface = drawingSurface;
	mFrontBuffer = drawingSurface;	// note: we know it's not really a screen port but we have to act as if...

	SetQuickDrawEnvironment(gs, clipToMainRegion);

  return (CommonInit());
}

//------------------------------------------------------------------------


NS_IMETHODIMP nsRenderingContextMac::CommonInit()
{
  ((nsDeviceContextMac *)mContext)->SetDrawingSurface(mCurrentBuffer);
  //((nsDeviceContextMac *)mContext)->InstallColormap();

  mContext->GetDevUnitsToAppUnits(mP2T);

  if (!mGS->mTMatrix)
  {
 		mGS->mTMatrix = new nsTransform2D();
	  if (mGS->mTMatrix)
	  {
			// apply the new scaling
		  float app2dev;
		  mContext->GetAppUnitsToDevUnits(app2dev);
	  	mGS->mTMatrix->AddScale(app2dev, app2dev);
		}
	}

  return NS_OK;
}

//------------------------------------------------------------------------

void	nsRenderingContextMac::SelectDrawingSurface(nsDrawingSurfaceMac aSurface, GraphicState* aGraphicState)
{
	// retreive that surface in the array
	PRInt32 index = mSurfaceArray->IndexOf((void*)aSurface);

	// if it can't be found, assume that it was created from another
	// rendering context, attach a GS to it and add it to the array
	PRBool isNewGS = PR_FALSE;
	GraphicState* gs;
	if (index < 0)
	{
		isNewGS = PR_TRUE;
		gs = new GraphicState();
		gs->Init(aSurface);
		mSurfaceArray->AppendElement((void*)aSurface);
		mGSArray->AppendElement((void*)gs);
		index = mSurfaceArray->Count() - 1;
	}

	// get/init the graphic state
	if (aGraphicState == nsnull)
	{
		// called from SelectOffscreenDrawingSurface...
		// no GS is specified, thus fetch in the array the one that corresponds to the surface
		gs = (GraphicState*)mGSArray->ElementAt(index);
	}
	else
	{
		// called from PopState...
		// we receive a GS, thus update the one we have in the array
		gs = (GraphicState*)mGSArray->ElementAt(index);
		gs->Duplicate(aGraphicState);
	}

	SetQuickDrawEnvironment(gs, clipToClipRegion);

	if (isNewGS)
		CommonInit();
}

//------------------------------------------------------------------------

void	nsRenderingContextMac::SetQuickDrawEnvironment(GraphicState* aGraphicState, ClipMethod clipMethod)
{
	mGS							= aGraphicState;
	mCurrentBuffer	= mGS->mRenderingSurface;

  ::SetPort(mCurrentBuffer);

  ::SetOrigin(-mGS->mOffx, -mGS->mOffy);		// line order...

	if (clipMethod == clipToMainRegion)				// ...does matter
		::SetClip(mGS->mMainRegion);
	else
		::SetClip(mGS->mClipRegion);

	::PenNormal();
	::PenMode(patCopy);
	::TextMode(srcOr);

  this->SetColor(mGS->mColor);

	if (mGS->mFontMetrics)
		SetFont(mGS->mFontMetrics);
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SetPortTextState()
{
	NS_PRECONDITION(mGS->mFontMetrics != nsnull, "No font metrics in SetPortTextState");
	
	if (nsnull == mGS->mFontMetrics)
		return NS_ERROR_NULL_POINTER;

	TextStyle		theStyle;
	nsFontMetricsMac::GetNativeTextStyle(*mGS->mFontMetrics, *mContext, theStyle);

	::TextFont(theStyle.tsFont);
	::TextSize(theStyle.tsSize);
	::TextFace(theStyle.tsFace);
	
	return NS_OK;
}


#pragma mark -

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: PushState(void)
{
	// create a GS
  GraphicState * gs = new GraphicState();

	// copy the current GS into it
	gs->Duplicate(mGS);

	// put the new GS at the end of the stack
  mGSStack->AppendElement(gs);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: PopState(PRBool &aClipEmpty)
{
  PRUint32 cnt = mGSStack->Count();
  if (cnt > 0) 
  {
    // get the GS from the stack
    GraphicState* gs = (GraphicState *)mGSStack->ElementAt(cnt - 1);

		// tell the current surface to use the GS (it will make a copy of it)
		SelectDrawingSurface(mCurrentBuffer, gs);

    // remove the GS object from the stack and delete it
    mGSStack->RemoveElementAt(cnt - 1);
    delete gs;
	}

  aClipEmpty = (::EmptyRgn(mGS->mClipRegion));

  return NS_OK;
}

#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{  
  if (aSurface == nsnull)
  {
  	// get back to the screen port
		SelectDrawingSurface(mFrontBuffer, nil);
	}
  else
  { 
		// select the surface
		SelectDrawingSurface((nsDrawingSurfaceMac)aSurface, nil);
  }

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{
	// apply the selected transformations
  PRInt32	x = aSrcX;
  PRInt32	y = aSrcY;
  if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
    mGS->mTMatrix->TransformCoord(&x, &y);

  nsRect dstRect = aDestBounds;
  if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
    mGS->mTMatrix->TransformCoord(&dstRect.x, &dstRect.y, &dstRect.width, &dstRect.height);

	// get the source and destination rectangles
  Rect macSrcRect, macDstRect;
  ::SetRect(&macSrcRect,
  		x,
  		y,
  		dstRect.width,
  		dstRect.height);

  ::SetRect(&macDstRect, 
	    dstRect.x, 
	    dstRect.y, 
	    dstRect.x + dstRect.width, 
	    dstRect.y + dstRect.height);
  
	// get the destination port
  nsDrawingSurfaceMac destPort;
  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
  {
    destPort = mCurrentBuffer;
    NS_ASSERTION((destPort != nsnull), "no back buffer");
  }
  else
  {
    destPort = mFrontBuffer;
	}
	
	// get the source clip region
	RgnHandle clipRgn;
  if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
  	clipRgn = ((nsDrawingSurfaceMac)aSrcSurf)->clipRgn;
  else
  	clipRgn = nil;

	// select the destination surface to set up the QuickDraw environment
	nsDrawingSurfaceMac saveSurface = mCurrentBuffer;
	if (destPort != mCurrentBuffer)
		SelectDrawingSurface(destPort, nil);

	// make sure we are using the right colors for CopyBits
  RGBColor foreColor;
  ::GetForeColor(&foreColor);
  if ((foreColor.red != 0x0000) || (foreColor.green != 0x0000) || (foreColor.blue != 0x0000))
  {
	  RGBColor rgbBlack = {0x0000,0x0000,0x0000};
		::RGBForeColor(&rgbBlack);
	}

  RGBColor backColor;
  ::GetBackColor(&backColor);
  if ((backColor.red != 0xFFFF) || (backColor.green != 0xFFFF) || (backColor.blue != 0xFFFF))
  {
	  RGBColor rgbWhite = {0xFFFF,0xFFFF,0xFFFF};
		::RGBBackColor(&rgbWhite);
	}

	// copy the bits now
	::CopyBits(
		  &((GrafPtr)aSrcSurf)->portBits,
		  &((GrafPtr)destPort)->portBits,
		  &macSrcRect,
		  &macDstRect,
		  ditherCopy,
		  clipRgn);

	// select back the original surface
	if (saveSurface != mCurrentBuffer)
		SelectDrawingSurface(saveSurface, nil);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
	// get depth
  PRUint32 depth = 8;
  if (mContext)
  	mContext->GetDepth(depth);

	// get rect
  Rect bounds;
  if (aBounds != nsnull)
  {
  	// fyi, aBounds->x and aBounds->y are always 0 here
  	::SetRect(&bounds, aBounds->x, aBounds->y, aBounds->x + aBounds->width, aBounds->y + aBounds->height);
  }
  else
  	::SetRect(&bounds, 0, 0, 2, 2);

	// create offscreen
  GWorldPtr offscreenGWorld;
  QDErr osErr = ::NewGWorld(&offscreenGWorld, depth, &bounds, nil, nil, 0);
  if (osErr != noErr)
  	return NS_ERROR_FAILURE;

	// keep the pixels locked... that's how it works on Windows and  we are forced to do
	// the same because the API doesn't give us any hook to do it at drawing time.
  ::LockPixels(::GetGWorldPixMap(offscreenGWorld));

	// erase the offscreen area
	CGrafPtr savePort;
	GDHandle saveDevice;
	::GetGWorld(&savePort, &saveDevice);
		::SetGWorld(offscreenGWorld, nil);
		::SetOrigin(bounds.left, bounds.top);
		::EraseRect(&bounds);
	::SetGWorld(savePort, saveDevice);
  
	// add the new surface and its corresponding GS to the arrays
  aSurface = offscreenGWorld;
 	GraphicState* gs = new GraphicState();					// create graphic state
	gs->Init(aSurface);															// use surface to initialize the new graphic state
	mSurfaceArray->AppendElement((void*)aSurface);	// add the new surface and GS to the arrays
	mGSArray->AppendElement((void*)gs);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DestroyDrawingSurface(nsDrawingSurface aSurface)
{
	if (aSurface)
	{
		// if that surface is still the current one, select the screen buffer
		if (aSurface == mCurrentBuffer)
			SelectDrawingSurface(mFrontBuffer, nil);

		// delete the offscreen
		GWorldPtr offscreenGWorld = (GWorldPtr)aSurface;
		::UnlockPixels(::GetGWorldPixMap(offscreenGWorld));
		::DisposeGWorld(offscreenGWorld);

		// remove the surface and its corresponding GS from the arrays
		PRInt32 index = mSurfaceArray->IndexOf((void*)aSurface);
		if (index >= 0)
		{
			mSurfaceArray->RemoveElementAt(index);

 			GraphicState* gs = (GraphicState*)mGSArray->ElementAt(index);
 			if (gs)
 				delete gs;
			mGSArray->RemoveElementAt(index);
		}
	}
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
	Rect macRect;
	::SetRect(&macRect, aRect.x, aRect.y, aRect.x + aRect.width, aRect.y + aRect.height);

	RgnHandle rectRgn = ::NewRgn();
	::RectRgn(rectRgn, &macRect);

	RgnHandle clipRgn = mGS->mClipRegion;
	if (clipRgn == nsnull)
		clipRgn = ::NewRgn();

	switch (aCombine)
	{
	  case nsClipCombine_kIntersect:
	  	::SectRgn(clipRgn, rectRgn, clipRgn);
	  	break;

	  case nsClipCombine_kUnion:
	  	::UnionRgn(clipRgn, rectRgn, clipRgn);
	  	break;

	  case nsClipCombine_kSubtract:
	  	::DiffRgn(clipRgn, rectRgn, clipRgn);
	  	break;

	  case nsClipCombine_kReplace:
	  	::CopyRgn(rectRgn, clipRgn);
	  	break;
	}
	::DisposeRgn(rectRgn);

	StartDraw();
		::SetClip(clipRgn);
	EndDraw();

	mGS->mClipRegion = clipRgn;
	aClipEmpty = ::EmptyRgn(clipRgn);

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
  nsRect rect;
  nsIRegion* pRegion = (nsIRegion*)&aRegion;
  pRegion->GetBoundingBox(&rect.x, &rect.y, &rect.width, &rect.height);
  SetClipRectInPixels(rect, aCombine, aClipEmpty);		//¥TODO: this is wrong: we should clip to the region, not to its bounding box

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
  ::SetRect(&therect,x,y,x+w,y+h);
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
  ::SetRect(&therect,x,y,x+w,y+h);
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
  ::SetRect(&therect,x,y,x+w,y+h);
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
  ::SetRect(&therect,x,y,x+w,y+h);
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
	SetPortTextState();

	// measure text
	short textWidth = ::TextWidth(aString, 0, aLength);
	aWidth = NSToCoordRound(float(textWidth) * mP2T);


#if 0
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

#endif

	EndDraw();
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth)
{
	nsString nsStr;
	nsStr.SetString(aString, aLength);
	char* cStr = nsStr.ToNewCString();
	ConvertLatin1ToMacRoman ( cStr );
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
		SetPortTextState();

		// substract ascent since drawing specifies baseline
		nscoord ascent = 0;
		mGS->mFontMetrics->GetMaxAscent(ascent);
		y += ascent;
	}

  mGS->mTMatrix->TransformCoord(&x,&y);

	::MoveTo(x,y);
  char* macRomanString;
  char stringBuffer[128];
  if ( aLength < 128 )
  {
  	memcpy( stringBuffer, aString, aLength+1 );
  	macRomanString = stringBuffer;
  }
  else
  {
  	macRomanString = strdup( aString );
  }
  ConvertLatin1ToMacRoman ( macRomanString );
  if ( aSpacing == NULL )
		::DrawText(macRomanString,0,aLength);
  else
  {
		int buffer[500];
		int* spacing = NULL;

		if (aLength > 500)
		  spacing = new int[aLength];
		else
		  spacing = buffer;
		
		mGS->mTMatrix->ScaleXCoords(aSpacing, aLength, spacing);
		PRInt32 currentX = x;
		for ( PRInt32 i = 0; i<= aLength; i++ )
		{
			::DrawChar( macRomanString[i] );
			currentX += spacing[ i ];
			::MoveTo( currentX, y );
		}
		// clean up and restore settings
		if ( (spacing != buffer))
			delete [] spacing; 
  }
  
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
	if ( macRomanString != stringBuffer )
		free( macRomanString );
	EndDraw();
  return NS_OK;
}

//------------------------------------------------------------------------
// ATSUI Hack 
// The following ATSUI hack should go away when we rework the Mac GFX
// It is implement in a quick and dirty faction without change nsIRenderingContext
// interface. The purpose is to use ATSUI in GFX so we can start Input Method work
// before the real ATSUI GFX adoption get finish.
//------------------------------------------------------------------------
#ifdef USE_ATSUI_HACK

#define FloatToFixed(a)		((Fixed)((float)(a) * fixed1))

static OSErr AtsuSetFont(ATSUStyle theStyle, ATSUFontID theFontID)
{
	ATSUAttributeTag 		theTag;
	ByteCount				theValueSize;
	ATSUAttributeValuePtr 	theValue;

	theTag = kATSUFontTag;
	theValueSize = (ByteCount) sizeof(ATSUFontID);
	theValue = (ATSUAttributeValuePtr) &theFontID;

	return ATSUSetAttributes(theStyle, 1, &theTag, &theValueSize, &theValue);	
}


static OSErr AtsuSetSize(ATSUStyle theStyle, Fixed size)
{
	ATSUAttributeTag 		theTag;
	ByteCount				theValueSize;
	ATSUAttributeValuePtr 	theValue;

	theTag = kATSUSizeTag;
	theValueSize = (ByteCount) sizeof(Fixed);
	theValue = (ATSUAttributeValuePtr) &size;

	return ATSUSetAttributes(theStyle, 1, &theTag, &theValueSize, &theValue);
}


static OSErr AtsuSetColor(ATSUStyle theStyle, RGBColor color)
{
	ATSUAttributeTag 		theTag;
	ByteCount				theValueSize;
	ATSUAttributeValuePtr 	theValue;

	theTag = kATSUColorTag;
	theValueSize = (ByteCount) sizeof(RGBColor);
	theValue = (ATSUAttributeValuePtr) &color;

	return ATSUSetAttributes(theStyle, 1, &theTag, &theValueSize, &theValue);
}


static OSErr SetStyleSize (nsIFontMetrics& inFontMetrics, nsIDeviceContext* aContext, ATSUStyle theStyle)
{
	float  dev2app;
	aContext->GetDevUnitsToAppUnits(dev2app);
	nsFont		*aFont;
	inFontMetrics.GetFont(aFont);
	return AtsuSetSize(theStyle, FloatToFixed((float(aFont->size) / dev2app)));
}


static OSErr SetStyleColor(nscolor aColor, ATSUStyle theStyle)
{
	RGBColor	thecolor;
	thecolor.red = COLOR8TOCOLOR16(NS_GET_R(aColor));
	thecolor.green = COLOR8TOCOLOR16(NS_GET_G(aColor));
	thecolor.blue = COLOR8TOCOLOR16(NS_GET_B(aColor));	
	return AtsuSetColor(theStyle, thecolor);	
}


static OSErr SetStyleFont (nsIFontMetrics& inFontMetrics, nsIDeviceContext* aContext, ATSUStyle theStyle)
{
  TextStyle		textStyle;
	nsFontMetricsMac::GetNativeTextStyle(inFontMetrics, *aContext, textStyle);

	ATSUFontID atsuFontID ;
	
	OSErr	err = ATSUFONDtoFontID( textStyle.tsFont, textStyle.tsFace,  &atsuFontID);
	if (noErr != err) 
		return err;
	return AtsuSetFont(theStyle, atsuFontID);
}


static OSErr SetATSUIFont(nsIFontMetrics& inFontMetrics, nscolor aColor, nsIDeviceContext* aContext, ATSUStyle theStyle)
{
	OSErr					err = 0;
	err = SetStyleSize(inFontMetrics, aContext, theStyle);
	if(noErr != err)
		return err;
	err = SetStyleFont(inFontMetrics, aContext, theStyle);
	if(noErr != err)
		return err;
	return SetStyleColor(aColor, theStyle);	
}


static Boolean IsATSUIAvailable()
{
	static Boolean gInitialized = FALSE;
	static Boolean gATSUIAvailable = FALSE;
	if (! gInitialized)
	{
		long				version;
		gATSUIAvailable = (::Gestalt(gestaltATSUVersion, &version) == noErr);
		gInitialized = TRUE;
	}
	return gATSUIAvailable;
}
#endif //USE_ATSUI_HACK


//------------------------------------------------------------------------
// ATSUI Hack 
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, nscoord aWidth,
                                         const nscoord* aSpacing)
{
#ifdef USE_ATSUI_HACK
   if (IsATSUIAvailable())
   { 

			StartDraw();

			PRInt32 x = aX;
			PRInt32 y = aY;
			ATSUTextLayout 	txLayout = nil;
			ATSUStyle				theStyle;
			OSErr						err;
			
			err = ATSUCreateStyle(&theStyle);
			NS_ASSERTION((noErr == err), "ATSUCreateStyle failed");

		  if (mGS->mFontMetrics)
		  {
				// set native font and attributes
		    nsFont *font;
		    mGS->mFontMetrics->GetFont(font);
				//nsFontMetricsMac::SetFont(*font, mContext);	// this is unnecessary

				// substract ascent since drawing specifies baseline
				nscoord ascent = 0;
				mGS->mFontMetrics->GetMaxAscent(ascent);
				y += ascent;

		    err = SetATSUIFont(*mGS->mFontMetrics, mGS->mColor, mContext, theStyle);
	  		NS_ASSERTION((noErr == err), "setATSUIFont failed");
				
			}

	  mGS->mTMatrix->TransformCoord(&x,&y);


		UniCharCount			runLengths = aLength;
	  err = ATSUCreateTextLayoutWithTextPtr(	(ConstUniCharArrayPtr)aString, 0, aLength, aLength,
												1, &runLengths, &theStyle,
												&txLayout);

		NS_ASSERTION((noErr == err), "ATSUCreateTextLayoutWithTextPtr failed");
		err = ATSUSetTransientFontMatching(txLayout, true);
		NS_ASSERTION((noErr == err), "ATSUSetTransientFontMatching failed");

		err = ATSUDrawText( txLayout, 0, aLength, Long2Fix(x), Long2Fix(y) );	
		NS_ASSERTION((noErr == err), "ATSUDrawText failed");
		err =   ATSUDisposeTextLayout(txLayout);
		NS_ASSERTION((noErr == err), "ATSUDisposeTextLayout failed");
		err =   ATSUDisposeStyle(theStyle);
		NS_ASSERTION((noErr == err), "ATSUDisposeStyle failed");

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
	else
#endif //USE_ATSUI_HACK
	{
		nsString nsStr;
		nsStr.SetString(aString, aLength);
		char* cStr = nsStr.ToNewCString();

		nsresult rv = DrawString(cStr, aLength, aX, aY, aWidth,aSpacing);

		delete[] cStr;

		 return rv;
	}
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const nsString& aString,
                                         nscoord aX, nscoord aY, nscoord aWidth,
                                         const nscoord* aSpacing)
{
 	nsresult rv = DrawString(aString.GetUnicode(), aString.Length(), aX, aY, aWidth, aSpacing);
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
  
  nsresult result =  aImage->Draw(*this,mCurrentBuffer,sr.x,sr.y,sr.width,sr.height,
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
  
	nsresult result = aImage->Draw(*this,mCurrentBuffer,tr.x,tr.y,tr.width,tr.height);

	EndDraw();
	return result;
}
