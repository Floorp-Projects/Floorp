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

#include "mfinder.h"
#include "macutil.h"
#include "miconutils.h"
#include "macgui.h"
#include "resgui.h"

#include "UStdBevels.h"

const Int32 kOutsideSlop = 0x80008000;

extern void HiliteRect( const Rect& r );

// pkc (6/14/96) Set sTruncChar to 0 initially
static unsigned char sTruncChar = 0;

LFinderHeader::LFinderHeader( LStream* inStream ):
	LView( inStream )
{
	fTextTraits = 130;
	fFinder = NULL;
}

void LFinderHeader::DrawColumn( UInt16 column )
{
	CStr255		text;
	Rect		columnRect;
	Style		style;
	
	if ( fFinder )
	{
		UGraphics::SetFore( CPrefs::Black );
		
		::PmBackColor(eStdGray86);
		::PenPat( &qd.black );

		fFinder->GetColumnTitle( column, text );
		fFinder->GetColumnRect( 1, column, columnRect );
		columnRect.top = 0;
		columnRect.bottom = mFrameSize.height;
		if ( ( columnRect.right - columnRect.left ) > 2 )
		{
			::EraseRect( &columnRect );
			Rect		tmp;
			
			tmp.top = columnRect.top;
			tmp.right = columnRect.right - 2;
			tmp.left = columnRect.left + 2;
			tmp.bottom = columnRect.bottom;
			
			if ( text[ 1 ] == '#' )
			{
				Int32		resID;
				Handle		iconSuite;
				
				myStringToNum( text, &resID ); 
				iconSuite = CIconList::GetIconSuite( resID );
				if ( iconSuite )
				{
					tmp.right = tmp.left + 16;
					if ( columnRect.right - columnRect.left > 16 )
						fFinder->DrawIcon( iconSuite, FALSE, tmp );
				}
			}
			else
			{
				style = fFinder->ColumnTextStyle( column );
				fFinder->DrawText( style, text, tmp, smTruncEnd, fTextTraits, tmp.bottom - 2 );
			}
			FrameButton( columnRect, FALSE );
		}
	}
}

void LFinderHeader::DrawSelf()
{
	UInt16		columns;
	
	if ( !fFinder )
		return;
		
	columns = fFinder->GetNumberOfColumns();
	
	UTextTraits::SetPortTextTraits( fTextTraits );

	for ( UInt16 i = 0; i < columns; i++ )
		DrawColumn( i );
}

void LFinderHeader::ClickSelf( const SMouseDownEvent& where )
{
	Int16		column;
	
	column = fFinder->InResizableColumnBar( where.whereLocal );
	
	if ( column != NO_COLUMN )
		fFinder->TrackResizeColumn( where, 1, column );
	else
	{
		column = fFinder->InColumn( where.whereLocal );
		if ( column != NO_COLUMN )
			this->TrackReorderColumns( where, column );
	}
}

void LFinderHeader::TrackReorderColumns( const SMouseDownEvent& where,
	UInt16 column )
{
	Point			whereP;
	Rect			columnRect;
	Rect			viewRect;
	RgnHandle		myRgn;
	Int16			delta;
	Int32			result;
	Int16			endColumn;
	
	if ( !this->FocusDraw() )
		return;
	
	whereP = where.whereLocal;
	fFinder->GetColumnRect( 1, column, columnRect );
	this->CalcLocalFrameRect( viewRect );
	
	if (	LDragAndDrop::DragAndDropIsPresent() && 
			::WaitMouseMoved( where.macEvent.where ) )
	{
		columnRect.top = viewRect.top++;
		columnRect.bottom = viewRect.bottom--;
		myRgn = ::NewRgn();
		::RectRgn( myRgn, &columnRect );
		
		result = ::DragGrayRgn(	myRgn, whereP, 
					&viewRect, &viewRect, hAxisOnly, NULL );		
	
		::DisposeRgn( myRgn );
	
		delta = LoWord( result );
		if ( ( result == kOutsideSlop ) || ( delta == 0 ) ) 
			return;
	
		whereP.h += delta;
		endColumn = fFinder->InColumn( whereP );
		if ( endColumn != NO_COLUMN && endColumn != column )
			fFinder->SwapColumns( endColumn, column );
	}
	else
		fFinder->SortByColumnNumber( column, (where.macEvent.modifiers & optionKey) != 0 );
}

void LFinderHeader::AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent )
{
	Int16 		i;

	PortToLocalPoint( inPortPt );
	i = fFinder->InResizableColumnBar( inPortPt );

	if ( i != NO_COLUMN )
		::SetCursor( *(::GetCursor( curs_HoriDrag ) ) );
	else
		LView::AdjustCursorSelf( inPortPt, inMacEvent );
}




LFinderView::LFinderView( LStream* inStream ):
	LView( inStream ),
	LDragAndDrop( GetMacPort(), this )
{
	fTextTraits = 130;
	for ( long i = 0; i < NUM_COLUMNS; i++ )
	{
		fColumnOffsets[ i ] = 0;
		fColumnIDs[ i ] = 0;
	}
	
	fNumberColumns = 1;
	fClipImage = FALSE;
	fAllowsRectSelection = TRUE;	
	fAllowsKeyNavigation = TRUE;
	fHighlightRow = TRUE;
	fHeader = NULL;
	fForeColor.red = fForeColor.green = fForeColor.blue = 0x0000;// black
	fBackColor.red = fBackColor.green = fBackColor.blue = 0xFFFF;// white 
	this->CaculateFontRect();
}

void LFinderView::CaculateFontRect()
{
	FontInfo		fontInfo;
	UInt16			fontHeight;
	
	fontInfo = SafeGetFontInfo( fTextTraits );
	fontHeight = fontInfo.ascent + fontInfo.descent + fontInfo.leading;
	fCellHeight = fontHeight < 16 ? 16 : fontHeight;
	fBaseline = fCellHeight - fontInfo.descent + fontInfo.leading;
}

void LFinderView::FinishCreateSelf()
{
	LView::FinishCreateSelf();
	
	LFinderHeader*		header = NULL;
	char				id[ 4 ];
	PaneIDT				paneID;
		
	*((PaneIDT*)id) = mPaneID;
	id[ 0 ] = 'H';
	paneID = *((PaneIDT*)id);
	header = (LFinderHeader*)mSuperView->FindPaneByID( paneID );
	if ( header )
		header->SetFinderView( this );
	fHeader = header;
}
		
void LFinderView::ResizeFrameBy( Int16 inWidthDelta, Int16 inHeightDelta, Boolean inRefresh )
{
	LView::ResizeFrameBy( inWidthDelta, inHeightDelta, inRefresh );
	if ( fClipImage )
	{
		SDimension16		frameSize;
			
		this->GetFrameSize( frameSize );
		this->ResizeImageTo( frameSize.width, mImageSize.height, inRefresh );
	}
}

// ¥¥ activate/deactivate feedback
void LFinderView::ActivateSelf()
{
	LView::ActivateSelf();
	RefreshSelectedCells();
}

void LFinderView::DeactivateSelf()
{
	LView::DeactivateSelf();
	RefreshSelectedCells();
}

