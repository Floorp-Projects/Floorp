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

#include "CTableKeySingleSelector.h"

#include <LTableView.h>

#include <LCommander.h>
#include <PP_KeyCodes.h>

// ---------------------------------------------------------------------------
//		¥ CTableKeySingleSelector
// ---------------------------------------------------------------------------

CTableKeySingleSelector::CTableKeySingleSelector(
	LTableView*		inTable)
	:	LAttachment(msg_KeyPress)
{
	mTable = inTable;
}


// ---------------------------------------------------------------------------
//		¥ CTableKeySingleSelector
// ---------------------------------------------------------------------------
//	Stream constructor.

CTableKeySingleSelector::CTableKeySingleSelector(
	LStream*	inStream)
	:	LAttachment(inStream)
{
	mTable = (dynamic_cast<LTableView*>
						(LAttachable::GetDefaultAttachable()));
						
	SetMessage(msg_KeyPress);
}


// ---------------------------------------------------------------------------
//		¥ ~CTableKeySingleSelector
// ---------------------------------------------------------------------------
//	Destructor

CTableKeySingleSelector::~CTableKeySingleSelector()
{
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ ExecuteSelf
// ---------------------------------------------------------------------------
//	Decode the message and dispatch it.

void
CTableKeySingleSelector::ExecuteSelf(
	MessageT	inMessage,
	void*		ioParam)
{
	SetExecuteHost(true);
	switch (inMessage)
	{
		case msg_KeyPress:
			HandleKeyEvent((EventRecord*) ioParam);
			break;
	}
}

// ---------------------------------------------------------------------------
//		¥ HandleKeyEvent
// ---------------------------------------------------------------------------
//	Recognize and dispatch the arrow keys.

void
CTableKeySingleSelector::HandleKeyEvent(
	const EventRecord* inEvent)
{
	// Sanity check: Make sure we point to a valid table.

	if (mTable == nil)
		return;

	// Prevent arrow keys from going through to host.

	SetExecuteHost(false);

	// Decode the key-down message.

	Int16 theKey = inEvent->message & charCodeMask;
	
	switch (theKey)
	{
		case char_UpArrow:
			UpArrow();
			break;
			
		case char_DownArrow:
			DownArrow();
			break;
			
		default:
			SetExecuteHost(true);		// some other key, let host respond
			break;
	}
}

#pragma mark -

// ---------------------------------------------------------------------------
//		¥ UpArrow
// ---------------------------------------------------------------------------
//	Select the next cell above the current cell. If no cell is selected,
//	select the bottom left cell.

void
CTableKeySingleSelector::UpArrow()
{

	// Find first selected cell.

	STableCell theCell;
	if (mTable->GetNextSelectedCell(theCell))
	{
		// Found a selected cell.
		// If not in the first row, move up one and select.

		if (theCell.row > 1)
		{
			theCell.row--;
			mTable->UnselectAllCells();
			mTable->SelectCell(theCell);		
			ScrollRowIntoFrame(theCell.row);
		}
	}
	else
	{
		// Nothing selected. Start from bottom.
		
		TableIndexT rows, cols;
		mTable->GetTableSize(rows, cols);
		
		if (rows > 0)
		{
			theCell.row = rows;
			theCell.col = 1;
			mTable->UnselectAllCells();
			mTable->SelectCell(theCell);
			ScrollRowIntoFrame(theCell.row);
		}
	}
}


// ---------------------------------------------------------------------------
//		¥ DownArrow
// ---------------------------------------------------------------------------
//	Select the next cell below the current cell. If no cell is selected,
//	select the top left cell.

void
CTableKeySingleSelector::DownArrow()
{

	// Find first selected cell.

	TableIndexT rows, cols;
	STableCell theCell;
	if (!mTable->GetNextSelectedCell(theCell))
	{
		// Nothing selected. Start from top.

		mTable->GetTableSize(rows, cols);
		if (rows > 0) {
			theCell.row = 1;
			theCell.col = 1;
			mTable->UnselectAllCells();
			mTable->SelectCell(theCell);
			ScrollRowIntoFrame(theCell.row);
		}
	
	}
	else
	{

		// Found a selected cell. Look for last selected cell.
		
		STableCell lastCell = theCell;
		while (mTable->GetNextSelectedCell(theCell))
		{
			lastCell = theCell;
		}
		
		// Found last selected cell.
		// If not in the last row, move down one and select.
		
		mTable->GetTableSize(rows, cols);
		
		if (lastCell.row < rows)
		{
			lastCell.row++;
			mTable->UnselectAllCells();
			mTable->SelectCell(lastCell);		
			ScrollRowIntoFrame(lastCell.row);
		}
	}
}

// ---------------------------------------------------------------------------
//		¥ ScrollRowIntoFrame
// ---------------------------------------------------------------------------

void
CTableKeySingleSelector::ScrollRowIntoFrame(
	TableIndexT		inRow)
{
	
	// Find out how many columns are in the table.
	
	TableIndexT rows, cols;
	mTable->GetTableSize(rows, cols);
	
	// Find the first seelcted cell.

	STableCell cell;
	cell.row = inRow;
	cell.col = 1;
	
	while (cell.col <= cols)
	{
		if (mTable->CellIsSelected(cell))
		{
			mTable->ScrollCellIntoFrame(cell);
			return;
		}
		
		cell.col++;
	}

	// No selection shown anywhere, just choose the first cell in the row.

	cell.col = 1;
	mTable->ScrollCellIntoFrame(cell);

}

