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

#include "CNotificationAttachment.h"

#include "PascalString.h"

CNotificationAttachment* CNotificationAttachment::sCurrentNotifier = NULL;

CNotificationAttachment::CNotificationAttachment()
:	LAttachment(msg_Event)
,	mIconSuite(NULL)
{
	mNotification.qLink = 0;
	mNotification.qType = nmType;
	mNotification.nmMark = true;
	Post();
	SetExecuteHost(true);
	sCurrentNotifier = this;
}

CNotificationAttachment::~CNotificationAttachment()
{
	Remove();
	sCurrentNotifier = NULL;
}

void CNotificationAttachment::ExecuteSelf(
	MessageT		inMessage,
	void			*ioParam)
{
	Assert_(inMessage == msg_Event);
	EventRecord* theEvent = (EventRecord*)ioParam;

	if (theEvent->what == osEvt)
		{
		Uint8	osEvtFlag = ((Uint32)theEvent->message) >> 24;
		if ((osEvtFlag == suspendResumeMessage) && (theEvent->message & resumeFlag))
			{
			delete this;
			}
		}
}

void CNotificationAttachment::Post(void)
{
	OSErr theErr = ::GetIconSuite(&mIconSuite, 170, svAllSmallData);
	ThrowIfOSErr_(theErr);

	::HNoPurge(mIconSuite);

	mNotification.nmIcon = mIconSuite;

	::GetIndString(mNoteString, 7099, 3);
	Assert_(mNoteString.Length());
	if (mNoteString.Length() > 0)
		mNotification.nmStr = mNoteString;
	else
		mNotification.nmStr = NULL;

	mNotification.nmSound = (Handle)-1;
	mNotification.nmResp = NULL;

	theErr = ::NMInstall(&mNotification);
	ThrowIfOSErr_(theErr);
}

void CNotificationAttachment::Remove(void)
{
	::NMRemove(&mNotification);

	if (mIconSuite != NULL)
		{
		::DisposeIconSuite(mIconSuite, true);
		mIconSuite = NULL;
		}
}

