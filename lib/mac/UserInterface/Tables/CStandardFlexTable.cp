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
	
	Created 3/25/96 - Tim Craycroft
	
*/


#pragma mark --- Includes

#include "CStandardFlexTable.h"

// Mac
#include <Drag.h>

// PowerPlant
#include <LTableMultiSelector.h>
#include <LTableArrayStorage.h>
#include <URegions.h>
#include <LDropFlag.h>

// ANSI
#include <string.h>
#include <stdio.h>

// Netscape Mac Libs
#include "LFlexTableGeometry.h"
#include "UGraphicGizmos.h"
#include "LTableRowSelector.h"
#include "CTableKeyAttachment.h"
#include "CContextMenuAttachment.h"
#include "CTargetFramer.h"
#include "UMailFolderMenus.h"
#include "CToolTipAttachment.h"
#include "CApplicationEventAttachment.h"

#include "macutil.h"
#include "resgui.h"
#include "secnav.h"
#include "cstring.h"

#include "CInlineEditField.h"
#include "CPasteSnooper.h"
#include "UDeferredTask.h"

#pragma mark -

//========================================================================================
class CDeferredInlineEditTask
//========================================================================================
:	public CDeferredTask
{
	public:
								CDeferredInlineEditTask(
									CStandardFlexTable*	inView,
									 const STableCell &inCell,
									 Rect & inTextRect);
	protected:
		virtual ExecuteResult	ExecuteSelf();
// data
	protected:
		CStandardFlexTable*		mTable;
		Rect					mTextRect;
		STableCell				mCell;
		UInt32					mEarliestExecuteTime;
}; // class CDeferredInlineEditTask

//----------------------------------------------------------------------------------------
CDeferredInlineEditTask::CDeferredInlineEditTask(
	CStandardFlexTable*	inView,
	const STableCell &inCell,
	Rect & inTextRect)
//----------------------------------------------------------------------------------------
:	mTable(inView)
,	mCell(inCell)
,	mTextRect(inTextRect)
,	mEarliestExecuteTime(::TickCount() + GetDblTime())
{
}

//----------------------------------------------------------------------------------------
CDeferredTask::ExecuteResult CDeferredInlineEditTask::ExecuteSelf()
//----------------------------------------------------------------------------------------
{
	// Current policy 98/04/07: start inline editing only if mouse stays over area and
	// host view remains on duty past the double-click time.
	mTable->FocusDraw();
	Point mouseLocal;
	::GetMouse(&mouseLocal);
	if (!::PtInRect(mouseLocal, &mTextRect))
		return eDoneDelete; // if mouse left area, cancel inline editing without executing.
	if (!mTable->IsOnDuty())
		return eDoneDelete; // if host view is deactivated, cancel inline editing.
	if (::TickCount() < mEarliestExecuteTime)
		return eWaitStayFront; // still waiting to see
	mTable->DoInlineEditing(mCell, mTextRect);
	return eDoneDelete; // remove from queue
} // CDeferredInlineEditTask::ExecuteSelf

#pragma mark --- CTableHeaderListener

//----------------------------------------------------------------------------------------
void CTableHeaderListener::ListenToMessage(MessageT inMessage, void *ioParam)
//----------------------------------------------------------------------------------------
{
	if (inMessage == LTableHeader::msg_SortedColumnChanged) {
		mTable->ChangeSort((const LTableHeader::SortChange*)ioParam);
	}
} // CTableHeaderListener::ListenToMessage


#pragma mark --- CTableHeaderListener

//----------------------------------------------------------------------------------------
void  CInlineEditorListener::ListenToMessage(MessageT inMessage, void * /*ioParam*/)
// Respond to changes in the inline editor
//----------------------------------------------------------------------------------------
{
	switch ( inMessage )
	{
		case CInlineEditField::msg_InlineEditFieldChanged:
			mTable->InlineEditorTextChanged();
			break;	
		case CInlineEditField::msg_HidingInlineEditField:
			mTable->InlineEditorDone();
			mTable->mRowBeingEdited = LArray::index_Bad;
			mTable->SetHiliteDisabled(false);
			break;
	} // case of which message
	
} // CInlineEditorListener::ListenToMessage

#pragma mark --- CClickTimer

//========================================================================================
class CClickTimer : public LPeriodical
//========================================================================================
{
public:
					CClickTimer(
						CStandardFlexTable* inTable,
						const STableCell& inCell,
						const SMouseDownEvent& inMouseDown);
protected:
	// The destructor is protected because the right way to delete is is to call either
	//	NoteSingleClick() or NoteDoubleClick();
	virtual			~CClickTimer();

public:
	static void		NoteDoubleClick(CStandardFlexTable* inTable);
	static void		NoteSingleClick(CStandardFlexTable* inTable);
	TableIndexT		GetClickedRow() { return mCell.row; }

protected:	
	void			ReverseTheHilite();
	virtual void	SpendTime(const EventRecord& inMacEvent);
	void			NoteSingleClick();

// Data:
protected:
	CStandardFlexTable* mTable;
	STableCell			mCell;
	SMouseDownEvent		mMouseDown; // Can't be a reference, orig. is on the stack & dies
	Boolean				mBusyDeleting;
}; // class CClickTimer

//----------------------------------------------------------------------------------------
CClickTimer::CClickTimer(
	CStandardFlexTable* inTable,
	const STableCell& inCell,
	const SMouseDownEvent& inMouseDown)
//----------------------------------------------------------------------------------------
:	mTable(inTable)
,	mCell(inCell)
,	mMouseDown(inMouseDown)
,	mBusyDeleting(false)
{
	mTable->mClickTimer = this;
	ReverseTheHilite(); // fake a new selection (temporarily).
	StartRepeating();
} //  CClickTimer::CClickTimer()

//----------------------------------------------------------------------------------------
CClickTimer::~CClickTimer()
//----------------------------------------------------------------------------------------
{
	mTable->mClickTimer = nil;
	mTable->SetHiliteDisabled(false); // make sure it's normal
} //  CClickTimer::~CClickTimer()

//----------------------------------------------------------------------------------------
void CClickTimer::SpendTime(const EventRecord& inMacEvent)
//----------------------------------------------------------------------------------------
{
	if (inMacEvent.when >= mMouseDown.macEvent.when + LMGetDoubleTime())
		NoteSingleClick();
} // CClickTimer::SpendTime()

//----------------------------------------------------------------------------------------
void CClickTimer::NoteDoubleClick(CStandardFlexTable* inTable)
//----------------------------------------------------------------------------------------
{
	CClickTimer* t = inTable->mClickTimer;
	if (t)
	{
		t->ReverseTheHilite(); // Show the selection again
		delete t;
	}
} // CClickTimer::NoteDoubleClick()

//----------------------------------------------------------------------------------------
void CClickTimer::NoteSingleClick(CStandardFlexTable* inTable)
//----------------------------------------------------------------------------------------
{
	CClickTimer* t = inTable->mClickTimer;
	if (t)
		t->NoteSingleClick();
}

//----------------------------------------------------------------------------------------
void CClickTimer::NoteSingleClick()
//----------------------------------------------------------------------------------------
{
	// Avoid reentrancy when we call mTable->ClickSelect().
	if (mBusyDeleting)
		return;
	mBusyDeleting = true;
	try
	{
#if 0
		//ReverseTheHilite(); // Show the selection again
		// The hiliting is now correct. Turn off hiliting to avoid a double flash.
		mTable->SetHiliteDisabled(true);
		mTable->SetFancyDoubleClick(false);
		mTable->ClickSelect(mCell, mMouseDown);
		mTable->SetFancyDoubleClick(true);
#else
		mTable->SetHiliteDisabled(true);
		mTable->LTableView::ClickSelect(mCell, mMouseDown);
#endif
	}
	catch (...)
	{
		delete this;
		throw;
	}
	delete this;
} // CClickTimer::NoteSingleClick()

const Boolean ignore = true;

//----------------------------------------------------------------------------------------
void CClickTimer::ReverseTheHilite()
// This relies on the implementation of HiliteRow, which ignores the "inActively"
// parameter.
//----------------------------------------------------------------------------------------
{
	STableCell cell = mTable->GetFirstSelectedCell();
	UInt32 total = mTable->GetSelectedRowCount();
	UInt32 count = 0;
	if (total > 0)
		for (; cell.row <= mTable->mRows && count < total; cell.row++)
		{
			if (mTable->CellIsSelected(cell))
			{
				mTable->HiliteRow(cell.row, ignore);
				count++;
			}
		}
	mTable->mDropRow = cell.row; // fake out the icon to draw selected
	mTable->HiliteRow(mCell.row, ignore);
	mTable->mDropRow = LArray::index_Bad;
} // CClickTimer::ReverseTheHilite

#pragma mark --- CStandardFlexTable

//----------------------------------------------------------------------------------------
CStandardFlexTable::CStandardFlexTable(LStream *inStream)
//----------------------------------------------------------------------------------------
:	CSpecialTableView(inStream)
,	LDragAndDrop(GetMacPort(), this)
,	LCommander()
,	LBroadcaster()
,	CQAPartnerTableMixin(this)
,	mTableHeaderListener(this)
,	mTableHeader(NULL)
,	mSelectionList(NULL)
,	mClickTimer(nil)
,	mFancyDoubleClick(false)
,	mHiliteDisabled(false)
,	mDropRow(LArray::index_Bad)
,	mInlineFeedbackOn(true)
,	mIsInternalDrop(false)
,	mIsDropBetweenRows(false)
,	mAllowDropAfterLastRow(true)
,	mAllowAutoExpand(false)
,	mTimeToExpandForDrop(ULONG_MAX)
,	mNameEditor(NULL)
,	mRowBeingEdited(0)
,	mInlineListener(this)
{
	*inStream >> mTableHeaderPaneID;
	*inStream >> mTextDrawingStuff.mTextTraitsID;
	*inStream >> mClickCountToOpen;
	
	SetRefreshAllWhenResized(true);
} // CStandardFlexTable::CStandardFlexTable

//----------------------------------------------------------------------------------------
CStandardFlexTable::~CStandardFlexTable()
//----------------------------------------------------------------------------------------
{
	CClickTimer::NoteSingleClick(this);
} // CStandardFlexTable::~CStandardFlexTable

//----------------------------------------------------------------------------------------
void CStandardFlexTable::FinishCreateSelf()
//----------------------------------------------------------------------------------------
{
	RGBColor ignore;
	UGraphicGizmos::CalcWindowTingeColors(GetMacPort(), mDropColor, ignore);

	LView *super = GetSuperView();		
	Assert_(super != NULL);
	mTableHeader = (LTableViewHeader*)dynamic_cast<LTableViewHeader*>(super->FindPaneByID(mTableHeaderPaneID));
	Assert_(mTableHeader);
	// This class assumes the header is fully created first (Oh, yukky-poo).
	Boolean HEADER_MUST_BE_IN_RESOURCE_BEFORE_TABLE = mTableHeader->GetTableView() != NULL;
	Assert_(HEADER_MUST_BE_IN_RESOURCE_BEFORE_TABLE);
	
	mTableHeader->AddListener(&mTableHeaderListener);	
	SetUpTableHelpers();
	
	// Add columns to match the header
	
	SynchronizeColumnsWithHeader();

	SetTextTraits(mTextDrawingStuff.mTextTraitsID);

	SetRefreshAllWhenResized(true);
	
	// If an editor for in-place editing exists, find it. Don't worry if one can't be found.
	mNameEditor = dynamic_cast<CInlineEditField*>(FindPaneByID(paneID_InlineEdit));
	if (mNameEditor)
	{
		mNameEditor->AddListener(&mInlineListener);
		mNameEditor->SetGrowableBorder(true);
		mNameEditor->AddAttachment(new CPasteSnooper(MAKE_RETURNS_SPACES));
	}
	
} // CStandardFlexTable::FinishCreateSelf

