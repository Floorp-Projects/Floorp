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

#include "CHyperTreeHeader.h"

#include <LString.h>
#include <LCaption.h>

CHyperTreeHeader::CHyperTreeHeader(LStream* inStream)
:	CTriStateTableHeader(inStream),
		mHTView(NULL)
{
}

CHyperTreeHeader::~CHyperTreeHeader()
{
}

void
CHyperTreeHeader::SetUpColumns ( HT_View inView )
{
	LTableHeader::SColumnData columnData;
	ColumnInfo columnInfo;
	PaneIDT columnPaneID = 1;
	void *columnToken;
	Uint32 columnWidth, columnTokenType;
	SPaneInfo paneInfo;
	char *columnName;
	vector<LTableHeader::SColumnData> dataVector;

	// link up with HT. We don't own the view, so don't dispose it later.
	mHTView = inView;
	if ( !inView )
		return;
		
	// initialize SPaneInfo fields that don't change
	paneInfo.width = 	50;
	paneInfo.height = 	12;
	paneInfo.visible =	false;
	paneInfo.enabled =	false;
	paneInfo.bindings.left = false;
	paneInfo.bindings.top = false;
	paneInfo.bindings.right = false;
	paneInfo.bindings.bottom = false;
	paneInfo.left =		0;
	paneInfo.top =		0;
	paneInfo.userCon =	NULL;
	paneInfo.superView = this;

	DeleteAllSubPanes();
	mColumnInfo.clear();

	// initialize LTableHeader::SColumnData fields that don't change	
	columnData.columnPosition = 0;
	columnData.flags = 0;

	HT_Cursor columnCursor = HT_NewColumnCursor(inView);

	// get column data from cursor
	bool foundHiddenColumn = false;
	while (HT_GetNextColumn(columnCursor, &columnName, &columnWidth, &columnToken, &columnTokenType))
	{
		LStr255 name(columnName);
		
		paneInfo.paneID =	columnPaneID;
		
		LCaption* columnCaption = new LCaption(paneInfo, name, columnTextTraits_ID);
		
		columnData.paneID = columnPaneID;
		columnData.columnWidth = columnWidth;
		
		dataVector.push_back(columnData);
		
		columnInfo.token = columnToken;
		columnInfo.tokenType = columnTokenType;
		
		mColumnInfo.push_back(columnInfo);
		
		columnCaption->Enable();
		columnCaption->Show();
		
		// once we find a column that is hidden, all the rest are hidden to, so set
		// the column before to be the last visible.
		if ( !foundHiddenColumn ) {
			if ( HT_GetColumnVisibility(inView, columnToken, columnTokenType) == false ) {
				mLastVisibleColumn = columnPaneID - 1;
				foundHiddenColumn = true;
			}
		}
		
		++columnPaneID;
	}
	
	// LTableHeader expects its mColumnData member to be a mac Handle;
	// therefore, create the Handle from dataVector.
	Handle handle = ::NewHandle(sizeof(LTableHeader::SColumnData) * dataVector.size());
	// I have no clue who is going to catch this, but if we can't allocate the
	// Handle, we're up the creek without a paddle and with a very leaky canoe.
	ThrowIfMemFail_(handle);
	LHandleStream dataStream(handle);
	vector<LTableHeader::SColumnData>::iterator iter = dataVector.begin();
	while (iter != dataVector.end())
	{
		Int32 byteCount = sizeof(LTableHeader::SColumnData);
		dataStream.PutBytes(&(*iter), byteCount);
		++iter;
	}
	if (mColumnData)
		::DisposeHandle(reinterpret_cast<Handle>(mColumnData));
	mColumnData = reinterpret_cast<LTableHeader::SColumnData**>(dataStream.DetachDataHandle());
	
	// now initialize LTableHeader data members and have it sync itself up. If we didn't
	// find a hidden column above, then all columns are visible.
	mColumnCount = dataVector.size();
	if ( !foundHiddenColumn )
		mLastVisibleColumn = mColumnCount;
	mLastShowableColumn = mColumnCount;
	if ( mLastVisibleColumn <= 0 )		// mLastVisibleColumn MUST NOT BE 0 or heap corruption will occur
		mLastVisibleColumn = 1;

	ConvertWidthsToAbsolute();
	ComputeColumnPositions();
	PositionColumnHeaders(true);
	
	HT_DeleteColumnCursor(columnCursor);
}


//
// GetColumnInfo
//
// Returns the HT-specific info about a column.
// IMPORTANT: This method is 0-based NOT 1-based (like most PowerPlant stuff).
//
CHyperTreeHeader::ColumnInfo&
CHyperTreeHeader::GetColumnInfo(Uint32 inColumnKey)
{
	vector<ColumnInfo>::iterator iter = mColumnInfo.begin();
	return *(iter + inColumnKey);
} // GetColumnInfo


//
// ShowHideRightmostColumn
//
// Tell HT about the new visibility on the rightmost column
//
void
CHyperTreeHeader :: ShowHideRightmostColumn ( Boolean inShow )
{
	ColumnIndexT savedLastVisibleColumn = mLastVisibleColumn;
	
	CTriStateTableHeader::ShowHideRightmostColumn ( inShow );
	
	ColumnInfo lastColumnInfo = GetColumnInfo ( savedLastVisibleColumn - 1 );	// GCI() is 0-based
	HT_SetColumnVisibility ( mHTView, lastColumnInfo.token, lastColumnInfo.tokenType,
								inShow ? PR_FALSE : PR_TRUE );
		// logic is backwards between this method and the last param to HT_SCV()
		
} // ShowHideRightmostColumn


//
// MoveColumn
//
// Tell HT about the new location of a column
//
void
CHyperTreeHeader :: MoveColumn ( ColumnIndexT inColumn, ColumnIndexT inMoveTo )
{
	if ( inColumn == inMoveTo )
		return;
	
	ColumnIndexT adjustedColumn = inColumn - 1;		// adjust for dealing with 0-based things
	ColumnIndexT adjustedMoveTo = inMoveTo - 1;
	
	ColumnInfo movingColumn = GetColumnInfo ( adjustedColumn ); 	// copy movingColumn for later readding
	ColumnInfo & destColumn = GetColumnInfo ( adjustedMoveTo );
	HT_SetColumnOrder ( mHTView, movingColumn.token, destColumn.token, false );

	// rearrange columns in vector list. We do this by deleting the column data from the vector and
	// reinserting at its new location (which is why we needed a copy above, not just a ref). If
	// the column is being moved towards the end, decrement the destination to compensate for the
	// column data being deleted in front of it.
	vector<ColumnInfo>::iterator it = mColumnInfo.begin() + adjustedColumn;
	mColumnInfo.erase ( it );
	if ( adjustedMoveTo > adjustedColumn )
		--adjustedMoveTo;						// move it back to compensate
	it = mColumnInfo.begin() + adjustedMoveTo;
	mColumnInfo.insert ( it, movingColumn );
	
	CTriStateTableHeader::MoveColumn ( inColumn, inMoveTo ) ;
	
} // MoveColumn
