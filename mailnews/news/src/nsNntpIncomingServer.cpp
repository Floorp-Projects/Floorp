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
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsIMsgNewsFolder.h"
#include "nsIFolder.h"
#include "nsIFileSpec.h"
#include "nsFileStream.h"
#include "nsCOMPtr.h"
#include "nsINntpService.h"
#include "nsINNTPProtocol.h"
#include "nsRDFCID.h"
#include "nsMsgNewsCID.h"
#include "nsNNTPProtocol.h"

#define VALID_VERSION			1
#define NEW_NEWS_DIR_NAME        "News"
#define PREF_MAIL_NEWSRC_ROOT    "mail.newsrc_root"
#define HOSTINFO_FILE_NAME		 "hostinfo.dat"

#define HOSTINFO_COUNT_MAX 200 /* number of groups to process at a time when reading the list from the hostinfo.dat file */
#define HOSTINFO_TIMEOUT 5   /* uSec to wait until doing more */

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
static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kNntpServiceCID, NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kSubscribableServerCID, NS_SUBSCRIBABLESERVER_CID);

NS_IMPL_ADDREF_INHERITED(nsNntpIncomingServer, nsMsgIncomingServer)
NS_IMPL_RELEASE_INHERITED(nsNntpIncomingServer, nsMsgIncomingServer)

NS_INTERFACE_MAP_BEGIN(nsNntpIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsINntpIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsIUrlListener)
    NS_INTERFACE_MAP_ENTRY(nsISubscribableServer)
	NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_INHERITING(nsMsgIncomingServer)

nsNntpIncomingServer::nsNntpIncomingServer() : nsMsgLineBuffer(nsnull, PR_FALSE)
{    
  NS_INIT_REFCNT();

  mNewsrcHasChanged = PR_FALSE;
  mGroupsEnumerator = nsnull;
  NS_NewISupportsArray(getter_AddRefs(m_connectionCache));
  mHostInfoLoaded = PR_FALSE;
  mHostInfoHasChanged = PR_FALSE;
  mLastGroupDate = 0;
  mUniqueId = 0;
  mPushAuth = PR_FALSE;
  mHasSeenBeginGroups = PR_FALSE;
  mVersion = 0;
  mGroupsOnServerIndex = 0;
  mGroupsOnServerCount = 0;
  SetupNewsrcSaveTimer();
}

nsNntpIncomingServer::~nsNntpIncomingServer()
{
	nsresult rv;

	if (mGroupsEnumerator) {
    	delete mGroupsEnumerator;
		mGroupsEnumerator = nsnull;
	}

	if (mUpdateTimer) {
      mUpdateTimer->Cancel();
      mUpdateTimer = nsnull;
    }
  if (mNewsrcSaveTimer)
  {
    mNewsrcSaveTimer->Cancel();
    mNewsrcSaveTimer = nsnull;
  }

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
  mNewsrcSaveTimer = do_CreateInstance("component://netscape/timer");
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
      aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
      connection = do_QueryInterface(aSupport);
		  if (connection)
    	  rv = connection->CloseConnection();
	  }
  }
  rv = WriteNewsrcFile();
  if (NS_FAILED(rv)) return rv;

  if (mHostInfoHasChanged) {
  	rv = WriteHostInfoFile(); 
  	if (NS_FAILED(rv)) return rv;
  }
	
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
		return NS_OK;
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
	rv = UpdateSubscribedInSubscribeDS();
	if (NS_FAILED(rv)) return rv;

	rv = StopPopulatingSubscribeDS();
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

PRBool
writeGroupToHostInfo(nsCString &aElement, void *aData)
{
	nsIOFileStream *stream;
	stream = (nsIOFileStream *)aData;
	
    // ",,1,0,0" is a temporary hack
	*stream << (const char *)aElement << ",,1,0,0" << MSG_LINEBREAK;

	return PR_TRUE;
}

