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

#include "nsIPop3IncomingServer.h"
#include "nsPop3IncomingServer.h"
#include "nsMsgIncomingServer.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"


/* get some implementation from nsMsgIncomingServer */
class nsPop3IncomingServer : public nsMsgIncomingServer,
                             public nsIPop3IncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsPop3IncomingServer();
    virtual ~nsPop3IncomingServer();
    
    NS_IMETHOD GetRootFolderPath(char **);
    NS_IMETHOD SetRootFolderPath(char *);

    NS_IMETHOD GetLeaveMessagesOnServer(PRBool *);
    NS_IMETHOD SetLeaveMessagesOnServer(PRBool);

    NS_IMETHOD GetDeleteMailLeftOnServer(PRBool *);
    NS_IMETHOD SetDeleteMailLeftOnServer(PRBool);

    NS_IMETHOD GetServerURI(char * *uri);
    
private:
    char *m_rootFolderPath;
    PRBool m_leaveOnServer;
    PRBool m_deleteMailLeftOnServer;
};

NS_IMPL_ISUPPORTS_INHERITED(nsPop3IncomingServer,
                            nsMsgIncomingServer,
                            nsIPop3IncomingServer);

                            

nsPop3IncomingServer::nsPop3IncomingServer() :
    m_rootFolderPath(0),
    m_leaveOnServer(PR_FALSE),
    m_deleteMailLeftOnServer(PR_FALSE)
{    
    NS_INIT_REFCNT();
}

nsPop3IncomingServer::~nsPop3IncomingServer()
{
    PR_FREEIF(m_rootFolderPath);
}

NS_IMPL_SERVERPREF_STR(nsPop3IncomingServer,
                       RootFolderPath,
                       "directory")

NS_IMPL_SERVERPREF_BOOL(nsPop3IncomingServer,
                        LeaveMessagesOnServer,
                        "leave_on_server")

NS_IMPL_SERVERPREF_INT(nsPop3IncomingServer,
                       DeleteMailLeftOnServer,
                       "delete_mail_left_on_server")

nsresult
nsPop3IncomingServer::GetServerURI(char **uri)
{
    nsresult rv;
    char *hostname;
    
    rv = GetHostName(&hostname);
    if (NS_FAILED(rv)) return rv;

    const char* urischema ="mailbox:/";

    // eventually, we want this to be mailbox://hostname/
    *uri = PL_strdup(urischema);

    PR_Free(hostname);
    return rv;
}
    
nsresult NS_NewPop3IncomingServer(const nsIID& iid,
                                  void **result)
{
    nsPop3IncomingServer *server;
    if (!result) return NS_ERROR_NULL_POINTER;
    server = new nsPop3IncomingServer();

    return server->QueryInterface(iid, result);
}


