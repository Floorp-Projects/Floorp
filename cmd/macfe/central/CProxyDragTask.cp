/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

//	CProxyDragTask.cp


#include "CProxyDragTask.h"

#include <LDragAndDrop.h>
#include <UException.h>
#include <UTextTraits.h>
#include <UGAColorRamp.h>
#include <LView.h>

#include "CProxyPane.h"
#include "StCaptureView.h"
#include "CGWorld.h"
#include "StRegionHandle.h"
#include "CEnvironment.h"

// ---------------------------------------------------------------------------
//		¥ CProxyDragTask
// ---------------------------------------------------------------------------

CProxyDragTask::CProxyDragTask(
	LView&					inProxyView,
	CProxyPane&				inProxyPane,
	LCaption&				inPageProxyCaption,
	const EventRecord&		inEventRecord,
	CExtraFlavorAdder*		inExtraFlavorAdder)
	
	:	mProxyView(inProxyView),
		mProxyPane(inProxyPane),
		mPageProxyCaption(inPageProxyCaption),
		mExtraFlavorAdder(inExtraFlavorAdder),

		Inherited(inEventRecord)
{
}

// ---------------------------------------------------------------------------
//		¥ ~CProxyDragTask
// ---------------------------------------------------------------------------

CProxyDragTask::~CProxyDragTask()
{
	delete mExtraFlavorAdder;
}

// ---------------------------------------------------------------------------
//		¥ DoDrag
// ---------------------------------------------------------------------------

OSErr
CProxyDragTask::DoDrag()
{
	MakeDragRegion(mDragRef, mDragRegion);
	AddFlavors(mDragRef);
	
	if (UEnvironment::HasFeature(env_HasDragMgrImageSupport))
	{
		try
		{
			DoTranslucentDrag();
		}
		catch (...)
		{
			DoNormalDrag();
		}
	}
	else
	{
		DoNormalDrag();
	}
	
	return noErr;
}

// ---------------------------------------------------------------------------
//		¥ DoNormalDrag
// ---------------------------------------------------------------------------

void
CProxyDragTask::DoNormalDrag()
{
	::TrackDrag(mDragRef, &mEventRecord, mDragRegion);
}

// ---------------------------------------------------------------------------
//		¥ DoTranslucentDrag
// ---------------------------------------------------------------------------

void
CProxyDragTask::DoTranslucentDrag()
{
	Rect				theFrame;
	StColorPortState	theColorPortState(mProxyView.GetMacPort());

	// Normalize the color state (to make CopyBits happy)
	
	StColorState::Normalize();
	
	// Build a GWorld containing the page proxy icon and title

	mProxyView.FocusDraw();
		
	mProxyView.CalcLocalFrameRect(theFrame);

	CGWorld theGWorld(theFrame, 0, useTempMem);
	StCaptureView theCaptureView(mProxyView);
	
	mPageProxyCaption.Show();

	try
	{
		theCaptureView.Capture(theGWorld);

		mProxyView.FocusDraw();

	   	Point theOffsetPoint = topLeft(theFrame);
		::LocalToGlobal(&theOffsetPoint);

		// Set the drag image

		StRegionHandle theTrackMask;

		mProxyPane.CalcLocalFrameRect(theFrame);
		ThrowIfOSErr_(::IconSuiteToRgn(theTrackMask, &theFrame, kAlignAbsoluteCenter, mProxyPane.GetIconSuiteH()));
		
		mPageProxyCaption.CalcLocalFrameRect(theFrame); // Use frame which bounds the actual text, not the frame bounds
		theTrackMask += theFrame;
		
		PixMapHandle theMap = ::GetGWorldPixMap(theGWorld.GetMacGWorld());
		OSErr theErr = ::SetDragImage(mDragRef, theMap, theTrackMask, theOffsetPoint, kDragDarkerTranslucency);
		ThrowIfOSErr_(theErr);
		
		// Track the drag
		
		::TrackDrag(mDragRef, &mEventRecord, mDragRegion);
	}
	catch (...)
	{
	}
	
	mPageProxyCaption.Hide();
}

// ---------------------------------------------------------------------------
//		¥ AddFlavorURL
// ---------------------------------------------------------------------------
	
void
CProxyDragTask::AddFlavors(DragReference inDragRef)
{
	Inherited::AddFlavors(inDragRef);
	if (mExtraFlavorAdder)
		mExtraFlavorAdder->AddExtraFlavorData(inDragRef, static_cast<ItemReference>(this));
}

// ---------------------------------------------------------------------------
//		¥ MakeDragRegion
// ---------------------------------------------------------------------------
	
void
CProxyDragTask::MakeDragRegion(
	DragReference			/*inDragRef*/,
	RgnHandle				/*inDragRegion*/)
{
	Rect		theFrame;
	
	// Add the page proxy icon region
	
	StRegionHandle theTrackMask;

	mProxyPane.CalcLocalFrameRect(theFrame);
	ThrowIfOSErr_(::IconSuiteToRgn(theTrackMask, &theFrame, kAlignAbsoluteCenter, mProxyPane.GetIconSuiteH()));
	theFrame = (**(RgnHandle)theTrackMask).rgnBBox;
	::LocalToGlobal(&topLeft(theFrame));
	::LocalToGlobal(&botRight(theFrame));
	
	AddRectDragItem(static_cast<ItemReference>(&mProxyPane), theFrame);
	
	// Add the page proxy caption region
		
	mPageProxyCaption.CalcLocalFrameRect(theFrame);
	::LocalToGlobal(&topLeft(theFrame));
	::LocalToGlobal(&botRight(theFrame));
	
	AddRectDragItem(static_cast<ItemReference>(&mPageProxyCaption), theFrame);
}
