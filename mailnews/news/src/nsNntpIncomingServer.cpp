/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsNntpIncomingServer.h"
#include "nsXPIDLString.h"
#include "nsIPref.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsEnumeratorUtils.h"
#include "nsIMsgNewsFolder.h"
#include "nsIFolder.h"
#include "nsIFileSpec.h"
#include "nsFileStream.h"

#ifdef DEBUG_seth
#define DO_HASHING_OF_HOSTNAME 1
#endif /* DEBUG_seth */

#ifdef DO_HASHING_OF_HOSTNAME
#include "nsMsgUtils.h"
#endif /* DO_HASHING_OF_HOSTNAME */

#define NEW_NEWS_DIR_NAME        "News"
#define PREF_MAIL_NEWSRC_ROOT    "mail.newsrc_root"

#if defined(XP_UNIX) || defined(XP_BEOS)
#define NEWSRC_FILE_PREFIX "newsrc-"
#else
#define NEWSRC_FILE_PREFIX ""
#endif /* XP_UNIX || XP_BEOS */

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);                            
static NS_DEFINE_IID(kIFileLocatorIID,      NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID,       NS_FILELOCATOR_CID);

NS_IMPL_ISUPPORTS_INHERITED(nsNntpIncomingServer,
                            nsMsgIncomingServer,
                            nsINntpIncomingServer);


nsNntpIncomingServer::nsNntpIncomingServer()
{    
    NS_INIT_REFCNT();

    mNewsrcHasChanged = PR_FALSE;
}

nsNntpIncomingServer::~nsNntpIncomingServer()
{
    CloseCachedConnections();
}

NS_IMPL_SERVERPREF_BOOL(nsNntpIncomingServer, NotifyOn, "notify.on");
NS_IMPL_SERVERPREF_BOOL(nsNntpIncomingServer, MarkOldRead, "mark_old_read");
NS_IMPL_SERVERPREF_BOOL(nsNntpIncomingServer, Abbreviate, "abbreviate");
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
	newsrcFileName.Append(hostname);
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
	nsresult rv;
    if (!spec) {
		return NS_ERROR_FAILURE;
	}
		
	PRBool exists;
		
	rv = spec->Exists(&exists);
	if (!exists) {
		rv = spec->Touch();
		if (NS_FAILED(rv)) return rv;
	}
			
    return SetFileValue("newsrc.file", spec);
}          

NS_IMETHODIMP
nsNntpIncomingServer::GetLocalStoreType(char **type)
{
    NS_ENSURE_ARG_POINTER(type);
    *type = nsCRT::strdup("news");
    return NS_OK;
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

    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = prefs->GetFilePref(PREF_MAIL_NEWSRC_ROOT, aNewsrcRootPath);
    if (NS_SUCCEEDED(rv)) return rv;
    NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = locator->GetFileLocation(nsSpecialFileSpec::App_NewsDirectory50, aNewsrcRootPath);
    if (NS_FAILED(rv)) return rv;

    rv = SetNewsrcRootPath(*aNewsrcRootPath);
    return rv;
}

NS_IMETHODIMP
nsNntpIncomingServer::WriteNewsrcFile()
{
    nsresult rv;

    PRBool newsrcHasChanged;
    rv = GetNewsrcHasChanged(&newsrcHasChanged);
    if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_NEWS
	nsXPIDLCString hostname;
	rv = GetHostName(getter_Copies(hostname));
	if (NS_FAILED(rv)) return rv;
#endif /* DEBUG_NEWS */

    if (newsrcHasChanged) {        
#ifdef DEBUG_NEWS
        printf("write newsrc file for %s\n", (const char *)hostname);
#endif
        nsCOMPtr <nsIFileSpec> newsrcFile;
        rv = GetNewsrcFilePath(getter_AddRefs(newsrcFile));
	    if (NS_FAILED(rv)) return rv;

        nsFileSpec newsrcFileSpec;
        rv = newsrcFile->GetFileSpec(&newsrcFileSpec);
        if (NS_FAILED(rv)) return rv;

        nsIOFileStream newsrcStream(newsrcFileSpec, (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));

        nsCOMPtr<nsIEnumerator> subFolders;
        nsCOMPtr<nsIFolder> rootFolder;
 
        rv = GetRootFolder(getter_AddRefs(rootFolder));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(rootFolder, &rv);
        if (NS_FAILED(rv)) return rv;

        nsXPIDLCString optionLines;
        rv = newsFolder->GetOptionLines(getter_Copies(optionLines));
        if (NS_SUCCEEDED(rv) && ((const char *)optionLines)) {
               newsrcStream << (const char *)optionLines;
#ifdef DEBUG_NEWS
               printf("option lines:\n%s",(const char *)optionLines);
#endif /* DEBUG_NEWS */
        }
#ifdef DEBUG_NEWS
        else {
            printf("no option lines to write out\n");
        }
#endif /* DEBUG_NEWS */

        nsXPIDLCString unsubscribedLines;
        rv = newsFolder->GetUnsubscribedNewsgroupLines(getter_Copies(unsubscribedLines));
        if (NS_SUCCEEDED(rv) && ((const char *)unsubscribedLines)) {
               newsrcStream << (const char *)unsubscribedLines;
#ifdef DEBUG_NEWS
               printf("unsubscribedLines:\n%s",(const char *)unsubscribedLines);
#endif /* DEBUG_NEWS */
        }
#ifdef DEBUG_NEWS
        else {
            printf("no unsubscribed lines to write out\n");
        } 
#endif /* DEBUG_NEWS */

        rv = rootFolder->GetSubFolders(getter_AddRefs(subFolders));
        if (NS_FAILED(rv)) return rv;

        nsAdapterEnumerator *simpleEnumerator = new nsAdapterEnumerator(subFolders);
        if (simpleEnumerator == nsnull) return NS_ERROR_OUT_OF_MEMORY;

        PRBool moreFolders;
        
        while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders)) && moreFolders) {
            nsCOMPtr<nsISupports> child;
            rv = simpleEnumerator->GetNext(getter_AddRefs(child));
            if (NS_SUCCEEDED(rv) && child) {
                newsFolder = do_QueryInterface(child, &rv);
                if (NS_SUCCEEDED(rv) && newsFolder) {
                    nsXPIDLCString newsrcLine;
                    rv = newsFolder->GetNewsrcLine(getter_Copies(newsrcLine));
                    if (NS_SUCCEEDED(rv) && ((const char *)newsrcLine)) {
                        newsrcStream << (const char *)newsrcLine;
#ifdef DEBUG_NEWS
                        printf("writing to newsrc file:\n");
                        printf("%s",(const char *)newsrcLine);
#endif /* DEBUG_NEWS */
                    }
                }
            }
        }
        delete simpleEnumerator;

        newsrcStream.close();
        
        rv = SetNewsrcHasChanged(PR_FALSE);
    }
#ifdef DEBUG_NEWS
    else {
        printf("no need to write newsrc file for %s, it was not dirty\n", (const char *)hostname);
    }
#endif /* DEBUG_NEWS */

    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetNewsrcHasChanged(PRBool aNewsrcHasChanged)
{
    mNewsrcHasChanged = aNewsrcHasChanged;
    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetNewsrcHasChanged(PRBool *aNewsrcHasChanged)
{
    if (!aNewsrcHasChanged) return NS_ERROR_NULL_POINTER;

    *aNewsrcHasChanged = mNewsrcHasChanged;
    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::CloseCachedConnections()
{
    return WriteNewsrcFile();
}
