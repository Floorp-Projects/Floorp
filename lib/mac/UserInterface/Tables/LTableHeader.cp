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
	
	Created 3/18/96 - Tim Craycroft (tj, x3672)

	To Do:
	
*/


// PowerPlant
#include <URegions.h>

// Mac Lib
#include "UserInterface_Defines.h"
#include "UGraphicGizmos.h"
#include "LTableHeader.h"
#include "CSimpleDividedView.h"

#include <math.h>

// Flags

typedef UInt16 HeaderFlagsT;
static const HeaderFlagsT cFlagHeaderSortedBackwards = 0x0001;

// Move me somewhere useful !!!
static void	LocalToGlobalRect(Rect& r);

//======================================
class StCurrentPort
//======================================
{
public:
	StCurrentPort(GrafPtr inPort) 	{ mPort = UQDGlobals::GetCurrentPort(); SetPort((GrafPtr)inPort); }
	~StCurrentPort() 				{ SetPort(mPort); }

private:
	GrafPtr	mPort;
};

//-----------------------------------
LTableHeader::LTableHeader( LStream *inStream )
	: LView(inStream), LBroadcaster()
//-----------------------------------
{
	ResIDT theBevelTraitsID;
	
	mColumnData = NULL;
	mColumnCount = mLastShowableColumn = mLastVisibleColumn = 0;
	
	*inStream >> mHeaderFlags;
		
	*inStream >> theBevelTraitsID;
	UGraphicGizmos::LoadBevelTraits(theBevelTraitsID, mSortedBevel);
	*inStream >> theBevelTraitsID;
	UGraphicGizmos::LoadBevelTraits(theBevelTraitsID, mUnsortedBevel);
	
	*inStream >> mReverseModifiers;
	
	*inStream >> mColumnListResID;
	
	ResizeImageTo(mFrameSize.width, mFrameSize.height, false);
}
	
//-----------------------------------
LTableHeader::~LTableHeader()
//-----------------------------------
{
	if (mColumnData != NULL) ::DisposeHandle( (Handle) mColumnData);
	mColumnData = NULL;
}

//-----------------------------------
void LTableHeader::FinishCreateSelf()
//-----------------------------------
{
	// Load up the column data	
	Handle columnData = ::GetResource('Cols', mColumnListResID);
	ThrowIfNULL_(columnData);
	::DetachResource((Handle)columnData);

	// LHandleStream will dispose of columnData
	LHandleStream streamMe(columnData);	
	ReadColumnState(&streamMe, false);

	// position the column header panes
	LView::FinishCreateSelf();
	PositionColumnHeaders(true);		
}

//-----------------------------------
void LTableHeader::DrawSelf()
//	Draw each column, draw any space to the right of the rightmost
//	column, and draw the column-hiding widget, if necessary.
//-----------------------------------
{
	UInt16	headerWidth = GetHeaderWidth();
	if (headerWidth == 0)
		return;
	
	StColorPenState::Normalize();
				
	// Draw bottom frame line
	
	Rect r;
	CalcLocalFrameRect(r);
	::MoveTo(r.left, r.bottom - 1);
	::LineTo(r.right - 1, r.bottom - 1);
				
	// Draw the column backgrounds and frames, carrying r.right
	// out of the loop so we know where the rightmost column ends
	
	RgnHandle updateRgn = GetLocalUpdateRgn();
	for (int i = 1; i <= mLastVisibleColumn; i++)
	{
		Rect		sect;
	
		LocateColumn(i, r);
		
		// Hack to tidy up 1-pixel offset of last header
		if (i == mLastVisibleColumn)
			r.right ++;
		
		SColumnData* cData = GetColumnData(i);
		
		// Checking against the update rgn is necessary because we sometimes
		// call Draw(), with a valid rgn to draw only particular columns
		// in order to reduce flicker.

		// If we didn't do this check we would end up erasing other columns,
		// as Draw would not redraw their header subpanes

		if ( updateRgn == NULL || ::SectRect(&r, &((**updateRgn).rgnBBox), &sect))
		{
			Boolean isSortedColumn = cData->GetPaneID() == GetColumnPaneID(mSortedColumn);
			DrawColumnBackground(r, isSortedColumn);
			if (isSortedColumn && cData->HasSortIcon())
			{
				// Draw the little icon
				const Int16 kSortIconWidth = 14; // which allows 1 pixel each side.
				ResIDT iconID = mSortedBackwards ? 14504 : 14505;
				Rect iconFrame = r;
				iconFrame.left = iconFrame.right - kSortIconWidth;
				::PlotIconID(&iconFrame, kAlignAbsoluteCenter, kTransformNone, iconID);
			}
		}
	}
	if (updateRgn)
		::DisposeRgn(updateRgn);

	// If a fixed-width column is rightmost, 
	// we may have some space to fill in between
	// it and the right edge of the view	
	if (headerWidth > r.right )
	{
		r.left = r.right;
		r.right = headerWidth;
		
		DrawColumnBackground(r, false);
	}
	CalcLocalFrameRect(r);
	r.left = r.right - kColumnHidingWidgetWidth + 1;
	DrawColumnBackground(r, false);

	// Draw the column hiding widget
	if (CanHideColumns())
	{
		SInt16	iconID;
		r.top -= 1; // Hate this, but the position had to be adjusted. - jrm.
		r.bottom -= 1; // Hate this, but the position had to be adjusted. - jrm.
		r.left -= 1;		
		if (mLastVisibleColumn <= 1) 
			if (mLastShowableColumn == 1)
				iconID = kColumnHiderDisabledIcon; // neither hide nor show possible
			else
				iconID = kColumnHiderHideDisabledIcon; // show is possible
		else
			if (mLastVisibleColumn == mLastShowableColumn)
				iconID = kColumnHiderShowDisabledIcon;
			else	
				iconID = kColumnHiderEnabledIcon;	
		::PlotIconID(&r, atNone, ttNone, iconID);
	}	
} // LTableHeader::DrawSelf

//-----------------------------------
void LTableHeader::DrawColumnBackground(
	const Rect 	& inWhere,
	Boolean	  	inSortColumn)
