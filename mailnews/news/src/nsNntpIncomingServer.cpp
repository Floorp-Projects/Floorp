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
#include "nsIMsgNewsFolder.h"
#include "nsIFolder.h"
#include "nsIFileSpec.h"
#include "nsFileStream.h"
#include "nsCOMPtr.h"
#include "nsINntpService.h"
#include "nsINNTPProtocol.h"
#include "nsIRDFService.h"
#include "nsRDFCID.h"
#include "nsMsgNewsCID.h"
#include "nsNNTPProtocol.h"

#define NEW_NEWS_DIR_NAME        "News"
#define PREF_MAIL_NEWSRC_ROOT    "mail.newsrc_root"

// this platform specific junk is so the newsrc filenames we create 
// will resemble the migrated newsrc filenames.
#if defined(XP_UNIX) || defined(XP_BEOS)
#define NEWSRC_FILE_PREFIX "newsrc-"
#define NEWSRC_FILE_SUFFIX ""
#else
#define NEWSRC_FILE_PREFIX ""
#define NEWSRC_FILE_SUFFIX ".rc"
#endif /* XP_UNIX || XP_BEOS */

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);                            
static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kNntpServiceCID, NS_NNTPSERVICE_CID);

NS_IMPL_ADDREF_INHERITED(nsNntpIncomingServer, nsMsgIncomingServer)
NS_IMPL_RELEASE_INHERITED(nsNntpIncomingServer, nsMsgIncomingServer)

NS_INTERFACE_MAP_BEGIN(nsNntpIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsINntpIncomingServer)
    NS_INTERFACE_MAP_ENTRY(nsIUrlListener)
    NS_INTERFACE_MAP_ENTRY(nsISubscribableServer)
NS_INTERFACE_MAP_END_INHERITING(nsMsgIncomingServer)

nsNntpIncomingServer::nsNntpIncomingServer()
{    
  NS_INIT_REFCNT();

  mNewsrcHasChanged = PR_FALSE;
  mGroupsEnumerator = nsnull;
  NS_NewISupportsArray(getter_AddRefs(m_connectionCache));
  mHostInfoLoaded = PR_FALSE;
  mHostInfoHasChanged = PR_FALSE;
}



