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

// mfinder.h

#pragma once
#include "PascalString.h"
#include "prtypes.h"

#define WIGLY_LEFT_OFFSET		0
#define ICON_WIDTH				16
#define ICON_HEIGHT				16
#define LEVEL_OFFSET			10

#define NO_COLUMN				-1
#define HEADER_ROW				-2

#define NO_CELL					0
#define LAST_CELL				0xFFFF

#define HIER_COLUMN				0

#define WIGLY_CLOSED_ICON		3060
#define WIGLY_OPEN_ICON			3061
#define WIGLY_INBETWEEN_ICON	3062
#define FOLDER_ICON				3064

#define SETFLAG( x, f )		((x)|=(f))
#define CLEARFLAG( x, f )	((x)&=~(f))

#define	NUM_COLUMNS			10

#define DRAG_NONE			0
#define DRAG_INSIDE			1
#define DRAG_AFTER			2
#define DRAG_BEFORE			3


// Gets around horrible bug in Apple's ::TruncText call
// that manifests itself in certain cases if you try to
// truncate in the middle
#define	CUSTOM_MIDDLE_TRUNCATION	666
void MiddleTruncationThatWorks(char *s, Int16 &ioLength, Int16 availableSpace);
void MiddleTruncationThatWorks(StringPtr s, SInt16 availableSpace);

class LFinderView;

typedef Point		Cell;

#include <LView.h>
#include <LDragAndDrop.h>
#include <LCommander.h>

class LFinderHeader: public LView
{
public:
	enum { class_ID = 'FNHD' };
	
						LFinderHeader();
						LFinderHeader( LStream* inStream );

	virtual void		SetFinderView( LFinderView* finder ) { fFinder = finder; }
	
	virtual void		DrawSelf();
	virtual void		ClickSelf( const SMouseDownEvent& where );
	virtual void		AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent );

	virtual void		DrawColumn( UInt16 column );
	virtual void		TrackReorderColumns( const SMouseDownEvent& where, UInt16 column );
protected:
	LFinderView*		fFinder;
	ResIDT				fTextTraits;
};

