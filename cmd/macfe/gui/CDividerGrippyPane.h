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
// Interface for class that adapts the CPatternedGrippyPane for use in the chrome of
// the window as visual feedback/interaction point for sliding a drawer in and out. 
//

#pragma once

#include "CPatternedGrippyPane.h"


class CDividerGrippyPane : public CPatternedGrippyPane, public LBroadcaster
{
public:
	enum { class_ID = 'DiPg' };
	
							CDividerGrippyPane(LStream* inStream);
	virtual					~CDividerGrippyPane() { };

	virtual void ClickSelf ( const SMouseDownEvent & inEvent ) ;
	virtual void AdjustCursorSelf ( Point inPortPt, const EventRecord &inMacEvent ) ;
	
	int foo;

}; // CDividerGrippyPane
