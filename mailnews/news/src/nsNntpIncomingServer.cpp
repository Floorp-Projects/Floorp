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
 * Seth Spitzer <sspitzer@netscape.com>
 * David Bienvenu <bienvenu@netscape.com>
 * Henrik Gemal <gemal@gemal.dk>
 */

#include "nsNntpIncomingServer.h"
#include "nsXPIDLString.h"
#include "nsIPref.h"
#include "nsIMsgNewsFolder.h"
#include "nsIFolder.h"
#include "nsIFileSpec.h"
#include "nsCOMPtr.h"
#include "nsINntpService.h"
#include "nsINNTPProtocol.h"
#include "nsMsgNewsCID.h"
#include "nsNNTPProtocol.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"

#define INVALID_VERSION         0
#define VALID_VERSION			1
#define NEW_NEWS_DIR_NAME        "News"
#define PREF_MAIL_NEWSRC_ROOT    "mail.newsrc_root"
#define HOSTINFO_FILE_NAME		 "hostinfo.dat"

#define NEWS_DELIMITER   '.'

// this platform specific junk is so the newsrc filenames we create 
// will resemble the migrated newsrc filenames.
#if defined(XP_UNIX) || defined(XP_BEOS)
#define NEWSRC_FILE_PREFIX "newsrc-"
#define NEWSRC_FILE_SUFFIX ""
#else
#define NEWSRC_FILE_PREFIX ""
#define NEWSRC_FILE_SUFFIX ".rc"
#endif /* XP_UNIX || XP_BEOS */

// ###tw  This really ought to be the most
// efficient file reading size for the current
// operating system.
#define HOSTINFO_FILE_BUFFER_SIZE 1024

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);                            
static NS_DEFINE_CID(kNntpServiceCID, NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kSubscribableServerCID, NS_SUBSCRIBABLESERVER_CID);

NS_IMPL_ADDREF_INHERITED(nsNntpIncomingServer, nsMsgIncomingServer)
NS_IMPL_RELEASE_INHERITED(nsNntpIncomingServer, nsMsgIncomingServer)

NS_INTERFACE_MAP_BEGIN(nsNntpIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsINntpIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsIUrlListener)
    NS_INTERFACE_MAP_ENTRY(nsISubscribableServer)
    NS_INTERFACE_MAP_ENTRY(nsISubscribeDumpListener)
NS_INTERFACE_MAP_END_INHERITING(nsMsgIncomingServer)

nsNntpIncomingServer::nsNntpIncomingServer() : nsMsgLineBuffer(nsnull, PR_FALSE)
{    
  NS_INIT_REFCNT();

  mNewsrcHasChanged = PR_FALSE;
  mGroupsEnumerator = nsnull;
  NS_NewISupportsArray(getter_AddRefs(m_connectionCache));

  mHostInfoLoaded = PR_FALSE;
  mHostInfoHasChanged = PR_FALSE;
  mVersion = INVALID_VERSION;

  mHostInfoStream = nsnull;

  mLastGroupDate = 0;
  mUniqueId = 0;
  mPushAuth = PR_FALSE;
  mHasSeenBeginGroups = PR_FALSE;
  SetupNewsrcSaveTimer();
}

