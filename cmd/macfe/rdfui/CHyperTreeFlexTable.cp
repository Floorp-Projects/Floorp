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

//
// Mike Pinkerton, Netscape Communications
//
// Subclass of CStandardFlexTable to handle working with XP RDF Hyper Trees
//

#include "CHyperTreeFlexTable.h"

// PowerPlant
#include <LTableArrayStorage.h>
#include <LDropFlag.h>

#include "CHyperTreeHeader.h"
#include "URDFUtilities.h"
#include "uapp.h"
#include "CURLDragHelper.h"
#include "CIconTextDragTask.h"
#include "CNetscapeWindow.h"
#include "resgui.h"
#include "macutil.h"
#include "ufilemgr.h"
#include "CInlineEditField.h"
#include "CContextMenuAttachment.h"

#include <vector.h>
#include <algorithm>


const ResIDT cFolderIconID = -3999;		// use the OS Finder folder icon
const ResIDT cItemIconID = 15313;
const ResIDT cFileIconID = 15408;


#pragma mark -- class CHyperTreeFlexTable --



// ------------------------------------------------------------
// Construction, Destruction
// ------------------------------------------------------------

CHyperTreeFlexTable::CHyperTreeFlexTable(LStream* inStream)
	:	CStandardFlexTable(inStream),
		mHTView(NULL),
		mHTNotificationData(NULL),
		mViewBeforeDrag(NULL)
{
	mSendDataUPP = NewDragSendDataProc(LDropArea::HandleDragSendData);
	ThrowIfNil_(mSendDataUPP);

}


CHyperTreeFlexTable::~CHyperTreeFlexTable()
{
}




// ------------------------------------------------------------
// CStandardFlexTable overrides
// ------------------------------------------------------------
ResIDT CHyperTreeFlexTable::GetIconID(TableIndexT inRow) const
{
	HT_Resource node = HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inRow) );
	if (node) {
		if ( HT_IsContainer(node) )
			return cFolderIconID;
		else {
			char* url = HT_GetNodeURL(node);
			if ( url && *url == 'f' )			// is it a file url?
				return cFileIconID;
			else
				return cItemIconID;			
		}
	}
	return cItemIconID;
}


//
// SetUpTableHelpers
//
// Call the inherited version to do most of the work but replace the
// selector with our own version to track the selection in the HT_API
//
void
CHyperTreeFlexTable :: SetUpTableHelpers()
{
	CStandardFlexTable::SetUpTableHelpers();
	
	// replace selector....
	SetTableSelector( new CHyperTreeSelector(this) );

} // SetupTableHelpers


//
// GetHiliteTextRect
//
// Find out what rectangle to draw selected in the currently selected row. This is
// based on the item title.
//
Boolean
CHyperTreeFlexTable::GetHiliteTextRect ( TableIndexT inRow, Rect& outRect) const
{
	STableCell cell(inRow, GetHiliteColumn());
	if (!GetLocalCellRect(cell, outRect))
		return false;
	Rect iconRect;
	GetIconRect(cell, outRect, iconRect);
	outRect.left = iconRect.right;

	// Get cell data for first column
	char buffer[1000];
	GetHiliteText(inRow, buffer, 1000, &outRect);

	return true;
	
} // GetHiliteTextRect


//
// GetMainRowText
//
// Returns the text of the title of the item.
//
void 
CHyperTreeFlexTable :: GetMainRowText( TableIndexT inRow, char* outText, UInt16 inMaxBufferLength) const
{
	if (!outText || inMaxBufferLength == 0)
		return;

	void* data;
	CHyperTreeHeader* header = dynamic_cast<CHyperTreeHeader*>(mTableHeader);
	Assert_(header != NULL);
	CHyperTreeHeader::ColumnInfo info = header->GetColumnInfo(FindTitleColumnID()-1);	// GCI() is 0-based
	HT_Resource node = HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inRow) );
	if ( node && HT_GetNodeData(node, info.token, info.tokenType, &data) ) {
		char* title = static_cast<char*>(data);
		strcpy ( outText, title );
	}
	else
		*outText = NULL;
	
} // GetMainRowText


//
// FindTitleColumnID
//
// Returns the index of the column that contains the item title and icon. This will search out the
// correct column by looking at the column headers.
//
// NOTE: THERE IS CURRENTLY NO BACKEND SUPPORT FOR REORDERING COLUMNS, SO WE MAKE AN EXPLICIT
// ASSUMPTION THAT THE FIRST COLUMN IS THE TITLE COLUMN. AT LEAST THIS SHOULD BE THE ONLY
// ROUTINE THAT HAS TO CHANGE WHEN THAT ASSUMPTION CHANGES.
//
Uint32
CHyperTreeFlexTable :: FindTitleColumnID ( ) const
{
	return 1;	// for now....

} // FindTitleColumnID


//
// CellInitiatesDrag
//
// Determines if a cell is allowed to start a drag. We disallow this for any cell except for the
// cell with the title and icon.
//
Boolean
CHyperTreeFlexTable :: CellInitiatesDrag ( const STableCell& inCell ) const
{
	return inCell.col == FindTitleColumnID() ? true : false;
	
} // CellInitiatesDrag


//
// CellSelects
//
// Determines if a cell is allowed to select the row. We disallow this for any cell except for the
// cell with the title and icon.
//
Boolean
CHyperTreeFlexTable :: CellSelects ( const STableCell& inCell ) const
{
	return inCell.col == FindTitleColumnID() ? true : false;
	
} // CellSelects


//
// DoHiliteRgn
//
// Override to not use the hilite color, since that would be confusing with in-place editing.
// This is called when the user clicks on a row in the table to hilite that particular row.
// The routine below is called when updating the window.
//
void 
CHyperTreeFlexTable :: DoHiliteRgn ( RgnHandle inHiliteRgn ) const
{	
	::InvertRgn ( inHiliteRgn );
		
} // DoHiliteRgn


//
// DoHiliteRect
//
// Override not to use hilite color, since it doesn't draw very well over the gray background.
//
void
CHyperTreeFlexTable :: DoHiliteRect ( const Rect & inHiliteRect ) const
{
	::InvertRect ( &inHiliteRect );

} // DoHiliteRect


