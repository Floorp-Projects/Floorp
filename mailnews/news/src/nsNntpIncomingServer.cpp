/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Seth Spitzer <sspitzer@netscape.com>
 * David Bienvenu <bienvenu@netscape.com>
 * Henrik Gemal <gemal@gemal.dk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsNntpIncomingServer.h"
#include "nsXPIDLString.h"
#include "nsEscape.h"
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
#include "nsInt64.h"
#include "nsMsgUtils.h"

#define INVALID_VERSION         0
#define VALID_VERSION           1
#define NEW_NEWS_DIR_NAME       "News"
#define PREF_MAIL_NEWSRC_ROOT   "mail.newsrc_root"
#define HOSTINFO_FILE_NAME      "hostinfo.dat"

#define NEWS_DELIMITER          '.'

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
static NS_DEFINE_CID(kSubscribableServerCID, NS_SUBSCRIBABLESERVER_CID);

NS_IMPL_ADDREF_INHERITED(nsNntpIncomingServer, nsMsgIncomingServer)
NS_IMPL_RELEASE_INHERITED(nsNntpIncomingServer, nsMsgIncomingServer)

NS_INTERFACE_MAP_BEGIN(nsNntpIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsINntpIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsIUrlListener)
    NS_INTERFACE_MAP_ENTRY(nsISubscribableServer)
    NS_INTERFACE_MAP_ENTRY(nsIOutlinerView)
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
  mHasSeenBeginGroups = PR_FALSE;
  mPostingAllowed = PR_FALSE;
  mLastUpdatedTime = 0;

  // these atoms are used for subscribe search
  mSubscribedAtom = getter_AddRefs(NS_NewAtom("subscribed"));
  mNntpAtom = getter_AddRefs(NS_NewAtom("nntp"));

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
NS_IMPL_SERVERPREF_BOOL(nsNntpIncomingServer, PushAuth, "always_authenticate");
NS_IMPL_SERVERPREF_INT(nsNntpIncomingServer, MaxArticles, "max_articles");

NS_IMETHODIMP
nsNntpIncomingServer::GetNewsrcFilePath(nsIFileSpec **aNewsrcFilePath)
{
  nsresult rv;
  if (mNewsrcFilePath)
  {
    *aNewsrcFilePath = mNewsrcFilePath;
    NS_IF_ADDREF(*aNewsrcFilePath);
    return NS_OK;
  }

  rv = GetFileValue("newsrc.file", aNewsrcFilePath);
  if (NS_SUCCEEDED(rv) && *aNewsrcFilePath) 
  {
    mNewsrcFilePath = *aNewsrcFilePath;
    return rv;
  }

  rv = GetNewsrcRootPath(getter_AddRefs(mNewsrcFilePath));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString hostname;
  rv = GetHostName(getter_Copies(hostname));
  if (NS_FAILED(rv)) return rv;

  // set the leaf name to "dummy", and then call MakeUnique with a suggested leaf name
  rv = mNewsrcFilePath->AppendRelativeUnixPath("dummy");
  if (NS_FAILED(rv)) return rv;
  nsCAutoString newsrcFileName(NEWSRC_FILE_PREFIX);
  newsrcFileName.Append(hostname);
  newsrcFileName.Append(NEWSRC_FILE_SUFFIX);
  rv = mNewsrcFilePath->MakeUniqueWithSuggestedName((const char *)newsrcFileName);
  if (NS_FAILED(rv)) return rv;

  rv = SetNewsrcFilePath(mNewsrcFilePath);
  if (NS_FAILED(rv)) return rv;

  *aNewsrcFilePath = mNewsrcFilePath;
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
    
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
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
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &rv));
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
  PRUint32 cnt;
  nsCOMPtr<nsISupports> aSupport;
  nsCOMPtr<nsINNTPProtocol> connection;

  // iterate through the connection cache and close the connections.
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
#ifdef DEBUG_seth
      printf("XXX connection timed out, close it, and remove it from the connection cache\n");