void LFinderView::RefreshSelectedCells()
{
	UInt32		topCell;
	UInt32		bottomCell;
	
	GetVisibleCells( topCell, bottomCell );
	if ( topCell == NO_CELL )
		return;
		
	for ( UInt32 cell = topCell; cell <= bottomCell; cell++ )
	{
		if ( this->IsCellSelected( cell ) )	// For each selected cell, add the outline to the region
			RefreshCells( cell, cell, FALSE );
	}
}

Boolean LFinderView::ObeyCommand( CommandT inCommand, void* ioParam )
{
	switch ( inCommand )
	{
		case msg_TabSelect:
			if ( !IsEnabled() )
				return FALSE;
			else
				return TRUE;
		break;
	}
	return LCommander::ObeyCommand( inCommand, ioParam );
}

// ¥¥ drawing/events
Boolean LFinderView::HandleKeyPress( const EventRecord& inEvent )
{
	Char16		key = inEvent.message;
	Char16		character = key & charCodeMask;

	if ( fAllowsKeyNavigation )
	{
		switch ( character )
		{
			case char_UpArrow:		this->KeyUp( inEvent );		return TRUE;
			case char_DownArrow:	this->KeyDown( inEvent );	return TRUE;
			case char_Home:			this->KeyHome( inEvent );	return TRUE;
			case char_End:			this->KeyEnd( inEvent );	return TRUE;
			case char_PageUp:		this->KeyPageUp( inEvent );	return TRUE;
			case char_PageDown:		this->KeyPageDown( inEvent );	return TRUE;
			case char_Return:
			case char_Enter:		this->KeyEnter( inEvent );	return TRUE;
		}
	}
	return LCommander::HandleKeyPress( inEvent );
}

void LFinderView::KeyUp( const EventRecord& event )
{
	UInt32		cell;

	cell = this->FirstSelectedCell();
	
	if ( cell > 1 )
		this->SelectItem( event, cell - 1, TRUE ); 
}

void LFinderView::KeyDown( const EventRecord& event )
{
	UInt32		cell;
	
	cell = this->FirstSelectedCell();
	
	if ( cell < this->GetVisibleCount() )
		this->SelectItem( event, cell + 1, TRUE );
}

void LFinderView::KeyEnter( const EventRecord& event )
{
	UInt32		cell;
	
	cell = this->FirstSelectedCell();
	
	if ( cell > 0 )
		this->DoDoubleClick( cell, event );
}

void LFinderView::KeyHome( const EventRecord& event )
{
	if ( this->GetVisibleCount() > 0 )
		this->SelectItem( event, 1, TRUE );
}

void LFinderView::KeyEnd( const EventRecord& event )
{
	if ( this->GetVisibleCount() > 0 )
		this->SelectItem( event, this->GetVisibleCount(), TRUE );
}

void LFinderView::KeyPageUp( const EventRecord& /*event*/ )
{
}

void LFinderView::KeyPageDown( const EventRecord& /*event*/ )
{
}

Boolean LFinderView::ColumnText( UInt32 /*cell*/, UInt16 /*column*/, CStr255& text )
{
	text = CStr255::sEmptyString;
	
	return FALSE;
}

// Figure out what cells have been exposed, and draw them
void LFinderView::DrawSelf()
{
	RgnHandle	localUpdateRgnH;
	Rect		updateRect;
	UInt32		topCell;
	UInt32		bottomCell;
	SPoint32	tl;
	SPoint32	br;
	
	localUpdateRgnH = GetLocalUpdateRgn();
	updateRect = (**localUpdateRgnH).rgnBBox;
	DisposeRgn( localUpdateRgnH );
	
	this->LocalToImagePoint( topLeft( updateRect ), tl );
	this->LocalToImagePoint( botRight( updateRect ), br );
	
	topCell = FetchCellAt( tl );
	bottomCell = FetchCellAt( br );
	
	::EraseRect(&updateRect);
	if ( topCell == NO_CELL )
		return;
		
	if ( bottomCell >= topCell )
	{
		UTextTraits::SetPortTextTraits( fTextTraits );
		for ( UInt32 i = topCell; i <= bottomCell; i++ )
			DrawCellAt( i );
	}
}

void LFinderView::RefreshCells( Int32 first, Int32 last, Boolean rightAway )
{
	if ( rightAway && FocusDraw() && IsVisible() )
	{
		UTextTraits::SetPortTextTraits( fTextTraits );

		if ( first < 1 )
			first = 1;

		if ( last > this->GetVisibleCount() )
			last = this->GetVisibleCount();
			
		for ( Int32 i = first; i <= last; i++ )
			DrawCellAt( i );
	}
	else if	( FocusDraw() & IsVisible())
	{
		Rect			refresh;
		
		Rect			r1;
		this->GetCellRect( first, r1 );

		refresh.top = r1.top;
		refresh.left = 0;
		refresh.right = r1.right;

		if ( last == LAST_CELL )
		{
			SPoint32		bigImageSize;
			Point			imageSize;

			bigImageSize.v = mImageSize.height;
			bigImageSize.h = mImageSize.width;
		
			this->ImageToLocalPoint( bigImageSize, imageSize );
			refresh.bottom = imageSize.v;
			refresh.right = imageSize.h;
		}
		else
		{
			Rect		r2;
			this->GetCellRect( last, r2 );
			refresh.bottom = r2.bottom;
		}
		
		::InvalRect( &refresh );
	}
}

void LFinderView::RemoveColumn( UInt16 column )
{
	Int16		count;
	UInt16		width;
	
	if ( column >= ( fNumberColumns - 1 ) )
		width = mImageSize.width - fColumnOffsets[ column ];
	else
		width = fColumnOffsets[ column + 1 ] - fColumnOffsets[ column ];

	for ( count = column; count < ( fNumberColumns - 1 ); count++ )
	{
		fColumnIDs[ count ] = fColumnIDs[ count + 1 ];
		fColumnOffsets[ count ] = fColumnOffsets[ count + 1 ] - width;
	}
	
	fColumnIDs[ fNumberColumns - 1 ] = 0;
	fColumnOffsets[ fNumberColumns - 1 ] = 0;
	
	fNumberColumns--;
}

void LFinderView::RefreshHeader()
{
	if ( fHeader )
		fHeader->Refresh();
}

void LFinderView::InsertColumn( UInt16 before, UInt16 columnID, UInt16 width )
{
	Int16		count;
	
	for ( count = fNumberColumns; count > before; count-- )
	{
		fColumnIDs[ count ] = fColumnIDs[ count - 1 ];
		fColumnOffsets[ count ] = fColumnOffsets[ count - 1 ] + width;
	}
	
	fColumnIDs[ before ] = columnID;
	
	if ( before == fNumberColumns )
		fColumnOffsets[ before ] = fColumnOffsets[ before - 1 ] + width;
		
	fNumberColumns++;
}
	
void LFinderView::SwapColumns( UInt16 columnA, UInt16 columnB )
{
	if ( columnA != columnB )
	{
		Int16		width;
		Int16		columnID;

		columnID = fColumnIDs[ columnB ];
		if ( columnB >= ( fNumberColumns - 1 ) )
			width = mImageSize.width - fColumnOffsets[ columnB ];
		else
			width = fColumnOffsets[ columnB + 1 ] - fColumnOffsets[ columnB ];

		this->RemoveColumn( columnB );
		this->InsertColumn( columnA, columnID, width );
		this->Refresh();
		if ( fHeader )
			fHeader->Refresh();
	}
}

