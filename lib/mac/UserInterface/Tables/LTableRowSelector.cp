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

/*
	
	Created 3/25/96 - Tim Craycroft

	To Do: fix DragSelect
*/


#include "LTableRowSelector.h"

#include "CStandardFlexTable.h"

/*
	LTableRowSelector::LTableRowSelector
*/
LTableRowSelector::LTableRowSelector(LTableView *inTableView, Boolean inAllowMultiple)
:	LTableSelector(inTableView)
,	LArray(1)
,	mAnchorRow(0)
,	mExtensionRow(0)
,	mAllowMultiple(inAllowMultiple)
,	mSelectedRowCount(0)
{
}

// TSM - Added two methods below for compatibility with PP 1.5

/*
	GetFirstSelectedCell
	
	Return the first selected cell, defined as the min row & col (closest to
	top-left corner). Return (0, 0) if no cell is selected.
*/
STableCell
LTableRowSelector::GetFirstSelectedCell() const
{
	TableIndexT firstSelectedRow = GetFirstSelectedRow();
	
	return (firstSelectedRow ? STableCell(firstSelectedRow, 1) : STableCell(0, 0));
}


/*
	GetFirstSelectedRow
	
	Return the first selected cell's row, defined as the min row & col (closest to
  	top-left corner). Return 0 if no row is selected.
*/
TableIndexT
LTableRowSelector::GetFirstSelectedRow() const
{
	if ( GetSelectedRowCount() > 0 ) {
		// We have selected cells
		STableCell curCell(1, 1);
		TableIndexT endRow, numCol;
		mTableView->GetTableSize(endRow, numCol);
		
		Assert_(endRow >= 1);	// If (mSelectedRowCount > 0), should always have (endRow >= 1)!
		do {
			if ( CellIsSelected(curCell) ) {
				// This is our first row
				return curCell.row;
			}
		} while ( ++curCell.row <= endRow );
	}
	
	return 0;
}


/*
	LTableRowSelector::CellIsSelected
*/
Boolean
LTableRowSelector::CellIsSelected(const STableCell	&inCell) const
{
	char item;
	
	FetchItemAt(inCell.row, &item);
	return (item != 0);
}


/*
	LTableRowSelector::SelectCell
*/
void	
LTableRowSelector::SelectCell(const STableCell	&inCell)
{
	if (mSelectedRowCount == 0)
	{
		// We must maintain the anchor row even if called without clicking.
		// Bug #57637 mcmullen 97/04/29.  In particular, for key selection,
		// UnselectAllCells will be called before reselecting, so we can use
		// an empty selection as the prerequisite for resetting the anchor row.
		mAnchorRow = inCell.row;
		mExtensionRow	= mAnchorRow;
	}
	DoSelect(inCell.row, true, true, true);
}
	
/*
	LTableRowSelector::UnselectCell
*/
void		
LTableRowSelector::UnselectCell(const STableCell &inCell)
{
	DoSelect(inCell.row, false, true, true);
	if (inCell.row == mAnchorRow)
	{
		mAnchorRow 		= 0;
		mExtensionRow 	= 0;
	}
}

/*
	LTableRowSelector::DoSelect
*/
void
LTableRowSelector::DoSelect(	const TableIndexT 	inRow, 
								Boolean 			inSelect, 
								Boolean 			inHilite,
								Boolean				inNotify	)
{
	if (inRow < 1 || inRow > GetCount())
		return;
	
	// Make sure for a single selector only 1 row is selected
	if ( inSelect && !mAllowMultiple )
	{
		if ( mSelectedRowCount > 0 )
		{
			DoSelectAll(false, false);
			mSelectedRowCount = 0; // to be safe
		}
	}
	
	char oldRowValue;
	FetchItemAt(inRow, &oldRowValue);
	char newRowValue = (inSelect) ? 1 : 0;		
	if (newRowValue == oldRowValue)
		return;
	// mark the row's new selected state
	AssignItemsAt(1, inRow, &newRowValue);
	// Maintain the count
	if (inSelect)
		mSelectedRowCount++;
	else
		mSelectedRowCount--;	

	// hilite (or unhilite) the entire row
	if (inHilite)
	{
		CStandardFlexTable* sft = dynamic_cast<CStandardFlexTable*>(mTableView);
		if (sft)
			sft->HiliteRow(inRow, inSelect);
		else
		{
			UInt32 nCols, nRows;			
			mTableView->GetTableSize(nRows, nCols);
			
			STableCell	cell(inRow, 1);
			cell.row = inRow;
			
			for (cell.col = 1;  cell.col <= nCols; cell.col++)
			{
				mTableView->HiliteCell(cell, inSelect);
			}
		}
	}

	if (inNotify)
		mTableView->SelectionChanged();
} // LTableRowSelector::DoSelect

