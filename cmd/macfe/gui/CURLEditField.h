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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CURLEditField.h
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//
//	This is the URL EditField for the browser.
//	It updates the text via broadcasts from a CNSContext, and it broadcasts
//	a message when the user types a key (so that we can change "Location" to "Goto")
//	and when the user hits return or enter

#pragma once

#include <LListener.h>
#include <LBroadcaster.h>

//#include "VEditField.h"
#include "CTSMEditField.h"

#include "PascalString.h" // for CString and subclasses

// Messages broadcasted by CURLEditField
enum {
	// user hit enter or return
	msg_UserSubmittedURL		=	'USuU',	// CStr255*	url
	// user typed into edit field
	msg_UserChangedURL			=	'UChU'	// <none>
};

//	CURLEditField
//
//	This class is similar to CURLText in mviews.cp, but it listens directly for
//	layout new document messages to set it's text

class CURLEditField : public CTSMEditField, public LBroadcaster, public LListener
{
	private:
		typedef CTSMEditField	Inherited;
		
	public:
		enum { class_ID = 'UEdF' };
		
								CURLEditField(LStream* inStream);
		virtual					~CURLEditField() {};
		
		virtual Boolean		HandleKeyPress(const EventRecord& inKeyEvent);
		void				DrawSelf();
		virtual void		ClickSelf(const SMouseDownEvent& inMouseDown);
		virtual void		ListenToMessage(MessageT inMessage, void* ioParam);

		virtual void 		SetDescriptor(ConstStr255Param inDescriptor);
		virtual void		GetDescriptor(Str255 outDescriptor);

		virtual void		SetDescriptor(const char *inDescriptor, Int32 inLength);
		virtual void 		GetDescriptor(char *inDescriptorStorage, Int32 &ioLength);
		
		virtual Boolean		ObeyCommand(CommandT inCommand,
										void *ioParam = nil);

	protected:
		virtual Boolean		DisplayURL(const char *inURL);
		virtual void		BeTarget();
	
		Boolean				mURLStringInSync;
};
