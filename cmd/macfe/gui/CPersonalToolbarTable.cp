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
// Implementation of the table that is the main component of the personal toolbar.
//

#include <vector.h>
#include <algorithm>

#include "CPersonalToolbarTable.h"
#include "uapp.h"
#include "LTableMultiGeometry.h"
#include "CURLDragHelper.h"
#include "macutil.h"
#include "CIconTextDragTask.h"
#include "resgui.h"
#include "URDFUtilities.h"
#include "prefapi.h"
#include "miconutils.h"
#include "UMemoryMgr.h"


static const RGBColor blue = { 0, 0, 0xFFFF };
static const RGBColor black = { 0, 0, 0 };
static const RGBColor bgColor = { 0xDDDD, 0xDDDD, 0xDDDD };


DragSendDataUPP CPersonalToolbarTable::sSendDataUPP;

const char* CPersonalToolbarTable::kMaxButtonCharsPref = "browser.personal_toolbar_button.max_chars";
const char* CPersonalToolbarTable::kMinButtonCharsPref = "browser.personal_toolbar_button.min_chars";

// init to something wacky so we can tell if they have been initialized already and avoid doing
// it again with every new toolbar
Uint32 CPersonalToolbarTable :: mMaxToolbarButtonChars = LArray::index_Last;
Uint32 CPersonalToolbarTable :: mMinToolbarButtonChars = LArray::index_Last;




CPersonalToolbarTable :: CPersonalToolbarTable ( LStream* inStream )
	: LSmallIconTable(inStream), LDragAndDrop ( GetMacPort(), this ),
		mDropCol(LArray::index_Bad), mHiliteCol(LArray::index_Bad), mDropOn(false), mButtonList(NULL),
		mIsInitialized(false)
{
	// setup our window into the RDF world and register this class as the one to be notified
	// when the personal toolbar changes.
	HT_Notification notifyStruct = CreateNotificationStruct();
	HT_Pane toolbarPane = HT_NewPersonalToolbarPane(notifyStruct);
	if ( !toolbarPane )
		throw SomethingBadInHTException();
	mToolbarView = HT_GetSelectedView ( toolbarPane );
	if ( !mToolbarView )
		throw SomethingBadInHTException();
	mToolbarRoot = HT_TopNode ( mToolbarView );
	if ( !mToolbarRoot )
		throw SomethingBadInHTException();
	HT_SetOpenState ( mToolbarRoot, PR_TRUE );	// ensure container is open so we can see its contents

	// read in the maximum size we're allowing for personal toolbar items. If not
	// found in prefs, use our own. Only do this if the static members have not yet
	// been initialized to prevent doing it with each new toolbar.
	if ( mMaxToolbarButtonChars == LArray::index_Last ) {
		int32 maxToolbarButtonChars, minToolbarButtonChars;
		if ( PREF_GetIntPref(kMaxButtonCharsPref, &maxToolbarButtonChars ) == PREF_ERROR )
			mMaxToolbarButtonChars = kMaxPersonalToolbarChars;
		else
			mMaxToolbarButtonChars = maxToolbarButtonChars;
		if ( PREF_GetIntPref(kMinButtonCharsPref, &minToolbarButtonChars ) == PREF_ERROR )
			mMinToolbarButtonChars = kMinPersonalToolbarChars;
		else
			mMinToolbarButtonChars = minToolbarButtonChars;
	}
	
	// LSmallIconTable wants to use a LTableSingleGeometry, but since we want the toolbar
	// to have variable column widths, replace it with a multiGeometry implementation
	if ( mTableGeometry ) {
		delete mTableGeometry;
		mTableGeometry = new LTableMultiGeometry ( this, mFrameSize.width, 20 );
	}
	
} // constructor


//
// Destructor
//
// Get rid of the button list. Since it is a list of objects, not pointers, we can just
// let the vector destructor handle everything.
//
CPersonalToolbarTable :: ~CPersonalToolbarTable ( )
{
	delete mButtonList;

} // destructor


//
// FinishCreateSelf
//
// Makes sure there is one row to put stuff in and creates a drag send proc.
//
void
CPersonalToolbarTable :: FinishCreateSelf ( )
{
	InsertRows ( 1, 1, NULL, 0, false );
	InitializeButtonInfo();
	
	if ( !sSendDataUPP) {
		sSendDataUPP = NewDragSendDataProc(LDropArea::HandleDragSendData);
		ThrowIfNil_(sSendDataUPP);
	}

	mIsInitialized = true;
	
} // FinishCreateSelf