//	Bevel and fill the column header rectangle. 
//-----------------------------------
{
	Rect r = inWhere;
	r.right -= 1;	// For frame borders
	r.bottom -= 1;
	const SBevelColorDesc& colors = (inSortColumn) ? mSortedBevel : mUnsortedBevel;
	 
	UGraphicGizmos::BevelRect(r, 1, colors.topBevelColor, colors.bottomBevelColor);

	::PmForeColor(colors.fillColor);
	::InsetRect(&r,1,1);
	::PaintRect(&r);
	::InsetRect(&r,-1,-1);
	
	::ForeColor(blackColor);
	::MoveTo(r.right, r.top);
	::LineTo(r.right, r.bottom - 1);
}									

//-----------------------------------
void LTableHeader::AdjustCursor(Point inPortPt,
							const EventRecord &inMacEvent)
//	Switch the cursor if it's over a column resize area. 
//	
//	We only give subviews a chance to change the cursor
//	if we're not over a resize area.
//-----------------------------------
{
#pragma unused(inMacEvent)
	
	Point		localPt = inPortPt;
	ColumnIndexT		column;
	
	PortToLocalPoint(localPt);
	if (IsInHorizontalSizeArea(localPt, column))
		::SetCursor(*GetCursor(kHorizontalResizeCursor));
	else
		LView::AdjustCursor(inPortPt, inMacEvent);
} // LTableHeader::AdjustCursor

//-----------------------------------
Boolean LTableHeader::IsInHorizontalSizeArea(
	Point 			inLocalPt, 
	ColumnIndexT& 	outLeftColumn)
//	Returns true if the given local pt is in a column-resize area. 
//	Also returns the index of the column to the left of that area
//	(the column to be resized).
//-----------------------------------
{
	
	SInt16 left = inLocalPt.h + 2;
	SInt16 right = inLocalPt.h - 2;
	
	// start at the division between the first and second columns
	// Go up to the division between the last visible and the following one,
	// if there is one.
	SInt16 lastColumnToCheck = mLastVisibleColumn;
	if (mLastShowableColumn > mLastVisibleColumn)
		lastColumnToCheck++;
	SColumnData* cRightNeigbor = *mColumnData + 1;
	for (outLeftColumn = 1;
		outLeftColumn < lastColumnToCheck;
		outLeftColumn++,cRightNeigbor++)
	{
		SInt16 columnRightEdge = cRightNeigbor->columnPosition;
		if (left 	>= columnRightEdge &&
			right 	<= columnRightEdge)
		{
			// OK, it's a column boundary.
			return (cRightNeigbor - 1)->CanResize();
		}
	}
	outLeftColumn = 0xFFFF;	
	return false;
} // LTableHeader::IsInHorizontalSizeArea

//-----------------------------------
void LTableHeader::Click(SMouseDownEvent &inEvent)
//	Handles all clicks in the view.  We never pass the click
//	down to subviews. 
//	
//	Handles changing sort column, resizing columns, moving 
//	columns, and showing/hiding columns.
//-----------------------------------
{
	// LPane::Click ususally does this...
	PortToLocalPoint(inEvent.whereLocal);
	
	// Handle column resize
	ColumnIndexT column;
	if ( IsInHorizontalSizeArea(inEvent.whereLocal, column))
	{
		TrackColumnResize(inEvent, column);
	}
	else
	{
		// Handle clicks in the column hiding widget
		if (CanHideColumns() && (inEvent.whereLocal.h > GetHeaderWidth())) 
		{
			if (mLastShowableColumn == 1 && mLastVisibleColumn == 1)
				return; // no hiding/showing possible.
			// The right arrow hides the rightmost column and the left arrow
			// shows it.
			enum { leftSide, rightSide } side
				= inEvent.whereLocal.h >= (mFrameSize.width - (kColumnHidingWidgetWidth/2))
				? rightSide
				: leftSide;
			if (mLastVisibleColumn > 1 && side == rightSide)
				ShowHideRightmostColumn(false); // hide
			else if (mLastVisibleColumn < mLastShowableColumn && side == leftSide)
				ShowHideRightmostColumn(true); // show.
		}
		else
		{
			// Click in column header, either drag it,  
			// or set it to be the sort column
			
			column = FindColumn(inEvent.whereLocal);
			if (column != 0)
			{
				if (mLastVisibleColumn > 1 && ::WaitMouseMoved(inEvent.macEvent.where))
					TrackColumnDrag(inEvent, column);
				else
				{
					Boolean sortReverse = false;
					if (mReverseModifiers != 0)
						sortReverse = (inEvent.macEvent.modifiers & mReverseModifiers) != 0;
					else
					{
						// Toggle mode if click again on sorted column.
						sortReverse = CycleSortDirection ( column );
					}
					SetSortedColumn(column, sortReverse);
				}
			}
		}
	}
} // LTableHeader::Click


//
// CycleSortDirection
//
// If the user clicks in the header of the already sorted column, they want to reverse
// the direction of the sort. This can be extended to become a tri-state switch that "unsorts"
// the column (revert to natural order) after doing forward/reverse sort, but this routine 
// doesn't do that.
// 
bool
LTableHeader :: CycleSortDirection ( ColumnIndexT & ioColumn )
{
	return (ioColumn == mSortedColumn) ? !mSortedBackwards : mSortedBackwards;

} // ToggleSortedColumn


//-----------------------------------
void LTableHeader::TrackColumnDrag(
	const			SMouseDownEvent& inEvent,
	ColumnIndexT		inColumn)
