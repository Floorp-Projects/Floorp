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

// Written by Mark G. Young

//	Add the CApplicationEventAttachment attachment to the application object.
//	Add the CCloseAllAttachment attachment to the application object.
//	Add the CCloseAllAttachment attachment to each "regular" closeable window.

#include "CCloseAllAttachment.h"

#ifndef __EVENTS__
#include <Events.h>
#endif

#include <LCommander.h>
#include <UDesktop.h>
#include "PascalString.h"
#include <LStream.h>
#include <LWindow.h>

#include "CApplicationEventAttachment.h"
#include "CDesktop.h"

#include "uerrmgr.h"

// ---------------------------------------------------------------------------
//		¥ CCloseAllAttachment
// ---------------------------------------------------------------------------

CCloseAllAttachment::CCloseAllAttachment(
	ResIDT		inCloseStringID,
	ResIDT		inCloseAllStringID)
	: 	mCloseStringID(inCloseStringID),
		mCloseAllStringID(inCloseAllStringID)
{
}

// ---------------------------------------------------------------------------
//		¥ CCloseAllAttachment
// ---------------------------------------------------------------------------

CCloseAllAttachment::CCloseAllAttachment(
	LStream*	inStream)
	:	super(inStream)
{
	inStream->ReadData(&mCloseStringID, sizeof(mCloseStringID));
	inStream->ReadData(&mCloseAllStringID, sizeof(mCloseAllStringID));
}

// ---------------------------------------------------------------------------
//		¥ ~CCloseAllAttachment
// ---------------------------------------------------------------------------

CCloseAllAttachment::~CCloseAllAttachment()
{
}

// ---------------------------------------------------------------------------
//		¥ ExecuteSelf
// ---------------------------------------------------------------------------

void
CCloseAllAttachment::ExecuteSelf(
	MessageT	inMessage,
	void*		ioParam)
{
	switch (inMessage)
	{
		case msg_CommandStatus:
			FindCommandStatus(static_cast<SCommandStatus*>(ioParam));
			break;
			
		case cmd_Close:
			ObeyCommand();
			break;
	}
}

// ---------------------------------------------------------------------------
//		¥ FindCommandStatus
// ---------------------------------------------------------------------------

void
CCloseAllAttachment::FindCommandStatus(
	SCommandStatus*	ioCommandStatus)
{
	if (ioCommandStatus->command == cmd_Close)
	{
		Boolean isCloseAll = false;
		
		mExecuteHost = true;

		LWindow* theTopRegular = UDesktop::FetchTopRegular();
		if (theTopRegular && theTopRegular->HasAttribute(windAttr_CloseBox))
		{
			*ioCommandStatus->enabled = true;
		}

		if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
		{
			isCloseAll = true;

			if (mCloseAllStringID)
				*(CStr255*)ioCommandStatus->name = ::GetPString(mCloseAllStringID);
			else
				*(CStr255*)ioCommandStatus->name = "Close All"; // not seen by user?
		}
		
		if (!isCloseAll)
		{
			if (mCloseStringID)
				*(CStr255*)ioCommandStatus->name = ::GetPString(mCloseStringID);
			else
				*(CStr255*)ioCommandStatus->name = "Close";
		}
	}
	else
	{
		mExecuteHost = true;
	}
}	

// ---------------------------------------------------------------------------
//		¥ ObeyCommand
// ---------------------------------------------------------------------------

void
CCloseAllAttachment::ObeyCommand()
{
	LWindow* theNextWindow = UDesktop::FetchTopRegular();
	LWindow* theWindow = nil;

	mExecuteHost = true;

	if (theNextWindow)
	{	
		// Note: we always skip the original front most regular window (it will
		// be handled by PowerPlant after we close all the other windows).
		
		theNextWindow = CDesktop::FetchNextRegular(*theNextWindow);
	}
	
	if (theNextWindow)
	{
		try
		{
			if (CApplicationEventAttachment::CurrentEventHasModifiers(optionKey))
			{
				while (theNextWindow)
				{
					theWindow		= theNextWindow;
					
					theNextWindow	= CDesktop::FetchNextRegular(*theWindow);	
								
					if (theWindow && theWindow->HasAttribute(windAttr_CloseBox))
						theWindow->ObeyCommand(cmd_Close, nil);
				}
			}
		}
		catch (...)
		{
		}
	}
}