//
// InitializeButtonInfo
//
// Create our list of information for each bookmark that belongs in the toolbar. This
// involves iterating over the RDF data in the folder pre-selected to be the personal
// toolbar folder, compiling some info about it, and adding to an internal list. 
//
// Why build into an internal list when HT already has it? The assumption is that users
// will be resizing windows more often than adding things to the personal toolbar. By
// keeping a list around, when the user resizes, we don't have to go back to HT for all
// this information but can keep it in our local list. Better still, this also allows us to have the
// routines that fill in the toolbar not rely directly on and will allow us to more easily
// rework the underlying toolbar infrastructure in the future.
//
void 
CPersonalToolbarTable :: InitializeButtonInfo ( )
{
	delete mButtonList;
	mButtonList = new ButtonList( HT_GetItemListCount(GetHTView()) );
	ButtonListIterator currButton = mButtonList->begin();
	
	HT_Cursor cursor = HT_NewCursor( mToolbarRoot );
	if ( cursor ) {
		HT_Resource currNode = HT_GetNextItem(cursor);
		while ( currNode ) {
			CUserButtonInfo info ( HT_GetNodeName(currNode), HT_GetNodeURL(currNode),
										0, 0, HT_IsContainer(currNode), currNode);
			*currButton = info;
			currNode = HT_GetNextItem(cursor);
			currButton++;
			
		} // for each top-level node
	}
	
} // InitializeButtonInfo


//
// HandleNotification
//
// Called when the user makes a change to the personal toolbar folder from some other
// place (like the bookmarks view). Reset our internal list and tell our listeners
// that things are now different.
//
void
CPersonalToolbarTable  :: HandleNotification(
	HT_Notification	/*notifyStruct*/,
	HT_Resource		node,
	HT_Event		event)
{
	switch (event)
	{
		case HT_EVENT_NODE_ADDED:
		case HT_EVENT_VIEW_REFRESH:			// catches deletes
		case HT_EVENT_NODE_VPROP_CHANGED:
		{
			// only update toolbar if the this view is the one that changed
			if ( HT_GetView(node) == GetHTView() )						
				ToolbarChanged();
			break;
		}		
	}
}


//
// ToolbarChanged
//
// The user has just made some change to the toolbar, such as manipulating items in the nav center
// or changing the personal toolbar folder. Since these changes could be quite extensive, we
// just clear out the whole thing and start afresh. Tell all the toolbars that they have to
// clear themselves and grab the new information. Changes to the toolbar directly, such as by
// drag and drop, are handled elsewhere.
//
void
CPersonalToolbarTable :: ToolbarChanged ( )
{
	if ( mCols )
		RemoveCols ( mCols, 1, false ); 
	InitializeButtonInfo();				// out with the old...
	FillInToolbar();					// ...in with the new

} // ToolbarHeaderChanged


//
// RemoveButton
//
// The user has explicitly removed a button from the toolbar in one of the windows (probably
// by dragging it to the trash). Let HT get rid of it.
//
void 
CPersonalToolbarTable :: RemoveButton ( Uint32 inIndex )
{
	// remove it from HT
	HT_Resource deadNode = HT_GetNthItem ( GetHTView(), URDFUtilities::PPRowToHTRow(inIndex) );
	HT_RemoveChild ( mToolbarRoot, deadNode );

	// no need to refresh because we will get an HT view update event and
	// refresh at that time. The deleted node will also be removed from the list as it is
	// rebuilt.
	
} // RemoveButton


//
// AddButton
//
// Given a pre-created HT item, add it to the personal toolbar folder before the
// given id and tell all the toolbars to update. This is called when the user
// drops an item from another RDF source on a personal toolbar
//
void 
CPersonalToolbarTable :: AddButton ( HT_Resource inBookmark, Uint32 inIndex )
{
	// Add this to RDF.
	if ( mButtonList->size() ) {
		// If we get back a null resource then we must be trying to drop after
		// the last element, or there just plain aren't any in the toolbar. Re-fetch the last element
		// (using the correct index) and add the new bookmark AFTER instead of before or just add
		// it to the parent for the case of an empty toolbar
		PRBool before = PR_TRUE;
		HT_Resource dropOn = NULL;
		if ( inIndex <= mButtonList->size() )
			dropOn = GetInfoForPPColumn(inIndex).GetHTResource();
		else {
			dropOn = (*mButtonList)[mButtonList->size() - 1].GetHTResource();
			before = PR_FALSE;
		}
		HT_DropHTRAtPos ( dropOn, inBookmark, before );
	} // if items in toolbar
	else
		HT_DropHTROn ( mToolbarRoot, inBookmark );
		
	// no need to tell toolbars to refresh because we will get an HT node added event and
	// refresh at that time.
	
} // Addbutton


