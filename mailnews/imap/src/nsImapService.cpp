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

#include "msgCore.h"    // precompiled header...

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

#include "nsIIMAPHostSessionList.h"
#include "nsImapService.h"
#include "nsImapUrl.h"
#include "nsImapProtocol.h"

static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);

nsImapService::nsImapService()
{
    NS_INIT_REFCNT();

	// mscott - the imap service really needs to be a service listener
	// on the host session list...
	nsresult rv = nsServiceManager::GetService(kCImapHostSessionList, nsIImapHostSessionList::GetIID(),
                                  (nsISupports**)&m_sessionList);
}

nsImapService::~nsImapService()
{
	// release the host session list
	if (m_sessionList)
		(void)nsServiceManager::ReleaseService(kCImapHostSessionList, m_sessionList);
}

NS_IMPL_THREADSAFE_ISUPPORTS(nsImapService, nsIImapService::GetIID());

NS_IMETHODIMP nsImapService::CreateImapConnection(PLEventQueue *aEventQueue, nsIImapProtocol ** aImapConnection)
{
	nsIImapProtocol * protocolInstance = nsnull;
	nsresult rv = NS_OK;
	if (aImapConnection)
	{
		rv = nsComponentManager::CreateInstance(kImapProtocolCID, nsnull, nsIImapProtocol::GetIID(), (void **) &protocolInstance);
		if (NS_SUCCEEDED(rv) && protocolInstance)
			rv = protocolInstance->Initialize(m_sessionList, aEventQueue);
		*aImapConnection = protocolInstance;
	}

	return rv;
}