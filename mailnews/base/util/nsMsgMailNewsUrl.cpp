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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "msgCore.h"
#include "nsMsgMailNewsUrl.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"
#include "nsXPIDLString.h"
#include "nsIDocumentLoader.h"
#include "nsILoadGroup.h"
#include "nsIWebShell.h"

static NS_DEFINE_CID(kUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

nsMsgMailNewsUrl::nsMsgMailNewsUrl()
{
    NS_INIT_REFCNT();
 
	// nsIURI specific state
	m_errorMessage = nsnull;
	m_runningUrl = PR_FALSE;
	m_updatingFolder = PR_FALSE;
  m_addContentToCache = PR_FALSE;

	nsComponentManager::CreateInstance(kUrlListenerManagerCID, nsnull, NS_GET_IID(nsIUrlListenerManager), (void **) getter_AddRefs(m_urlListeners));
	nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, NS_GET_IID(nsIURL), (void **) getter_AddRefs(m_baseURL));
}
 
nsMsgMailNewsUrl::~nsMsgMailNewsUrl()
{
	PR_FREEIF(m_errorMessage);
}
  
NS_IMPL_THREADSAFE_ADDREF(nsMsgMailNewsUrl);
NS_IMPL_THREADSAFE_RELEASE(nsMsgMailNewsUrl);

NS_INTERFACE_MAP_BEGIN(nsMsgMailNewsUrl)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMsgMailNewsUrl)
   NS_INTERFACE_MAP_ENTRY(nsIMsgMailNewsUrl)
   NS_INTERFACE_MAP_ENTRY(nsIURL)
   NS_INTERFACE_MAP_ENTRY(nsIURI)
NS_INTERFACE_MAP_END_THREADSAFE

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIMsgMailNewsUrl specific support
////////////////////////////////////////////////////////////////////////////////////