//
// AddButton
//
// Given just a URL, add it to the personal toolbar folder before the
// given id and tell all the toolbars to update. This is called when the user
// drops an item from another RDF source on a personal toolbar
//
void 
CPersonalToolbarTable :: AddButton ( const string & inURL, const string & inTitle, Uint32 inIndex )
{
	// Add this to RDF.
	if ( mButtonList->size() ) {
		// If we get back a null resource then we must be trying to drop after
		// the last element, or there just plain aren't any in the toolbar. Re-fetch the last element
		// (using the correct index) and add the new bookmark AFTER instead of before or just add
		// it to the parent for the case of an empty toolbar
		PRBool before = PR_TRUE;
		HT_Resource dropOn = NULL;
		if ( inIndex <= mButtonList->size() )
			dropOn = GetInfoForPPColumn(inIndex).GetHTResource();
		else {
			dropOn = (*mButtonList)[mButtonList->size() - 1].GetHTResource();
			before = PR_FALSE;
		}
		HT_DropURLAndTitleAtPos ( dropOn, const_cast<char*>(inURL.c_str()), 
									const_cast<char*>(inTitle.c_str()), before );
	} // if items in toolbar
	else
		HT_DropURLAndTitleOn ( mToolbarRoot, const_cast<char*>(inURL.c_str()), 
									const_cast<char*>(inTitle.c_str()) );  
		
	// no need to tell toolbars to refresh because we will get an HT node added event and
	// refresh at that time.
	
} // Addbutton


//
// FillInToolbar
//
// Add rows to the table and try to give each column as much space as possible on the fly. 
// Don't make any assumptions about the incoming text mode...be paranoid!
//
void 
CPersonalToolbarTable :: FillInToolbar ( )
{
	const Uint32 kPadRight = 15;
	const Uint32 kBMIconWidth = 28;
	const Uint32 kButtonCount = mButtonList->size();
	const Uint32 kMaxLength = GetMaxToolbarButtonChars();
	const Uint32 kMinLength = GetMinToolbarButtonChars();

	StTextState saved;
	UTextTraits::SetPortTextTraits(kTextTraitsID);
	
	//
	// create an array that holds the # of characters currently in each column. Initialize this with
	// the min column size (specified in prefs), or less if the title has less chars.
	//
	vector<Uint16> widthArray(kButtonCount);
	vector<Uint16>::iterator widthArrayIter = widthArray.begin();
	Uint16 totalWidth = 0;
	for ( ButtonListConstIterator it = mButtonList->begin(); it != mButtonList->end(); it++ ) {
		
		Uint16 numChars = min ( kMinLength, it->GetName().length() );
		*widthArrayIter = numChars;
		totalWidth += ::TextWidth(it->GetName().c_str(), 0, numChars) + kBMIconWidth;		// extend the total...
		widthArrayIter++;
		
	} // initialize each item
	
	
	if ( mCols > kButtonCount )
		RemoveRows ( mCols - kButtonCount, 0, false );
	else
		InsertCols ( kButtonCount - mCols, 0, NULL, 0, false );
	
	// compute how much room we have left to divvy out after divvying out the minimum above. Leave
	// a few pixels on the right empty...
	int16 emptySpace = (mFrameSize.width - kPadRight) - totalWidth;
	
	//
	// divvy out space to each column, one character at a time to each column
	//
	while ( emptySpace > 0 ) {

		bool updated = false;
		widthArrayIter = widthArray.begin();
		for ( ButtonListConstIterator it = mButtonList->begin(); it != mButtonList->end(); it++, widthArrayIter++ ) {
			if ( emptySpace && *widthArrayIter < it->GetName().length() && *widthArrayIter < kMaxLength ) {
				// subtract off the single character we're adding to the column
				char addedChar = it->GetName()[*widthArrayIter];
				emptySpace -= ::TextWidth(&addedChar, 0, 1);
				(*widthArrayIter)++;
				updated = true;			// we found something to add, allow us to continue
			}
		} // for each column

		// prevent infinite looping if no column can be expanded and we still have extra room remaining
		if ( !updated )
			break;
			
	} // while space remains to divvy out
	
	//
	// assign the widths to the columns and set the data in the columns.
	//
	STableCell where (1, 1);
	widthArrayIter = widthArray.begin();
	for ( ButtonListConstIterator it = mButtonList->begin(); it != mButtonList->end(); it++ ) {

		const string & bmText = it->GetName();
		Uint32 textLength = bmText.length();

		//
		// fill in the cell with the name of the bookmark and set the column width to
		// the width of the string. If the name is longer than the max, chop it.
		//
		SIconTableRec data;
		Uint16 dispLength = *widthArrayIter;
		::BlockMove ( bmText.c_str(), &data.name[1], dispLength + 1 );
		data.name[0] = dispLength <= kMaxLength ? dispLength : kMaxLength;	
		data.iconID = it->IsFolder() ? kFOLDER_ICON : kBOOKMARK_ICON;
		SetCellData ( where, &data, sizeof(SIconTableRec) );
		Uint32 pixelWidth = ::TextWidth(bmText.c_str(), 0, dispLength)+ kBMIconWidth;
		mTableGeometry->SetColWidth (  pixelWidth, where.col, where.col );

		widthArrayIter++;
		where.col++;		// next column, please....
		
	} // for each bookmark in the folder

	Refresh();
	
} // FillInToolbar