//

// HiliteSelection
//
// Overload the LTableView class' version of this routine to hilite the selected items
// without setting the hilite bit before drawing. This is called when the window is updating
// to redraw the selected items.
//
void
CHyperTreeFlexTable :: HiliteSelection( Boolean	inActively, Boolean	/*inHilite*/)
{
	if (FocusExposed()) {
		RgnHandle	hiliteRgn = ::NewRgn();
		GetHiliteRgn(hiliteRgn);
        StColorPenState saveColorPen;   // Preserve color & pen state
		StColorPenState::Normalize();
		
		if (inActively) {
			::InvertRgn(hiliteRgn);
			
		} else {
			::PenMode(srcXor);
			::FrameRgn(hiliteRgn);
		}

		::DisposeRgn(hiliteRgn);
	}
	
} // HiliteSelection


//
// SyncSelectionWithHT
//
// The selection has somehow changed by the backend's decree. Make sure that we are in
// sync with that.
//
void
CHyperTreeFlexTable :: SyncSelectionWithHT ( )
{
	CHyperTreeSelector* sel = dynamic_cast<CHyperTreeSelector*>(GetTableSelector());
	Assert_( sel != NULL);
	
	sel->SyncSelectorWithHT();
	
	SelectionChanged();
	ScrollSelectionIntoFrame();
		
} // SyncSelectionWithHT


//
// DrawCellContents
//
// Draw what goes inside each cell, naturally
//
void CHyperTreeFlexTable::DrawCellContents( const STableCell& inCell, const Rect& inLocalRect)
{
	PaneIDT	cellType = GetCellDataType(inCell);

	// Get info for column
	CHyperTreeHeader* header = dynamic_cast<CHyperTreeHeader*>(mTableHeader);
	Assert_(header != NULL);
	CHyperTreeHeader::ColumnInfo info = header->GetColumnInfo(inCell.col - 1);

	//
	// make the backdrop of the cell look like the Finder in OS8. Color/pen state is already
	// restored for us by CStandardFlexTable::DrawCell() so we don't have to worry about reseting
	// the bg color later.
	//
	{
		static const RGBColor normalColumnColor = { 0xEEEE, 0xEEEE, 0xEEEE };
		static const RGBColor sortedColumnColor = { 0xDDDD, 0xDDDD, 0xDDDD };
		RGBColor backColor;
		PaneIDT columnPane;
		
		Rect backRect = inLocalRect;
		backRect.bottom--;				// leave a one pixel line on the bottom as separator
		backRect.right++;				// cover up vertical dividing line on right side
		backColor = inCell.col == header->GetSortedColumn(columnPane) ? sortedColumnColor : normalColumnColor;
		::RGBBackColor(&backColor);
		::EraseRect(&backRect);
	}
	
	
	// Get cell data
	HT_Resource node = HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inCell.row) );
	if (node)
	{
		if ( HT_IsSeparator(node) ) {
			StColorPenState savedPenState;
			RGBColor black = { 0, 0, 0 };
			Uint16 left = inLocalRect.left;
			
			if ( inCell.col == 1 ) {
				StTextState savedTextState;
				Rect iconRect, textRect = inLocalRect;
				
				// get level indent for text
				GetIconRect ( inCell, inLocalRect, iconRect );
				textRect.left = iconRect.left;
				left = iconRect.left;
				::MoveTo ( left, iconRect.top );
				
				//еее REPLACE WITH GetIndString
				DrawTextString("<Separator>", &mTextFontInfo, 0, textRect);
				left += ::TextWidth("<Separator>", 0, 11) + 5;
			}
			
			::RGBForeColor ( &black );
			::MoveTo ( left,
						inLocalRect.top + ((inLocalRect.bottom - inLocalRect.top) / 2) );
			::PenSize ( 2, 2 );
			::PenPat ( &qd.gray );
			::Line ( inLocalRect.right - left, 0 );
		}
		else {
			void* data;
			char* str;
			Rect localRect = inLocalRect;
			if (HT_GetNodeData(node, info.token, info.tokenType, &data) && data)
			{
				switch (info.tokenType) {
					case HT_COLUMN_STRING:
					case HT_COLUMN_DATE_STRING:
						str = static_cast<char*>(data);
						if (inCell.col == FindTitleColumnID())
						{
							localRect.left = DrawIcons(inCell, localRect);
							localRect.left += CStandardFlexTable::kDistanceFromIconToText;
						}
						break;

					case HT_COLUMN_DATE_INT:
					case HT_COLUMN_INT:
						char intStr[32];
						sprintf(intStr, "%d", (int)data);
						str = intStr;
						break;
				}
				DrawTextString(str, &mTextFontInfo, 0, localRect);
			}
		}
	}
}


Boolean	CHyperTreeFlexTable::CellHasDropFlag(const STableCell& inCell, Boolean& outIsExpanded) const
{
	// bail quickly if this cell isn't a title column
	if ( FindTitleColumnID() != inCell.col )
		return false;
		
	HT_Resource node = HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inCell.row) );
	if (node)
	{
		PRBool result = HT_IsContainer(node);
		if (result)
		{
			PRBool openState;
			HT_Error err = HT_GetOpenState(node, &openState);
			if (err == HT_NoErr)
				outIsExpanded = openState;
		}
		else
			outIsExpanded = false;
		return result;
	}
	return false;
	
} // CellHasDropFlag


UInt16 CHyperTreeFlexTable::GetNestedLevel(TableIndexT inRow) const
{
	HT_Resource node = HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inRow) );
	if (node)
	{
		// subtract one because root node as indentation of 1
		return (HT_GetItemIndentation(node) - 1);
	}
	return CStandardFlexTable::GetNestedLevel(inRow);
}


