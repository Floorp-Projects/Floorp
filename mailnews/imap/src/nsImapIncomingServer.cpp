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
#include "nsImapIncomingServer.h"
#include "nsMsgIncomingServer.h"

#include "nsIPref.h"

#include "prmem.h"
#include "plstr.h"


/* get some implementation from nsMsgIncomingServer */
class nsImapIncomingServer : public nsMsgIncomingServer,
                             public nsIImapIncomingServer
                             
{
public:
    NS_DECL_ISUPPORTS_INHERITED

    nsImapIncomingServer();
    virtual ~nsImapIncomingServer();

	// we support the nsIImapIncomingServer interface
	NS_IMPL_CLASS_GETSET_STR(RootFolderPath, m_rootFolderPath);
   
    NS_IMETHOD LoadPreferences(nsIPref *prefs, const char *serverKey);

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

nsresult
nsImapIncomingServer::LoadPreferences(nsIPref *prefs,
                                      const char *serverKey)
{
#ifdef DEBUG
    printf("Loading imap prefs for %s..\n",serverKey);
#endif
    
    nsMsgIncomingServer::LoadPreferences(prefs, serverKey);
    
	// now load imap server specific preferences
    m_rootFolderPath = getCharPref(prefs, serverKey, "directory");
    return NS_OK;
}

nsresult NS_NewImapIncomingServer(const nsIID& iid,
                                  void **result)
{
    nsImapIncomingServer *server;
    if (!result) return NS_ERROR_NULL_POINTER;
    server = new nsImapIncomingServer();

    return server->QueryInterface(iid, result);
}


