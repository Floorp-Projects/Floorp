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

#include "nsIImapIncomingServer.h"
#include "nsIImapHostSessionList.h"
#include "nsImapIncomingServer.h"
#include "nsMsgIncomingServer.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"

static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);

/* get some implementation from nsMsgIncomingServer */
class nsImapIncomingServer : public nsMsgIncomingServer,
                             public nsIImapIncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsImapIncomingServer();
    virtual ~nsImapIncomingServer();

	// we support the nsIImapIncomingServer interface
	NS_IMETHOD GetRootFolderPath(char **);
	NS_IMETHOD SetRootFolderPath(char *);
	NS_IMETHOD SetKey(char * aKey);  // override nsMsgIncomingServer's implementation...

private:
	char *m_rootFolderPath;
};

NS_IMPL_ISUPPORTS_INHERITED(nsImapIncomingServer,
                            nsMsgIncomingServer,
                            nsIImapIncomingServer);

                            
nsImapIncomingServer::nsImapIncomingServer() : m_rootFolderPath(nsnull)
{    
    NS_INIT_REFCNT();
}

nsImapIncomingServer::~nsImapIncomingServer()
{
	PR_FREEIF(m_rootFolderPath);
}

NS_IMPL_SERVERPREF_STR(nsImapIncomingServer, RootFolderPath, "directory")

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

nsresult NS_NewImapIncomingServer(const nsIID& iid,
                                  void **result)
{
    nsImapIncomingServer *server;
    if (!result) return NS_ERROR_NULL_POINTER;
    server = new nsImapIncomingServer();
    return server->QueryInterface(iid, result);
}


