/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "msgCore.h"

#include "nsString2.h"

#include "nsIImapIncomingServer.h"
#include "nsIMAPHostSessionList.h"
#include "nsMsgIncomingServer.h"
#include "nsImapIncomingServer.h"
#include "nsIImapUrl.h"
#include "nsIUrlListener.h"
#include "nsIEventQueue.h"
#include "nsIImapProtocol.h"
#include "nsISupportsArray.h"
#include "nsVoidArray.h"
#include "nsCOMPtr.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"

static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);
static NS_DEFINE_CID(kImapProtocolCID, NS_IMAPPROTOCOL_CID);

/* get some implementation from nsMsgIncomingServer */
class nsImapIncomingServer : public nsMsgIncomingServer,
                             public nsIImapIncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsImapIncomingServer();
    virtual ~nsImapIncomingServer();

    // overriding nsMsgIncomingServer methods
	NS_IMETHOD SetKey(char * aKey);  // override nsMsgIncomingServer's implementation...
	NS_IMETHOD GetServerURI(char * *aServerURI);

	// we support the nsIImapIncomingServer interface
    NS_IMETHOD GetMaximumConnectionsNumber(PRInt32* maxConnections);
    NS_IMETHOD SetMaximumConnectionsNumber(PRInt32 maxConnections);
    
    NS_IMETHOD GetImapConnectionAndLoadUrl(nsIEventQueue* aClientEventQueue,
                                           nsIImapUrl* aImapUrl,
                                           nsIUrlListener* aUrlListener = 0,
                                           nsISupports* aConsumer = 0,
                                           nsIURL** aURL = 0);
    NS_IMETHOD LoadNextQueuedUrl();
    NS_IMETHOD RemoveConnection(nsIImapProtocol* aImapConnection);

private:
    nsresult CreateImapConnection (nsIEventQueue* aEventQueue,
                                   nsIImapUrl* aImapUrl,
                                   nsIImapProtocol** aImapConnection);
	char *m_rootFolderPath;
    nsCOMPtr<nsISupportsArray> m_connectionCache;
    nsCOMPtr<nsISupportsArray> m_urlQueue;
    nsVoidArray m_urlConsumers;
};

NS_IMPL_ISUPPORTS_INHERITED(nsImapIncomingServer,
                            nsMsgIncomingServer,
                            nsIImapIncomingServer);

                            
nsImapIncomingServer::nsImapIncomingServer() : m_rootFolderPath(nsnull)
{    
    NS_INIT_REFCNT();
    nsresult rv;
	rv = NS_NewISupportsArray(getter_AddRefs(m_connectionCache));
    rv = NS_NewISupportsArray(getter_AddRefs(m_urlQueue));
}

nsImapIncomingServer::~nsImapIncomingServer()
{
	PR_FREEIF(m_rootFolderPath);
}

NS_IMETHODIMP nsImapIncomingServer::SetKey(char * aKey)  // override nsMsgIncomingServer's implementation...
{
	nsMsgIncomingServer::SetKey(aKey);

	// okay now that the key has been set, we need to add ourselves to the
	// host session list...

	// every time we create an imap incoming server, we need to add it to the
	// host session list!! 

	// get the user and host name and the fields to the host session list.
	char * userName = nsnull;
	char * hostName = nsnull;
	
	nsresult rv = GetHostName(&hostName);
	rv = GetUserName(&userName);

	NS_WITH_SERVICE(nsIImapHostSessionList, hostSession, kCImapHostSessionList, &rv);
    if (NS_FAILED(rv)) return rv;

	hostSession->AddHostToList(hostName, userName);
	PR_FREEIF(userName);
	PR_FREEIF(hostName);

	return rv;
}

NS_IMETHODIMP nsImapIncomingServer::GetServerURI(char ** aServerURI)
{
	nsresult rv = NS_OK;
	nsString2 serverUri("imap://", eOneByte);
	char * hostName = nsnull;
	rv = GetHostName(&hostName);
	if (NS_FAILED(rv))
		return rv;

	serverUri += hostName;
	if (aServerURI)
		*aServerURI = PL_strdup(serverUri.GetBuffer());


	PR_FREEIF(hostName);

	return rv;
}

NS_IMPL_SERVERPREF_INT(nsImapIncomingServer, MaximumConnectionsNumber,
                       "max_cached_connections");

NS_IMETHODIMP
nsImapIncomingServer::GetImapConnectionAndLoadUrl(nsIEventQueue*
                                                  aClientEventQueue,
                                                  nsIImapUrl* aImapUrl,
                                                  nsIUrlListener*
                                                  aUrlListener,
                                                  nsISupports* aConsumer,
                                                  nsIURL** aURL)
{
    nsresult rv = NS_OK;
    nsIImapProtocol* aProtocol = nsnull;
    
    rv = CreateImapConnection(aClientEventQueue, aImapUrl, &aProtocol);
    if (NS_FAILED(rv)) return rv;

    if (aUrlListener)
        aImapUrl->RegisterListener(aUrlListener);

    if (aProtocol)
    {
        rv = aProtocol->LoadUrl(aImapUrl, aConsumer);
    }
    else
    {   // unable to get an imap connection to run the url; add to the url
        // queue
        PR_CEnterMonitor(this);
        m_urlQueue->AppendElement(aImapUrl);
        m_urlConsumers.AppendElement((void*)aConsumer);
        NS_IF_ADDREF(aConsumer);
        PR_CExitMonitor(this);
    }
    if (aURL)
    {
        *aURL = aImapUrl;
        NS_IF_RELEASE(*aURL);
    }

    return rv;
}

