/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CMouseDispatcher.h"
#include "macutil.h"


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CMouseDispatcher::CMouseDispatcher()
	:	LAttachment(msg_Event, true)
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CMouseDispatcher::ExecuteSelf(
	MessageT			inMessage,
	void*				ioParam)
{
	Assert_(inMessage == msg_Event);
	const EventRecord* theEvent = (const EventRecord*)ioParam;
	
	// High level events dont have the mouse position in the where field
	// of the event record.
	if (theEvent->what == kHighLevelEvent)
		return;

	// Don't process mouse position for update events (otherwise, tooltip
	// panes that are drawn under the mouse position get deleted before
	// the update event even gets processed - jrm 98/05/05.
	if (theEvent->what == updateEvt)
		return;

	LPane* theCurrentPane = nil;
	LPane* theLastPane = LPane::GetLastPaneMoused();

	Point thePortMouse;	
	WindowPtr theMacWindow;
	Int16 thePart = ::FindWindow(theEvent->where, &theMacWindow);
	LWindow	*theWindow = LWindow::FetchWindowObject(theMacWindow);

	if (theWindow != nil)
	{
		thePortMouse = theEvent->where;
		theWindow->GlobalToPortPoint(thePortMouse);

		// Note: it is the responsibility of the pane
		// to guard with IsEnabled(), IsActive(), etc.
		// Some panes need to execute MouseEnter, etc
		// even in inactive windows
		
		theCurrentPane = FindPaneHitBy(theEvent->where);
	}


	// It is okay to assume that the mouse point was converted to port coordinates
	// in the conditional above.  If it was not, it's because the current pane will
	// be nil, and the only thing that will be called later is MouseLeave() which
	// dosen't need it.
	SMouseTrackParms theMouseParms;
	theMouseParms.portMouse = thePortMouse;
	theMouseParms.macEvent = *theEvent;
	theMouseParms.paneOfAttachment = nil; // will be changed by an attachment to show it's there.

	// At this point, even if theCurrentPane and theLastPane are different, it
	// might be because theCurrentPane is a tooltip pane, put there by the
	// attachment.  This is no reason for doing "mouse left" processing.
	// Use theLastPane instead.
	if (theCurrentPane != theLastPane
	&& theLastPane  && theLastPane->Contains(thePortMouse.h, thePortMouse.v))
		{
		if (theLastPane->ExecuteAttachments(msg_MouseWithin, &theMouseParms)
		&& theMouseParms.paneOfAttachment == theCurrentPane)
			{
			theLastPane->MouseWithin(thePortMouse, *theEvent);
			if (theCurrentPane != nil) // see explanation below.
				theCurrentPane = FindPaneHitBy(theEvent->where);
			}
		}
	if ((!theCurrentPane || theMouseParms.paneOfAttachment != theCurrentPane)
	&& theCurrentPane != theLastPane)
		{
		if (theLastPane != nil)
			{
			if (theLastPane->ExecuteAttachments(msg_MouseLeft, &theMouseParms))
				{
				theLastPane->MouseLeave();
				// If theLastPane is a CPatternButton with a tooltip attachment,
				// the ExecuteAttachments() method above deletes the tooltip pane.
				// This is normal behaviour. However, if we are unlucky enough,
				// theCurrentPane can point to the tooltip pane which has just
				// been deleted. So it is important to re-initialize theCurrentPane
				// at this point:
				if (theCurrentPane != nil)
					theCurrentPane = FindPaneHitBy(theEvent->where);
				}

			}
		
		if (theCurrentPane != nil)
			{
			if (theCurrentPane->ExecuteAttachments(msg_MouseEntered, &theMouseParms))
				theCurrentPane->MouseEnter(thePortMouse, *theEvent);
			}
		
		LPane::SetLastPaneMoused(theCurrentPane);
		}
	else if (theCurrentPane != nil)
		{
		if (theCurrentPane->ExecuteAttachments(msg_MouseWithin, &theMouseParms))
			theCurrentPane->MouseWithin(thePortMouse, *theEvent);
		}
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CMouseTrackAttachment::CMouseTrackAttachment()
	:	LAttachment(msg_AnyMessage)
	,	mOwningPane(nil)
	,	mPaneMustBeActive(true)
	,	mWindowMustBeActive(true)
	,	mMustBeEnabled(true)
{
	SetExecuteHost(true);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

CMouseTrackAttachment::CMouseTrackAttachment(LStream* inStream)
	:	LAttachment(inStream)
	,	mOwningPane(nil)
	,	mPaneMustBeActive(true)
	,	mWindowMustBeActive(true)
	,	mMustBeEnabled(true)
{
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean CMouseTrackAttachment::EnsureOwningPane()
{
	if (!mOwningPane)
	{
		mOwningPane = dynamic_cast<LPane*>(GetOwnerHost());
		Assert_(mOwningPane); 	// you didn't attach to an LPane* derivative
	}
	if (!mOwningPane)
		return false;
	if (mMustBeEnabled && !mOwningPane->IsEnabled())
		return false;
	if (mPaneMustBeActive && !mOwningPane->IsActive())
		return false;
	if (mWindowMustBeActive)
	{
		LWindow* win
			= dynamic_cast<LWindow*>(
				LWindow::FetchWindowObject(mOwningPane->GetMacPort()));
		if (!win || !win->IsActive())
			return false;
	}
	return true;
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void CMouseTrackAttachment::ExecuteSelf(
	MessageT			inMessage,
	void*				ioParam)
{
	SMouseTrackParms* theTrackParms = (SMouseTrackParms*)ioParam;

	if (!EnsureOwningPane())
		return;
		
	switch (inMessage)
		{
		case msg_MouseEntered:
			Assert_(theTrackParms != nil);
			if ( theTrackParms )
				MouseEnter(theTrackParms->portMouse, theTrackParms->macEvent);
			break;
			
		case msg_MouseWithin:
			Assert_(theTrackParms != nil);
			if ( theTrackParms )
				MouseWithin(theTrackParms->portMouse, theTrackParms->macEvent);
			break;
			
		case msg_MouseLeft:
			MouseLeave();
			break;	
		}
};