#endif
      aConnection->CloseConnection();
            m_connectionCache->RemoveElement(aConnection);
            retVal = PR_TRUE;
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

  nsresult rv = protocolInstance->QueryInterface(NS_GET_IID(nsINNTPProtocol), (void **) aNntpConnection);
  // take the protocol instance and add it to the connectionCache
  if (NS_SUCCEEDED(rv) && *aNntpConnection)
    m_connectionCache->AppendElement(*aNntpConnection);
  return rv;
}

/* By default, allow the user to open at most this many connections to one news host */
#define kMaxConnectionsPerHost 2

NS_IMETHODIMP
nsNntpIncomingServer::GetNntpConnection(nsIURI * aUri, nsIMsgWindow *aMsgWindow,
                                           nsINNTPProtocol ** aNntpConnection)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsINNTPProtocol> connection;
  nsCOMPtr<nsINNTPProtocol> freeConnection;
  PRBool isBusy = PR_TRUE;


  PRInt32 maxConnections = kMaxConnectionsPerHost; 
  rv = GetMaximumConnectionsNumber(&maxConnections);
  if (NS_FAILED(rv) || maxConnections == 0)
  {
    maxConnections = kMaxConnectionsPerHost;
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
#ifdef DEBUG_seth
  printf("XXX there are %d nntp connections in the conn cache.\n", (int)cnt);
#endif
  for (PRUint32 i = 0; i < cnt && isBusy; i++) 
  {
    aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
    connection = do_QueryInterface(aSupport);
    if (connection)
        rv = connection->GetIsBusy(&isBusy);
    if (NS_FAILED(rv)) 
    {
        connection = nsnull;
        continue;
    }
    if (!freeConnection && !isBusy && connection)
    {
       freeConnection = connection;
    }
  }
    
  if (ConnectionTimeOut(freeConnection))
      freeConnection = nsnull;

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

  // a user might have a new server without any groups.
  // if so, bail out.  no need to establish a connection to the server
  PRInt32 numGroups = 0;
  rv = GetNumGroupsNeedingCounts(&numGroups);
  NS_ENSURE_SUCCESS(rv,rv);

  if (!numGroups)
    return NS_OK;

  nsCOMPtr<nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  rv = nntpService->UpdateCounts(this, aMsgWindow);
  NS_ENSURE_SUCCESS(rv,rv);
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


PRBool
checkIfSubscribedFunction(nsCString &aElement, void *aData)
{
	if (nsCRT::strcmp((const char *)aData, (const char *)aElement) == 0) {
		return PR_FALSE;
	}
	else {
		return PR_TRUE;
	}
}


NS_IMETHODIMP
nsNntpIncomingServer::ContainsNewsgroup(const char *name, PRBool *containsGroup)
{
	NS_ASSERTION(name && nsCRT::strlen(name),"no name");
	if (!name || !containsGroup) return NS_ERROR_NULL_POINTER;
	if (!nsCRT::strlen(name)) return NS_ERROR_FAILURE;

	*containsGroup = !(mSubscribedNewsgroups.EnumerateForwards((nsCStringArrayEnumFunc)checkIfSubscribedFunction, (void *)name));
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SubscribeToNewsgroup(const char *name)
{
	nsresult rv;

	NS_ASSERTION(name && nsCRT::strlen(name),"no name");
	if (!name) return NS_ERROR_NULL_POINTER;
	if (!nsCRT::strlen(name)) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIFolder> folder;
	rv = GetRootFolder(getter_AddRefs(folder));
	if (NS_FAILED(rv)) return rv;
	if (!folder) return NS_ERROR_FAILURE;
	
	nsCOMPtr<nsIMsgFolder> msgfolder = do_QueryInterface(folder, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!msgfolder) return NS_ERROR_FAILURE;

	nsXPIDLString newsgroupName;
	rv = NS_MsgDecodeUnescapeURLPath(name, getter_Copies(newsgroupName));
	NS_ENSURE_SUCCESS(rv,rv);

	rv = msgfolder->CreateSubfolder(newsgroupName.get(), nsnull);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

PRBool
writeGroupToHostInfoFile(nsCString &aElement, void *aData)
{
    nsIOFileStream *stream;
    stream = (nsIOFileStream *)aData;
    NS_ASSERTION(stream, "no stream");
    if (!stream) {
        // stop, something is bad.
        return PR_FALSE;
    }

    nsXPIDLString name;
    nsresult rv = NS_MsgDecodeUnescapeURLPath(aElement.get(), getter_Copies(name)); 
    if (NS_FAILED(rv)) {
        // stop, something is bad.
        return PR_FALSE;
    }

    nsCAutoString nameOnDisk;
    nameOnDisk.AssignWithConversion(name.get());

    // XXX todo ",,1,0,0" is a temporary hack, fix it
    *stream << nameOnDisk.get() << ",,1,0,0" << MSG_LINEBREAK;
    return PR_TRUE;
}

nsresult
nsNntpIncomingServer::WriteHostInfoFile()
{
    nsresult rv = NS_OK;

    if (!mHostInfoHasChanged) {
        return NS_OK;
	}

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
	*mHostInfoStream << "" << MSG_LINEBREAK;
	*mHostInfoStream << "begingroups" << MSG_LINEBREAK;

	// XXX todo, sort groups first?

	mGroupsOnServer.EnumerateForwards((nsCStringArrayEnumFunc)writeGroupToHostInfoFile, (void *)mHostInfoStream);

    mHostInfoStream->close();
    delete mHostInfoStream;
    mHostInfoStream = nsnull;

	mHostInfoHasChanged = PR_FALSE;
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

    char *buffer = nsnull;
    rv = mHostInfoFile->OpenStreamForReading();
    NS_ENSURE_SUCCESS(rv,rv);

    PRInt32 numread = 0;

    if (NS_FAILED(mHostInfoInputStream.GrowBuffer(HOSTINFO_FILE_BUFFER_SIZE))) {
    	return NS_ERROR_FAILURE;
    }
	
	mHasSeenBeginGroups = PR_FALSE;

    while (1) {
        buffer = mHostInfoInputStream.GetBuffer();
        rv = mHostInfoFile->Read(&buffer, HOSTINFO_FILE_BUFFER_SIZE, &numread);
        NS_ENSURE_SUCCESS(rv,rv);
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

    mHostInfoFile->CloseStream();
     
	rv = UpdateSubscribed();
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

  nsCOMPtr<nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  mHostInfoLoaded = PR_FALSE;
  mVersion = INVALID_VERSION;
  mGroupsOnServer.Clear();

  if (!aForceToServer) {
	rv = LoadHostInfoFile();	
  	if (NS_FAILED(rv)) return rv;
  }

  // mHostInfoLoaded can be false if we failed to load anything
  if (!mHostInfoLoaded || (mVersion != VALID_VERSION)) {
    // set these to true, so when we are done and we call WriteHostInfoFile() 
    // we'll write out to hostinfo.dat
	mHostInfoHasChanged = PR_TRUE;
	mVersion = VALID_VERSION;

	mGroupsOnServer.Clear();

	rv = nntpService->GetListOfGroupsOnServer(this, aMsgWindow);
	if (NS_FAILED(rv)) return rv;
  }
  else {
	rv = StopPopulating(aMsgWindow);
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
    mTempSubscribed.AppendCString(nsCAutoString(path));

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
	mTempSubscribed.Clear();
	mSubscribedNewsgroups.EnumerateForwards((nsCStringArrayEnumFunc)setAsSubscribedFunction, (void *)this);
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::AddTo(const char *aName, PRBool addAsSubscribed, PRBool changeIfExists)
{
    nsresult rv = EnsureInner();
    NS_ENSURE_SUCCESS(rv,rv);

    nsAutoString newsgroupName;
    newsgroupName.AssignWithConversion(aName);

    char *escapedName = nsEscape(NS_ConvertUCS2toUTF8(newsgroupName.get()).get(), url_Path);
    if (!escapedName) return NS_ERROR_OUT_OF_MEMORY;

    rv = AddGroupOnServer(escapedName);
    NS_ENSURE_SUCCESS(rv,rv);
 
    rv = mInner->AddTo(escapedName,addAsSubscribed,changeIfExists);
    NS_ENSURE_SUCCESS(rv,rv);

    PR_FREEIF(escapedName);
    return rv;
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

	rv = serverFolder->PropagateDelete(newsgroupFolder, PR_TRUE /* delete storage */, nsnull);
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
		NS_ASSERTION(NS_SUCCEEDED(rv),"failed to add line");
		if (NS_SUCCEEDED(rv)) {
          // since we've seen one group, we can claim we've loaded the hostinfo file
          mHostInfoLoaded = PR_TRUE;
		}
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
			} else if (PL_strcmp(line, "version") == 0) {
				mVersion = strtol(equalPos, nsnull, 16);
			}
		}	
	}

	return 0;
}

nsresult
nsNntpIncomingServer::AddGroupOnServer(const char *name)
{
	mGroupsOnServer.AppendCString(nsCAutoString(name));
	return NS_OK;
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

    rv = mInner->SetState(path, state, stateChanged);
    if (*stateChanged) {
      if (state)
        mTempSubscribed.AppendCString(nsCAutoString(path));
      else
        mTempSubscribed.RemoveCString(nsCAutoString(path));
    }
    return rv;
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

    // we force the newrc to be dirty, so it will get written out when
    // we call WriteNewsrcFile()
    rv = SetNewsrcHasChanged(PR_TRUE);
    NS_ENSURE_SUCCESS(rv,rv);
    return WriteNewsrcFile();
}

NS_IMETHODIMP
nsNntpIncomingServer::ForgetPassword()
{
    nsresult rv;

    // clear password of root folder (for the news account)
    nsCOMPtr<nsIFolder> rootFolder;
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    NS_ENSURE_SUCCESS(rv,rv);
    if (!rootFolder) return NS_ERROR_FAILURE;

    nsCOMPtr <nsIMsgNewsFolder> newsFolder = do_QueryInterface(rootFolder, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    if (!newsFolder) return NS_ERROR_FAILURE;

    rv = newsFolder->ForgetGroupUsername();
    NS_ENSURE_SUCCESS(rv,rv);
    rv = newsFolder->ForgetGroupPassword();
    NS_ENSURE_SUCCESS(rv,rv);

    // clear password of all child folders
    nsCOMPtr<nsIEnumerator> subFolders;

    rv = rootFolder->GetSubFolders(getter_AddRefs(subFolders));
    NS_ENSURE_SUCCESS(rv,rv);

    nsAdapterEnumerator *simpleEnumerator = new nsAdapterEnumerator(subFolders);
    if (!simpleEnumerator) return NS_ERROR_OUT_OF_MEMORY;

    PRBool moreFolders = PR_FALSE;
        
    nsresult return_rv = NS_OK;

    while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders)) && moreFolders) {
        nsCOMPtr<nsISupports> child;
        rv = simpleEnumerator->GetNext(getter_AddRefs(child));
        if (NS_SUCCEEDED(rv) && child) {
            newsFolder = do_QueryInterface(child, &rv);
            if (NS_SUCCEEDED(rv) && newsFolder) {
                rv = newsFolder->ForgetGroupUsername();
                if (NS_FAILED(rv)) return_rv = rv;
                rv = newsFolder->ForgetGroupPassword();
                if (NS_FAILED(rv)) return_rv = rv;
            }
            else {
                return_rv = NS_ERROR_FAILURE;
            }
        }
    }
    delete simpleEnumerator;

    return return_rv;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetSupportsExtensions(PRBool *aSupportsExtensions)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetSupportsExtensions(PRBool aSupportsExtensions)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::AddExtension(const char *extension)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
   
NS_IMETHODIMP
nsNntpIncomingServer::QueryExtension(const char *extension, PRBool *result)
{
#ifdef DEBUG_seth
  printf("no extension support yet\n");
#endif
  *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetPostingAllowed(PRBool *aPostingAllowed)
{
  *aPostingAllowed = mPostingAllowed;
  return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetPostingAllowed(PRBool aPostingAllowed)
{
  mPostingAllowed = aPostingAllowed;
  return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetLastUpdatedTime(PRUint32 *aLastUpdatedTime)
{
  *aLastUpdatedTime = mLastUpdatedTime;
  return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetLastUpdatedTime(PRUint32 aLastUpdatedTime)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::AddPropertyForGet(const char *name, const char *value)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::QueryPropertyForGet(const char *name, char **value)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
  
NS_IMETHODIMP
nsNntpIncomingServer::AddSearchableGroup(const char *name)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::QuerySearchableGroup(const char *name, PRBool *result)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::AddSearchableHeader(const char *name)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::QuerySearchableHeader(const char *name, PRBool *result)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}
  
NS_IMETHODIMP
nsNntpIncomingServer::FindGroup(const char *name, nsIMsgNewsFolder **result)
{
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(result);

  nsresult rv;
  nsCOMPtr<nsIFolder> rootFolder;
  rv = GetRootFolder(getter_AddRefs(rootFolder));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!rootFolder) return NS_ERROR_FAILURE;

  nsCOMPtr <nsIMsgFolder> serverFolder = do_QueryInterface(rootFolder, &rv);
  NS_ENSURE_SUCCESS(rv,rv);
  if (!serverFolder) return NS_ERROR_FAILURE;

  nsCOMPtr <nsIFolder> subFolder;
  rv = serverFolder->FindSubFolder(name, getter_AddRefs(subFolder));
  NS_ENSURE_SUCCESS(rv,rv);
  if (!subFolder) return NS_ERROR_FAILURE;

  rv = subFolder->QueryInterface(NS_GET_IID(nsIMsgNewsFolder), (void**)result);
  NS_ENSURE_SUCCESS(rv,rv);
  if (!*result) return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetFirstGroupNeedingExtraInfo(char **result)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetGroupNeedsExtraInfo(const char *name, PRBool needsExtraInfo)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsNntpIncomingServer::GroupNotFound(const char *name, PRBool opening)
{
  NS_ENSURE_ARG_POINTER(name);
  printf("alert, asking if the user wants to auto unsubscribe from %s\n", name);
  return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetPrettyNameForGroup(const char *name, const char *prettyName)
{
  NS_ASSERTION(0,"not implemented");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetCanSearchMessages(PRBool *canSearchMessages)
{
    NS_ENSURE_ARG_POINTER(canSearchMessages);
    *canSearchMessages = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetOfflineSupportLevel(PRInt32 *aSupportLevel)
{
    NS_ENSURE_ARG_POINTER(aSupportLevel);
    nsresult rv;
    
    rv = GetIntValue("offline_support_level", aSupportLevel);
    if (*aSupportLevel != OFFLINE_SUPPORT_LEVEL_UNDEFINED) return rv;
    
    // set default value
    *aSupportLevel = OFFLINE_SUPPORT_LEVEL_EXTENDED;
    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetDefaultCopiesAndFoldersPrefsToServer(PRBool *aCopiesAndFoldersOnServer)
{
    NS_ENSURE_ARG_POINTER(aCopiesAndFoldersOnServer);

    /**
     * When a news account is created, the copies and folder prefs for the 
     * associated identity don't point to folders on the server. 
     * This makes sense, since there is no "Drafts" folder on a news server.
     * They'll point to the ones on "Local Folders"
     */

    *aCopiesAndFoldersOnServer = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetCanCreateFoldersOnServer(PRBool *aCanCreateFoldersOnServer)
{
    NS_ENSURE_ARG_POINTER(aCanCreateFoldersOnServer);

    // No folder creation on news servers. Return false.
    *aCanCreateFoldersOnServer = PR_FALSE;
    return NS_OK;
}

PRBool
buildSubscribeSearchResult(nsCString &aElement, void *aData)
{
    nsresult rv = NS_OK;
    nsNntpIncomingServer *server;
    server = (nsNntpIncomingServer *)aData;
    NS_ASSERTION(server, "no server");
    if (!server) {
        return PR_FALSE;
    }
 
    rv = server->AppendIfSearchMatch(aElement.get());
    NS_ASSERTION(NS_SUCCEEDED(rv),"AddSubscribeSearchResult failed");
    return PR_TRUE;
}

nsresult
nsNntpIncomingServer::AppendIfSearchMatch(const char *newsgroupName)
{
    // we've converted mSearchValue to lower case
    // do the same to the newsgroup name before we do our strstr()
    // this way searches will be case independant
    nsCAutoString lowerCaseName(newsgroupName);
    lowerCaseName.ToLowerCase();

    if (PL_strstr(lowerCaseName.get(), mSearchValue.get())) {
        mSubscribeSearchResult.AppendCString(nsCAutoString(newsgroupName));
    }
    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetSearchValue(const char *searchValue)
{
    mSearchValue = searchValue;
    // force the search string to be lower case
    // so that we can do case insensitive searching
    mSearchValue.ToLowerCase();

    mSubscribeSearchResult.Clear();
    mGroupsOnServer.EnumerateForwards((nsCStringArrayEnumFunc)buildSubscribeSearchResult, (void *)this);

    if (mOutliner) {
     mOutliner->Invalidate();
     mOutliner->InvalidateScrollbar();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetSupportsSubscribeSearch(PRBool *retVal)
{
    *retVal = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetRowCount(PRInt32 *aRowCount)
{
    *aRowCount = mSubscribeSearchResult.Count();
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetSelection(nsIOutlinerSelection * *aSelection)
{
  *aSelection = mOutlinerSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::SetSelection(nsIOutlinerSelection * aSelection)
{
  mOutlinerSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetCellProperties(PRInt32 row, const PRUnichar *colID, nsISupportsArray *properties)
{
    if (colID[0] == 's') { 
        // if <name> is in our temporary list of subscribed groups
        // add the "subscribed" property so the check mark shows up
        // in the "subscribedCol"
        nsCString name;
        mSubscribeSearchResult.CStringAt(row, name);
        if (mTempSubscribed.IndexOf(name) != -1) {
          properties->AppendElement(mSubscribedAtom); 
        }
    }
    else if (colID[0] == 'n') {
      // add the "nntp" property to the "nameCol" 
      // so we get the news folder icon in the search view
      properties->AppendElement(mNntpAtom); 
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetColumnProperties(const PRUnichar *colID, nsIDOMElement *colElt, nsISupportsArray *properties)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::IsContainer(PRInt32 index, PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::IsSeparator(PRInt32 index, PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::IsSorted(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::CanDropOn(PRInt32 index, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::CanDropBeforeAfter(PRInt32 index, PRBool before, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::Drop(PRInt32 row, PRInt32 orientation)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetLevel(PRInt32 index, PRInt32 *_retval)
{
    *_retval = 0;
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::GetCellText(PRInt32 row, const PRUnichar *colID, PRUnichar **_retval)
{
    if (colID[0] == 'n') {
      nsCString str;
      mSubscribeSearchResult.CStringAt(row, str);
      // some servers have newsgroup names that are non ASCII.  we store those as escaped
      // unescape here so the UI is consistent
      nsresult rv = NS_MsgDecodeUnescapeURLPath(str.get(), _retval);
      NS_ENSURE_SUCCESS(rv,rv);
    }
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::SetOutliner(nsIOutlinerBoxObject *outliner)
{
  mOutliner = outliner;
  return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::ToggleOpenState(PRInt32 index)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::CycleHeader(const PRUnichar *colID, nsIDOMElement *elt)
{
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::SelectionChanged()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::CycleCell(PRInt32 row, const PRUnichar *colID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::IsEditable(PRInt32 row, const PRUnichar *colID, PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP 
nsNntpIncomingServer::SetCellText(PRInt32 row, const PRUnichar *colID, const PRUnichar *value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::PerformAction(const PRUnichar *action)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsNntpIncomingServer::PerformActionOnCell(const PRUnichar *action, PRInt32 row, const PRUnichar *colID)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetCanFileMessagesOnServer(PRBool *aCanFileMessagesOnServer)
{
    NS_ENSURE_ARG_POINTER(aCanFileMessagesOnServer);

    // No folder creation on news servers. Return false.
    *aCanFileMessagesOnServer = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetFilterScope(nsMsgSearchScopeValue *filterScope)
{
   NS_ENSURE_ARG_POINTER(filterScope);

   *filterScope = nsMsgSearchScope::news;
   return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetSearchScope(nsMsgSearchScopeValue *searchScope)
{
   NS_ENSURE_ARG_POINTER(searchScope);

   if (WeAreOffline()) {
     *searchScope = nsMsgSearchScope::localNews;
   }
   else {
     *searchScope = nsMsgSearchScope::news;
   }
   return NS_OK;
}