nsresult nsMsgMailNewsUrl::GetUrlState(PRBool * aRunningUrl)
{
	if (aRunningUrl)
		*aRunningUrl = m_runningUrl;

	return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetUrlState(PRBool aRunningUrl, nsresult aExitCode)
{
	m_runningUrl = aRunningUrl;
	nsCOMPtr <nsIMsgStatusFeedback> statusFeedback;

	if (NS_SUCCEEDED(GetStatusFeedback(getter_AddRefs(statusFeedback))) && statusFeedback)
	{
		if (m_runningUrl)
			statusFeedback->StartMeteors();
		else
		{
			statusFeedback->ShowProgress(0);
			statusFeedback->StopMeteors();
		}
	}
	if (m_urlListeners)
	{
		if (m_runningUrl)
		{
			m_urlListeners->OnStartRunningUrl(this);
		}
		else
		{
			m_urlListeners->OnStopRunningUrl(this, aExitCode);
		}
	}
  else
    printf("no listeners in set url state\n");

	return NS_OK;
}

nsresult nsMsgMailNewsUrl::RegisterListener (nsIUrlListener * aUrlListener)
{
	if (m_urlListeners)
		m_urlListeners->RegisterListener(aUrlListener);
	return NS_OK;
}

nsresult nsMsgMailNewsUrl::UnRegisterListener (nsIUrlListener * aUrlListener)
{
	if (m_urlListeners)
		m_urlListeners->UnRegisterListener(aUrlListener);
	return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetErrorMessage (const char * errorMessage)
{
	// functionality has been moved to nsIMsgStatusFeedback
	return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsMsgMailNewsUrl::GetErrorMessage (char ** errorMessage)
{
	// functionality has been moved to nsIMsgStatusFeedback
	return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetServer(nsIMsgIncomingServer ** aIncomingServer)
{
	// mscott --> we could cache a copy of the server here....but if we did, we run
	// the risk of leaking the server if any single url gets leaked....of course that
	// shouldn't happen...but it could. so i'm going to look it up every time and
	// we can look at caching it later.

	nsXPIDLCString host;
	nsXPIDLCString scheme;
	nsXPIDLCString userName;

	nsresult rv = GetHost(getter_Copies(host));
	GetUsername(getter_Copies(userName));

  if ((const char *) userName)
	nsUnescape(NS_CONST_CAST(char*,(const char*)userName));

	rv = GetScheme(getter_Copies(scheme));
    if (NS_SUCCEEDED(rv))
    {
        if (nsCRT::strcmp((const char *)scheme, "pop") == 0)
            scheme = "pop3";
        NS_WITH_SERVICE(nsIMsgAccountManager, accountManager,
                        NS_MSGACCOUNTMANAGER_PROGID, &rv);
        if (NS_FAILED(rv)) return rv;
        
        nsCOMPtr<nsIMsgIncomingServer> server;
        rv = accountManager->FindServer(userName,
                                        host,
                                        scheme,
                                        aIncomingServer);
    }

    return rv;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetStatusFeedback(nsIMsgStatusFeedback *aMsgFeedback)
{
	if (aMsgFeedback)
		m_statusFeedback = do_QueryInterface(aMsgFeedback);
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetMsgWindow(nsIMsgWindow **aMsgWindow)
{
	nsresult rv = NS_OK;
	// note: it is okay to return a null msg window and not return an error
	// it's possible the url really doesn't have msg window
	if (!m_msgWindow)
	{
//		NS_WITH_SERVICE(nsIMsgMailSession, mailSession, kMsgMailSessionCID, &rv); 

//		if(NS_SUCCEEDED(rv))
//		mailSession->GetTemporaryMsgStatusFeedback(getter_AddRefs(m_statusFeedback));
	}
	if (aMsgWindow)
	{
		*aMsgWindow = m_msgWindow;
		NS_IF_ADDREF(*aMsgWindow);
	}
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetMsgWindow(nsIMsgWindow *aMsgWindow)
{
	if (aMsgWindow)
		m_msgWindow = do_QueryInterface(aMsgWindow);
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetStatusFeedback(nsIMsgStatusFeedback **aMsgFeedback)
{
	nsresult rv = NS_OK;
	// note: it is okay to return a null status feedback and not return an error
	// it's possible the url really doesn't have status feedback
	if (!m_statusFeedback)
	{

		if(m_msgWindow)
		{
			m_msgWindow->GetStatusFeedback(getter_AddRefs(m_statusFeedback));
		}
	}
	if (aMsgFeedback)
	{
		*aMsgFeedback = m_statusFeedback;
		NS_IF_ADDREF(*aMsgFeedback);
	}
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
	nsresult rv = NS_OK;
	// note: it is okay to return a null load group and not return an error
	// it's possible the url really doesn't have load group
	if (!m_loadGroup)
	{
		if (m_msgWindow)
		{
			nsCOMPtr <nsIWebShell> webShell;
			m_msgWindow->GetRootWebShell(getter_AddRefs(webShell));
			if (webShell)
			{
				nsCOMPtr <nsIDocumentLoader> docLoader;
				webShell->GetDocumentLoader(*getter_AddRefs(docLoader));
				if (docLoader)
					docLoader->GetLoadGroup(getter_AddRefs(m_loadGroup));
			}
		}
	}

	if (aLoadGroup)
	{
		*aLoadGroup = m_loadGroup;
		NS_IF_ADDREF(*aLoadGroup);
	}
	else
		rv = NS_ERROR_NULL_POINTER;
	return rv;

}

NS_IMETHODIMP nsMsgMailNewsUrl::GetUpdatingFolder(PRBool *aResult)
{
  NS_ENSURE_ARG(aResult);
	*aResult = m_updatingFolder;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetUpdatingFolder(PRBool updatingFolder)
{
	m_updatingFolder = updatingFolder;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetAddToMemoryCache(PRBool *aAddToCache)
{
  NS_ENSURE_ARG(aAddToCache); 
	*aAddToCache = m_addContentToCache;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetAddToMemoryCache(PRBool aAddToCache)
{
	m_addContentToCache = aAddToCache;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::IsUrlType(PRUint32 type, PRBool *isType)
{
	//base class doesn't know about any specific types
	NS_ENSURE_ARG(isType);
	*isType = PR_FALSE;
	return NS_OK;

}

NS_IMETHODIMP nsMsgMailNewsUrl::SetSearchSession(nsIMsgSearchSession *aSearchSession)
{
	if (aSearchSession)
		m_searchSession = do_QueryInterface(aSearchSession);
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetSearchSession(nsIMsgSearchSession **aSearchSession)
{
  NS_ENSURE_ARG(aSearchSession);
	*aSearchSession = m_searchSession;
	NS_IF_ADDREF(*aSearchSession);
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// End nsIMsgMailNewsUrl specific support
////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIURI support
////////////////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP nsMsgMailNewsUrl::GetSpec(char * *aSpec)
{
	return m_baseURL->GetSpec(aSpec);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetSpec(const char * aSpec)
{
	return m_baseURL->SetSpec(aSpec);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetScheme(char * *aScheme)
{
	return m_baseURL->GetScheme(aScheme);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetScheme(const char * aScheme)
{
	return m_baseURL->SetScheme(aScheme);
}


NS_IMETHODIMP nsMsgMailNewsUrl::GetPreHost(char * *aPreHost)
{
	return m_baseURL->GetPreHost(aPreHost);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetPreHost(const char * aPreHost)
{
	return m_baseURL->SetPreHost(aPreHost);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetUsername(char * *aUsername)
{
	return m_baseURL->GetUsername(aUsername);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetUsername(const char * aUsername)
{
	return m_baseURL->SetUsername(aUsername);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetPassword(char * *aPassword)
{
	return m_baseURL->GetPassword(aPassword);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetPassword(const char * aPassword)
{
	return m_baseURL->SetPassword(aPassword);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetHost(char * *aHost)
{
	return m_baseURL->GetHost(aHost);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetHost(const char * aHost)
{
	return m_baseURL->SetHost(aHost);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetPort(PRInt32 *aPort)
{
	return m_baseURL->GetPort(aPort);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetPort(PRInt32 aPort)
{
	return m_baseURL->SetPort(aPort);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetPath(char * *aPath)
{
	return m_baseURL->GetPath(aPath);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetPath(const char * aPath)
{
	return m_baseURL->SetPath(aPath);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetURLParser(nsIURLParser * *aURLParser)
{
	return m_baseURL->GetURLParser(aURLParser);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetURLParser(nsIURLParser* aURLParser)
{
	return m_baseURL->SetURLParser(aURLParser);
}

NS_IMETHODIMP nsMsgMailNewsUrl::Equals(nsIURI *other, PRBool *_retval)
{
	return m_baseURL->Equals(other, _retval);
}


NS_IMETHODIMP nsMsgMailNewsUrl::Clone(nsIURI **_retval)
{
	return m_baseURL->Clone(_retval);
}	

NS_IMETHODIMP nsMsgMailNewsUrl::SetRelativePath(const char *i_RelativePath)
{
	return m_baseURL->SetRelativePath(i_RelativePath);
}

NS_IMETHODIMP nsMsgMailNewsUrl::Resolve(const char *relativePath, char **result) 
{
  // mailnews urls aren't like http or file urls...
  // we don't have relative urls you can resolve against other urls.
  // in fact, trying to do so leads to very bad things!! so instead
  // of trying to resolve the url, return about:blank as a dummy place holder
  // I tried returning just an error code but too many callers always
  // assume they get back a url =(
  *result = nsCRT::strdup("about:blank");
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetDirectory(char * *aDirectory)
{
	return m_baseURL->GetDirectory(aDirectory);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetDirectory(const char *aDirectory)
{

	return m_baseURL->SetDirectory(aDirectory);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFileName(char * *aFileName)
{
	return m_baseURL->GetFileName(aFileName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFileBaseName(char * *aFileBaseName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileBaseName(const char * aFileBaseName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFileExtension(char * *aFileExtension)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileExtension(const char * aFileExtension)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileName(const char * aFileName)
{
	return m_baseURL->SetFileName(aFileName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetParam(char * *aParam)
{
	return m_baseURL->GetParam(aParam);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetParam(const char *aParam)
{
	return m_baseURL->SetParam(aParam);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetQuery(char * *aQuery)
{
	return m_baseURL->GetQuery(aQuery);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetQuery(const char *aQuery)
{
	return m_baseURL->SetQuery(aQuery);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetRef(char * *aRef)
{
	return m_baseURL->GetRef(aRef);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetRef(const char *aRef)
{
	return m_baseURL->SetRef(aRef);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFilePath(char **o_DirFile)
{
	return m_baseURL->GetFilePath(o_DirFile);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFilePath(const char *i_DirFile)
{
	return m_baseURL->SetFilePath(i_DirFile);
}