//	Tracks the dragging of column inColumn, moving it,
//	and redrawing as necessary.
//-----------------------------------
{
	Point		dropPoint;	
	StRegion	dragRgn;
	Rect		dragRect;
	Rect		dragBounds;
	Rect		dragSlop;
	
	if (!FocusDraw()) return;
	
	// Create the rgn we're dragging
	//
	// This is virtualize so subclasses can make a 
	// more interesting rectangle...
	//
	ComputeColumnDragRect(inColumn, dragRect);
	::RectRgn(dragRgn, &dragRect);
	
	// Compute the bounds and slop of the drag
	CalcLocalFrameRect(dragBounds);
	LocalToGlobalRect(dragBounds);	
	dragSlop = dragBounds;
	::InsetRect(&dragSlop, -5, -5);
	
	// drag me
	{
		StCurrentPort	portState(LMGetWMgrPort());
		const Rect 		wideopen = { 0xFFFF, 0xFFFF, 0x7FFF, 0x7FFF };
		
		ClipRect(&wideopen);
		*(long*)(&dropPoint) = ::DragGrayRgn(
									dragRgn, 
									inEvent.macEvent.where, 
									&dragBounds,
									&dragSlop, 
									kVerticalConstraint,
									NULL);
	
	}
	
	if ((dropPoint.h != (SInt16) 0x8000 || dropPoint.v != (SInt16) 0x8000)	&&
		(dropPoint.h != 0 || dropPoint.v != 0))
	{
		ColumnIndexT	dropColumn;		
	
		// find the column over which we finished the drag
		dropPoint.h += inEvent.whereLocal.h;
		dropPoint.v += inEvent.whereLocal.v;
		
		dropColumn = FindColumn(dropPoint);
		
		// move the column we were dragging
		if (dropColumn != 0 && inColumn != dropColumn) {
			MoveColumn(inColumn, dropColumn);	
		}
	}
} // LTableHeader::TrackColumnDrag

//-----------------------------------
void LTableHeader::TrackColumnResize(
	const SMouseDownEvent	&inEvent,
	ColumnIndexT			inLeftColumn	)
//	Tracks resizing of a column, redrawing as necessary.
//-----------------------------------
{
	Rect			dragBoundsRect;
	Rect			slopRect;
	StRegion		dragRgn;
	Rect			dragRect;
		
	CheckVisible(inLeftColumn);

	if (!FocusDraw()) return;
	
	
	// Compute the actual region we're dragging
	//
	// This is virtualized so subclasses can reflect the size-change
	// in some other view's space (like down a table column)
	
	SColumnData* cData = GetColumnData(inLeftColumn);
	ComputeResizeDragRect(inLeftColumn, dragRect);
	::RectRgn(dragRgn, &dragRect);	
	
	// Compute drag bounds and slop
	CalcLocalFrameRect(dragBoundsRect);
	dragBoundsRect.left = (inLeftColumn != 1) ? cData->columnPosition + 4 : 4;
	
	LocalToGlobalRect(dragBoundsRect);
	slopRect = dragBoundsRect;
	InsetRect(&slopRect, -5, -5);
	
	// drag me
	Point dragDiff;	
	{
		StCurrentPort portState(LMGetWMgrPort());
		const Rect wideopen = { 0xFFFF, 0xFFFF, 0x7FFF, 0x7FFF };
		
		::ClipRect(&wideopen);

		// Track the drag, then resize the column if it was a valid drag
		*(long *)&dragDiff = ::DragGrayRgn(
								dragRgn, 
								inEvent.macEvent.where, 
								&dragBoundsRect,
								&slopRect, 
								kVerticalConstraint,
								NULL);
	}
	if ((dragDiff.h != (SInt16) 0x8000 || dragDiff.v != (SInt16) 0x8000)	&&
		(dragDiff.h != 0     || dragDiff.v != 0 ))
	{
		if (cData->CanResizeBy(dragDiff.h))
			ResizeColumn(inLeftColumn, dragDiff.h);
	}
} // LTableHeader::TrackColumnResize

//-----------------------------------
void LTableHeader::ResizeColumn(
	ColumnIndexT	inLeftColumn,
	SInt16			inLeftColumnDelta	)
//	Resize inLeftColumn by inLeftColumnDelta.	
//	Columns to the right of inLeftColumn are resized
//	proportionally.
//-----------------------------------
{
	if (inLeftColumnDelta == 0)
		return;

	SColumnData* thisColData = GetColumnData(inLeftColumn);
	
	thisColData->columnWidth += inLeftColumnDelta;
	SColumnData* nextColData = thisColData + 1;
	int i = inLeftColumn + 1;
	Boolean oneColumnCanResize = false;
	for (; i <= mLastVisibleColumn; i++, nextColData++)
		if (nextColData->CanResize())
		{
			oneColumnCanResize = true;
			break;
		}

	SInt16 newSpace = GetHeaderWidth() - (thisColData->columnPosition + thisColData->columnWidth);
	SInt16 oldSpace = newSpace + inLeftColumnDelta;

	// Use proportional distribution if there's a resizable column on the right, AND either
	// we're shrinking the column or there's enough slack in the columns to the right.
	Boolean useProportionalRedistribution = false;
	if (oneColumnCanResize)
	{
		if (inLeftColumnDelta < 0 && inLeftColumn != mLastVisibleColumn)
			useProportionalRedistribution = true;
		else if (inLeftColumnDelta > 0
				&& newSpace >= GetMinWidthOfRange(inLeftColumn + 1, mLastVisibleColumn))
			useProportionalRedistribution = true;
	}

	if (useProportionalRedistribution)
	{
		RedistributeSpace(inLeftColumn + 1, mLastVisibleColumn, newSpace, oldSpace);
		ComputeColumnPositions();
		PositionColumnHeaders(false);
		RedrawColumns(inLeftColumn, mLastVisibleColumn);
		return;
	}
	else if (inLeftColumnDelta > 0)
	{
		// This is the case when we resized the column to the rightmost edge, or so
		// far that there is not enough space for all the columns between
		// the resized column and the edge.  To redistribute the space would reduce the sizes
		// of all the resizable columns in this range to zero.  This would be bad (and was
		// bug number 62743).
		
		// Move columns offscreen one at a time until there's enough room for the remainder.
		while (mLastVisibleColumn > inLeftColumn
				&& newSpace < GetWidthOfRange(inLeftColumn + 1, mLastVisibleColumn))
		{
			mLastVisibleColumn--;
		}
	}
	else if (inLeftColumnDelta < 0)
	{
		// Move as many columns onscreen as will fit
		while (mLastVisibleColumn + 1 <= mLastShowableColumn
				&& GetWidthOfRange(inLeftColumn + 1, mLastVisibleColumn + 1) <= newSpace)
		{
			mLastVisibleColumn++;
		}
	}
	// Finally,  expand this column to the right just enough to fit all the
	// columns that we showed or left showing, and then  compute all the positions.
	UInt16 widthsToRight = GetWidthOfRange(inLeftColumn + 1, mLastVisibleColumn);
	thisColData->columnWidth = GetHeaderWidth() - widthsToRight - thisColData->columnPosition;
	ComputeColumnPositions();
	PositionColumnHeaders(false);
	RedrawColumns(inLeftColumn, mLastVisibleColumn);
} // LTableHeader::ResizeColumn