//----------------------------------------------------------------------------------------
CStandardFlexTable::TextDrawingStuff&
CStandardFlexTable::GetTextStyle(const STableCell& inCell)
// This is called by ApplyTextStyle(), which is called byDrawCell() before calling
// DrawCellContents().  It is also called by CalcToolTipText(), so that the tooltip
// looks the same as the cell contents.
//----------------------------------------------------------------------------------------
{
	if (inCell.row != mTextDrawingStuff.lastCell.row)
	{
		// mTextDrawingStuff.encoding =  TextDrawingStuff::eDefault;
		mTextDrawingStuff.style = normal;
		mTextDrawingStuff.mode = srcOr;
		// Initialize text color to black
		UInt16* ip = &mTextDrawingStuff.color.red;
		for (int i = 0; i < 3; i++)
			*ip++ = 0x0000;
		mTextDrawingStuff.lastCell = inCell;
	}
	return mTextDrawingStuff;
} // CThreadView::GetTextStyle

//----------------------------------------------------------------------------------------
void CStandardFlexTable::SetTextTraits(ResIDT inTextTraitsID)
//----------------------------------------------------------------------------------------
{
	// load the text traits and pre-measure them
	StTextState	textState;
	
	SInt16		rowHeight;
	
	mTextDrawingStuff.mTextTraitsID = inTextTraitsID;
	UTextTraits::SetPortTextTraits(mTextDrawingStuff.mTextTraitsID);
	::GetFontInfo(&mTextDrawingStuff.mTextFontInfo);
	
	rowHeight = mTextDrawingStuff.mTextFontInfo.ascent
				+ mTextDrawingStuff.mTextFontInfo.descent + 1;
	if (rowHeight < kMinRowHeight) { 
		rowHeight = kMinRowHeight;
	}
		
	mTableGeometry->SetRowHeight(rowHeight, 0, 0);
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::ScrollRowIntoFrame(TableIndexT inRow)
//----------------------------------------------------------------------------------------
{
	// This avoids excessive erasure done in the base class.
	if (inRow == LArray::index_Bad)
		return;
	STableCell cellToShow(inRow, 1);
	SPoint32 startLocation;
	GetImageLocation(startLocation);
	ScrollCellIntoFrame(cellToShow);
	SPoint32 endLocation;
	GetImageLocation(endLocation);
	if (startLocation.h != endLocation.h || startLocation.v != endLocation.v)
	{
		// 97/08/06.  jrm: not entirely sure about whether this will give us
		//				a redraw bug, but ScrollCellIntoFrame seems to end up
		//				calling UpdatePort().  So I've added DontRefresh() here,
		//				and it seems to cut down on flashing.
		DontRefresh();
	}
} // CStandardFlexTable::ScrollRowIntoFrame

//----------------------------------------------------------------------------------------
void CStandardFlexTable::SelectRow(TableIndexT inRow)
//----------------------------------------------------------------------------------------
{
	STableCell cellToSelect(inRow, 1);
	if (GetSelectedRowCount() == 0)
		ScrollRowIntoFrame(inRow);
	SelectCell(cellToSelect);
} // CStandardFlexTable::SelectRow

//----------------------------------------------------------------------------------------
void CStandardFlexTable::ScrollSelectionIntoFrame()
//----------------------------------------------------------------------------------------
{
	if (mTableSelector)
	{
		STableCell cellToSelect = mTableSelector->GetFirstSelectedCell();
		if ( cellToSelect.row != LArray::index_Bad )
			ScrollRowIntoFrame(cellToSelect.row);
		else
			ScrollRowIntoFrame(1);				// if no selection, scroll to topw
	}
} // CStandardFlexTable::SelectRow

//----------------------------------------------------------------------------------------
void CStandardFlexTable::SynchronizeColumnsWithHeader()
//----------------------------------------------------------------------------------------
{
	Assert_(mTableHeader);
	Int32 deltaColumns = (Int32) mTableHeader->CountVisibleColumns() - (Int32) mCols;
	if ( deltaColumns != 0 ) {
		if ( deltaColumns > 0 ) {
			InsertCols(deltaColumns, mCols, NULL, 0, false);
		} else {
			RemoveCols(-deltaColumns, mCols + deltaColumns + 1, false);
		}
	}
} // CStandardFlexTable::SynchronizeColumnsFromHeader

//----------------------------------------------------------------------------------------
void CStandardFlexTable::AddAttachmentFirst(LAttachment* inAttachment, Boolean inOwnsAttachment)
// nb: AddAttachment adds at end.  But this can be called if you want the new attachment
// to be executed before the existing ones.
//----------------------------------------------------------------------------------------
{
	LAttachment* beforeItem = nil;
	if (mAttachments)
		mAttachments->FetchItemAt(LArray::index_First, beforeItem);
	LCommander::AddAttachment(inAttachment, beforeItem, inOwnsAttachment);
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::SetUpTableHelpers()
//----------------------------------------------------------------------------------------
{
	SetTableGeometry( new LFlexTableGeometry(this, mTableHeader) );
	SetTableSelector( new LTableRowSelector(this));
					// NOTE: There is a dependency between LTableRowSelector and
					// CThreadView because CThreadView::DoSelectThread() needs to
					// select several rows at once.

	// standard keyboard navigation.
	AddAttachment( new CTableRowKeyAttachment(this) );
	// We don't need table storage. It's safe to have a null one.
	// It is NOT safe, to call SetTableStorage(NULL), as its 
	// implementation in LTableView does not check against NULL.	
	//	SetTableStorage(NULL);
	Assert_(mTableStorage == NULL);
} // CStandardFlexTable::SetUpTableHelpers

//----------------------------------------------------------------------------------------
void CStandardFlexTable::RefreshCellRange(
	const STableCell	&inTopLeft,
	const STableCell	&inBotRight)
// Fix a bug in LTableView.  If neither cell intersects the frame, there are two
// cases: union of two cells intersects in empty rect or entire rect.  PP always
// assumed the latter.
//----------------------------------------------------------------------------------------
{
	// I added this test to prevent updating the entire frame in the case
	// of an empty intersection (a very common case!).
	// Calculate the convex hull of the two cells
	Int32	cell1Left, cell1Top, dummy1, dummy2;
	mTableGeometry->GetImageCellBounds(inTopLeft, cell1Left, cell1Top,
							dummy1, dummy2);
	Int32	cell2Right, cell2Bottom;
	mTableGeometry->GetImageCellBounds(inBotRight, dummy1, dummy2,
							cell2Right, cell2Bottom);
	// Does this big rect intersect the frame?  If not, nothing to do.
	if (!ImageRectIntersectsFrame(cell1Left, cell1Top, cell2Right, cell2Bottom))
		return;
	LTableView::RefreshCellRange(inTopLeft, inBotRight);
} // CStandardFlexTable::RefreshCellRange

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DrawSelf()
//----------------------------------------------------------------------------------------
{
	mTextDrawingStuff.Invalidate();
	StTextState	textState;			
	UTextTraits::SetPortTextTraits(mTextDrawingStuff.mTextTraitsID);
	ApplyForeAndBackColors();
	LTableView::DrawSelf();
	ExecuteAttachments(CTargetFramer::msg_DrawSelfDone, this);
} // CStandardFlexTable::DrawSelf

//----------------------------------------------------------------------------------------
TableIndexT CStandardFlexTable::CountExtraRowsControlledByCell(const STableCell&) const
//----------------------------------------------------------------------------------------
{
	return 0;
} // CStandardFlexTable::CountExtraRowsControlledByCell

//----------------------------------------------------------------------------------------
void CStandardFlexTable::ApplyTextStyle(const STableCell& inCell) const
// This is called by DrawCell() before calling DrawCellContents()
//----------------------------------------------------------------------------------------
{
	TextDrawingStuff& stuff
		= const_cast<CStandardFlexTable*>(this)->GetTextStyle(inCell);
	::TextFace(stuff.style);
	::TextMode(stuff.mode);
	::RGBForeColor(&stuff.color);
} // CMessageFolderView::ApplyTextStyle

//----------------------------------------------------------------------------------------
void CStandardFlexTable::GetMainRowText(
	TableIndexT			/*inRow*/,
	char*				outText,
	UInt16				inMaxBufferLength) const
// Calculate the text and (if ioRect is not passed in as null) a rectangle that fits it.
// Return result indicates if any of the text is visible in the cell
//----------------------------------------------------------------------------------------
{	
	if (outText && inMaxBufferLength > 0)
		*outText = '\0';
} // CStandardFlexTable::GetMainRowText

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::GetHiliteText(
	TableIndexT			inRow,
	char*				outText,
	UInt16				inMaxTextLength,
	Boolean				okIfRowHidden,
	Rect*				ioRect) const
// Calculate the text and (if ioRect is not passed in as null) a rectangle that fits it.
// Caller must initialize the rectangle
// Return result indicates if any of the text is visible in the cell
//----------------------------------------------------------------------------------------
{	
	GetMainRowText(inRow, outText, inMaxTextLength);
	if (!*outText)
		return false;
	if (!ioRect)
		return true;
	ioRect->left += kDistanceFromIconToText;
	ioRect->top += 2;
	ioRect->bottom -= 2;
	TableIndexT		hiliteCol = GetHiliteColumn();
	StColorState penState;
	UTextTraits::SetPortTextTraits(mTextDrawingStuff.mTextTraitsID);
	ApplyTextStyle(STableCell(inRow, hiliteCol));

	ioRect->right = ioRect->left + GetTextWidth((const char*)outText, ::strlen(outText));
	
	Rect cellRect;
	
	if (okIfRowHidden)
	{
		GetLocalCellRectAnywhere(STableCell(inRow, hiliteCol), cellRect);
	}
	else
	{
		if ( !GetLocalCellRect(STableCell(inRow, hiliteCol), cellRect) ||
				(ioRect->left >= cellRect.right) )
			return false;
	}
	
	if (ioRect->right > cellRect.right)
		ioRect->right = cellRect.right;
	
	return true;
} // CStandardFlexTable::GetHiliteText
//----------------------------------------------------------------------------------------
UInt16 CStandardFlexTable::GetTextWidth(
	const char*				text,
	int len
) const
//----------------------------------------------------------------------------------------
{
	if ( mTextDrawingStuff.encoding == CStandardFlexTable::TextDrawingStuff::eDefault)
	{
		return ::TextWidth(text, 0, len);
	} else {
		return UGraphicGizmos::GetUTF8TextWidth(text,len);		
	}
} //  CStandardFlexTable::GetTextWidth


//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::GetHiliteTextRect(
	const TableIndexT	inRow,
	Boolean				okIfRowHidden,
	Rect&				outRect) const
//----------------------------------------------------------------------------------------
{
	// Default is to return a rect enclosing the entire row.  Derived classes may wish
	// to use GetHiliteText instead.
	STableCell cell(inRow, 1);
	if (!GetLocalCellRect(cell, outRect))
		return false;
	cell.col = mCols;
	Rect cellRect;
	// Its inadequate to just use the mCols of the right most cell since if it is not visible
	// GetLocalCellRect will return false
	while (!GetLocalCellRect(cell, cellRect))
	{
		cell.col--;
		Assert_( cell.col > 0 );
	}
	outRect.right = cellRect.right;
	return true;
} // CStandardFlexTable::GetHiliteTextRect

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::GetIconRect(
	const STableCell&	inCell,
	const Rect&			inLocalRect,
	Rect&				outRect) const
//----------------------------------------------------------------------------------------
{
	GetDropFlagRect(inLocalRect, outRect);
	if (outRect.right > outRect.left) // have a drop flag
		outRect.right -= kDropIconWastedSpace;
	outRect.left = outRect.right + kIconMargin
					+ kIndentPerLevel * GetNestedLevel(inCell.row);
	outRect.right = outRect.left + kIconWidth;
	return true;
} // CStandardFlexTable::GetIconRect

//----------------------------------------------------------------------------------------
void CStandardFlexTable::HiliteRow(
	TableIndexT	inRow,
	Boolean				/*inHilite*/)
//----------------------------------------------------------------------------------------
{
	if (mHiliteDisabled)
		return;
	StRegion hiliteRgn;
    if (FocusExposed() && GetRowHiliteRgn(inRow, hiliteRgn))
    {
        StColorPenState saveColorPen;   // Preserve color & pen state
    	TableIndexT col = GetHiliteColumn();
    	if (col)
    	{
			Rect r;
			STableCell cell(inRow, col);
			GetLocalCellRect(cell, r);
			DrawIcons(cell, r);
    	}
        if (!IsActive() || !IsOnDuty())
        {
        	// Make a frame.  Do it this way so we get the "Invert" color trickery
        	// from the toolbox.  The previous code (srcXOR/FrameRgn) did not work
        	// well when the table was not the active view.
        	StRegion tempRgn;
        	::CopyRgn(hiliteRgn, tempRgn);
        	::InsetRgn(tempRgn, 1, 1);
        	::DiffRgn(hiliteRgn, tempRgn, hiliteRgn);
        }
		ApplyForeAndBackColors();
		DoHiliteRgn(hiliteRgn);		// can be overridden to do something different
	}
} // CStandardFlexTable::HiliteRow

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DoHiliteRgn( RgnHandle inHiliteRgn ) const
// Hilite the given area using the system hilite color. Override this to do 
// something different (like not use the hilite color).
//----------------------------------------------------------------------------------------
{
    UDrawingUtils::SetHiliteModeOn(); // Don't use hilite color, except for inline editing.
	::InvertRgn(inHiliteRgn);
} // CStandardFlexTable: DoHiliteRgn

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DoHiliteRect( const Rect & inHiliteRect ) const
// Hilite the given area using the system hilite color. Override this to do 
// something different (like not use the hilite color).
//----------------------------------------------------------------------------------------
{
    UDrawingUtils::SetHiliteModeOn(); // Don't use hilite color, except for inline editing.
	::InvertRect(&inHiliteRect);
} // CStandardFlexTable::DoHiliteRect

//----------------------------------------------------------------------------------------
void CStandardFlexTable::Click(SMouseDownEvent &inMouseDown)
//	Click the table. Special processing for drag-n-drop stuff.
//----------------------------------------------------------------------------------------
{
	LPane *clickedPane = FindSubPaneHitBy(inMouseDown.wherePort.h,
										  inMouseDown.wherePort.v);
	if ( clickedPane != nil )
		clickedPane->Click(inMouseDown);
	else
	{
		if ( !inMouseDown.delaySelect ) LCommander::SwitchTarget(this);
		StValueChanger<Boolean> change(inMouseDown.delaySelect, false);
			// For drag-n-drop functionality
		LPane::Click(inMouseDown);	//   will process click on this View
	}
} // CStandardFlexTable::Click

//----------------------------------------------------------------------------------------
Boolean	CStandardFlexTable::ClickDropFlag(
	const STableCell		&inCell,
	const SMouseDownEvent	&inMouseDown)
// Handle drop-flag ("twistie icon") clicks. Return true if handled.
//----------------------------------------------------------------------------------------
{
	Boolean isCellExpanded;
	if (CellHasDropFlag(inCell, isCellExpanded))
	{
		Rect cellRect, flagRect;		
		GetLocalCellRect(inCell, cellRect);
		GetDropFlagRect(cellRect, flagRect);		
		if (::PtInRect(inMouseDown.whereLocal, &flagRect) && FocusDraw())
		{
			Rect clipRect = flagRect;
			clipRect.right -= kDropIconWastedSpace;
			StClipRgnState clip(clipRect);
			if (LDropFlag::TrackClick(flagRect, inMouseDown.whereLocal, isCellExpanded))
				SetCellExpansion(inCell, !isCellExpanded);
			return true;
		}
	}	
	return false;
} // CStandardFlexTable::ClickDropFlag

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::HandleFancyDoubleClick(
	const STableCell		&inCell,
	const SMouseDownEvent	&inMouseDown)
//	Fancy double-click behavior.  There are two reasons for this new feature of 4.5:
//	е	Double-clicking a thread in a message thread view (for example) should
//		not select the new item, because then the selected message would load in
//		the message pane of the thread window as well as in the newly-opened message
//		window.  The user would prefer to maintain the message displayed in the current
//		window, and open the double-clicked, different message in the separate window.
//	е	Selecting a new row can be expensive, because it causes the message/folder
//		to load.  This loading usually unwanted after a double-click.
//----------------------------------------------------------------------------------------
{
	if (mFancyDoubleClick
		&& !CellIsSelected(inCell)
		&& (inMouseDown.macEvent.modifiers & shiftKey) == 0
		&& (inMouseDown.macEvent.modifiers & cmdKey) == 0
		)
	{
		if (GetClickCount() == 1)
		{
			// Single-click. First cancel any previous double-click in progress.
			CClickTimer::NoteSingleClick(this);
			// Set a timer.  If the timer times out, it will set the hilite
			// back and call ClickSelect again with fancy double-click set to false.
			mClickTimer = new CClickTimer(this, inCell, inMouseDown);
		}
		else
		{
			// Double-click.  Get rid of the timer.
			CClickTimer::NoteDoubleClick(this);
			OpenRow(inCell.row);
		}
		return true;
	}
	return false;
} // CStandardFlexTable::HandleFancyDoubleClick

//----------------------------------------------------------------------------------------
Boolean	CStandardFlexTable::ClickSelect(
	const STableCell		&inCell,
	const SMouseDownEvent	&inMouseDown)
//----------------------------------------------------------------------------------------
{
	Rect cellRect;
	GetLocalCellRect(inCell, cellRect);
	
	FocusDraw();	// To be on the safe side
	
	// Compute the text and icon rectangles and if the click is in them
	Rect textRect, iconRect;
	GetHiliteTextRect(inCell.row, false, textRect);
	GetIconRect(inCell, cellRect, iconRect);
	Boolean clickIsInIcon = ::PtInRect(inMouseDown.whereLocal, &iconRect);
	Boolean clickIsInTitle = ::PtInRect(inMouseDown.whereLocal, &textRect);
	
	// If the click is outside of the icon/title, or if it is in the title
	// (and in-place editing is on), then temporarily turn off the fancy double-click
	// stuff since we don't want it to happen.  Use a StValueChanger stack object,
	// which will restore the state even on an exception or return statement.
	StValueChanger<Boolean> changer(mFancyDoubleClick, mFancyDoubleClick);
	if (!clickIsInIcon && !clickIsInTitle)
		mFancyDoubleClick = false;	// click was outside icon&title
	if (clickIsInTitle && mNameEditor)
		mFancyDoubleClick = false;	// click was in title and we're doing inplace editing			

#define CHECK_DRAGS_BEFORE_DOUBLECLICK 1
#if !CHECK_DRAGS_BEFORE_DOUBLECLICK
	// The way it was before 98/04/03
	if (HandleFancyDoubleClick(inCell, inMouseDown))
		return false;
#endif
	
	// 0, 1, >=2 rows selected are the values that change command-enabling configurations.
	int selectedRowCountForCommandStatus = GetSelectedRowCount();
	if (selectedRowCountForCommandStatus > 2)
		selectedRowCountForCommandStatus = 2;
	Boolean result = false;
	
	// There are two cases here: user clicks in the hilite column and they don't click in
	// the hilite column. If they do, only select if the click is in the icon or the icon
	// text. Any other click w/in that column unselects or start a marquee selection. If
	// they click outside the hilite column, process as normal.
	
	if (inCell.col != GetHiliteColumn() || clickIsInIcon || clickIsInTitle)
	{
#if CHECK_DRAGS_BEFORE_DOUBLECLICK
		// Experimental, new way 98/04/03.  The restraints are:
		//	е	Drags should be checked first.
		//	е	In the spirit of fancy double click, drags of unselected items
		//		should be allowed if fancyDoubleClick is in force.
		//	е	Fancy double-click must be checked before calling LTableView::ClickSelect
		//		(because the FDC detector calls it eventually as appropriate).
		//	е	Selection must occur before context menus are informed of the click.  
		//		This is because context menus perform commands on the current selection.
		// Therefore,
		//		DRAG before DOUBLE-CLICK DETECTION before SELECTION before CONTEXT MENUS.
		
		// Preflight to determine drag/click/contextmenu.
		EClickState mouseResult = CContextMenuAttachment::WaitMouseAction( inMouseDown.whereLocal,
																			inMouseDown.macEvent.when);

		if (! CellInitiatesDrag(inCell) && mouseResult == eMouseDragging)
			mouseResult = eMouseTimeout;
		switch (mouseResult)
		{
			case eMouseDragging:
				// We are dragging, and may be dragging a non-selected item, which is
				// permissible now.
				// First cancel any previous double-click in progress.  This will select
				// the cell previously clicked, by the way.
				CClickTimer::NoteSingleClick(this);
				if (!mFancyDoubleClick)
				{
					// Normal case
					LTableView::ClickSelect(inCell, inMouseDown);
					DragSelection(inCell, inMouseDown);
					result = true;
				}
				else if (CellIsSelected(inCell))
				{
					DragSelection(inCell, inMouseDown);
					// cell and the other selected cells
					result = false;
				}
				else if (userCanceledErr == DragRow(inCell.row, inMouseDown))
				{
					// We tried to drag the unselected row.
					// userCanceledErr means the drag did not really occur.  Select the
					// item in this case.
					LTableView::ClickSelect(inCell, inMouseDown);
					result = true;
				}
				goto cmd_status; // because we changed the selection.
				
			case eMouseTimeout:
				if (!IsTarget())
					break; // no context menus when not the active window.
				// We are showing the context menu
				// First cancel any previous double-click in progress.  This will select
				// the cell previously clicked, by the way.
				CClickTimer::NoteSingleClick(this);

				CContextMenuAttachment::SExecuteParams params;
				params.inMouseDown = &inMouseDown;
				
				// First call ClickSelect to select the cell before showing the menu
				LTableView::ClickSelect(inCell, inMouseDown);
				
				// If the preflight test is a sane strategy, then the call we will now
				// make should always show the popup menu.  However, the user may
				// examine the popup menu and track out.  The call should then return
				// true, and we should handle it like a click by falling through:
				if (!ExecuteAttachments(
						CContextMenuAttachment::msg_ContextMenu,
						(void*)&params))
				{
					break;
				}
				// else fall through and handle the click
			case eMouseUpEarly:
			case eMouseWasCommandClick:		
				// CLICK!
				if (!IsTarget())
				{
					// Become target.  Don't do other click processing.
					SwitchTarget(this);
					break;
				}
				if (HandleFancyDoubleClick(inCell, inMouseDown))
					return false;
				if (!LTableView::ClickSelect(inCell, inMouseDown))
					return false;

				// Check if the click is in the title of the icon. If so, and an editor is
				// present and we're not doing a shift-selection extension and it's not a 
				// double click then invoke the inline editor. Otherwise process the click
				// normally. (whew!)
				//
				// The reason for not inline editing on a double-click is that it allows the 
				// user to double-click anywhere on the item to open it, like in the Finder.
				// Doing a double-click to edit an item doesn't make sense and the user
				// would get confused.
				// open selection if we've got the right click count
				if (clickIsInTitle
					&& mNameEditor
					&& !(inMouseDown.macEvent.modifiers & shiftKey)
					&& GetClickCount() != 2)
						CDeferredTaskManager::Post(
							new CDeferredInlineEditTask(this, inCell, textRect),
							this, true);
				if (GetClickCount() == ClickCountToOpen())
				{
					// Cancel inline editing.  Probably redundant.
					if (mNameEditor)
						mNameEditor->UpdateEdit(nil, nil, nil);
					OpenClick( inCell );
				}
				result = true;
						
				break;
		} // switch
				
#else
		if (LTableView::ClickSelect(inCell, inMouseDown))
		{
			// Handle it ourselves if the popup attachment doesn't want it.
			CContextMenuAttachment::SExecuteParams params;
			params.inMouseDown = &inMouseDown;
			if (ExecuteAttachments(CContextMenuAttachment::msg_ContextMenu, (void*)&params))
			{
				// drag ? - don't become target
				if (CellInitiatesDrag(inCell) && ::WaitMouseMoved(inMouseDown.macEvent.where))
					DragSelection(inCell, inMouseDown);
				else
				{
					// become target
					if (!IsTarget())
						SwitchTarget(this);

					// Check if the click is in the title of the icon. If so, and an editor is
					// present and we're not doing a shift-selection extension and it's not a 
					// double click then invoke the inline editor. Otherwise process the click
					// normally. (whew!)
					//
					// The reason for not inline editing on a double-click is that it allows the 
					// user to double-click anywhere on the item to open it, like in the Finder.
					// Doing a double-click to edit an item doesn't make sense and the user
					// would get confused.
					// еееDouble click check doesn't work right now.....will fix later....еее
					if (clickIsInTitle
						&& mNameEditor
						&& !(inMouseDown.macEvent.modifiers & shiftKey)
						&& GetClickCount() != 2)
						DoInlineEditing(inCell, textRect);
					else
					{				
						// open selection if we've got the right click count
						if (GetClickCount() == ClickCountToOpen())
							OpenClick ( inCell );
					}
				}
				result = true;
			}		
		}
#endif // CHECK_DRAGS_BEFORE_DOUBLECLICK
	} // if click in icon or text
	else
	{
		CClickTimer::NoteSingleClick(this);

		// this is the case where you've clicked in a valid row, but
		// outside the contents of the cell
		if (! LWindow::FetchWindowObject(GetMacPort())->IsOnDuty())
			return result;

		// since the click is not in the icon and not in the text, the selection should be
		// cleared before we try to show any context menus. This is faster than using
		// UnselectAllCells() because it only iterates over the current selection.
		Rect empty = { 0, 0, 0, 0 };
		UnselectCellsNotInSelectionOutline(empty);
		CContextMenuAttachment::SExecuteParams params;
		params.inMouseDown = &inMouseDown;
		if (ExecuteAttachments(CContextMenuAttachment::msg_ContextMenu, (void*)&params))
			HandleSelectionTracking(inMouseDown);
	}
	
cmd_status:
	int newSelectedRowCountForCommandStatus = GetSelectedRowCount();
	if (newSelectedRowCountForCommandStatus > 2)
		newSelectedRowCountForCommandStatus = 2;
	if (selectedRowCountForCommandStatus != newSelectedRowCountForCommandStatus)
		SetUpdateCommandStatus(true);
	return result;
} // CStandardFlexTable::ClickSelect

//----------------------------------------------------------------------------------------
void CStandardFlexTable::ClickSelf(const SMouseDownEvent &inMouseDown)
// Overridden so that certain cells (e.g. toggled icons) don't select on click and to
// handle marquee selection.
//----------------------------------------------------------------------------------------
{
	STableCell	hitCell;
	SPoint32	imagePt;
	
	LocalToImagePoint(inMouseDown.whereLocal, imagePt);
	if (GetCellHitBy(imagePt, hitCell))
	{
		// Handle drop-flag clicks first
		if (ClickDropFlag(hitCell, inMouseDown))
			return;

		// See if the cell selects. If so go ahead and process it (could be a drag or
		// selection outline). If the cell does not select, the cell may still really want
		// the click (like the "marked read message" column in mail/news. If it does, give
		// it the click, otherwise start doing a marquee selection.
		if ( CellSelects(hitCell) ) {
			if ( ClickSelect(hitCell, inMouseDown) )
				ClickCell(hitCell, inMouseDown);
		}
		else {
			if ( CellWantsClick(hitCell) )
				ClickCell(hitCell, inMouseDown);
			else
				HandleSelectionTracking(inMouseDown);
		}
	}
	else
	{
		// if this window is not foremost, don't do anything
		if (! LWindow::FetchWindowObject(GetMacPort())->IsOnDuty())
			return;

		// since the click is not in any cell, the selection should be cleared before
		// we try to show the context menus. This is faster than using
		// UnselectAllCells() because it only iterates over the current selection.
		Rect empty = { 0, 0, 0, 0 };
		UnselectCellsNotInSelectionOutline(empty);

		CContextMenuAttachment::SExecuteParams params;
		params.inMouseDown = &inMouseDown;
		if ( ExecuteAttachments(CContextMenuAttachment::msg_ContextMenu, (void*)&params) )
			HandleSelectionTracking(inMouseDown);
	}		
} // CStandardFlexTable::ClickSelf

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DoInlineEditing( const STableCell &inCell )
// Perform the necessary setup for inline editing of a row's main text and then show the edit field.
// This version of the routine is public and does not require any external knowledge of table internals
// besides the cell that we should edit. Most of the work is done by calling the routine below
//----------------------------------------------------------------------------------------
{
	Rect textRect;
	GetHiliteTextRect( inCell.row, false, textRect );
	DoInlineEditing( inCell, textRect );	
} // DoInlineEditing


//----------------------------------------------------------------------------------------
void CStandardFlexTable::DoInlineEditing(const STableCell &inCell, Rect& inTextRect)
// Perform the necessary setup for inline editing of a row's main text and then show
// the edit field
//----------------------------------------------------------------------------------------
{
	mRowBeingEdited = inCell.row; // do this first so derived classes can check it.
	ExecuteAttachments(msg_HideTooltip, nil);
	if (!CanDoInlineEditing())	// bail if inline editing is temporarily turned off
	{
		mRowBeingEdited = LArray::index_Bad;
		return;
	}
	
	// Erase the text rectangle so that when the text field shrinks, you won't see the
	// old name behind it. This is kinda skanky, but we need to make sure we draw the
	// background color correctly.
	// I also believe it is unnecessary, because it didn't solve the problem of the cell
	// being redrawn on a refresh, so and so that had to be done anyway. A better way
	// is to invalidate the rectangle, and make the table smart about not drawing the
	// text when editing is going on.   - jrm 98/04/02
	#if 1
	::InvalRect(&inTextRect); // table, which is inline-edit aware, will erase the text.
	SetHiliteDisabled(true); // Don't draw hilite rects, either.
	#else
	RGBColor backColor; 
	::GetCPixel(inTextRect.right - 1, inTextRect.top, &backColor);
	::RGBBackColor(&backColor);
	::EraseRect(&inTextRect);
	#endif

	SPoint32 imagePoint;
	LocalToImagePoint(topLeft(inTextRect), imagePoint);
	imagePoint.h -= 1; imagePoint.v -= 1;
	
	LCommander::SwitchTarget(mNameEditor);
	
	// Don't broadcast that the selection has changed until we update the edit field
	SetNotifyOnSelectionChange(false);
	SelectCell(STableCell(mRowBeingEdited, GetHiliteColumn()));
	SetNotifyOnSelectionChange(true);
	
	// Need 512 bytes for the text result, because of the &*(&^% way GetHiliteText
	// works now.
	char nameString[512];
	GetHiliteText(mRowBeingEdited, nameString, sizeof(nameString), false, &inTextRect);
	InsetRect(&inTextRect, -2, -2);
	inTextRect.bottom += 2;
	SetEditParam(inTextRect.right - inTextRect.left, inTextRect.bottom - inTextRect.top, nameString, imagePoint);
	SelectionChanged();
} // DoInlineEditing

//----------------------------------------------------------------------------------------
void CStandardFlexTable::SetEditParam(int w, int h, char* str, SPoint32& ImagePoint)

{
	mNameEditor->ResizeFrameTo(w, h, true);
	mNameEditor->UpdateEdit(CStr255(str), &ImagePoint, nil);
} // SetEditParam
//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::CellSelects(const STableCell& /*inCell*/) const
// Determines if a cell is allowed to select the row.
//----------------------------------------------------------------------------------------
{
	return true;
} // CStandardFlexTable::CellSelects

//----------------------------------------------------------------------------------------
void CStandardFlexTable::HandleSelectionTracking ( const SMouseDownEvent & inMouseDown )
// Set up everything to start tracking a selection marquee
//----------------------------------------------------------------------------------------
{
	CClickTimer::NoteSingleClick(this);
	
	if (!IsTarget())
		SwitchTarget(this);
	// bail if the table specifically doesn't want the tracking.
	if ( !TableDesiresSelectionTracking() )
		return;
		
	// Click is outside of any Cell. Unselect everything if the shift key is up.
	if ( !(inMouseDown.macEvent.modifiers & shiftKey) && !(inMouseDown.macEvent.modifiers & cmdKey))
		UnselectAllCells();
	
	// track the mouse for doing drag selection
	FocusDraw();
	TrackSelection( inMouseDown );
	
} // HandleSelectionTracking

//----------------------------------------------------------------------------------------
void CStandardFlexTable::TrackSelection( const SMouseDownEvent & inMouseDown )
// Do the tracking of when the mouse is down and the user wants to do a multiple selection
// by drawing a selection marquee.
//----------------------------------------------------------------------------------------
{
	Point originalLoc = inMouseDown.whereLocal;

	Rect 	selectionRect	= { 0, 0, 0, 0 };
	Rect 	cellHotSpotRect = { 0, 0, 0, 0 }, iconHotSpotRect = { 0, 0, 0, 0 };

	Boolean	shiftKeyDown 	= (inMouseDown.macEvent.modifiers & shiftKey) != 0;
	Boolean	cmdKeyDown 		= (inMouseDown.macEvent.modifiers & cmdKey) != 0;

	Point lastMousePoint = { 0, 0 }, curMousePoint = { 0, 0 }, curOrigin = { 0, 0 };

	TableIndexT	currentRow 	= 0L, currentCol = 0L, maxRows = 0L, maxColumns = 0L;
	
	// may get a null selector for some tables not using the table row selector
	LTableRowSelector	*theSelector = dynamic_cast<LTableRowSelector *>(GetTableSelector());
	StRegion			previousSelection;
	
	if (theSelector != nil)
		//LTableRowSelector inherits from LArray, so we can call copy constructor
		theSelector->MakeSelectionRegion(previousSelection, GetHiliteColumn());
	
	StColorPenState 	oldPenState;
	
	GetTableSize( maxRows, maxColumns );
	
	STableCell		topLeftCell( 0, 0 );
	STableCell		botRightCell( 0, 0 );
	STableCell		oldTopLeftCell( maxRows, 0 );		// initial values important for optimization below
	STableCell		oldBotRightCell( 1, 0 );
	
	FocusDraw();
	
	SetNotifyOnSelectionChange(false);
	while ( ::StillDown() )
	{
		::GetMouse( &curMousePoint );
		
		if (curMousePoint.v != lastMousePoint.v || curMousePoint.h != lastMousePoint.h)
		{
			::PenPat( &qd.gray );
			::PenMode( patXor );
			::PenSize( 2, 2 );

			if ( !::EmptyRect(&selectionRect) )
				::FrameRect( &selectionRect );
			
			lastMousePoint = curMousePoint;
			
			::Pt2Rect( curMousePoint, originalLoc, &selectionRect );	

			// Don't let the selection rect be empty. Trust me.
			if ( (selectionRect.right - selectionRect.left) == 0 )
				::InsetRect( &selectionRect, -1, 0 );
			if ( (selectionRect.bottom - selectionRect.top) == 0 )
				::InsetRect( &selectionRect, 0, -1 );
			
			//
			// Restore the text traits, scroll the table if needed, unselect all the cells 
			// that shouldn't be selected, select the ones that should be selected, 
			// reset the text traits and *then* finally draw the gray rect.
			//
			
			oldPenState.Restore();

			if ( AutoScrollImage(curMousePoint) ) 
				FocusDraw();
		
			FetchIntersectingCells( selectionRect, topLeftCell, botRightCell );
		
			STableCell theCell(0, GetHiliteColumn());
			
			// Do this inline now, because we need to get at some more local variables
			// UnselectCellsNotInSelectionOutline(selectionRect);
			
			if (!shiftKeyDown && !cmdKeyDown)
			{
				// Deselect cells not in selection rect
				while ( GetNextSelectedRow(theCell.row) )
				{
					if ( !HitCellHotSpot( theCell, selectionRect) )
						UnselectCell( theCell );
				}
			} else if (shiftKeyDown)
			{
				// Only deselect cells that have left the current selection marquee,
				// i.e. those that are neither in the marquee, nor the previous selection
				while ( GetNextSelectedRow(theCell.row) )
				{
					Point		thePt;
					theCell.ToPoint(thePt);
					if ( !HitCellHotSpot( theCell, selectionRect) && !::PtInRgn(thePt, previousSelection)) {
						UnselectCell( theCell );
					}
				}
				
			} else if (cmdKeyDown)
			{
				// Put the selection back to its previous state for cells that are no longer
				// covered by the marquee
				
				// optimization to reduce the amount of looping here
				TableIndexT		extentTop = MIN(topLeftCell.row, oldTopLeftCell.row);
				TableIndexT		extentBottom = MAX(botRightCell.row, oldBotRightCell.row);
				
				if (extentBottom > mRows)
					extentBottom = mRows;
				
				theCell.row = extentTop;
				
				while ( theCell.row <= extentBottom)
				{
					if ( !HitCellHotSpot( theCell, selectionRect) )
					{
						Point		thePt;
						theCell.ToPoint(thePt);
						Boolean		alreadySelected = CellIsSelected( theCell );
						
						if (::PtInRgn(thePt, previousSelection)) {
							if ( !alreadySelected) SelectCell( theCell );
						} else {
							if ( alreadySelected ) UnselectCell( theCell );
						}
					}
					theCell.row ++;
				}
			}

			// find what cells are in w/in the rectangle and select them if the rectangle
			// encloses/touches the cell hot spot (usually the icon or icon text).
			
			for ( currentRow = topLeftCell.row; currentRow <= botRightCell.row; currentRow++ )
			{
				STableCell		currentCell( currentRow, GetHiliteColumn() );
				
				// We are assuming now that all descendents from CStandardFlex table
				// have row-based selection. Since this assumption is also made elsewhere
				// (e.g. in xxxxx) we should be safe. The class hierarchy should make it
				// explicit though.
				/*
				for ( currentCol = topLeftCell.col; currentCol <= botRightCell.col; currentCol++ )
				{
					currentCell.row = currentRow;
					currentCell.col = currentCol;
					if (!CellIsSelected(currentCell) && HitCellHotSpot(currentCell, selectionRect))
						SelectCell( currentCell );
				} // for each column
				*/
				
				if (cmdKeyDown)
				{
					// When the marquee hits a cell, toggle the select for that cell
					
					if ( HitCellHotSpot(currentCell, selectionRect) )
					{
						Point		thePt;
						currentCell.ToPoint(thePt);
					
						if ( ::PtInRgn(thePt, previousSelection) )
							UnselectCell( currentCell );
						else
							SelectCell( currentCell );
					}
					
				} else
				{
					if (!CellIsSelected(currentCell) && HitCellHotSpot(currentCell, selectionRect))
						SelectCell( currentCell );
				}
				
			} // for each row
			
			FocusDraw();
			
			oldTopLeftCell = topLeftCell;
			oldBotRightCell = botRightCell;
			
			oldPenState.Save();

			::PenPat( &qd.gray );
			::PenMode( patXor );
			::PenSize( 2, 2 );
			::FrameRect( &selectionRect );

		} // if mouse has moved
		
	} // while mouse still down
	
	SetNotifyOnSelectionChange(true);
	SelectionChanged();

	if ( !::EmptyRect(&selectionRect) )
	{
		FocusDraw(); // who knows what went on inside "SelectionChanged()"?
		
		::PenPat( &qd.gray );
		::PenMode( patXor );
		::PenSize( 2, 2 );
		::FrameRect( &selectionRect );
		
		oldPenState.Normalize();
	}
		
} // TrackSelection

//----------------------------------------------------------------------------------------
void CStandardFlexTable::UnselectCellsNotInSelectionOutline( const Rect & inSelectionRect )
// Iterate over existing selection looking for things that are no longer w/in
// the rectangle and unselect them. We turn off the "notify on selection changed"
// mechanism so as not to be very slow...
//
// IMPORTANT: This routine assumes a selection model that selects the entire row.
// If another selection model is used (such as one that only selects individual
// cells), this needs to be rewritten.
//----------------------------------------------------------------------------------------
{
	SetNotifyOnSelectionChange ( false );
	
	TableIndexT currentRow = LArray::index_Bad;
	while ( GetNextSelectedRow(currentRow) ) {

		STableCell currentCell;
		currentCell.row = currentRow;
		currentCell.col = GetHiliteColumn();
		if ( !HitCellHotSpot( currentCell, inSelectionRect) ) {
			UnselectCell( currentCell );
			currentRow = currentCell.row;
		}
		
	}
	
	SetNotifyOnSelectionChange ( true );

} // UnselectCellsNotInSelectionOutline

//----------------------------------------------------------------------------------------
void CStandardFlexTable::GetLocalCellRectAnywhere( const STableCell	&inCell, Rect &outCellRect) const
// Unlike GetLocalCellRect(), this returns a valid rectangle whether or not this cell
// is in view
//----------------------------------------------------------------------------------------
{
	Int32	cellLeft, cellTop, cellRight, cellBottom;
	mTableGeometry->GetImageCellBounds(inCell, cellLeft, cellTop, cellRight, cellBottom);
	
	SPoint32	imagePt;
	imagePt.h = cellLeft;
	imagePt.v = cellTop;
	ImageToLocalPoint(imagePt, topLeft(outCellRect));
	outCellRect.right = outCellRect.left + (cellRight - cellLeft);
	outCellRect.bottom = outCellRect.top + (cellBottom - cellTop);

}


//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::HitCellHotSpot( const STableCell &inCell, const Rect &inTotalSelectionRect )
//----------------------------------------------------------------------------------------
{
	TableIndexT hiliteCol = GetHiliteColumn();
	
	//FocusDraw();	//to be on the safe side, but it's too slow
	
	if ( hiliteCol != LArray::index_Bad )
	{
		if ( hiliteCol == inCell.col ) {
			Rect cellRect;
			
			GetLocalCellRectAnywhere(inCell, cellRect);
		
			Rect textRect, iconRect;
			GetHiliteTextRect( inCell.row, true, textRect );
			GetIconRect( inCell, cellRect, iconRect );
			UnionRect(&textRect, &iconRect, &textRect);
	 		return ::SectRect(&textRect, &inTotalSelectionRect, &textRect);
		}
	}
	else			// check for any overlap with the row
	{
		Rect	leftCellRect	= { 0, 0, 0, 0 }, rightCellRect	= { 0, 0, 0, 0 };
		
		STableCell	leftCell(inCell.row, 1);
		STableCell	rightCell(inCell.row, mCols);
		GetLocalCellRectAnywhere( leftCell, leftCellRect );
		GetLocalCellRectAnywhere( rightCell, rightCellRect );
		leftCellRect.right = rightCellRect.right;
		return ::SectRect( &leftCellRect, &inTotalSelectionRect, &leftCellRect );		
	}
	
	return false;
} // HitCellHotSpot


//----------------------------------------------------------------------------------------
void CStandardFlexTable::BeTarget() 
//----------------------------------------------------------------------------------------
{
	LCommander::BeTarget();
	Activate();
	ExecuteAttachments(CTargetFramer::msg_BecomingTarget, this);
} // CStandardFlexTable::BeTarget

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DontBeTarget() 
//----------------------------------------------------------------------------------------
{
	CClickTimer::NoteSingleClick(this);
	ExecuteAttachments(CTargetFramer::msg_ResigningTarget, this);
	Deactivate();
	LCommander::DontBeTarget();
} // CStandardFlexTable::DontBeTarget

// ===== BEGINCEXTable Refugees

//----------------------------------------------------------------------------------------
void CStandardFlexTable::RefreshRowRange(TableIndexT inStartRow, TableIndexT inEndRow) const
//----------------------------------------------------------------------------------------
{
	TableIndexT topRow, botRow;
	if ( inStartRow < inEndRow ) {
		topRow = inStartRow; botRow = inEndRow;
	} else {
		topRow = inEndRow; botRow = inStartRow;
	}
	
	// RefreshCellRange should be const, but isn't
	CStandardFlexTable* self = const_cast<CStandardFlexTable*>(this);
	self->RefreshCellRange(STableCell(topRow, 1), STableCell(botRow, mCols));
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::ReadSavedTableStatus(LStream *inStatusData)
//----------------------------------------------------------------------------------------
{
	if ( inStatusData != nil )
	{
		
		Assert_(mTableHeader != nil);

		mTableHeader->ReadColumnState(inStatusData);
	}
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::WriteSavedTableStatus(LStream *outStatusData)
//----------------------------------------------------------------------------------------
{
	Assert_(mTableHeader != nil);
	mTableHeader->WriteColumnState(outStatusData);
}
// ===== END CEXTable Refugee functions

//----------------------------------------------------------------------------------------
PaneIDT CStandardFlexTable::GetCellDataType(const STableCell &inCell) const
//----------------------------------------------------------------------------------------
{
	return mTableHeader->GetColumnPaneID(inCell.col);
} // CStandardFlexTable::GetCellDataType

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::GetNextSelectedRow(TableIndexT& inOutAfterRow) const
//----------------------------------------------------------------------------------------
{
	TableIndexT	nRows, nCols;
	GetTableSize(nRows, nCols);
	STableCell cell(inOutAfterRow, nCols);
	if (LTableView::GetNextSelectedCell(cell))
	{
		inOutAfterRow = cell.row;
		return true;
	}
	return false;
	
} // CStandardFlexTable::GetNextSelectedRow

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::GetLastSelectedRow(TableIndexT& inOutBeforeRow) const
// Currently a little inefficient...
//----------------------------------------------------------------------------------------
{
	STableCell cell(inOutBeforeRow, 1);
	while (cell.row-- > LArray::index_Bad)
	{
		if (CellIsSelected(cell))
		{
			inOutBeforeRow = cell.row;
			return true;
		}
	}
	return false;
} // CStandardFlexTable::GetLastSelectedRow

//----------------------------------------------------------------------------------------
void CStandardFlexTable::FindCommandStatus(
	CommandT			inCommand,
	Boolean				&outEnabled,
	Boolean				&outUsesMark,
	Char16				&outMark,
	Str255				outName)
//----------------------------------------------------------------------------------------
{
	switch (inCommand)
	{
		case cmd_GetInfo:
			outEnabled = GetSelectedRowCount() == 1;
			break;
		// These commands require one or more lines to be selected.
		case cmd_OpenSelection:
		case cmd_OpenSelectionNewWindow:
		case cmd_Clear:
		case cmd_SecurityInfo:
			outEnabled = GetSelectedRowCount() > 0;
			break;
		// Selection not necessary
		case cmd_SelectAll:
			outEnabled = true;
			break;
		default:
			LCommander::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
	}
} // CStandardFlexTable::FindCommandStatus

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::ObeyCommand(
	CommandT	inCommand,
	void		*ioParam)
//----------------------------------------------------------------------------------------
{
	Boolean	result = false;
	switch(inCommand)
	{
		case cmd_OpenSelectionNewWindow:
		case cmd_OpenSelection:
			CClickTimer::NoteSingleClick(this);
			OpenSelection();
			return true;
		case cmd_GetInfo:
			CClickTimer::NoteSingleClick(this);
			GetInfo();
			return true;
		case cmd_SelectAll:
			SelectAllCells();
			return true;
		case cmd_Clear:
			CClickTimer::NoteSingleClick(this);
			DeleteSelection(CApplicationEventAttachment::GetCurrentEvent());
			return true;
		default:
			result = LCommander::ObeyCommand(inCommand, ioParam);
			break;
	}
	return result;
} // CStandardFlexTable::ObeyCommand


//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::HandleKeyPress(	const EventRecord& inKeyEvent)
//----------------------------------------------------------------------------------------
{
	// We are NOT masking off keyUp events.  So let's beware of them!
	if (inKeyEvent.what == keyUp)
		return false;
	Char16	c = inKeyEvent.message & charCodeMask;
	switch (c)
	{
		case char_Return:
		case char_Enter:
			CClickTimer::NoteSingleClick(this);
			OpenSelection();
			return true;
		case char_Backspace:
		case char_FwdDelete:
			CClickTimer::NoteSingleClick(this);
			DeleteSelection(inKeyEvent);
			return true;
		case char_LeftArrow:
		case char_RightArrow:
			CClickTimer::NoteSingleClick(this);
			if (GetSelectedRowCount() == 1)
			{
				STableCell cell = mTableSelector->GetFirstSelectedCell();
				Boolean isCellExpanded;
				// selected cell will be in column 1, but dropflag may not be.
				for (; IsValidCell(cell); cell.col++)
				{
					if (CellHasDropFlag(cell, isCellExpanded))
					{
						SetCellExpansion(cell, c == char_RightArrow);
						break;
					}
				}
			}
		default:
			LCommander::HandleKeyPress(inKeyEvent);
			return true;
	} // switch
	return false;
} // CStandardFlexTable::HandleKeyPress

//----------------------------------------------------------------------------------------
void CStandardFlexTable::OpenClick( const STableCell& /* inCell */  )
//----------------------------------------------------------------------------------------
{
	OpenSelection();
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::OpenSelection()
//----------------------------------------------------------------------------------------
{
	TableIndexT selectedRow = 0;
	while (GetNextSelectedRow(selectedRow) && !CmdPeriod())
		OpenRow(selectedRow);
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::GetInfo()
//----------------------------------------------------------------------------------------
{
	// Base class does nothing
}

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::RowCanAcceptDrop(
	DragReference	/*inDragRef*/,
	TableIndexT		/*inDropRow*/)
//----------------------------------------------------------------------------------------
{
	return false;
} // CStandardFlexTable::RowCanAcceptDrop

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::RowCanAcceptDropBetweenAbove(
	DragReference	/*inDragRef*/,
	TableIndexT		/*inDropRow*/)
//----------------------------------------------------------------------------------------
{
	return false;
} // CStandardFlexTable::RowCanAcceptDropBetweenAbove

//----------------------------------------------------------------------------------------
TableIndexT CStandardFlexTable::GetHiliteColumn() const
//----------------------------------------------------------------------------------------
{
	return LArray::index_Bad; // default is to hilite the whole row.
} // CStandardFlexTable::GetHiliteColumn

//----------------------------------------------------------------------------------------
void CStandardFlexTable::HiliteDropArea(DragReference inDragRef)
// Show that this view is a drop site, but only if there aren't any items
// in the tree already. If there are, the inline drop feedback should take care of it.
//----------------------------------------------------------------------------------------
{
	if ( mRows && mInlineFeedbackOn )		// let in-line drop feedback do the job
		return;

	Rect frame;
	CalcLocalFrameRect ( frame );
	
	// show the drag hilite in drop area
	try {
		StRegion rgn;
		::RectRgn ( rgn, &frame );
		StColorPenState::Normalize();
		::InsetRgn ( rgn, 5, 5 );
		::ShowDragHilite ( inDragRef, rgn, true );
	}
	catch ( ... ) { }
	
} // CStandardFlexTable::HiliteDropArea

//----------------------------------------------------------------------------------------
void CStandardFlexTable::ComputeItemDropAreas(
	const Rect& inLocalCellRect,
	Rect& outTop, 
	Rect& outBottom )
// When a drag goes over a cell that contains a non-folder, divide the node in half. The
// top half will represent dropping above the item, the bottom after.
//----------------------------------------------------------------------------------------
{
	outBottom = outTop = inLocalCellRect;
	uint16 midPt = (outTop.bottom - outTop.top) / 2;
	outTop.bottom = outTop.top + midPt;
	outBottom.top = outTop.bottom + 1;
} // ComputeItemDropAreas // ComputeItemDropAreas

//----------------------------------------------------------------------------------------
void CStandardFlexTable::ComputeFolderDropAreas(
	const Rect& inLocalCellRect,
	Rect& outTop, 
	Rect& outBottom )
// When a drag goes over a cell that contains a folder, divide the cell area into 3 parts.
// The middle area, which corresponds to a drop on the folder takes up the majority of
// the cell space with the top and bottom areas (corresponding to drop before and drop
// after, respectively) taking up the rest at the ends.
//----------------------------------------------------------------------------------------
{
	const Uint8 capAreaHeight = 4;

	outBottom = outTop = inLocalCellRect;
	outTop.bottom = outTop.top + capAreaHeight;
	outBottom.top = outBottom.bottom - capAreaHeight;
} // ComputeFolderDropAreas

//----------------------------------------------------------------------------------------
void CStandardFlexTable::InsideDropArea(DragReference inDragRef)
// (override from LDragAndDrop)
//----------------------------------------------------------------------------------------
{
	Point			mouseLoc;
	SPoint32		imagePt;
	Rect			rowBounds;					// only top/bottom are important
	
	FocusDraw();
	
#if 0
	// If the table is sorted, don't let the user drop in any given location. Just
	// hilight the entire area
	//еее this can't go here, else you won't be able to drop in folders when view sorted!!
	if ( ! TableSupportsNaturalOrderSort() ) {
		StValueChanger<Boolean> old ( mInlineFeedbackOn, false );
		HiliteDropArea(inDragRef);
		return;
	} // if container is sorted	
#endif
	
	::GetDragMouse(inDragRef, &mouseLoc, NULL);
	::GlobalToLocal(&mouseLoc);
	// Auto scroll code -- jrm 97/05/03
	if (AutoScrollImage(mouseLoc))
	{
		Rect frame;
		CalcLocalFrameRect(frame);
		Int32 pt = ::PinRect(&frame, mouseLoc);
		mouseLoc = *(Point*)&pt;
	}
	LocalToImagePoint(mouseLoc, imagePt);
	TableIndexT rowMouseIsOver = mTableGeometry->GetRowHitBy(imagePt);
	GetLocalCellRect ( STableCell(rowMouseIsOver, GetHiliteColumn()), rowBounds );

	// Find the index of where we want to drop the item and if it is between two rows
	// of if it is on a row (can only happen if row is a container).
	Boolean newIsDropBetweenRows = false;
	TableIndexT newDropRow = LArray::index_Bad;
	if ( rowMouseIsOver >= 1 )
	{
		// is the current row in the existing table or off the end? If it's off the end, we
		// know we're dropping between rows.
		if ( rowMouseIsOver <= mRows ) {
			// divide row horizontally into several parts (3 if a container, 2 if not) and determine which
			// part of the row the mouse is over. This will determine if we should interpret this as
			// a drop before/after or a drop on.
			if ( RowIsContainer(rowMouseIsOver) ) {
				Rect topArea, bottomArea;
				ComputeFolderDropAreas ( rowBounds, topArea, bottomArea );
				if ( ::PtInRect(mouseLoc, &topArea) ) {
					newDropRow = rowMouseIsOver;			// before current row
					newIsDropBetweenRows = true;
				}
				else if ( ::PtInRect(mouseLoc, &bottomArea) ) {
					newDropRow = rowMouseIsOver + 1;		// after current row
					newIsDropBetweenRows = true;
				}
				else
					newDropRow = rowMouseIsOver;			// hilite folder, don't draw line
			}
			else {
				Rect topArea, bottomArea;
				ComputeItemDropAreas ( rowBounds, topArea, bottomArea );
				if ( ::PtInRect(mouseLoc, &topArea) )
					newDropRow = rowMouseIsOver;			// before current row
				else
					newDropRow = rowMouseIsOver + 1;		// after current row
				newIsDropBetweenRows = true;				// draw line between rows
			}
		}
		else {
			newDropRow = mRows + 1;
			newIsDropBetweenRows = true;
			newDropRow = rowMouseIsOver;
		}
		
		// we now know where the drop SHOULD go, now check if it CAN go there.
		newIsDropBetweenRows &= RowCanAcceptDropBetweenAbove(inDragRef, newDropRow);
		if (!newIsDropBetweenRows && !RowCanAcceptDrop(inDragRef, newDropRow)) {
			mCanAcceptCurrentDrag = false;
			newDropRow = LArray::index_Bad;
		}
	}
	
	// if things have changed from last time, unhilite the last drop feedback and redraw
	// the new drop feedback.
	if (newDropRow != mDropRow || newIsDropBetweenRows != mIsDropBetweenRows)
	{				
		TableIndexT oldDropRow = mDropRow;
		mDropRow = LArray::index_Bad; // so that we'll unhilite
		HiliteDropRow(oldDropRow, mIsDropBetweenRows);
		mIsDropBetweenRows = newIsDropBetweenRows;
		
		if (newDropRow != LArray::index_Bad && mAllowDropAfterLastRow)
		{
			const Uint16 kLingerTickCount = 50;		// seems like a good time
			
			mDropRow = newDropRow; // so that we'll hilite
			HiliteDropRow(newDropRow, mIsDropBetweenRows);
			// Set a timer so that if the user lingers over this cell for long enough,
			// we'll auto-expand the container.
			mTimeToExpandForDrop = kLingerTickCount + ::TickCount();
		}
		else
		{
			mDropRow = LArray::index_Bad;
			mTimeToExpandForDrop = ULONG_MAX;
		}
	}
	else if (mAllowAutoExpand
		&& mDropRow != LArray::index_Bad 			// it's (still) a container we can drop on
		&& !mIsDropBetweenRows) 					// and we're (still) right on it
	{
		if (::TickCount() >= mTimeToExpandForDrop)
		{
			// the user's holding the mouse down on this row, & we just noticed
			STableCell cell(mDropRow, GetHiliteColumn());
			Boolean expanded;
			if (RowIsContainer(mDropRow, &expanded) && !expanded)
			{
				SetCellExpansion(cell, true);
				// The revealed rows should be there now, just draw them.
				// We have to draw immediately, of course, because we're not doing
				// event processing while the mouse is down.
				RefreshRowRange(mDropRow, mRows);
				UpdatePort();
				HiliteDropRow ( mDropRow, false );		// redraw updated drop feedback 
			}
			mTimeToExpandForDrop = ULONG_MAX;
		}
	}
	if (! mAllowDropAfterLastRow)
		mCanAcceptCurrentDrag = (mDropRow != LArray::index_Bad);
	
} // CStandardFlexTable::InsideDropArea

//----------------------------------------------------------------------------------------
void CStandardFlexTable::HiliteDropRow(TableIndexT	inRow, Boolean inDrawBarAbove)
//----------------------------------------------------------------------------------------
{
	if (inRow == LArray::index_Bad)
		return;

	STableCell cell(inRow, 1);
	if (mIsInternalDrop && CellIsSelected(cell))
		return; // Don't show hiliting feedback for a drag when over the selection.

	TableIndexT dropHiliteColumn = GetHiliteColumn();
	if ( dropHiliteColumn != LArray::index_Bad )
		cell.col = dropHiliteColumn;
	else
		cell.col = 1;
	Rect r;		
	GetLocalCellRect(cell, r);
	if (inDrawBarAbove || dropHiliteColumn == LArray::index_Bad)
	{
		r.left 	= 0;
		r.right	= mFrameSize.width;
		if (inDrawBarAbove)
		{
#if 1
			// here's how you draw a bar above.
			r.bottom = r.top + 1;
			r.top -= 1;
#else
			// here's how you underline below.
			r.top = r.bottom - 1;
			r.bottom += 1;
#endif
		}
		DoHiliteRect(r);
	}
	else
	{
		DrawIcons(cell, r);
		StRegion hiliteRgn;
		GetRowHiliteRgn(inRow, hiliteRgn);
		::InvertRgn(hiliteRgn);
	}
} // CStandardFlexTable::HiliteDropRow

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DrawCellContents(
	const STableCell& /*inCell*/,
	const Rect& /*inLocalRect*/)
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DrawCell(
	const STableCell& inCell,
	const Rect& inLocalRect)
//----------------------------------------------------------------------------------------
{
	// Compute the update rgn's intersection with the cell
	// bounds and bail if they're disjoint.
	RgnHandle updateRgn = GetLocalUpdateRgn();
	if (!updateRgn)
		return;
	Rect updateRect = (**updateRgn).rgnBBox;
	::DisposeRgn(updateRgn);
	
	Rect intersection;
	if (!::SectRect(&updateRect, &inLocalRect, &intersection))
		return;
		
	// Save clip, and clip to the cell/updateRgn intersection
	StClipRgnState savedClip(intersection);

	Rect insetRect = inLocalRect;
	insetRect.right--;
	EraseCellBackground ( inCell, inLocalRect );

	// If we are inline-editing, don't redraw the cell that is being edited. However, there
	// is a problem where if a refresh occurs before the inline becomes visible, the icon of
	// the row being edited never gets drawn (because we don't call DrawCellContents(), duh!).
	// If that is the case, just draw the icon. The best fix is to not have that redraw happen,
	// but this can tide us over until we can get all the extra redraws out (just in case we
	// have to ship before that happens). I guess making it stink would get those extra redraw
	// bugs fixed faster, but that is another debate for another time.... 
	ApplyTextStyle(inCell.row);
	if ( mRowBeingEdited != inCell.row )
		DrawCellContents(inCell, insetRect);
	else {
		if ( inCell.col == GetHiliteColumn() )
			DrawIcons ( inCell, inLocalRect );
	}
	
} // CStandardFlexTable::DrawCell


//----------------------------------------------------------------------------------------
void CStandardFlexTable::EraseCellBackground(
	const STableCell& inCell,
	const Rect& inLocalRect)
// Erase the background of the cell. Override to draw different colors or do things like
// _not_ draw when you have, for instance, a background image.
//----------------------------------------------------------------------------------------
{
	StColorState penState;
	RGBColor white = { 0xFFFF, 0xFFFF, 0xFFFF };
	::RGBBackColor(&white);
	
	// Inset the rectangle on the right for better separation
	// (long text looks bad when truncated).
	Rect localRect = inLocalRect;
	Rect insetRect = inLocalRect;
	insetRect.right--;
	// The rightmost column overlaps the scrollbar.  We must not draw on the overlap.
	if (inCell.col == mTableHeader->CountVisibleColumns())
		localRect = insetRect;
	::EraseRect(&inLocalRect);

} // CStandardFlexTable::EraseCellBackground


//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::GetRowDragRgn(TableIndexT inRow, RgnHandle ioHiliteRgn) const
// The drag region is the hilite region plus the icon
//----------------------------------------------------------------------------------------
{
	::SetEmptyRgn(ioHiliteRgn);
	Rect cellRect;
	TableIndexT col = GetHiliteColumn();
	if ( !col )
		col = 1;
	STableCell cell(inRow, col);
	if (!GetLocalCellRect(cell, cellRect))
		return false;
	ResIDT iconID = GetIconID(inRow);
	if (iconID)
	{
		GetIconRect(cell, cellRect, cellRect);
		::IconIDToRgn(ioHiliteRgn, &cellRect, atNone, iconID);
	}
	StRegion cellRgn;
	GetRowHiliteRgn(inRow, cellRgn);
	::UnionRgn(ioHiliteRgn, cellRgn, ioHiliteRgn);
	return true;
} // CStandardFlexTable::GetRowHiliteRgn

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::GetRowHiliteRgn(TableIndexT inRow, RgnHandle ioHiliteRgn) const
//----------------------------------------------------------------------------------------
{
	::SetEmptyRgn(ioHiliteRgn);
	Rect cellRect;
	if (!GetHiliteTextRect(inRow, false, cellRect))
		return false;
	::RectRgn(ioHiliteRgn, &cellRect);
	return true;
} // CStandardFlexTable::GetRowHiliteRgn

//----------------------------------------------------------------------------------------
void CStandardFlexTable::GetHiliteRgn(RgnHandle	ioHiliteRgn)
//----------------------------------------------------------------------------------------
{
	::SetEmptyRgn(ioHiliteRgn);
	Rect visRect;
	GetRevealedRect(visRect);			// Check if Table is revealed
	if (!::EmptyRect(&visRect))
	{
		PortToLocalPoint(topLeft(visRect));
		PortToLocalPoint(botRight(visRect));
		
		STableCell topLeftCell, botRightCell;
		FetchIntersectingCells(visRect, topLeftCell, botRightCell);
		
		StRegion cellRgn;
		STableCell cell(1, GetHiliteColumn());				// Loop thru all cells
		for (cell.row = topLeftCell.row; cell.row <= botRightCell.row; cell.row++)
		{
			if (CellIsSelected(cell))
			{
				if (GetRowHiliteRgn(cell.row, cellRgn))
					::UnionRgn(ioHiliteRgn, cellRgn, ioHiliteRgn);
			}
		}
	}
} // CStandardFlexTable::GetHiliteRgn

//----------------------------------------------------------------------------------------
void CStandardFlexTable::EnterDropArea(
	DragReference		inDragRef,
	Boolean				inDragHasLeftSender)
// CStandardFlexTable overrides the LDropArea base method, because it assumes a drop-on-row
// scenario
//----------------------------------------------------------------------------------------
{
	LDragAndDrop::EnterDropArea(inDragRef,inDragHasLeftSender);
	
} // CStandardFlexTable::EnterDropArea

//----------------------------------------------------------------------------------------
void CStandardFlexTable::LeaveDropArea(DragReference inDragRef)
// CStandardFlexTable overrides the LDropArea base method, because it assumes a drop-on-row
// scenario
//----------------------------------------------------------------------------------------
{
	FocusDraw();
	if (mDropRow)
	{
		TableIndexT oldDropRow = mDropRow;
		mDropRow = LArray::index_Bad; // so that we'll unhilite
		HiliteDropRow(oldDropRow, mIsDropBetweenRows);	
	}	
	
//	HiliteSelection( IsActive(), true); // Finder does not do this.
	mIsDropBetweenRows = false;
	mIsInternalDrop = false;
	mTimeToExpandForDrop = ULONG_MAX;
	
	LDragAndDrop::LeaveDropArea(inDragRef);
	
} // CStandardFlexTable::LeaveDropArea

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::PointInDropArea(Point inGlobalPt)
// Overridden to return true just above and below the view, to allow autoscroll
// (override from LDragAndDrop)
//----------------------------------------------------------------------------------------
{
#define kSlopPixels 20
	if (!IsEnabled())
		return false;
	Point portPoint = inGlobalPt;
	GlobalToPortPoint(portPoint);
	return ( (portPoint.h >= mFrameLocation.h)  &&
			 (portPoint.h < mFrameLocation.h + mFrameSize.width)  &&
			 (portPoint.v >= mFrameLocation.v - kSlopPixels)  &&
			 (portPoint.v < mFrameLocation.v + mFrameSize.height + kSlopPixels) );
} // CStandardFlexTable::PointInDropArea

//----------------------------------------------------------------------------------------
void CStandardFlexTable::StartAutoScroll()
//----------------------------------------------------------------------------------------
{
	// Turn off any tooltips
	ExecuteAttachments(msg_HideTooltip, nil);

#if 0
	// This will fix bug #123615, but perhaps it redraws too often.
	// If there is a pending update, as from a tooltip that was just hidden, then the
	// view won't refresh properly after we change
	// the image coordinates.  We probably need to refresh the whole frame.  Oh well...
	StRegion updateRegion;
	GetWindowUpdateRgn(GetMacPort(), updateRegion);
	if (!EmptyRgn(updateRegion))
		Refresh();
#endif // 0

	// Notify the CTargetFramer (if any) to erase the frame hilite
	if (IsTarget())
		ExecuteAttachments(CTargetFramer::msg_ResigningTarget, this);
		// invalidates the border, and stops the refresh from drawing a new one

	// If we are about to autoscroll and we are calling scrollbits, unhilite
	// the drop row as well
	TableIndexT currentDropRow = mDropRow;
	mDropRow = -mDropRow; // So that we'll unhilite (Why it's SInt32 - use that sign bit!)
	HiliteDropRow(currentDropRow, mIsDropBetweenRows);
} // CStandardFlexTable::StartAutoScroll

//----------------------------------------------------------------------------------------
void CStandardFlexTable::EndAutoScroll()
//----------------------------------------------------------------------------------------
{
	// Turn hiliting back on
	mDropRow = -mDropRow; // so that we'll hilite
	HiliteDropRow(mDropRow, mIsDropBetweenRows);
	// Notify the CTargetFramer to draw the border now.
	if (IsTarget())
		ExecuteAttachments(CTargetFramer::msg_BecomingTarget, this); // invert
} // CStandardFlexTable::EndAutoScroll

//----------------------------------------------------------------------------------------
void CStandardFlexTable::ScrollImageBy(
	Int32				inLeftDelta,
	Int32				inTopDelta,
	Boolean				inRefresh)
// Called by ScrollPinnedImageBy, ScrollImageTo, etc in PP.  This is a bottleneck function,
// so a good place to fix these visual problems when scrolling.
//----------------------------------------------------------------------------------------
{	
	// Turds will be left if we call scrollbits and then refresh in the inherited
	// ScrollImageBy().
	if (inRefresh)
		StartAutoScroll();
	Inherited::ScrollImageBy(inLeftDelta, inTopDelta, inRefresh);
	if (inRefresh && FocusDraw())
		EndAutoScroll();
} // CStandardFlexTable::ScrollImageBy

//----------------------------------------------------------------------------------------
OSErr CStandardFlexTable::DragSelection(
	const STableCell& 		inCell, 
	const SMouseDownEvent&	inMouseDown	)
//----------------------------------------------------------------------------------------
{
#pragma unused(inCell)

	if (!FocusDraw())
		return userCanceledErr;

	DragReference dragRef = 0;
	OSErr trackResult = noErr;	
	try
	{
		StRegion dragRgn;
		OSErr err = ::NewDrag(&dragRef);
		ThrowIfOSErr_(err);	
		AddSelectionToDrag(dragRef, dragRgn);	
		if ((inMouseDown.macEvent.modifiers & optionKey) != 0)
		{
			CursHandle copyDragCursor = ::GetCursor(curs_CopyDrag); // finder's copy-drag cursor
			if (copyDragCursor != nil)
				::SetCursor(*copyDragCursor);
		}
		trackResult = ::TrackDrag(dragRef, &(inMouseDown.macEvent), dragRgn);
	}
	catch( ... )
	{
	}
	
	if (dragRef != 0)
		::DisposeDrag(dragRef);
	return trackResult;
} // CStandardFlexTable::DragSelection

//----------------------------------------------------------------------------------------
OSErr CStandardFlexTable::DragRow(
	TableIndexT				inRow, 
	const SMouseDownEvent&	inMouseDown	)
//----------------------------------------------------------------------------------------
{
	if (!FocusDraw())
		return userCanceledErr;
	
	DragReference dragRef = 0;
	OSErr trackResult = noErr;
	try
	{
		StRegion dragRgn;
		OSErr err = ::NewDrag(&dragRef);
		ThrowIfOSErr_(err);	
		AddRowToDrag(inRow, dragRef, dragRgn);	
		if ((inMouseDown.macEvent.modifiers & optionKey) != 0)
		{
			CursHandle copyDragCursor = ::GetCursor(curs_CopyDrag); // finder's copy-drag cursor
			if (copyDragCursor != nil)
				::SetCursor(*copyDragCursor);
		}
		trackResult = ::TrackDrag(dragRef, &(inMouseDown.macEvent), dragRgn);
	}
	catch(OSErr err)
	{
		trackResult = err;
	}
	catch( ... )
	{
	}
	
	if (dragRef != 0)
		::DisposeDrag(dragRef);
	return trackResult;
} // CStandardFlexTable::DragRow

//----------------------------------------------------------------------------------------
void CStandardFlexTable::AddSelectionToDrag(DragReference inDragRef, RgnHandle inDragRgn)
//----------------------------------------------------------------------------------------
{
	StRegion	tempRgn;
			
	::SetEmptyRgn(inDragRgn);
	
	STableCell cell(0, 1);
	while (GetNextSelectedRow(cell.row))
	{
		AddRowDataToDrag(cell.row, inDragRef);
		GetRowDragRgn(cell.row, tempRgn);
		::UnionRgn(tempRgn, inDragRgn, inDragRgn);
	}
	::CopyRgn(inDragRgn, tempRgn);	
	::InsetRgn(tempRgn, 1, 1);
	::DiffRgn(inDragRgn, tempRgn, inDragRgn);

	// Convert the region to global coordinates
	Point p;
	p.h = p.v = 0;
	::LocalToGlobal(&p);
	::OffsetRgn(inDragRgn, p.h, p.v);
} // CStandardFlexTable::AddSelectionToDrag

//----------------------------------------------------------------------------------------
void CStandardFlexTable::AddRowToDrag(
	TableIndexT inRow,
	DragReference inDragRef,
	RgnHandle inDragRgn)
//----------------------------------------------------------------------------------------
{		
	AddRowDataToDrag(inRow, inDragRef);
	GetRowDragRgn(inRow, inDragRgn);
	
	StRegion	tempRgn;
	::CopyRgn(inDragRgn, tempRgn);	
	::InsetRgn(tempRgn, 1, 1);
	::DiffRgn(inDragRgn, tempRgn, inDragRgn);

	// Convert the region to global coordinates
	Point p;
	p.h = p.v = 0;
	::LocalToGlobal(&p);
	::OffsetRgn(inDragRgn, p.h, p.v);
} // CStandardFlexTable::AddRowToDrag 

//----------------------------------------------------------------------------------------
void CStandardFlexTable::ChangeSort(const LTableHeader::SortChange* /*inSortChange*/)
//----------------------------------------------------------------------------------------
{
} // CStandardFlexTable::ChangeSort

//----------------------------------------------------------------------------------------
void CStandardFlexTable::GetDropFlagRect(
	const Rect&	inLocalCellRect,
	Rect& outFlagRect	) const
//----------------------------------------------------------------------------------------
{
	UInt32	vDiff = (inLocalCellRect.bottom - inLocalCellRect.top - 12) >> 1;
	
	outFlagRect.top 	= inLocalCellRect.top + vDiff;
	outFlagRect.bottom = outFlagRect.top + 12;
	
	outFlagRect.left 	= inLocalCellRect.left + 2; // was 3
	outFlagRect.right 	= outFlagRect.left + 16;	
} // CStandardFlexTable::GetDropFlagRect

//----------------------------------------------------------------------------------------
TableIndexT CStandardFlexTable::GetSelectedRowCount() const
//----------------------------------------------------------------------------------------
{
	return ((LTableRowSelector*)mTableSelector)->GetSelectedRowCount();
		// safe cast. 
} // CStandardFlexTable::GetSelectedRowCount

//----------------------------------------------------------------------------------------
void CStandardFlexTable::SetNotifyOnSelectionChange(Boolean inDoNotify)
//----------------------------------------------------------------------------------------
{
	if (inDoNotify)
		StartBroadcasting();
	else
		StopBroadcasting();
} // CStandardFlexTable::SetNotifyOnSelectionChange

//-----------------------------------
Boolean CStandardFlexTable::GetNotifyOnSelectionChange()
//-----------------------------------
{
	return IsBroadcasting();
	
} // CStandardFlexTable::GetNotifyOnSelectionChange

//----------------------------------------------------------------------------------------
const TableIndexT* CStandardFlexTable::GetUpdatedSelectionList(TableIndexT& outSelectionSize)
							// Returns a ONE-BASED list of TableIndexT, as it should.
							// The result is const because this is not a copy, it is
							// the actual list cached in this object.  You probably want
							// to use CMailFlexTable::GetSelection() for message stuff.
//----------------------------------------------------------------------------------------
{
	outSelectionSize = GetSelectedRowCount();
	// if we've got a selection list, it's assumed up to date,
	// so return it.  When the selection changes, we just get
	// rid of it (in SelectionChanged()) and set mSelectionList to NULL
	if (mSelectionList)
		return mSelectionList;
	// If there is a selection, allocate the list
	if (outSelectionSize > 0) 
	{
		mSelectionList = new TableIndexT[outSelectionSize];
		if (!mSelectionList)
			outSelectionSize = 0;
	}
	if (mSelectionList)
	{
		TableIndexT selectedRow = 0;
		TableIndexT *item = mSelectionList;
		for (int i = 0; i < outSelectionSize; i++)
		{
			Boolean ok = GetNextSelectedRow(selectedRow);
			Assert_(ok);
			if (!ok) break;
			*item++ = selectedRow;
		}
	}
	return mSelectionList;
} // CStandardFlexTable::GetUpdatedSelectionList

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::IsColumnVisible(PaneIDT inID)
//----------------------------------------------------------------------------------------
{
	return mTableHeader->IsColumnVisible(inID);
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::SelectionChanged()
// invalidate any cached selection list
//----------------------------------------------------------------------------------------
{
 	delete [] mSelectionList;
	mSelectionList = NULL;
	BroadcastMessage(msg_SelectionChanged, this);
} // CStandardFlexTable::SelectionChanged

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DrawTextString(
	const char*		inText, 
	const TextDrawingStuff & inTextInfo,
	SInt16			inMargin,
	const Rect&		inBounds,
	SInt16			inJustification,
	Boolean			inDoTruncate,
	TruncCode		inTruncWhere)
// correctly handles UTF8 encoding
//----------------------------------------------------------------------------------------
{
	Rect r = inBounds;
	
	r.left  += inMargin;
	r.right -= inMargin;
	
	if (inTextInfo.encoding == CStandardFlexTable::TextDrawingStuff::eDefault)
		UGraphicGizmos::PlaceTextInRect(inText,
											strlen(inText),
											r,
											inJustification,
											teCenter,
											&inTextInfo.mTextFontInfo,
											inDoTruncate,
											inTruncWhere );								
	else
		UGraphicGizmos::PlaceUTF8TextInRect(inText,
											strlen(inText),
											r,
											inJustification,
											teCenter,
											&inTextInfo.mTextFontInfo,
											inDoTruncate,
											inTruncWhere );								

} // CStandardFlexTable::DrawTextString

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DrawIconFamily(
 	ResIDT				inIconID,
	SInt16				inIconWidth,
	SInt16				inIconHeight,
	IconTransformType	inTransform,
	const Rect&			inBounds)
//----------------------------------------------------------------------------------------
{
	OSErr	err;
	Rect	iconRect = inBounds;
	
	iconRect.right 	= iconRect.left + inIconWidth;
	iconRect.bottom	= iconRect.top  + inIconHeight;
	
	UGraphicGizmos::CenterRectOnRect(iconRect, inBounds);	
	err = ::PlotIconID(&iconRect, atNone, inTransform, inIconID);
} // CStandardFlexTable::DrawIconFamily

//----------------------------------------------------------------------------------------
SInt16 CStandardFlexTable::DrawIcons(
	const STableCell& inCell,
	const Rect& inLocalRect) const
//----------------------------------------------------------------------------------------
{
	Rect iconRect;
	GetDropFlagRect(inLocalRect, iconRect);
	// Draw the drop flag if the folder has subfolders
	// UI issue: the following test makes us differ from the Finder: there, all
	// folders have dropflags, whether they have subfolders or not.  This diff may confuse
	// users... if so, just comment out the "if" line. - jrm
	Boolean expanded;
	if (CellHasDropFlag(inCell, expanded))
	{
		StColorState state; // LDropFlag::Draw has a bug (clobbers the color).
		LDropFlag::Draw(iconRect, expanded);
	}

	// Work out the correct transform type, and then call our virtual method so that
	// derived classes can draw the icons they want.
	IconTransformType transformType = kTransformNone;
	if (mClickTimer)
	{
		// A fancy double-click is in progress.
		// Only the fake selection should be hilited
		if (inCell.row == mClickTimer->GetClickedRow())
			transformType = kTransformSelected;
	}
	else
	{
		if (inCell.row == mDropRow)
		{
			// Drop-target always hilited
			transformType = kTransformSelected;
		}
		else if (IsActive() && CellIsSelected(inCell))
		{
			// Only the real selection is hilited (and only in the active pane)
			transformType = kTransformSelected;
		}
	}
	GetIconRect(inCell, inLocalRect, iconRect);
	DrawIconsSelf(inCell, transformType, iconRect);
	return iconRect.right;
} // CStandardFlexTable::DrawIcons

//----------------------------------------------------------------------------------------
void CStandardFlexTable::DrawIconsSelf(
	const STableCell& inCell,
	IconTransformType inTransformType,
	const Rect& inIconRect) const
//----------------------------------------------------------------------------------------
{
	ResIDT iconID = GetIconID(inCell.row);
	if (iconID)
		DrawIconFamily(iconID, 16, 16, inTransformType, inIconRect);
} // CStandardFlexTable::DrawIconsSelf

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::IsSortedBackwards() const
//----------------------------------------------------------------------------------------
{
	return mTableHeader->IsSortedBackwards();
}
						
//----------------------------------------------------------------------------------------
PaneIDT CStandardFlexTable::GetSortedColumn() const
//----------------------------------------------------------------------------------------
{
	PaneIDT result;
	mTableHeader->GetSortedColumn(result);
	return result;
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::SetRightmostVisibleColumn(UInt16 inLastDesiredColumn)
//----------------------------------------------------------------------------------------
{
	mTableHeader->SetRightmostVisibleColumn(inLastDesiredColumn);
} // CStandardFlexTable::SetRightmostVisibleColumn

//----------------------------------------------------------------------------------------
void CStandardFlexTable::CalcToolTipText(
	const STableCell& /*inCell*/,
	StringPtr outText,
	TextDrawingStuff& outStuff,
	Boolean& outTruncationOnly)
//----------------------------------------------------------------------------------------
{
	outText[0] = 0;
//	outStuff.encoding = TextDrawingStuff::eDefault;
//	outStuff.style = normal;
//	outStuff.mode = srcOr;
} // CStandardFlexTable::CalcTipText

//----------------------------------------------------------------------------------------
Boolean CStandardFlexTable::TableSupportsNaturalOrderSort() const
// Describes whether or not the table currently supports dropping things in the middle
// (natural order). This will not be the case, for example, when the table is sorted.
//----------------------------------------------------------------------------------------
{
	return true;
} // CStandardFlexTable::TableSupportsNaturalOrderSort


//----------------------------------------------------------------------------------------
void CStandardFlexTable::ClickCountToOpen ( Uint16 inNewCount )
// Set the number of clicks required to open a row. Validates incoming count.
//----------------------------------------------------------------------------------------
{
	if ( inNewCount <= 0 )
		inNewCount = 1;
	
	mClickCountToOpen = inNewCount;

} // CStandardFlexTable::TableSupportsNaturalOrderSort


//----------------------------------------------------------------------------------------
Boolean
CStandardFlexTable :: RowIsContainer( const TableIndexT & inRow, Boolean* outIsExpanded ) const
// Default implementation, just returns false.
//----------------------------------------------------------------------------------------
{
	Assert_(outIsExpanded != NULL);
	if ( outIsExpanded )
		*outIsExpanded = false;
		
	return false;

} // RowIsContainer


#pragma mark -
#if defined(QAP_BUILD)
#include <LScrollerView.h>

//----------------------------------------------------------------------------------------
void CStandardFlexTable::QapGetListInfo(PQAPLISTINFO pInfo)
//----------------------------------------------------------------------------------------
{
	if (pInfo == nil)
		return;

	// get table size
	TableIndexT outRows, outCols;
	GetTableSize(outRows, outCols);
	
	// get range of cells that are within the frame
	Rect	frame;
	CalcLocalFrameRect(frame);
	STableCell	topLeftCell, botRightCell;
	FetchIntersectingCells(frame, topLeftCell, botRightCell);

	// fetch vertical scrollbar Macintosh control
	ControlHandle macVScroll = NULL;
	LScrollerView *myScroller = dynamic_cast<LScrollerView *>(GetSuperView());
	if (myScroller != NULL)
	{
#if 0
// there is no public access to the scrollbars in PowerPlant's LScrollerView (pinkerton)
		if (myScroller->GetVScrollbar() != NULL)
			macVScroll = myScroller->GetVScrollbar()->GetMacControl();
#endif
	}

	pInfo->itemCount	= (short)outRows;
	pInfo->topIndex 	= topLeftCell.row - 1;	// must start at 0 even though in QAP-script, Select("#1") selects the first line
//	pInfo->topOffset	= 0;
	pInfo->itemHeight 	= GetRowHeight(0);
	pInfo->visibleCount = botRightCell.row - topLeftCell.row + 1;
	pInfo->vScroll 		= macVScroll;
	pInfo->isMultiSel 	= true;
	pInfo->isExtendSel 	= true;
	pInfo->hasText 		= true;
}


//----------------------------------------------------------------------------------------
Ptr CStandardFlexTable::QapAddCellToBuf(Ptr pBuf, Ptr pLimit, const STableCell& sTblCell)
//----------------------------------------------------------------------------------------
{
	char 	str[cMaxMailFolderNameLength + 1];
	short	len = 0;

	GetQapRowText(sTblCell.row, str, sizeof(str));
	len = strlen(str) + 1;

	if (pBuf + sizeof(short) + len >= pLimit)
		return NULL;

	*(unsigned short *)pBuf = sTblCell.row - 1;
	if (CellIsSelected(sTblCell))
		*(unsigned short *)pBuf |= 0x8000;

	pBuf += sizeof(short);

	strcpy(pBuf, str);
	pBuf += len;

	return pBuf;
}

//----------------------------------------------------------------------------------------
void CStandardFlexTable::GetQapRowText(
	TableIndexT			/*inRow*/,
	char*				outText,
	UInt16				inMaxBufferLength) const
// Calculate the text and (if ioRect is not passed in as null) a rectangle that fits it.
// Return result indicates if any of the text is visible in the cell
//----------------------------------------------------------------------------------------
{
	if (outText && inMaxBufferLength > 0)
		*outText = '\0';
} // CStandardFlexTable::GetQapRowText

#endif //QAP_BUILD
