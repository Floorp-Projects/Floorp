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

// CToolbarPatternBevelView.cp

#include <LArrayIterator.h>

#include "CToolbarPatternBevelView.h"
#include "CToolbarBevelButton.h"
#include "StRegionHandle.h"

#include <stddef.h>

// Bug #79175
// Change the update command status so that the "Show/Hide Component Bar" works
CToolbarPatternBevelView::CToolbarPatternBevelView(LStream* inStream)
:	CAMSavvyBevelView(inStream)
{
	LCommander::SetUpdateCommandStatus(true);
}

// Bug #79175
// Change the update command status so that the "Show/Hide Component Bar" works
CToolbarPatternBevelView::~CToolbarPatternBevelView()
{
	LCommander::SetUpdateCommandStatus(true);
}

void CToolbarPatternBevelView::HandleModeChange( Int8 inNewMode, SDimension16& outSizeChange )
	{
		CalcArrangement(false, inNewMode, outSizeChange);
	}

void CToolbarPatternBevelView::RotateArrangement( SDimension16& outSizeChange )
	{
		CalcArrangement(true, Int8(), outSizeChange);
	}

void CToolbarPatternBevelView::CalcArrangement( Boolean inRotateArrangement, Int8 inNewMode, SDimension16& outSizeChange)
	/*
		Note: either rotates the arrangement _or else_ changes the mode, but not both.

		...treats |CToolbarButtons| sub-views as though they are layed out in a grid
		whose cell-size is the maximum height and width of any of them.  In a mode change,
		their sizes change, the grid must be recalculated, and the buttons moved
		appropriately.
	*/
{
	size_t				number_of_buttons = 0;
	SDimension16	old_cell_size={ 0, 0 }, new_cell_size={ 0, 0 };
	SDimension16	grid_origin;

		/*
			Iterate over the |CToolbarButtons| within me, _twice_...
		*/

		// ...once to calculate the old and new grid cell-size, and change the mode of each button;
	LPane* current_subview_ptr = 0;
	for ( LArrayIterator iter(mSubPanes); iter.Next(&current_subview_ptr); )
		if ( CToolbarBevelButton* button = dynamic_cast<CToolbarBevelButton*>(current_subview_ptr) )
			{
				++number_of_buttons;

				SDimension16 button_size;

					// Use the pre-ChangeMode size of this button to calculate |old_cell_size|
				button->GetFrameSize(button_size);


					/*
						The origin of the grid is the remainder when dividing the maximum sized
						cells offset by its size (independently for x and y).
					*/

				Boolean recalc_origin_x = button_size.width > old_cell_size.width;
				Boolean recalc_origin_y = button_size.height > old_cell_size.height;

				if ( recalc_origin_x || recalc_origin_y )
					{
						Rect button_rect;
						button->CalcLocalFrameRect(button_rect);

						if ( recalc_origin_x )
							{
								old_cell_size.width = button_size.width;
								grid_origin.width = button_rect.left - (button_size.width * (button_rect.left / button_size.width));
							}
						else // recalc_origin_y
							{
								old_cell_size.height = button_size.height;
								grid_origin.height = button_rect.top - (button_size.height * (button_rect.top / button_size.height));
							}
					}


				if ( !inRotateArrangement )
					{
						SDimension16 unused;
						button->ChangeMode(inNewMode, unused);

							// ...and the post-ChangeMode size for |new_cell_size|
						button->GetFrameSize(button_size);
						if ( new_cell_size.width < button_size.width )
							new_cell_size.width = button_size.width;
						if ( new_cell_size.height < button_size.height )
							new_cell_size.height = button_size.height;
					}
			}

	if ( inRotateArrangement )
		new_cell_size = old_cell_size;


	Int16 number_of_columns	= 0;
	Int16 number_of_rows		= 0;

		// ...and again (if buttons were found in the first pass) to actually move them.
	if ( number_of_buttons > 0 )
		{
			for ( LArrayIterator iter(mSubPanes); (number_of_buttons > 0) && iter.Next(&current_subview_ptr); )
				if ( CToolbarBevelButton* button = dynamic_cast<CToolbarBevelButton*>(current_subview_ptr) )
					{
						--number_of_buttons;

							// Determine which cell this button occupies.  It hasn't been moved yet, so use |old_cell_size|, and the buttons top-left.
						Rect button_rect;
						button->CalcLocalFrameRect(button_rect);
						Int16 column	= (button_rect.left - grid_origin.width) / old_cell_size.width;
						Int16 row			= (button_rect.top - grid_origin.height) / old_cell_size.height;

						if ( inRotateArrangement )
							{
								Int16 temp = column;
								column = row;
								row = temp;
							}

						if ( number_of_columns <= column )
							number_of_columns = column + 1;
						if ( number_of_rows <= row )
							number_of_rows = row + 1;

							// Calculate the padding needed to center this button within a (new-size) cell.
						SDimension16 button_size;
						button->GetFrameSize(button_size);
						Int16	horizontal_padding	= (new_cell_size.width - button_size.width) / 2;
						Int16 vertical_padding		= (new_cell_size.height - button_size.height) / 2;

							// Calculate the buttons new position within its superview (i.e., within |me|)
						Int32 x = grid_origin.width + (column * new_cell_size.width) + horizontal_padding;
						Int32 y = grid_origin.height + (row * new_cell_size.height) + vertical_padding;

						button->PlaceInSuperFrameAt(x, y, true);	// ...only moves if it has to
					}
		}

	Int16 old_number_of_columns	= inRotateArrangement ? number_of_rows : number_of_columns;
	Int16 old_number_of_rows		= inRotateArrangement ? number_of_columns : number_of_rows;

	outSizeChange.width		= (number_of_columns	* new_cell_size.width)	- (old_number_of_columns	* old_cell_size.width);
	outSizeChange.height	= (number_of_rows			* new_cell_size.height)	- (old_number_of_rows			* old_cell_size.height);
}