nsNntpIncomingServer::~nsNntpIncomingServer()
{
	nsresult rv;

    if (mGroupsEnumerator) {
        delete mGroupsEnumerator;
        mGroupsEnumerator = nsnull;
    }

    if (mNewsrcSaveTimer) {
        mNewsrcSaveTimer->Cancel();
        mNewsrcSaveTimer = nsnull;
    }

    if (mHostInfoStream) {
        mHostInfoStream->close();
        delete mHostInfoStream;
        mHostInfoStream = nsnull;
    }

    rv = ClearInner();
	NS_ASSERTION(NS_SUCCEEDED(rv), "ClearInner failed");

    rv = CloseCachedConnections();
	NS_ASSERTION(NS_SUCCEEDED(rv), "CloseCachedConnections failed");
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

	// set the leaf name to "dummy", and then call MakeUnique with a suggested leaf name
	rv = path->AppendRelativeUnixPath("dummy");
	if (NS_FAILED(rv)) return rv;
	nsCAutoString newsrcFileName(NEWSRC_FILE_PREFIX);
	newsrcFileName.Append(hostname);
	newsrcFileName.Append(NEWSRC_FILE_SUFFIX);
	rv = path->MakeUniqueWithSuggestedName((const char *)newsrcFileName);
	if (NS_FAILED(rv)) return rv;

	rv = SetNewsrcFilePath(path);
	if (NS_FAILED(rv)) return rv;

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
    NS_ENSURE_ARG_POINTER(aNewsrcRootPath);
    *aNewsrcRootPath = nsnull;
    
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
    
    PRBool havePref = PR_FALSE;
    nsCOMPtr<nsIFile> localFile;
    nsCOMPtr<nsILocalFile> prefLocal;
    rv = prefs->GetFileXPref(PREF_MAIL_NEWSRC_ROOT, getter_AddRefs(prefLocal));
    if (NS_SUCCEEDED(rv)) {
        localFile = prefLocal;
        havePref = PR_TRUE;
    }
    if (!localFile) {
        rv = NS_GetSpecialDirectory(NS_APP_NEWS_50_DIR, getter_AddRefs(localFile));
        if (NS_FAILED(rv)) return rv;
        havePref = PR_FALSE;
    }
        
    PRBool exists;
    rv = localFile->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
        rv = localFile->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;
    
    // Make the resulting nsIFileSpec
    // TODO: Convert arg to nsILocalFile and avoid this
    nsXPIDLCString pathBuf;
    rv = localFile->GetPath(getter_Copies(pathBuf));
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIFileSpec> outSpec;
    rv = NS_NewFileSpec(getter_AddRefs(outSpec));
    if (NS_FAILED(rv)) return rv;
    outSpec->SetNativePath(pathBuf);
    
    if (!havePref || !exists)
        rv = SetNewsrcRootPath(outSpec);
        
    *aNewsrcRootPath = outSpec;
    NS_IF_ADDREF(*aNewsrcRootPath);
    return rv;
}

/* static */ void nsNntpIncomingServer::OnNewsrcSaveTimer(nsITimer *timer, void *voidIncomingServer)
{
	nsNntpIncomingServer *incomingServer = (nsNntpIncomingServer*)voidIncomingServer;
	incomingServer->WriteNewsrcFile();		
}


nsresult nsNntpIncomingServer::SetupNewsrcSaveTimer()
{
	nsInt64 ms(5000 * 60);   // hard code, 5 minutes.
	//Convert biffDelay into milliseconds
	PRUint32 timeInMSUint32 = (PRUint32)ms;
	//Can't currently reset a timer when it's in the process of
	//calling Notify. So, just release the timer here and create a new one.
	if(mNewsrcSaveTimer)
	{
		mNewsrcSaveTimer->Cancel();
	}
    mNewsrcSaveTimer = do_CreateInstance("@mozilla.org/timer;1");
	mNewsrcSaveTimer->Init(OnNewsrcSaveTimer, (void*)this, timeInMSUint32, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);

    return NS_OK;
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
		if (NS_FAILED(rv)) return rv;
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
	nsresult rv;
	// iterate through the connection cache for a connection that can handle this url.
	PRUint32 cnt;
  nsCOMPtr<nsISupports> aSupport;
  nsCOMPtr<nsINNTPProtocol> connection;

  if (m_connectionCache)
  {
    rv = m_connectionCache->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    for (PRUint32 i = 0; i < cnt; i++) 
	  {
      aSupport = getter_AddRefs(m_connectionCache->ElementAt(0));
      connection = do_QueryInterface(aSupport);
		  if (connection)
      {
    	  rv = connection->CloseConnection();
        RemoveConnection(connection);
      }
	  }
  }
  rv = WriteNewsrcFile();
  if (NS_FAILED(rv)) return rv;

  rv = WriteHostInfoFile(); 
  if (NS_FAILED(rv)) return rv;
	
  return NS_OK;
}

NS_IMPL_SERVERPREF_INT(nsNntpIncomingServer, MaximumConnectionsNumber,
                       "max_cached_connections");