nsresult
nsNntpIncomingServer::WriteHostInfoFile()
{
	nsresult rv;
#ifdef DEBUG_NEWS
	printf("WriteHostInfoFile()\n");
#endif

	PRInt32 firstnewdate;

	LL_L2I(firstnewdate, mFirstNewDate);

	nsXPIDLCString hostname;
	rv = GetHostName(getter_Copies(hostname));
	if (NS_FAILED(rv)) return rv;
	

    nsFileSpec hostinfoFileSpec;
    rv = mHostInfoFile->GetFileSpec(&hostinfoFileSpec);
    if (NS_FAILED(rv)) return rv;

    nsIOFileStream hostinfoStream(hostinfoFileSpec, (PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE));

#ifdef DEBUG_NEWS
	printf("xxx todo missing some formatting, need to fix this, see nsNNTPHost.cpp\n");
#endif

    hostinfoStream << "# News host information file." << MSG_LINEBREAK;
	hostinfoStream << "# This is a generated file!  Do not edit." << MSG_LINEBREAK;
	hostinfoStream << "" << MSG_LINEBREAK;
	hostinfoStream << "version=1" << MSG_LINEBREAK;
	hostinfoStream << "newsrcname=" << (const char*)hostname << MSG_LINEBREAK;
	hostinfoStream << "lastgroupdate=" << mLastGroupDate << MSG_LINEBREAK;
	hostinfoStream << "firstnewdate=" << firstnewdate << MSG_LINEBREAK;
	hostinfoStream << "uniqueid=" << mUniqueId << MSG_LINEBREAK;
	hostinfoStream << "pushauth=" << mPushAuth << MSG_LINEBREAK;
	hostinfoStream << "" << MSG_LINEBREAK;
	hostinfoStream << "begingroups" << MSG_LINEBREAK;

	mGroupsOnServer.EnumerateForwards((nsCStringArrayEnumFunc)writeGroupToHostInfo, (void *)&hostinfoStream);

	mHostInfoHasChanged = PR_FALSE;

    hostinfoStream.close();
	return NS_OK;
}

nsresult
nsNntpIncomingServer::LoadHostInfoFile()
{
	nsresult rv;
	
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
#ifdef DEBUG_NEWS
	printf("LoadHostInfoFile()\n");
#endif
  	mHostInfoLoaded = PR_TRUE;
	return NS_OK;
}

nsresult
nsNntpIncomingServer::PopulateSubscribeDatasourceFromHostInfo()
{
	nsresult rv;
#ifdef DEBUG_NEWS
	printf("PopulateSubscribeDatasourceFromHostInfo()\n");
#endif
	mGroupsOnServerCount = mGroupsOnServer.Count();
	nsCString currentGroup;

	while (mGroupsOnServerIndex < mGroupsOnServerCount) {
		mGroupsOnServer.CStringAt(mGroupsOnServerIndex, currentGroup);
		rv = AddToSubscribeDS((const char *)currentGroup, PR_FALSE, PR_TRUE);
		if (NS_FAILED(rv)) return rv;
#ifdef DEBUG_NEWS
		printf("%d = %s\n",mGroupsOnServerIndex,(const char *)currentGroup);
#endif

		mGroupsOnServerIndex++;
		if ((mGroupsOnServerIndex % HOSTINFO_COUNT_MAX) == 0) break;
	}


	if (mMsgWindow) {
		nsCOMPtr <nsIMsgStatusFeedback> msgStatusFeedback;

		rv = mMsgWindow->GetStatusFeedback(getter_AddRefs(msgStatusFeedback));
		if (NS_FAILED(rv)) return rv;

		if (mGroupsOnServerCount != 0) {
			// xxx todo
			// if old percent == new percent, don't call ShowProgress()
			// why poke the front end, through xpconnect, if we don't have to?
			rv = msgStatusFeedback->ShowProgress((mGroupsOnServerIndex * 100) / mGroupsOnServerCount);
		}
		if (NS_FAILED(rv)) return rv;
	}

	if (mGroupsOnServerIndex < mGroupsOnServerCount) {
#ifdef DEBUG_NEWS
		printf("there is more to do...\n");
#endif
		if (mUpdateTimer) {
			mUpdateTimer->Cancel();
			mUpdateTimer = nsnull;
		}

    	mUpdateTimer = do_CreateInstance("component://netscape/timer", &rv);
    	NS_ASSERTION(NS_SUCCEEDED(rv),"failed to create timer");
    	if (NS_FAILED(rv)) return rv;

		const PRUint32 kUpdateTimerDelay = HOSTINFO_TIMEOUT;
        rv = mUpdateTimer->Init(NS_STATIC_CAST(nsITimerCallback*,this), kUpdateTimerDelay);
        NS_ASSERTION(NS_SUCCEEDED(rv),"failed to init timer");
        if (NS_FAILED(rv)) return rv;
	}
	else {
#ifdef DEBUG_NEWS
		printf("we are done\n");
#endif
		rv = UpdateSubscribedInSubscribeDS();
		if (NS_FAILED(rv)) return rv;

		rv = StopPopulatingSubscribeDS();
		if (NS_FAILED(rv)) return rv;
	}

	return NS_OK;
}

