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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

 
#include "nsRenderingContextMac.h"
#include "nsDeviceContextMac.h"
#include "nsFontMetricsMac.h"
#include "nsIImage.h"
#include "nsIRegion.h"
#include "nsIEnumerator.h"
#include "nsRegionMac.h"
#include "nsGraphicState.h"
#include "nsGraphicsImpl.h"

#include "nsTransform2D.h"
#include "nsVoidArray.h"
#include "nsGfxCIID.h"
#include "nsGfxUtils.h"
#include "nsCOMPtr.h"

#include "plhash.h"

#include <FixMath.h>
#include <Gestalt.h>
#include <Quickdraw.h>
#include "nsCarbonHelpers.h"

#define STACK_TREASHOLD 1000


//------------------------------------------------------------------------

nsRenderingContextMac::nsRenderingContextMac()
{
	NS_INIT_REFCNT();

	mP2T						= 1.0f;
	mContext					= nsnull ;

	mSavePort					= nsnull;
	mSaveDevice       = nsnull;
	mFrontSurface				= new nsDrawingSurfaceMac();

	mCurrentSurface				= nsnull;
	mPort						= nsnull;
	mGS							= nsnull;

	mGSStack					= new nsVoidArray();
  
	mChanges					= kEverythingChanged;
  mLineStyle = nsLineStyle_kSolid;
#ifdef IBMBIDI
  mRightToLeftText = PR_FALSE;
#endif
}


//------------------------------------------------------------------------

nsRenderingContextMac::~nsRenderingContextMac()
{
	// restore stuff
	NS_IF_RELEASE(mContext);
	if (mSavePort) {
		::SetGWorld(mSavePort, mSaveDevice);
		::SetOrigin(mSavePortRect.left, mSavePortRect.top);
	}

	// delete surfaces
	if (mFrontSurface) {
		delete mFrontSurface;
		mFrontSurface = nsnull;
	}

	mCurrentSurface = nsnull;
	mPort = nsnull;
	mGS = nsnull;

	// delete the stack and its contents
	if (mGSStack != nsnull) {
	  PRInt32 cnt = mGSStack->Count();
	  for (PRInt32 i = 0; i < cnt; i ++) {
	    nsGraphicState* gs = (nsGraphicState*)mGSStack->ElementAt(i);
    	if (gs != nsnull)
    		sGraphicStatePool.ReleaseGS(gs); //delete gs;
	  }
	  delete mGSStack;
	  mGSStack = nsnull;
	}
}


NS_IMPL_ISUPPORTS1(nsRenderingContextMac, nsIRenderingContext)


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::Init(nsIDeviceContext* aContext, nsIWidget* aWindow)
{
	// make sure all allocations in the constructor succeeded.
	if (nsnull == mFrontSurface || nsnull == mGSStack)
		return NS_ERROR_OUT_OF_MEMORY;
		
	if (nsnull == aWindow->GetNativeData(NS_NATIVE_WINDOW))
		return NS_ERROR_NOT_INITIALIZED;

	if (aContext != mContext) {
		NS_IF_RELEASE(mContext);
		mContext = aContext;
		NS_IF_ADDREF(mContext);
	}

 	// select the surface
	mFrontSurface->Init(aWindow);
	SelectDrawingSurface(mFrontSurface);

	// NOTE: here, we used to clip out the children from mGS->mMainRegion
	// and copy the resulting region into mMainRegion and mClipRegion.
	// This is no longer necessary because when initializing mGS from an nsIWidget, we
	// call GetNativeData(NS_NATIVE_REGION) that now returns the widget's visRegion
	// with the children already clipped out (as well as the areas masked by the 
	// widget's parents).

	return NS_OK;
}

//------------------------------------------------------------------------

// should only be called for an offscreen drawing surface, without an offset or clip region
NS_IMETHODIMP nsRenderingContextMac::Init(nsIDeviceContext* aContext, nsDrawingSurface aSurface)
{
	// make sure all allocations in the constructor succeeded.
	if (nsnull == mFrontSurface || nsnull == mGSStack)
		return NS_ERROR_OUT_OF_MEMORY;
		
	mContext = aContext;
	NS_IF_ADDREF(mContext);

	// select the surface
	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
	SelectDrawingSurface(surface);

	return NS_OK;
}

//------------------------------------------------------------------------