//-----------------------------------
UInt16 LTableHeader::GetMinWidthOfRange(
	ColumnIndexT	inFromColumn,
	ColumnIndexT	inToColumn	) const
//-----------------------------------
{
	Assert_(inFromColumn > 0);
	Assert_(inToColumn > 0);
	if (inToColumn > mLastVisibleColumn)
		inToColumn = mLastVisibleColumn;
	if (inFromColumn > inToColumn)
		return 0;

	SColumnData*	cData = &(*mColumnData)[inFromColumn - 1];
	if (inToColumn > mLastVisibleColumn)
		inToColumn = mLastVisibleColumn;
	
	SInt16 minWidth = 0;
	for (int i = inFromColumn; i <= inToColumn; i++, cData++)
	{
		if (cData->CanResize())
			minWidth += SColumnData::kMinWidth;
		else
			minWidth += cData->columnWidth;
	}
	return minWidth;
} // LTableHeader::GetMinWidthOfRange

//-----------------------------------
UInt16 LTableHeader::GetWidthOfRange(
	ColumnIndexT	inFromColumn,
	ColumnIndexT	inToColumn	) const
//-----------------------------------
{
	Assert_(inFromColumn > 0);
	Assert_(inToColumn > 0);
	if (inToColumn > mLastVisibleColumn)
		inToColumn = mLastVisibleColumn;
	if (inFromColumn > inToColumn)
		return 0;

	SColumnData*	cData = &(*mColumnData)[inFromColumn - 1];
	if (inToColumn > mLastVisibleColumn)
		inToColumn = mLastVisibleColumn;
	
	SInt16 width = 0;
	for (int i = inFromColumn; i <= inToColumn; i++, cData++)
		width += cData->columnWidth;
	return width;
} // LTableHeader::GetMinWidthOfRange

//-----------------------------------
void LTableHeader::SetRightmostVisibleColumn(ColumnIndexT inLastDesiredColumn)
//-----------------------------------
{
	CheckLegal(inLastDesiredColumn);
	ColumnIndexT currentLastVisible = mLastVisibleColumn;
	while (mLastVisibleColumn < inLastDesiredColumn)
	{
		ShowHideRightmostColumn(true);
		if (mLastVisibleColumn == currentLastVisible)
			return; // prevent infinite loop
		currentLastVisible = mLastVisibleColumn;
	}
	while (mLastVisibleColumn > inLastDesiredColumn)
	{
		ShowHideRightmostColumn(false);
		if (mLastVisibleColumn == currentLastVisible)
			return; // prevent infinite loop
		currentLastVisible = mLastVisibleColumn;
	}
} // LTableHeader::SetRightmostVisibleColumn

//-----------------------------------
void LTableHeader::ShowHideRightmostColumn(Boolean inShow)
//	Show or hide the rightmost column.	
//	mLastVisibleColumn is the rightmost visible column.
//-----------------------------------
{
	Assert_((inShow && mLastVisibleColumn < mLastShowableColumn) ||
			(!inShow && mLastVisibleColumn > 1));
					
	ColumnIndexT savedLastVisibleColumn = mLastVisibleColumn;
	Boolean ok;
	
	UInt16 headerWidth = GetHeaderWidth();
#ifdef DEBUG
	SColumnData*	cData = GetColumnData(1);
	SInt16 width = 0;
	for (int i = 1; i <= mLastVisibleColumn; i++, cData++)
		width += cData->columnWidth;
	Assert_(width == headerWidth);
#endif // DEBUG
	UInt16 newWidth, oldWidth;
	UInt16 colWidth;
	if (inShow)
	{
		mLastVisibleColumn++;
		SColumnData* column = GetColumnData(mLastVisibleColumn);
		if (column->columnWidth < SColumnData::kMinWidth)
			column->columnWidth = SColumnData::kMinWidth;
		colWidth = column->columnWidth;
		if (colWidth < headerWidth / 3)
		{
			// Move new column in at full size, shrinking existing columns to make room
			oldWidth = headerWidth;
			newWidth = headerWidth - colWidth;
			ok = RedistributeSpace(1, mLastVisibleColumn - 1, newWidth, oldWidth);
		}
		else
		{
			// Move them all in proportionally.
			oldWidth = headerWidth + colWidth;
			newWidth = headerWidth;
			ok = RedistributeSpace(1, mLastVisibleColumn, newWidth, oldWidth);
		}
	}
	else
	{
		newWidth = headerWidth;
		colWidth = GetColumnWidth(mLastVisibleColumn);	// col being hidden
		mLastVisibleColumn--;
		oldWidth = headerWidth - colWidth;
		ok = RedistributeSpace(1, mLastVisibleColumn, newWidth, oldWidth);
	}
	if (!ok)
	{
		// It didn't work.  Restore the original visible column count
		// So undo!  There are probably no resizable columns to allow the change.
		mLastVisibleColumn = savedLastVisibleColumn;
		return;
	}
	ComputeColumnPositions();
	PositionColumnHeaders(false);
	RedrawColumns(1, mLastVisibleColumn);
} // LTableHeader::ShowHideRightmostColumn

