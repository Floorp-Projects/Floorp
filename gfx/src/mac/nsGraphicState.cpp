/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK ***** */

#include "nsGraphicState.h"
#include "nsDrawingSurfaceMac.h"
#include "nsRegionMac.h"
#include "nsIFontMetrics.h"
#include "nsCarbonHelpers.h"
#include "nsRegionPool.h"

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

void nsGraphicState::Init(nsIDrawingSurface* aSurface)
{
	// retrieve the grafPort
	nsDrawingSurfaceMac* surface = static_cast<nsDrawingSurfaceMac*>(aSurface);
	CGrafPtr port;
	surface->GetGrafPtr(&port);

	// init from grafPort
	Init(port);
}

//------------------------------------------------------------------------

void nsGraphicState::Init(CGrafPtr aPort)
{
	// delete old values
	Clear();

	// init from grafPort (usually an offscreen port)
	RgnHandle	rgn = sNativeRegionPool.GetNewRegion(); //::NewRgn();
	if ( rgn ) {
		Rect bounds;
		::RectRgn(rgn, ::GetPortBounds(aPort, &bounds));
	}

  Rect    portBounds;
  ::GetPortBounds(aPort, &portBounds);
  mOffx = -portBounds.left;
  mOffy = -portBounds.top;
  
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