#define SAVE_VERSION	25
void LFinderView::SavePlace( LStream* inStream )
{
	WriteVersionTag( inStream, SAVE_VERSION );
	inStream->WriteData( &fNumberColumns, sizeof( fNumberColumns ) );
	
	for ( Int16 i = 0; i < fNumberColumns; i++ )
	{
		inStream->WriteData( &fColumnOffsets[ i ], sizeof( fColumnOffsets[ 0 ] ) );
		inStream->WriteData( &fColumnIDs[ i ], sizeof( fColumnIDs[ 0 ] ) );
	}
}

void LFinderView::RestorePlace( LStream* inStream )
{
	Int16		numColumns;
	Int16		columnOffset;
	Int16		columnID;
	
	if ( !ReadVersionTag( inStream, SAVE_VERSION ) )
		return;

	inStream->ReadData( &numColumns, sizeof( fNumberColumns ) );
	
	XP_ASSERT( numColumns < NUM_COLUMNS );
	
	if ( numColumns < 1 || numColumns > NUM_COLUMNS )
		return;
		
	for ( Int16 i = 0; i < numColumns; i++ )
	{
		inStream->ReadData( &columnOffset, sizeof( fColumnOffsets[ 0 ] ) );
		inStream->ReadData( &columnID, sizeof( fColumnIDs[ 0 ] ) );
		
		fColumnOffsets[ i ] = columnOffset;
		fColumnIDs[ i ] = columnID;
	}
}

Boolean LFinderView::DispatchClick( const SMouseDownEvent& where, UInt32 cell, Int16 column )
{
	Boolean			handled = FALSE;
	
	switch ( column )
	{
		case HIER_COLUMN:
		{
			Rect		rect;

			if ( cell == NO_CELL && fAllowsRectSelection )
				this->TrackSpace( where, cell );
			else
			{
				this->GetTextRect( cell, rect );
				if ( ::PtInRect( where.whereLocal, &rect ) )
					this->TrackCell( where, cell );
				else
				{
					this->GetIconRect( cell, rect );
					if ( ::PtInRect( where.whereLocal, &rect ) )
						this->TrackCell( where, cell );
					else
					{
						this->GetWiglyRect( cell, rect );
						if (	this->CanDrawHierarchy( cell ) &&
								this->IsCellHeader( cell ) &&
								::PtInRect( where.whereLocal, &rect ) )
						{
							this->TrackWigly( where, cell );
						}
						else
						{
							if ( fAllowsRectSelection )
								this->TrackSpace( where, cell );
						}
					}
				}
			}
			handled = TRUE;
		}
		break;

		case NO_COLUMN:
		{
			if ( fAllowsRectSelection )
				this->TrackSpace( where, cell );
			else if ( cell != NO_CELL )
				this->SelectItem( where.macEvent, cell, TRUE );
			handled = TRUE;
		}
		break;
		
		default:
		break;
	}
	return handled;
}

void LFinderView::SortByColumnNumber( UInt16 /*column*/, Boolean /*switchOrder*/ )
{
}

// Complex -- click on wigly, just track this click
void LFinderView::ClickSelf( const SMouseDownEvent& where )
{
	SPoint32		whereImage;
	Int16			column;
	Boolean			inBar;
	UInt32			cell;
	
	LocalToImagePoint( where.whereLocal, whereImage );

	cell = this->FetchCellAt( whereImage );
	column = this->GetClickKind( where, cell, inBar );

	if ( inBar )
	{
		this->TrackResizeColumn( where, cell, column );
	}
	else
	{
		if ( this->DispatchClick( where, cell, column ) && GetClickCount() < 2 )
		{
			// We only switch the target if it's not a double-click. Otherwise,
			// the double-click dispatch may have caused a target change and
			// we would not want to undo that.
			this->SwitchTarget( this );
		}
	}
}

void LFinderView::DoResizeColumn( Int16 column, Int16 delta, Boolean inRefresh )
{
	for ( long i = column + 1; i < fNumberColumns; i++ )
		fColumnOffsets[ i ] += delta;

	if ( inRefresh && fHeader )
	{
		if ( fHeader )
			fHeader->Refresh();
		this->Refresh();
	}
}

void LFinderView::TrackResizeColumn( const SMouseDownEvent& where, UInt32 cell, Int16 column )
{
	Rect			columnRect;
	Rect			viewRect;
	RgnHandle		myRgn;
	Int16			delta;
	Int32			result;

	if ( !this->FocusDraw() )
		return;
	
	this->GetColumnRect( cell, column, columnRect );
	this->CalcLocalFrameRect( viewRect );
	
	columnRect.left = columnRect.right - 1;
	columnRect.right = columnRect.right + 1;
	if ( fHeader )
	{
		SDimension16		size;
		fHeader->GetFrameSize( size );
		columnRect.top -= size.height;
		viewRect.top -= size.height;
	}
	columnRect.top = viewRect.top--;
	columnRect.bottom = viewRect.bottom++;
	myRgn = ::NewRgn();
	::RectRgn( myRgn, &columnRect );
	
	viewRect.left = fColumnOffsets[ column ];
	if ( viewRect.left < 1 )
		viewRect.left = 1;
	viewRect.right -= 1;

	result = ::DragGrayRgn(	myRgn, where.whereLocal, 
				&viewRect, &viewRect, hAxisOnly, NULL );		

	::DisposeRgn( myRgn );

	delta = LoWord( result );
	if ( ( result == kOutsideSlop ) || ( delta == 0 ) ) 
		return;

	this->DoResizeColumn( column, delta, TRUE );
}

UInt32 LFinderView::FetchCellAt( SPoint32& imagePt )
{
	UInt32		cell = NO_CELL;
	
	cell = ( imagePt.v / fCellHeight ) + 1;
	if ( this->GetVisibleCount() < cell )
		cell = this->GetVisibleCount();
	return cell;
}

void LFinderView::GetVisibleCells( UInt32& top, UInt32& bottom )
{
	SPoint32		loc;
	
	loc.v = mFrameLocation.v - mImageLocation.v;
	loc.h = mFrameLocation.h - mImageLocation.h;
	
	top = this->FetchCellAt( loc );
	
	loc.v += ( mFrameSize.height - 1 );
	loc.h += ( mFrameSize.width - 1 );
	
	bottom = this->FetchCellAt( loc );	
}

void LFinderView::RevealCell( UInt32 cell )
{
	XP_ASSERT( cell );

	if ( this->FocusDraw() )
	{
		SPoint32		pt;
		Point			localPt;
		Rect			frame;
		
		pt.h = 0;
		pt.v = ( cell - 1 ) * fCellHeight;
		this->CalcLocalFrameRect( frame );
		this->ImageToLocalPoint( pt, localPt );

		if ( localPt.v < frame.top )
			this->ScrollImageTo( 0, pt.v, TRUE );
		
		if ( ( localPt.v + fCellHeight ) > ( frame.bottom - 1 ) )
			this->ScrollImageTo( 0, ( pt.v + fCellHeight ) - mFrameSize.height + 1, TRUE );
	}
}

void LFinderView::GetColumnTitle( UInt16 column, CStr255& title )
{
	::GetIndString( title, mUserCon, fColumnIDs[ column ] + 1 );
}

void LFinderView::SetForeColor( const RGBColor& fore )
{
	fForeColor = fore;
}

void LFinderView::SetBackColor( const RGBColor& back )
{
	fBackColor = back;
}

