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
:	CTriStateTableHeader(inStream)
{
}

CHyperTreeHeader::~CHyperTreeHeader()
{
}

void
CHyperTreeHeader::SetUpColumns(HT_Cursor columnCursor)
{
	LTableHeader::SColumnData columnData;
	ColumnInfo columnInfo;
	PaneIDT columnPaneID = 1;
	void *columnToken;
	Uint32 columnWidth, columnTokenType;
	SPaneInfo paneInfo;
	char *columnName;
	vector<LTableHeader::SColumnData> dataVector;

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

	// initialize LTableHeader::SColumnData fields that don't change	
	columnData.columnPosition = 0;
	columnData.flags = 0;

	// get column data from cursor
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
		dataStream.PutBytes(iter, byteCount);
		++iter;
	}
	if (mColumnData)
		::DisposeHandle(reinterpret_cast<Handle>(mColumnData));
	mColumnData = reinterpret_cast<LTableHeader::SColumnData**>(dataStream.DetachDataHandle());
	// now initialize LTableHeader data members and have it sync itself up
	mColumnCount = dataVector.size();
	mLastVisibleColumn = mColumnCount;
	ConvertWidthsToAbsolute();
	ComputeColumnPositions();
	PositionColumnHeaders(true);
}

CHyperTreeHeader::ColumnInfo&
CHyperTreeHeader::GetColumnInfo(Uint32 inColumnKey)
{
	vector<ColumnInfo>::iterator iter = mColumnInfo.begin();
	return *(iter + inColumnKey);
}