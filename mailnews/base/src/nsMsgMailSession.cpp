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

#include "msgCore.h" // for pre-compiled headers
#include "nsIMsgIdentity.h"
#include "nsMsgIdentity.h"
#include "nsMsgMailSession.h"

NS_IMPL_ISUPPORTS(nsMsgMailSession, nsIMsgMailSession::GetIID());

nsMsgMailSession::nsMsgMailSession()
{
	NS_INIT_REFCNT();
	m_currentIdentity = new nsMsgIdentity();
	if (m_currentIdentity)
		NS_ADDREF(m_currentIdentity);

}

nsMsgMailSession::~nsMsgMailSession()
{
	NS_IF_RELEASE(m_currentIdentity);
}


// nsIMsgMailSession
nsresult nsMsgMailSession::GetCurrentIdentity(nsIMsgIdentity ** aIdentity)
{
	if (aIdentity)
	{
		*aIdentity = m_currentIdentity;
		NS_IF_ADDREF(m_currentIdentity);
	}

	return NS_OK;
}
