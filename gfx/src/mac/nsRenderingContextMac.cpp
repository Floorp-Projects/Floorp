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
#include "nsRegionMac.h"

#include "nsTransform2D.h"
#include "nsVoidArray.h"
#include "nsGfxCIID.h"
#include "nsCOMPtr.h"
#include "plhash.h"

#include <FixMath.h>
#include <Gestalt.h>


#pragma mark -

//------------------------------------------------------------------------
//	GraphicState
//
//------------------------------------------------------------------------

#define DrawingSurface	nsDrawingSurfaceMac


//------------------------------------------------------------------------

GraphicState::GraphicState()
{
	// everything is initialized to 0 through the 'new' operator
}


//------------------------------------------------------------------------

GraphicState::GraphicState(GraphicState* aGS)
{
	this->Duplicate(aGS);
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

  mOffx						= 0;
  mOffy						= 0;
  mColor 					= NS_RGB(255,255,255);
	mFont						= 0;
  mFontMetrics		= nsnull;
  mCurrFontHandle	= 0;
}

//------------------------------------------------------------------------

void GraphicState::Init(nsDrawingSurface aSurface)
{
	// retrieve the grafPort
	DrawingSurface* surface = static_cast<DrawingSurface*>(aSurface);
	GrafPtr port;
	surface->GetGrafPtr(&port);

	// init from grafPort
	Init(port);
}

//------------------------------------------------------------------------

void GraphicState::Init(GrafPtr aPort)
{
	// delete old values
	Clear();

	// init from grafPort (usually an offscreen port)
	RgnHandle	rgn = ::NewRgn();
  ::RectRgn(rgn, &aPort->portRect);

  mMainRegion			= rgn;
  mClipRegion			= DuplicateRgn(rgn);

}

//------------------------------------------------------------------------

void GraphicState::Init(nsIWidget* aWindow)
{
	// delete old values
	Clear();

	// init from widget
  mOffx						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETX);
  mOffy						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETY);

	RgnHandle widgetRgn = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION);
	mMainRegion			= DuplicateRgn(widgetRgn);
  mClipRegion			= DuplicateRgn(widgetRgn);
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

  mOriginalSurface	= new nsDrawingSurfaceMac();
  mFrontSurface			= new nsDrawingSurfaceMac();

	mCurrentSurface		= nsnull;
  mPort							= nsnull;
	mGS								= nsnull;

  mGSStack					= new nsVoidArray();

  mChanges = kFontChanged | kColorChanged;
}


//------------------------------------------------------------------------

nsRenderingContextMac::~nsRenderingContextMac()
{
	// restore stuff
  NS_IF_RELEASE(mContext);
  if (mOriginalSurface)
  {
		GrafPtr port;
		mOriginalSurface->GetGrafPtr(&port);
		::SetPort(port);

		::SetOrigin(0,0); 		//¥TODO? Setting to 0,0 may not really reset the state properly.
													// Maybe we should also restore the GS from mOriginalSurface.
	}

	// delete surfaces
	if (mOriginalSurface)
	{
		delete mOriginalSurface;
		mOriginalSurface = nsnull;
	}

	if (mFrontSurface)
	{
		delete mFrontSurface;
		mFrontSurface = nsnull;
	}

	mCurrentSurface = nsnull;
	mPort = nsnull;
	mGS = nsnull;

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
 
	GrafPtr port;
	mOriginalSurface->GetGrafPtr(&port);
	if(port == nsnull)
		mOriginalSurface->Init(aWindow);

 	// select the surface
	mFrontSurface->Init(aWindow);
	SelectDrawingSurface(mFrontSurface);

	// clip out the children from the GS main region
	nsCOMPtr<nsIEnumerator> children ( dont_AddRef(aWindow->GetChildren()) );
  nsresult result = NS_OK;
	if (children)
	{
		RgnHandle myRgn = ::NewRgn();
		RgnHandle childRgn = ::NewRgn();
		::CopyRgn(mGS->mMainRegion, myRgn);

		children->First();
		do
		{
			nsCOMPtr<nsISupports> child;
			if ( NS_SUCCEEDED(children->CurrentItem(getter_AddRefs(child))) )
			{	
				// thanks to sdr@camsoft.com for pointing out a memory leak here,
				// leading to the use of nsCOMPtr
				nsCOMPtr<nsIWidget> childWidget;
        childWidget = do_QueryInterface( child, &result);
				if ( childWidget ) {
					nsRect childRect;
					Rect macRect;
					childWidget->GetBounds(childRect);
					::SetRect(&macRect, childRect.x, childRect.y, childRect.x + childRect.width, childRect.y + childRect.height);
					::RectRgn(childRgn, &macRect);
					::DiffRgn(myRgn, childRgn, myRgn);
				}	
			}
		} while (NS_SUCCEEDED(children->Next()));			

		::CopyRgn(myRgn, mGS->mMainRegion);
		::CopyRgn(myRgn, mGS->mClipRegion);
		::DisposeRgn(myRgn);
		::DisposeRgn(childRgn);
	}

  return result;
}

