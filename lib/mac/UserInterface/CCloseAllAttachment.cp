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
