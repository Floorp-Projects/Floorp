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

// Implementation of the class that handles the nav-center in a stand-alone window.

#include "CNavCenterWindow.h"
#include "CRDFCoordinator.h"

#include "resgui.h"
#include "Netscape_Constants.h"
#include "CPrefsDialog.h"


//
// Constructor
//
CNavCenterWindow :: CNavCenterWindow ( LStream* inStream )
	: CNetscapeWindow ( inStream, WindowType_NavCenter ), CSaveWindowStatus(this)
{
}


//
// Destructor
//
CNavCenterWindow :: ~CNavCenterWindow ( ) 
{
	mTree->UnregisterNavCenter();
}


//
// FinishCreateSelf
//
// Handle the remainder of the initialization stuff...
//
void
CNavCenterWindow :: FinishCreateSelf()
{
	// use saved positioning information
	FinishCreateWindow();
	
	mTree = dynamic_cast<CRDFCoordinator*>(FindPaneByID('RCoo'));
	Assert_(mTree);
	mTree->RegisterNavCenter(NULL);

	SwitchTarget(mTree);
	
} // FinishCreateSelf


//
// BringPaneToFront
//		
// make the given pane the one that is in front
//
void
CNavCenterWindow :: BringPaneToFront ( HT_ViewType inPane ) 
{
	mTree->SelectView ( inPane );
		
} // BringPaneToFront
		

//
// DoClose
//
// Make sure the size/location gets saved when we close the window...
//
void 
CNavCenterWindow :: DoClose()
{
	AttemptCloseWindow();			// save state
	CMediatedWindow::DoClose();		// finish the job...
	
} // DoClose


//
// AttemptClose
//
// Make sure the size/location gets saved when we close the window...
//
// ¥ At some point this should be made like CBrowserWindow::AttemptClose 
//	 to do scripting right
//
void 
CNavCenterWindow :: AttemptClose()
 {
	AttemptCloseWindow();				// save state
	CMediatedWindow::AttemptClose();	// finish the job...
	
} // AttemptClose


//
// DoDefaultPrefs
//
// Show the prefs when called upon
//
void 
CNavCenterWindow :: DoDefaultPrefs()
{
	CPrefsDialog::EditPrefs(CPrefsDialog::eExpandBrowser);
}
