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


/*====================================================================================*/
	#pragma mark INCLUDE FILES
/*====================================================================================*/

#include "CSizeBox.h"


/*====================================================================================*/
	#pragma mark TYPEDEFS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CONSTANTS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL CLASS DECLARATIONS
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark INTERNAL FUNCTION PROTOTYPES
/*====================================================================================*/


/*====================================================================================*/
	#pragma mark CLASS IMPLEMENTATIONS
/*====================================================================================*/

#pragma mark -

/*======================================================================================
	Broadcast a message.
======================================================================================*/

void CSizeBox::ClickSelf(const SMouseDownEvent &inMouseDown) {

	// Find the window object
	LWindow *theWindow = LWindow::FetchWindowObject(GetMacPort());
	
	// Click in its grow
	theWindow->ClickInGrow(inMouseDown.macEvent);
}


/*======================================================================================
	Redraw the control.
======================================================================================*/

void CSizeBox::ActivateSelf(void) {

	inherited::ActivateSelf();
	
	if ( FocusExposed() ) {
		DrawSelf();
	}
}


/*======================================================================================
	Redraw the control.
======================================================================================*/

void CSizeBox::DeactivateSelf(void) {

	inherited::ActivateSelf();
	
	if ( FocusExposed() ) {
		DrawSelf();
	}
}


/*======================================================================================
	Draw the control.
======================================================================================*/

void CSizeBox::DrawSelf(void) {

	Rect frame;
	if ( !CalcLocalFrameRect(frame) ) return;
	
	ApplyForeAndBackColors();

	if ( IsActive() ) {
		inherited::DrawSelf();
	} else {
		::InsetRect(&frame, 1, 1);
		::EraseRect(&frame);
		::InsetRect(&frame, -1, -1);
		::FrameRect(&frame);
	}
}