Boolean LFinderView::FocusDraw(LPane* /*inSubPane*/)
{
	Boolean		focus;
	
	focus = LView::FocusDraw();
	if ( focus )
	{
		UGraphics::SetIfBkColor( fBackColor );
		UGraphics::SetIfColor( fForeColor );
	}

	return focus;
}

Boolean LFinderView::TrackHeader( const SMouseDownEvent& /*where*/, UInt16 clickWhere )
{
	Boolean				hilited = FALSE;
	Rect				whichRect;

	if ( !this->FocusDraw() )
		return FALSE;
		
	this->GetColumnRect( 1, clickWhere, whichRect );
	whichRect.top = 1;
	whichRect.bottom = fCellHeight - 2;
	
	while ( ::Button() )
	{
		SystemTask();
		FocusDraw();

		Point				mouse;
		::GetMouse( &mouse );
		// ¥Êidling
		// ¥ user feedback
		if ( PtInRect( mouse, &whichRect ) )
		{
			if ( !hilited )
			{
				hilited = TRUE;
				HiliteRect( whichRect );
			}
		}
		else
		{
			if ( hilited )
			{
				hilited = FALSE;
				HiliteRect( whichRect );
			}
		}
	}
	
	return hilited;	
}


void LFinderView::DrawCellAt( UInt32 cell )
{
	Rect		cellRect;
	
	this->GetCellRect( cell, cellRect );

	FocusDraw();
	::EraseRect( &cellRect );
		
	// ¥Êdo not draw if we are not visible
	// conversion of local to image coordinates (see LocalToImagePoint)
	if ( !ImageRectIntersectsFrame(	cellRect.left - mImageLocation.h - mPortOrigin.h,
									cellRect.top - mImageLocation.v - mPortOrigin.v,
									cellRect.right - mImageLocation.h - mPortOrigin.h,
									cellRect.bottom - mImageLocation.v - mPortOrigin.v ) )
		return;

	for ( long i = 0; i < fNumberColumns; i++ )
		this->DrawCellColumn( cell, i );
		
	this->HighlightCell( cell );
}

void LFinderView::PutOnDuty(LCommander* inNewTarget)
{
	LCommander::PutOnDuty(inNewTarget);
	RefreshSelectedCells();
}

void LFinderView::TakeOffDuty()
{
	LCommander::TakeOffDuty();
	RefreshSelectedCells();
}

void LFinderView::GetWiglyRect( UInt32 cell, Rect& where )
{
	where.bottom = ( cell * fCellHeight ) + mPortOrigin.v + mImageLocation.v + 1;
	where.top = where.bottom - ICON_HEIGHT;
	where.left = WIGLY_LEFT_OFFSET + mPortOrigin.h + mImageLocation.h;
	where.right = where.left + ICON_WIDTH;
}

void LFinderView::GetIconRect( UInt32 cell, Rect& where )
{
	where.top = ( ( cell - 1 ) * fCellHeight ) + mPortOrigin.v + mImageLocation.v;
	where.bottom = where.top + ICON_HEIGHT;
	where.left = ( this->CellIndentLevel( cell ) * LEVEL_OFFSET ) +
		WIGLY_LEFT_OFFSET + ICON_WIDTH +
		mPortOrigin.h + mImageLocation.h;
	where.right = where.left + ICON_WIDTH;
}

void LFinderView::GetTextRect( UInt32 cell, Rect& where )
{
	where.top = ( ( cell - 1 ) * fCellHeight ) +
		mPortOrigin.v + mImageLocation.v;
	where.bottom = where.top + fCellHeight;
	where.left = ( this->CellIndentLevel( cell ) * LEVEL_OFFSET ) + 
		mPortOrigin.h + mImageLocation.h +
		WIGLY_LEFT_OFFSET + (2 * ICON_WIDTH);
		
	CStr255		text;
	this->CellText( cell, text );
	long width = ::StringWidth( text );
	if ( width )
		where.right = where.left + width + 2;
	else
		where.right = where.left;
}

void LFinderView::DrawIcon( Handle iconSuite, Boolean selected, const Rect& where )
{
	OSErr		err;
	
	if ( !iconSuite )
		return;
	
	if ( !selected )
		err = ::PlotIconSuite( &where, atHorizontalCenter | atVerticalCenter,
					ttNone, iconSuite );
	else
		err = ::PlotIconSuite( &where, atHorizontalCenter | atVerticalCenter,
					ttSelected, iconSuite );
}

void LFinderView::DrawText( const Style& style, const CStr255& text, const Rect& where,
	const TruncCode& trunc )
{
	DrawText(style, text, where, trunc, fTextTraits, fBaseline );
}

void LFinderView::DrawText( const Style& style, const CStr255& text, const Rect& where,
	const TruncCode& trunc, ResIDT inTextTraits, Int16 baseline )
{
	UTextTraits::SetPortTextTraits( inTextTraits );
	
	CStr255	texta = text;

	::TextFace( style );
	::MoveTo( where.left + 2, where.top + baseline - 2 );
	
	if ( where.right >= ( where.left + 2 ) )
	{
		SInt16	width;
		
		width = where.right - where.left - 2;
		
		if ( trunc != CUSTOM_MIDDLE_TRUNCATION )
			::TruncString( width, texta, trunc );
		else
			MiddleTruncationThatWorks( texta, width );
		
		::DrawString( texta );
	}
}

Boolean LFinderView::CanDrawHierarchy( UInt32 /*cell*/ )
{
	return TRUE;
}

void LFinderView::DrawHierarchy( UInt32 cell )
{
	Handle		iconSuite = NULL;
	Rect		where;
	CStr255		text;
	TruncCode	trunc;
	Style		textStyle;
	
	this->CellText( cell, text );
	
	// ¥Êplot the wigly
	if ( this->IsCellHeader( cell ) )
	{
		this->GetWiglyRect( cell, where );
		
		if ( this->IsHeaderFolded( cell ) )
			iconSuite = CIconList::GetIconSuite( WIGLY_CLOSED_ICON );
		else
			iconSuite = CIconList::GetIconSuite( WIGLY_OPEN_ICON );
		this->DrawIcon( iconSuite, FALSE, where );
	}

	// ¥ plot icon
	if ( this->CellIcon( cell ) != 0 )
	{
		this->GetIconRect( cell, where );
		
		iconSuite = CIconList::GetIconSuite( this->CellIcon( cell ) );
		this->DrawIcon( iconSuite,
			( this->IsCellSelected( cell ) && ( mActive == triState_On ) ),
			where );
	}

	// ¥ plot text
	textStyle = CellTextStyle( cell );
	::TextFace( textStyle );

	this->GetTextRect( cell, where );
	trunc = this->ColumnTruncationStyle( 0 );
	this->DrawText( this->CellTextStyle( cell ) , text, where, trunc );
}

void LFinderView::DrawCellColumn( UInt32 /*cell*/, UInt16 /*column*/ )
{
}