nsNntpIncomingServer::~nsNntpIncomingServer()
{
	nsresult rv;

	if (mGroupsEnumerator) {
    	delete mGroupsEnumerator;
		mGroupsEnumerator = nsnull;
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
	nsCAutoString newsrcFileName = NEWSRC_FILE_PREFIX;
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
	// iterate through the connection cache for a connection that can handle this url.
	PRUint32 cnt;
  nsCOMPtr<nsISupports> aSupport;
  nsCOMPtr<nsINNTPProtocol> connection;

  if (m_connectionCache)
  {
    nsresult rv = m_connectionCache->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    for (PRUint32 i = 0; i < cnt; i++) 
	  {
      aSupport = getter_AddRefs(m_connectionCache->ElementAt(i));
      connection = do_QueryInterface(aSupport);
		  if (connection)
    	  rv = connection->CloseConnection();
	  }
  }
  return WriteNewsrcFile();
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
    
  if (ConnectionTimeOut(connection))
      connection = null_nsCOMPtr();
  if (ConnectionTimeOut(freeConnection))
      freeConnection = null_nsCOMPtr();

	// if we got here and we have a connection, then we should return it!
	if (!isBusy && connection)
	{
		*aNntpConnection = freeConnection;
    freeConnection->Initialize(aUri, aMsgWindow);
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
nsNntpIncomingServer::AddSubscribedNewsgroups()
{
	nsresult rv;
    nsCOMPtr<nsIEnumerator> subFolders;
    nsCOMPtr<nsIFolder> rootFolder;
    nsCOMPtr<nsIFolder> currFolder;
 
    rv = GetRootFolder(getter_AddRefs(rootFolder));
    if (NS_FAILED(rv)) return rv;

	rv = rootFolder->GetSubFolders(getter_AddRefs(subFolders));
    if (NS_FAILED(rv)) return rv;

    nsAdapterEnumerator *simpleEnumerator = new nsAdapterEnumerator(subFolders);
    if (simpleEnumerator == nsnull) return NS_ERROR_OUT_OF_MEMORY;

    PRBool moreFolders;
        
    while (NS_SUCCEEDED(simpleEnumerator->HasMoreElements(&moreFolders)) && moreFolders) {
        nsCOMPtr<nsISupports> child;
        rv = simpleEnumerator->GetNext(getter_AddRefs(child));
        if (NS_SUCCEEDED(rv) && child) {
            currFolder = do_QueryInterface(child, &rv);
            if (NS_SUCCEEDED(rv) && currFolder) {
				nsXPIDLString name;
				rv = currFolder->GetName(getter_Copies(name));
				if (NS_SUCCEEDED(rv) && name) {
					nsCAutoString asciiName(name);
					rv = SetNewsgroupAsSubscribed((const char *)asciiName);
				}
            }
        }
    }

    delete simpleEnumerator;
	return NS_OK;
}

nsresult
nsNntpIncomingServer::SetNewsgroupAsSubscribed(const char *aName)
{
	nsresult rv;

	NS_ASSERTION(aName,"newsgroup with no name");
	if (!aName) return NS_ERROR_FAILURE;

#ifdef DEBUG_NEWS
	printf("SetNewsgroupAsSubscribed(%s)\n",aName);
#endif
	nsXPIDLCString serverUri;

	rv = GetServerURI(getter_Copies(serverUri));
	if (NS_FAILED(rv)) return rv;

	nsCAutoString groupUri;
	groupUri = (const char *)serverUri;
	groupUri += "/";
	groupUri += aName;

	nsCOMPtr <nsIRDFService> rdfService = do_GetService(kRDFServiceCID, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!rdfService) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIRDFResource> newsgroupResource;
	rv = rdfService->GetResource((const char *) groupUri, getter_AddRefs(newsgroupResource));

#if 0
	nsCOMPtr<nsIRDFLiteral> totalMessagesLiteral;
	nsAutoString totalMessagesString(aTotalMessages);
	rv = rdfService->GetLiteral(totalMessagesString.GetUnicode(), getter_AddRefs(totalMessagesLiteral));
	if(NS_FAILED(rv)) return rv;
	nsCOMPtr<nsIRDFResource> kNC_TotalMessages;
	rv = rdfService->GetResource("http://home.netscape.com/NC-rdf#TotalMessages", getter_AddRefs(kNC_TotalMessages));
	if(NS_FAILED(rv)) return rv;
#endif

	nsCOMPtr<nsIRDFLiteral> subscribedLiteral;
	nsAutoString subscribedString("true");
	rv = rdfService->GetLiteral(subscribedString.GetUnicode(), getter_AddRefs(subscribedLiteral));
	if(NS_FAILED(rv)) return rv;
	nsCOMPtr<nsIRDFResource> kNC_Subscribed;
	rv = rdfService->GetResource("http://home.netscape.com/NC-rdf#Subscribed", getter_AddRefs(kNC_Subscribed));
	if(NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFDataSource> ds;
	rv = rdfService->GetDataSource("rdf:subscribe",getter_AddRefs(ds));
	if(NS_FAILED(rv)) return rv;
	if (!ds) return NS_ERROR_FAILURE;

#if 0
	rv = ds->Assert(newsgroupResource, kNC_TotalMessages, totalMessagesLiteral, PR_TRUE);
	if(NS_FAILED(rv)) return rv;
#endif

	nsCOMPtr<nsIRDFNode> oldLiteral;
	rv = ds->GetTarget(newsgroupResource, kNC_Subscribed, PR_TRUE, getter_AddRefs(oldLiteral));
	if(NS_FAILED(rv)) return rv;

	rv = ds->Change(newsgroupResource, kNC_Subscribed, oldLiteral, subscribedLiteral);
	if(NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::AddNewNewsgroup(const char *aName)
{
	nsresult rv;

	NS_ASSERTION(aName,"attempting to add newsgroup with no name");
	if (!aName) return NS_ERROR_FAILURE;

#ifdef DEBUG_NEWS
	printf("AddNewNewsgroup(%s)\n",aName);
#endif
	nsXPIDLCString serverUri;

	rv = GetServerURI(getter_Copies(serverUri));
	if (NS_FAILED(rv)) return rv;

	nsCAutoString groupUri;
	groupUri = (const char *)serverUri;
	groupUri += "/";
	groupUri += aName;

	nsCOMPtr <nsIRDFService> rdfService = do_GetService(kRDFServiceCID, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!rdfService) return NS_ERROR_FAILURE;

	nsCOMPtr<nsIRDFResource> newsgroupResource;
	rv = rdfService->GetResource((const char *) groupUri, getter_AddRefs(newsgroupResource));
	
	nsCOMPtr<nsIRDFLiteral> nameLiteral;
	nsAutoString nameString(aName);
	rv = rdfService->GetLiteral(nameString.GetUnicode(), getter_AddRefs(nameLiteral));
	if(NS_FAILED(rv)) return rv;
	nsCOMPtr<nsIRDFResource> kNC_Name;
	rv = rdfService->GetResource("http://home.netscape.com/NC-rdf#Name", getter_AddRefs(kNC_Name));
	if(NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFLiteral> subscribedLiteral;
	nsAutoString subscribedString("false");
	rv = rdfService->GetLiteral(subscribedString.GetUnicode(), getter_AddRefs(subscribedLiteral));
	if(NS_FAILED(rv)) return rv;
	nsCOMPtr<nsIRDFResource> kNC_Subscribed;
	rv = rdfService->GetResource("http://home.netscape.com/NC-rdf#Subscribed", getter_AddRefs(kNC_Subscribed));
	if(NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFDataSource> ds;
	rv = rdfService->GetDataSource("rdf:subscribe",getter_AddRefs(ds));
	if(NS_FAILED(rv)) return rv;
	if (!ds) return NS_ERROR_FAILURE;

	rv = ds->Assert(newsgroupResource, kNC_Name, nameLiteral, PR_TRUE);
	if(NS_FAILED(rv)) return rv;

	rv = ds->Assert(newsgroupResource, kNC_Subscribed, subscribedLiteral, PR_TRUE);
	if(NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFResource> kNC_Child;
	rv = rdfService->GetResource("http://home.netscape.com/NC-rdf#child", getter_AddRefs(kNC_Child));
	if(NS_FAILED(rv)) return rv;

	// this assumes a lot about how we get back the list of newsgroups from the server
	PRInt32 slashpos = groupUri.RFindChar('/',PR_TRUE);
	PRInt32 dotpos = groupUri.RFindChar('.',PR_TRUE);
	nsCOMPtr <nsIRDFResource> parent;

	if (dotpos > slashpos) {
		groupUri.Truncate(dotpos);
	
		rv = rdfService->GetResource((const char *)groupUri, getter_AddRefs(parent));
		if(NS_FAILED(rv)) return rv;
	}
	else {
		rv = rdfService->GetResource((const char *)serverUri, getter_AddRefs(parent));
		if(NS_FAILED(rv)) return rv;
	}
		
	rv = ds->Assert(parent, kNC_Child, newsgroupResource, PR_TRUE);
	if(NS_FAILED(rv)) return rv;

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
	rv = AddSubscribedNewsgroups();
	if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISubscribeListener> listener;
	rv = GetSubscribeListener(getter_AddRefs(listener));
	if (NS_FAILED(rv)) return rv;
	if (!listener) return NS_ERROR_FAILURE;
	
	rv = listener->OnStopPopulating();
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

	nsAutoString newsgroupName(name);
	rv = msgfolder->CreateSubfolder(newsgroupName.GetUnicode());
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::PopulateSubscribeDatasource(nsIMsgWindow *aMsgWindow)
{
	nsresult rv;
	printf("in PopulateSubscribeDatasource()\n");
	nsCOMPtr<nsINntpService> nntpService = do_GetService(kNntpServiceCID, &rv);
	if (NS_FAILED(rv)) return rv;
	if (!nntpService) return NS_ERROR_FAILURE;

        rv = nntpService->BuildSubscribeDatasource(this, aMsgWindow);
        if (NS_FAILED(rv)) return rv;

        return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::SetSubscribeListener(nsISubscribeListener *aListener)
{
	if (!aListener) return NS_ERROR_NULL_POINTER;
	mSubscribeListener = aListener;
	return NS_OK;
}

NS_IMETHODIMP
nsNntpIncomingServer::GetSubscribeListener(nsISubscribeListener **aListener)
{
	if (!aListener) return NS_ERROR_NULL_POINTER;
	if (mSubscribeListener) {
			*aListener = mSubscribeListener;
			NS_ADDREF(*aListener);
	}
	return NS_OK;
}
