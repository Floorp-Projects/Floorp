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
static NS_DEFINE_CID(kImapUrlCID, NS_IMAPURL_CID);

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

NS_IMETHODIMP nsImapService::SelectFolder(PLEventQueue * aClientEventQueue, nsIImapMailfolder * aImapMailfolder, 
										  nsIUrlListener * aUrlListener, nsIURL ** aURL)
{

	// create a protocol instance to handle the request.
	// NOTE: once we start working with multiple connections, this step will be much more complicated...but for now
	// just create a connection and process the request.
	
	nsIImapProtocol * protocolInstance = nsnull;
	nsresult rv = CreateImapConnection(aClientEventQueue, &protocolInstance);
	nsIImapUrl * imapUrl = nsnull;

	if (NS_SUCCEEDED(rv) && protocolInstance)
	{
		// now we need to create an imap url to load into the connection. The url needs to represent a select folder action.
		rv = nsComponentManager::CreateInstance(kImapUrlCID, nsnull,
                                            nsIImapUrl::GetIID(), (void **)
                                            &imapUrl);
	}
	if (NS_SUCCEEDED(rv) && imapUrl)
	{
		rv = imapUrl->SetImapAction(nsIImapUrl::nsImapSelectFolder);
		rv = imapUrl->SetImapMailfolder(aImapMailfolder);
		// hmmm this is cludgy...we need to get the incoming server, get the host and port, and generate an imap url spec
		// based on that information then tell the imap parser to parse that spec...*yuck*. I have to think of a better way
		// for automatically generating the spec based on the incoming server data...
		nsIMsgIncomingServer * server = nsnull;
		rv = imapUrl->GetServer(&server);
		if (NS_SUCCEEDED(rv) && server)
		{
			char * hostName = nsnull;
			rv = server->GetHostName(&hostName);
			if (NS_SUCCEEDED(rv) && hostName)
			{
				char * urlSpec = PR_smprintf("imap://%s", hostName);
				rv = imapUrl->SetSpec(urlSpec);
				PR_Free(hostName);
				PR_FREEIF(urlSpec);
			} // if we got a host name
		} // if we have a incoming server

		imapUrl->RegisterListener(aUrlListener);  // register listener if there is one.
		protocolInstance->LoadUrl(imapUrl, nsnull);
		if (aURL)
			*aURL = imapUrl; 
		else
			NS_RELEASE(imapUrl); // release our ref count from the create instance call...
	} // if we have a url to run....

	return rv;
}