//
// MouseLeave
//
// Called when the mouse leaves the personal toolbar. Redraw the previous selection to get rid
// of the blue text color.
//
void 
CPersonalToolbarTable :: MouseLeave ( ) 
{
	STableCell refresh(1, mHiliteCol);
	mHiliteCol = LArray::index_Bad;
	
	StClipRgnState savedClip;
	FocusDraw();
	RedrawCellWithTextClipping ( refresh );
	
} // MouseLeave


//
// MouseWithin
//
// Called while the mouse moves w/in the personal toolbar. Find which cell the mouse is
// currently over and remember it. If we've moved to a different cell, make sure to 
// refresh the old one so it is drawn normally.
//
void 
CPersonalToolbarTable :: MouseWithin ( Point inPortPt, const EventRecord& ) 
{
	// get the previous selection
	STableCell old(1, mHiliteCol);
	SPoint32 imagePt;	
	PortToLocalPoint(inPortPt);
	LocalToImagePoint(inPortPt, imagePt);
	
	STableCell hitCell;
	if ( GetCellHitBy(imagePt, hitCell) )
		if ( old != hitCell ) {
			StClipRgnState savedClip;
			
			mHiliteCol = hitCell.col;

			FocusDraw();
			if ( old.col )
				RedrawCellWithTextClipping(old);
			if ( hitCell.col )
				RedrawCellWithTextClipping(hitCell);
			
			ExecuteAttachments(msg_HideTooltip, this);	// hide tooltip
		}

} // MouseWithin


//
// Click
//
// Allow drags to occur when another window is in front of us.
//
void
CPersonalToolbarTable :: Click ( SMouseDownEvent& inMouseDown )
{
	PortToLocalPoint(inMouseDown.whereLocal);
	UpdateClickCount(inMouseDown);
	
	if (ExecuteAttachments(msg_Click, &inMouseDown))
	{
		ClickSelf(inMouseDown);
	}
}


#if 0
//
// Click
//
// Overridden to handle clase of drag and drop when communicator is in the background. Code
// copied from CWPro1 CD sample code (CDragAndDropTable.cp).
//
// FOR SOME REASON, THIS DOESN'T WORK....DUNNO WHY...
void
CPersonalToolbarTable :: Click ( SMouseDownEvent& inMouseDown )
{
	if ( inMouseDown.delaySelect && DragAndDropIsPresent() ) {

		// In order to support dragging from an inactive window,
		// we must explicitly test for delaySelect and the
		// presence of Drag and Drop.

		// Convert to a local point.
		PortToLocalPoint( inMouseDown.whereLocal );
		
		// Execute click attachments.
		if ( ExecuteAttachments( msg_Click, &inMouseDown ) ) {
		
			// Handle the actual click event.
			ClickSelf( inMouseDown );

		}
	
	} else {

		// Call inherited for default behavior.	
		LTableView::Click( inMouseDown );
	
	}

} // Click
#endif


//
// ClickSelect
//
// Override to always say that we can be selected, because there is no concept of selection
// in this toolbar.
Boolean
CPersonalToolbarTable :: ClickSelect( const STableCell &/*inCell*/, const SMouseDownEvent &/*inMouseDown*/)
{
	return true;
}


