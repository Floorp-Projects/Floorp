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
