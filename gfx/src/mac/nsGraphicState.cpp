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

#include "nsGraphicState.h"
#include "nsDrawingSurfaceMac.h"
#include "nsRegionMac.h"
#include "nsIFontMetrics.h"

nsGraphicStatePool sGraphicStatePool;


//------------------------------------------------------------------------

nsGraphicStatePool::nsGraphicStatePool()
{
	mFreeList = nsnull;
}

//------------------------------------------------------------------------

nsGraphicStatePool::~nsGraphicStatePool()
{
	nsGraphicState* gs = mFreeList;
	while (gs != nsnull) {
		nsGraphicState* next = gs->mNext;
		delete gs;
		gs = next;
	}
}

//------------------------------------------------------------------------

nsGraphicState* nsGraphicStatePool::GetNewGS()
{
	nsGraphicState* gs = mFreeList;
	if (gs != nsnull) {
		mFreeList = gs->mNext;
		return gs;
	}
	return new nsGraphicState;
}

//------------------------------------------------------------------------

void nsGraphicStatePool::ReleaseGS(nsGraphicState* aGS)
{
	// clear the graphics state? this will cause its transformation matrix and regions
	// to be released. I'm dubious about that. in fact, shouldn't the matrix always be
	// a member object of the graphics state? why have a separate allocation at all?
	aGS->Clear();
	aGS->mNext = mFreeList;
	mFreeList = aGS;
}


#pragma mark -
//------------------------------------------------------------------------

nsGraphicState::nsGraphicState()
{
	// everything is initialized to 0 through the 'new' operator
}

//------------------------------------------------------------------------

nsGraphicState::~nsGraphicState()
{
	Clear();
}

//------------------------------------------------------------------------

void nsGraphicState::Clear()
{
	mTMatrix.SetToIdentity();

	if (mMainRegion) {
		sNativeRegionPool.ReleaseRegion(mMainRegion); //::DisposeRgn(mMainRegion);
		mMainRegion = nsnull;
	}

	if (mClipRegion) {
		sNativeRegionPool.ReleaseRegion(mClipRegion); //::DisposeRgn(mClipRegion);
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

void nsGraphicState::Init(nsDrawingSurface aSurface)
{
	// retrieve the grafPort
	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
	GrafPtr port;
	surface->GetGrafPtr(&port);

	// init from grafPort
	Init(port);
}

//------------------------------------------------------------------------

void nsGraphicState::Init(GrafPtr aPort)
{
	// delete old values
	Clear();

	// init from grafPort (usually an offscreen port)
	RgnHandle	rgn = sNativeRegionPool.GetNewRegion(); //::NewRgn();
#if TARGET_CARBON
	if ( rgn ) {
		Rect bounds;
		::RectRgn(rgn, ::GetPortBounds(aPort, &bounds));
	}
#else
	if (rgn)
	  ::RectRgn(rgn, &aPort->portRect);
#endif

  mMainRegion					= rgn;
  mClipRegion					= DuplicateRgn(rgn);
}

//------------------------------------------------------------------------

void nsGraphicState::Init(nsIWidget* aWindow)
{
	// delete old values
	Clear();

	// init from widget
  mOffx						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETX);
  mOffy						= (PRInt32)aWindow->GetNativeData(NS_NATIVE_OFFSETY);

	RgnHandle widgetRgn = (RgnHandle)aWindow->GetNativeData(NS_NATIVE_REGION);
	mMainRegion					= DuplicateRgn(widgetRgn);
  mClipRegion					= DuplicateRgn(widgetRgn);
}

//------------------------------------------------------------------------

void nsGraphicState::Duplicate(nsGraphicState* aGS)
{
	// clear old values? no need, just copying them any way.
	// Clear();

	// copy new ones
	mTMatrix.SetMatrix(&aGS->mTMatrix);

	mOffx						= aGS->mOffx;
	mOffy						= aGS->mOffy;

	mMainRegion			= DuplicateRgn(aGS->mMainRegion, mMainRegion);
	mClipRegion			= DuplicateRgn(aGS->mClipRegion, mClipRegion);

	mColor					= aGS->mColor;
	mFont						= aGS->mFont;
	
	NS_IF_RELEASE(mFontMetrics);
	mFontMetrics		= aGS->mFontMetrics;
	NS_IF_ADDREF(mFontMetrics);

	mCurrFontHandle	= aGS->mCurrFontHandle;
	
	mChanges				= aGS->mChanges;
}


//------------------------------------------------------------------------

RgnHandle nsGraphicState::DuplicateRgn(RgnHandle aRgn, RgnHandle aDestRgn)
{
	RgnHandle dupRgn = aDestRgn;
	if (aRgn)	{
		if (nsnull == dupRgn)
			dupRgn = sNativeRegionPool.GetNewRegion(); //::NewRgn();
		if (dupRgn)
			::CopyRgn(aRgn, dupRgn);
	}
	return dupRgn;
}