// used by nsDeviceContextMac::CreateRenderingContext() for printing
nsresult nsRenderingContextMac::Init(nsIDeviceContext* aContext, GrafPtr aPort)
{
	// make sure all allocations in the constructor succeeded.
	if (nsnull == mFrontSurface || nsnull == mGSStack)
		return NS_ERROR_OUT_OF_MEMORY;
		
	mContext = aContext;
	NS_IF_ADDREF(mContext);

 	// select the surface
	mFrontSurface->Init(aPort);
	SelectDrawingSurface(mFrontSurface);

	return NS_OK;
}

//------------------------------------------------------------------------

void nsRenderingContextMac::SelectDrawingSurface(nsDrawingSurfaceMac* aSurface, PRUint32 aChanges)
{
	if (! aSurface)
		return;

	if (!mSavePort) {
		::GetGWorld(&mSavePort, &mSaveDevice);
		if (mSavePort)
			::GetPortBounds((GrafPtr)mSavePort, &mSavePortRect);
	}
	
	// if surface is changing, be extra conservative about graphic state changes.
	if (mCurrentSurface != aSurface)
		aChanges = kEverythingChanged;
	
	mCurrentSurface = aSurface;
	aSurface->GetGrafPtr(&mPort);
	mGS = aSurface->GetGS();
	mTranMatrix = &(mGS->mTMatrix);

	// quickdraw initialization
	::SetPort(mPort);

	::SetOrigin(-mGS->mOffx, -mGS->mOffy);		// line order...

	if (aChanges & kClippingChanged)
		::SetClip(mGS->mClipRegion);			// ...does matter

	::PenNormal();
	::PenMode(patCopy);
	::TextMode(srcOr);

	if (aChanges & kColorChanged)
		this->SetColor(mGS->mColor);

	if (mGS->mFontMetrics && (aChanges & kFontChanged))
		SetFont(mGS->mFontMetrics);
	
	if (!mContext) return;
	
	// GS and context initializations
	((nsDeviceContextMac *)mContext)->SetDrawingSurface(mPort);
#if 0
	((nsDeviceContextMac *)mContext)->InstallColormap();
#endif

	mContext->GetDevUnitsToAppUnits(mP2T);

	if (mGS->mTMatrix.GetType() == MG_2DIDENTITY) {
		// apply the new scaling
		float app2dev;
		mContext->GetAppUnitsToDevUnits(app2dev);
		mGS->mTMatrix.AddScale(app2dev, app2dev);
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

NS_IMETHODIMP nsRenderingContextMac::PushState(void)
{
	// create a GS
	nsGraphicState * gs = sGraphicStatePool.GetNewGS();
	if (!gs)
		return NS_ERROR_OUT_OF_MEMORY;

	// save the current set of graphics changes.
	mGS->SetChanges(mChanges);

	// copy the current GS into it
	gs->Duplicate(mGS);

	// put the new GS at the end of the stack
	mGSStack->AppendElement(gs);
  
	// reset the graphics changes. this always represents a delta from previous state to current.
	mChanges = 0;

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::PopState(PRBool &aClipEmpty)
{
	PRInt32 count = mGSStack->Count();
	if (count > 0) {
		PRInt32 index = count - 1;
	
		// get the GS from the stack
		nsGraphicState* gs = (nsGraphicState *)mGSStack->ElementAt(index);

		// copy the GS into the current one and tell the current surface to use it
		mGS->Duplicate(gs);
		SelectDrawingSurface(mCurrentSurface, mChanges);
		
		// restore the current set of changes.
		mChanges = mGS->GetChanges();

		// remove the GS object from the stack and delete it
		mGSStack->RemoveElementAt(index);
		sGraphicStatePool.ReleaseGS(gs);
		
		// make sure the matrix is pointing at the current matrix
		mTranMatrix = &(mGS->mTMatrix);
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
  PushState();

  return mCurrentSurface->Lock(aX, aY, aWidth, aHeight,
  			aBits, aStride, aWidthBytes, aFlags);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::UnlockDrawingSurface(void)
{
  PRBool  clipstate;
  PopState(clipstate);

  mCurrentSurface->Unlock();
  
  return NS_OK;
}

//------------------------------------------------------------------------


NS_IMETHODIMP nsRenderingContextMac::SelectOffScreenDrawingSurface(nsDrawingSurface aSurface)
{  
	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);

	if (surface != nsnull)
		SelectDrawingSurface(surface);				// select the offscreen surface...
	else
		SelectDrawingSurface(mFrontSurface);		// ...or get back to the window port

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetDrawingSurface(nsDrawingSurface *aSurface)
{  
	*aSurface = mCurrentSurface;
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::CopyOffScreenBits(nsDrawingSurface aSrcSurf,
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
	nsDrawingSurfaceMac* srcSurface = static_cast<nsDrawingSurfaceMac*>(aSrcSurf);
	GrafPtr srcPort;
	srcSurface->GetGrafPtr(&srcPort);

	// apply the selected transformations
	PRInt32 x = aSrcX;
	PRInt32 y = aSrcY;
	if (aCopyFlags & NS_COPYBITS_XFORM_SOURCE_VALUES)
		mGS->mTMatrix.TransformCoord(&x, &y);

	nsRect dstRect = aDestBounds;
	if (aCopyFlags & NS_COPYBITS_XFORM_DEST_VALUES)
		mGS->mTMatrix.TransformCoord(&dstRect.x, &dstRect.y, &dstRect.width, &dstRect.height);

	// get the source and destination rectangles
	Rect macSrcRect, macDstRect;
	::SetRect(&macSrcRect, x, y, x + dstRect.width, y + dstRect.height);
	::SetRect(&macDstRect, dstRect.x, dstRect.y, dstRect.x + dstRect.width, dstRect.y + dstRect.height);
  
	// get the source clip region
	StRegionFromPool clipRgn;
	if (!clipRgn) return NS_ERROR_OUT_OF_MEMORY;
		
	if (aCopyFlags & NS_COPYBITS_USE_SOURCE_CLIP_REGION) {
		::GetPortClipRegion(srcPort, clipRgn);
	} else
		::CopyRgn(mGS->mMainRegion, clipRgn);

	// get the destination port and surface
	GrafPtr destPort;
	nsDrawingSurfaceMac* destSurface;
	if (aCopyFlags & NS_COPYBITS_TO_BACK_BUFFER) {
		destSurface	= mCurrentSurface;
		destPort		= mPort;
		NS_ASSERTION((destPort != nsnull), "no back buffer");
	} else {
		destSurface	= mFrontSurface;
		mFrontSurface->GetGrafPtr(&destPort);
	}

	// select the destination surface to set the colors
	nsDrawingSurfaceMac* saveSurface = nsnull;
	if (mCurrentSurface != destSurface) {
		saveSurface = mCurrentSurface;
		SelectDrawingSurface(destSurface);
	}

	// make sure the current port is correct. where else does this need to be done?
	StPortSetter setter(destPort);

	// set the right colors for CopyBits
	RGBColor foreColor;
	Boolean changedForeColor = false;
	::GetForeColor(&foreColor);
	if ((foreColor.red != 0x0000) || (foreColor.green != 0x0000) || (foreColor.blue != 0x0000)) {
		RGBColor rgbBlack = {0x0000,0x0000,0x0000};
		::RGBForeColor(&rgbBlack);
		changedForeColor = true;
	}

	RGBColor backColor;
	Boolean changedBackColor = false;
	::GetBackColor(&backColor);
	if ((backColor.red != 0xFFFF) || (backColor.green != 0xFFFF) || (backColor.blue != 0xFFFF)) {
		RGBColor rgbWhite = {0xFFFF,0xFFFF,0xFFFF};
		::RGBBackColor(&rgbWhite);
		changedBackColor = true;
	}

	// copy the bits now
	::CopyBits(
#if TARGET_CARBON
          ::GetPortBitMapForCopyBits(srcPort),
          ::GetPortBitMapForCopyBits(destPort),
#else
		  &srcPort->portBits,
		  &destPort->portBits,
#endif
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

NS_IMETHODIMP nsRenderingContextMac::CreateDrawingSurface(nsRect *aBounds, PRUint32 aSurfFlags, nsDrawingSurface &aSurface)
{
	aSurface = nsnull;

	PRUint32 depth = 8;
	if (mContext)
		mContext->GetDepth(depth);

	// get rect
	Rect macRect;
	if (aBounds != nsnull) {
  		// fyi, aBounds->x and aBounds->y are always 0 here
  		::SetRect(&macRect, aBounds->x, aBounds->y, aBounds->XMost(), aBounds->YMost());
	} else
		::SetRect(&macRect, 0, 0, 2, 2);

	nsDrawingSurfaceMac* surface = new nsDrawingSurfaceMac();
	if (!surface)
		return NS_ERROR_OUT_OF_MEMORY;

	nsresult rv = surface->Init(depth, macRect.right, macRect.bottom, aSurfFlags);
	if (NS_SUCCEEDED(rv))
		aSurface = surface;
	else
		delete surface;

	return rv;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DestroyDrawingSurface(nsDrawingSurface aSurface)
{
	if (!aSurface)
		return NS_ERROR_FAILURE;

	// if that surface is still the current one, select the front surface

	if (aSurface == mCurrentSurface)
		SelectDrawingSurface(mFrontSurface);

	// delete the offscreen
	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);

/* The dtor does this...
	GWorldPtr offscreenGWorld;
	surface->GetGrafPtr(reinterpret_cast<GrafPtr*>(&offscreenGWorld));
	::UnlockPixels(::GetGWorldPixMap(offscreenGWorld));
	::DisposeGWorld(offscreenGWorld);
*/
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
#ifdef IBMBIDI
  // QuickDraw can handle arabic and hebrew drawing
  result |= NS_RENDERING_HINT_BIDI_REORDERING;
  result |= NS_RENDERING_HINT_ARABIC_SHAPING;
#endif
	aResult = result;
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::Reset(void)
{
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetDeviceContext(nsIDeviceContext *&aContext)
{
	if (mContext) {
		aContext = mContext;
		NS_ADDREF(aContext);
	} else {
		aContext = nsnull;
	}
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::IsVisibleRect(const nsRect& aRect, PRBool &aVisible)
{
	aVisible = PR_TRUE;
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SetClipRect(const nsRect& aRect, nsClipCombine aCombine, PRBool &aClipEmpty)
{
	nsRect  trect = aRect;

	mGS->mTMatrix.TransformCoord(&trect.x, &trect.y, &trect.width, &trect.height);

	Rect macRect;
	::SetRect(&macRect, trect.x, trect.y, trect.x + trect.width, trect.y + trect.height);

	RgnHandle rectRgn = sNativeRegionPool.GetNewRegion();
	RgnHandle clipRgn = mGS->mClipRegion;
	if (!clipRgn || !rectRgn)
		return NS_ERROR_OUT_OF_MEMORY;

	::RectRgn(rectRgn, &macRect);

	switch (aCombine) {
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
//  	::CopyRgn(rectRgn, clipRgn);
		::SectRgn(rectRgn, mGS->mMainRegion, clipRgn);
		break;
	}
	sNativeRegionPool.ReleaseRegion(rectRgn);

	{
		StPortSetter setter(mPort);
		::SetClip(clipRgn);
	}

	mGS->mClipRegion = clipRgn;
	aClipEmpty = ::EmptyRgn(clipRgn);
	
	// note that the clipping changed.
	mChanges |= kClippingChanged;

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetClipRect(nsRect &aRect, PRBool &aClipValid)
{
	Rect	cliprect;

	if (mGS->mClipRegion != nsnull) {
		::GetRegionBounds(mGS->mClipRegion, &cliprect);
		aRect.SetRect(cliprect.left, cliprect.top, cliprect.right - cliprect.left, cliprect.bottom - cliprect.top);
		aClipValid = PR_TRUE;
 	} else {
		aRect.SetRect(0,0,0,0);
		aClipValid = PR_FALSE;
 	}

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SetClipRegion(const nsIRegion& aRegion, nsClipCombine aCombine, PRBool &aClipEmpty)
{
	RgnHandle regionH;
	aRegion.GetNativeRegion((void*&)regionH);

	RgnHandle clipRgn = mGS->mClipRegion;
	if (!clipRgn) return NS_ERROR_OUT_OF_MEMORY;

	switch (aCombine) {
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
//  	::CopyRgn(regionH, clipRgn);
		::SectRgn(regionH, mGS->mMainRegion, clipRgn);
		break;
	}

		
	{
		StPortSetter setter(mPort);
		::SetClip(clipRgn);
	}

	mGS->mClipRegion = clipRgn;
	aClipEmpty = ::EmptyRgn(clipRgn);

	// note that the clipping changed.
	mChanges |= kClippingChanged;

	return NS_OK;
}

/**
 * Fills in |aRegion| with a copy of the current clip region.
 */
NS_IMETHODIMP nsRenderingContextMac::CopyClipRegion(nsIRegion &aRegion)
{
	nsRegionMac* macRegion = (nsRegionMac*)&aRegion;
	macRegion->SetNativeRegion(mGS->mClipRegion);
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetClipRegion(nsIRegion **aRegion)
{
	nsresult  rv = NS_OK;

	NS_ASSERTION(!(nsnull == aRegion), "no region ptr");

	if (nsnull == *aRegion) {
		nsRegionMac *rgn = new nsRegionMac();

		if (nsnull != rgn) {
			NS_ADDREF(rgn);
			rv = rgn->Init();

			if (NS_OK != rv)
				NS_RELEASE(rgn);
			else
				*aRegion = rgn;
		} else
			rv = NS_ERROR_OUT_OF_MEMORY;
	}

	if (rv == NS_OK) {
		nsRegionMac* macRegion = *(nsRegionMac**)aRegion;
		macRegion->SetNativeRegion(mGS->mClipRegion);
	}

	return rv;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SetColor(nscolor aColor)
{
	StPortSetter setter(mPort);

	#define COLOR8TOCOLOR16(color8)	 ((color8 << 8) | color8)

	RGBColor color;
	color.red = COLOR8TOCOLOR16(NS_GET_R(aColor));
	color.green = COLOR8TOCOLOR16(NS_GET_G(aColor));
	color.blue = COLOR8TOCOLOR16(NS_GET_B(aColor));
	::RGBForeColor(&color);
	mGS->mColor = aColor ;

	mChanges |= kColorChanged;
  	
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetColor(nscolor &aColor) const
{
	aColor = mGS->mColor;
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SetLineStyle(nsLineStyle aLineStyle)
{
	// note: the line style must be saved in the nsGraphicState like font, color, etc...
  mLineStyle = aLineStyle;
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetLineStyle(nsLineStyle &aLineStyle)
{
  aLineStyle = mLineStyle;
	return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SetFont(const nsFont& aFont)
{
	NS_IF_RELEASE(mGS->mFontMetrics);

	if (mContext)
		mContext->GetMetricsFor(aFont, mGS->mFontMetrics);
		
	mChanges |= kFontChanged;
		
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::SetFont(nsIFontMetrics *aFontMetrics)
{
	NS_IF_RELEASE(mGS->mFontMetrics);
	mGS->mFontMetrics = aFontMetrics;
	NS_IF_ADDREF(mGS->mFontMetrics);
	mChanges |= kFontChanged;
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetFontMetrics(nsIFontMetrics *&aFontMetrics)
{
	NS_IF_ADDREF(mGS->mFontMetrics);
	aFontMetrics = mGS->mFontMetrics;
	return NS_OK;
}

//------------------------------------------------------------------------

// add the passed in translation to the current translation
NS_IMETHODIMP nsRenderingContextMac::Translate(nscoord aX, nscoord aY)
{
	mGS->mTMatrix.AddTranslation((float)aX,(float)aY);
	return NS_OK;
}

//------------------------------------------------------------------------

// add the passed in scale to the current scale
NS_IMETHODIMP nsRenderingContextMac::Scale(float aSx, float aSy)
{
	mGS->mTMatrix.AddScale(aSx, aSy);
	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetCurrentTransform(nsTransform2D *&aTransform)
{
	aTransform = &mGS->mTMatrix;
	return NS_OK;
}


#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
	StPortSetter setter(mPort);

	mGS->mTMatrix.TransformCoord(&aX0,&aY0);
	mGS->mTMatrix.TransformCoord(&aX1,&aY1);

	// make the line one pixel shorter to match other platforms
	nscoord diffX = aX1 - aX0;
	if (diffX)
		diffX -= (diffX > 0 ? 1 : -1);

	nscoord diffY = aY1 - aY0;
	if (diffY)
		diffY -= (diffY > 0 ? 1 : -1);

	// draw line
	::MoveTo(aX0, aY0);
	::Line(diffX, diffY);

	return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawStdLine(nscoord aX0, nscoord aY0, nscoord aX1, nscoord aY1)
{
	StPortSetter setter(mPort);

	// make the line one pixel shorter to match other platforms
	nscoord diffX = aX1 - aX0;
	if (diffX)
		diffX -= (diffX > 0 ? 1 : -1);

	nscoord diffY = aY1 - aY0;
	if (diffY)
		diffY -= (diffY > 0 ? 1 : -1);

	// draw line
	::MoveTo(aX0, aY0);
	::Line(diffX, diffY);

	return NS_OK;
}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawPolyline(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	StPortSetter setter(mPort);

	PRUint32   i;
	PRInt32    x,y;

	x = aPoints[0].x;
	y = aPoints[0].y;
	mGS->mTMatrix.TransformCoord((PRInt32*)&x,(PRInt32*)&y);
	::MoveTo(x,y);

	for (i = 1; i < aNumPoints; i++){
		x = aPoints[i].x;
		y = aPoints[i].y;

		mGS->mTMatrix.TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
	}

	return NS_OK;
}

	/**
	 * A note about Quickdraw coordinates:  When Apple designed Quickdraw, signed 16-bit coordinates
	 * were considered to be large enough. Although computer displays with > 65535 pixels in either
	 * dimension are still far off, most modern graphics systems allow at least 32-bit signed coordinates
	 * so we have to take extra care when converting from GFX coordinates to Quickdraw coordinates,
	 * especially when constructing rectangles.
	 */

	inline short pinToShort(nscoord value)
	{
		if (value < -32768)
			return -32768;
		if (value > 32767)
			return 32767;
		return (short) value;
	}


//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawRect(const nsRect& aRect)
{
	return DrawRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	StPortSetter setter(mPort);
	
	nscoord x,y,w,h;
	Rect	therect;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	mGS->mTMatrix.TransformCoord(&x, &y, &w, &h);
	::SetRect(&therect, pinToShort(x), pinToShort(y), pinToShort(x + w), pinToShort(y + h));
	::FrameRect(&therect);

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillRect(const nsRect& aRect)
{
	return FillRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	StPortSetter setter(mPort);

	nscoord x,y,w,h;
	Rect	therect;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	// TODO - cps - must debug and fix this 
	mGS->mTMatrix.TransformCoord(&x, &y, &w, &h);
	::SetRect(&therect, pinToShort(x), pinToShort(y), pinToShort(x + w), pinToShort(y + h));
	::PaintRect(&therect);

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	StPortSetter setter(mPort);

	PRUint32   i;
	PolyHandle thepoly;
	PRInt32    x,y;

	thepoly = ::OpenPoly();
	if (nsnull == thepoly) {
		return NS_ERROR_OUT_OF_MEMORY;
	}

	x = aPoints[0].x;
	y = aPoints[0].y;
	mGS->mTMatrix.TransformCoord((PRInt32*)&x,(PRInt32*)&y);
	::MoveTo(x,y);

	for (i = 1; i < aNumPoints; i++) {
		x = aPoints[i].x;
		y = aPoints[i].y;
		
		mGS->mTMatrix.TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
	}

	::ClosePoly();

	::FramePoly(thepoly);
	::KillPoly(thepoly);

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillPolygon(const nsPoint aPoints[], PRInt32 aNumPoints)
{
	StPortSetter setter(mPort);

	PRUint32   i;
	PolyHandle thepoly;
	PRInt32    x,y;

	thepoly = ::OpenPoly();
	if (nsnull == thepoly) {
		return NS_ERROR_OUT_OF_MEMORY;
	}

	x = aPoints[0].x;
	y = aPoints[0].y;
	mGS->mTMatrix.TransformCoord((PRInt32*)&x,(PRInt32*)&y);
	::MoveTo(x,y);

	for (i = 1; i < aNumPoints; i++) {
		x = aPoints[i].x;
		y = aPoints[i].y;
		mGS->mTMatrix.TransformCoord((PRInt32*)&x,(PRInt32*)&y);
		::LineTo(x,y);
	}

	::ClosePoly();
	::PaintPoly(thepoly);
	::KillPoly(thepoly);

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawEllipse(const nsRect& aRect)
{
	return DrawEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	StPortSetter setter(mPort);

	nscoord x,y,w,h;
	Rect    therect;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	mGS->mTMatrix.TransformCoord(&x,&y,&w,&h);
	::SetRect(&therect, pinToShort(x), pinToShort(y), pinToShort(x + w), pinToShort(y + h));
	::FrameOval(&therect);

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillEllipse(const nsRect& aRect)
{
	return FillEllipse(aRect.x, aRect.y, aRect.width, aRect.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillEllipse(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	StPortSetter setter(mPort);

	nscoord x,y,w,h;
	Rect    therect;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	mGS->mTMatrix.TransformCoord(&x,&y,&w,&h);
	::SetRect(&therect, pinToShort(x), pinToShort(y), pinToShort(x + w), pinToShort(y + h));
	::PaintOval(&therect);

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
	return DrawArc(aRect.x,aRect.y,aRect.width,aRect.height,aStartAngle,aEndAngle);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
	StPortSetter setter(mPort);

	nscoord x,y,w,h;
	Rect    therect;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	mGS->mTMatrix.TransformCoord(&x,&y,&w,&h);
	::SetRect(&therect, pinToShort(x), pinToShort(y), pinToShort(x + w), pinToShort(y + h));
	::FrameArc(&therect,aStartAngle,aEndAngle);

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillArc(const nsRect& aRect,
                                 float aStartAngle, float aEndAngle)
{
	return FillArc(aRect.x, aRect.y, aRect.width, aRect.height, aStartAngle, aEndAngle);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::FillArc(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight,
                                 float aStartAngle, float aEndAngle)
{
	StPortSetter setter(mPort);

	nscoord x,y,w,h;
	Rect	therect;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	mGS->mTMatrix.TransformCoord(&x,&y,&w,&h);
	::SetRect(&therect, pinToShort(x), pinToShort(y), pinToShort(x + w), pinToShort(y + h));
	::PaintArc(&therect,aStartAngle,aEndAngle);

	return NS_OK;
}

#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetWidth(char ch, nscoord &aWidth)
{
	if (ch == ' ' && mGS->mFontMetrics) {
		nsFontMetricsMac* fontMetricsMac = static_cast<nsFontMetricsMac*>(mGS->mFontMetrics);
		return fontMetricsMac->GetSpaceWidth(aWidth);
	}

	char buf[1];
	buf[0] = ch;
	return GetWidth(buf, 1, aWidth);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetWidth(PRUnichar ch, nscoord &aWidth, PRInt32 *aFontID)
{
	if (ch == ' ' && mGS->mFontMetrics) {
		nsFontMetricsMac* fontMetricsMac = static_cast<nsFontMetricsMac*>(mGS->mFontMetrics);
		return fontMetricsMac->GetSpaceWidth(aWidth);
	}

	PRUnichar buf[1];
	buf[0] = ch;
	return GetWidth(buf, 1, aWidth, aFontID);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetWidth(const nsString& aString, nscoord &aWidth, PRInt32 *aFontID)
{
	return GetWidth(aString.get(), aString.Length(), aWidth, aFontID);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetWidth(const char *aString, nscoord &aWidth)
{
	return GetWidth(aString, strlen(aString), aWidth);
}

//------------------------------------------------------------------------

NS_IMETHODIMP
nsRenderingContextMac::GetWidth(const char* aString, PRUint32 aLength, nscoord& aWidth)
{
	StPortSetter setter(mPort);

	// set native font and attributes
	SetPortTextState();

//   below is a bad assert, aString is not guaranteed null terminated...
//    NS_ASSERTION(strlen(aString) >= aLength, "Getting width on garbage string");
  
	// measure text
	short textWidth = ::TextWidth(aString, 0, aLength);
	aWidth = NSToCoordRound(float(textWidth) * mP2T);

	return NS_OK;
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::GetWidth(const PRUnichar *aString, PRUint32 aLength, nscoord &aWidth, PRInt32 *aFontID)
{
	StPortSetter setter(mPort);
	
 	nsresult rv = SetPortTextState();
 	if (NS_FAILED(rv))
 		return rv;

	NS_PRECONDITION(mGS->mFontMetrics != nsnull, "No font metrics in SetPortTextState");
	
	if (nsnull == mGS->mFontMetrics)
 		return NS_ERROR_NULL_POINTER;

#ifdef IBMBIDI
	rv = mUnicodeRenderingToolkit.PrepareToDraw(mP2T, mContext, mGS,mPort, mRightToLeftText);
#else
	rv = mUnicodeRenderingToolkit.PrepareToDraw(mP2T, mContext, mGS,mPort);
#endif
	if (NS_SUCCEEDED(rv))
    	rv = mUnicodeRenderingToolkit.GetWidth(aString, aLength, aWidth, aFontID);
    
	return rv;
}



#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawString(const char *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY,
                                         const nscoord* aSpacing)
{
	StPortSetter setter(mPort);

	PRInt32 x = aX;
	PRInt32 y = aY;
	
	if (mGS->mFontMetrics) {
		// set native font and attributes
		SetPortTextState();

		// substract ascent since drawing specifies baseline
		nscoord ascent = 0;
		mGS->mFontMetrics->GetMaxAscent(ascent);
		y += ascent;
	}

	mGS->mTMatrix.TransformCoord(&x,&y);

	::MoveTo(x,y);
	if ( aSpacing == NULL )
		::DrawText(aString,0,aLength);
	else
	{
		int buffer[STACK_TREASHOLD];
		int* spacing = (aLength <= STACK_TREASHOLD ? buffer : new int[aLength]);
		if (spacing)
		{
			mGS->mTMatrix.ScaleXCoords(aSpacing, aLength, spacing);
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
			return NS_ERROR_OUT_OF_MEMORY;
	}

	return NS_OK;
}




//------------------------------------------------------------------------
NS_IMETHODIMP nsRenderingContextMac::DrawString(const PRUnichar *aString, PRUint32 aLength,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{
	StPortSetter setter(mPort);

 	nsresult rv = SetPortTextState();
 	if (NS_FAILED(rv))
 		return rv;

	NS_PRECONDITION(mGS->mFontMetrics != nsnull, "No font metrics in SetPortTextState");
	
	if (nsnull == mGS->mFontMetrics)
		return NS_ERROR_NULL_POINTER;

#ifdef IBMBIDI	
	rv = mUnicodeRenderingToolkit.PrepareToDraw(mP2T, mContext, mGS,mPort,mRightToLeftText);
#else
	rv = mUnicodeRenderingToolkit.PrepareToDraw(mP2T, mContext, mGS,mPort);
#endif
	if (NS_SUCCEEDED(rv))
		rv = mUnicodeRenderingToolkit.DrawString(aString, aLength, aX, aY, aFontID, aSpacing);

	return rv;        
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawString(const nsString& aString,
                                         nscoord aX, nscoord aY, PRInt32 aFontID,
                                         const nscoord* aSpacing)
{
 	return DrawString(aString.get(), aString.Length(), aX, aY, aFontID, aSpacing);
}


#pragma mark -
//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY)
{
	nscoord width,height;

	width = NSToCoordRound(mP2T * aImage->GetWidth());
	height = NSToCoordRound(mP2T * aImage->GetHeight());

	return DrawImage(aImage,aX,aY,width,height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawImage(nsIImage *aImage, nscoord aX, nscoord aY,
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

NS_IMETHODIMP nsRenderingContextMac::DrawImage(nsIImage *aImage, const nsRect& aSRect, const nsRect& aDRect)
{
	StPortSetter setter(mPort);

	nsRect sr = aSRect;
	nsRect dr = aDRect;
	mGS->mTMatrix.TransformCoord(&sr.x,&sr.y,&sr.width,&sr.height);
	sr.x -= mTranMatrix->GetXTranslationCoord();
	sr.y -= mTranMatrix->GetYTranslationCoord();
	mGS->mTMatrix.TransformCoord(&dr.x,&dr.y,&dr.width,&dr.height);

	return aImage->Draw(*this, mCurrentSurface, sr.x, sr.y, sr.width, sr.height,
						dr.x, dr.y, dr.width, dr.height);
}

//------------------------------------------------------------------------

NS_IMETHODIMP nsRenderingContextMac::DrawImage(nsIImage *aImage, const nsRect& aRect)
{
	StPortSetter setter(mPort);

	nsRect tr = aRect;
	mGS->mTMatrix.TransformCoord(&tr.x,&tr.y,&tr.width,&tr.height);
  
	return aImage->Draw(*this, mCurrentSurface, tr.x, tr.y, tr.width, tr.height);
}

#if 0
/** ---------------------------------------------------
 *  See documentation in nsIRenderingContext.h
 *	@update 3/16/00 dwc
 */
NS_IMETHODIMP 
nsRenderingContextMac::DrawTile(nsIImage *aImage,nscoord aX0,nscoord aY0,nscoord aX1,nscoord aY1,
                                                    nscoord aWidth,nscoord aHeight)
{

  return NS_OK;
}
#endif


NS_IMETHODIMP nsRenderingContextMac::RetrieveCurrentNativeGraphicData(PRUint32 * ngd)
{
  return NS_OK;
}

NS_IMETHODIMP nsRenderingContextMac::InvertRect(const nsRect& aRect)
{
	return InvertRect(aRect.x, aRect.y, aRect.width, aRect.height);
}

NS_IMETHODIMP nsRenderingContextMac::InvertRect(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
	StPortSetter setter(mPort);

	nscoord x,y,w,h;
	Rect	therect;

	x = aX;
	y = aY;
	w = aWidth;
	h = aHeight;

	mGS->mTMatrix.TransformCoord(&x, &y, &w, &h);
	::SetRect(&therect, pinToShort(x), pinToShort(y), pinToShort(x + w), pinToShort(y + h));
	::InvertRect(&therect);

	return NS_OK;
}

NS_IMETHODIMP nsRenderingContextMac::GetGraphics(nsIGraphics* *aGraphics)
{
	NS_ASSERTION((aGraphics != nsnull), "aGraphics is NULL");
	nsCOMPtr<nsIGraphics> graphics = new nsGraphicsImpl(this);
	if (graphics)
		return graphics->QueryInterface(NS_GET_IID(nsIGraphics), (void**)aGraphics);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

#ifdef MOZ_MATHML

NS_IMETHODIMP
nsRenderingContextMac::GetBoundingMetrics(const char*        aString, 
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsRenderingContextMac::GetBoundingMetrics(const PRUnichar*   aString, 
                                          PRUint32           aLength,
                                          nsBoundingMetrics& aBoundingMetrics,
                                          PRInt32*           aFontID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


#endif /* MOZ_MATHML */


#ifdef IBMBIDI
NS_IMETHODIMP
nsRenderingContextMac::SetRightToLeftText(PRBool aIsRTL)
{
  mRightToLeftText = aIsRTL;
	return NS_OK;
}
#endif

