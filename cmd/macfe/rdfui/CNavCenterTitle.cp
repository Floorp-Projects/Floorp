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
// Class that draws the header area for the nav center which shows the title
// of the currently selected view as well as harboring the closebox for
// an easy way to close up the navCenter shelf.
//

#include "CNavCenterTitle.h"
#include "CNavCenterSelectorPane.h"		// for message id's

#include <LCaption.h>


CNavCenterTitle :: CNavCenterTitle ( LStream *inStream )
	: CAMSavvyBevelView (inStream),
		mTitle(NULL)
{

}


CNavCenterTitle :: ~CNavCenterTitle()
{
	// nothing to do 
}


//
// FinishCreateSelf
//
// Last minute setup stuff....
//
void
CNavCenterTitle :: FinishCreateSelf ( )
{
	mTitle = dynamic_cast<LCaption*>(FindPaneByID(kTitlePaneID));
	Assert_(mTitle != NULL);
	
} // FinishCreateSelf


//
// ListenToMessage
//
// We want to know when the selected workspace changes so that we can update the
// title string. The RDFCoordinator sets us up as a listener to the selector pane
// which will broadcast when things change.
//
void
CNavCenterTitle :: ListenToMessage ( MessageT inMessage, void* ioParam ) 
{
	switch ( inMessage ) {
	
		case CNavCenterSelectorPane::msg_ActiveSelectorChanged:
		{
			HT_View newView = reinterpret_cast<HT_View>(ioParam);
			if ( newView ) {
				// do not delete |buffer|
				const char* buffer = HT_GetViewName ( newView );
				TitleCaption().SetDescriptor(LStr255(buffer));
				
				// if we're in the middle of a drag and drop, draw NOW, not
				// when we get a refresh event.
				if ( ::StillDown() ) {
					FocusDraw();
					Draw(nil);
				}
			}
		}
	
	} // case of which message

} // ListenToMessage