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

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// CTaskBarView.cp
// A subclass of UTearOffBar for task bar view in main browser window
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#include "CTaskBarView.h"
#include "CGrayBevelView.h"

CTaskBarView::CTaskBarView(LStream *inStream) :
CTearOffBar(inStream)
{
	*inStream >> fControlPaneID;
	fAllowClickDrag = true;
}

CTaskBarView::~CTaskBarView()
{
}

void CTaskBarView::Click( SMouseDownEvent	&inMouseDown )
{
									// Check if a SubPane of this View
									//   is hit
// Modified code
// Check if we have clicked on a 0 pane
	LPane	*clickedPane;
	clickedPane = FindDeepSubPaneContaining(inMouseDown.wherePort.h,
											inMouseDown.wherePort.v );
	if	( clickedPane != nil )
	{
		// If the controlling pane is click, we treat it as if the CTaskBarView
		// were clicked and we process the drag.
		if (clickedPane->GetPaneID() == fControlPaneID)
		{
			ClickSelf(inMouseDown);
			return;
		}
	}

// LView::Click code
	// If it was one of our subpanes (but not the controlling pane-- we would have
	// already returned) then we let it get the click.
	clickedPane = FindSubPaneHitBy(inMouseDown.wherePort.h,
											inMouseDown.wherePort.v);

	if	( clickedPane != nil )
	{		//  SubPane is hit, let it respond to the Click
		clickedPane->Click(inMouseDown);
	}
}