//-----------------------------------
UInt16 LTableHeader::SumOfVisibleResizableWidths() const
//----------------------------------- 
{
	UInt16 result = 0;
	SColumnData*	cData = *mColumnData;
	// Using weighted proportions.  Set d = the total of the proportion
	// values for the VISIBLE columns
	for (int i = 1; i <= mLastVisibleColumn; i++, cData++)
	{
		if (cData->CanResize())
			result += cData->columnWidth;
	}
	return result;
} // LTableHeader::SumOfVisibleResizableWidths

//-----------------------------------
Boolean LTableHeader::RedistributeSpace(
	ColumnIndexT	inFromColumn,
	ColumnIndexT	inToColumn,
	SInt16	inNewSpace,
	SInt16	inOldSpace)
//	Given old and new amounts of horizontal space, this resizes a range of columns
//  such that they each have the same percentage of space in the new space as they
//	did in the old space. 	
//	
//	Fixed-width columns are not resized, of course.
//	
//	Assumes all resizable column widths are in weighted proportion.
//	Converts all these widths to absolute pixel values in such a way that the
//	"visible" columns take up the full image size.  The proportions are maintained, so
//	there's no need to convert to percentages: there's nothing magic about the number
//	100.
//	
//	If the last visible column does not reach the right edge exactly, the last visible
//	resizable column will be extended or shrunk to make it do so. (this is largely
//	due to round-off errors in our arithmetic).	
//-----------------------------------
{
	CheckVisible(inFromColumn);
	CheckVisible(inToColumn);
	Assert_(inFromColumn <= inToColumn);
	Assert_(inOldSpace > 0);
	if (inOldSpace <= 0 || inNewSpace <= 0)
		return false;

	SInt16 fixedWidth = GetFixedWidthInRange(inFromColumn, inToColumn);

	// Loop through columns, resizing them in proportion.  This keeps the proportional
	// sizes equal.  While looping, note the last resizable column in the range, which will
	// be used for adjustments.  Also tally the new resizable total width in the range.
	SColumnData* cData = GetColumnData(inFromColumn);
	UInt16 sumOfWidths = 0;								// Tally to be used after loop completes
	float newSpaceFloat = inNewSpace - fixedWidth;		// Take invariant conversion out of the loop
	float oldSpaceFloat = inOldSpace - fixedWidth;		// ...
	for (int i = inFromColumn; i <= inToColumn; i++, cData++)
	{
		if (cData->CanResize())
		{
			// Resize this column to take the same proportional amount of space, rounding
			// as appropriate
			SInt16 newWidth = (newSpaceFloat * cData->columnWidth + 0.5) / oldSpaceFloat;
			if (newWidth >= SColumnData::kMinWidth || newWidth > cData->columnWidth)
				cData->columnWidth = newWidth;
		}
		// For columns in range (only), add to tally.
		sumOfWidths += cData->columnWidth;
	}
	// Adjust the resizable columns to remove any rounding error.  We used to add this to the
	// last column, but that would make it get bigger, and bigger, and bigger,...
	// So now, go around to each resizable column in rotation and make it eat some of the
	// error, until it all fits exactly.
	Int16 roundingError = inNewSpace - sumOfWidths;
	if (roundingError == 0)
		return true; // perfick!
	if (sumOfWidths == inOldSpace)
		return false; // Oh, diddums.  No resizable columns
	// OK, if we get this far, then some columns were able to resize
	// (sumOfWidths != inOldSpace), and there's merely a rounding error to adjust.
	int signedUnit = roundingError < 0 ? -1 : +1;
//#ifdef DEBUG
//	int maxRound =  (inToColumn - inFromColumn + 2) / 2;
//	Assert_(roundingError * signedUnit <= maxRound);
//#endif
	static int rovingIndex = 0;
	const Int16 initialError = roundingError;
	const int initialIndex = rovingIndex;
	Int16		panicCount = 0;
	
	while (roundingError)
	{
		rovingIndex++;
		if (rovingIndex > inToColumn)
			rovingIndex = inFromColumn;
		if (initialIndex == rovingIndex && initialError == roundingError)
		{
			Assert_(false); // not making progress!  No resizable columns?
			return false;
		}
		cData = GetColumnData(rovingIndex);
		if (cData->CanResizeBy(signedUnit))
		{
			cData->columnWidth += signedUnit;
			roundingError -= signedUnit;
		}
		
		if (++panicCount > 100)
			break;
	}
	return true;
} // LTableHeader::RedistributeSpace
									
//-----------------------------------
SInt16 LTableHeader::GetFixedWidthInRange(
	ColumnIndexT	inFromColumn,
	ColumnIndexT	inToColumn	)
//	Returns the horizontal space taken by all fixed-width
//	columns in a given column range (inclusive).
//-----------------------------------
{
	CheckVisible(inFromColumn);
	CheckVisible(inToColumn);
	Assert_(inFromColumn <= inToColumn);

	SColumnData*	cData = GetColumnData(inFromColumn);
	SInt16 fixedWidth = 0;
	if (inToColumn > mLastVisibleColumn)
		inToColumn = mLastVisibleColumn;
	
	for (int i = inFromColumn; i <= inToColumn; i++, cData++)
	{
		if (!cData->CanResize())
			fixedWidth += cData->columnWidth;
	}
	return fixedWidth;
} // LTableHeader::GetFixedWidthInRange

//-----------------------------------
UInt16 LTableHeader::GetHeaderWidth() const
//	Returns the width of all the visible columns,
//	not including the column hiding widget.
//-----------------------------------
{
	Int16 result = mImageSize.width - kColumnHidingWidgetWidth;
	return result < 0 ? 0 : (UInt16)result;
}
						
