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
	
	Created 3/20/96 - Tim Craycroft

	To Do:
	
*/

#include "LTableViewHeader.h"
#include "CSpecialTableView.h"

#include "LIconPane.h"

/*	LTableViewHeader::LTableViewHeader	*/
LTableViewHeader::LTableViewHeader(LStream *inStream)
:	LTableHeader(inStream)
,	mTableViewID(0)
,	mTableView(NULL)
{	
	*inStream >> mTableViewID;
}

/*	
	LTableViewHeader::FinishCreateSelf
	
	Find the LTableView we're using	
*/
void
LTableViewHeader::FinishCreateSelf()
{
	LTableHeader::FinishCreateSelf();
	LView *super = GetSuperView();
	ThrowIfNULL_(super);	
	mTableView = (CSpecialTableView *) super->FindPaneByID(mTableViewID);
	Assert_(mTableView != NULL);
	// NOTE: CStandardFlexTable must be created second.  Its FinishCreateSelf
	// queries our mTableView member to make sure that this is true.
}

/*	
	LTableViewHeader::ChangeIconOfColumn
	
	Sets the icon id of the LIconPane inColumn to be inIconID. You'd better be
	sure that inColumn really is an LIconPane if you're gonna do this
*/
void LTableViewHeader::ChangeIconOfColumn( PaneIDT	inColumn, ResIDT	inIconID)
{ 
	UInt16		col = ColumnFromID(inColumn);
	LIconPane	*iconPane = dynamic_cast<LIconPane *>(GetColumnPane(col));
	Assert_(iconPane);
#ifdef DEBUG
	if (iconPane)
#endif
		iconPane->SetIconID(inIconID);
}

/*	
	LTableViewHeader::ComputeResizeDragRect
	
	Extend the bottom of the rect to the bottom of the table
*/
void
LTableViewHeader::ComputeResizeDragRect(UInt16 inLeftColumn, Rect &outDragRect)
{
	LTableHeader::ComputeResizeDragRect(inLeftColumn, outDragRect);
	outDragRect.bottom = GetGlobalBottomOfTable();	
}


/*	
	LTableViewHeader::ComputeColumnDragRect
	
	Extend the bottom of the rect to the bottom of the table
*/
void
LTableViewHeader::ComputeColumnDragRect(UInt16 inLeftColumn, Rect &outDragRect)
{
	LTableHeader::ComputeColumnDragRect(inLeftColumn, outDragRect);
	outDragRect.bottom = GetGlobalBottomOfTable();	
}


/*	
	LTableViewHeader::GetGlobalBottomOfTable
	
	Returns bottom of the table in global coords
*/
SInt16
LTableViewHeader::GetGlobalBottomOfTable() const
{
	Rect	r;
	
	mTableView->CalcPortFrameRect(r);
	
	PortToGlobalPoint((botRight(r)));
	return r.bottom;
}


/*	
	LTableViewHeader::RedrawColumns
	
	Refreshes the appropriate cells in the table.
*/
void
LTableViewHeader::RedrawColumns(UInt16 inFrom, UInt16 inTo)
{
	LTableHeader::RedrawColumns(inFrom, inTo);
	
	Boolean		active;
	TableIndexT	rows, cols;
	STableCell	fromCell, toCell;
	
	active = mTableView->IsActive();

	mTableView->GetTableSize(rows,cols);
	
	
	SPoint32		cellPt;
	SDimension16	frameSize;

	mTableView->GetScrollPosition(cellPt);
	mTableView->GetFrameSize(frameSize);
	
	mTableView->GetCellHitBy(cellPt, fromCell);
	fromCell.col = inFrom;
	
	cellPt.v += frameSize.height - 1;
	mTableView->GetCellHitBy(cellPt, toCell);
	toCell.col = inTo;
		
	mTableView->RefreshCellRange(fromCell, toCell);
}


/*	
	LTableViewHeader::ShowHideRightmostColumn
	
	Adds or removes a column in the table.
*/
void	
LTableViewHeader::ShowHideRightmostColumn(Boolean inShow)
{
	ColumnIndexT initialVisibleColumnCount = CountVisibleColumns();
	LTableHeader::ShowHideRightmostColumn(inShow); // can fail, eg when no resizable columns left
	if (initialVisibleColumnCount != CountVisibleColumns())
	{
		if (inShow)
			mTableView->InsertCols(1, initialVisibleColumnCount, NULL, 0, true);
		else
			mTableView->RemoveCols(1, initialVisibleColumnCount, true);
	}
}


/*	
	LTableViewHeader::ResizeImageBy
	
	Adjust the table's image size.
*/
void	
LTableViewHeader::ResizeImageBy(	Int32	inWidthDelta, 
									Int32 	inHeightDelta,
									Boolean inRefresh		)
{
	LView::ResizeImageBy(inWidthDelta, inHeightDelta, inRefresh);
	if (mTableView) // warning: this can be called before FinishCreate!
		mTableView->AdjustImageSize(inRefresh);
}


/*	
	LTableViewHeader::ReadColumnState
	
	Adjust the table's image size.
*/
void	
LTableViewHeader::ReadColumnState(LStream * inStream, Boolean inMoveHeaders)
{
	LTableHeader::ReadColumnState(inStream, inMoveHeaders);	
	if ( mTableView )
	{
		mTableView->SynchronizeColumnsWithHeader();
	}
}