NS_IMETHODIMP
nsImapIncomingServer::LoadNextQueuedUrl()
{
    PRUint32 cnt = 0;
    nsresult rv = NS_OK;

    PR_CEnterMonitor(this);
    m_urlQueue->Count(&cnt);
    if (cnt > 0)
    {
        nsCOMPtr<nsIImapUrl>
            aImapUrl(do_QueryInterface(m_urlQueue->ElementAt(0)));

        if (aImapUrl)
        {
            nsISupports *aConsumer =
                (nsISupports*)m_urlConsumers.ElementAt(0);

            NS_IF_ADDREF(aConsumer);
            
            nsIImapProtocol * protocolInstance = nsnull;
            rv = CreateImapConnection(nsnull, aImapUrl,
                                               &protocolInstance);
            if (NS_SUCCEEDED(rv) && protocolInstance)
            {
                rv = protocolInstance->LoadUrl(aImapUrl, aConsumer);
                m_urlQueue->RemoveElementAt(0);
                m_urlConsumers.RemoveElementAt(0);
            }

            NS_IF_RELEASE(aConsumer);
        }
    }
    PR_CExitMonitor(this);
    return rv;
}


NS_IMETHODIMP
nsImapIncomingServer::RemoveConnection(nsIImapProtocol* aImapConnection)
{
    PRInt32 elementIndex = -1;
    nsresult rv;
    PR_CEnterMonitor(this);

    if (aImapConnection)
    {
        // preventing earlier release of the protocol
        nsCOMPtr<nsIImapProtocol>
            aConnection(do_QueryInterface(aImapConnection,&rv));
        aImapConnection->TellThreadToDie(PR_TRUE);

        m_connectionCache->RemoveElement(aImapConnection);
    }

    PR_CExitMonitor(this);
    return NS_OK;
}

nsresult
nsImapIncomingServer::CreateImapConnection(nsIEventQueue *aEventQueue, 
                                           nsIImapUrl * aImapUrl, 
                                           nsIImapProtocol ** aImapConnection)
{
	nsresult rv = NS_OK;
	PRBool canRunUrl = PR_FALSE;
    PRBool hasToWait = PR_FALSE;
	nsCOMPtr<nsIImapProtocol> connection;
    nsCOMPtr<nsIImapProtocol> freeConnection;
    PRBool isBusy = PR_FALSE;
    PRBool isInboxConnection = PR_FALSE;

    PR_CEnterMonitor(this);

    PRInt32 maxConnections = 2; // default to be two
    rv = GetMaximumConnectionsNumber(&maxConnections);
    if (NS_FAILED(rv) || maxConnections < 2)
    {
        maxConnections = 2;
        rv = SetMaximumConnectionsNumber(maxConnections);
    }

    *aImapConnection = nsnull;
	// iterate through the connection cache for a connection that can handle this url.
	PRUint32 cnt;
    rv = m_connectionCache->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    for (PRUint32 i = 0; i < cnt && !canRunUrl && !hasToWait; i++) 
	{
        connection = do_QueryInterface(m_connectionCache->ElementAt(i));
		if (connection)
			connection->CanHandleUrl(aImapUrl, canRunUrl, hasToWait);
        
        if (!freeConnection && !canRunUrl && !hasToWait && connection)
        {
            connection->IsBusy(isBusy, isInboxConnection);
            if (!isBusy && !isInboxConnection)
                freeConnection = connection;
        }
	}

	// if we got here and we have a connection, then we should return it!
	if (canRunUrl && connection)
	{
		*aImapConnection = connection;
		NS_IF_ADDREF(*aImapConnection);
	}
    else if (hasToWait)
    {
        // do nothing; return NS_OK; for queuing
    }
	else if (cnt < maxConnections && aEventQueue)
	{	
		// create a new connection and add it to the connection cache
		// we may need to flag the protocol connection as busy so we don't get
        // a race 
		// condition where someone else goes through this code 
		nsIImapProtocol * protocolInstance = nsnull;
		rv = nsComponentManager::CreateInstance(kImapProtocolCID, nsnull,
                                                nsIImapProtocol::GetIID(),
                                                (void **) &protocolInstance);
		if (NS_SUCCEEDED(rv) && protocolInstance)
        {
            NS_WITH_SERVICE(nsIImapHostSessionList, hostSession,
                            kCImapHostSessionList, &rv);
            if (NS_SUCCEEDED(rv))
                rv = protocolInstance->Initialize(hostSession, aEventQueue);
        }
		
		// take the protocol instance and add it to the connectionCache
		if (protocolInstance)
			m_connectionCache->AppendElement(protocolInstance);
		*aImapConnection = protocolInstance; // this is already ref counted.

	}
    else if (freeConnection)
    {
        *aImapConnection = freeConnection;
        NS_IF_ADDREF(*aImapConnection);
    }
    else // cannot get anyone to handle the url queue it
    {
        // queue the url
    }

    PR_CExitMonitor(this);
	return rv;
}

nsresult NS_NewImapIncomingServer(const nsIID& iid,
                                  void **result)
{
    nsImapIncomingServer *server;
    if (!result) return NS_ERROR_NULL_POINTER;
    server = new nsImapIncomingServer();
    return server->QueryInterface(iid, result);
}