//------------------------------------------------------------------------

// should only be called for an offscreen drawing surface, without an offset or clip region
NS_IMETHODIMP nsRenderingContextMac::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);
   
	GrafPtr port;
	mOriginalSurface->GetGrafPtr(&port);
	if(port==nsnull)
		mOriginalSurface->Init(aSurface);

	// select the surface
	DrawingSurface* surface = static_cast<DrawingSurface*>(aSurface);
	SelectDrawingSurface(surface);

  return NS_OK;
}

//------------------------------------------------------------------------

// used by nsDeviceContextMac::CreateRenderingContext() for printing
nsresult nsRenderingContextMac::Init(nsIDeviceContext* aContext, GrafPtr aPort)
{
  mContext = aContext;
  NS_IF_ADDREF(mContext);
 
	GrafPtr port;
	mOriginalSurface->GetGrafPtr(&port);
	if(port==nsnull)
		mOriginalSurface->Init(aPort);

 	// select the surface
	mFrontSurface->Init(aPort);
	SelectDrawingSurface(mFrontSurface);

  return NS_OK;
}

//------------------------------------------------------------------------

void	nsRenderingContextMac::SelectDrawingSurface(DrawingSurface* aSurface)
{
	if (! aSurface)
		return;

	mCurrentSurface = aSurface;
	aSurface->GetGrafPtr(&mPort);
	mGS		= aSurface->GetGS();

	// quickdraw initialization
  ::SetPort(mPort);

  ::SetOrigin(-mGS->mOffx, -mGS->mOffy);		// line order...

	::SetClip(mGS->mClipRegion);							// ...does matter

	::PenNormal();
	::PenMode(patCopy);
	::TextMode(srcOr);

  this->SetColor(mGS->mColor);

	if (mGS->mFontMetrics)
		SetFont(mGS->mFontMetrics);
	
	if (!mContext) return;
	
	// GS and context initializations
  ((nsDeviceContextMac *)mContext)->SetDrawingSurface(mPort);
#if 0
	((nsDeviceContextMac *)mContext)->InstallColormap();
#endif

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
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SetPortTextState()
{
	NS_PRECONDITION(mGS->mFontMetrics != nsnull, "No font metrics in SetPortTextState");
	
	if (nsnull == mGS->mFontMetrics)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(mContext != nsnull, "No device context in SetPortTextState");
	
	if (nsnull == mContext)
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
	if (!gs)
		return NS_ERROR_OUT_OF_MEMORY;

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

		// copy the GS into the current one and tell the current surface to use it
		mGS->Duplicate(gs);
		SelectDrawingSurface(mCurrentSurface);

    // remove the GS object from the stack and delete it
    mGSStack->RemoveElementAt(cnt - 1);
    delete gs;
	}

  aClipEmpty = (::EmptyRgn(mGS->mClipRegion));

  return NS_OK;
}

