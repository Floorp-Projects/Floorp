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

#include "nsINntpIncomingServer.h"
#include "nsNntpIncomingServer.h"
#include "nsMsgIncomingServer.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"


/* get some implementation from nsMsgIncomingServer */
class nsNntpIncomingServer : public nsMsgIncomingServer,
                             public nsINntpIncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsNntpIncomingServer();
    virtual ~nsNntpIncomingServer();
    
    NS_IMETHOD GetRootFolderPath(char **);
    NS_IMETHOD SetRootFolderPath(char *);

private:
    char *m_rootFolderPath;
};

NS_IMPL_ISUPPORTS_INHERITED(nsNntpIncomingServer,
                            nsMsgIncomingServer,
                            nsINntpIncomingServer);

                            

nsNntpIncomingServer::nsNntpIncomingServer() :
    m_rootFolderPath(0)
{    
    NS_INIT_REFCNT();
}

nsNntpIncomingServer::~nsNntpIncomingServer()
{
    PR_FREEIF(m_rootFolderPath);
}

NS_IMPL_SERVERPREF_STR(nsNntpIncomingServer,
                       RootFolderPath,
                       "directory")

nsresult NS_NewNntpIncomingServer(const nsIID& iid,
                                  void **result)
{
    nsNntpIncomingServer *server;
    if (!result) return NS_ERROR_NULL_POINTER;
    server = new nsNntpIncomingServer();

    return server->QueryInterface(iid, result);
}


