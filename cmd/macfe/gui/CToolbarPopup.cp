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

#include "CToolbarPopup.h"

// this class overrides LGAPopup in order to draw popup menus in the editor and compose
// windows in a dimmed state when the application is switched out.

CToolbarPopup::CToolbarPopup( LStream *inStream ) : LGAPopup( inStream )
{
}


CToolbarPopup::~CToolbarPopup()
{
}


void CToolbarPopup::DrawSelf()
{

	StColorPenState::Normalize ();
	
	// ¥ Get the control drawn in its various states
	if ( IsEnabled () && IsActive () )
	{
		if ( IsHilited ())
			DrawPopupHilited ();
		else
			DrawPopupNormal ();
	}
	else
		DrawPopupDimmed ();

	// ¥ Get the arrow drawn
	DrawPopupArrow ();
	
	// ¥ Draw the popup Label
	if ( !IsArrowOnly ())
		DrawPopupLabel ();

	// ¥ Get the title for the popup drawn
	if ( !IsArrowOnly ())
		DrawPopupTitle ();
	
}	//	LGAPopup::DrawSelf


void CToolbarPopup::ActivateSelf()
{
	// ¥ Get things redrawn so that we can see the state change
	Draw ( nil );
}


void CToolbarPopup::DeactivateSelf()
{
	// ¥ Get things redrawn so that we can see the state change
	Draw ( nil );
}


#pragma mark -


CIconToolbarPopup::CIconToolbarPopup( LStream *inStream ) : CGAIconPopup( inStream )
{
}


CIconToolbarPopup::~CIconToolbarPopup()
{
}


void CIconToolbarPopup::DrawSelf()
{
	StColorPenState::Normalize ();
	
	// ¥ Get the control drawn in its various states
	if ( IsEnabled() && IsActive())
	{
		if ( IsHilited())
			DrawPopupHilited();
		else
			DrawPopupNormal();
	}
	else
		DrawPopupDimmed();

	// ¥ Get the arrow drawn
	DrawPopupArrow();
	
	// ¥ Draw the popup Label
	if ( !IsArrowOnly())
		DrawPopupLabel();

	// ¥ Get the title for the popup drawn
	if ( !IsArrowOnly())
		DrawPopupTitle();
	
}
	

// override since we don't want to use the constants but the actual size of the icon
void CIconToolbarPopup::DrawPopupTitle(void)
{
	LGAPopup::DrawPopupTitle();
	
	Int16 iconID = GetTitleIconID();
	
	if ( iconID != 0 ) {
	
		CIconHandle theIconH = ::GetCIcon(iconID);
		
		if ( theIconH != nil ) {
			Rect iconRect;
			LGAPopup::CalcTitleRect(iconRect);
			
			Rect trueSize = (**theIconH).iconPMap.bounds;
			if ( EmptyRect( &trueSize ) )
				trueSize = (**theIconH).iconBMap.bounds;
			
			// if we still don't have a good rect, bail out--nothing to draw
			if ( EmptyRect( &trueSize ) )
				return;
			
			iconRect.right = iconRect.left + (trueSize.right - trueSize.left);
			iconRect.top = iconRect.top + ((iconRect.bottom - iconRect.top - (trueSize.bottom - trueSize.top)) / 2);
			iconRect.bottom = iconRect.top + trueSize.bottom - trueSize.top;
			
			Int16 transform = ttNone;
			
			if ( IsEnabled() ) {
				if ( IsHilited() ) {
					transform = ttSelected;
				}
			} else {
				transform = ttDisabled;
			}
		
			::PlotCIconHandle(&iconRect, ttNone, transform, theIconH);
			
			::DisposeCIcon(theIconH);
		}
	}
}


void CIconToolbarPopup::ActivateSelf()
{
	// ¥ Get things redrawn so that we can see the state change
	Draw ( nil );
}

void CIconToolbarPopup::DeactivateSelf()
{
	// ¥ Get things redrawn so that we can see the state change
	Draw ( nil );
}