void
nsNntpIncomingServer::Notify(nsITimer *timer)
{
  NS_ASSERTION(timer == mUpdateTimer.get(), "Hey, this ain't my timer!");
  mUpdateTimer = nsnull;    // release my hold  
  PopulateSubscribeDatasourceFromHostInfo();
}

NS_IMETHODIMP
nsNntpIncomingServer::PopulateSubscribeDatasourceWithUri(nsIMsgWindow *aMsgWindow, PRBool aForceToServer, const char *uri)
{
	nsresult rv;

#ifdef DEBUG_NEWS
	printf("PopulateSubscribeDatasourceWithUri(%s)\n",uri);
#endif

	rv = StopPopulatingSubscribeDS();
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SubscribeCleanup()
{
	nsresult rv;
	if (mInner) {
		rv = mInner->SetSubscribeListener(nsnull);
		if (NS_FAILED(rv)) return rv;

		mInner = nsnull;
	}
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::PopulateSubscribeDatasource(nsIMsgWindow *aMsgWindow, PRBool aForceToServer)
{
  nsresult rv;

  rv = StartPopulatingSubscribeDS();
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsINntpService> nntpService = do_GetService(kNntpServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  if (!nntpService) return NS_ERROR_FAILURE; 

  if (!aForceToServer && !mHostInfoLoaded) {
	// will set mHostInfoLoaded, if we were able to load the hostinfo.dat file
	rv = LoadHostInfoFile();	

  	if (NS_FAILED(rv)) return rv;
  }

  if (!aForceToServer && mHostInfoLoaded && (mVersion == VALID_VERSION)) {
	mGroupsOnServerIndex = 0;
	mGroupsOnServerCount = mGroupsOnServer.Count();
	mMsgWindow = aMsgWindow;

	rv = PopulateSubscribeDatasourceFromHostInfo();
	if (NS_FAILED(rv)) return rv;
  }
  else {
	// xxx todo move this somewhere else
	mHostInfoHasChanged = PR_TRUE;
	mVersion = 1;
	mHostInfoLoaded = PR_TRUE;
	mGroupsOnServer.Clear();

	rv = nntpService->BuildSubscribeDatasource(this, aMsgWindow);
	if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::AddNewsgroupToSubscribeDS(const char *aName)
{
	nsresult rv;

	// since this comes from the server, append it to the list
	mGroupsOnServer.AppendCString(nsCAutoString(aName));

	rv = AddToSubscribeDS(aName, PR_FALSE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetIncomingServer(nsIMsgIncomingServer *aServer)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetIncomingServer(aServer);
}

NS_IMETHODIMP
nsNntpIncomingServer::SetShowFullName(PRBool showFullName)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetShowFullName(showFullName);
}

NS_IMETHODIMP
nsNntpIncomingServer::SetDelimiter(char aDelimiter)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetDelimiter(aDelimiter);
}

NS_IMETHODIMP
nsNntpIncomingServer::SetAsSubscribedInSubscribeDS(const char *aURI)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetAsSubscribedInSubscribeDS(aURI);
}

PRBool
setAsSubscribedFunction(nsCString &aElement, void *aData)
{
	nsresult rv;
	nsNntpIncomingServer *server;
	server = (nsNntpIncomingServer *)aData;
	
	nsXPIDLCString serverURI;

	nsCOMPtr<nsIFolder> rootFolder;
    rv = server->GetRootFolder(getter_AddRefs(rootFolder));
    if (NS_FAILED(rv)) return rv;
	if (!rootFolder) return NS_ERROR_FAILURE;

	rv = rootFolder->GetURI(getter_Copies(serverURI));
    if (NS_FAILED(rv)) return rv;

	nsCAutoString uriStr;
	uriStr = (const char*)serverURI;
	uriStr += '/';
	uriStr += (const char *)aElement;

	rv = server->SetAsSubscribedInSubscribeDS((const char *)uriStr);

	NS_ASSERTION(NS_SUCCEEDED(rv),"SetAsSubscribedInSubscribeDS failed");
	return PR_TRUE;
}

NS_IMETHODIMP
nsNntpIncomingServer::UpdateSubscribedInSubscribeDS()
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;

	mSubscribedNewsgroups.EnumerateForwards((nsCStringArrayEnumFunc)setAsSubscribedFunction, (void *)this);
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::AddToSubscribeDS(const char *aName, PRBool addAsSubscribed, PRBool changeIfExists)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->AddToSubscribeDS(aName,addAsSubscribed,changeIfExists);
}

NS_IMETHODIMP
nsNntpIncomingServer::SetPropertiesInSubscribeDS(const char *uri, const PRUnichar *aName, nsIRDFResource *aResource, PRBool subscribed, PRBool changeIfExists)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->SetPropertiesInSubscribeDS(uri,aName,aResource,subscribed,changeIfExists);
}

NS_IMETHODIMP
nsNntpIncomingServer::FindAndAddParentToSubscribeDS(const char *uri, const char *serverUri, const char *aName, nsIRDFResource *aChildResource)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	return mInner->FindAndAddParentToSubscribeDS(uri,serverUri,aName,aChildResource);
}

