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

///////////////////////////////////////////////////////////////////////////////////////
//
//	Who			When		What
//	---			----		----
//	
//	piro		12/1/97		Finished first implementation
//
///////////////////////////////////////////////////////////////////////////////////////

#include "CSingleTextColumn.h"
#include <LStream.h>
#include <LTableMonoGeometry.h>
#include <LTableSingleSelector.h>
#include <LTableArrayStorage.h>
#include <UDrawingUtils.h>
#include <UTextTraits.h>
#include <UGAColorRamp.h>

///////////////////////////////////////////////////////////////////////////////////////
// Constants

const MessageT	kDoubleClickMsg = 'chED';
const short		kColWidth = 100;
const short		kRowHeight = 16;
const short		kStringSize = 256;
const short		kSubtractToFitInScroller = 2;

///////////////////////////////////////////////////////////////////////////////////////
// Implementation

///////////////////////////////////////////////////////////////////////////////////////
// CSingleTextColumn
CSingleTextColumn::CSingleTextColumn( LStream *inStream)
	: LTableView(inStream), mTxtrID( 0 )
{
	*inStream >> mTxtrID;
	InitSingleTextColumn();
}

///////////////////////////////////////////////////////////////////////////////////////
// ~CSingleTextColumn
CSingleTextColumn::~CSingleTextColumn()
{
}

///////////////////////////////////////////////////////////////////////////////////////
// InitSingleTextColumn()
void
CSingleTextColumn::InitSingleTextColumn()
{
	// single size geometry
	SetTableGeometry(new LTableMonoGeometry(this, mFrameSize.width - kSubtractToFitInScroller, kRowHeight));

	// single selection
	SetTableSelector(new LTableSingleSelector(this));
	
	// storage
	SetTableStorage( new LTableArrayStorage( this, (unsigned long) 0 ) );

	LTableView::InsertCols(1, 0, nil, 0, Refresh_No);
}

///////////////////////////////////////////////////////////////////////////////////////
// ClickCell( const STableCell& inCell, const SMouseDownEvent& inMouseDown )
void 		
CSingleTextColumn::ClickCell( const STableCell&, const SMouseDownEvent& )
{
	if ( GetClickCount() == 1 ) // single click
	{
		FocusDraw();
	}
	else if ( GetClickCount() == 2 ) // double click
	{
		BroadcastMessage( msg_SingleTextColumnDoubleClick, static_cast<void*>( this ) );
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// ClickCell( const STableCell& inCell, const SMouseDownEvent& inMouseDown )
void
CSingleTextColumn::DrawCell( const STableCell &inCell, const Rect &inLocalRect)
{
	Rect	textRect = inLocalRect;
	::InsetRect(&textRect, 2, 0);
	
	Str255	str;
	Uint32	len = sizeof(str);
	GetCellData(inCell, str, len);
	
	Int16	just = UTextTraits::SetPortTextTraits(mTxtrID);
	UTextDrawing::DrawWithJustification((Ptr) str + 1, str[0], textRect, just);
}

///////////////////////////////////////////////////////////////////////////////////////
// ClickSelf( const SMouseDownEvent&	inMouseDown )
void
CSingleTextColumn::ClickSelf( const SMouseDownEvent&	inMouseDown )
{
	STableCell	hitCell;
	SPoint32	imagePt;
	
	LocalToImagePoint(inMouseDown.whereLocal, imagePt);
	if ( GetCellHitBy( imagePt, hitCell ) )
			LTableView::ClickSelf( inMouseDown );
}

///////////////////////////////////////////////////////////////////////////////////////
// DrawSelf()
void
CSingleTextColumn::DrawSelf()
{
	RgnHandle localUpdateRgnH = GetLocalUpdateRgn();
	Rect updateRect = (**localUpdateRgnH).rgnBBox;
	DisposeRgn(localUpdateRgnH);
	
	{
		StColorState saveTheColor;
		RGBBackColor( &( UGAColorRamp::GetWhiteColor() ) );
		EraseRect(&updateRect);
	}
	
	LTableView::DrawSelf();
}
