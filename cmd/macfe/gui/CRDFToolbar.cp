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

#include <vector>
#include <cassert>
#include "htrdf.h"
#include "CPaneEnabler.h"

#include "vocab.h"							// provides tokens needed in lookup functions
	// ...and because vocab.h mistakenly does not declare these, we must
extern RDF_NCVocab		gNavCenter;
extern RDF_CoreVocab	gCoreVocab;





namespace
	{
			// BULLSHIT ALERT: the following magic numbers should be derived from HT
		const SInt16 TOOLBAR_HORIZONTAL_PADDING	= 2;
		const SInt16 TOOLBAR_VERTICAL_PADDING		= 2;
		const SInt16 TOOLBAR_INITIAL_VERTICAL_OFFSET = 14;

		struct item_spec
			{
				item_spec()
					{
						// nothing else to do
					}

				item_spec( CRDFToolbarItem& item, SDimension16 available_space )
						: _item( &item ),
							_size( item.NaturalSize(available_space) ),
							_is_stretchy( item.IsStretchy() )
					{
						// nothing else to do
					}

				CRDFToolbarItem*	_item;
				SDimension16			_size;
				bool							_is_stretchy;
			};

		typedef std::vector<item_spec> item_list;


		void
		layout_row( item_list::iterator first, item_list::iterator last, SInt16 inLeft, SInt16 inBottom, SInt16 stretchy_space )
			{
				for ( ; first != last; ++first )
					{
						if ( first->_is_stretchy )
							first->_size.width += stretchy_space;

						first->_item->ResizeFrameTo(first->_size.width, first->_size.height, false);
						first->_item->PlaceInSuperFrameAt(inLeft, inBottom-first->_size.height, false);

						inLeft += first->_size.width + TOOLBAR_HORIZONTAL_PADDING;
					}
			}

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
				info.enabled		= false;
				
				SBooleanRect bindings = { true, true, true, false };
				info.bindings	= bindings;

				info.left				= 0;
				info.top				= 0;

				info.userCon		= 0;
				info.superView	= pp_superview;

				return info;
			}

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

		bool
		is_docked( HT_View /*ht_view*/ )
			{
				return false;
			}


	} // namespace




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
		
		// when the toolbars stream in, we need to create them as invisible, disabled, and
		// inactive. The first two are handled above in pane_params_from() so we need to
		// make sure it is deactivated. All three properties will be adjusted to their
		// correct settings in EnableSelf() below, which is called when the toolbar is
		// added to the container.
		Deactivate();
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
	while ( (item = HT_GetNextItem(cursor)) != 0 )
		AddHTButton(item);

	HT_DeleteCursor(cursor);

	LayoutButtons();
	
} // FillInToolbar




void
CRDFToolbar::LayoutButtons()
		/*
			...some property has changed that may effect the layout of the items within me.
			I need to re-calculate the layout of my contents.

			TO DO: make sure this doesn't get called recursively, i.e., when it resizes itself.
				No harm done, just wasted work.

			TO DO: make and use an |auto_ptr<_HT_Cursor>|.
		*/
	{
			// loop over the items, if we can
		if ( HT_Cursor cursor = HT_NewCursor(TopNode()) )
			{
				SDimension16 toolbar_size;
				GetFrameSize(toolbar_size);
					// BULLSHIT ALERT: do I need to account for the grippy pane?

				SInt16 row_bottom = 0;

				SInt16 row_height = 0;
				SInt16 width_available = toolbar_size.width;
				size_t number_of_stretchy_items = 0;

				item_list row;
				row.reserve(32); // reserve room for enough items to make allocation unlikely

				while ( HT_Resource resource = HT_GetNextItem(cursor) )
					if ( CRDFToolbarItem* toolbar_item = reinterpret_cast<CRDFToolbarItem*>(HT_GetNodeFEData(resource)) )
						{
							item_spec spec(*toolbar_item, toolbar_size);

								// If the current item is too big to fit in this row...
							if ( spec._size.width > width_available )
								{
										// ...we're done with this row.  Lay it out.
									layout_row(row.begin(), row.end(), TOOLBAR_INITIAL_VERTICAL_OFFSET, row_bottom+=(TOOLBAR_VERTICAL_PADDING+row_height), width_available / number_of_stretchy_items);

										// And start accumulating a new row.
									row.resize(0);
									row_height = 0;
									width_available = toolbar_size.width;
									number_of_stretchy_items = 0;
								}

								// Add this item to the row we're currently accumulating.
							row.push_back(spec);

								// ...and account for its size.
							width_available -= (spec._size.width + TOOLBAR_HORIZONTAL_PADDING);
							if ( spec._size.height > row_height )
								row_height = spec._size.height;	// this item makes the row taller
							if ( spec._is_stretchy )
								++number_of_stretchy_items;
						}

					// The final row comprises all remaining accumulated items.  Lay it out.
				layout_row(row.begin(), row.end(), TOOLBAR_INITIAL_VERTICAL_OFFSET, row_bottom+=(TOOLBAR_VERTICAL_PADDING+row_height), width_available / number_of_stretchy_items);
				if ( toolbar_size.height != (row_bottom += TOOLBAR_VERTICAL_PADDING) )
					ResizeFrameTo(toolbar_size.width, row_bottom, false);

				HT_DeleteCursor(cursor);
			}
	}






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
	
	// IMPORTANT: these must be done AFTER PutInside()
	newItem->FinishCreate();
	newItem->HookUpToListeners();				
	
	HT_SetNodeFEData(inNode, newItem);
	
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


void
CRDFToolbar::DrawStandby( const Point&, const IconTransformType ) const
		// Called to take alternative action when the BG image is not ready, or does not exist.
	{
		EraseBackground();
	}


void
CRDFToolbar::EraseBackground() const
		// Erase to the HT supplied color.  We know we don't have an image, or it would have been drawn already.
	{
		Rect backRect = { 0, 0, mFrameSize.height, mFrameSize.width };
		
		URDFUtilities::SetupBackgroundColor ( TopNode(), gNavCenter->viewBGColor, kThemeListViewBackgroundBrush );
		::EraseRect(&backRect);
	}


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


void
CRDFToolbar::ShowSelf ( )
{
	CDragBar::ShowSelf();
	Enable();
	Activate();
	
} // ShowSelf