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

// ===========================================================================
//	CProxyPane.cp
//
// ===========================================================================

#include <string>

#include "CProxyPane.h"

#include <UException.h>
#include <LCaption.h>
#include <LView.h>
#include <LString.h>
#include <UAttachments.h>

#include "CStandardFlexTable.h"
#include "CViewUtils.h"
#include "CProxyDragTask.h"
#include "CNetscapeWindow.h"
#include "CBrowserContext.h"
#include "CSuspenderResumer.h"
#include "resgui.h"
#include "mkgeturl.h"
#include "CURLDispatcher.h"
#include "macutil.h"
#include "CURLDragHelper.h"

// ---------------------------------------------------------------------------
//		¥ CProxyPane
// ---------------------------------------------------------------------------

CProxyPane::CProxyPane(
	LStream*		inStream)
		:	mNetscapeWindow(nil),
			mProxyView(nil),
			mPageProxyCaption(nil),
			mSendDataUPP(nil),
			mMouseInFrame(false),
			
			LDragAndDrop(GetMacPort(), this),
			
			Inherited(inStream)
{
	SetIconIDs(kProxyIconNormalID, kProxyIconMouseOverID);
}

// ---------------------------------------------------------------------------
//		¥ ~CProxyPane
// ---------------------------------------------------------------------------

CProxyPane::~CProxyPane()
{
	if (mSendDataUPP)
	{
		DisposeRoutineDescriptor(mSendDataUPP);
	}
}

// ---------------------------------------------------------------------------
//		¥ FinishCreateSelf
// ---------------------------------------------------------------------------

void
CProxyPane::FinishCreateSelf()
{
	Inherited::FinishCreateSelf();

	mSendDataUPP = NewDragSendDataProc(LDropArea::HandleDragSendData);
	ThrowIfNil_(mSendDataUPP);
	
	ThrowIfNil_(mNetscapeWindow			= dynamic_cast<CNetscapeWindow*>(LWindow::FetchWindowObject(GetMacPort())));
	ThrowIfNil_(mProxyView				= dynamic_cast<LView*>(mNetscapeWindow->FindPaneByID(kProxyViewID)));
	ThrowIfNil_(mPageProxyCaption		= dynamic_cast<LCaption*>(mNetscapeWindow->FindPaneByID(kProxyTitleCaptionID)));

}

// ---------------------------------------------------------------------------
//		¥ ListenToMessage
// ---------------------------------------------------------------------------

void
CProxyPane::ListenToMessage(
	MessageT		inMessage,
	void*			ioParam)
{
	const Uint8 kMaxTitleLength = 50;
	switch (inMessage)
	{
		case msg_NSCDocTitleChanged: // browser, mail window and message window case
			{
				const char* theCTitle = static_cast<const char*>(ioParam);
				
				if (theCTitle)
				{
					const char* tempCaption;
					URL_Struct* theURL = nil;
					if (strlen(theCTitle))			// We have a title
						tempCaption = theCTitle;
					else
					{
						// We don't have a title: use the URL instead of the title						
						ThrowIfNil_(mNetscapeWindow->GetWindowContext());
					
						theURL = mNetscapeWindow->CreateURLForProxyDrag(nil);
						if (theURL && theURL->address)
							tempCaption = theURL->address;
					}
					
					// now we at least have something (either title or url). Handle middle
					// truncation, etc and then set the new title.
					string finalCaption = CURLDragHelper::MakeIconTextValid( tempCaption );
					
					NET_FreeURLStruct( theURL ); // we're done with the address string.
					mPageProxyCaption->SetDescriptor(LStr255(finalCaption.c_str()));
										
					Boolean enabled = false;
					Boolean a; Char16 b; Str255 c;
					LCommander* target = LCommander::GetTarget();
					if (target)
						target->ProcessCommandStatus(cmd_AddToBookmarks, enabled, a,b, c);
					if (enabled)
						Enable();
					else
						Disable();

				}
				else
					Disable();

			}
			break;
	}
}

// ---------------------------------------------------------------------------
//		¥ ToggleIcon
// ---------------------------------------------------------------------------

void
CProxyPane::ToggleIcon(
	ResIDT			inResID)
{
	StPortOriginState thePortOriginState(GetMacPort());

	SetIconResourceID(inResID);
	LoadIconSuiteHandle();
	
	Refresh();
	UpdatePort();
}

// ---------------------------------------------------------------------------
//		¥ ToggleIconSelected
// ---------------------------------------------------------------------------

void
CProxyPane::ToggleIconSelected(
	Boolean			inIconIsSelected)
{
	StPortOriginState thePortOriginState(GetMacPort());
	
	if (inIconIsSelected)
	{
		SetIconTransform(kTransformSelected);
	}
	else
	{
		SetIconTransform(kTransformNone);
	}
	
	Refresh();
	UpdatePort();
}						

// ---------------------------------------------------------------------------
//		¥ MouseEnter
// ---------------------------------------------------------------------------