#pragma mark -

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FocusDraw
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Set up coordinates system and clipping region
//
//	Panes rely on their superview to focus them. When a Pane requests
//	focus for drawing, we set the clipping region to the revealed
//	portion of that Pane's Frame minus the Frames of all sibling Panes
//	that are in front of that Pane.

Boolean
CToolbarPatternBevelView::FocusDraw(
	LPane	*inSubPane)
{
	Boolean		revealed = true;
	
	if (inSubPane == nil) {			// Focus this view
		revealed = LView::FocusDraw();
		
	} else {						// Focus a SubPane
	
		if (EstablishPort()) {		// Set current Mac Port	
			sInFocusView = nil;		// Saved focus is now invalid
				
									// Set up local coordinate system
			::SetOrigin(mPortOrigin.h, mPortOrigin.v);
			
									// Build clipping region
									
									// Start with the intersection of the
									//   revealed rect of this View with the
									//   Frame of the SubPane
			Rect	subRect;
			inSubPane->CalcPortFrameRect(subRect);
			
			if (!::SectRect(&subRect, &mRevealedRect, &subRect)) {
									// No intersection, so subpane is
									//   not revealed. Set empty clip.
				::ClipRect(&subRect);
				return false;
			}
			
									// SubPane is revealed. Make region
									//   from the intersection.
			StRegionHandle	clipR;
			::RectRgn(clipR, &subRect);
			
				// Determine first sibling which is in front of this SubPane.
			ArrayIndexT		subIndex = 0;			
			LPane*			thePane = nil;
			
			{
				LArrayIterator	iterator(mSubPanes, LArrayIterator::from_Start);
				while (iterator.Next(&thePane))
				{	
					subIndex++;
					if (thePane == inSubPane)
						break;
				}
			}

				// Subtract Frames of all sibling Panes (which are visible)
				// that are in front of this SubPane (i.e., come after the
				// SubPane in this View's list of SubPanes.
			
			StRegionHandle	siblingR;
			LArrayIterator	iterator(mSubPanes, subIndex);
			Rect			siblingFrame;
			
			while (iterator.Next(&thePane))
			{					
				if (thePane->IsVisible() && thePane->CalcPortFrameRect(siblingFrame))
				{
									// Subtract sibling's Frame from
									//   the clipping region
					::RectRgn(siblingR, &siblingFrame);
					::DiffRgn(clipR, siblingR, clipR);
				}
			}

									// Convert Clip region from Port to
									//   Local coords and set it
			::OffsetRgn(clipR, mPortOrigin.h, mPortOrigin.v);
			::SetClip(clipR);
			
			revealed = !::EmptyRgn(clipR);

		} else {
			SignalPStr_("\pFocus View with no GrafPort");
			revealed = false;
		}
	
	}
	
	return revealed;
}
