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
 * Copyright (C) 1996 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CProgressBroadcaster.h

#pragma once

#include <LBroadcaster.h>

//======================================
class CProgressBroadcaster : public LBroadcaster
//======================================
{
public:
	enum {	msg_StatusText = 'StRp',
			msg_StatusPercent = 'StPc',
			msg_StatusComplete = 'StCt'	
		 };


	struct StatusInfo
	{
		const char* message;
		Int32 percent;
		int level;
		
		StatusInfo () : message(NULL), percent(0), level(0) {}
	};

	CProgressBroadcaster() {}
	virtual ~CProgressBroadcaster() {}
};
