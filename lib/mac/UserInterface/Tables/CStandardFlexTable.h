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

/*
	
	Created 4/10/96 - Tim Craycroft

*/

#pragma once

#include "CSpecialTableView.h"
#include "CKeyUpReceiver.h"

#include <LCommander.h>
#include <LListener.h>
//#include <QAP_Assist.h>

#include "LTableViewHeader.h"
	// Need LTableHeader here, need the whole thing in the .cp file.
	// Might as well go the whole hog.

class CStandardFlexTable;
class CClickTimer;
class CInlineEditField;


//========================================================================================
class CTableHeaderListener : public LListener
//========================================================================================
{
	friend class CStandardFlexTable;
	CTableHeaderListener(CStandardFlexTable* inTable) : mTable(inTable) {}
	void ListenToMessage(MessageT inMessage, void *ioParam);
protected:
	CStandardFlexTable* mTable;
}; // class CTableHeaderListener


class CInlineEditorListener : public LListener
{
public:
	CInlineEditorListener ( CStandardFlexTable* inTable ) : mTable(inTable) { }
protected:
	void ListenToMessage(MessageT inMessage, void *ioParam);
	CStandardFlexTable* mTable;
}; // class CInlineEditorListener


//========================================================================================
class CStandardFlexTable
// Base class of all our tables in Netscape.  Differs from LTableView in the essential
// matter of allowing selection of an entire row only.
//========================================================================================
:	public CSpecialTableView
,	public LDragAndDrop
,	public LCommander
,	public LBroadcaster
,	public CKeyUpReceiver
//,	public CQAPartnerTableMixin
{
private:
	typedef CSpecialTableView Inherited;
	friend class CTableHeaderListener;
	friend class CClickTimer;
	friend class CInlineEditorListener;
public:
	enum {
		class_ID = 'sfTb',
		msg_SelectionChanged = 'selC'
	};
	
	PaneIDT				GetSortedColumn() const;
	Boolean				IsSortedBackwards() const;
	LTableViewHeader*	GetTableHeader() const { return mTableHeader; }
	Boolean 			IsColumnVisible(PaneIDT inID);
	TableIndexT			GetSelectedRowCount() const;
	virtual void		SelectionChanged();	
	void				AddAttachmentFirst(
							LAttachment* inAttachment,
							Boolean inOwnsAttachment = true);
						// nb: AddAttachment adds at end.  But this can be called if you want the new attachment
						// to be executed before the existing ones.
	virtual void		SetNotifyOnSelectionChange(Boolean inDoNotify);
	virtual Boolean 	GetNotifyOnSelectionChange();
	void				SetRightmostVisibleColumn(UInt16 inLastDesiredColumn);

		// an externally public way to tell the table to begin an inplace edit. This is
		// useful when handling menu commands, etc and not mouse clicks in the table.
	void				DoInlineEditing ( const STableCell & inCell ) ;
	
	virtual void		RefreshCellRange(
							const STableCell	&inTopLeft,
							const STableCell	&inBotRight); // Overridden to fix a bug in PP.

protected:
	enum { kMinRowHeight = 18, kDistanceFromIconToText = 5, kIconMargin = 4, kIndentPerLevel = 10,
			kIconWidth = 16 };

	// for in-place editing, give the CInlineEditor this paneID.
	enum { paneID_InlineEdit = 'EDIT' } ;
	
// ------------------------------------------------------------
// Construction
// ------------------------------------------------------------
						CStandardFlexTable(LStream *inStream);
	virtual				~CStandardFlexTable();
	virtual void		SetUpTableHelpers();	
	virtual void		SynchronizeColumnsWithHeader();

// ------------------------------------------------------------
// Generic PowerPlant
// ------------------------------------------------------------
	virtual void		FinishCreateSelf();
	virtual void		DrawSelf();		
	virtual Boolean		HandleKeyPress(const EventRecord &inKeyEvent);

	virtual void		Click(SMouseDownEvent &inMouseDown);
	virtual void		BeTarget();
	virtual void		DontBeTarget();

// ------------------------------------------------------------
// Sorting
// ------------------------------------------------------------
	virtual void		ChangeSort(const LTableHeader::SortChange* inSortChange);

// ------------------------------------------------------------
// Selection, MouseDowns
// ------------------------------------------------------------
	virtual Boolean		ClickSelect(
								const STableCell		&inCell,
								const SMouseDownEvent	&inMouseDown);
	virtual void		ClickSelf(const SMouseDownEvent &inMouseDown);
	virtual Boolean		ClickDropFlag(
								const STableCell	&inCell,
								const SMouseDownEvent &inMouseDown);
	virtual Boolean		CellSelects(const STableCell& inCell) const;
	virtual Boolean		CellWantsClick ( const STableCell & /*inCell*/ ) const { return false; }
	void				SetFancyDoubleClick(Boolean inFancy) { mFancyDoubleClick = inFancy; }
	void				SetHiliteDisabled(Boolean inDisable) { mHiliteDisabled = inDisable; }
	virtual void 		HandleSelectionTracking ( const SMouseDownEvent & inMouseDown ) ;
	virtual void		TrackSelection ( const SMouseDownEvent & inMouseDown ) ;
	virtual Boolean 	HitCellHotSpot ( const STableCell &inCell, 
											const Rect &inTotalSelectionRect ) ;
	virtual void		UnselectCellsNotInSelectionOutline ( const Rect & selectionRect ) ;
		// override to turn off selection outline tracking
	virtual Boolean		TableDesiresSelectionTracking ( ) { return true; }
								
	virtual const TableIndexT*	GetUpdatedSelectionList(TableIndexT& outSelectionSize);
							// Returns a ONE-BASED list of TableIndexT, as it should.
							// The result is const because this is not a copy, it is
							// the actual list cached in this object.  You probably want
							// to use CMailFlexTable::GetSelection() for messages stuff.
public:
	Boolean				GetNextSelectedRow(TableIndexT& inOutAfterRow) const;
	Boolean				GetLastSelectedRow(TableIndexT& inOutBeforeRow) const;
protected:
	void				SelectRow(TableIndexT inRow);
	void				ScrollRowIntoFrame(TableIndexT inRow);
	void				ScrollSelectionIntoFrame();

// ------------------------------------------------------------
// Drawing
// ------------------------------------------------------------
public:
	void				HiliteRow(
							TableIndexT			inRow,
							Boolean				inHilite);
protected:

		// Why don't we just overload DoHilite() to take two different params? Because
		// of the scoping rules of C++, if you override a name in one scope, say by a
		// derived class, it hides all other versions of that name. As a result, if anyone
		// overloaded DrawHilite() in a derived class, it would give a warning that we're
		// hiding the other versions of DrawHilite(). This is bad.
	virtual void 		DoHiliteRgn( RgnHandle inHiliteRgn ) const;
	virtual void		DoHiliteRect( const Rect & inHiliteRect ) const;
	
		// override to do the right thing with what user types in in-place editor
	virtual void		InlineEditorTextChanged( ) { }
	virtual void		InlineEditorDone( ) { }
	virtual void		DoInlineEditing( const STableCell &inCell, Rect & inTextRect );
	virtual bool		CanDoInlineEditing( ) { return true; }
	
	virtual void		DrawCell(
							const STableCell		&inCell,
							const Rect				&inLocalRect);
	virtual void		DrawCellContents(
							const STableCell		&inCell,
							const Rect				&inLocalRect);
	virtual void		GetMainRowText(
							TableIndexT			inRow,
							char*				outText,
							UInt16				inMaxBufferLength) const;
	virtual UInt16		GetNestedLevel(TableIndexT /*inRow*/) const
							{ return 0; /* default, no indentation */ }
							
	virtual Boolean		GetHiliteText(
							TableIndexT			inRow,
							char*				outText,
							UInt16				inMaxBufferLength,
							Rect*				ioRect) const;
	virtual Boolean		GetHiliteTextRect(
							const TableIndexT	inRow,
							Rect&				outRect) const;
	virtual void		GetHiliteRgn(
							RgnHandle			ioHiliteRgn);
	virtual ResIDT		GetIconID(
							TableIndexT			/*inRow*/) const { return 0; }
	virtual SInt16		DrawIcons(
							const STableCell&	inCell,
							const Rect& 		inLocalRect) const; // don't override, use...
	virtual void		DrawIconsSelf(
							const STableCell&	inCell,
							IconTransformType	inTransformType,
							const Rect&			inIconRect) const;
	virtual Boolean		GetIconRect(
							const STableCell&	inCell,
							const Rect&			inLocalRect,
							Rect&				outRect) const;
	virtual Boolean		GetRowHiliteRgn(TableIndexT inRow, RgnHandle ioHiliteRgn) const;
	virtual Boolean		GetRowDragRgn(TableIndexT inRow, RgnHandle ioHiliteRgn) const;
	virtual void		ApplyTextStyle(TableIndexT inRow) const;
	
// ------------------------------------------------------------
// Row Expansion/Collapsing
// ------------------------------------------------------------
protected:
	virtual void		SetCellExpansion(const STableCell& /* inCell */, Boolean /* inExpand */) {}
	virtual Boolean		CellHasDropFlag(const STableCell& /* inCell */, Boolean& /* outIsExpanded */) const { return true; }											
	virtual Boolean		CellInitiatesDrag(const STableCell& /* inCell */) const { return false; }											
	virtual	void		GetDropFlagRect( const Rect& inLocalCellRect,
										 Rect&		 outFlagRect) const;
	virtual TableIndexT CountExtraRowsControlledByCell(const STableCell& inCell) const;
						// return zero if there are none.

// ------------------------------------------------------------
// Drag Support
// ------------------------------------------------------------
	virtual void		DragSelection(const STableCell& inCell, const SMouseDownEvent &inMouseDown);
	virtual void		AddSelectionToDrag(DragReference inDragRef, RgnHandle inDragRgn);
	virtual void		AddRowDataToDrag(	TableIndexT		/* inRow */,
											DragReference	/* inDragRef */	) {};
	virtual void		InsideDropArea(DragReference inDragRef);
	virtual void		EnterDropArea(DragReference inDragRef, Boolean inDragHasLeftSender);
	virtual void		LeaveDropArea(DragReference	inDragRef);
	virtual Boolean		PointInDropArea(Point inGlobalPt); // add slop for autoscroll
	virtual void		ScrollImageBy(
								Int32				inLeftDelta,
								Int32				inTopDelta,
								Boolean				inRefresh);
								// unhilite before and after.
	// Specials
	virtual void		HiliteDropRow(TableIndexT inRow, Boolean inDrawBarAbove);
	virtual TableIndexT	GetHiliteColumn() const;
	virtual Boolean		RowCanAcceptDrop(
							DragReference	inDragRef,
							TableIndexT		inDropRow);
	virtual Boolean		RowCanAcceptDropBetweenAbove(
							DragReference	inDragRef,
							TableIndexT		inDropRow);
	virtual Boolean		RowIsContainer ( const TableIndexT & /* inRow */ ) const { return false; }
	void ComputeItemDropAreas ( const Rect & inLocalCellRect, Rect & oTop, Rect & oBottom ) ;
	void ComputeFolderDropAreas ( const Rect & inLocalCellRect, Rect & oTop, Rect & oBottom ) ;

// ------------------------------------------------------------
// Miscellany
// ------------------------------------------------------------
	PaneIDT				GetCellDataType(const STableCell &inCell) const;
	void				ListenToHeaderMessage(MessageT inMessage, void *ioParam)
							{ mTableHeaderListener.ListenToMessage(inMessage, ioParam); }
						
	void				SetTextTraits(ResIDT inmTextTraitsID);	

//-----------------------------------
// Commands
//-----------------------------------
protected:
	virtual void		FindCommandStatus(
							CommandT	inCommand,
							Boolean		&outEnabled,
							Boolean		&outUsesMark,
							Char16		&outMark,
							Str255		outName);
	virtual Boolean		ObeyCommand(
							CommandT	inCommand,
							void		*ioParam);

	virtual void		DeleteSelection() = 0;
	virtual	void		OpenRow(TableIndexT /* inRow */) {}
	virtual void		GetInfo();

public:
	virtual	void		OpenSelection();

	// stuff that should go away, moved into a utility class
	
	static void DrawIconFamily( 	ResIDT				inIconID,
									SInt16				inIconWidth,
									SInt16				inIconHeight,
									IconTransformType	inTransform,
									const Rect&			inBounds);
	
	static void DrawTextString(	const char*		inText, 
								const FontInfo*	inFontInfo,
								SInt16			inMargin,
								const Rect&		inBounds,
								SInt16			inJustification = teFlushLeft,
								Boolean			doTruncate = true,
								TruncCode		truncWhere = truncMiddle);

// ------------------------------------------------------------
// QA Partner support
// ------------------------------------------------------------
#if defined(QAP_BUILD)		
public:
	virtual void		QapGetListInfo (PQAPLISTINFO pInfo);
	virtual Ptr			QapAddCellToBuf(Ptr pBuf, Ptr pLimit, const STableCell& sTblCell);
	virtual void		GetQapRowText(TableIndexT inRow, char* outText, UInt16 inMaxBufferLength) const;
#endif

//-----------------------------------
// Data
//-----------------------------------
protected:
	CTableHeaderListener mTableHeaderListener;
	ResIDT				mTextTraitsID;
	FontInfo			mTextFontInfo;
	PaneIDT				mTableHeaderPaneID;																			
	LTableViewHeader*	mTableHeader;
	Int16				mClickCountToOpen;	
	TableIndexT*		mSelectionList;
	CClickTimer*		mClickTimer;
	Boolean				mFancyDoubleClick;
	Boolean				mHiliteDisabled;

// support for dragging, including dragging rows to new positions
	TableIndexT			mDropRow;								
	RGBColor			mDropColor;	
	Boolean				mIsInternalDrop;		// a drop of one row on another ?
	Boolean				mIsDropBetweenRows;		// changing order
	
	CInlineEditField*	mNameEditor;			// used for inline editing
	TableIndexT			mRowBeingEdited;
	CInlineEditorListener mInlineListener;		// listens to the editor and tells us things
	
}; // class CStandardFlexTable
