/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "StCaptureView.h"
#include "UGraphicGizmos.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

StCaptureView::StCaptureView(LView& inView)
	:	mOffscreenMacWorld(nil),
		mView(inView)
{
	Init();

	SDimension16 theViewFrameSize;
	mView.GetFrameSize(theViewFrameSize);
	
	mPortionToCapture.left		= mPortionToCapture.top = 0;
	mPortionToCapture.right		= mPortionToCapture.left + theViewFrameSize.width;
	mPortionToCapture.bottom	= mPortionToCapture.top  + theViewFrameSize.height;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

StCaptureView::StCaptureView(LView& inView, const Rect& inPortionToCapture)
	:	mOffscreenMacWorld(nil),
		mView(inView),
		mPortionToCapture(inPortionToCapture)
{
	Init();
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
StCaptureView::Init()
{
	if (mView.IsActive())
		mActive = triState_On;
		
	if (mView.IsEnabled())
		mEnabled = triState_On;
		
	if (mView.IsVisible())
		mVisible = triState_On;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

StCaptureView::~StCaptureView()
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void StCaptureView::Capture(CGWorld& ioWorld)
{
	try
	{
		PlaceInSuperFrameAt(-mPortionToCapture.left, -mPortionToCapture.top, false);
		ResizeFrameTo(
						mPortionToCapture.right  - mPortionToCapture.left,
						mPortionToCapture.bottom - mPortionToCapture.top,
						false);

		InstallOccupant(&mView, kAlignNone);

		Rect theSaveFrame = ioWorld.GetBounds();
		if (!ioWorld.BeginDrawing())
		{
			throw memFullErr;
		}

		mOffscreenMacWorld = ioWorld.GetMacGWorld();

		OutOfFocus(nil);
		mView.Draw(nil);
		OutOfFocus(nil);

		ioWorld.EndDrawing();
		ioWorld.UpdateOrigin(topLeft(theSaveFrame));		

		RemoveOccupant();
		mOffscreenMacWorld = nil;
	}
	catch (...)
	{
		RemoveOccupant();
		mOffscreenMacWorld = nil;
		throw;
	}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean StCaptureView::EstablishPort(void)
{
	GrafPtr	thisPort = GetMacPort();
	Boolean	portSet = (thisPort != nil);
	if (portSet && (UQDGlobals::GetCurrentPort() != thisPort))
		{
		if (thisPort == (GrafPtr)mOffscreenMacWorld)
			::SetGWorld((GWorldPtr)thisPort, nil);
		else
			::SetPort(thisPort);
		}
	return portSet;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

GrafPtr StCaptureView::GetMacPort(void) const
{
	GrafPtr	theMacPort;
	if (mOffscreenMacWorld != nil)
		theMacPort = (GrafPtr)mOffscreenMacWorld;
	else
		theMacPort = LView::GetMacPort();

	return theMacPort;
}

