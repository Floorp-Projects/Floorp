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

#pragma once

#include <LAttachment.h>

const MessageT	msg_MouseEntered		=	'MsEd';
const MessageT	msg_MouseWithin			=	'MsWn';
const MessageT	msg_MouseLeft			=	'MsLt';

typedef struct SMouseTrackParms {
	Point			portMouse;
	EventRecord		macEvent;
} SMouseTrackParms;


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CMouseDispatcher
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CMouseDispatcher : public LAttachment
{
	public:
							CMouseDispatcher();
	protected:

		virtual void		ExecuteSelf(
									MessageT			inMessage,
									void*				ioParam);
};


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	CMouseTrackAttachment
//
// 	Abstract base class for attachments that want to get mouse tracking
//	messages.
//
// 	In order to use these class of attachments, a CMouseDispatcher must be
//	present.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CMouseTrackAttachment : public LAttachment
{
	public:
							CMouseTrackAttachment();
							CMouseTrackAttachment(LStream* inStream);
		
	protected:

		virtual void		ExecuteSelf(
									MessageT			inMessage,
									void*				ioParam);

		virtual void		MouseEnter(
									Point				inPortPt,
									const EventRecord&	inMacEvent) = 0;

		virtual void		MouseWithin(
									Point				inPortPt,
									const EventRecord&	inMacEvent) = 0;

		virtual void		MouseLeave(void) = 0;

		Boolean				EnsureOwningPane();

		LPane*				mOwningPane;
		Boolean				mMustBeActive;
		Boolean				mMustBeEnabled;
};



