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
 * Copyright (C) 1997 Netscape Communications Corporation.  All Rights
 * Reserved.
 */



// CNewsSubscriber.h

#pragma once
#include "msgcom.h"

//======================================
class CNewsSubscriber
//======================================
{
	public:
	
		static Boolean			DoAddNewsHost();
		static Boolean			DoSubscribeNewsGroup(MSG_Host* host = nil, Boolean inModal = false);

		static void				SetHost(MSG_Host* host)	{ mHost = host; };
		static MSG_Host*		GetHost()				{ return mHost; };

	private:

		static MSG_Host*	mHost;

}; // class CNewsSubscriber