//
// SetCellExpansion
//
// User clicked on the disclosure triangle, tell HT to open/close the container. This call
// may fail if the user has password protected the folder and they get the password wrong.
// If that is the case, redraw the drop flag to its correct state.
//
void CHyperTreeFlexTable::SetCellExpansion(
	const STableCell& inCell,
	Boolean inExpand)
{
	HT_Resource node = HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inCell.row) );
	if (node)
	{
		if ( HT_SetOpenState(node, (PRBool)inExpand) != noErr )
			RefreshCell(inCell);
	}
}


// ------------------------------------------------------------
// HT Operations
// ------------------------------------------------------------


//
// OpenView
//
// opens up the given view in the navCenter (such as bookmarks). Make sure the selector for this
// table knows about new view, sync the number of rows in the table with the
// number of rows in the HT and setup the columns.
//
// This routine is called as a result of the HT notification mechanism (not directly from any
// FE events like clicks). That means that the shelf may not be open yet.
//
void
CHyperTreeFlexTable::OpenView( HT_View inHTView )
{
	// Hide the inline editor. This will send us a message that the name changed and attempt
	// to redraw the entire window. We don't want this because is causes an ugly flash as it
	// redraws the old view then draws the new view. Setting the visRgn to empty prevents
	// the redraw of the old view.
	if ( mNameEditor && mNameEditor->IsVisible() ) {
		StVisRgn saved(GetMacPort());
		mNameEditor->UpdateEdit(nil, nil, nil);
	}

	mHTView = mViewBeforeDrag = inHTView;
	CHyperTreeSelector* sel = dynamic_cast<CHyperTreeSelector*>(GetTableSelector());
	Assert_( sel != NULL);
	sel->TreeView ( inHTView );

	CHyperTreeHeader* header = dynamic_cast<CHyperTreeHeader*>(mTableHeader);
	Assert_(header != NULL);
	HT_Cursor columnCursor = HT_NewColumnCursor(inHTView);
	header->SetUpColumns(columnCursor);
	HT_DeleteColumnCursor(columnCursor);
	SynchronizeColumnsWithHeader();	
	if ( header->GetHeaderWidth() )		// don't do this if shelf collapsed, we'll do it again later
		SetRightmostVisibleColumn(1);	//еее HACK UNTIL WE GET THE COLUMN STUFF FIGURED OUT....
	
	Uint32 count = HT_GetItemListCount(inHTView);
	if (mRows != count)
	{
		if (count > mRows)
			InsertRows(count - mRows, 0, NULL, 0, false);
		else
			RemoveRows(mRows - count, 0, true);
	}
		
	SyncSelectionWithHT();
	
	// redraw the table. If the mouse is down, we're probably in the middle of a 
	// drag and drop so we want to redraw NOW (not when we get an update event after
	// the mouse has been released).
	if ( ::StillDown() ) {
		UnselectAllCells();
		FocusDraw();
		Rect localRect;
		CalcLocalFrameRect(localRect);
		localRect.left++;
		::EraseRect(&localRect);
		GetSuperView()->Draw(nil);		// redraw NOW to get the # of visible rows correct
	}
	else
		Refresh();
		
}

void CHyperTreeFlexTable::ExpandNode(HT_Resource inHTNode)
{
	Int32 index = HT_GetNodeIndex(GetHTView(), inHTNode);
	
	Uint32 count = HT_GetCountVisibleChildren(inHTNode);
	if (count > 0)
		InsertRows(count, index + 1, NULL, 0, true);
}

void CHyperTreeFlexTable::CollapseNode(HT_Resource inHTNode)
{
	Int32 index = HT_GetNodeIndex(GetHTView(), inHTNode);
	
	Uint32 count = HT_GetCountVisibleChildren(inHTNode);
	if (count > 0)
		RemoveRows(count, index + 2, true);
}

//-----------------------------------
// Commands
//-----------------------------------

//
// FindCommandStatus
//
void
CHyperTreeFlexTable :: FindCommandStatus ( CommandT inCommand, Boolean &outEnabled,
										Boolean &outUsesMark, Char16 &outMark, Str255 outName)
{
	// safety check
	if ( ! mHTView )
		return;
	
	HT_Pane thePane = HT_GetPane(mHTView);
	
	outUsesMark = false;

	if ( inCommand >= cmd_NavCenterBase && inCommand <= cmd_NavCenterCap ) {
		outEnabled = HT_IsMenuCmdEnabled(thePane, (HT_MenuCmd)(inCommand - cmd_NavCenterBase));
		return;
	}

	// process the regular menus...
	switch ( inCommand ) {
	
		//
		// handle commands that we have to share with other parts of the UI
		//
		case cmd_Cut:
			outEnabled = HT_IsMenuCmdEnabled(thePane, HT_CMD_CUT );
			break;
		case cmd_Copy:
			outEnabled = HT_IsMenuCmdEnabled(thePane, HT_CMD_COPY );
			break;
		case cmd_Paste:
			outEnabled = HT_IsMenuCmdEnabled(thePane, HT_CMD_PASTE );
			break;
		case cmd_Clear:
			outEnabled = HT_IsMenuCmdEnabled(thePane, HT_CMD_DELETE_FILE );
			break;

		default:
			LCommander::FindCommandStatus(inCommand, outEnabled, outUsesMark, outMark, outName);
			break;
	
	} // case of command
	
} // FindCommandStatus


//
// DeleteSelection
//
// Delete the selection in the given view. This is called when the user hits backspace
// in the current view, so we don't need to worry (as below) about the view switching
// during a drag and drop.
//
void
CHyperTreeFlexTable::DeleteSelection ( )
{
	HT_Pane pane = HT_GetPane(mHTView);
	if ( HT_IsMenuCmdEnabled(pane, HT_CMD_CUT) )	//еее these should be HT_CMD_CLEAR
		HT_DoMenuCmd ( pane, HT_CMD_CUT );
	
} // DeleteSelection


