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
#include "nsIProfile.h"
#include "prenv.h"

#ifdef DEBUG_seth
#define DO_HASHING_OF_HOSTNAME 1
#endif /* DEBUG_seth */

#ifdef DO_HASHING_OF_HOSTNAME
#include "nsMsgUtils.h"
#endif /* DO_HASHING_OF_HOSTNAME */

#define NEW_NEWS_DIR_NAME        "News"
#define PREF_MAIL_NEWSRC_ROOT    "mail.newsrc_root"

#if defined(XP_UNIX) || defined(XP_BEOS)
#define NEWSRC_FILE_PREFIX ".newsrc-"
#else
#define NEWSRC_FILE_PREFIX ""
#endif /* XP_UNIX || XP_BEOS */

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);                            

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

NS_IMPL_SERVERPREF_INT(nsNntpIncomingServer, NotifySize, "notify.size");
NS_IMPL_SERVERPREF_BOOL(nsNntpIncomingServer, NotifyOn, "notify.on");
NS_IMPL_SERVERPREF_BOOL(nsNntpIncomingServer, MarkOldRead, "mark_old_read");
NS_IMPL_SERVERPREF_INT(nsNntpIncomingServer, MaxArticles, "max_articles");

NS_IMETHODIMP
nsNntpIncomingServer::GetNewsrcFilePath(nsIFileSpec **aNewsrcFilePath)
{
	nsresult rv;
	rv = GetFileValue("newsrc.file", aNewsrcFilePath);
	if (NS_SUCCEEDED(rv) && *aNewsrcFilePath) return rv;

	nsCOMPtr<nsIFileSpec> path;

	rv = GetNewsrcRootPath(getter_AddRefs(path));
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

NS_IMETHODIMP
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

NS_IMETHODIMP
nsNntpIncomingServer::SetNewsrcRootPath(nsIFileSpec *aNewsrcRootPath)
{
    nsresult rv;
    
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
    if (NS_SUCCEEDED(rv) && prefs) {
        rv = prefs->SetFilePref(PREF_MAIL_NEWSRC_ROOT,aNewsrcRootPath, PR_FALSE /* set default */);
        return rv;
    }
    else {
        return NS_ERROR_FAILURE;
    }
}

NS_IMETHODIMP
nsNntpIncomingServer::GetNewsrcRootPath(nsIFileSpec **aNewsrcRootPath)
{
    nsresult rv;
    nsFileSpec dir;

    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefs->GetFilePref(PREF_MAIL_NEWSRC_ROOT, aNewsrcRootPath);
    if (NS_SUCCEEDED(rv)) return rv;
#ifdef XP_UNIX
    // root the newsrc files to <profile>/News, except on UNIX
    // on UNIX, set it to ~.
    // this may change soon, and on UNIX, the default will also be <profile>/News
    char *unixHomeDirectory = PR_GetEnv("HOME");
    dir = unixHomeDirectory;
#else
    NS_WITH_SERVICE(nsIProfile, profile, NS_PROFILE_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = profile->GetCurrentProfileDir(&dir);
    if (NS_FAILED(rv)) return rv;

    dir += NEW_NEWS_DIR_NAME;
#endif /* XP_UNIX */

    rv = SetNewsrcRootPath(*aNewsrcRootPath);
    if (NS_FAILED(rv)) return rv;
    
    rv = NS_NewFileSpecWithSpec(dir, aNewsrcRootPath);
    return rv;
}

