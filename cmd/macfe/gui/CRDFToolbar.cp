/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "CRDFToolbar.h"
#include "CRDFToolbarItem.h"
#include "URDFUtilities.h"

#include <cassert>
#include "htrdf.h"

#include "vocab.h"							// provides tokens needed in lookup functions
	// ...and because vocab.h mistakenly does not declare these, we must
extern RDF_NCVocab		gNavCenter;
extern RDF_CoreVocab	gCoreVocab;


	/*
		The functions |pane_params_from|, |view_params_from|, and |is_docked| should be
		within an anonymous namespace.
	*/

static
SPaneInfo
pane_params_from( HT_View /*ht_view*/, LView* pp_superview )
	{
		SPaneInfo info;

		info.paneID			= 0;

		SDimension16 superview_size;
		pp_superview->GetFrameSize(superview_size);
		info.width			= superview_size.width;

		info.height			= 55; // NO! Get this value from the |HT_View|.
		info.visible		= false;		// we'll get shown when bar is added.
		info.enabled		= true;
		
		SBooleanRect bindings = { true, true, true, false };
		info.bindings	= bindings;

		info.left				= 0;
		info.top				= 0;

		info.userCon		= 0;
		info.superView	= pp_superview;

		return info;
	}

static
SViewInfo
view_params_from( HT_View /*ht_view*/ )
	{
		SViewInfo info;

		SDimension32 image_size = { 0, 0 };
		info.imageSize = image_size;

		SPoint32 scroll_pos = { 0, 0 };
		info.scrollPos = scroll_pos;

		SPoint32 scroll_unit = { 1, 1 };
		info.scrollUnit = scroll_unit;

		info.reconcileOverhang = 0;

		return info;
	}

static
bool
is_docked( HT_View /*ht_view*/ )
	{
		return false;
	}



CRDFToolbar::CRDFToolbar( HT_View ht_view, LView* pp_superview )
		:	CDragBar( pane_params_from(ht_view, pp_superview), view_params_from(ht_view), is_docked(ht_view) ),
			_ht_view(ht_view)
	{
		assert( _ht_view );											// There must be an |HT_View|...
		assert( !HT_GetViewFEData(_ht_view) );	//	...and it must not be linked to any other FE object.

		// the toolbar can stream in at any time, so we need to make sure that the container gets
		// updated even after it has been initialized. Setting the |available| flag to false ensures
		// that the container will update correctly when it sees this toolbar.
 		SetAvailable(false);
		
		HT_SetViewFEData(_ht_view, this);

			// TO BE FIXED: 1103 needs a better name and visibility
			// TO BE FIXED: is there a better way to insert the grippy pane?

		LWindow* window = LWindow::FetchWindowObject(pp_superview->GetMacPort());
		UReanimator::CreateView(1103, this, window);	// create the CPatternedGrippyPane
#if 0
		LView* view = UReanimator::CreateView(1104, this, window);	// create the CToolbarPatternBevelView
		view->ResizeFrameBy(-12, 0, false);
#endif

		notice_background_changed();

		// The top node of the toolbar may or may not be open (sigh). Make sure it _is_ open
		// and then fill in our toolbars from the contents.
		HT_SetOpenState(TopNode(), PR_TRUE);
		FillInToolbar();
	}

CRDFToolbar::~CRDFToolbar()
	{
		assert( _ht_view );											// There must be an |HT_View|...

		HT_SetViewFEData(_ht_view, 0);
	}


//
// FillInToolbar
//
// Loop through HT and create buttons for each item present. Once we build them all, 
// lay them out. 
//
// Note: We will certainly get more buttons streaming in, so we'll have to
// pitch our layout and re-layout again, but they may not stream in for a while so 
// we still should layout what we have (WinFE has this same problem and hyatt
// and I have yelled at rjc about it...but to no avail).
//
void
CRDFToolbar :: FillInToolbar ( )
{
	assert(HTView() && TopNode());
	
	HT_Cursor cursor = HT_NewCursor(TopNode());
	if (cursor == NULL)
		return;

	HT_Resource item = NULL;
	while (item = HT_GetNextItem(cursor))
		AddHTButton(item);

	HT_DeleteCursor(cursor);

	LayoutButtons();
	
} // FillInToolbar