PRBool
nsNntpIncomingServer::ConnectionTimeOut(nsINNTPProtocol* aConnection)
{
    PRBool retVal = PR_FALSE;
    if (!aConnection) return retVal;
    nsresult rv;

    PRTime cacheTimeoutLimits;

    LL_I2L(cacheTimeoutLimits, 170 * 1000000); // 170 seconds in microseconds
    PRTime lastActiveTimeStamp;
    rv = aConnection->GetLastActiveTimeStamp(&lastActiveTimeStamp);

    PRTime elapsedTime;
    LL_SUB(elapsedTime, PR_Now(), lastActiveTimeStamp);
    PRTime t;
    LL_SUB(t, elapsedTime, cacheTimeoutLimits);
    if (LL_GE_ZERO(t))
    {
        nsCOMPtr<nsINNTPProtocol> aProtocol(do_QueryInterface(aConnection,
                                                              &rv));
        if (NS_SUCCEEDED(rv) && aProtocol)
        {
            m_connectionCache->RemoveElement(aConnection);
            retVal = PR_TRUE;
        }
    }
    return retVal;
}


nsresult
nsNntpIncomingServer::CreateProtocolInstance(nsINNTPProtocol ** aNntpConnection, nsIURI *url,
                                             nsIMsgWindow *aMsgWindow)
{
	// create a new connection and add it to the connection cache
	// we may need to flag the protocol connection as busy so we don't get
    // a race 
	// condition where someone else goes through this code 
	nsNNTPProtocol * protocolInstance = new nsNNTPProtocol(url, aMsgWindow);
  if (!protocolInstance)
    return NS_ERROR_OUT_OF_MEMORY;
//	nsresult rv = nsComponentManager::CreateInstance(kImapProtocolCID, nsnull,
//                                            NS_GET_IID(nsINntpProtocol),
//                                            (void **) &protocolInstance);
  nsresult rv = protocolInstance->QueryInterface(NS_GET_IID(nsINNTPProtocol), (void **) aNntpConnection);
	// take the protocol instance and add it to the connectionCache
	if (NS_SUCCEEDED(rv) && *aNntpConnection)
		m_connectionCache->AppendElement(*aNntpConnection);
	return rv;
}


NS_IMETHODIMP
nsNntpIncomingServer::GetNntpConnection(nsIURI * aUri, nsIMsgWindow *aMsgWindow,
                                           nsINNTPProtocol ** aNntpConnection)
{
	nsresult rv = NS_OK;
	nsCOMPtr<nsINNTPProtocol> connection;
	nsCOMPtr<nsINNTPProtocol> freeConnection;
  PRBool isBusy = PR_FALSE;


  PRInt32 maxConnections = 2; // default to be 2
  rv = GetMaximumConnectionsNumber(&maxConnections);
  if (NS_FAILED(rv) || maxConnections == 0)
  {
    maxConnections = 2;
    rv = SetMaximumConnectionsNumber(maxConnections);
  }
  else if (maxConnections < 1)
  {   // forced to use at least 1
    maxConnections = 1;
    rv = SetMaximumConnectionsNumber(maxConnections);
  }

  *aNntpConnection = nsnull;
	// iterate through the connection cache for a connection that can handle this url.
	PRUint32 cnt;
  nsCOMPtr<nsISupports> aSupport;

  rv = m_connectionCache->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  for (PRUint32 i = 0; i < cnt && !isBusy; i++) 
	{
    aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
    connection = do_QueryInterface(aSupport);
		if (connection)
    	rv = connection->IsBusy(&isBusy);
    if (NS_FAILED(rv)) 
    {
        connection = null_nsCOMPtr();
        continue;
    }
    if (!freeConnection && !isBusy && connection)
    {
       freeConnection = connection;
    }
	}
    
  if (ConnectionTimeOut(freeConnection))
      freeConnection = null_nsCOMPtr();

	// if we got here and we have a connection, then we should return it!
	if (!isBusy && freeConnection)
	{
		*aNntpConnection = freeConnection;
    freeConnection->SetIsCachedConnection(PR_TRUE);
		NS_IF_ADDREF(*aNntpConnection);
	}
	else // have no queueing mechanism - just create the protocol instance.
	{	
		rv = CreateProtocolInstance(aNntpConnection, aUri, aMsgWindow);
	}
  return rv;
}


/* void RemoveConnection (in nsINNTPProtocol aNntpConnection); */
NS_IMETHODIMP nsNntpIncomingServer::RemoveConnection(nsINNTPProtocol *aNntpConnection)
{
    if (aNntpConnection)
        m_connectionCache->RemoveElement(aNntpConnection);

    return NS_OK;
}


