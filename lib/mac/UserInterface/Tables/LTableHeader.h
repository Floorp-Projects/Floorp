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
	
	Created 3/18/96 - Tim Craycroft

*/


#pragma once


#include <LView.h>
#include "UStdBevels.h"



//======================================
class LTableHeader
//======================================
:	public LView
,	public LBroadcaster
{
public:
	enum	{ class_ID = 'tblH' 				};
	enum	{ msg_SortedColumnChanged = 'srtC'	};
	enum	{ kColumnStateResourceType = 'Cols'	};
	typedef UInt16 ColumnIndexT;

	// This is the ioParam in the message broadcast
	// when the sort order is changed (msg_SortedColumnChanged).
	struct SortChange
	{
		PaneIDT			sortColumnID;	// pane id of header of sort column
		ColumnIndexT	sortColumn;		// physical sort column
		Boolean			reverseSort;	// was reverse sort specified ?
	};

	LTableHeader(LStream *inStream);
	virtual ~LTableHeader();
	
	
	// PowerPlant hooks
	virtual void	FinishCreateSelf();
	virtual void	DrawSelf();
	virtual void	Click(SMouseDownEvent &inMouseDown);
	
	virtual void	AdjustCursor(	Point inPortPt,
									const EventRecord &inMacEvent);

	virtual void	ResizeFrameBy(	Int16	inWidthDelta,
									Int16	inHeightDelta,
									Boolean	inRefresh);

	// Column info access
	inline ColumnIndexT		CountColumns() const 		{ return mColumnCount;  		};
	inline ColumnIndexT		CountVisibleColumns() const { return mLastVisibleColumn; 	};
	void					SetRightmostVisibleColumn(ColumnIndexT inLastDesiredColumn);
	ColumnIndexT			SumOfVisibleResizableWidths() const;
	ColumnIndexT			ColumnFromID(PaneIDT inPaneID)	const;
	ColumnIndexT			GetSortedColumn(PaneIDT& outHeaderPane) const;
	ColumnIndexT			GetMinWidthOfRange(
								ColumnIndexT	inFromColumn,
								ColumnIndexT	inToColumn	) const;
	ColumnIndexT			GetWidthOfRange(
								ColumnIndexT	inFromColumn,
								ColumnIndexT	inToColumn	) const;
	Boolean					IsSortedBackwards() const { return mSortedBackwards; }
	Boolean					IsColumnVisible(PaneIDT inID) {
								LPane *thePane = GetColumnPane(ColumnFromID(inID));
								return (thePane != nil ) ? thePane->IsVisible() : false;
							}
	virtual void 			SetSortedColumn(ColumnIndexT inColumn, Boolean inReverse, Boolean inRefresh=true);
	void 					SetWithoutSort(ColumnIndexT inColumn, Boolean inReverse ,Boolean inRefresh= true);
	virtual void 			SimulateClick(PaneIDT inID);
	virtual void 			SetSortOrder(Boolean inReverse);
	virtual bool			CycleSortDirection ( ColumnIndexT & ioColumn ) ;
	
	inline PaneIDT			GetColumnPaneID(ColumnIndexT inColumn) const;	// returns PaneID of column's header
	SInt16					GetColumnWidth(ColumnIndexT inColumn) const;	// returns column's width
	SInt16					GetColumnPosition(ColumnIndexT inColumn) const;	// returns column's horiz position
	UInt16					GetHeaderWidth() const;	// returns width occupied by columns


	typedef UInt32	HeaderFlags;
	enum {	kHeaderCanHideColumns	=	0x1 };	

	//-----------------------------------
	typedef UInt32 ColumnFlags;
	struct SColumnData
	// All column data is stored in an array kept in a handle (resource type 'Cols')
	// This is the data we store for each column - an array element within the handle
	//-----------------------------------
	{
		PaneIDT			GetPaneID() const
			{
				return paneID;
			}
		Boolean			CanResize() const
			{
				return (flags & kColumnDoesNotResize) == 0;
			}
		Boolean			CanResizeTo(UInt16 inNewSize) const
			{
				return CanResize()
							&& (inNewSize >= kMinWidth || inNewSize > columnWidth);
			}
		Boolean			CanResizeBy(SInt16 inDelta) const
			{
				return CanResize()
					&& (inDelta > 0 || columnWidth + inDelta >= kMinWidth);
			}
		Boolean			CanSort() const
			{
				return (flags & kColumnDoesNotSort) == 0;
			}
		Boolean			HasSortIcon() const
			{
				return (flags & (kColumnDoesNotSort | kColumnSuppressSortIcon)) == 0;
			}
		Boolean			CanDisplayTitle() const
			{
				return (flags & kColumnDoesNotDisplayTitle) == 0;
			}
		Boolean			CanShow() const
			{
				return (flags & kColumnDoesNotShow) == 0;
			}

		private:
			enum 
			{
				kColumnDoesNotSort			=	1<<0
			,	kColumnDoesNotResize		=	1<<1
			,	kColumnSuppressSortIcon		=	1<<2
			,	kColumnDoesNotMove			=	1<<3
			,	kColumnDoesNotDisplayTitle	=	1<<4
			,	kColumnDoesNotShow			=	1<<5
			};
		public:
			enum  { kMinWidth = 16 };
		// ---- Data:
		public:
			PaneIDT			paneID;
			SInt16			columnWidth;
			SInt16			columnPosition;		// meaningless when saved/read to/from disk
			ColumnFlags		flags;
	};	// ----- struct SColumnData
	
	struct SavedHeaderState
	{
		ColumnIndexT		columnCount;
		ColumnIndexT		lastVisibleColumn;
		ColumnIndexT		sortedColumn;
		UInt16				pad;
		SColumnData			columnData[1];	
	};

	virtual void	WriteColumnState(LStream * inStream);
	virtual void	ReadColumnState(LStream * inStream, Boolean inMoveHeaders = true);
	SColumnData*	GetColumnData(ColumnIndexT inColumn) const
							{ return &(*mColumnData)[inColumn - 1]; }
	
protected:
	// Column geometry
	ColumnIndexT	FindColumn( Point inLocalPoint ) const;
	void			LocateColumn( ColumnIndexT inColumn, Rect & outWhere) const;
	void			InvalColumn( ColumnIndexT inColumn);
	void			ComputeColumnPositions();
	virtual void	PositionOneColumnHeader( ColumnIndexT inColumn, Boolean inRefresh = true);
	void			PositionColumnHeaders(Boolean inRefresh = true);
	void			ConvertWidthsToAbsolute();
	Boolean			RedistributeSpace(	ColumnIndexT	inFromCol,
										ColumnIndexT	inToCol,
										SInt16	inNewSpace,
										SInt16	inOldSpace);
										
	SInt16			GetFixedWidthInRange(	ColumnIndexT	inFromCol,
											ColumnIndexT	inToCol	);
										

	// Column resizing
	Boolean			IsInHorizontalSizeArea(Point inLocalPt, ColumnIndexT& outLeftColumn);
	void			TrackColumnResize(const SMouseDownEvent	&inEvent, ColumnIndexT inLeftColumn);
	virtual void	ComputeResizeDragRect(ColumnIndexT inLeftColumn, Rect	&outDragRect);
	virtual void	ResizeColumn(ColumnIndexT inLeftColumn, SInt16 inLeftColumnDelta);
	
	// Column dragging
	void			TrackColumnDrag(const SMouseDownEvent &	inEvent, ColumnIndexT inColumn);
	virtual void	ComputeColumnDragRect(ColumnIndexT inColumn, Rect& outDragRect);
	virtual void	MoveColumn(ColumnIndexT inColumn, ColumnIndexT inMoveTo);

	
	// Column drawing
	virtual void	DrawColumnBackground(const Rect& inWhere, Boolean inSortColumn);
	virtual void	RedrawColumns(ColumnIndexT inFrom, ColumnIndexT inTo);								

	// Column hiding
	virtual void	ShowHideRightmostColumn(Boolean inShow);
	
	
	// Private Column info
	//
	// These are all excellent candidates for inlines, but we'll
	// let the compiler decide...
	//
	Boolean			CanColumnResize(ColumnIndexT inColumn) 			const;
	Boolean			CanColumnSort(ColumnIndexT inColumn) 			const;
	Boolean			CanColumnShow(ColumnIndexT inColumn) 			const;
	Boolean			ColumnHasSortIcon(ColumnIndexT inColumn) 		const;
	Boolean			CanHideColumns() 								const;

#ifdef Debug_Signal
	void 			CheckVisible(ColumnIndexT inIndex) const
		{
			Assert_(inIndex > 0 && inIndex <= mLastVisibleColumn);
		}
	void 			CheckLegal(ColumnIndexT inIndex) const
		{
			Assert_(inIndex > 0 && inIndex <= mColumnCount);
		}
	void 			CheckLegalHigh(ColumnIndexT inIndex) const
		{
			Assert_(inIndex >= 0 && inIndex <= mColumnCount);
		}
#else
	void 			CheckVisible(ColumnIndexT /*inIndex*/) const { }
	void 			CheckLegal(ColumnIndexT /*inIndex*/) const { }
	void 			CheckLegalHigh(ColumnIndexT /*inIndex*/) const { }
#endif //Debug_Signal

	LPane*	GetColumnPane(ColumnIndexT inColumn);

protected:
	enum	
	{ 
	 	kColumnMargin 				= 2,
	 	kColumnHidingWidgetWidth 	= 16
	};

	HeaderFlags			mHeaderFlags;
	SColumnData **		mColumnData;
	SBevelColorDesc		mUnsortedBevel;
	SBevelColorDesc		mSortedBevel;

	ColumnIndexT		mColumnCount;		// 1-based count of all columns
	ColumnIndexT		mLastShowableColumn;	// 1-based count of cols that can show
	ColumnIndexT		mLastVisibleColumn;	// 1-based index of rightmost visible column
	ColumnIndexT		mSortedColumn;		// 1-based index of sorted columns
	Boolean				mSortedBackwards;
	UInt16				mReverseModifiers;	// key event modifier bit-mask for reverse-sort
	SInt16				mFixedWidth;		// total width occupied by visible fixed-size columns
	Int16				mColumnListResID;

};