void
CProxyPane::MouseEnter(
	Point				/*inPortPt*/,
	const EventRecord&	/*inMacEvent*/)
{
	mMouseInFrame = true;
	if (IsEnabled() && !CSuspenderResumer::IsSuspended())
	{
		// We intentionally "animate" the icon even when it's not in
		// a frontmost window. Since the animation is intended to 
		// somehow signify that the icon is draggable, it makes sense
		// for it to animate even in a deactive window. Recall that
		// we use delay select to do the proper drag UI so that icon
		// is really active even in an inactive window.
		
		ToggleIcon(mProxyIconMouseOverID);
	}
}
		
// ---------------------------------------------------------------------------
//		¥ MouseLeave
// ---------------------------------------------------------------------------
									
void
CProxyPane::MouseLeave()
{
	mMouseInFrame = false;
	if (IsEnabled() && !CSuspenderResumer::IsSuspended())
	{
		ToggleIcon(mProxyIconNormalID);
	}
}

// ---------------------------------------------------------------------------
//		¥ Click
// ---------------------------------------------------------------------------
//	We override Click here so we can handle the delay select appropriately for
//	dragging the page proxy.

void
CProxyPane::Click(
	SMouseDownEvent&		inMouseDown)
{
	PortToLocalPoint(inMouseDown.whereLocal);
	UpdateClickCount(inMouseDown);
	
	if (ExecuteAttachments(msg_Click, &inMouseDown))
	{
		ClickSelf(inMouseDown);
	}
}

// ---------------------------------------------------------------------------
//		¥ ClickSelf
// ---------------------------------------------------------------------------

void
CProxyPane::ClickSelf(
	const SMouseDownEvent&	inMouseDown)
{	
	FocusDraw();
	
	Point theGlobalPoint = inMouseDown.wherePort;
	PortToGlobalPoint(theGlobalPoint);
	
	StToggleIconSelected theToggleIconSelected(*this, true);
	
	if (::WaitMouseMoved(theGlobalPoint))
	{
		if (LDropArea::DragAndDropIsPresent())
		{
			CProxyDragTask theTask(
				*mProxyView,
				*this,
				*mPageProxyCaption,
				inMouseDown.macEvent,
				mNetscapeWindow->CreateExtraFlavorAdder());
												
			OSErr theErr = ::SetDragSendProc(
				theTask.GetDragReference(),
				mSendDataUPP,
				(LDragAndDrop*)this);
			ThrowIfOSErr_(theErr);
			
			theTask.DoDrag();
		}
	}
	else
	{
		if (GetClickCount() == 2)
		{
			LCommander* target = LCommander::GetTarget();
			if (target)
				target->ObeyCommand(cmd_AddToBookmarks, nil);
			
			// Provide visual feedback by hilighting the bookmark menu -- mdp
			const long kVisualDelay = 8;		// 8 ticks, as recommended by Apple
			unsigned long temp;
			::HiliteMenu( cBookmarksMenuID );
			::Delay( kVisualDelay, &temp );
			::HiliteMenu( 0 );
			
			// Also put a message in the status window -- mdp
			Str255 mesg;
			MWContext* context = *mNetscapeWindow->GetWindowContext();
			::GetIndString( mesg, 7099, 20 );	// why are there no constants for these?
			FE_Progress( context, CStr255(mesg) );
		}
	}
}

// ---------------------------------------------------------------------------
//		¥ ActivateSelf
// ---------------------------------------------------------------------------

void
CProxyPane::ActivateSelf()
{
	// Intentionally blank. The inherited version draws icon enabled (to undo what
	// was done in DeactivateSelf() which is not what we want.
}

// ---------------------------------------------------------------------------
//		¥ DeactivateSelf
// ---------------------------------------------------------------------------

void
CProxyPane::DeactivateSelf()
{
	// Intentionally blank. The inherited version draws icon disabled which
	// is not what we want.
}
 
// ---------------------------------------------------------------------------
//		¥ DoDragSendData
// ---------------------------------------------------------------------------

void
CProxyPane::DoDragSendData(
	FlavorType		inFlavor,
	ItemReference	inItemRef,
	DragReference	inDragRef)
{
	char title[256];
	URL_Struct* url = mNetscapeWindow->CreateURLForProxyDrag(title);
	CURLDragHelper::DoDragSendData(url->address, title, inFlavor, inItemRef, 
										inDragRef);
} // CProxyPane::DoDragSendData

//-----------------------------------
void CProxyPane::HandleEnablingPolicy()
//-----------------------------------
{
	// We may be in background, and we need to allow dragging.  So the idea is to
	// enable the icon if it WOULD be enabled when the window was active.  So
	// do nothing (neither enable nor disable) when in the background.
	if (mNetscapeWindow->IsOnDuty())
	{
		LCommander* target = LCommander::GetTarget();
		if (target)
		{
			Boolean enabled = false;
			Boolean a; Char16 b; Str255 c;
			target->ProcessCommandStatus(cmd_AddToBookmarks, enabled, a,b, c);
			if (enabled)
				Enable();
			else
				Disable();	
		}
	}
} // CProxyPane::HandleEnablingPolicy