//
// ClickCell
//
// Override the default behavior of selecting a cell to open the url associated with the bookmark
// there.
//
void
CPersonalToolbarTable :: ClickCell(const STableCell &inCell, const SMouseDownEvent &inMouseDown)
{
	if ( ::WaitMouseMoved(inMouseDown.macEvent.where) ) {
	
		if (LDropArea::DragAndDropIsPresent()) {
		
			mDraggedCell = inCell;		// save which cell is being dragged for later
			
			// create the drag task
			Rect bounds;
			GetLocalCellRect ( inCell, bounds );
			const CUserButtonInfo & data = GetInfoForPPColumn(inCell.col);
			string finalCaption = CURLDragHelper::MakeIconTextValid ( data.GetName().c_str() );
			CIconTextSuite suite ( this, bounds, kBOOKMARK_ICON, finalCaption, data.GetHTResource() );
			CIconTextDragTask theTask(inMouseDown.macEvent, &suite, bounds);
			
			// setup our special data transfer proc called upon drag completion
			OSErr theErr = ::SetDragSendProc ( theTask.GetDragReference(), sSendDataUPP, (LDropArea*) this );
			ThrowIfOSErr_(theErr);
			
			theTask.DoDrag();
			
			// remove the url if it went into the trash
			if ( theTask.DropLocationIsFinderTrash() )
				RemoveButton ( inCell.col );
			
		} // if d&d allowed
		
	} // if drag
	else {
	
		const CUserButtonInfo & data = GetInfoForPPColumn(inCell.col);
		
		// code for context menu here, if appropriate....
		
		if ( data.IsFolder() ) {
			SysBeep(1);
		}
		else
			CFrontApp::DoGetURL( data.GetURL().c_str() );
		
	} // else just a click
	
} // ClickCell


//
// RedrawCellWithHilite
//
// A helpful utility routine that wraps DrawCell() with calls to setup the drawing params
// appropriately. When redrawing a cell during a drag and drop, call this routine instead
// of calling DrawCell directly.
//
void
CPersonalToolbarTable :: RedrawCellWithHilite ( const STableCell inCell, bool inHiliteOn )
{
	Rect localCellRect;
	GetLocalCellRect ( inCell, localCellRect );
	
	// since mDropOn is used as the flag in DrawCell() for whether or not we want to 
	// draw the hiliting on the cell, save its value and set it to what was passed in
	// before calling DrawCell().
	StValueChanger<Boolean> oldHilite(mDropOn, inHiliteOn);	//еее won't link with bool type =(
	
	// if the hiliting is being turned off, erase the cell so it draws normally again
	if ( !inHiliteOn ) {
		StColorState saved;
		::RGBBackColor(&bgColor);
		::EraseRect(&mTextHiliteRect);		// we can be sure this has been previously computed
	}
	
	DrawCell ( inCell, localCellRect );
	
} // RedrawCellWithHilite


//
// DrawCell
//
// Override to draw differently when this is the selected cell. Otherwise, pass it back
// to the inherited routine to draw normally. 
//
void 
CPersonalToolbarTable :: DrawCell ( const STableCell &inCell, const Rect &inLocalRect )
{
	StTextState savedText;
	StColorPenState savedColor;
	::RGBForeColor(&black);
	
	SIconTableRec iconAndName;
	Uint32 dataSize = sizeof(SIconTableRec);
	GetCellData(inCell, &iconAndName, dataSize);
	
	Rect iconRect;
	iconRect.left = inLocalRect.left + 3;
	iconRect.right = iconRect.left + 16;
	iconRect.bottom = inLocalRect.bottom - 2;
	iconRect.top = iconRect.bottom - 16;
	IconTransformType transform = kTransformNone;
	if ( mDropOn )										// handle drop on folder
		transform = kTransformSelected;
	::PlotIconID(&iconRect, atNone, transform, iconAndName.iconID);
	
	UTextTraits::SetPortTextTraits(kTextTraitsID);
	
	if ( mDropOn ) {									// handle drop on folder
		mTextHiliteRect = ComputeTextRect ( iconAndName, inLocalRect );
		StColorState savedColorForTextDrawing;
		::RGBBackColor(&black);
		::EraseRect(&mTextHiliteRect);
		::TextMode(srcXor);
	}
	else if ( mHiliteCol == inCell.col ) {
		TextFace(underline);
		RGBForeColor( &blue );
	}
	::MoveTo(inLocalRect.left + 22, inLocalRect.bottom - 4);
	::DrawString(iconAndName.name);
	
} // DrawCell


//
// DoDragSendData
//
// When the drag started, the ClickCell() routine cached which cell was the start of the drag in
// |mDraggedCell|. Extract the URL and Title from this cell and pass it along to the helper
// routine from CURLDragHelper
// 
void
CPersonalToolbarTable :: DoDragSendData( FlavorType inFlavor, ItemReference inItemRef,
											DragReference inDragRef)
{
	const CUserButtonInfo & dragged = GetInfoForPPColumn ( mDraggedCell.col );
	CURLDragHelper::DoDragSendData ( dragged.GetURL().c_str(), 
										const_cast<char*>(dragged.GetName().c_str()),
										inFlavor, inItemRef, inDragRef );

} // DoDragSendData