/*
	LTableRowSelector::SelectAllCells
*/
void	
LTableRowSelector::SelectAllCells()
{
	DoSelectAll(true, true);
}

/*
	LTableRowSelector::UnselectAllCells
*/
void
LTableRowSelector::UnselectAllCells()
{
	if ( mSelectedRowCount > 0 ) {
		DoSelectAll(false, true);
		mSelectedRowCount = 0; // to be safe
	}
}

/*
	LTableRowSelector::DoSelectAll
*/
void
LTableRowSelector::DoSelectAll(Boolean inSelect, Boolean inNotify)
{
	UInt32 nCols, nRows;
	mTableView->GetTableSize(nRows, nCols);

	if (nRows > 0) // Prevents bug: DoSelectRange won't check!
		DoSelectRange(1, nRows, inSelect, inNotify);
	else
	{
		mSelectedRowCount = 0;
		if (inNotify)
			mTableView->SelectionChanged();
	}
}

/*
	LTableRowSelector::DoSelectRange
	
	Selects a range of rows, where inFrom and inTo can
	be in any order.
*/
void
LTableRowSelector::DoSelectRange(	TableIndexT	inFrom, 
									TableIndexT inTo, 
									Boolean 	inSelect,
									Boolean		inNotify	)
{
	TableIndexT	top, bottom;
	STableCell	cell;
	
	// figure out our order
	if (inTo > inFrom) 
	{
		top 	= inFrom;
		bottom 	= inTo;
	}
	else
	{
		top 	= inTo;
		bottom  = inFrom;
	}
	cell.col = 1;
	for (cell.row=top; cell.row <= bottom; cell.row++)
		DoSelect(cell.row, inSelect, true, false);
	if (inNotify)
		mTableView->SelectionChanged();
}

/*
	LTableRowSelector::ExtendSelection
*/
void
LTableRowSelector::ExtendSelection( TableIndexT inRow)
{
	Assert_(mAllowMultiple);
	
	// mExtension row is the row to which we've 
	// currently extended the selection
	
	// mAnchor row is the originally selected row

	if (mExtensionRow != inRow)
	{
		if (inRow > mExtensionRow) {
			DoSelectRange(mExtensionRow+1, inRow, true, true);
		}
		else {
		 	DoSelectRange(inRow, mExtensionRow-1, true, true);
		}
		
		mExtensionRow = inRow;
	}
}

/*
	LTableRowSelector::ClickSelect
*/
void		
LTableRowSelector::ClickSelect(	const STableCell		&inCell,
								const SMouseDownEvent	&inMouseDown)
{
	// This code largely ripped off from PowerPlant's 
	// LTableMultiSelector
	//
	
	// Command-key discontiguous selection....
	if (mAllowMultiple && (inMouseDown.macEvent.modifiers & cmdKey)!= 0)
	{
		//
		
		//
		
		if (CellIsSelected(inCell))
		{
			DoSelect(inCell.row, false, true, true);
			mAnchorRow 		= 0;
			mExtensionRow 	= 0;
		}
		else
		{
			DoSelect(inCell.row, true, true, true);
			mAnchorRow 		= inCell.row;
			mExtensionRow	= mAnchorRow;
		}
	}
	
	// shift-key selection extension
	else if ( mAllowMultiple && 
				((inMouseDown.macEvent.modifiers & shiftKey) != 0) &&
				mAnchorRow != 0 )	
	{	
		ExtendSelection(inCell.row);
	}
	
	// normal selection
	else
	{
		mAnchorRow 		= inCell.row;
		mExtensionRow 	= mAnchorRow;
		
		if (!CellIsSelected(inCell))
		{
			DoSelectAll(false, false);
			DoSelect(inCell.row, true, true, true);
		}
	}
	
}
				
/*
	LTableRowSelector::DragSelect
*/	
Boolean
LTableRowSelector::DragSelect(	const STableCell		&inCell,
								const SMouseDownEvent	&inMouseDown)
{
	// Again, largely ripped off from PowerPlant
	ClickSelect(inCell, inMouseDown);
	
	STableCell	currCell = inCell;	
	while (::StillDown()) 
	{
		STableCell	hitCell;
		SPoint32	imageLoc;
		Point		mouseLoc;
		
		::GetMouse(&mouseLoc);
		if (mTableView->AutoScrollImage(mouseLoc)) 
		{
			mTableView->FocusDraw();
			Rect	frame;
			mTableView->CalcLocalFrameRect(frame);
			Int32 pt = ::PinRect(&frame, mouseLoc);
			mouseLoc = *(Point*)&pt;
		}
		
		mTableView->LocalToImagePoint(mouseLoc, imageLoc);
		mTableView->GetCellHitBy(imageLoc, hitCell);
		if (mTableView->IsValidCell(hitCell) &&
			(currCell != hitCell)) 
		{

			currCell = hitCell;
		}
		
		
		ExtendSelection(currCell.row);
	}	
	
	return true;
}
	
