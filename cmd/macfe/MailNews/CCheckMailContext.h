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



// CCheckMailContext.h

#pragma once

#include "CMailNewsContext.h"

//======================================
class CCheckMailContext : public CMailNewsContext
//======================================
{
public:	
	enum	
	{
		BIFF_NOTIFY_ICONSUITE = 15242,
		msg_MailNotificationState = 'malS'
	};
private: // must call GetCheckMailContext
			CCheckMailContext();
public:

	static CCheckMailContext*	Get();
	static void					SuspendResume();
	static void					Initialize(void* inUser);	
	static void					Release(void* inUser);	
	void						SetState(MSG_BIFF_STATE state);
	MSG_BIFF_STATE				GetState() const {	return mCheckMailState; }
	
protected:
#if 0
	void						DoSuspendResume();
#endif
	void						RemoveNotification();
	void						InstallNotification();		// do fun mac stuff
	virtual						~CCheckMailContext();

// -- Data --
protected:
	static CCheckMailContext	*sCheckMailContext;
	MSG_BIFF_STATE				mCheckMailState;
	
	Boolean						mOutstandingNotification;
	NMRec						mNotification;
	Handle						mMailNotifyIcon;
}; // class CCheckMailContext
