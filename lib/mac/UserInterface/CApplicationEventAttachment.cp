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


//	Usage Notes:
//
//	(1) Add this attachment as the first attachment to the application object.
//	(2) The EventRecord from ProcessNextEvent *must* persist across calls to
//		ProcessNextEvent in order to prevent dangling references to the event.

#include "CApplicationEventAttachment.h"

#include <LPeriodical.h>

#include "nspr.h"
#include "CMochaHacks.h"

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