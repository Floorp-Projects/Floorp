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

