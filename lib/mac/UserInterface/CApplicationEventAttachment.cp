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


//	Usage Notes:
//
//	(1) Add this attachment as the first attachment to the application object.
//	(2) The EventRecord from ProcessNextEvent *must* persist across calls to
//		ProcessNextEvent in order to prevent dangling references to the event.

#include "CApplicationEventAttachment.h"

#include <LPeriodical.h>

#include "nspr.h"
#include "CMochaHacks.h"

enum {
	kPrivateNSPREventType = 13
};

// ---------------------------------------------------------------------------------
//		¥ [Static data members]
// ---------------------------------------------------------------------------------

LApplication*	CApplicationEventAttachment::sApplication	= nil;
EventRecord*	CApplicationEventAttachment::sCurrentEvent	= nil;
EventRecord		CApplicationEventAttachment::sFakeNullEvent;
Boolean			CApplicationEventAttachment::sFakeNullEventInitialized = false;

// ---------------------------------------------------------------------------
//		¥ CApplicationEventAttachment
// ---------------------------------------------------------------------------

CApplicationEventAttachment::CApplicationEventAttachment()
	:	LAttachment(msg_Event)
{
}

// ---------------------------------------------------------------------------
//		¥ ~CApplicationEventAttachment
// ---------------------------------------------------------------------------

CApplicationEventAttachment::~CApplicationEventAttachment()
{
}

// ---------------------------------------------------------------------------
//		¥ ExecuteSelf
// ---------------------------------------------------------------------------
//	Remember the current event and the application object

void
CApplicationEventAttachment::ExecuteSelf(
	MessageT		inMessage,
	void*			ioParam)
{
#pragma unused(inMessage)

	sCurrentEvent = static_cast<EventRecord*>(ioParam);
	
	if (!sApplication)
	{
		sApplication = dynamic_cast<LApplication*>(mOwnerHost);
	}
}

// ---------------------------------------------------------------------------
//		¥ GetApplication
// ---------------------------------------------------------------------------

LApplication&
CApplicationEventAttachment::GetApplication()
{
	ThrowIfNil_(sApplication);
	
	return *sApplication;
}

// ---------------------------------------------------------------------------
//		¥ GetCurrentEvent
// ---------------------------------------------------------------------------

EventRecord&
CApplicationEventAttachment::GetCurrentEvent()
{
	if (!sCurrentEvent)
	{
		if (!sFakeNullEventInitialized)
		{
			sFakeNullEvent.what = nullEvent;
			sFakeNullEvent.message = 0;
			sFakeNullEvent.when = 0;
			sFakeNullEvent.where.h = sFakeNullEvent.where.v	= 0;
			sFakeNullEvent.modifiers = 0;
			
			sFakeNullEventInitialized = true;
		}
		
		return sFakeNullEvent;
	}
	
	return *sCurrentEvent;
}

// ---------------------------------------------------------------------------
//		¥ CurrentEventHasModifiers
// ---------------------------------------------------------------------------

Boolean
CApplicationEventAttachment::CurrentEventHasModifiers(
	EventModifiers		inModifiers)
{
	Boolean			hasModifier		= false;
	EventModifiers	hackedModifiers	= inModifiers;
	
	if (GetCurrentEvent().what == kPrivateNSPREventType)
	{
		// This is ridiculous. Sometimes when event.what is kPrivateNSPREventType
		// the modifiers are mapped to the mocha stuff and sometimes they're 
		// native mac. So we have to first see if it seems to be mapped as mocha
		// before arbitrarily mapping inModifiers. And to make matters worse,
		// the darn mocha event mask stuff overlaps native mac modifiers for
		// the alt (option) modifier - so we're snookered in that case. TODO: is this
		// the best we can do short of fixing mocha event masks to match
		// the mac (at lease in the case of the Mac - we could #ifdef it).
		
		if ((GetCurrentEvent().modifiers & EVENT_ALT_MASK) ||
			(GetCurrentEvent().modifiers & EVENT_CONTROL_MASK) ||
			(GetCurrentEvent().modifiers & EVENT_SHIFT_MASK) ||
			(GetCurrentEvent().modifiers & EVENT_META_MASK))
		{
			hackedModifiers = CMochaHacks::MochaModifiers(inModifiers);
		}
	}

	return ((GetCurrentEvent().modifiers & hackedModifiers) ? true : false);
}	

// ---------------------------------------------------------------------------
//		¥ ProcessNextEvent
// ---------------------------------------------------------------------------

void
CApplicationEventAttachment::ProcessNextEvent()
{
	GetApplication().ProcessNextEvent();
}