//
// DeleteSelectionByDragToTrash
//
// Called when the user drags some stuff in the pane into the trash can to delete
// it. We don't rely on the current view or the PowerPlant selection because it
// may have been wiped out by a temporary drag over another workspace pane on
// the way to the trash.
//
void
CHyperTreeFlexTable :: DeleteSelectionByDragToTrash ( LArray & inItems )
{
	//еее return if we're not allowed to delete these objects!!!
	
	CIconTextSuite* curr = NULL;
	LArrayIterator it ( inItems );
	while ( it.Next(&curr) ) {
		HT_Resource node = curr->GetHTNodeData();
		HT_Resource parent = HT_GetParent(node);
		Assert_( node != NULL && parent != NULL );
		HT_RemoveChild ( parent, node );
	}
	
} // DeleteSelectionByDragToTrash


//
// OpenRow
//
// Called to tell us when the user has clicked on a row in such a way that it should
// open. Whether this is by single click or double click is left up to parameters to
// the CStandardFlexTable. Regardless, open the url at this row if it is one....
//
void
CHyperTreeFlexTable :: OpenRow ( TableIndexT inRow )
{
	HT_Resource node = HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inRow) );
	if (node) {
	
		// we can ignore the click if it is a container.
		if ( !HT_IsContainer(node) && !HT_IsSeparator(node) )
			CFrontApp::DoGetURL( HT_GetNodeURL(node) );
					
	} // if valid node

} // OpenRow


//
// RowCanAcceptDrop
//
// Check if the given row can accept any of the items dropped on it. We iterate over each item
// being dropped and ask the backend if that item is ok. If any item can be dropped, the entire
// lot can be dropped.
//
Boolean 
CHyperTreeFlexTable::RowCanAcceptDrop ( DragReference inDragRef, TableIndexT inDropRow )
{
	if ( inDropRow > mRows )
		return false;
		
	HT_Resource targetNode = HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inDropRow) );
	return NodeCanAcceptDrop ( inDragRef, targetNode );
	
} // CStandardFlexTable::RowCanAcceptDrop


//
// RowCanAcceptDropBetweenAbove
//
// Check if the item can be dropped above the current node. This means we need to ask the parent
// of the node for this row if it can be dropped on.
//
Boolean 
CHyperTreeFlexTable::RowCanAcceptDropBetweenAbove( DragReference inDragRef, TableIndexT inDropRow )
{
	if ( inDropRow > mRows ) 
		return NodeCanAcceptDrop ( inDragRef, HT_TopNode(GetHTView()) );
		
	HT_Resource targetNode = HT_GetParent( HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inDropRow)) );
	return NodeCanAcceptDrop ( inDragRef, targetNode );
	
} // CStandardFlexTable::RowCanAcceptDropBetweenAbove


//
// RowIsContainer
//
// Is this row a folder or a leaf? HT will tell us!
//
Boolean		
CHyperTreeFlexTable :: RowIsContainer ( const TableIndexT & inRow ) const
{
	if ( inRow > mRows )
		return false;
		
	return HT_IsContainer ( HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(inRow)) );

} // RowIsContainer


//
// HiliteDropRow
//
// Override to handle drawing the "insertion bar" for every row to always indicate to the
// user exactly where the drop will occur. The inherited version of this class only draws
// the insertion bar when the mouse is between two cells, and I really don't like that.
//
void 
CHyperTreeFlexTable :: HiliteDropRow ( TableIndexT inRow, Boolean inDrawBarAbove )
{
	if (inRow == LArray::index_Bad)
		return;

	STableCell cell(inRow, GetHiliteColumn());
	if ( mIsInternalDrop && CellIsSelected(cell) )
		return; 		// Don't show hiliting feedback for a drag when over the selection.

	Rect insertionBar, cellRect;		
	GetLocalCellRect(cell, cellRect);
	insertionBar = cellRect;

	// when the mouse is over a cell, there are several possibilities:
	// - if the cell is a container, hilite the container and draw the insertion bar
	//   below the last visible child of the container. The left edge should be indented
	//   from the left edge of the container. 
	// - if the cell is a normal row, draw the bar directly above the row at
	//   the same horizontal indent as the row.
	// - if the cell is a container but the mouse is between rows, treat it like
	//   a normal row and draw the bar directly above at the same level and do not
	//   hilite the container.
	// - if the mouse is off the end, draw the bar directly below the last row
	//   at the same indent as the 1st row (since they should be siblings).
	// The only problem with the behavior occurs when you want to drop something
	// directly at the end of an open container. Moving the mouse to where you think
	// would do it ends up dropping the item after the parent container at the 
	// same level as the parent, not as the last item in the container. As a result,
	// the only way to add something to the end of a container is to drop it on the
	// container itself. The drag feedback illustrates that this will happen when you
	// drag on top of a container.

	const Uint32 kInsertionBarLength = 35;
	Rect iconRect;
	GetIconRect ( cell, insertionBar, iconRect );
	mDropNode = HT_GetNthItem( GetHTView(), URDFUtilities::PPRowToHTRow(cell.row) );
	if ( mDropNode && HT_IsContainer(mDropNode) && !inDrawBarAbove ) {
		
		// place the insertion bar below the last visible row at the appropriate
		// indent level for its children
		Uint32 children = HT_GetCountVisibleChildren(mDropNode);
		Uint32 rowHeight = GetRowHeight(inRow);
		insertionBar.bottom = insertionBar.top + rowHeight + 1;
		insertionBar.bottom += (children * GetRowHeight(inRow));
		insertionBar.left = iconRect.left + kIndentPerLevel;	// one indentation
		insertionBar.right	= insertionBar.left + kInsertionBarLength;
		insertionBar.top = insertionBar.bottom - 2;		
		
		// hilite the container
		DrawIcons(cell, cellRect);
		StRegion hiliteRgn;
		GetRowHiliteRgn(inRow, hiliteRgn);
		::InvertRgn(hiliteRgn);
	
	} // if drop on container
	else {
		
		// setup insertion bar rectangle. When we're dropping at the end of the list, use
		// the indent of the 1st item in the list, as they will be siblings.
		if ( inRow <= mRows ) {
			insertionBar.left = iconRect.left - 5;
			insertionBar.right	= insertionBar.left + kInsertionBarLength;
			insertionBar.bottom = insertionBar.top + 1;
			insertionBar.top -= 1;
		}
		else {
			// compute the icon rect for the 1st row
			Rect iconRect, cellRect;
			STableCell firstCell(1, GetHiliteColumn());
			GetLocalCellRect ( firstCell, cellRect );
			GetIconRect ( firstCell, cellRect, iconRect );
			
			// use the last row to compute where the bar should go vertically
			Rect lastRowRect;
			GetLocalCellRect ( STableCell(mRows, GetHiliteColumn()), lastRowRect );
			
			insertionBar.left = iconRect.left;
			insertionBar.right = insertionBar.left + kInsertionBarLength;
			insertionBar.top = lastRowRect.bottom + 1;
			insertionBar.bottom = insertionBar.top + 2;
		}
	
	} // else drop on item or between rows
	
	DoHiliteRect ( insertionBar );
	
} // HiliteDropRow