//
// HiliteDropArea
//
// Show that this toolbar is a drop site for urls
//
void
CPersonalToolbarTable :: HiliteDropArea ( DragReference inDragRef )
{
	Rect frame;
	CalcLocalFrameRect ( frame );
	
	// show the drag hilite in drop area
	RgnHandle rgn = ::NewRgn();
	ThrowIfNil_(rgn);
	::RectRgn ( rgn, &frame );
	::ShowDragHilite ( inDragRef, rgn, true );
	::DisposeRgn ( rgn );	

} // HiliteDropArea


//
// InsideDropArea
//
// Called repeatedly while mouse is inside this during a drag. For each column, the cell
// can be divided into several sections.
// 
void
CPersonalToolbarTable :: InsideDropArea ( DragReference inDragRef )
{
	FocusDraw();
	
	Point mouseLoc;
	SPoint32 imagePt;
	::GetDragMouse(inDragRef, &mouseLoc, NULL);
	::GlobalToLocal(&mouseLoc);
	LocalToImagePoint(mouseLoc, imagePt);

	Rect localRect;
	TableIndexT colMouseIsOver = mTableGeometry->GetColHitBy(imagePt);
	GetLocalCellRect ( STableCell(1, colMouseIsOver), localRect );
	TableIndexT newDropCol;
	bool newDropOn = false;
	if ( colMouseIsOver <= mCols ) {
		const CUserButtonInfo & info = GetInfoForPPColumn(colMouseIsOver);
		if ( info.IsFolder() ) {
			Rect leftSide, rightSide;
			ComputeFolderDropAreas ( localRect, leftSide, rightSide );
			if ( ::PtInRect(mouseLoc, &leftSide) )
				newDropCol = colMouseIsOver;			// before this cell
			else if ( ::PtInRect(mouseLoc, &rightSide) )
				newDropCol = colMouseIsOver + 1;		// after this cell
			else {
				newDropCol = colMouseIsOver;
				newDropOn = true;						// hilite folder, don't draw line
			}
		}
		else {
			// draw line between rows
			Rect leftSide, rightSide;
			ComputeItemDropAreas ( localRect, leftSide, rightSide );
			if ( ::PtInRect(mouseLoc, &leftSide) )
				newDropCol = colMouseIsOver;			// before this cell
			else
				newDropCol = colMouseIsOver + 1;		// after this cell
		}
	} // if drag over existing column
	else {
		// else drag over empty part of toolbar
 		newDropCol = mCols + 1;
	}
	
	// if something has changed, redraw as necessary
	if ( newDropCol != mDropCol || newDropOn != mDropOn ) {
	
		// unhilight old one (if necessary)
		if ( mDropOn )
			RedrawCellWithHilite( STableCell(1, mDropCol), false );
		else
			if ( mDropCol > 0 )
				DrawDividingLine ( mDropCol );
		
		mDropCol = newDropCol;
		mDropOn = newDropOn;
		
		// hilight new one
		if ( mDropOn )
			RedrawCellWithHilite ( STableCell(1, mDropCol), true );
		else
			DrawDividingLine ( newDropCol );
		
	} // if mouse moved to another drop location
	
} // InsideDropArea


//
// LeaveDropArea
//
// Clean up the drop feedback stuff when the mouse leaves the area
//
void
CPersonalToolbarTable :: LeaveDropArea( DragReference inDragRef )
{
	FocusDraw();
	
	// if we were hiliting a folder, redraw it without the hiliting. If we were drawing a 
	// line, undraw it.
	if ( mDropOn )
		RedrawCellWithHilite ( STableCell(1,mDropCol), false );
	else
		DrawDividingLine( mDropCol );

	mDropCol = LArray::index_Bad;
	mDropOn = false;
	
	// Call inherited.
	LDragAndDrop::LeaveDropArea( inDragRef );
}


//
// ComputeItemDropAreas
//
// When a drag goes over a cell that contains a single node, divide the node in half. The left
// side will represent dropping before the node, the right side after.
//
void
CPersonalToolbarTable :: ComputeItemDropAreas ( const Rect & inLocalCellRect, Rect & oLeftSide, 
												Rect & oRightSide )
{
	oRightSide = oLeftSide = inLocalCellRect;
	uint16 midPt = (oLeftSide.right - oLeftSide.left) / 2;
	oLeftSide.right = oLeftSide.left + midPt;
	oRightSide.left = oLeftSide.left + midPt + 1;
	
} // ComputeItemDropAreas


