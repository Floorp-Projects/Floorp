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
#include "nsIPref.h"

#ifdef DEBUG_seth
#define DO_HASHING_OF_HOSTNAME 1
#endif

#ifdef DO_HASHING_OF_HOSTNAME
#include "nsMsgUtils.h"
#endif /* DO_HASHING_OF_HOSTNAME */

#if defined(XP_UNIX) || defined(XP_BEOS)
#define NEWSRC_FILE_PREFIX ".newsrc-"
#else
#define NEWSRC_FILE_PREFIX ""
#endif /* XP_UNIX || XP_BEOS */

static NS_DEFINE_CID(kCPrefServiceCID, NS_PREF_CID);                            

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

NS_IMETHODIMP
nsNntpIncomingServer::GetNewsrcFilePath(nsIFileSpec **aNewsrcFilePath)
{
	nsresult rv;
	rv = GetFileValue("newsrc.file", aNewsrcFilePath);
	if (NS_SUCCEEDED(rv) && *aNewsrcFilePath) return rv;

	nsCOMPtr<nsIFileSpec> path;
        NS_WITH_SERVICE(nsIPref, prefs, kCPrefServiceCID, &rv);
    	if (NS_FAILED(rv) || (!prefs)) {
		return rv;
    	}  

	rv = prefs->GetFilePref("mail.newsrc_root", getter_AddRefs(path));
	if (NS_FAILED(rv)) return rv;

	nsXPIDLCString hostname;
	rv = GetHostName(getter_Copies(hostname));
	if (NS_FAILED(rv)) return rv;

        nsCAutoString newsrcFileName = NEWSRC_FILE_PREFIX;
	newsrcFileName += hostname;
#ifdef DO_HASHING_OF_HOSTNAME
    	NS_MsgHashIfNecessary(newsrcFileName);
#endif /* DO_HASHING_OF_HOSTNAME */
	path->AppendRelativeUnixPath(newsrcFileName);

	SetNewsrcFilePath(path);

        *aNewsrcFilePath = path;
	NS_ADDREF(*aNewsrcFilePath);

	return NS_OK;
}     

NS_IMETHODIMP
nsNntpIncomingServer::SetNewsrcFilePath(nsIFileSpec *spec)
{
    if (spec) {
        spec->CreateDir();
        return SetFileValue("newsrc.file", spec);
    }
    else {
	return NS_ERROR_FAILURE;
    }
}          

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



