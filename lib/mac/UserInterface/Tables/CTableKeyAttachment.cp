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

#include "CTableKeyAttachment.h"

#include <PP_KeyCodes.h>
#include "StSetBroadcasting.h"
#include "CSpecialTableView.h"

//testing
#include "XP_Trace.h"


CTableKeyAttachment::CTableKeyAttachment(CSpecialTableView* inView)
	:	LKeyScrollAttachment(inView)
{
	mTableView = dynamic_cast<CSpecialTableView*>(mViewToScroll);
	Assert_(mTableView);
	
	// set getting keyups to bullet-proof us from someone messing with the System Event Mask
	mGettingKeyUps = (::LMGetSysEvtMask() & keyUpMask);
}

CTableKeyAttachment::~CTableKeyAttachment()
{
}

	
void CTableKeyAttachment::ExecuteSelf(
	MessageT		inMessage,
	void			*ioParam)
{
	// In communicator, keyup events are turned on.  This is for the benefit of Java
	// applets, and we don't want to handle them for key scrolling in tables.
	
	// Well, we do actually. If we get a keyDown event, or autokey events, we don't
	// want to update the list until the user lets go. So we turn off broadcasting
	// until we get a keyUp. We also don't want to change the selection on the keyup,
	// since we've already done that for the corresponding keyDown/autoKey.
	
	if (inMessage != msg_KeyPress)
		return;
	
	EventRecord*	eventParam = (EventRecord *)ioParam;
		
	mExecuteHost = false;
	Int16 theKey = eventParam->message & charCodeMask;
	//Boolean bCommandDown = (eventParam->modifiers & cmdKey) != 0;
	Boolean bShiftDown = (eventParam->modifiers & shiftKey) != 0;
	//Boolean bOptionDown = (eventParam->modifiers & optionKey) != 0;
	Boolean bControlDown = (eventParam->modifiers & controlKey) != 0;
	// get the table dimensions
	TableIndexT theRowCount, theColCount;		
	mTableView->GetTableSize(theRowCount, theColCount);
	
	if (mGettingKeyUps)
	{
		if (theKey == char_DownArrow || theKey == char_UpArrow)
		{
			if (eventParam->what == keyDown || eventParam->what == autoKey) {
				mTableView->SetNotifyOnSelectionChange(false);
			} else if (eventParam->what == keyUp)
			{
				mTableView->SetNotifyOnSelectionChange(true);
				mTableView->SelectionChanged();
				return;
			}
		} else {
			// the only key-ups we want are arrows, and we've already done all we need with them
			if (eventParam->what == keyUp)
				return;
		}
	}
	
	switch (theKey)
	{
		case char_PageUp:
		case char_PageDown:
			LKeyScrollAttachment::ExecuteSelf(inMessage, ioParam);
			break;
		case char_DownArrow:
		if (!bControlDown)
		{
			// Get the last currently selected cell
			STableCell afterLastSelectedCell, theCell; // inited to 0,0 in constructor
			// Keep a copy of theCell, because GetNextSelectedCell clobbers it if
			// there are no more selected cells.
			while (mTableView->GetNextSelectedCell(theCell))
			{
				theCell.row++;
				afterLastSelectedCell = theCell;
			}
			if (afterLastSelectedCell.row == 0) // there were none selected
			{
				theCell.row = theRowCount;
				theCell.col = 1;
			}
			else
			{
				theCell = afterLastSelectedCell;
			}
			if (mTableView->IsValidCell(theCell))
				SelectCell(theCell, bShiftDown);
			break;
		}
		// else falls through
		
		case char_End:
			{
			STableCell theLastCell(theRowCount, theColCount);
			SelectCell(theLastCell, bShiftDown);
			}
			break;
		
		case char_UpArrow:
		if (!bControlDown)
			{
			// Get the first currently selected cell
			STableCell theCell; // inited to 0,0 in constructor
			if (mTableView->GetNextSelectedCell(theCell))
				theCell.row--;
			else
			{
				theCell.row = 1;
				theCell.col = 1;
			}
			if (mTableView->IsValidCell(theCell))
				SelectCell(theCell, bShiftDown);
			break;
			}
		// else falls through
		
		case char_Home:
			{
			STableCell theFirstCell(1,1);
			if (mTableView->IsValidCell(theFirstCell))
				SelectCell(theFirstCell, bShiftDown);
			}
			break;
		
		default:
			mExecuteHost = true;	// Some other key, let host respond
			break;
		}
}



