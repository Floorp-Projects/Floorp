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

//	StBlockingDialogHandler.cp

#include "StBlockingDialogHandler.h"

#include <Balloons.h>

StBlockingDialogHandler::StBlockingDialogHandler(
	ResIDT 			inDialogResID,
	LCommander*		inSuper)
	:	StDialogHandler(inDialogResID, inSuper)
{
	// disable the Help menu while a dialog is in front
	// (to prevent loading of Help URLs)

	MenuHandle		balloonMenuH = NULL;
	HMGetHelpMenuHandle( &balloonMenuH );
	if (balloonMenuH)
		DisableItem(balloonMenuH, 0);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

StBlockingDialogHandler::~StBlockingDialogHandler()
{
	MenuHandle		balloonMenuH = NULL;
	HMGetHelpMenuHandle( &balloonMenuH );
	if (balloonMenuH)
		EnableItem(balloonMenuH, 0);
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥	
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

Boolean StBlockingDialogHandler::ExecuteAttachments(
	MessageT		inMessage,
	void			*ioParam)
{
	Boolean	executeHost = true;
	
		// Execute the Attachments for the EventDispatcher that was
		// in control before this one took over
	
//	if (mSaveDispatcher != nil) {
//		executeHost = mSaveDispatcher->ExecuteAttachments(inMessage, ioParam);
//	}
	
		// Inherited function will execute Attachments for this object
	
	return (executeHost && LAttachable::ExecuteAttachments(inMessage, ioParam));
}

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Override to prevent About menu item from being enabled.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
void
StBlockingDialogHandler::FindCommandStatus(
	CommandT	inCommand,
	Boolean		&outEnabled,
	Boolean&	/* outUsesMark */,
	Char16&		/* outMark */,
	Str255		/* outName */)
{
	outEnabled = false;
}