void LFinderView::HighlightCell( UInt32 cell )
{
	Rect		where;
	
	if ( fHighlightRow )
		this->GetCellRect( cell, where );
	else
	{
		this->GetTextRect( cell, where );
		where.top += 2;
		where.bottom -= 2;
	}
	
	// ¥Êdo the highlighting
	if ( this->IsCellSelected( cell ) )
	{
		if ( mActive == triState_On && this->IsOnDuty() )
		{
			LMSetHiliteMode( 0 );
			::InvertRect( &where );
		}
		else
		{
			RGBColor		hiliteColor;

			::PenPat( &qd.black );
			LMGetHiliteRGB( &hiliteColor );	/* don't put two ':' in front of this line */
			UGraphics::SetIfColor( hiliteColor );
			::FrameRect( &where );
		}
	}
}

Int16 LFinderView::InResizableColumnBar( const Point where )
{
	long		inColumnBar = NO_COLUMN;
	Int16		h = where.h;

	for ( long i = 0; i < ( fNumberColumns - 1 ); i++ )
	{
		Int16		offset;

		offset = fColumnOffsets[ i + 1 ];
				
		if ( ( h > ( offset - 2 ) ) && ( h < ( offset + 2 ) ) )
		{
			inColumnBar = i;
			break;
		}
	}

	return inColumnBar;
}

Int16 LFinderView::InColumn( const Point point )
{
	for ( long i = 0; i < fNumberColumns; i++ )
	{
		Rect		tmp;
		this->GetColumnRect( 1, i, tmp );
		if ( point.h >= tmp.left && point.h < tmp.right )
			return i;
	}
	return NO_COLUMN;
}
	
// the coordinates returned are in Local coordinates
Int16 LFinderView::GetClickKind( const SMouseDownEvent& where, UInt32 /*cell*/, 
	Boolean& inBar )
{
	Int16	i;
	
	i = this->InResizableColumnBar( where.whereLocal );

	if ( i != NO_COLUMN )
	{
		inBar = TRUE;
		return i;
	}

	inBar = FALSE;
	i = this->InColumn( where.whereLocal );
	return i;
}

void LFinderView::GetCellRect( UInt32 cell, Rect& r )
{
	Int32		localV;
	Int32		localH;
	
	localV = mPortOrigin.v + mImageLocation.v;
	localH = mPortOrigin.h + mImageLocation.h;
		
	// ¥ enclosing rectangle
	r.bottom = cell * fCellHeight + localV;
	r.top = r.bottom - fCellHeight;
	r.left = localH;
	r.right = mImageSize.width + localH;
}


void LFinderView::GetColumnRect( UInt32 cell, UInt16 column, Rect& r )
{
	Int32		localV;
	Int32		localH;
	
	localV = mPortOrigin.v + mImageLocation.v;
	localH = mPortOrigin.h + mImageLocation.h;
		
	r.bottom = cell * fCellHeight + localV; 
	r.top = r.bottom - fCellHeight;
	r.left = fColumnOffsets[ column ] + localH;
	if ( column >= ( fNumberColumns - 1 ) )
		r.right = mImageSize.width + localH;
	else
		r.right = fColumnOffsets[ column + 1 ] + localH;

	if ( r.right < r.left )
		r.right = mImageSize.width + localH;
}

			
// ¥Êtracks movement inside little finder triangle
void LFinderView::TrackWigly( const SMouseDownEvent& where, UInt32 cell )
{
	Boolean				hilited = FALSE;
	Point				mouse;
	Rect				r;
	Handle				iconSuite = NULL;
	
	this->GetWiglyRect( cell, r );

	if ( this->IsHeaderFolded( cell ) )
		iconSuite = CIconList::GetIconSuite( WIGLY_CLOSED_ICON );
	else
		iconSuite = CIconList::GetIconSuite( WIGLY_OPEN_ICON );
	
	while ( ::Button() )
	{
		// ¥Êidling
		SystemTask();
		FocusDraw();

		::GetMouse( &mouse );
		// ¥ user feedback
		if ( PtInRect( mouse, &r ) )
		{
			if ( !hilited )
			{
				hilited = TRUE;
				this->DrawIcon( iconSuite, TRUE, r );
			}
		}
		else
		{
			if ( hilited )
			{
				hilited = FALSE;
				this->DrawIcon( iconSuite, FALSE, r );
			}
		}
	}

	FocusDraw();

	if ( hilited )
	{
		iconSuite = CIconList::GetIconSuite( WIGLY_INBETWEEN_ICON );
		if ( this->IsHeaderFolded( cell ) )	// Open to closed transition
		{
			iconSuite = CIconList::GetIconSuite( WIGLY_OPEN_ICON );

			// ¥ need to erase the rect, because icons draw masked
			::EraseRect( &r );
			this->DrawIcon( iconSuite, FALSE, r );

			this->FoldHeader( cell, FALSE, TRUE, (where.macEvent.modifiers & optionKey) != 0 );
		}
		else
		{
			iconSuite = CIconList::GetIconSuite( WIGLY_CLOSED_ICON );

			// ¥ need to erase the rect, because icons draw masked
			::EraseRect( &r );
			this->DrawIcon( iconSuite, FALSE, r );

			this->FoldHeader( cell, TRUE, TRUE, (where.macEvent.modifiers & optionKey) != 0 );
		}
	}
}

Boolean LFinderView::TrackMark( UInt16 track, const SMouseDownEvent& /*where*/, UInt32 cell,
	UInt16 drawIconID, UInt16 notDrawIconID )
{
	Boolean				hilited = FALSE;
	Rect				whichRect;
	Handle				drawIcon = NULL;
	Handle				notDrawIcon = NULL;
	Boolean				selected = FALSE;
	RGBColor			backColor;
		
	if ( !this->FocusDraw() )
		return FALSE;
		
	this->GetColumnRect( cell, track, whichRect );
	
	drawIcon = CIconList::GetIconSuite( drawIconID );
	notDrawIcon = CIconList::GetIconSuite( notDrawIconID );
	
	if ( !drawIcon || !notDrawIcon )
		return FALSE;
	
	whichRect.left += 2;
	whichRect.right = whichRect.left + ICON_WIDTH;
	whichRect.bottom = whichRect.top + ICON_HEIGHT;
	
	selected = this->IsCellSelected( cell );
	if ( selected )
		LMGetHiliteRGB( &backColor );
	else
		backColor = CPrefs::GetColor( CPrefs::White );
	
	while ( ::Button() )
	{
		// ¥Êidling
		SystemTask();
		FocusDraw();

		Point				mouse;
		::GetMouse( &mouse );
		// ¥ user feedback
		if ( PtInRect( mouse, &whichRect ) )
		{
			if ( !hilited )
			{
				hilited = TRUE;
				::RGBBackColor( &backColor );			
				::EraseRect( &whichRect );
				this->DrawIcon( drawIcon, FALSE, whichRect );
			}
		}
		else
		{
			if ( hilited )
			{
				hilited = FALSE;
				::RGBBackColor( &backColor );
				::EraseRect( &whichRect );
				this->DrawIcon( notDrawIcon, FALSE, whichRect );
			}
		}
	}
	
	return hilited;	
}

void LFinderView::TrackCell( const SMouseDownEvent &where, UInt32 cell )
{
	if ( !this->FocusDraw() )
		return;
		
	if ( GetClickCount() < 2 )
	{
		this->SelectItem( where.macEvent, cell, TRUE );

		// ¥Êtricky: select the cell right away
		if (	this->IsCellSelected( cell ) && 
				LDragAndDrop::DragAndDropIsPresent() && 
				::WaitMouseMoved( where.macEvent.where ) )
		{
			// ¥ reset the drag location
			fDragTarget.where = DRAG_NONE;
			this->MakeDragTask( where );
		}
	}
	else
		this->DoDoubleClick( cell, where.macEvent );
}