NS_IMETHODIMP
nsNntpIncomingServer::StopPopulatingSubscribeDS()
{
	nsresult rv;

    nsCOMPtr<nsISubscribeListener> listener;
	rv = GetSubscribeListener(getter_AddRefs(listener));
	if (NS_FAILED(rv)) return rv;
	if (!listener) return NS_ERROR_FAILURE;

	rv = listener->OnStopPopulating();
	if (NS_FAILED(rv)) return rv;

	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
	rv = mInner->StopPopulatingSubscribeDS();
	if (NS_FAILED(rv)) return rv;

	//xxx todo when do I set this to null?
	//mInner = nsnull;
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::StartPopulatingSubscribeDS()
{
	nsresult rv;

	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;

    rv = SetIncomingServer(this);
    if (NS_FAILED(rv)) return rv;

    rv = SetDelimiter('.');
    if (NS_FAILED(rv)) return rv;

	rv = SetShowFullName(PR_TRUE);
    if (NS_FAILED(rv)) return rv;

	return mInner->StartPopulatingSubscribeDS();
}

NS_IMETHODIMP
nsNntpIncomingServer::SetSubscribeListener(nsISubscribeListener *aListener)
{	
	nsresult rv;
	mInner = do_CreateInstance(kSubscribableServerCID,&rv);
    if (NS_FAILED(rv)) return rv;
    if (!mInner) return NS_ERROR_FAILURE;

	return mInner->SetSubscribeListener(aListener);
}

NS_IMETHODIMP
nsNntpIncomingServer::GetSubscribeListener(nsISubscribeListener **aListener)
{
	NS_ASSERTION(mInner,"not initialized");
	if (!mInner) return NS_ERROR_FAILURE;
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
		nsCString str(line);
		mGroupsOnServer.AppendCString(str);
	}
	else {
		if (nsCRT::strncmp(line,"begingroups", 11) == 0) {
			mGroupsOnServer.Clear();
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
