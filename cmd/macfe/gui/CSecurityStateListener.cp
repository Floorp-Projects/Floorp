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

#include "CSecurityStateListener.h"

#include "CBrowserContext.h"	// for msg_SecurityState
#include "ssl.h"	// for SSL_SECURITY_STATUS_* defines

CSecurityStateListener::CSecurityStateListener()
{
}

CSecurityStateListener::~CSecurityStateListener()
{
}

void CSecurityStateListener::ListenToMessage(
	MessageT inMessage,
	void* ioParam)
{
	if (inMessage == msg_SecurityState)
	{
		int status = (int)ioParam;
		ESecurityState state = eUnsecureState;
		if (status == SSL_SECURITY_STATUS_ON_LOW || status == SSL_SECURITY_STATUS_ON_HIGH)
		{
			state = eSecureState;
		}
		NoteSecurityState(state);
	}
}