//-----------------------------------
void LTableHeader::MoveColumn(ColumnIndexT inColumn, ColumnIndexT inMoveTo)
//	If inColumn > inMoveTo, moves inColumn BEFORE inMoveTo.
//	If inColumn < inMoveTo, moves inColumn AFTER inMoveTo.
//-----------------------------------
{
	SColumnData	*	cData 		= *mColumnData,
				*	shiftFrom, 
				*	shiftTo;
	SColumnData		swap;
	Boolean			adjustedSort = false;
	
	// save off the data for the column we're moving
	::BlockMoveData(&(cData[inColumn-1]), &swap, sizeof(SColumnData));
	
	// Handle case where we're moving the sorted column		
	if (mSortedColumn == inColumn)
	{
		adjustedSort 	= true;
		mSortedColumn 	= inMoveTo;
	}
	
	//
	// Figure out which direction we're moving the column
	// and how we need to shift the data in the columndata array
	//
	SInt32 delta = inColumn - inMoveTo;
	if (delta > 0)
	{
		shiftFrom	= &(cData[inMoveTo-1]);
		shiftTo		= shiftFrom + 1;
		
		if (!adjustedSort && mSortedColumn >= inMoveTo && mSortedColumn < inColumn)
			mSortedColumn += 1;
	}
	else
	{
		delta 		= -delta;
		shiftTo 	= &(cData[inColumn-1]);
		shiftFrom	= shiftTo + 1;
		
		if (!adjustedSort && mSortedColumn > inColumn && mSortedColumn <=inMoveTo)
			mSortedColumn -= 1;
	}
	
	// Shift the data in the columnData array and then copy in the data
	// of the column we're moving
	::BlockMoveData(shiftFrom, shiftTo, (delta*sizeof(SColumnData)));
	::BlockMoveData(&swap, &(cData[inMoveTo-1]), sizeof(SColumnData));
	
	// resposition everything
	ComputeColumnPositions();
	PositionColumnHeaders(false);
	
	
	if (inColumn > inMoveTo)
		RedrawColumns(inMoveTo, inColumn);
	else
		RedrawColumns(inColumn, inMoveTo);
} // LTableHeader::MoveColumn
	
//-----------------------------------
void LTableHeader::ComputeColumnPositions()
//	Computes the horizontal position of each column, assuming 
//	each column's width is valid in pixels.
//	
//	If the rightmost visible column doesn't fill the column area, 
//	it will be extended to do so, UNLESS it is a fixed-width column.
//-----------------------------------
{
	UInt16 headerWidth = GetHeaderWidth();
	if (headerWidth == 0)
		return;
	// Compute each visible column's position
	SColumnData* cData = *mColumnData;
	SInt16 accumulator = 0;
	for (int i = 1; i <= mLastVisibleColumn; i++, cData++)
	{
		cData->columnPosition = accumulator;
		accumulator += cData->columnWidth;
	}
	
	// If the last column is resizable, expand or shrink it
	// as necessary to reach the edge of the header
	cData--;
	if (cData->CanResize())
	{
		if ( (cData->columnPosition + cData->columnWidth) != headerWidth)
			cData->columnWidth = headerWidth - cData->columnPosition;
	}
} // LTableHeader::ComputeColumnPositions

//-----------------------------------
void LTableHeader::ComputeResizeDragRect(ColumnIndexT		inLeftColumn,
										Rect	&	outDragRect		)
//	Computes rectangle to be dragged when resizing a column.
//-----------------------------------
{
	outDragRect.top 	= 	0;
	outDragRect.bottom	=	mFrameSize.height + 100;
	outDragRect.left	=	GetColumnPosition(inLeftColumn+1) - 1;
	outDragRect.right	=	outDragRect.left + 1;
	
	LocalToGlobalRect(outDragRect);
}										

//-----------------------------------
void LTableHeader::ComputeColumnDragRect(
	ColumnIndexT		inColumn,
	Rect			&outDragRect)
//-----------------------------------
{
	LocateColumn(inColumn, outDragRect);
	outDragRect.bottom += 100;	
	LocalToGlobalRect(outDragRect);
}										

//-----------------------------------
void LTableHeader::SetSortedColumn(
	ColumnIndexT 	inColumn, 
	Boolean			inReverse,
	Boolean			inRefresh	)
//-----------------------------------
{
	if (!CanColumnSort(inColumn))
		return;
	if (inColumn == mSortedColumn && inReverse == mSortedBackwards)
		return;
	SInt16 oldSorted = mSortedColumn;
	mSortedColumn = inColumn;
	mSortedBackwards = inReverse;
	if (inRefresh && FocusDraw())
	{	
		
		
		if (oldSorted != 0) {
			RedrawColumns(oldSorted, oldSorted);
		}
		
		if (mSortedColumn != 0) {
			RedrawColumns(mSortedColumn, mSortedColumn);
		}
	}
	// Tell any listeners that the sort column changed
	LTableHeader::SortChange	changeRecord;
	
	changeRecord.sortColumnID 	= GetColumnPaneID(mSortedColumn);
	changeRecord.sortColumn		= mSortedColumn;
	changeRecord.reverseSort	= mSortedBackwards;	
	BroadcastMessage(msg_SortedColumnChanged, &changeRecord);
}

//-----------------------------------
void LTableHeader::SetWithoutSort(
	ColumnIndexT 	inColumn, 
	Boolean			inReverse,
	Boolean			inRefresh	)
//-----------------------------------
{
	if (!CanColumnSort(inColumn))
		return;
	if (inColumn == mSortedColumn && inReverse == mSortedBackwards)
		return;
	SInt16 oldSorted = mSortedColumn;
	mSortedColumn = inColumn;
	mSortedBackwards = inReverse;
	if( inRefresh )
		Refresh();	
}

//-----------------------------------
void LTableHeader::SimulateClick(PaneIDT inID)
//-----------------------------------
{
	PaneIDT result; 
	
	ColumnIndexT index = GetSortedColumn(result);
	
	if ( (result == inID) || !IsEnabled() || !IsColumnVisible(inID)) return;
	
	SetSortedColumn(ColumnFromID(inID), IsSortedBackwards(), true);
}

//-----------------------------------
void LTableHeader::SetSortOrder(Boolean inReverse)
//-----------------------------------
{
	PaneIDT result; 
	
	ColumnIndexT index = GetSortedColumn(result);
	
	if ( !result || !IsEnabled() || !IsColumnVisible(result)) return;
	
	SetSortedColumn(index, inReverse, true);
}

