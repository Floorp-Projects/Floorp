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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//		Simplest divided view. Just takes in the panes as the parameters
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "prtypes.h"

#include "CSimpleDividedView.h"

// PP

#include <URegions.h>
#include <UDebugging.h>
#include <UCursor.h>

#ifndef VERT_CURSOR 
#define VERT_CURSOR 1136
#endif

#ifndef HORI_CURSOR
#define HORI_CURSOR 1120
#endif

//#define DRAW_DEBUG_RECT

// ---------------------------------------------------------------------------
//		¥ CSimpleDividedView constructor 
// ---------------------------------------------------------------------------
// Read in the data

CSimpleDividedView::CSimpleDividedView(LStream *inStream) : LPane(inStream)
{
	*inStream >> fTopLeftViewID;
	*inStream >> fBottomRightViewID;
	*inStream >> fTopLeftMinSize;
	*inStream >> fBottomRightMinSize;
	fIsVertical = -1;
}

// ---------------------------------------------------------------------------
//		¥ CSimpleDividedView destructor 
// ---------------------------------------------------------------------------
CSimpleDividedView::~CSimpleDividedView()
{
}

// ---------------------------------------------------------------------------
//		¥ RepositionView 
// ---------------------------------------------------------------------------
// Squeze ourselves between topLeft and bottomRight view

void CSimpleDividedView::RepositionView()
{
	Rect	r1, r2, thisRect;
	
	if ( !GetViewRects(r1, r2) )
	{
		Hide();
		return;
	}
	
	Show();

	if (fIsVertical == -1)
		ReadjustConstraints();
	if (fIsVertical)
	{
		// Get the smallest rectangle between the two
		thisRect.top =  r1.bottom;
		thisRect.bottom = r2.top;
		thisRect.left = r1.left > r2.left ? r1.left : r2.left;
		thisRect.right = r1.right < r2.right ? r1.right : r2.right;
		if ( thisRect.top == thisRect.bottom )
		{
			thisRect.top -= 1;
			thisRect.bottom += 1;
		}
	}
	else
	{
		thisRect.left = r1.right;
		thisRect.right = r2.left;
		thisRect.top = r1.top > r2.top ? r1.top : r2.top;
		thisRect.bottom = r1.bottom < r2.bottom ? r1.bottom : r2.bottom;
		if ( thisRect.left == thisRect.right )
		{
			thisRect.left -= 1;
			thisRect.right += 1;
		}
	}
	MoveBy( thisRect.left - mFrameLocation.h, thisRect.top - mFrameLocation.v, false );
	ResizeFrameTo( thisRect.right - thisRect.left, thisRect.bottom - thisRect.top , false);
}

// ---------------------------------------------------------------------------
//		¥ GetViewRects 
// ---------------------------------------------------------------------------
// r1 is the fTopLeftViewID rect
// r2 is the fBottomRightViewID rect

Boolean CSimpleDividedView::GetViewRects(Rect& r1, Rect& r2)
{
	LPane * pane1, * pane2;
	
	// Get the location of the views
	pane1 = mSuperView->FindPaneByID( fTopLeftViewID );
	SignalIfNot_(pane1);

	pane2 = mSuperView->FindPaneByID( fBottomRightViewID );
	SignalIfNot_(pane2);
	
	if ( pane1->IsVisible() && pane2->IsVisible() )
	{
		SDimension16 size1, size2;
		SPoint32 loc1, loc2;

		pane1->GetFrameSize( size1 );
		pane2->GetFrameSize( size2 );
		
		pane1->GetFrameLocation( loc1 );
		pane2->GetFrameLocation( loc2 );
		
		::SetRect( &r1, loc1.h, loc1.v, loc1.h+size1.width, loc1.v + size1.height );
		::SetRect( &r2, loc2.h, loc2.v, loc2.h+size2.width, loc2.v + size2.height );
		return true;
	}
	else
		return false;
}

// ---------------------------------------------------------------------------
//		¥ ReadjustConstraints 
// ---------------------------------------------------------------------------
// Figure out which is the topLeft view
// Readjust our position

void CSimpleDividedView::ReadjustConstraints()
{

	Rect	r1, r2;

	if ( !GetViewRects(r1, r2 ) )
		return;
	// Are we vertical drag?
	// If so, top and bottom of top pane are less than top of other pane
	
	if ( (r1.top < r2.top ) && ( r1.bottom < r2.top ) )
	// r1 on top of r2
	{
		fIsVertical = TRUE;
	}
	else if ( (r2.top < r1.top ) && ( r2.bottom < r1.top ) )
	// r2 on top of r1
	{
		fIsVertical = TRUE;
		PaneIDT temp = fTopLeftViewID;
		fTopLeftViewID = fBottomRightViewID;
		fBottomRightViewID = temp;
	} else if ( (r1.left < r2.left ) && ( r1.left < r2.bottom ) )
	// r1 left of r2
	{
		fIsVertical = FALSE;
	} else if ( (r2.left < r1.left ) && ( r2.left < r1.bottom ) )
	{
	// r2 to left of r1
		fIsVertical = FALSE;
		PaneIDT temp = fTopLeftViewID;
		fTopLeftViewID = fBottomRightViewID;
		fBottomRightViewID = temp;
	}
	else
		SignalCStr_("The views are not properly adjusted");
		
// Readjust our position
	RepositionView();
}