//
// ItemIsAcceptable
//
// Determine if the current item can be dropped in this pane. Check to see if the 
// pane as a whole accepts drops (history, for example, will not). The data, at this
// point, is fairly moot and will be checked in RowCanAcceptDrop*() routines.
//
Boolean
CHyperTreeFlexTable :: ItemIsAcceptable ( DragReference /*inDragRef*/, ItemReference /*inItemRef*/ )
{
	return HT_CanDropURLOn ( HT_TopNode(GetHTView()), "http://foo.com" );
	
} // ItemIsAcceptable


//
// DragSelection
//
// Overridden to use our own version of the drag task...
//
void 
CHyperTreeFlexTable :: DragSelection(
	const STableCell& 		inCell, 
	const SMouseDownEvent&	inMouseDown	)
{
	FocusDraw();
	mViewBeforeDrag = GetHTView();
	Assert_(mViewBeforeDrag != NULL);
	
	// add the members of the selection to a list we will pass to the drag task.
	LArray selection;
	STableCell cell(0, 1);
	while (GetNextSelectedRow(cell.row)) {	
		Rect bounds, iconBounds;
		GetLocalCellRect(cell, bounds);
		GetIconRect	( cell, bounds, iconBounds );
		bounds.left = iconBounds.left;
		
		HT_Resource node = HT_GetNthItem(mViewBeforeDrag, URDFUtilities::PPRowToHTRow(cell.row) );
		cstring finalCaption = CURLDragHelper::MakeIconTextValid ( HT_GetNodeName(node) );
		CIconTextSuite* suite = new CIconTextSuite( this, bounds, GetIconID(cell.row), finalCaption, node );
		selection.InsertItemsAt ( 1, LArray::index_Last, &suite );
	}
	
	
	// create the drag task
	Rect cellBoundsOfClick;
	GetLocalCellRect(inCell, cellBoundsOfClick);
	CIconTextDragTask theTask(inMouseDown.macEvent, selection, cellBoundsOfClick);
	
	// setup our special data transfer proc called upon drag completion
	OSErr theErr = ::SetDragSendProc ( theTask.GetDragReference(), mSendDataUPP, (LDropArea*) this );
	ThrowIfOSErr_(theErr);
	
	theTask.DoDrag();
	
	// There is a problem with the HT backend that if you tell it to say, delete
	// both a node and its parent, things get really screwed up when it tries to
	// double-delete the child. Iterate over each item, and for each node, if 
	// we find any children in the list, remove them. LArrayIterator should be
	// smart enough to handle deletions while iterating....
	LArrayIterator it(selection);
	CIconTextSuite* curr = NULL;
	while ( it.Next(&curr) ) {
		HT_Resource currentNode = curr->GetHTNodeData();
		CIconTextSuite* possibleChildSuite = NULL;
		LArrayIterator inner(selection);
		Uint32 counter = 1;									// LArray counts from 1
		while ( inner.Next(&possibleChildSuite) ) {
			HT_Resource possibleChild = possibleChildSuite->GetHTNodeData();
			if ( HT_GetParent(possibleChild) == currentNode )
				selection.RemoveItemsAt(1, counter);		// child found, delete it
			else
				counter++;
		}
	}
	
	// remove the items if they went into the trash
	if ( theTask.DropLocationIsFinderTrash() )
		DeleteSelectionByDragToTrash(selection);

	UnselectAllCells();
	
	// cleanup after ourselves
	LArrayIterator it2(selection);
	curr = NULL;
	while ( it.Next(&curr) )
		delete curr;
		
	mViewBeforeDrag = GetHTView();
		
} // DragSelection


//
// DoDragSendData
//
// Called when this window is the originator of the drag, after the drop has succeeded. Since we
// were the ones who created the drag task, we know that one of the flavors is emHTNodeDrag, which
// contains as its data the HT_Resource representing this drag item. Use that resource to get
// the url and title.
//
void
CHyperTreeFlexTable :: DoDragSendData( FlavorType inFlavor, ItemReference inItemRef,
											DragReference inDragRef)
{
	CNetscapeWindow* theWindow = dynamic_cast<CNetscapeWindow*>(LWindow::FetchWindowObject(GetMacPort()));
	ThrowIfNil_(theWindow);
	
	HT_Resource node = NULL;
	Size dataSize = sizeof(HT_Resource);
	OSErr err = ::GetFlavorData(inDragRef, inItemRef, emHTNodeDrag, &node, &dataSize, 0);
	if ( node && !err )
		CURLDragHelper::DoDragSendData ( HT_GetNodeURL(node), HT_GetNodeName(node),
											inFlavor, inItemRef, inDragRef );
	else
		DebugStr("\pFlavor data does not contain a valid HT_Resource structure");
		
} // DoDragSendData


