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
#include "nsTransform2D.h"
#include "nsRegionMac.h"


nsGraphicStatePool sGraphicStatePool;


//------------------------------------------------------------------------

nsGraphicStatePool::nsGraphicStatePool()
{
	for (short i = 0; i < kGSPoolCount; i ++)
	{
		mGSArray[i].mGS = new nsGraphicState();
		mGSArray[i].mFree = PR_TRUE;
	}
}

//------------------------------------------------------------------------

nsGraphicStatePool::~nsGraphicStatePool()
{
	for (short i = 0; i < kGSPoolCount; i ++)
	{
		delete mGSArray[i].mGS;
	}
}

//------------------------------------------------------------------------

nsGraphicState* nsGraphicStatePool::GetNewGS()
{
	for (short i = 0; i < kGSPoolCount; i ++)
	{
		if (mGSArray[i].mFree)
		{
			mGSArray[i].mFree = PR_FALSE;
			return mGSArray[i].mGS;
		}
	}
	return (new nsGraphicState());	// we overflew the pool: return a new GraphicState
}

//------------------------------------------------------------------------

void nsGraphicStatePool::ReleaseGS(nsGraphicState* aGS)
{
	for (short i = 0; i < kGSPoolCount; i ++)
	{
		if (mGSArray[i].mGS == aGS)
		{
			mGSArray[i].mGS->Clear();
			mGSArray[i].mFree = PR_TRUE;
			return;
		}
	}
	delete aGS;	// we overflew the pool: delete the GraphicState
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
	if (mTMatrix)
	{
		delete mTMatrix;
		mTMatrix = nsnull;
	}

	if (mMainRegion)
	{
		sNativeRegionPool.ReleaseRegion(mMainRegion); //::DisposeRgn(mMainRegion);
		mMainRegion = nsnull;
	}

	if (mClipRegion)
	{
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
	// delete old values
	Clear();

	// copy new ones
	if (aGS->mTMatrix)
		mTMatrix = new nsTransform2D(aGS->mTMatrix);
	else
		mTMatrix = nsnull;

	mOffx						= aGS->mOffx;
	mOffy						= aGS->mOffy;

	mMainRegion					= DuplicateRgn(aGS->mMainRegion);
	mClipRegion					= DuplicateRgn(aGS->mClipRegion);

	mColor					= aGS->mColor;
	mFont						= aGS->mFont;
	mFontMetrics		= aGS->mFontMetrics;
	NS_IF_ADDREF(mFontMetrics);

	mCurrFontHandle	= aGS->mCurrFontHandle;
}


//------------------------------------------------------------------------

RgnHandle nsGraphicState::DuplicateRgn(RgnHandle aRgn)
{
	RgnHandle dupRgn = nsnull;
	if (aRgn)
	{
		dupRgn = sNativeRegionPool.GetNewRegion(); //::NewRgn();
		if (dupRgn)
			::CopyRgn(aRgn, dupRgn);
	}
	return dupRgn;
}