//-----------------------------------
void LTableHeader::RedrawColumns(ColumnIndexT inFrom, ColumnIndexT inTo)
//	Immediately redraws the given column range.
//	We don't do update events so we can avoid the flicker
//	of update rgn being erased to white.
//-----------------------------------
{
	UInt16 headerWidth = GetHeaderWidth();
	
	// Compute the top-left of the refresh area
	Rect r;
	LocateColumn(inFrom, r);
	LocalToPortPoint(topLeft(r));
	
	// Get the bottom right of the refresh area
	if (inFrom == inTo)
	{
		LocalToPortPoint(botRight(r));
	}
	else
	{
		Rect right;
		
		if (inTo <= mLastVisibleColumn)
		{
			LocateColumn(inTo, right);
			LocalToPortPoint(botRight(right));	
		}
		else
		{
			right.right = headerWidth;
			LocalToPortPoint(botRight(right));
		}
		
		r.bottom = right.bottom;
		r.right  = right.right;
	}

	// Redraw
	StRegion rgn;
	::RectRgn(rgn, &r);	
	Draw(rgn);
}

//-----------------------------------
void LTableHeader::LocateColumn( ColumnIndexT inColumn,
	 						Rect & outWhere) const
//	Computes the bounding rect of the given column 
//	in local coordinates.
//-----------------------------------
{
	outWhere.top 	= 0;
	outWhere.bottom = mFrameSize.height;
	
	outWhere.left 	= GetColumnPosition(inColumn);
	outWhere.right	= outWhere.left + GetColumnWidth(inColumn);
}

//-----------------------------------
 LTableHeader::ColumnIndexT LTableHeader::FindColumn( Point inLocalPoint ) const
//	Given a local point, returns the index of the
//	column containing the point.  Returns 0 for no
//	column.
//-----------------------------------
{
	int	i;

	if (inLocalPoint.h >= 0 && inLocalPoint.h <= GetHeaderWidth())
	{
		for (i = mLastVisibleColumn; i > 0; i--)
		{
			if (inLocalPoint.h >= GetColumnPosition(i))
				return i;
		}
	}
	return 0;
}

//-----------------------------------
void LTableHeader::InvalColumn( ColumnIndexT inColumn)
//	Invalidates a column header area.
//-----------------------------------
{
	if (FocusDraw())
	{
		Rect	r;
		
		LocateColumn(inColumn, r);
		LocalToPortPoint(topLeft(r));
		LocalToPortPoint(botRight(r));
		InvalPortRect(&r);
	}
}

//-----------------------------------
void LTableHeader::PositionColumnHeaders(Boolean inRefresh)
//	Positions all the column header panes to be aligned
//	with the current column positions.
//-----------------------------------
{
	if (GetHeaderWidth() == 0)
		return;
	int i;
	for (i = 1; i <= mLastVisibleColumn; i++)
	{
		PositionOneColumnHeader(i, inRefresh);
		LPane* headerPane = GetColumnPane(i);
		if (headerPane)
		{
			if (GetColumnData(i)->CanDisplayTitle())
				headerPane->Show();
			else
				headerPane->Hide();
			if (!inRefresh)
				headerPane->DontRefresh(true);
		}
	}
	for ( ; i <= mColumnCount; i++)
	{
		LPane* headerPane = GetColumnPane(i);
		if (headerPane)
		{
			headerPane->Hide();
			if (!inRefresh)
				headerPane->DontRefresh(true);
		}
	}
} // LTableHeader::PositionColumnHeaders

//-----------------------------------
void LTableHeader::PositionOneColumnHeader( ColumnIndexT inColumn, Boolean inRefresh)
//	Positions a column header pane to be aligned with the its 
//	column position.
//	
//	The column header pane is sized to the width of the column. 
//	Its vertical position is not changed.
//-----------------------------------
{
	Rect columnBounds;
	LocateColumn(inColumn, columnBounds);
	LPane* headerPane = GetColumnPane(inColumn);
	
	// Position the pane horizontally within the column
	if (headerPane)
	{
		SDimension16	paneSize;				
		headerPane->GetFrameSize(paneSize);

		SPoint32 paneLocation;
		headerPane->GetFrameLocation(paneLocation);

		Int32 horizDelta
			= (columnBounds.left + kColumnMargin) - paneLocation.h + mFrameLocation.h;
		Int32 paneWidth	= columnBounds.right - (columnBounds.left + (kColumnMargin * 2));
				
		headerPane->ResizeFrameTo(paneWidth, paneSize.height, inRefresh);	
		headerPane->MoveBy( horizDelta, 0, inRefresh);										
	}	
} // LTableHeader::PositionOneColumnHeader

//-----------------------------------
PaneIDT LTableHeader::GetColumnPaneID(ColumnIndexT inColumn) const
//	Returns the Pane id of the given column's header-pane.	
//	Returns 0 if there is no header-pane.
//-----------------------------------
{
	CheckLegalHigh(inColumn);
	if (inColumn != 0)
		return (GetColumnData(inColumn)->GetPaneID());
	return 0;
}

//-----------------------------------
LPane *	LTableHeader::GetColumnPane(ColumnIndexT inColumn)
//-----------------------------------
{
	LView	*super = GetSuperView();
	
	CheckLegal(inColumn);
	Assert_(super != NULL);
	
	PaneIDT id = GetColumnPaneID(inColumn);
	
	if (id != 0)
		return super->FindPaneByID(id);
	return NULL;
}

//-----------------------------------
SInt16 LTableHeader::GetColumnWidth(ColumnIndexT inColumn) const
//-----------------------------------
{
	CheckLegal(inColumn);
	// widths are always in absolute when this is called.
	return GetColumnData(inColumn)->columnWidth;
}

//-----------------------------------
SInt16 LTableHeader::GetColumnPosition(ColumnIndexT inColumn) const
//-----------------------------------
{
	CheckLegal(inColumn);
	return ( GetColumnData(inColumn)->columnPosition);
}

