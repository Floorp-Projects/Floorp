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

#include "nsINoIncomingServer.h"
#include "nsNoIncomingServer.h"
#include "nsMsgIncomingServer.h"
#include "nsMsgLocalCID.h"
#include "nsMsgFolderFlags.h"
#include "nsIFileSpec.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsXPIDLString.h"

/* get some implementation from nsMsgIncomingServer */
class nsNoIncomingServer : public nsMsgIncomingServer,
                             public nsINoIncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSINOINCOMINGSERVER

    nsNoIncomingServer();
    virtual ~nsNoIncomingServer();
    
    NS_IMETHOD GetServerURI(char * *uri);
};

NS_IMPL_ISUPPORTS_INHERITED(nsNoIncomingServer,
                            nsMsgIncomingServer,
                            nsINoIncomingServer);

                            

nsNoIncomingServer::nsNoIncomingServer()
{    
    NS_INIT_REFCNT();
}

nsNoIncomingServer::~nsNoIncomingServer()
{
}

nsresult
nsNoIncomingServer::GetServerURI(char **uri)
{
    nsresult rv;

    nsXPIDLCString username;
    rv = GetUsername(getter_Copies(username));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString hostname;
    rv = GetHostName(getter_Copies(hostname));
    if (NS_FAILED(rv)) return rv;

    *uri = PR_smprintf("mailbox://%s@%s",
                       (const char *)username,
                       (const char *)hostname);
    return rv;
}

NS_IMETHODIMP nsNoIncomingServer::CreateDefaultMailboxes(nsIFileSpec *path)
{
        nsresult rv;
        PRBool exists;
        if (!path) return NS_ERROR_NULL_POINTER;

        // todo, use a string bundle for this

	// notice, no Inbox

        rv =path->AppendRelativeUnixPath("Trash");
        if (NS_FAILED(rv)) return rv;
        rv = path->Exists(&exists);
        if (!exists) {
                rv = path->Touch();
                if (NS_FAILED(rv)) return rv;
        }

        rv = path->SetLeafName("Sent");
        if (NS_FAILED(rv)) return rv;
        rv = path->Exists(&exists);
        if (NS_FAILED(rv)) return rv;
        if (!exists) {
                rv = path->Touch();
                if (NS_FAILED(rv)) return rv;
        }

        rv = path->SetLeafName("Drafts");
        if (NS_FAILED(rv)) return rv;
        rv = path->Exists(&exists);
        if (NS_FAILED(rv)) return rv;
        if (!exists) {
                rv = path->Touch();
                if (NS_FAILED(rv)) return rv;
        }

        rv = path->SetLeafName("Templates");
        if (NS_FAILED(rv)) return rv;
        rv = path->Exists(&exists);
        if (NS_FAILED(rv)) return rv;
        if (!exists) {
                rv = path->Touch();
                if (NS_FAILED(rv)) return rv;
        }

        rv = path->SetLeafName("Unsent Messages");
        if (NS_FAILED(rv)) return rv;
        rv = path->Exists(&exists);
        if (NS_FAILED(rv)) return rv;
        if (!exists) {
                rv = path->Touch();
                if (NS_FAILED(rv)) return rv;
        }
        return rv;
}

nsresult NS_NewNoIncomingServer(const nsIID& iid,
                                  void **result)
{
    nsNoIncomingServer *server;
    if (!result) return NS_ERROR_NULL_POINTER;
    server = new nsNoIncomingServer();

    return server->QueryInterface(iid, result);
}