void LFinderView::DoDoubleClick( UInt32 cell, const EventRecord &/*event*/)
{
	if ( this->IsCellHeader( cell ) )
	{
		Boolean		folded = this->IsHeaderFolded( cell );
		this->FoldHeader( cell, !folded, TRUE, FALSE );
		RefreshCells( cell, cell, FALSE );
	}
}

void LFinderView::TrackSpace( const SMouseDownEvent& where, UInt32 /*cell*/ )
{
	Boolean		extend;
	Rect		antsRect;
	Rect		oldAntsRect;
	Point		start;
	Point		end;
	Point		current;
	SPoint32	tmpPt;
	EventRecord		tmp;
	
	UInt32		topCell;
	UInt32		botCell;
	
	extend = ( where.macEvent.modifiers & shiftKey ) != 0;

	if ( !extend )
		this->ClearSelection();
		
	start = where.whereLocal;
	end = start;
	antsRect = RectFromTwoPoints( start, end );
	
	tmp = where.macEvent;
	tmp.modifiers |= shiftKey;
	 
	// ¥ draw ants
	DrawAntsRect( antsRect, patXor );	
	while ( ::Button() )
	{
		// ¥Êidling
		SystemTask();
		FocusDraw();
		
		// ¥Êants always drawn when entering the loop. erase on exit
		::GetMouse( &current );
		if ( ( current.v != end.v ) || ( current.h != end.h ) )
		{
			FocusDraw();
			// ¥Êerase the ants
			DrawAntsRect( antsRect, patXor );	
			AutoScrollImage( current );

			end = current;
			oldAntsRect = antsRect;
			antsRect = RectFromTwoPoints( start, end );

			this->LocalToImagePoint( topLeft( oldAntsRect ), tmpPt );
			topCell = this->FetchCellAt( tmpPt );
			this->LocalToImagePoint( botRight( oldAntsRect ), tmpPt );
			botCell = this->FetchCellAt( tmpPt );
			
			this->LocalToImagePoint( topLeft( antsRect ), tmpPt );
			topCell = MIN( topCell, this->FetchCellAt( tmpPt ) );
			this->LocalToImagePoint( botRight( antsRect ), tmpPt );
			botCell = MAX( botCell, this->FetchCellAt( tmpPt ) );
			
			for ( UInt32 i = topCell; i <= botCell; i++ )
			{
				if ( SectCellRect( i, oldAntsRect ) != SectCellRect( i, antsRect ) )
					this->SelectItem( tmp, i, TRUE );
			}
			FocusDraw();
			// ¥Êdraw ants
			DrawAntsRect( antsRect, patXor );
		}
	}
	// ¥ draw ants
	DrawAntsRect( antsRect, patXor );
}

// ¥Êdoes the icon, or the text of the cell sect this rect?
Boolean LFinderView::SectCellRect( UInt32 cell, Rect r )
{
	Rect			iconRect;
	Rect			textRect;
	Rect			dummy;

	this->GetIconRect( cell, iconRect );
	this->GetTextRect( cell, textRect );
		
	return ( ::SectRect( &iconRect, &r, &dummy ) ||
			 ::SectRect( &textRect, &r, &dummy ) );
}

void LFinderView::LocalToGlobalRect( Rect& r )
{
	Point		p1;
	Point		p2;
	
	p1.h = r.left; p1.v = r.top;
	p2.h = r.right; p2.v = r.bottom;
	
	LocalToPortPoint( p1 );
	LocalToPortPoint( p2 );
	PortToGlobalPoint( p1 );
	PortToGlobalPoint( p2 );
	
	r.left = p1.h;
	r.top = p1.v;
	r.right = p2.h;
	r.bottom = p2.v;
}


// User feedback on dragging
void LFinderView::InsideDropArea( DragReference /*inDragRef*/ )
{
	Point			mouse;
	SPoint32		imageMouse;
	DropLocation	newDropLoc;
	Rect			enclosingRect;
	UInt32			cell;
	Boolean			nearTop;
	Boolean			nearBottom;
		
	if ( !this->FocusDraw() )
		return;
		
	// ¥ find out where the mouse is
	::GetMouse( &mouse );

	this->LocalToImagePoint( mouse, imageMouse );
	
	// ¥Êget information about the cell
	cell = this->FetchCellAt( imageMouse );
	if ( cell > 0 && cell <= this->GetVisibleCount() )
	{
		newDropLoc.cell = cell;
		this->GetCellRect( newDropLoc.cell, enclosingRect );
		nearTop = imageMouse.v < ( ( cell - 1 ) * fCellHeight + 2 );
	 	nearBottom = imageMouse.v > ( cell * fCellHeight + 2 );

		// ¥ cannot drag onto a selected cell?
		if ( this->IsCellSelected( newDropLoc.cell ) )
			newDropLoc.where = DRAG_NONE;
		else if ( this->IsCellHeader( newDropLoc.cell ) )	// Header 
		{
			if ( nearTop )				// Near the top of the header
			{
				newDropLoc.cell -= 1;
				newDropLoc.where = DRAG_AFTER;
			}
			else if ( nearBottom )		// Near the bottom of the header
			{
				newDropLoc.where = DRAG_AFTER;				
			}
			else						// Over the icon
				newDropLoc.where = DRAG_INSIDE;
		}
		else							// We are over a cell, the boundary is the half of the cell
			if (imageMouse.v < ((cell * fCellHeight) - (fCellHeight / 2)))
			{
				newDropLoc.cell -= 1;
				newDropLoc.where = DRAG_AFTER;
			}
			else
				newDropLoc.where = DRAG_AFTER;
	
		if (	( newDropLoc.cell == fDragTarget.cell ) && 
				( newDropLoc.where == fDragTarget.where ) )
			return;
	
		HighlightDropLocation( FALSE );
		fDragTarget = newDropLoc;
		HighlightDropLocation( TRUE );
	}
	XP_TRACE(("cell: %d, where: %d\n", fDragTarget.cell, fDragTarget.where ));
}

void LFinderView::LeaveDropArea( DragReference inDragRef )
{
	LDragAndDrop::LeaveDropArea( inDragRef );
	HighlightDropLocation( FALSE );

	fDragTarget.cell = 0;
	fDragTarget.where = DRAG_NONE;
	
	
	Point mouse;
	Rect  frame;
	
	FocusDraw();
	::GetMouse(&mouse);
	CalcLocalFrameRect(frame);
	
	// yippee
	fDragLeaveDirection.top = fDragLeaveDirection.left = 
	fDragLeaveDirection.bottom = fDragLeaveDirection.right = false;
	
	
	// Remember which way the cursor left us, because those are the only directions
	// we'll allow autoscrolling until we are reentered.
	
	if ( ! (fDragLeaveDirection.left = (mouse.h < frame.left)))
		fDragLeaveDirection.right = (mouse.h > frame.right);
		
	if ( ! (fDragLeaveDirection.top = (mouse.v < frame.top)))
		fDragLeaveDirection.bottom = (mouse.v > frame.bottom);

}