//
// ReceiveDragItem
//
// Called for each item dropped on the table. FindAcceptableFlavor() will try to get us
// the highest fidelity flavor to do the drop...
//
void
CHyperTreeFlexTable :: ReceiveDragItem ( DragReference inDragRef, DragAttributes /*inDragAttrs*/,
											ItemReference inItemRef, Rect & /*inItemBounds*/ )
{
	try {
		FlavorType useFlavor = FindAcceptableFlavor ( inDragRef, inItemRef );
		Size theDataSize = 0;
		switch ( useFlavor ) {
		
			case emHTNodeDrag:
			{
				HT_Resource draggedNode = NULL;
				HT_Resource node = NULL;
				theDataSize = sizeof(void*);
				ThrowIfOSErr_(::GetFlavorData( inDragRef, inItemRef, emHTNodeDrag, &node, &theDataSize, 0 ));
				HandleDropOfHTResource ( node );
			}
			break;
				
			case emBookmarkDrag:
			{
				OSErr err = ::GetFlavorDataSize(inDragRef, inItemRef, emBookmarkDrag, &theDataSize);
				if ( err && err != badDragFlavorErr )
					Throw_(err);
				else if ( theDataSize ) {
					vector<char> urlAndTitle ( theDataSize + 1 );
					try {
						ThrowIfOSErr_( ::GetFlavorData( inDragRef, inItemRef, emBookmarkDrag,
														urlAndTitle.begin(), &theDataSize, 0 ) );
						urlAndTitle[theDataSize] = NULL;
						HandleDropOfPageProxy ( urlAndTitle );
					}
					catch ( ... ) { 
						DebugStr ( "\pError getting flavor data for proxy drag" );
					}
				}
			}
			break;
				
			case flavorTypeHFS:
			{
				HFSFlavor theData;
				Boolean ignore1, ignore2;
				
				::GetFlavorDataSize(inDragRef, inItemRef, flavorTypeHFS, &theDataSize);
				OSErr anError = ::GetFlavorData(inDragRef, inItemRef, flavorTypeHFS, &theData, &theDataSize, nil);
				if ( anError == badDragFlavorErr )
					anError = ::GetHFSFlavorFromPromise (inDragRef, inItemRef, &theData, true);
			
				// if there's an error resolving the alias, the local file url will refer to the alias itself.
				::ResolveAliasFile(&theData.fileSpec, true, &ignore1, &ignore2);
				char* localURL = CFileMgr::GetURLFromFileSpec(theData.fileSpec);
				Assert_(localURL != nil);
				HandleDropOfLocalFile ( localURL, CStr255(theData.fileSpec.name) );	
			}
			break;
				
			case 'TEXT':
				DebugStr("\pchecking for TEXT flavor on drop into tree, unimplemented; g");
				break;
		
		} // case of best flavor
	}
	catch ( ... ) {
		DebugStr ( "\pCan't find the flavor we want; g" );
	}
					
} // ReceiveDragItem


//
// HandleDropOfPageProxy
//
// Called when the user drops the page proxy icon in the NavCenter. The page proxy flavor data
// consists of the url and title, separated by a return. Extract the components and create a
// new node.
//
void
CHyperTreeFlexTable :: HandleDropOfPageProxy ( vector<char> & inURLAndTitle )
{
	char* title = find(inURLAndTitle.begin(), inURLAndTitle.end(), '\r');
	if ( title != inURLAndTitle.end() ) {
		title[0] = NULL;
		title++;
		char* url = inURLAndTitle.begin();
	
		// Extract the node where the drop will occur. If the drop is on a container, put
		// it in the container, unless the mouse is actually between the rows which means the
		// user wants to put it at the same level as the container. Otherwise just put it
		// above (and at the same level) as the item it is dropped on.
		bool dropAtEnd = mDropNode == NULL;
		if ( dropAtEnd ) {
			mDropNode = HT_GetNthItem( GetHTView(), URDFUtilities::PPRowToHTRow(mRows) );
			if ( !mDropNode ) {
				// the view is empty, do drop on and bail
				HT_DropURLAndTitleOn ( HT_TopNode(GetHTView()), url, title );
				return;
			}
		}
		if ( HT_IsContainer(mDropNode) && !mIsDropBetweenRows )
			HT_DropURLAndTitleOn ( mDropNode, url, title );
		else
			HT_DropURLAndTitleAtPos ( mDropNode, url, title, dropAtEnd ? PR_FALSE : PR_TRUE );
	}

} // HandleDropOfPageProxy


//
// HandleDropOfHTResource
//
// Called when the user drops something from one RDF-savvy location (navcenter, personal toolbar)
// to another. This may result in a move or a copy, so do the right thing for each.
//
void
CHyperTreeFlexTable :: HandleDropOfHTResource ( HT_Resource dropNode )
{
	// Extract the node where the drop will occur. If the drop is on a container, put
	// it in the container, unless the mouse is actually between the rows which means the
	// user wants to put it at the same level as the container. Otherwise just put it
	// above (and at the same level) as the item it is dropped on.
	bool dropAtEnd = mDropNode == NULL;
	if ( dropAtEnd ) {
		mDropNode = HT_GetNthItem( GetHTView(), URDFUtilities::PPRowToHTRow(mRows) );
		if ( !mDropNode ) {
			// the view is empty, do drop on and bail
			HT_DropHTROn ( HT_TopNode(GetHTView()), dropNode );
			return;
		}
	} // if we're dropping at end
	if ( HT_IsContainer(mDropNode) && !mIsDropBetweenRows )
		HT_DropHTROn ( mDropNode, dropNode );
	else
		HT_DropHTRAtPos ( mDropNode, dropNode, dropAtEnd ? PR_FALSE : PR_TRUE );
	
} // HandleDropOfHTResource


//
// HandleDropOfLocalFile
//
// Called when the user drops something from the Finder into the navCenter. Create a 
// bookmark with a file URL for that file. This assumes aliases have already been
// resolved
//
void
CHyperTreeFlexTable :: HandleDropOfLocalFile ( const char* inFileURL, const char* inFileName )
{
	// Extract the node where the drop will occur. If the drop is on a container, put
	// it in the container, unless the mouse is actually between the rows which means the
	// user wants to put it at the same level as the container. Otherwise just put it
	// above (and at the same level) as the item it is dropped on.
	bool dropAtEnd = mDropNode == NULL;
	if ( dropAtEnd ) {
		mDropNode = HT_GetNthItem( GetHTView(), URDFUtilities::PPRowToHTRow(mRows) );
		if ( !mDropNode ) {
			// the view is empty, do drop on and bail
			HT_DropURLAndTitleOn ( HT_TopNode(GetHTView()), 
									const_cast<char*>(inFileURL), const_cast<char*>(inFileName) );
			return;
		}
	}
	if ( HT_IsContainer(mDropNode) && !mIsDropBetweenRows )
		HT_DropURLAndTitleOn ( mDropNode, const_cast<char*>(inFileURL), const_cast<char*>(inFileName) );
	else
		HT_DropURLAndTitleAtPos ( mDropNode, const_cast<char*>(inFileURL), const_cast<char*>(inFileName), 
									dropAtEnd ? PR_FALSE : PR_TRUE );
	
} // HandleDropOfLocalFile