//-----------------------------------
Boolean	LTableHeader::CanColumnResize(ColumnIndexT inColumn) const
//-----------------------------------
{
	CheckLegal(inColumn);

	// check all of the columns to the right to see if they can be resized
	// if they can't be resized we should NOT be able to resize this column
	// example:  | resizCol1 | resizCol2 | fixedCol3 |
	// if we are working off:            ^
	// we shouldn't be able to resize (shrink) here because there is nowhere
	// for any extra space to go and we don't erase where column was.
	// note that inColumn is 1-based but mColumnData (within loop) is 0-based
	
	SColumnData* cData = GetColumnData(CountVisibleColumns());
	Boolean canFurtherRightResize = false;
	for (int i = CountVisibleColumns(); i > inColumn; i--,cData--)
	{
		if (cData->CanResize())
		{
			canFurtherRightResize = true;
			break;
		}
	}
	return canFurtherRightResize && GetColumnData(inColumn)->CanResize();
} // LTableHeader::CanColumnResize

//-----------------------------------
Boolean	LTableHeader::CanColumnSort(ColumnIndexT inColumn) const
//-----------------------------------
{
	CheckLegal(inColumn);
	return GetColumnData(inColumn)->CanSort();
}

//-----------------------------------
Boolean	LTableHeader::CanColumnShow(ColumnIndexT inColumn) const
//-----------------------------------
{
	CheckLegal(inColumn);
	return GetColumnData(inColumn)->CanShow();
}

//-----------------------------------
LTableHeader::ColumnIndexT LTableHeader::GetSortedColumn(PaneIDT &outHeaderPane) const
//-----------------------------------
{ 
	outHeaderPane = GetColumnPaneID(mSortedColumn);
	return mSortedColumn; 
}

//-----------------------------------
LTableHeader::ColumnIndexT LTableHeader::ColumnFromID(PaneIDT inPaneID) const
//-----------------------------------
{ 
	SColumnData* c = *mColumnData;
	for (int i = 1; i <= mColumnCount; i++,c++)
		if (c->paneID == inPaneID)
			return i;
	return 0;
}

//-----------------------------------
Boolean LTableHeader::CanHideColumns() const
//-----------------------------------
{
	 return mHeaderFlags & kHeaderCanHideColumns == kHeaderCanHideColumns;
}

//-----------------------------------
void LTableHeader::WriteColumnState( LStream * inStream)	
//	Writes out the current column state with column
//	widths stored as percentages (except for fixed-width)
//	columns.
//-----------------------------------
{
	Assert_(mColumnData != NULL);
	
	HeaderFlagsT flags = 0;
	
	if ( mSortedBackwards ) flags |= cFlagHeaderSortedBackwards;
	
	*inStream << mColumnCount;
	*inStream << mLastVisibleColumn;
	*inStream << mSortedColumn;
	*inStream << (HeaderFlagsT) flags;			// pad
	
	SInt32	dataSize = mColumnCount * sizeof(SColumnData);
	
	StHandleLocker locker((Handle)mColumnData);
	
	// write out the widths
	inStream->PutBytes(*mColumnData, dataSize);

} // LTableHeader::WriteColumnState

//-----------------------------------
void LTableHeader::ReadColumnState( LStream * inStream, Boolean inMoveHeaders)		
//	Reads in column state for the header, converts the column widths
//	to absolute pixel values, computes column positions, and optionally
//	positions the column header panes.	
//-----------------------------------
{
	*inStream >> mColumnCount;
	*inStream >> mLastVisibleColumn;
	*inStream >> mSortedColumn;
	HeaderFlagsT flags;
	*inStream >> flags;
	mSortedBackwards = (flags & cFlagHeaderSortedBackwards) != 0;

//	Assert_(mColumnCount > 0);
	// Columns for RDF views are taken from the RDF database as opposed to
	// stored in a resource. Therefore allow ReadColumnState to handle case
	// where there are no columns specified in 'Cols' resource
	if (mColumnCount > 0)
	{
		CheckLegal(mLastVisibleColumn);
		CheckLegalHigh(mSortedColumn);

		SInt32	dataSize = mColumnCount * sizeof(SColumnData);
	
		if (mColumnData != NULL) ::DisposeHandle((Handle)mColumnData);
		mColumnData = (SColumnData **) ::NewHandle(dataSize);
		ThrowIfNULL_(mColumnData);

		StHandleLocker locker((Handle)mColumnData);
		inStream->GetBytes(*mColumnData, dataSize);

		// find the last showable column
		for (UInt16 i = 1; i <= mColumnCount; i++)
		{
			// non-showing cols must be at the end
			Assert_( CanColumnShow(i) ? true : i > mLastVisibleColumn);

			if (CanColumnShow(i))
				mLastShowableColumn = i;
		}

		ConvertWidthsToAbsolute();
		ComputeColumnPositions();
		
		if (inMoveHeaders)
			PositionColumnHeaders(true);
	}
		
	Refresh();
} // LTableHeader::ReadColumnState

//-----------------------------------
void LTableHeader::ConvertWidthsToAbsolute()
//-----------------------------------
{	
	SColumnData*	cData = *mColumnData;
	UInt16 oldSpace = 0;
	for (int i = 1; i <= mLastVisibleColumn; i++, cData++)
		oldSpace += cData->columnWidth;
	RedistributeSpace(1, mLastVisibleColumn, GetHeaderWidth(), oldSpace);
} //  LTableHeader::ConvertWidthsToAbsolute

//-----------------------------------
void LTableHeader::ResizeFrameBy(
	Int16	inWidthDelta,
	Int16	inHeightDelta,
	Boolean	inRefresh)
//	When the frame is resized, we resize our image to match,
//	first converting to percentages so that we can resize
//	our columns proportionally.
//-----------------------------------
{
	ResizeImageBy(	inWidthDelta, 
					inHeightDelta, 
					inRefresh	);
	if ( mColumnCount != 0 )
	{									
		// Move the columns and column headers
		ConvertWidthsToAbsolute();
		ComputeColumnPositions();
		PositionColumnHeaders();
	}	
	LView::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
}								

//-----------------------------------
static void LocalToGlobalRect(Rect& r)
//-----------------------------------
{
	::LocalToGlobal((Point *) &(r.top));
	::LocalToGlobal((Point *) &(r.bottom));
}