// TRUE to show highlight, FALSE to hide
void LFinderView::HighlightDropLocation( Boolean show )
{
	Rect			textRect;
	Rect			iconRect;
	Handle			iconSuite;
	
	if ( !this->FocusDraw() )
		return;
		
	if ( fDragTarget.where == DRAG_NONE )
		return;

	if ( this->IsCellSelected( fDragTarget.cell ) && show )
		return;
		
	this->GetTextRect( fDragTarget.cell, textRect );
	this->GetIconRect( fDragTarget.cell, iconRect );
	
	switch ( fDragTarget.where )
	{
		case DRAG_NONE:
		break;
		
		case DRAG_INSIDE:
			iconSuite = CIconList::GetIconSuite( this->CellIcon( fDragTarget.cell ) );
			this->DrawIcon( iconSuite, show, iconRect );
		break;
		
		case DRAG_AFTER:
			::PenMode( patXor );
			::PenPat( &qd.black );
			::PenSize( 1, 2 );
			MoveTo( textRect.left + 1, textRect.bottom - 1 );
			LineTo( textRect.right - 1, textRect.bottom - 1 );
			::PenSize( 1, 1 );
		break;
	}
}	


void LFinderView::AddFlavors( DragReference /*inDragRef*/ )
{
}

// For each visible selected cell, add it to the drag rgn
void LFinderView::MakeDragRegion( DragReference /*inDragRef*/, RgnHandle dragRegion )
{
	UInt32			topCell;
	UInt32			bottomCell;

	GetVisibleCells( topCell, bottomCell );

	StRegion		tempOutRgn;
	StRegion		tempInRgn;

	for ( UInt32 cell = topCell; cell <= bottomCell; cell++ )
	{
		// ¥ for each selected cell, add the outline to the region
		if ( this->IsCellSelected( cell ) )	
		{
			Rect		iconRect;
			Rect		textRect;
			
			this->GetIconRect( cell, iconRect );
			this->GetTextRect( cell, textRect );
			
			InsetRect( &textRect, 1, 2 );
			
			// ¥ must translate local rect into global one
			LocalToGlobalRect( textRect );
			LocalToGlobalRect( iconRect );
			
			// ¥ add two rects
			RectRgn( tempOutRgn, &iconRect );
			RectRgn( tempInRgn, &iconRect );
			InsetRgn( tempInRgn, 1, 1 );

			DiffRgn( tempOutRgn, tempInRgn, tempInRgn );
			UnionRgn( tempInRgn, dragRegion, dragRegion );

			RectRgn( tempOutRgn, &textRect );	// text
			RectRgn( tempInRgn, &textRect );

			InsetRgn( tempInRgn, 1, 1 );
			DiffRgn( tempOutRgn, tempInRgn, tempInRgn );
			UnionRgn( tempInRgn, dragRegion, dragRegion );
		}
	}
}

Boolean LFinderView::ResizeTo( UInt32 newHeight, UInt32 newWidth )
{
	if ( newHeight != mImageSize.height || newWidth != mImageSize.width )
	{
		ResizeImageTo( newWidth, newHeight, TRUE );
		Rect		extraRect;	
		extraRect.bottom = mFrameSize.height;
		extraRect.right = mFrameSize.width;
		
		if ( ( newHeight < mFrameSize.height ) && FocusDraw() )
		{
			extraRect.top = newHeight;
			extraRect.left = 0;
			::EraseRect( &extraRect );
			::InvalRect( &extraRect );
		}

		if ( ( newWidth < mFrameSize.width ) && FocusDraw() )
		{
			extraRect.top = 0;
			extraRect.left = newWidth;
			::EraseRect( &extraRect );
			::InvalRect( &extraRect );
		}
		return TRUE;
	}
	else
		return FALSE;
}

void LFinderView::AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent )
{
	Int16 		i;
	PortToLocalPoint( inPortPt );
	i = this->InResizableColumnBar( inPortPt );

	if ( i != NO_COLUMN )
		::SetCursor( *(::GetCursor( curs_HoriDrag ) ) );
	else
		LView::AdjustCursorSelf( inPortPt, inMacEvent );
}

/*******************************************************************************
 * LDragFinderView
 * Generic dragger for the Finder view
 *******************************************************************************/
LDragFinderTask::LDragFinderTask( const EventRecord& inEventRecord, LFinderView* view):
	LDragTask( inEventRecord )
{
	fView = view;
	
	fTrackerProc = NewDragTrackingHandlerProc(LDragFinderTask::ScrollingTracker);
	
	XP_ASSERT(fTrackerProc != NULL);
	
	if (fTrackerProc != NULL)
		(void) ::InstallTrackingHandler(fTrackerProc, fView->GetMacPort(), fView);
}

LDragFinderTask::~LDragFinderTask()
{
	if (fTrackerProc != NULL)
	{
		(void) ::RemoveTrackingHandler(fTrackerProc, fView->GetMacPort());
		DisposeRoutineDescriptor(fTrackerProc);
	}
}


void LDragFinderTask::MakeDragRegion( DragReference inDragRef, RgnHandle inDragRegion )
{
	if ( fView )
		fView->MakeDragRegion( inDragRef, inDragRegion );
}

void LDragFinderTask::AddFlavors( DragReference inDragRef )
{
	if ( fView )
		fView->AddFlavors( inDragRef );
}



pascal OSErr
LDragFinderTask::ScrollingTracker(	DragTrackingMessage 	message, 
									WindowPtr 				/*theWindow*/, 
									void 				*	handlerRefCon, 
									DragReference 			theDragRef		)
{									
	LFinderView *	view = (LFinderView *) handlerRefCon;
	
	if (message == kDragTrackingInWindow)
		 view->DragScroll(theDragRef);
	
	return noErr;
}


	