class LFinderView: public LView,
	public LDragAndDrop,
	public LCommander
{
public:
	friend class		LFinderHeader;
	
	enum { class_ID = 'FDTA' };
	
						LFinderView( LStream* inStream );
	virtual void		FinishCreateSelf();

	virtual void		CaculateFontRect();
	
	virtual Boolean		IsCellSelected( Int32 /* cell */ ) { return FALSE; }
	virtual UInt32		CellIndentLevel( Int32 /* cell */ ) { return 0; }
	virtual ResIDT		CellIcon( Int32 /* cell */ ) { return 0; }
	virtual Boolean		IsCellHeader( Int32 /* cell */ ) { return FALSE; }
	virtual Boolean		IsHeaderFolded( Int32 /* cell */ ) { return FALSE; }
	virtual void		CellText( Int32 /* cell */, CStr255& text ) 
						{ text = CStr255::sEmptyString; }
	virtual Style		CellTextStyle( Int32 /* cell */ ) { return normal; }
	virtual Style		ColumnTextStyle( UInt16 /* column */ ) { return normal; }
	virtual TruncCode	ColumnTruncationStyle( UInt16 /* column */ ) { return smTruncEnd; }
	
	// ¥¥Êoverride
	virtual void*		GetCellData( UInt32 /* cell */ ) { return NULL; }
	
	virtual Boolean		SyncDisplay( UInt32 /* visCells */ ) { return FALSE; }		// Resize the view to the number of bookmarks
	virtual void		SelectItem( const EventRecord& /* rec */, UInt32 /* cell */, Boolean /* refresh */ ) { }
	virtual void		FoldHeader( UInt32 /* cell */, Boolean /* fold */, Boolean /* refresh */, Boolean /* foldAll */ ) { }
	virtual void		DoDoubleClick( UInt32 cell, const EventRecord &event );
	virtual void		ClearSelection() { }
	virtual UInt32		GetVisibleCount() { return 0; }
	virtual UInt32		FirstSelectedCell() { return 0; }
	virtual void		GetCellRect( UInt32 cell, Rect& rect );
	virtual void		GetColumnRect( UInt32 cell, UInt16 columnID, Rect& r );
	virtual void		RevealCell( UInt32 cell );
	virtual UInt16		GetNumberOfColumns() { return fNumberColumns; }
	virtual void		GetColumnTitle( UInt16 column, CStr255& title );
	
	virtual UInt16		GetColumnHeight() { return fCellHeight; }
	
	virtual void		RefreshCells( Int32 first, Int32 last,
								Boolean rightAway );	// Using refresh, or right away?

	virtual void		SwapColumns( UInt16 columnA, UInt16 columnB );
	virtual void		SortByColumnNumber( UInt16 column, Boolean switchOrder );
	
	virtual Int16		InResizableColumnBar( const Point where );
	virtual Int16		InColumn( const Point where );
	virtual void		TrackResizeColumn( const SMouseDownEvent& where, UInt32 cell, Int16 column );

	// ¥¥ PowerPlant overrides
	virtual void		ActivateSelf();
	virtual void		DeactivateSelf();
	virtual void		DrawSelf();
	virtual Boolean		FocusDraw(LPane* inSubPane = nil);
	virtual Boolean		HandleKeyPress( const EventRecord& inEvent );
	virtual Boolean		ObeyCommand( CommandT inCommand, void* ioParam );
	virtual void 		ClickSelf( const SMouseDownEvent& where );
	virtual void		ResizeFrameBy( Int16 inWidthDelta, Int16 inHeightDelta, Boolean inRefresh );
	virtual void		PutOnDuty( LCommander* inNewTarget );
	virtual void		TakeOffDuty();
			
	// ¥¥Êdrag & drop
	virtual void		AddFlavors( DragReference inDragRef );
	virtual void		MakeDragRegion( DragReference inDragRef, RgnHandle dragRegion );

	virtual void		InsideDropArea( DragReference inDragRef );
	virtual void		LeaveDropArea( DragReference inDragRef );
	virtual void		DoDragSendData( FlavorType /* flavor */, ItemReference /* itemRef */, DragReference /* dragRef */ ) { }
	virtual Boolean		ItemIsAcceptable( DragReference /* inDragRef */, ItemReference /* inItemRef */ ) { return FALSE; }
	virtual void		ReceiveDragItem( DragReference /* inDragRef */, DragAttributes /* inDragAttrs */,
							ItemReference /* inItemRef */, Rect& /* inItemBounds */ ) { }

	virtual void		HiliteDropArea( DragReference /* inDragRef */ ) { }
			
	
	void				SetForeColor( const RGBColor& fore );
	void				SetBackColor( const RGBColor& back );

	virtual void		DrawIcon( Handle iconSuite,
							Boolean selected,
							const Rect& where );
							
	virtual void		DrawText( const Style& style, 
							const CStr255& text,
							const Rect& where,
							const TruncCode& trunc );

	virtual void		SavePlace( LStream* inStream );
	virtual void		RestorePlace( LStream* inStream );

	virtual void		RefreshHeader();

	virtual void		DragScroll(DragReference /* theDragRef */) { }
			
protected:
	struct DropLocation
	{
		UInt32		cell;
		UInt32		where;
	};

	// ¥¥ cell drawing
	// override
	virtual UInt32		FetchCellAt( SPoint32& imagePt );	// Cell at this coordinate

	virtual void		KeyUp( const EventRecord& event );
	virtual void		KeyDown( const EventRecord& event );
	virtual void		KeyEnter( const EventRecord& event );
	virtual void		KeyHome( const EventRecord& event );
	virtual void		KeyEnd( const EventRecord& event );
	virtual void		KeyPageUp( const EventRecord& event );
	virtual void		KeyPageDown( const EventRecord& event );
		
	virtual void		RefreshSelectedCells();

	// ¥¥ access	
	virtual void		GetVisibleCells( UInt32& top, UInt32& bottom );	// Get visible cells
	virtual void		DrawCellAt( UInt32 cell );			// Draw the cell
	virtual void		DrawCellColumn( UInt32 cell, UInt16 column );

	virtual Int16		GetClickKind( const SMouseDownEvent& where, UInt32 cell, 
							Boolean& inBar );	// Where did we click?
	virtual void		DoResizeColumn( Int16 column, Int16 delta, Boolean inRefresh );
	virtual Boolean		DispatchClick( const SMouseDownEvent& where, UInt32 cell, Int16 column );
	
	virtual Boolean		TrackHeader( const SMouseDownEvent& where, UInt16 column );
							
	void				LocalToGlobalRect( Rect& r );
	Boolean				SectCellRect( UInt32 cell, Rect r );	// Does cell intersect this local rect?

	virtual void		HighlightDropLocation( Boolean show );	// TRUE to show highlight, FALSE to hide
	virtual void		MakeDragTask( const SMouseDownEvent & /* where */ ) { }
	
	virtual Boolean		ResizeTo( UInt32 newHeight, UInt32 newWidth );

	virtual void		DrawHierarchy( UInt32 cell );
	virtual Boolean		CanDrawHierarchy( UInt32 cell );
	
	virtual void		TrackWigly( const SMouseDownEvent& where, UInt32 cell );
	virtual void		TrackCell( const SMouseDownEvent& where, UInt32 cell );
	virtual void		TrackSpace( const SMouseDownEvent& where, UInt32 cell );
	virtual Boolean		TrackMark( UInt16 columnID, const SMouseDownEvent& where, UInt32 cell,
							UInt16 drawIconID, UInt16 notDrawIconID );

	virtual void		GetWiglyRect( UInt32 cell, Rect& rect );
	virtual void		GetIconRect( UInt32 cell, Rect& rect );
	virtual void		GetTextRect( UInt32 cell, Rect& rect );
	virtual void		HighlightCell( UInt32 cell );

	virtual Boolean		ColumnText( UInt32 cell, UInt16 column, CStr255& text );

	virtual void		AdjustCursorSelf( Point inPortPt, const EventRecord& inMacEvent );

	virtual void		RemoveColumn( UInt16 column );
	virtual void		InsertColumn( UInt16 before, UInt16 columnID, UInt16 width );

	virtual void		DrawText( const Style& style, 
							const CStr255& text,
							const Rect& where,
							const TruncCode& trunc,
							ResIDT inTextTraits,
							Int16 baseline );
	
	Boolean				fClipImage;
	Boolean				fAllowsRectSelection;
	Boolean				fAllowsKeyNavigation;
	Boolean				fHighlightRow;
	
	UInt16				fCellHeight;
	UInt16				fBaseline;
	ResIDT				fTextTraits;	// Font
	RGBColor			fForeColor;
	RGBColor			fBackColor;
	
	Int16				fNumberColumns;
	Int16				fColumnOffsets[ NUM_COLUMNS ];
	Int16				fColumnIDs[ NUM_COLUMNS ];
	LFinderHeader*		fHeader;
		
	DropLocation		fDragTarget;
	SBooleanRect		fDragLeaveDirection;
};

class LDragFinderTask: public LDragTask
{
public:
						LDragFinderTask( const EventRecord& inEventRecord, LFinderView* view );
	virtual				~LDragFinderTask();
	
	virtual void		AddFlavors( DragReference inDragRef );
	virtual void		MakeDragRegion( DragReference inDragRef, RgnHandle inDragRegion );
	
protected:

	static pascal OSErr	ScrollingTracker(	DragTrackingMessage 	message, 
											WindowPtr 				theWindow, 
											void 				*	handlerRefCon, 
											DragReference 			theDragRef		);

	DragTrackingHandlerUPP	fTrackerProc;
	LFinderView*			fView;
};