//
// FindAcceptableFlavor
// THROWS int
//
// Checks the item being dropped and returns the best flavor that we support. It checks in
// for the following:
//	- HT Node
//	- proxy icon
//	- file from desktop
//	- a TEXT url from some other app
//
FlavorType
CHyperTreeFlexTable :: FindAcceptableFlavor ( DragReference inDragRef, ItemReference inItemRef )
{
	FlavorFlags flags;
	FlavorType checkFor[] = { emHTNodeDrag, emBookmarkDrag, 'TEXT', flavorTypeHFS,
								flavorTypePromiseHFS, NULL };				// end this in a NULL
	for ( int i = 0; checkFor[i]; i++ )
		if ( ::GetFlavorFlags(inDragRef, inItemRef, checkFor[i], &flags) != badDragFlavorErr )
			return checkFor[i];
	
	// cant find anything we're looking for by this point
	throw(-1);
	return 0;
	
} // FindAcceptableFlavor


//
// NodeCanAcceptDrop
//
// Check each item in the drop to see if it can be dropped on the particular node given in |inTargetNode|.
//
Boolean
CHyperTreeFlexTable :: NodeCanAcceptDrop ( DragReference inDragRef, HT_Resource inTargetNode )
{
	Uint16 itemCount;
	::CountDragItems(inDragRef, &itemCount);
	Boolean acceptableDrop = false;
	bool targetIsContainer = HT_IsContainer ( inTargetNode );
	
	for ( Uint16 item = 1; item <= itemCount; item++ ) {
		ItemReference itemRef;
		::GetDragItemReferenceNumber(inDragRef, item, &itemRef);

		try {
			FlavorType useFlavor = FindAcceptableFlavor ( inDragRef, itemRef );
			switch ( useFlavor ) {
			
				case emHTNodeDrag:
				{
					HT_Resource draggedNode = NULL;
					Size theDataSize = sizeof(HT_Resource);
					ThrowIfOSErr_(::GetFlavorData( inDragRef, itemRef, emHTNodeDrag, &draggedNode, &theDataSize, 0 ));
					if ( targetIsContainer )
						acceptableDrop |= HT_CanDropHTROn ( inTargetNode, draggedNode );		// FIX TO CHANGE CURSOR
					else
						acceptableDrop |= HT_CanDropHTRAtPos ( inTargetNode, draggedNode, PR_TRUE );				
				}
				break;
					
				case emBookmarkDrag:
				{
					// We don't care what kind of url it is, just that it is a url so pass a 
					// bogus url to the backend.
					if ( targetIsContainer )
						acceptableDrop |= HT_CanDropURLOn ( inTargetNode, "http://foo.com" );	// FIX TO CHANGE CURSOR
					else
						acceptableDrop |= HT_CanDropURLAtPos ( inTargetNode, "http://foo.com", PR_TRUE );
				}
				break;
					
				case flavorTypeHFS:
				case flavorTypePromiseHFS:
					// We don't care what kind of url it is, just that it is a url so pass a 
					// bogus url to the backend.
					if ( targetIsContainer )
						acceptableDrop |= HT_CanDropURLOn ( inTargetNode, "file://foo" );	// FIX TO CHANGE CURSOR....
					else
						acceptableDrop |= HT_CanDropURLAtPos ( inTargetNode, "file://foo", PR_TRUE );
					break;
					
				case 'TEXT':
					break;
					
			}
		}
		catch ( ... ) {
			DebugStr("\pDrag with no recognized flavors; g");
		}

	} // for each file

	return acceptableDrop;

} // NodeCanAcceptDrop


//
// InlineEditorDone
//
// Called when the user hits return/enter or clicks outside of the inline editor. Tell RDF about the
// change and update the table.
//
void
CHyperTreeFlexTable :: InlineEditorDone ( ) 
{
	Str255 newName;
	mNameEditor->GetDescriptor(newName);
	cstring nameAsCString(newName);

	HT_Resource editedNode = HT_GetNthItem(GetHTView(), URDFUtilities::PPRowToHTRow(mRowBeingEdited) );	
	HT_SetNodeName ( editedNode, nameAsCString );
	
} // InlineEditorDone


//
// CanDoInlineEditing
//
// While we normally want to be able to do inline editing, we have to turn it off for panes
// that don't allow editing (like History). Assumes mRowBeingEdited is correctly set.
//
bool
CHyperTreeFlexTable :: CanDoInlineEditing ( ) 
{
	CHyperTreeHeader* header = dynamic_cast<CHyperTreeHeader*>(mTableHeader);
	Assert_(header);
	CHyperTreeHeader::ColumnInfo info = header->GetColumnInfo ( FindTitleColumnID() - 1 );
	
	HT_Resource item = HT_GetNthItem( GetHTView(), URDFUtilities::PPRowToHTRow(mRowBeingEdited) );
	return HT_IsNodeDataEditable ( item, info.token, info.tokenType );
	
} // CanDoInlineEditing


//
// ChangeSort
//
// called when the user clicks in one of the column headings to change the sort column or
// to change the sort order of the given column.
//
void 
CHyperTreeFlexTable :: ChangeSort ( const LTableHeader::SortChange* inSortChange )
{
	CHyperTreeHeader* header = dynamic_cast<CHyperTreeHeader*>(mTableHeader);
	Assert_(header);
	
	// CHyperTreeHeader::GetColumnInfo() is zero-based and PP is one-based.
	if ( inSortChange->sortColumn >= 1 ) {
		CHyperTreeHeader::ColumnInfo info = header->GetColumnInfo ( inSortChange->sortColumn - 1 );
		HT_SetSortColumn ( GetHTView(), info.token, info.tokenType, inSortChange->reverseSort ? PR_TRUE : PR_FALSE );
	}
	else
		HT_SetSortColumn ( GetHTView(), NULL, 0, PR_FALSE );	// return to natural order
		
	Refresh();
	
} // ChangeSort