//
// ComputeFolderDropAreas
//
// When a drag goes over a cell that contains a folder, divide the cell area into 3 parts. The middle
// area, which corresponds to a drop on the folder takes up the majority of the cell space with the
// left and right areas (corresponding to drop before and drop after, respectively) taking up the rest
// at the ends.
//
void
CPersonalToolbarTable :: ComputeFolderDropAreas ( const Rect & inLocalCellRect, Rect & oLeftSide, 
												Rect & oRightSide )
{
	// make sure the left/right area widths aren't too big for the cell
	Uint16 endAreaWidth = 20;
	if ( inLocalCellRect.right - inLocalCellRect.left < 50 )
		endAreaWidth = 5;
		
	oRightSide = oLeftSide = inLocalCellRect;
	oLeftSide.right = oLeftSide.left + endAreaWidth;
	oRightSide.left = oRightSide.right - endAreaWidth;
	
} // ComputeFolderDropAreas


//
// DrawDividingLine
//
// Draws a vertical black line before the given column in the toolbar (or undraws if one
// is already there). If the column given is after the right edge of the last column, it
// draws after the last column (as the user expects).
//
void
CPersonalToolbarTable :: DrawDividingLine( TableIndexT inCol )
{
	Uint32 numItems = mButtonList->size();
	if ( !numItems )			// don't draw anything if toolbar empty
		return;
	
	// Setup the target cell.
	STableCell theCell;
	theCell.row = 1;
	theCell.col = inCol;

	// find the boundary of the cell we're supposed to be drawing the line before. If
	// the incoming column is at the end (greater than the # of buttons) then hack up
	// a boundary rect to draw after the last column.
	Rect cellBounds;
	if ( inCol > numItems ) {
		// get the boundary of the last column
		theCell.col = numItems;
		GetLocalCellRect ( theCell, cellBounds );
		
		// make the left edge of our fake column the right edge of the last column
		cellBounds.left = cellBounds.right;
	}
	else
		GetLocalCellRect ( theCell, cellBounds );
	
	// Focus the pane and get the table and cell frames.
	Rect theFrame;
	if ( FocusDraw() && CalcLocalFrameRect( theFrame ) ) {

		// Save the draw state and clip the list view rect.
		StColorPenState	theDrawState;
		StClipRgnState	theClipState( theFrame );

		// Setup the color and pen state then draw the line
		::ForeColor( blackColor );
		::PenMode( patXor );
		::PenSize( 2, 2 );
		::MoveTo( cellBounds.left, cellBounds.top );
		::LineTo( cellBounds.left, cellBounds.bottom );
	
	}
	
} // DrawDividingLine


//
// ReceiveDragItem
//
// Pass this along to the implementation in CHTAwareURLDragMixin.
//
void
CPersonalToolbarTable :: ReceiveDragItem ( DragReference inDragRef, DragAttributes inDragAttrs,
											ItemReference inItemRef, Rect & inItemBounds )
{
	CHTAwareURLDragMixin::ReceiveDragItem(inDragRef, inDragAttrs, inItemRef, inItemBounds );

} // ReceiveDragItem


//
// ItemIsAcceptable
//
// If FindBestFlavor() finds an acceptable flavor, then this item can be accepted. If not, it
// can't. We don't really care at this point what that flavor is.
//
Boolean
CPersonalToolbarTable :: ItemIsAcceptable ( DragReference inDragRef, ItemReference inItemRef )
{
	FlavorType ignored;
	return FindBestFlavor ( inDragRef, inItemRef, ignored );
	
} // ItemIsAcceptable


//
// HandleDropOfHTResource
//
// Called when the user drops an HT_Resource from some other HT aware view on the
// personal toolbar. Add a button for this new item.
//
void
CPersonalToolbarTable :: HandleDropOfHTResource ( HT_Resource inDropNode ) 
{
	if ( mDropOn ) {
		SysBeep(1);		//еее implement
	}
	else
		AddButton ( inDropNode, mDropCol );	

} // HandleDropOfHTResource


//
// HandleDropOfPageProxy
//
// Called when user drops the page proxy icon (or an embedded url) onto the personal
// toolbar. Add a button for this new item.
//
void
CPersonalToolbarTable :: HandleDropOfPageProxy ( const char* inURL, const char* inTitle )
{
	if ( mDropOn ) {
		SysBeep(1);		//еее implement
	}
	else
		AddButton ( inURL, inTitle, mDropCol );

} // HandleDropOfPageProxy


//
// HandleDropOfLocalFile
//
// A local file is just an url and a title, so cheat.
//
void
CPersonalToolbarTable :: HandleDropOfLocalFile ( const char* inFileURL, const char* fileName,
													const HFSFlavor & /*inFileData*/ )
{
	HandleDropOfPageProxy ( inFileURL, fileName );
	
} // HandleDropOfLocalFile


