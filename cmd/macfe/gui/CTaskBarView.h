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
//	CTaskBarView.h
//  TaskBar view in browser window
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#pragma once

#include "UTearOffPalette.h"

class CTaskBarView : public CTearOffBar
// This class differs from CTearOffBar in two ways.
//		1) With the exception of the controlling pane (see below) it does not
//			create a floating window by dragging (it ignores mouse clicks).
//		2) This class relys on subpane, called the controlling pane. If this
//			pane is *clicked or dragged* it creates a floating window.
{
	public:
		enum { class_ID = 'TskV' };
		
								CTaskBarView(LStream *inStream);
		virtual					~CTaskBarView();

		virtual void		Click( SMouseDownEvent	&inMouseDown );
	
	private:
		PaneIDT	fControlPaneID;	// the pane ID of the pane that controls dragging
};
