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
	
	Created 4/10/96 - Tim Craycroft

*/

#pragma once

#include "CSpecialTableView.h"
#include "CKeyUpReceiver.h"

#include <LCommander.h>
#include <LListener.h>
#include <QAP_Assist.h>

#include "NetscapeDragFlavors.h"
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
	CInlineEditorListener( CStandardFlexTable* inTable ) : mTable(inTable) { }
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
,	public CQAPartnerTableMixin
{
private:
	typedef CSpecialTableView Inherited;
	friend class CTableHeaderListener;
	friend class CClickTimer;
	friend class CInlineEditorListener;
	friend class CDeferredInlineEditTask;
	friend class CTableToolTipPane;
public:
	enum {
		class_ID = 'sfTb',
		msg_SelectionChanged = 'selC'
	};
	struct TextDrawingStuff
	{
		TextDrawingStuff ( )
			: mTextTraitsID(0), style(0), mode(0), encoding(eDefault)
		{
			color.red = color.green = color.blue = 0;
			Invalidate();
		}
		
		// Information is gathered in this struct for use by the table (and its
		// derived classes) in drawing text within cells.  It is also used by
		// CTableTooltipPane, to make the font in the tooltips match that in the
		// table cells.
		// -------
		// Tablewide stuff that does not get invalidated.  Before 98/06/29 (Nova)
		// these two fields were direct members of CStandardFlexTable.
		ResIDT				mTextTraitsID;
		FontInfo			mTextFontInfo;
		// -------
		// Cell-by-cell stuff that gets invalidated
		enum { eDefault, eUTF8 } encoding;
		char style;
		int mode;
		RGBColor color;
		STableCell lastCell; // for caching
		void Invalidate() { lastCell.row = lastCell.col = LArray::index_Bad; }
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
	virtual Boolean		TableSupportsNaturalOrderSort() const;

	void				DoInlineEditing ( const STableCell & inCell ) ;
		// a public way to tell the table to begin an in-place edit. This is
		// useful when handling menu commands, etc and not mouse clicks in the table.
	
protected:
	enum {
		kMinRowHeight = 18
	,	kDistanceFromIconToText = 5
	,	kDropIconWastedSpace = 4
	,	kIconMargin = 1
	,	kIndentPerLevel = 14
	,	kIconWidth = 16
	};

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

	virtual void		RefreshCellRange(
							const STableCell	&inTopLeft,
							const STableCell	&inBotRight); // Overridden to fix a bug in PP.
	virtual void		Click(SMouseDownEvent &inMouseDown);
	virtual void		BeTarget();
	virtual void		DontBeTarget();
// ------------------------------------------------------------
// CEXTable Refugees
// ------------------------------------------------------------
public:
	void				SelectScrollCell(const STableCell &inCell)
						{
							SelectCell(inCell);
							ScrollCellIntoFrame(inCell);						
						}
	
	void				RefreshRowRange(TableIndexT inStartRow, TableIndexT inEndRow) const;
	void				ReadSavedTableStatus(LStream *inStatusData);
	void				WriteSavedTableStatus(LStream *outStatusData);

// ------------------------------------------------------------
// Sorting
// ------------------------------------------------------------
protected:
	virtual void		ChangeSort(const LTableHeader::SortChange* inSortChange);

// ------------------------------------------------------------
// Selection, MouseDowns
// ------------------------------------------------------------
public:
	void				SetFancyDoubleClick(Boolean inFancy) { mFancyDoubleClick = inFancy; }
	Uint16				ClickCountToOpen ( ) const { return mClickCountToOpen; }
	void				ClickCountToOpen ( Uint16 inCount );

protected:
	virtual Boolean		ClickSelect(
								const STableCell		&inCell,
								const SMouseDownEvent	&inMouseDown);

	virtual void		ClickSelf(const SMouseDownEvent &inMouseDown);

	Boolean				HandleFancyDoubleClick(
								const STableCell		&inCell,
								const SMouseDownEvent	&inMouseDown);

	virtual Boolean		ClickDropFlag(
								const STableCell	&inCell,
								const SMouseDownEvent &inMouseDown);
	virtual Boolean		CellSelects(const STableCell& inCell) const;
	virtual Boolean		CellWantsClick( const STableCell & /*inCell*/ ) const { return false; }
	void				SetHiliteDisabled(Boolean inDisable) { mHiliteDisabled = inDisable; }
	virtual void 		HandleSelectionTracking( const SMouseDownEvent & inMouseDown ) ;
	virtual void		TrackSelection( const SMouseDownEvent & inMouseDown ) ;
	virtual Boolean 	HitCellHotSpot( const STableCell &inCell, 
											const Rect &inTotalSelectionRect ) ;

	// the the cell rect in local coords even if scrolled out the the view
	virtual void		GetLocalCellRectAnywhere( const STableCell	&inCell, Rect &outCellRect) const;

	virtual void		UnselectCellsNotInSelectionOutline( const Rect & selectionRect ) ;
		// override to turn off selection outline tracking
	virtual Boolean		TableDesiresSelectionTracking( ) const { return true; }
								
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
	virtual void 		SetEditParam(int w, int h, char* str, SPoint32& ImagePoint);



	virtual Boolean		CanDoInlineEditing( ) const { return true; }
	
	virtual void		DrawCell(
							const STableCell		&inCell,
							const Rect				&inLocalRect);
	virtual void		DrawCellContents(
							const STableCell		&inCell,
							const Rect				&inLocalRect);
	virtual void 		EraseCellBackground(
							const STableCell& inCell,
							const Rect& inLocalRect);
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
							Boolean				okIfRowHidden,
							Rect*				ioRect) const;
	virtual Boolean		GetHiliteTextRect(
							const TableIndexT	inRow,
							Boolean				okIfRowHidden,
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
	virtual TextDrawingStuff& GetTextStyle(
							const STableCell& inCell);
	virtual void		ApplyTextStyle(const STableCell& inCell) const;
	virtual UInt16		GetTextWidth(const char* str, int len) const; 
	
// ------------------------------------------------------------
// Row Expansion/Collapsing
// ------------------------------------------------------------
protected:
	virtual void		SetCellExpansion(const STableCell& /* inCell */, Boolean /* inExpand */) {}
	virtual Boolean		CellHasDropFlag(const STableCell& /* inCell */, Boolean& /* outIsExpanded */) const { return false; }											
	virtual Boolean		CellInitiatesDrag(const STableCell& /* inCell */) const { return false; }											
	virtual	void		GetDropFlagRect( const Rect& inLocalCellRect,
										 Rect&		 outFlagRect) const;
	virtual TableIndexT CountExtraRowsControlledByCell(const STableCell& inCell) const;
						// return zero if there are none.

// ------------------------------------------------------------
// Drag Support
// ------------------------------------------------------------
	virtual OSErr			DragSelection(
								const STableCell& inCell,
								const SMouseDownEvent &inMouseDown);
	virtual OSErr			DragRow(
								TableIndexT				inRow, 
								const SMouseDownEvent&	inMouseDown);
	virtual void			AddSelectionToDrag(
								DragReference inDragRef,
								RgnHandle inDragRgn);
	virtual void			AddRowToDrag(
								TableIndexT inRow,
								DragReference inDragRef,
								RgnHandle inDragRgn);
	virtual void			AddRowDataToDrag(
								TableIndexT		/* inRow */,
											DragReference	/* inDragRef */	) {};
	virtual void		HiliteDropArea(DragReference inDragRef);
	virtual void		InsideDropArea(DragReference inDragRef);
	virtual void		EnterDropArea(DragReference inDragRef, Boolean inDragHasLeftSender);
	virtual void		LeaveDropArea(DragReference	inDragRef);
	virtual Boolean		PointInDropArea(Point inGlobalPt); // add slop for autoscroll

	// Autoscroll support --------------------------------------
	virtual void		ScrollImageBy(
								Int32				inLeftDelta,
								Int32				inTopDelta,
								Boolean				inRefresh);
								// unhilite before and after.
	
	// Utility routines for supporting autoscroll routines ------
	void					StartAutoScroll();
	void					EndAutoScroll();
	
	
	// Specials
	virtual void		HiliteDropRow(TableIndexT inRow, Boolean inDrawBarAbove);
	virtual TableIndexT	GetHiliteColumn() const;
	virtual Boolean		RowCanAcceptDrop(
							DragReference	inDragRef,
							TableIndexT		inDropRow);
	virtual Boolean		RowCanAcceptDropBetweenAbove(
							DragReference	inDragRef,
							TableIndexT		inDropRow);
	virtual Boolean		RowIsContainer(
							const TableIndexT & /* inRow */ ) const { return false; }
	virtual Boolean		RowIsContainer(
							const TableIndexT & /* inRow */, Boolean* /*outIsExpanded*/ ) const ;
	void				ComputeItemDropAreas(
							const Rect& inLocalCellRect,
							Rect& outTop,
							Rect& outBottom );
	void				ComputeFolderDropAreas(
							const Rect & inLocalCellRect,
							Rect& outTop,
							Rect& outBottom );

// ------------------------------------------------------------
// Tooltip support
// ------------------------------------------------------------
	virtual void			CalcToolTipText(
								const STableCell& inCell,
								StringPtr outText,
								TextDrawingStuff& outStuff,
								Boolean& outTruncationOnly);	

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

	virtual void		DeleteSelection(const EventRecord& inMacEvent) = 0;
	virtual	void		OpenRow(TableIndexT /* inRow */) {}
	virtual void		GetInfo();

public:
	virtual void		OpenClick( const STableCell& inCell  );
	virtual	void		OpenSelection();

	// stuff that should go away, moved into a utility class
	
	static void DrawIconFamily( 	ResIDT				inIconID,
									SInt16				inIconWidth,
									SInt16				inIconHeight,
									IconTransformType	inTransform,
									const Rect&			inBounds);
	
	static void DrawTextString(	const char*		inText, 
								const TextDrawingStuff & inTextInfo,
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
	virtual void		QapGetListInfo(PQAPLISTINFO pInfo);
	virtual Ptr			QapAddCellToBuf(Ptr pBuf, Ptr pLimit, const STableCell& sTblCell);
	virtual void		GetQapRowText(TableIndexT inRow, char* outText, UInt16 inMaxBufferLength) const;
#endif

//-----------------------------------
// Data
//-----------------------------------
protected:
	CTableHeaderListener mTableHeaderListener;
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
	Boolean				mAllowDropAfterLastRow;	// true to allow drops in the whitespace after the table
	Boolean				mInlineFeedbackOn;		// do we draw the inline feedback or frame entire area?

	Boolean				mAllowAutoExpand;
	UInt32				mTimeToExpandForDrop;
	CInlineEditField*	mNameEditor;			// used for inline editing
	TableIndexT			mRowBeingEdited;
	CInlineEditorListener mInlineListener;		// listens to the editor and tells us things
	TextDrawingStuff	mTextDrawingStuff;
	
}; // class CStandardFlexTable