#pragma mark -

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::LockDrawingSurface(PRInt32 aX, PRInt32 aY,
                                                          PRUint32 aWidth, PRUint32 aHeight,
                                                          void **aBits, PRInt32 *aStride,
                                                          PRInt32 *aWidthBytes, PRUint32 aFlags)
{
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::UnlockDrawingSurface(void)
{
  return NS_OK;
}

//------------------------------------------------------------------------


NS_IMETHODIMP nsRenderingContextMac::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{  
	DrawingSurface* surface = static_cast<DrawingSurface*>(aSurface);

  if (surface != nsnull)
		SelectDrawingSurface(surface);				// select the offscreen surface...
  else
		SelectDrawingSurface(mFrontSurface);	// ...or get back to the window port

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetDrawingSurface(nsDrawingSurface *aSurface)
{  
  *aSurface = mCurrentSurface;
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: CopyOffScreenBits(nsDrawingSurface aSrcSurf,
                                                         PRInt32 aSrcX, PRInt32 aSrcY,
                                                         const nsRect &aDestBounds,
                                                         PRUint32 aCopyFlags)
{
	// hack: shortcut to bypass empty frames
	// or frames entirely recovered with other frames
	if ((aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER) == 0)
		if (::EmptyRgn(mFrontSurface->GetGS()->mMainRegion))
			return NS_OK;

	// retrieve the surface
	DrawingSurface* srcSurface = static_cast<DrawingSurface*>(aSrcSurf);
	GrafPtr srcPort;
	srcSurface->GetGrafPtr(&srcPort);

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
  
	// get the source clip region
	RgnHandle clipRgn;
  if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION)
  	clipRgn = srcPort->clipRgn;
  else
  	clipRgn = nil;

	// get the destination port and surface
  GrafPtr destPort;
	DrawingSurface* destSurface;
  if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER)
  {
    destSurface	= mCurrentSurface;
    destPort		= mPort;
    NS_ASSERTION((destPort != nsnull), "no back buffer");
  }
  else
  {
    destSurface	= mFrontSurface;
		mFrontSurface->GetGrafPtr(&destPort);
	}

	// select the destination surface to set the colors
	DrawingSurface* saveSurface = nsnull;
	if (mCurrentSurface != destSurface)
	{
		saveSurface = mCurrentSurface;
		SelectDrawingSurface(destSurface);
	}

	// set the right colors for CopyBits
  RGBColor foreColor;
  Boolean changedForeColor = false;
  ::GetForeColor(&foreColor);
  if ((foreColor.red != 0x0000) || (foreColor.green != 0x0000) || (foreColor.blue != 0x0000))
  {
	  RGBColor rgbBlack = {0x0000,0x0000,0x0000};
		::RGBForeColor(&rgbBlack);
		changedForeColor = true;
	}

  RGBColor backColor;
  Boolean changedBackColor = false;
  ::GetBackColor(&backColor);
  if ((backColor.red != 0xFFFF) || (backColor.green != 0xFFFF) || (backColor.blue != 0xFFFF))
  {
	  RGBColor rgbWhite = {0xFFFF,0xFFFF,0xFFFF};
		::RGBBackColor(&rgbWhite);
		changedBackColor = true;
	}

	// copy the bits now
	::CopyBits(
		  &srcPort->portBits,
		  &destPort->portBits,
		  &macSrcRect,
		  &macDstRect,
		  srcCopy,
		  clipRgn);

	// restore colors and surface
	if (changedForeColor)
		::RGBForeColor(&foreColor);
	if (changedBackColor)
			::RGBBackColor(&backColor);

	if (saveSurface != nsnull)
		SelectDrawingSurface(saveSurface);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
  PRUint32 depth = 8;
  if (mContext)
  	mContext->GetDepth(depth);

	// get rect
  Rect macRect;
  if (aBounds != nsnull)
  {
  	// fyi, aBounds->x and aBounds->y are always 0 here
  	::SetRect(&macRect, aBounds->x, aBounds->y, aBounds->XMost(), aBounds->YMost());
  }
  else
  	::SetRect(&macRect, 0, 0, 2, 2);

	nsDrawingSurfaceMac* surface = new nsDrawingSurfaceMac();
	if (!surface)
		return NS_ERROR_OUT_OF_MEMORY;

	surface->Init(depth,macRect.right,macRect.bottom,aSurfFlags);
	aSurface = surface;	

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DestroyDrawingSurface(nsDrawingSurface aSurface)
{
	if (!aSurface)
  	return NS_ERROR_FAILURE;

	// if that surface is still the current one, select the front surface

	if (aSurface == mCurrentSurface)
		SelectDrawingSurface(mFrontSurface);

	// delete the offscreen
	DrawingSurface* surface = static_cast<DrawingSurface*>(aSurface);
	GWorldPtr offscreenGWorld;
	mOriginalSurface->GetGrafPtr(&(GrafPtr)offscreenGWorld);
	::UnlockPixels(::GetGWorldPixMap(offscreenGWorld));
	::DisposeGWorld(offscreenGWorld);

	// delete the surface
	delete surface;

  return NS_OK;
}


#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetHints(PRUint32& aResult)
{
  PRUint32 result = 0;

  // QuickDraw is prefered over to ATSUI for drawing 7-bit text
  // (it's not 8-bit: the name of the constant is misleading)
  result |= NS_RENDERING_HINT_FAST_8BIT_TEXT;

  aResult = result;
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
	RgnHandle regionH;
	aRegion.GetNativeRegion(regionH);

	RgnHandle clipRgn = mGS->mClipRegion;
	if (clipRgn == nsnull)
		clipRgn = ::NewRgn();

	switch (aCombine)
	{
	  case nsClipCombine_kIntersect:
	  	::SectRgn(clipRgn, regionH, clipRgn);
	  	break;

	  case nsClipCombine_kUnion:
	  	::UnionRgn(clipRgn, regionH, clipRgn);
	  	break;

	  case nsClipCombine_kSubtract:
	  	::DiffRgn(clipRgn, regionH, clipRgn);
	  	break;

	  case nsClipCombine_kReplace:
	  	::CopyRgn(regionH, clipRgn);
	  	break;
	}

	StartDraw();
		::SetClip(clipRgn);
	EndDraw();

	mGS->mClipRegion = clipRgn;
	aClipEmpty = ::EmptyRgn(clipRgn);

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetClipRegion(nsIRegion **aRegion)
{
  nsresult  rv = NS_OK;

  NS_ASSERTION(!(nsnull == aRegion), "no region ptr");

  if (nsnull == *aRegion)
  {
    nsRegionMac *rgn = new nsRegionMac();

    if (nsnull != rgn)
    {
      NS_ADDREF(rgn);

      rv = rgn->Init();

      if (NS_OK != rv)
        NS_RELEASE(rgn);
      else
        *aRegion = rgn;
    }
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
  }

  if (rv == NS_OK)
  {
		nsRegionMac** macRegion = (nsRegionMac**)aRegion;
		(*macRegion)->SetNativeRegion(mGS->mClipRegion);
  }

  return rv;
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
 
  mChanges |= kColorChanged;

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
	// note: the line style must be saved in the GraphicState like font, color, etc...
	NS_NOTYETIMPLEMENTED("nsRenderingContextMac::SetLineStyle");//¥TODO
  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetLineStyle(nsLineStyle &aLineStyle)
{
	NS_NOTYETIMPLEMENTED("nsRenderingContextMac::GetLineStyle");//¥TODO
  return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetFont(const nsFont& aFont)
{
	NS_IF_RELEASE(mGS->mFontMetrics);

	if (mContext)
		mContext->GetMetricsFor(aFont, mGS->mFontMetrics);
  mChanges |= kFontChanged;

  return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: SetFont(nsIFontMetrics *aFontMetrics)
{
	NS_IF_RELEASE(mGS->mFontMetrics);
	mGS->mFontMetrics = aFontMetrics;
	NS_IF_ADDREF(mGS->mFontMetrics);
  mChanges |= kFontChanged;
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


#pragma mark -
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
	NS_NOTYETIMPLEMENTED("nsRenderingContextMac::DrawPolyline");	//¥TODO
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


#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(char ch, nscoord &aWidth)
{
  char buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
{
  PRUnichar buf[1];
  buf[0] = ch;
  return GetWidth(buf, 1, aWidth, aFontID);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(const nsString& aString, nscoord &aWidth, PRInt32 *aFontID)
{
  return GetWidth(aString.GetUnicode(), aString.Length(), aWidth, aFontID);
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

//   below is a bad assert, aString is not guaranteed null terminated...
//    NS_ASSERTION(strlen(aString) >= aLength, "Getting width on garbage string");
  
	// measure text
	short textWidth = ::TextWidth(aString, 0, aLength);
	aWidth = NSToCoordRound(float(textWidth) * mP2T);

	EndDraw();
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth, PRInt32 *aFontID)
{
	// First, let's try to convert the Unicode string to MacRoman
	// and measure it as a C-string by using QuickDraw

	nsresult res = NS_OK;
	char buffer[500];
	char* buf = (aLength <= 500 ? buffer : new char[aLength]);
	if (!buf)
		return NS_ERROR_OUT_OF_MEMORY;

	PRBool isMacRomanFont = PR_TRUE; // XXX we need to set up this value latter.
	PRBool useQD = nsATSUIUtils::ConvertToMacRoman(aString, aLength, buf, ! isMacRomanFont);

	if (useQD)
		res = GetWidth(buf, aLength, aWidth);

	if (buf != buffer)
		delete[] buf;

	if (useQD)
		return res;


	// If the string cannot be converted to MacRoman, let's draw it by using ATSUI.
	// If ATSUI is not available, measure the C-string without conversion.

	if (nsATSUIUtils::IsAvailable())
	{
		mATSUIToolkit.PrepareToDraw((mChanges & (kFontChanged | kColorChanged) != 0), mGS, mPort, mContext);
		res = mATSUIToolkit.GetWidth(aString, aLength, aWidth, aFontID);	
	}
	else
	{
		// XXX Unicode Broken!!! We should use TEC here to convert Unicode to different script run
		nsString nsStr;
		nsStr.SetString(aString, aLength);
		char* cStr = nsStr.ToNewCString();

		res = GetWidth(cStr, aLength, aWidth);

		delete[] cStr;
		if (nsnull != aFontID)
			*aFontID = 0;
	}
  return res;
}


#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const char *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY,
                                         const nscoord* aSpacing)
{
	nsresult res = NS_OK;
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
  if ( aSpacing == NULL )
		::DrawText(aString,0,aLength);
  else
  {
		int buffer[500];
		int* spacing = (aLength <= 500 ? buffer : new int[aLength]);
		if (spacing)
		{
			mGS->mTMatrix->ScaleXCoords(aSpacing, aLength, spacing);
			PRInt32 currentX = x;
			for (PRInt32 i = 0; i < aLength; i++)
			{
				::DrawChar(aString[i]);
				currentX += spacing[i];
				::MoveTo(currentX, y);
			}
			if (spacing != buffer)
				delete[] spacing;
		}
		else
			res =  NS_ERROR_OUT_OF_MEMORY;
  }

  EndDraw();
  return res;
}



//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{
	// First, let's try to convert the Unicode string to MacRoman
	// and draw it as a C-string by using QuickDraw
		
	nsresult res = NS_OK;
	char buffer[500];
	char* buf = (aLength <= 500 ? buffer : new char[aLength]);
	if (!buf)
		return NS_ERROR_OUT_OF_MEMORY;

	PRBool isMacRomanFont = PR_TRUE; // XXX we need to set up this value latter.
	PRBool useQD = nsATSUIUtils::ConvertToMacRoman(aString, aLength, buf, ! isMacRomanFont);

	if (useQD)
		res = DrawString(buf, aLength, aX, aY, aSpacing);

	if (buf != buffer)
		delete[] buf;

	if (useQD)
		return res;


	// If the string cannot be converted to MacRoman, let's draw it by using ATSUI.
	// If ATSUI is not available, draw the C-string without conversion.

	if (nsATSUIUtils::IsAvailable())
	{
		mATSUIToolkit.PrepareToDraw((mChanges & (kFontChanged | kColorChanged) != 0), mGS, mPort, mContext);
		res = mATSUIToolkit.DrawString(aString, aLength, aX, aY, aFontID, aSpacing);	
	}
	else
	{
		// XXX Unicode Broken!!! We should use TEC here to convert Unicode to different script run
		nsString nsStr;
		nsStr.SetString(aString, aLength);
		char* cStr = nsStr.ToNewCString();

		res = DrawString(cStr, aLength, aX, aY, aSpacing);

		delete[] cStr;
	}
	return res;	
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac :: DrawString(const nsString& aString,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{
 	nsresult rv = DrawString(aString.GetUnicode(), aString.Length(), aX, aY, aFontID, aSpacing);
	return rv;
}




#pragma mark -
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
  
  nsresult result =  aImage->Draw(*this,mPort,sr.x,sr.y,sr.width,sr.height,
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
  
	nsresult result = aImage->Draw(*this,mPort,tr.x,tr.y,tr.width,tr.height);

	EndDraw();
	return result;
}