// ---------------------------------------------------------------------------
//		¥ FinishCreateSelf 
// ---------------------------------------------------------------------------
// Position this view

void CSimpleDividedView::FinishCreateSelf()
{
	ReadjustConstraints();
}

// ---------------------------------------------------------------------------
//		¥ AdaptToSuperFrameSize
// ---------------------------------------------------------------------------
// Do not want to move the view around 
void CSimpleDividedView::AdaptToSuperFrameSize(Int32 /* inSurrWidthDelta */,
										Int32 /* inSurrHeightDelta */,
										Boolean /* inRefresh */)
{
	ReadjustConstraints();
}

// ---------------------------------------------------------------------------
//		¥ ClickSelf
// ---------------------------------------------------------------------------
// Drag the bar within the limits
// Readjust the subview size

void CSimpleDividedView::ClickSelf(const SMouseDownEvent	&inMouseDown)
{

	long	dragDistance;
	Rect limitRect, slopRect, frame;
	
	FocusDraw();
	CalcLocalFrameRect(frame);
	StRegion rgn(frame);
	
	// Figure out the limits, taking minimum size into account
	{
		Rect r1, r2;
		if ( !GetViewRects(r1, r2) )
			return;
		if (fIsVertical)
		{
			limitRect.top = r1.top + fTopLeftMinSize;
			limitRect.bottom = r2.bottom - fBottomRightMinSize;
			limitRect.left = frame.left;
			limitRect.right = frame.right;

			slopRect.left = limitRect.left - 50;
			slopRect.right = limitRect.right + 50;
			slopRect.top = r1.top - 30;
			slopRect.bottom = r2.bottom + 30;
		}
		else
		{
			limitRect.left = r1.left + fTopLeftMinSize;
			limitRect.right = r2.right - fBottomRightMinSize;
			limitRect.top = r1.top;
			limitRect.bottom = r2.bottom;
			slopRect = limitRect;
			slopRect.left -= fTopLeftMinSize;
			slopRect.right += fBottomRightMinSize;
			slopRect.bottom += 50;
			slopRect.top -= 50;
		}
	}
	
	// Drag it
	dragDistance = ::DragGrayRgn(	rgn, inMouseDown.whereLocal, 
								&limitRect, &slopRect, 
								fIsVertical ? vAxisOnly : hAxisOnly, nil );
	
	Int16	horizDrag = LoWord(dragDistance);
	Int16	vertDrag = HiWord(dragDistance);
	
	// Move/resize the views
	if ( (dragDistance != 0x80008000)  &&	// 0x80008000 == Drag_Aborted from UFloatingDesktop
		 (horizDrag != 0 || vertDrag != 0) ) 
	{
		LPane * p1, *p2;
		p1 = mSuperView->FindPaneByID(fTopLeftViewID);
		p2 = mSuperView->FindPaneByID(fBottomRightViewID);
		
		p1->ResizeFrameBy( horizDrag, vertDrag, true );
		p2->MoveBy( horizDrag, vertDrag, true );
		p2->ResizeFrameBy( -horizDrag, -vertDrag, true );
		
		RepositionView();
		
		BroadcastMessage(msg_SubviewChangedSize, this);		
	}
}

// ---------------------------------------------------------------------------
//		¥ AdjustCursorSelf
// ---------------------------------------------------------------------------
// Standard vert/horizontal sliders

void CSimpleDividedView::AdjustCursorSelf(Point /* inPortPt */, const EventRecord& /* inMacEvent */ )
{
	ResIDT cursID = fIsVertical ? VERT_CURSOR : HORI_CURSOR;
	CursHandle h = GetCursor (cursID);
	if (h)
		SetCursor( *h );
	else
		SignalCStr_("Signal those strings");
}

// ---------------------------------------------------------------------------
//		¥ DrawSelf
// ---------------------------------------------------------------------------
// Debug only, draw a big red slab
#ifdef DEBUG
void CSimpleDividedView::DrawSelf()
{
#ifdef DRAW_DEBUG_RECT
	Rect	frame;
	CalcLocalFrameRect(frame);
	RGBColor c;
	c.red = 0xDD6B; c.green = 0x08C2; c.blue = 0x06A2;
	RGBForeColor (&c);
	PaintRect(&frame);
#endif
}
#endif