void
MiddleTruncationThatWorks(	StringPtr	s,
							SInt16		availableSpace	)
{							
#if 1
	
	if (availableSpace > 40)
	{	
		::TruncString( availableSpace, s, truncMiddle );
	}
	else
	{
		// OK, we're trying to ship 4.0 (like yesterday) but this middle truncation stuff
		// just doesn't work for international and local versions (problem with ellipsis
		// and problem with multi-byte characters)
		// Due to these problems and other problems that haven't yet been discovered in 4.0,
		// we decided to not allow any middle truncation (allows truncEnd)

		::TruncString( availableSpace, s, truncEnd );
	}
#else
	Boolean		alreadyFit;
	SInt16		ellipsisWidth, width, halfSpace;
	UInt32		index, measureBytes;
	UInt8		length;
	unsigned char ellipsis;
	Handle		ellipsisHandle = NULL;	
	
	//
	// "Fast" case
	// 
	if ( ::StringWidth(s) <= availableSpace)
		return;		
		
	//
	// Get to know the ellipsis
	//
	// pkc (6/14/96) Hack for localization; use ellipsis character
	// loaded from resource.
	if( sTruncChar == 0 )
	{
		ellipsisHandle = ::GetResource('elps', 1000);
		if( ellipsisHandle )
		{
			// set sTruncChar and release resource
			sTruncChar = **ellipsisHandle;
			::ReleaseResource(ellipsisHandle);
		}
		else
		{
			// What should we do if we can't load the resource?
			// For now, set to roman ellipsis because I'm lazy.
			sTruncChar = 'É';
		}
	}
	ellipsis = sTruncChar;
	ellipsisWidth = ::CharWidth(ellipsis);
	if (ellipsisWidth > availableSpace)
	{
		s[0] = 0;	// cheezy bailout
		return;
	}
	
	if (ellipsisWidth == availableSpace)
	{
		s[0] = 1;
		s[1] = ellipsis;
		return;
	}
	
	
	//
	// Find block of text that takes up half of the available space
	// on the left. (minus half the ellipsis)
	//
	
	halfSpace	= ((availableSpace >> 1) - (ellipsisWidth >> 1));
	index		= halfSpace/8 + 1;		// just a beginning guess
	
	alreadyFit	= false;				
	
	do
	{
		width = ::TextWidth((char*)s, 1, index);
		
		// Too wide ?, go narrow
		if (width > halfSpace)
		{
			index--;
			
			// If we fit with the step back, bail
			if (alreadyFit)
				break;
		}
		else
		
		// Do we fit ? try wider
		if (width < halfSpace)
		{
			index++;
			alreadyFit = true;		// we'll try adding one more char, 
									// but if it fails, just bounce back 
									// to this case		
		}
		else
			break;
	
		if (index == 0)		
			break;					// no room for any text on the left
			
		XP_ASSERT(index <= s[0]);
		
	} while (true);
	
	
	// jam in the ellipsis
	s[index+1] = ellipsis;
	length = index+1;
	
	//
	// if index is 0, we have no room for anything 
	// but the ellipsis, so bail
	//
	if (index == 0)	
	{
		s[0] = 1;
		return;
	}
	
	//
	// Now find text to fit to the right of the ellipsis
	//
	
	halfSpace 		= availableSpace - (width + ellipsisWidth);
	index 			= s[0] - (halfSpace/8);						// another guess to begin
	measureBytes 	= (s[0] - index) + 1;
	alreadyFit		= false;
	
	do
	{
		width = ::TextWidth((char*)s, index, measureBytes);
	
		// If we're too wide, take a step back.
		if (width > halfSpace)
		{
			index++;
			measureBytes--;
			
			//
			// if we already know that we fit 
			// with the step back, get out 
			//
			
			if (alreadyFit)	
				break;
		}	
		
		// If we're too narrow, go wider
		else if (width < halfSpace)
		{
			index--;
			measureBytes++;
			alreadyFit = true;	// we'll try adding one more char, 
								// but if it fails, just bounce back 
								// to this case			
		}
		else
			break;
			
		
		if (index > s[0])
		{
			measureBytes = 0;	// no room for any text on the right
			break;
		}	
		
		XP_ASSERT(index > 0);
			
	} while (true);
	
	
	if (measureBytes != 0)
	{
		//
		// shift the right half of the text down to meet the ellipsis
		//
		::BlockMoveData( &s[index], &s[length+1], measureBytes);
		
		length += measureBytes;
	}
	
	s[0] = length;
	
	XP_ASSERT( ::StringWidth(s) <= availableSpace );
#endif
}	
		
void MiddleTruncationThatWorks(char *s, Int16 &ioLength, Int16 availableSpace)
{							
#if 1
	if (availableSpace > 40)
	{	
		::TruncText( availableSpace, s, &ioLength, truncMiddle );
	}
	else
	{
	// OK, we're trying to ship 4.0 (like yesterday) but this middle truncation stuff
	// just doesn't work for international and local versions (problem with ellipsis
	// and problem with multi-byte characters)
	// Due to these problems and other problems that haven't yet been discovered in 4.0,
	// we decided to not allow any middle truncation (allows truncEnd)

		::TruncText( availableSpace, s, &ioLength, truncEnd );
	}
#else
	Boolean		alreadyFit;
	SInt16		ellipsisWidth, width, halfSpace;
	UInt32		index, measureBytes;
	Int16		length = outLength;
	unsigned char ellipsis;
	Handle		ellipsisHandle = NULL;	
	
	//
	// "Fast" case
	// 
	if ( ::TextWidth(s, 0, length) <= availableSpace)
		return;		
		
	//
	// Get to know the ellipsis
	//
	// pkc (6/14/96) Hack for localization; use ellipsis character
	// loaded from resource.
	if( sTruncChar == 0 )
	{
		ellipsisHandle = ::GetResource('elps', 1000);
		if( ellipsisHandle )
		{
			// set sTruncChar and release resource
			sTruncChar = **ellipsisHandle;
			::ReleaseResource(ellipsisHandle);
		}
		else
		{
			// What should we do if we can't load the resource?
			// For now, set to roman ellipsis because I'm lazy.
			sTruncChar = 'É';
		}
	}
	ellipsis = sTruncChar;
	ellipsisWidth = ::CharWidth(ellipsis);
	if (ellipsisWidth > availableSpace)
	{
		outLength = 0;	// cheezy bailout
		return;
	}
	
	if (ellipsisWidth == availableSpace)
	{
		outLength = 1;
		s[0] = ellipsis;
		return;
	}
	
	
	//
	// Find block of text that takes up half of the available space
	// on the left. (minus half the ellipsis)
	//
	
	halfSpace	= ((availableSpace >> 1) - (ellipsisWidth >> 1));
	index		= halfSpace/8 + 1;		// just a beginning guess
	
	alreadyFit	= false;				
	
	do
	{
		width = ::TextWidth(s, 0, index + 1);
		
		// Too wide ?, go narrow
		if (width > halfSpace)
		{
			index--;
			
			// If we fit with the step back, bail
			if (alreadyFit)
				break;
		}
		else
		
		// Do we fit ? try wider
		if (width < halfSpace)
		{
			index++;
			alreadyFit = true;		// we'll try adding one more char, 
									// but if it fails, just bounce back 
									// to this case		
		}
		else
			break;
	
		if (index == -1)		
			break;					// no room for any text on the left
		
		XP_ASSERT(index < outLength);
		
	} while (true);
	
	
	// jam in the ellipsis
	s[index+1] = ellipsis;
	length = index+1;
	
	//
	// if index is 0, we have no room for anything 
	// but the ellipsis, so bail
	//
	if (index == -1)	
	{
		outLength = 1;
		return;
	}
	
	//
	// Now find text to fit to the right of the ellipsis
	//
	halfSpace 		= availableSpace - (width + ellipsisWidth);
	index 			= outLength - (halfSpace/8);						// another guess to begin
	measureBytes 	= outLength - index + 1;
	alreadyFit		= false;
	
	do
	{
		width = ::TextWidth(s, index, measureBytes);
	
		// If we're too wide, take a step back.
		if (width > halfSpace)
		{
			index++;
			measureBytes--;
			
			//
			// if we already know that we fit 
			// with the step back, get out 
			//
			
			if (alreadyFit)	
				break;
		}	
		
		// If we're too narrow, go wider
		else if (width < halfSpace)
		{
			index--;
			measureBytes++;
			alreadyFit = true;	// we'll try adding one more char, 
								// but if it fails, just bounce back 
								// to this case			
		}
		else
			break;
			
		
		if (index > outLength)
		{
			measureBytes = 0;	// no room for any text on the right
			break;
		}	
		
		XP_ASSERT(index > 0);
			
	} while (true);
	
	
	if (measureBytes != 0)
	{
		//
		// shift the right half of the text down to meet the ellipsis
		//
		::BlockMoveData( &s[index], &s[length+1], measureBytes);
		
		length += measureBytes;
	}
	
	outLength = length;
	
	XP_ASSERT( ::TextWidth(s, 0, outLength) <= availableSpace );
#endif
}	
	
	


