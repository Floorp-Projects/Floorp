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

/* A list (single-column; single selection) of c-string data */
/* variable size data */


#include "CStringListView.h"
#include "LTableMultiGeometry.h"
#include "LTableSingleSelector.h"
#include "LTableArrayStorage.h"


void *CStringListView::CreateStringListViewStream( LStream *inStream )
{
	return new CStringListView( inStream );
}


CStringListView::CStringListView( LStream *inStream )
		: LTableView( inStream ), LBroadcaster(), LCommander()
{
}


CStringListView::~CStringListView()
{
}


void CStringListView::FinishCreateSelf()
{
	// SetUpTableHelpers()
	SetTableGeometry(new LTableMultiGeometry(this, mFrameSize.width, 18));
	SetTableSelector(new LTableSingleSelector(this));
	SetTableStorage(new LTableArrayStorage( this, (Uint32)0 ));	// 0 == variable size

	InsertCols( 1, 0, NULL, 0, Refresh_No);
}


Boolean CStringListView::GetPreviousRow( STableCell &ioCell) const
{
	Boolean	prevCellExists = true;
	
	TableIndexT	row = ioCell.row - 1;	// Previous Cell is in previous row
	
	if (row > mRows) {					// Test if beyond last row
		row = mRows;
	}
	
	if (row < 1) {						// Special test for row zero
		prevCellExists = false;
		row = 0;
	}
		
	ioCell.SetCell(row, ioCell.col);
	return prevCellExists;
}


Boolean CStringListView::HandleKeyPress( const EventRecord &inKeyEvent )
{
	STableCell	c = GetFirstSelectedCell();
	
	Char16	theChar = inKeyEvent.message & charCodeMask;
	switch ( theChar & charCodeMask )
	{
		case char_UpArrow:
			if ( GetPreviousRow( c ) )
			{
				SelectCell( c );
				ScrollCellIntoFrame( c );
			}
			return true;
			break;
		
		case char_DownArrow:
			if ( GetNextCell( c ) )
			{
				SelectCell( c );
				ScrollCellIntoFrame( c );
			}
			return true;
			break;
		
		case char_Home:
			c.row = 1;
			if ( IsValidCell( c ) )
			{
				SelectCell( c );
				ScrollCellIntoFrame( c );
			}
			return true;
			break;
		
		case char_End:
			{
			TableIndexT	rows, cols;
			GetTableSize( rows, cols );
			c.row = rows;
			if ( IsValidCell( c ) )
			{
				SelectCell( c );
				ScrollCellIntoFrame( c );
			}
			return true;
			}
			break;
	}
	
	return LCommander::HandleKeyPress( inKeyEvent );
}


void CStringListView::DontBeTarget()
{
	UnselectAllCells();
}


void CStringListView::BeTarget()
{

}


void CStringListView::DrawCell( const STableCell &inCell, const Rect &inLocalRect )
{
	Rect	textRect = inLocalRect;
	::InsetRect(&textRect, 2, 0);
	
	Str255	str;
	Uint32	len = sizeof(str);
	GetCellData(inCell, str, len);
	
	Int16 just = teJustLeft;
	UTextDrawing::DrawWithJustification((Ptr) str, len, textRect, just);
}


void CStringListView::ClickCell( const STableCell &inCell, const SMouseDownEvent &inMouseDown )
{
	LTableView::ClickCell( inCell, inMouseDown );
	
	// technically this isn't 100% accurate since the cell might already be selected
	// in which case the selection hasn't changed...
	BroadcastMessage( msg_SelectionChanged, this );
}