NS_IMETHODIMP 
nsNntpIncomingServer::PerformExpand(nsIMsgWindow *aMsgWindow)
{
	nsresult rv;
#ifdef DEBUG_NEWS
	printf("PerformExpand for nntp\n");
#endif

	nsCOMPtr<nsINntpService> nntpService = do_GetService(kNntpServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;
	if (!nntpService) return NS_ERROR_FAILURE;

	rv = nntpService->UpdateCounts(this, aMsgWindow);
    if (NS_FAILED(rv)) return rv;
	return NS_OK;
}

// remove this when news supports filters
NS_IMETHODIMP
nsNntpIncomingServer::GetFilterList(nsIMsgFilterList **aResult)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetNumGroupsNeedingCounts(PRInt32 *aNumGroupsNeedingCounts)
{
	nsresult rv;

    nsCOMPtr<nsIEnumerator> subFolders;
    nsCOMPtr<nsIFolder> rootFolder;
 
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    if (NS_FAILED(rv)) return rv;

	PRBool hasSubFolders = PR_FALSE;
	rv = rootFolder->GetHasSubFolders(&hasSubFolders);
    if (NS_FAILED(rv)) return rv;
	
	if (!hasSubFolders) {
		*aNumGroupsNeedingCounts = 0;
		return NS_OK;
	}

	rv = rootFolder->GetSubFolders(getter_AddRefs(subFolders));
    if (NS_FAILED(rv)) return rv;

	if (mGroupsEnumerator) {
    	delete mGroupsEnumerator;
		mGroupsEnumerator = nsnull;
	}
	mGroupsEnumerator = new nsAdapterEnumerator(subFolders);
    if (mGroupsEnumerator == nsnull) return NS_ERROR_OUT_OF_MEMORY;

	PRUint32 count = 0;
	rv = rootFolder->Count(&count);
    if (NS_FAILED(rv)) return rv;
		
	*aNumGroupsNeedingCounts = (PRInt32) count;
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetFirstGroupNeedingCounts(nsISupports **aFirstGroupNeedingCounts)
{
	nsresult rv;

	if (!aFirstGroupNeedingCounts) return NS_ERROR_NULL_POINTER;

    PRBool moreFolders;
    if (!mGroupsEnumerator) return NS_ERROR_FAILURE;

	rv = mGroupsEnumerator->HasMoreElements(&moreFolders);
	if (NS_FAILED(rv)) return rv;

	if (!moreFolders) {
		*aFirstGroupNeedingCounts = nsnull;
    	delete mGroupsEnumerator;
		mGroupsEnumerator = nsnull;
		return NS_OK; // this is not an error - it just means we reached the end of the groups.
	}

    rv = mGroupsEnumerator->GetNext(aFirstGroupNeedingCounts);
	if (NS_FAILED(rv)) return rv;
	if (!*aFirstGroupNeedingCounts) return NS_ERROR_FAILURE;

	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::DisplaySubscribedGroup(nsIMsgNewsFolder *aMsgFolder, PRInt32 aFirstMessage, PRInt32 aLastMessage, PRInt32 aTotalMessages)
{
	nsresult rv;

	if (!aMsgFolder) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG_NEWS
	printf("DisplaySubscribedGroup(...,%ld,%ld,%ld)\n",aFirstMessage,aLastMessage,aTotalMessages);
#endif
	rv = aMsgFolder->UpdateSummaryFromNNTPInfo(aFirstMessage,aLastMessage,aTotalMessages);
	return rv;
}

NS_IMETHODIMP
nsNntpIncomingServer::PerformBiff()
{
#ifdef DEBUG_NEWS
	printf("PerformBiff for nntp\n");
#endif
	return PerformExpand(nsnull);
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetServerRequiresPasswordForBiff(PRBool *_retval)
{
    if (!_retval) return NS_ERROR_NULL_POINTER;

	*_retval = PR_FALSE;
	return NS_OK;
}


NS_IMETHODIMP
nsNntpIncomingServer::OnStartRunningUrl(nsIURI *url)
{
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::OnStopRunningUrl(nsIURI *url, nsresult exitCode)
{
	nsresult rv;
	rv = UpdateSubscribed();
	if (NS_FAILED(rv)) return rv;

	rv = StopPopulating(mMsgWindow);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::ContainsNewsgroup(const char *name, PRBool *containsGroup)
{
	nsresult rv;

	NS_ASSERTION(name && PL_strlen(name),"no name");
	if (!name || !containsGroup) return NS_ERROR_NULL_POINTER;
	if (!nsCRT::strlen(name)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIFolder> folder;
	rv = GetRootFolder(getter_AddRefs(folder));
	if (NS_FAILED(rv)) return rv;
	if (!folder) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsIMsgFolder> msgfolder = do_QueryInterface(folder, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!msgfolder) return NS_ERROR_FAILURE;

	rv = msgfolder->ContainsChildNamed(name, containsGroup);
	if (NS_FAILED(rv)) return NS_ERROR_FAILURE;

	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SubscribeToNewsgroup(const char *name)
{
	nsresult rv;

	NS_ASSERTION(name && PL_strlen(name),"no name");
	if (!name) return NS_ERROR_NULL_POINTER;
	if (!nsCRT::strlen(name)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIFolder> folder;
	rv = GetRootFolder(getter_AddRefs(folder));
	if (NS_FAILED(rv)) return rv;
	if (!folder) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsIMsgFolder> msgfolder = do_QueryInterface(folder, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!msgfolder) return NS_ERROR_FAILURE;

	nsAutoString newsgroupName; newsgroupName.AssignWithConversion(name);
	rv = msgfolder->CreateSubfolder(newsgroupName.GetUnicode());
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

nsresult
nsNntpIncomingServer::WriteHostInfoFile()
{
    nsresult rv = NS_OK;

#ifdef DEBUG_NEWS
	printf("WriteHostInfoFile()\n");
#endif

    if (!mHostInfoHasChanged) {
#ifdef DEBUG_NEWS
        printf("don't write out the hostinfo file\n");
#endif
        return NS_OK;
    }
#ifdef DEBUG_NEWS
    else {
        printf("write out the hostinfo file\n");
    }
#endif

    rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mInner->SetDumpListener(this);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = mInner->DumpTree();
    NS_ENSURE_SUCCESS(rv,rv);

    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::StartDumping()
{
	nsresult rv;
#ifdef DEBUG_NEWS
    printf("start dumping\n");
#endif

	PRInt32 firstnewdate;

	LL_L2I(firstnewdate, mFirstNewDate);

	nsXPIDLCString hostname;
	rv = GetHostName(getter_Copies(hostname));
    NS_ENSURE_SUCCESS(rv,rv);
	
    nsFileSpec hostinfoFileSpec;
    rv = mHostInfoFile->GetFileSpec(&hostinfoFileSpec);
    NS_ENSURE_SUCCESS(rv,rv);

    if (mHostInfoStream) {
        mHostInfoStream->close();
        delete mHostInfoStream;
        mHostInfoStream = nsnull;
    }

    mHostInfoStream = new nsIOFileStream(hostinfoFileSpec, (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));
    NS_ASSERTION(mHostInfoStream, "no stream!");

    // todo, missing some formatting, see the 4.x code
    *mHostInfoStream << "# News host information file." << MSG_LINEBREAK;
	*mHostInfoStream << "# This is a generated file!  Do not edit." << MSG_LINEBREAK;
	*mHostInfoStream << "" << MSG_LINEBREAK;
	*mHostInfoStream << "version=" << VALID_VERSION << MSG_LINEBREAK;
	*mHostInfoStream << "newsrcname=" << (const char*)hostname << MSG_LINEBREAK;
	*mHostInfoStream << "lastgroupdate=" << mLastGroupDate << MSG_LINEBREAK;
	*mHostInfoStream << "firstnewdate=" << firstnewdate << MSG_LINEBREAK;
	*mHostInfoStream << "uniqueid=" << mUniqueId << MSG_LINEBREAK;
	*mHostInfoStream << "pushauth=" << mPushAuth << MSG_LINEBREAK;
	*mHostInfoStream << "" << MSG_LINEBREAK;
	*mHostInfoStream << "begingroups" << MSG_LINEBREAK;

    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::DumpItem(const char *name)
{
    NS_ASSERTION(mHostInfoStream, "no stream!");
    // todo ",,1,0,0" is a temporary hack, fix it
    *mHostInfoStream << name << ",,1,0,0" << MSG_LINEBREAK;
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::DoneDumping()
{
    nsresult rv;

#ifdef DEBUG_NEWS
    printf("done dumping\n");
#endif

    NS_ASSERTION(mHostInfoStream, "no stream!");
    mHostInfoStream->close();
    delete mHostInfoStream;
    mHostInfoStream = nsnull;

	mHostInfoHasChanged = PR_FALSE;

    rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    rv = mInner->SetDumpListener(nsnull);
    NS_ENSURE_SUCCESS(rv,rv);

	return NS_OK;
}

nsresult
nsNntpIncomingServer::LoadHostInfoFile()
{
	nsresult rv;
	
    // we haven't loaded it yet
    mHostInfoLoaded = PR_FALSE;

	rv = GetLocalPath(getter_AddRefs(mHostInfoFile));
	if (NS_FAILED(rv)) return rv;
	if (!mHostInfoFile) return NS_ERROR_FAILURE;

	rv = mHostInfoFile->AppendRelativeUnixPath(HOSTINFO_FILE_NAME);
	if (NS_FAILED(rv)) return rv;

	PRBool exists;
	rv = mHostInfoFile->Exists(&exists);
	if (NS_FAILED(rv)) return rv;

	// it is ok if the hostinfo.dat file does not exist.
	if (!exists) return NS_OK;

	nsInputFileStream hostinfoStream(mHostInfoFile); 

    PRInt32 numread = 0;

    if (NS_FAILED(mHostInfoInputStream.GrowBuffer(HOSTINFO_FILE_BUFFER_SIZE))) {
    	return NS_ERROR_FAILURE;
    }
	
	mHasSeenBeginGroups = PR_FALSE;

    while (1) {
    	numread = hostinfoStream.read(mHostInfoInputStream.GetBuffer(), HOSTINFO_FILE_BUFFER_SIZE);
        if (numread == 0) {
      		break;
    	}
    	else {
      		rv = BufferInput(mHostInfoInputStream.GetBuffer(), numread);
      		if (NS_FAILED(rv)) {
        		break;
      		}
    	}
  	}

  	hostinfoStream.close();
    
	rv = UpdateSubscribed();
	if (NS_FAILED(rv)) return rv;

	rv = StopPopulating(mMsgWindow);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::StartPopulatingWithUri(nsIMsgWindow *aMsgWindow, PRBool aForceToServer, const char *uri)
{
	nsresult rv = NS_OK;

#ifdef DEBUG_seth
	printf("StartPopulatingWithUri(%s)\n",uri);
#endif

    rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    rv = mInner->StartPopulatingWithUri(aMsgWindow, aForceToServer, uri);
    NS_ENSURE_SUCCESS(rv,rv);

	rv = StopPopulating(mMsgWindow);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SubscribeCleanup()
{
	nsresult rv = NS_OK;
    rv = ClearInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::StartPopulating(nsIMsgWindow *aMsgWindow, PRBool aForceToServer)
{
  nsresult rv;

  mMsgWindow = aMsgWindow;

  rv = EnsureInner();
  NS_ENSURE_SUCCESS(rv,rv);
  rv = mInner->StartPopulating(aMsgWindow, aForceToServer);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = SetDelimiter(NEWS_DELIMITER);
  if (NS_FAILED(rv)) return rv;
    
  rv = SetShowFullName(PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsINntpService> nntpService = do_GetService(kNntpServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  if (!nntpService) return NS_ERROR_FAILURE; 

  mHostInfoLoaded = PR_FALSE;
  mVersion = INVALID_VERSION;

  if (!aForceToServer) {
	rv = LoadHostInfoFile();	
  	if (NS_FAILED(rv)) return rv;
  }

  // mHostInfoLoaded can be false if we failed to load anything
  if (!mHostInfoLoaded || (mVersion != VALID_VERSION)) {
    // set these to true, so when we call WriteHostInfoFile() 
    // we write it out to disk
	mHostInfoHasChanged = PR_TRUE;
	mVersion = VALID_VERSION;

    // todo, make sure inner has been freed before we start?

	rv = nntpService->GetListOfGroupsOnServer(this, aMsgWindow);
	if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::AddNewsgroupToList(const char *aName)
{
	nsresult rv;

	rv = AddTo(aName, PR_FALSE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetIncomingServer(nsIMsgIncomingServer *aServer)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetIncomingServer(aServer);
}

NS_IMETHODIMP
nsNntpIncomingServer::SetShowFullName(PRBool showFullName)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetShowFullName(showFullName);
}

nsresult
nsNntpIncomingServer::ClearInner()
{
    nsresult rv = NS_OK;

    if (mInner) {
        rv = mInner->SetSubscribeListener(nsnull);
        NS_ENSURE_SUCCESS(rv,rv);

        rv = mInner->SetIncomingServer(nsnull);
        NS_ENSURE_SUCCESS(rv,rv);

        mInner = nsnull;
    }
    return NS_OK;
}

nsresult
nsNntpIncomingServer::EnsureInner()
{
    nsresult rv = NS_OK;

    if (mInner) return NS_OK;

    mInner = do_CreateInstance(kSubscribableServerCID,&rv);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!mInner) return NS_ERROR_FAILURE;

    rv = SetIncomingServer(this);
    NS_ENSURE_SUCCESS(rv,rv);

    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetDelimiter(char *aDelimiter)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->GetDelimiter(aDelimiter);
}

NS_IMETHODIMP
nsNntpIncomingServer::SetDelimiter(char aDelimiter)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetDelimiter(aDelimiter);
}

NS_IMETHODIMP
nsNntpIncomingServer::SetAsSubscribed(const char *path)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetAsSubscribed(path);
}

PRBool
setAsSubscribedFunction(nsCString &aElement, void *aData)
{
	nsresult rv = NS_OK;
    nsNntpIncomingServer *server;
    server = (nsNntpIncomingServer *)aData;
    NS_ASSERTION(server, "no server");
    if (!server) {
        // todo is this correct?
        return PR_FALSE;
    }
 
	rv = server->SetAsSubscribed((const char *)aElement);
	NS_ASSERTION(NS_SUCCEEDED(rv),"SetAsSubscribed failed");
	return PR_TRUE;
}

NS_IMETHODIMP
nsNntpIncomingServer::UpdateSubscribed()
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	mSubscribedNewsgroups.EnumerateForwards((nsCStringArrayEnumFunc)setAsSubscribedFunction, (void *)this);
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::AddTo(const char *aName, PRBool addAsSubscribed, PRBool changeIfExists)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->AddTo(aName,addAsSubscribed,changeIfExists);
}

NS_IMETHODIMP
nsNntpIncomingServer::StopPopulating(nsIMsgWindow *aMsgWindow)
{
	nsresult rv = NS_OK;

    nsCOMPtr<nsISubscribeListener> listener;
	rv = GetSubscribeListener(getter_AddRefs(listener));
    NS_ENSURE_SUCCESS(rv,rv);
	if (!listener) return NS_ERROR_FAILURE;

	rv = listener->OnDonePopulating();
    NS_ENSURE_SUCCESS(rv,rv);

    rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	rv = mInner->StopPopulating(aMsgWindow);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = WriteHostInfoFile();
    if (NS_FAILED(rv)) return rv;

	//xxx todo when do I set this to null?
	//rv = ClearInner();
    //NS_ENSURE_SUCCESS(rv,rv);
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetSubscribeListener(nsISubscribeListener *aListener)
{	
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->SetSubscribeListener(aListener);
}

NS_IMETHODIMP
nsNntpIncomingServer::GetSubscribeListener(nsISubscribeListener **aListener)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
	return mInner->GetSubscribeListener(aListener);
}

NS_IMETHODIMP
nsNntpIncomingServer::Subscribe(const PRUnichar *aUnicharName)
{
	nsCAutoString name;
	name.AssignWithConversion(aUnicharName);
	return SubscribeToNewsgroup((const char *)name);
}

NS_IMETHODIMP
nsNntpIncomingServer::Unsubscribe(const PRUnichar *aUnicharName)
{
	nsresult rv;
	nsCAutoString name;
	name.AssignWithConversion(aUnicharName);

	nsCOMPtr<nsIFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    if (NS_FAILED(rv)) return rv;
	if (!rootFolder) return NS_ERROR_FAILURE;

    nsCOMPtr <nsIMsgFolder> serverFolder = do_QueryInterface(rootFolder, &rv);
    if (NS_FAILED(rv)) return rv;	
	if (!serverFolder) return NS_ERROR_FAILURE;

	nsCOMPtr <nsIFolder> subFolder;
	rv = serverFolder->FindSubFolder(name, getter_AddRefs(subFolder));
    if (NS_FAILED(rv)) return rv;	

    nsCOMPtr <nsIMsgFolder> newsgroupFolder = do_QueryInterface(subFolder, &rv);
    if (NS_FAILED(rv)) return rv;	
	if (!newsgroupFolder) return NS_ERROR_FAILURE;

	rv = serverFolder->PropagateDelete(newsgroupFolder, PR_TRUE /* delete storage */);
    if (NS_FAILED(rv)) return rv;	

	/* since we've unsubscribed to a newsgroup, the newsrc needs to be written out */
    rv = SetNewsrcHasChanged(PR_TRUE);
	if (NS_FAILED(rv)) return rv;
	return NS_OK;
}

PRInt32
nsNntpIncomingServer::HandleLine(char* line, PRUint32 line_size)
{
	NS_ASSERTION(line, "line is null");
	if (!line) return 0;

	// skip blank lines and comments
	if (line[0] == '#' || line[0] == '\0') return 0;
	
	line[line_size] = 0;

	if (mHasSeenBeginGroups) {
		char *commaPos = PL_strchr(line,',');
		if (commaPos) *commaPos = 0;

		nsresult rv = AddTo(line, PR_FALSE, PR_TRUE);
	    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to add the group");
        // since we've seen one group, we can claim we've loaded the hostinfo file
        mHostInfoLoaded = PR_TRUE;
	}
	else {
		if (nsCRT::strncmp(line,"begingroups", 11) == 0) {
			mHasSeenBeginGroups = PR_TRUE;
		}
		char*equalPos = PL_strchr(line, '=');	
		if (equalPos) {
			*equalPos++ = '\0';
			if (PL_strcmp(line, "lastgroupdate") == 0) {
				mLastGroupDate = strtol(equalPos, nsnull, 16);
			} else if (PL_strcmp(line, "firstnewdate") == 0) {
				PRInt32 firstnewdate = strtol(equalPos, nsnull, 16);
				LL_I2L(mFirstNewDate, firstnewdate);
			} else if (PL_strcmp(line, "uniqueid") == 0) {
				mUniqueId = strtol(equalPos, nsnull, 16);
			} else if (PL_strcmp(line, "pushauth") == 0) {
				mPushAuth = strtol(equalPos, nsnull, 16);
			} else if (PL_strcmp(line, "version") == 0) {
				mVersion = strtol(equalPos, nsnull, 16);
			}
		}	
	}

	return 0;
}


NS_IMETHODIMP
nsNntpIncomingServer::AddNewsgroup(const char *name)
{
	// handle duplicates?
	mSubscribedNewsgroups.AppendCString(nsCAutoString(name));
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::RemoveNewsgroup(const char *name)
{
	// handle duplicates?
	mSubscribedNewsgroups.RemoveCString(nsCAutoString(name));
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetState(const char *path, PRBool state, PRBool *stateChanged)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->SetState(path, state, stateChanged);
}

NS_IMETHODIMP
nsNntpIncomingServer::HasChildren(const char *path, PRBool *aHasChildren)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->HasChildren(path, aHasChildren);
}

NS_IMETHODIMP
nsNntpIncomingServer::IsSubscribed(const char *path, PRBool *aIsSubscribed)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->IsSubscribed(path, aIsSubscribed);
}

NS_IMETHODIMP
nsNntpIncomingServer::GetLeafName(const char *path, PRUnichar **aLeafName)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->GetLeafName(path, aLeafName);
}

NS_IMETHODIMP
nsNntpIncomingServer::GetFirstChildURI(const char * path, char **aResult)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->GetFirstChildURI(path, aResult);
}

NS_IMETHODIMP
nsNntpIncomingServer::GetChildren(const char *path, nsISupportsArray *array)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->GetChildren(path, array);
}

NS_IMETHODIMP
nsNntpIncomingServer::CommitSubscribeChanges()
{
    nsresult rv;
    rv = SetNewsrcHasChanged(PR_TRUE);
    NS_ENSURE_SUCCESS(rv,rv);

    return WriteNewsrcFile();
}

NS_IMETHODIMP
nsNntpIncomingServer::DumpTree()
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->DumpTree();
}

NS_IMETHODIMP
nsNntpIncomingServer::SetDumpListener(nsISubscribeDumpListener *dumpListener)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);
    return mInner->SetDumpListener(dumpListener);
}
