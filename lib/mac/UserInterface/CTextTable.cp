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