/*
	LTableRowSelector::InsertRows
	
	Add space in our array for tracking the selected state of the new rows
*/	
void		
LTableRowSelector::InsertRows(	UInt32		inHowMany,
								TableIndexT	inAfterRow)
{
	// Before inserting the cells, check for a selection
	// change message.

	Boolean selectionChanged = false;
	UInt32 nRows = GetCount();
		//¥¥¥¥¥¥¥¥¥¥¥¥¥¥
		// WARNING: Don't ask the table for its size, it's changed already.
		//¥¥¥¥¥¥¥¥¥¥¥¥¥¥
	// We loop from the insertion point to find any selected row.
	for (TableIndexT row = inAfterRow + 1; row <= nRows; row++)
	{
		char item;
		FetchItemAt(row, &item);
		if (item)
		{
			selectionChanged = true;
			break;
		}
	}
	char unselectedItem = 0;
	InsertItemsAt(inHowMany, inAfterRow + 1, &unselectedItem);
		// jrm: I increment inAfterRow, because of the difference between "at" and "after".

	// The selection will be changed if we added any row before any selected row
	// (because it will move down).
	if (selectionChanged)
		mTableView->SelectionChanged();
}

/*
	LTableRowSelector::RemoveRows
	
	Remove the space from our array.
*/	
void
LTableRowSelector::RemoveRows(	Uint32		inHowMany,
								TableIndexT	inFromRow )
{
	// Before removing the cells, maintain the row count and check for a selection
	// change message.
	
	TableIndexT lastRemovedRow = inFromRow + inHowMany - 1;
	Boolean selectionChanged = false;
	UInt32 nRows = GetCount();
		//¥¥¥¥¥¥¥¥¥¥¥¥¥¥
		// WARNING: Don't ask the table for its size, it's changed already.
		//¥¥¥¥¥¥¥¥¥¥¥¥¥¥
	// Loop from the first removed row to the end.
	// Any selected row encountered before the last removed row is deducted
	// from the tally.  Then we continue to find any selected
	// row.  If we find one, then the selection will have changed.
	// In the case when the removed range includes a selected row, this will
	// loop to the end of the removed range, in order to maintain the
	// selected row count.  In the case when the removed range does not include
	// a selected row, this must loop beyond that range till it finds a
	// selected row.
	for (TableIndexT row = inFromRow; row <= nRows; row++)
	{
		char item;
		FetchItemAt(row, &item);
		if (item)
		{
			selectionChanged = true;
			if (row <= lastRemovedRow)
				mSelectedRowCount--;
			else
				break;
		}
	}
	RemoveItemsAt(inHowMany, inFromRow);
	// The selection will be changed if we removed any row up to the last selected row
	// (because it will either have been removed, or it will move up).
	if (selectionChanged)
		mTableView->SelectionChanged();
}								

void
LTableRowSelector::MakeSelectionRegion(RgnHandle ioRgnHandle, TableIndexT hiliteColumn)
{
	Assert_(ioRgnHandle);
	
	::SetEmptyRgn(ioRgnHandle);
	
	Boolean			inSelection = false;
	TableIndexT		selectionStart;
	TableIndexT 	nRows = GetCount();
	
	StRegion		tempRgn;
	
	for (TableIndexT row = 1; row <= nRows; row ++)
	{
		Boolean		rowSelected;
		
		FetchItemAt(row, &rowSelected);
		
		// I'm trying to reduce the number of region operations here
		if (rowSelected && !inSelection)		// start of selection group
		{
			selectionStart = row;
			inSelection = true;
		
		} else if (!rowSelected && inSelection)	// end of selection group
		{
			Rect	selectRect = {selectionStart, hiliteColumn, row, hiliteColumn + 1};
			
			::RectRgn(tempRgn, &selectRect);
			::UnionRgn(tempRgn, ioRgnHandle, ioRgnHandle);
			inSelection = false;
		}
	}
	// do the last group if necessary
	if (inSelection)
	{
		Rect	selectRect = {selectionStart, hiliteColumn, nRows + 1, hiliteColumn + 1};
			
		::RectRgn(tempRgn, &selectRect);
		::UnionRgn(tempRgn, ioRgnHandle, ioRgnHandle);
	}
}


void
LTableRowSelector::SetSelectionFromRegion(RgnHandle inRgnHandle)
{
	Assert_(inRgnHandle);

	TableIndexT 	nRows = GetCount();
	
	UnselectAllCells();
	
	STableCell		theCell(1, 1);
	
	for (theCell.row = 1; theCell.row <= nRows; theCell.row ++)
	{
		Point	thePoint = { /* cast */theCell.row, 1};		//v, h
		
		if ( ::PtInRgn(thePoint, inRgnHandle) )
			DoSelect(theCell.row, true, true, true);
	}
}