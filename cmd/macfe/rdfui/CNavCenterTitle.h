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

#include "htrdf.h"
#include "CAMSavvyBevelView.h"


class CNavCenterTitle 
	: public CAMSavvyBevelView, public LListener
{
public:
	
	enum { class_ID = 'hbar', pane_ID = 'hbar', kTitlePaneID = 'titl',
			kCloseBoxPaneID = 'clos' } ;
	enum { msg_CloseShelfNow = 'clos' } ;
	
		// Construction, Destruction
	CNavCenterTitle(LStream *inStream);
	~CNavCenterTitle();

protected:

		// PowerPlant overrides
	virtual void ListenToMessage ( MessageT inMessage, void* ioParam ) ;
	virtual void FinishCreateSelf ( ) ;

		// Provide access to the LCaption that displays the title
	LCaption& TitleCaption ( ) { return *mTitle; }
	const LCaption& TitleCaption ( ) const { return *mTitle; }
	
private:

	LCaption* mTitle;
		
}; // class CNavCenterTitle