//
// LayoutButtons
//
// Do the work to layout the buttons
//
void
CRDFToolbar :: LayoutButtons ( )
{
	Uint32 horiz = 20;

	bool iconOnTop = false;
	char* value = NULL;
	PRBool success = HT_GetTemplateData ( TopNode(), gNavCenter->toolbarBitmapPosition, HT_COLUMN_STRING, &value );
	if ( success && value ) {
		if ( strcmp(value, "top") == 0 )
			iconOnTop = true;
	}
	
	HT_Cursor cursor = HT_NewCursor(TopNode());
	if (cursor == NULL)
		return;

	HT_Resource item = NULL;
	while (item = HT_GetNextItem(cursor)) {
		CRDFToolbarItem* button = reinterpret_cast<CRDFToolbarItem*>(HT_GetNodeFEData(item));
		Uint32 hDelta = 0;
		if ( button ) {
			if ( iconOnTop ) {
				button->ResizeFrameTo ( 50, 50, false );
				button->PlaceInSuperFrameAt ( horiz, 5, false );
				hDelta = 50;
			}
			else {
				button->ResizeFrameTo ( 80, 20, false );
				button->PlaceInSuperFrameAt ( horiz, 5, false );
				hDelta = 80;
			}			
		}
		horiz += hDelta;
	}
	Refresh();

	HT_DeleteCursor(cursor);
	
} // LayoutButtons


//
// AddHTButton
//
// Make a button that corresponds to the given HT_Resource. The button can be
// one of numerous types, including things like separators, throbbers, or
// the url entry field.
//
void
CRDFToolbar :: AddHTButton ( HT_Resource inNode )
{
	string nodeName = HT_GetNodeName(inNode);
	string commandURL = HT_GetNodeURL(inNode);
	
//	DebugStr(LStr255(nodeName.c_str()));
//	DebugStr(LStr255(commandURL.c_str()));
		
	CRDFToolbarItem* newItem = NULL;
	if (HT_IsURLBar(inNode))
		newItem = new CRDFURLBar(inNode);
	else if (HT_IsSeparator(inNode))
		newItem = new CRDFSeparator(inNode);
	else newItem = new CRDFPushButton(inNode);

	if ( newItem ) {
		newItem->PutInside ( this );
		newItem->ResizeFrameTo ( 50, 50, false );		// give it a default size
	}
	
/*
	pButton->Create(this, GetDisplayMode(), CSize(60,42), CSize(85, 25), csAmpersandString,
					tooltipText, statusBarText, CSize(23,17), 
					m_nMaxToolbarButtonChars, m_nMinToolbarButtonChars, bookmark,
					item, (HT_IsContainer(item) ? TB_HAS_DRAGABLE_MENU | TB_HAS_IMMEDIATE_MENU : 0));
*/

	HT_SetNodeFEData(inNode, newItem);
	
	//еее deal with computing height/width??? They do on WinFE
	
} // AddHTButton


void
CRDFToolbar::Draw( RgnHandle inSuperDrawRgnH )
	{
			// We don't like the way |CDragBar| does it
		LView::Draw(inSuperDrawRgnH);
	}

void
CRDFToolbar::DrawSelf()
	{
		Rect frame;
		if ( CalcLocalFrameRect(frame) )
			{
				Point top_left = { frame.top, frame.left };
				DrawImage(top_left, kTransformNone, frame.right-frame.left, frame.bottom-frame.top);
			}
		else
			EraseBackground();
		// Note: I don't want |CDragBar::DrawSelf()|s behavior, and |LView| doesn't implement
		//	|DrawSelf|, so, nothing else to do here.
	}

void
CRDFToolbar::ImageIsReady()
	{
		Refresh();
	}


//
// DrawStandby
//
// Called when the bg image is not present (or there isn't one). Needs to repaint the background the
// appropriate color.
//
void
CRDFToolbar::DrawStandby( const Point&, const IconTransformType ) const
{
	EraseBackground();
}


//
// EraseBackground
//
// Draw the bg of the toolbar to match what HT tells us. We know at this point that we don't have
// a bg image, else it would have been drawn already.
//
void
CRDFToolbar :: EraseBackground ( ) const
{
	Rect backRect = { 0, 0, mFrameSize.height, mFrameSize.width };
	
	URDFUtilities::SetupBackgroundColor ( TopNode(), gNavCenter->viewBGColor, kThemeListViewBackgroundBrush );
	::EraseRect(&backRect);

} // EraseBackground


void
CRDFToolbar::HandleNotification( HT_Notification, HT_Resource inNode, HT_Event event, void* /*inToken*/, uint32 /*inTokenType*/ )
	{
		switch ( event )
			{
				case HT_EVENT_NODE_ADDED:
					AddHTButton(inNode);
					LayoutButtons();
					break;
					
				case HT_EVENT_NODE_VPROP_CHANGED:
					notice_background_changed();
					break;
			}
	}

void
CRDFToolbar::notice_background_changed()
	{
		char* cp = 0;
		if ( HT_GetTemplateData(TopNode(), gNavCenter->viewBGURL, HT_COLUMN_STRING, &cp) )
			SetImageURL(string(cp));
	}

