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

// Interface to the class that handles the nav-center in a stand-alone window


#include "CNetscapeWindow.h"
#include "CSaveWindowStatus.h"
#include "htrdf.h"

class LStream;
class CRDFCoordinator;


class CNavCenterWindow : public CNetscapeWindow, CSaveWindowStatus
{
	public:
		enum { class_ID = 'NavW', res_ID = 9500 } ;
		
		CNavCenterWindow ( LStream* inStream );
		virtual ~CNavCenterWindow ( ) ;
		
		// make the given pane the one that is in front
		virtual void BringPaneToFront ( HT_ViewType inPane ) ;

		virtual	CNSContext*	GetWindowContext() const { return nil; };
		
		virtual void DoClose() ;
		virtual void AttemptClose() ;

	protected:

		virtual UInt16 GetValidStatusVersion(void) const { return 0x0201; }
		virtual ResIDT GetStatusResID(void) const { return res_ID; }

		virtual void FinishCreateSelf();
		
		void DoDefaultPrefs() ;
		
		CRDFCoordinator* mTree;

}; // CNavCenterWindow
