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

#include "nsNntpIncomingServer.h"
#include "nsXPIDLString.h"

NS_IMPL_ISUPPORTS_INHERITED(nsNntpIncomingServer,
                            nsMsgIncomingServer,
                            nsINntpIncomingServer);

                            

nsNntpIncomingServer::nsNntpIncomingServer()
{    
    NS_INIT_REFCNT();
}

nsNntpIncomingServer::~nsNntpIncomingServer()
{
}

NS_IMPL_SERVERPREF_STR(nsNntpIncomingServer,
                       RootFolderPath,
                       "directory")
    
NS_IMPL_SERVERPREF_STR(nsNntpIncomingServer,
                       NewsrcFilePath,
                       "newsrc.file")

nsresult
nsNntpIncomingServer::GetServerURI(char **uri)
{
    nsresult rv;

    nsXPIDLCString hostname;
    rv = GetHostName(getter_Copies(hostname));

    nsXPIDLCString username;
    rv = GetUsername(getter_Copies(username));
    
    if (NS_FAILED(rv)) return rv;

    if ((const char*)username)
        *uri = PR_smprintf("news://%s@%s", (const char*)username,
                           (const char*)hostname);
    else
        *uri = PR_smprintf("news://%s", (const char*)hostname);

    return rv;
}