//
// MouseLeave
//
// Called when the mouse leaves the tree view. Just update our "hot" cell to 0,0.
//
void 
CHyperTreeFlexTable :: MouseLeave ( ) 
{
	mTooltipCell = STableCell(0, 0);
		
} // MouseLeave


//
// MouseWithin
//
// Called while the mouse moves w/in the tree view. Find which cell the mouse is
// currently over and if it differs from the last cell it was in, hide the 
// tooltip and remember the new cell.
//
void 
CHyperTreeFlexTable :: MouseWithin ( Point inPortPt, const EventRecord& ) 
{
	SPoint32 imagePt;	
	PortToLocalPoint(inPortPt);
	LocalToImagePoint(inPortPt, imagePt);
	
	STableCell hitCell;
	if ( GetCellHitBy(imagePt, hitCell) )
		if ( mTooltipCell != hitCell ) {
			mTooltipCell = hitCell;
			ExecuteAttachments(msg_HideTooltip, this);	// hide tooltip
		}

} // MouseWithin


//
// FindTooltipForMouseLocation
//
// Return the appropriate title for the current mouse location.
void
CHyperTreeFlexTable :: FindTooltipForMouseLocation ( const EventRecord& inMacEvent,
														StringPtr outTip )
{
	Point temp = inMacEvent.where;
	SPoint32 where;	
	GlobalToPortPoint(temp);
	PortToLocalPoint(temp);
	LocalToImagePoint(temp, where);

	STableCell hitCell;
	if ( GetCellHitBy(where, hitCell) && hitCell.col <= mCols ) {
	
		CHyperTreeHeader* header = dynamic_cast<CHyperTreeHeader*>(mTableHeader);
		Assert_(header != NULL);
		CHyperTreeHeader::ColumnInfo info = header->GetColumnInfo(hitCell.col - 1);
		
		HT_Resource node = HT_GetNthItem( GetHTView(), URDFUtilities::PPRowToHTRow(hitCell.row) );
		void* data;
		if ( HT_GetNodeData(node, info.token, info.tokenType, &data) && data ) {
			switch (info.tokenType) {
				case HT_COLUMN_STRING:
					const char* str = static_cast<char*>(data);
					outTip[0] = strlen(str);
 					strcpy ( (char*) &outTip[1], str );
 					break;
 					
 				default:
 					outTip[0] = 0;		// don't display tooltip for other data types
 			} // case of column data type
 		} // if data is valid
	} // if valid cell
	else
		::GetIndString ( outTip, 10506, 16);		// supply a helpful message...

} // FindTooltipForMouseLocation



//
// AdjustCursorSelf
//
// Handle changing cursor to contextual menu cursor if cmd key is down
//
void
CHyperTreeFlexTable::AdjustCursorSelf( Point /*inPoint*/, const EventRecord& inEvent )
{
	ExecuteAttachments(CContextMenuAttachment::msg_ContextMenuCursor, 
								static_cast<void*>(const_cast<EventRecord*>(&inEvent)));

}

#pragma mark -- class CHyperTreeSelector --

//
// DoSelect
//
// We need to make sure that the HTView's concept of the selection is in sync with what PP
// believes to be the current selection. Since all modifications to the selection are
// channeled through this method, we just override it to also tell HT about the selection
// change. This also picks up SelectCell() and UnselectCell() calls since the LTableRowSelector
// routes those calls here
//
void
CHyperTreeSelector :: DoSelect(	const TableIndexT 	inRow, 
								Boolean 			inSelect, 
								Boolean 			inHilite,
								Boolean				inNotify	)
{
	HT_Resource node = HT_GetNthItem( mTreeView, URDFUtilities::PPRowToHTRow(inRow) );
	if (node) {
		// when we update the HT selection, we'll get an update event. We don't want
		// this because we know we're updating it.
		HT_NotificationMask oldMask;
		HT_Pane pane = HT_GetPane ( mTreeView );
		HT_GetNotificationMask ( pane, &oldMask );
		HT_SetNotificationMask ( pane, 0L );
		HT_SetSelectedState	( node, inSelect ? PR_TRUE : PR_FALSE );	
		HT_SetNotificationMask ( pane, oldMask );
	}
	LTableRowSelector::DoSelect(inRow, inSelect, inHilite, inNotify);

} // DoSelect


//
// CellIsSelected
//
// Just let HT tell us about what is selected and what isn't.
//
Boolean
CHyperTreeSelector :: CellIsSelected ( const STableCell &inCell ) const
{
	HT_Resource node = HT_GetNthItem( mTreeView, URDFUtilities::PPRowToHTRow(inCell.row) );
	return HT_IsSelected(node);

} // CellIsSelected


//
// SyncSelectorWithHT
//
// Since HT is the one that retains the selection for the view, when a new view is
// displayed, we will be out of sync. This methods gets us back in sync.
//
void
CHyperTreeSelector :: SyncSelectorWithHT ( )
{
	// unselect all cells in this selector so we start with a clean slate
	mSelectedRowCount = 0;
	const char notSelected = 0;
	for ( int i = 1; i <= GetCount(); i++ )
		AssignItemsAt ( 1, i, &notSelected );
	
	// walk through the HT selection and update our internal bookkeeping info
	HT_Resource curr = HT_GetNextSelection(mTreeView, HT_TopNode(mTreeView));
	while ( curr ) {
		LTableRowSelector::DoSelect ( URDFUtilities::HTRowToPPRow(HT_GetNodeIndex(mTreeView, curr)),
										true, true, false );
		curr = HT_GetNextSelection(mTreeView, curr);
	}
	
} // SyncSelectorWithHT