//
// HandleDropOfText
//
// Need to implement
//
void 
CPersonalToolbarTable :: HandleDropOfText ( const char* inTextData )
{
	DebugStr("\pDropping TEXT here not implemented; g");
	
} // HandleDropOfText


void
CPersonalToolbarTable :: FindTooltipForMouseLocation ( const EventRecord& inMacEvent,
														StringPtr outTip )
{
	Point temp = inMacEvent.where;
	SPoint32 where;	
	GlobalToPortPoint(temp);
	PortToLocalPoint(temp);
	LocalToImagePoint(temp, where);
	
	STableCell hitCell;
	if ( GetCellHitBy(where, hitCell) && hitCell.col <= mCols ) {
		const CUserButtonInfo & bkmk = GetInfoForPPColumn(hitCell.col);
		outTip[0] = bkmk.GetName().length();
		strcpy ( (char*) &outTip[1], bkmk.GetName().c_str() );
	}
	else
		::GetIndString ( outTip, 10506, 12);		// supply a helpful message...

} // FindTooltipForMouseLocation


void
CPersonalToolbarTable :: ResizeFrameBy ( Int16 inWidth, Int16 inHeight, Boolean inRefresh )
{
	LSmallIconTable::ResizeFrameBy(inWidth, inHeight, inRefresh);
	
	// don't call ToolbarChanged() because that will reload from HT. Nothing has really
	// changed except for the width of the toolbar. Checking if we've gone through
	// FinishCreateSelf() will cut down on the number of times we recompute the toolbar
	// as the window is being created.
	if ( mIsInitialized )
		FillInToolbar();
	 
} // ResizeFrameTo


//
// ComputeTextRect
//
// Compute the area of the text rectangle so we can clip to it on redraw
//
Rect
CPersonalToolbarTable :: ComputeTextRect ( const SIconTableRec & inText, const Rect & inLocalRect )
{
	Rect textRect;
	
	uint16 width = ::TextWidth(inText.name, 1, *inText.name);
	textRect.left = inLocalRect.left + 20;
	textRect.right = textRect.left + width + 2;
	textRect.bottom = inLocalRect.bottom - 1;
	textRect.top = inLocalRect.top + 1;

	return textRect;
	
} // ComputeTextRect


//
// RedrawCellWithTextClipping
//
// A pretty way to redraw the cell because it won't flash. This sets the clip region to the 
// text that needs to be redrawn so it looks fast to the user.
//
// NOTE: This blows away the current clip region so if you care, you must restore it afterwards. I
// don't do it here because you may want to make a couple of these calls in a row and restoring the
// clip rect in between would be a waste. It also blows away the background color.
//
void
CPersonalToolbarTable :: RedrawCellWithTextClipping ( const STableCell & inCell )
{
	StTextState savedText;
	SIconTableRec iconAndName;
	Rect localRect;

	UTextTraits::SetPortTextTraits(kTextTraitsID);
			
	Uint32 dataSize = sizeof(SIconTableRec);
	GetCellData(inCell, &iconAndName, dataSize);
	GetLocalCellRect ( inCell, localRect );
	Rect textRect = ComputeTextRect(iconAndName,localRect);
	::ClipRect(&textRect);
	::RGBBackColor(&bgColor);
	::EraseRect(&textRect);
	DrawCell(inCell, localRect);

} // RedrawCellWithTextClipping


//
// GetInfoForPPColumn
//
// Given a column in a powerplant table, this returns a reference to the appropriate CUserButtonInfo
// class for that column. 
inline const CUserButtonInfo & 
CPersonalToolbarTable :: GetInfoForPPColumn ( const TableIndexT & inCol ) const
{
	// recall PP is 1-based and HT (and vectors) are 0 based, so do the conversion first
	return (*mButtonList)[URDFUtilities::PPRowToHTRow(inCol)];

} // GetInfoForPPColumn


#pragma mark -

///////////////////////////////////////////////////////////////////////////////////
//									Class CUserButtonInfo
///////////////////////////////////////////////////////////////////////////////////

CUserButtonInfo::CUserButtonInfo( const string & pName, const string & pURL, Uint32 nBitmapID,
									 Uint32 nBitmapIndex, bool bIsFolder, HT_Resource inRes )
	: mName(pName), mURL(pURL), mBitmapID(nBitmapID), mBitmapIndex(nBitmapIndex), 
		mIsResourceID(true), mIsFolder(bIsFolder), mResource(inRes)
{
}


CUserButtonInfo :: CUserButtonInfo ( )
	: mBitmapID(0), mBitmapIndex(0), mIsResourceID(false), mIsFolder(false), mResource(NULL)
{

}


CUserButtonInfo::~CUserButtonInfo()
{
	// string destructor takes care of everything...
}