void
CTableKeyAttachment::SelectCell( const STableCell& inCell, Boolean multiple)
{
	if (!multiple)
	{
		StSetBroadcasting		saveBroadcastState((CStandardFlexTable *)mTableView, false);
		mTableView->UnselectAllCells();
	}
	mTableView->ScrollCellIntoFrame(inCell); // Do this before selecting (avoids turds)
	mTableView->SelectCell(inCell);
}


#pragma mark -


CTableRowKeyAttachment::CTableRowKeyAttachment(CSpecialTableView* inView)
	:	CTableKeyAttachment(inView)
{
}

CTableRowKeyAttachment::~CTableRowKeyAttachment()
{
}


void CTableRowKeyAttachment::ExecuteSelf(MessageT inMessage, void	*ioParam)
{
	// In communicator, keyup events are turned on.  This is for the benefit of Java
	// applets, and we don't want to handle them for key scrolling in tables.
	
	// Well, we do actually. If we get a keyDown event, or autokey events, we don't
	// want to update the list until the user lets go. So we turn off broadcasting
	// until we get a keyUp. We also don't want to change the selection on the keyup,
	// since we've already done that for the corresponding keyDown/autoKey.
	
	if (inMessage != msg_KeyPress)
		return;
	
	EventRecord*	eventParam = (EventRecord *)ioParam;
		
	mExecuteHost = false;
	Int16 theKey = eventParam->message & charCodeMask;
	Boolean bShiftDown = (eventParam->modifiers & shiftKey) != 0;
	Boolean bControlDown = (eventParam->modifiers & controlKey) != 0;
	// get the table dimensions
	TableIndexT theRowCount, theColCount;		
	mTableView->GetTableSize(theRowCount, theColCount);
	
	if (mGettingKeyUps)
	{
		if (theKey == char_DownArrow || theKey == char_UpArrow)
		{
			if (eventParam->what == keyDown || eventParam->what == autoKey) {
				mTableView->SetNotifyOnSelectionChange(false);
			} else if (eventParam->what == keyUp)
			{
				mTableView->SetNotifyOnSelectionChange(true);
				mTableView->SelectionChanged();
				return;
			}
		} else {
			// the only key-ups we want are arrows, and we've already done all we need with them
			if (eventParam->what == keyUp)
				return;
		}
	}
	
	switch (theKey)
	{
		case char_PageUp:
		case char_PageDown:
			LKeyScrollAttachment::ExecuteSelf(inMessage, ioParam);
			break;
		case char_DownArrow:
			if (!bControlDown)
			{
				// Get the last currently selected row
				TableIndexT		rows, cols;
				
				mTableView->GetTableSize(rows, cols);
				
				STableCell theCell(rows, 1);
				// We don't want to use GetNextCell() here, because we don't want
				// the overhead of stepping through each cell on the row. We know
				// that all the cells in a selected row are selected.
				while (mTableView->IsValidRow(theCell.row))
				{
					if ( mTableView->CellIsSelected(theCell) )
						break;
					
					theCell.row --;
				}
				
				if (mTableView->IsValidRow(theCell.row) )	// get the one after the selected cell
					theCell.row ++;
				else										// select the last cell
				{
					theCell.row = rows;
					theCell.col = 1;
				}
				if (mTableView->IsValidCell(theCell))
					SelectCell(theCell, bShiftDown);
				break;
			}
			// else falls through
		
		case char_End:
			{
				STableCell theLastCell(theRowCount, 1);	// still the first column
				SelectCell(theLastCell, bShiftDown);
			}
			break;
		
		case char_UpArrow:
			if (!bControlDown)
			{
				// Get the first currently selected row
				STableCell theCell(1, 1);
				
				while (mTableView->IsValidRow(theCell.row))
				{
					if ( mTableView->CellIsSelected(theCell) )
						break;
					
					theCell.row ++;
				}
				
				if (mTableView->IsValidRow(theCell.row) )	// get the one before the selected cell
					theCell.row --;
				else										// select the first cell
				{
					theCell.row = 1;
					theCell.col = 1;
				}
				if (mTableView->IsValidCell(theCell))
					SelectCell(theCell, bShiftDown);
				break;
			}
		// else falls through
		
		case char_Home:
			{
				STableCell theFirstCell(1, 1);
				if (mTableView->IsValidCell(theFirstCell))
					SelectCell(theFirstCell, bShiftDown);
			}
			break;
		
		default:
			mExecuteHost = true;	// Some other key, let host respond
			break;
		}
}
