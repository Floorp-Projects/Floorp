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

#ifdef PowerPlant_PCH
#include PowerPlant_PCH
#endif

#include "CTextTable.h"

#include <UDrawingUtils.h>
#include <PP_Messages.h>

// ---------------------------------------------------------------------------
//		¥ CTextTable(LStream*)
// ---------------------------------------------------------------------------
//	Construct from the data in a Stream

CTextTable::CTextTable(
	LStream*	inStream)
	
	:	super(inStream)
{
}

// ---------------------------------------------------------------------------
//		¥ ~CTextTable
// ---------------------------------------------------------------------------
//	Destructor

CTextTable::~CTextTable()
{
}

// ---------------------------------------------------------------------------
//		¥ ClickSelf
// ---------------------------------------------------------------------------

void
CTextTable::ClickSelf(
	const SMouseDownEvent&	inMouseDown)
{
	if (FocusDraw())
	{
		SwitchTarget(this);
		
		UpdatePort ();

		super::ClickSelf(inMouseDown);
	}
}

// ---------------------------------------------------------------------------
//		¥ ObeyCommand
// ---------------------------------------------------------------------------
//	Respond to Command message

Boolean
CTextTable::ObeyCommand(
	CommandT	inCommand,
	void*		ioParam)
{
	Boolean cmdHandled = true;
	
	switch (inCommand)
	{
		case msg_TabSelect:
			if (!IsEnabled())
			{
				cmdHandled = false;
			}
			break;
			
		default:
			cmdHandled = LCommander::ObeyCommand(inCommand, ioParam);
			break;
	}
	
	return cmdHandled;
}

// ---------------------------------------------------------------------------
//		¥ ObeyCommand
// ---------------------------------------------------------------------------
//	Deactivate list if we aren't going to be the target anymore

void CTextTable::DontBeTarget()
{
	Deactivate();
	// UnselectAllCells();
}


void CTextTable::BeTarget()
{
	Activate();
}


