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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "msgCore.h"
#include "nsMsgMailNewsUrl.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgMailSession.h"
#include "nsIMsgAccountManager.h"
#include "nsXPIDLString.h"
#include "nsIDocumentLoader.h"
#include "nsILoadGroup.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIOService.h"
#include "nsNetCID.h"

static NS_DEFINE_CID(kUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

nsMsgMailNewsUrl::nsMsgMailNewsUrl()
{
    NS_INIT_REFCNT();
 
	// nsIURI specific state
	m_errorMessage = nsnull;
	m_runningUrl = PR_FALSE;
	m_updatingFolder = PR_FALSE;
  m_addContentToCache = PR_FALSE;
  m_msgIsInLocalCache = PR_FALSE;
  m_suppressErrorMsgs = PR_FALSE;

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
  if (m_runningUrl == aRunningUrl) // we already knew this.
    return NS_OK;
	m_runningUrl = aRunningUrl;
	nsCOMPtr <nsIMsgStatusFeedback> statusFeedback;

  // put this back - we need it for urls that don't run through the doc loader
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
      m_loadGroup = nsnull; // try to break circular refs.
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

	/* GetUsername() returns an unescaped string.
	 * do not unescape it again.
	 */
	GetUsername(getter_Copies(userName));

	rv = GetScheme(getter_Copies(scheme));
    if (NS_SUCCEEDED(rv))
    {
        if (nsCRT::strcmp((const char *)scheme, "pop") == 0)
            scheme.Adopt(nsCRT::strdup("pop3"));
        nsCOMPtr<nsIMsgAccountManager> accountManager = 
                 do_GetService(NS_MSGACCOUNTMANAGER_CONTRACTID, &rv);
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
//		nsCOMPtr<nsIMsgMailSession> mailSession = 
//		         do_GetService(kMsgMailSessionCID, &rv); 

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
            nsCOMPtr<nsIDocShell> docShell;
            m_msgWindow->GetRootDocShell(getter_AddRefs(docShell));
            nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));

#if 0   // since we're not going through the doc loader for most mail/news urls,
       //, this code isn't useful
        // but I can imagine it could be useful at some point.

            // load group needs status feedback set, since it's
            // not the main window load group.
            nsCOMPtr<nsIMsgStatusFeedback> statusFeedback;
            m_msgWindow->GetStatusFeedback(getter_AddRefs(statusFeedback));

            if (statusFeedback)
            {
              nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(docShell));
              nsCOMPtr<nsIWebProgressListener> webProgressListener(do_QueryInterface(statusFeedback));

              // register our status feedback object
              if (statusFeedback && docShell)
              {
                webProgressListener = do_QueryInterface(statusFeedback);
                webProgress->AddProgressListener(webProgressListener);
              }
            }
#endif
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

NS_IMETHODIMP nsMsgMailNewsUrl::GetMsgIsInLocalCache(PRBool *aMsgIsInLocalCache)
{
  NS_ENSURE_ARG(aMsgIsInLocalCache); 
	*aMsgIsInLocalCache = m_msgIsInLocalCache;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetMsgIsInLocalCache(PRBool aMsgIsInLocalCache)
{
	m_msgIsInLocalCache = aMsgIsInLocalCache;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetSuppressErrorMsgs(PRBool *aSuppressErrorMsgs)
{
  NS_ENSURE_ARG(aSuppressErrorMsgs); 
	*aSuppressErrorMsgs = m_suppressErrorMsgs;
	return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetSuppressErrorMsgs(PRBool aSuppressErrorMsgs)
{
	m_suppressErrorMsgs = aSuppressErrorMsgs;
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

#define FILENAME_PART "&filename="
#define FILENAME_PART_LEN 10

NS_IMETHODIMP nsMsgMailNewsUrl::SetSpec(const char * aSpec)
{
  // Parse out "filename" attribute if present.
  char *start, *end;
  start = PL_strcasestr(aSpec,FILENAME_PART);
  if (start)
  {	// Make sure we only get our own value.
    end = PL_strcasestr((char*)(start+FILENAME_PART_LEN),"&");
    if (end)
    {
      *end = 0;
      mAttachmentFileName = start+FILENAME_PART_LEN;
      *end = '&';
    }
    else
      mAttachmentFileName = start+FILENAME_PART_LEN;
  }
  // Now, set the rest.
  return m_baseURL->SetSpec(aSpec);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetPrePath(char * *aPrePath)
{
	return m_baseURL->GetPrePath(aPrePath);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetPrePath(const char * aPrePath)
{
	return m_baseURL->SetPrePath(aPrePath);
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
	/* note:  this will return an unescaped string */
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

NS_IMETHODIMP nsMsgMailNewsUrl::Equals(nsIURI *other, PRBool *_retval)
{
	return m_baseURL->Equals(other, _retval);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SchemeIs(const char *aScheme, PRBool *_retval)
{
  nsXPIDLCString scheme;
  nsresult rv = m_baseURL->GetScheme(getter_Copies(scheme));
  NS_ENSURE_SUCCESS(rv,rv);

  // fix #76200 crash on email with <img> with no src.
  //
  // make sure we have a scheme before calling SchemeIs()
  // we have to do this because url parsing can result in a null mScheme
  // this extra string copy should be removed when #73845 is fixed.
  if (scheme.get()) {
    return m_baseURL->SchemeIs(aScheme, _retval);
  }
  else {
    *_retval = PR_FALSE;
    return NS_OK;
  }
}

NS_IMETHODIMP nsMsgMailNewsUrl::Clone(nsIURI **_retval)
{
  nsresult rv;
  nsXPIDLCString urlSpec;
  nsCOMPtr<nsIIOService> ioService = do_GetService(kIOServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;
  rv = GetSpec(getter_Copies(urlSpec));
  if (NS_FAILED(rv)) return rv;
  return ioService->NewURI(urlSpec, nsnull, _retval);
}	

NS_IMETHODIMP nsMsgMailNewsUrl::Resolve(const char *relativePath, char **result) 
{
  // only resolve anchor urls....i.e. urls which start with '#' against the mailnews url...
  // everything else shouldn't be resolved against mailnews urls.
  nsresult rv = NS_OK;

  if (relativePath && relativePath[0] == '#') // an anchor
    return m_baseURL->Resolve(relativePath, result);
  else
  {
    // if relativePath is a complete url with it's own scheme then allow it...
    nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    PRUint32 startPos;
    PRUint32 endPos;
    nsXPIDLCString scheme;

    rv = ioService->ExtractScheme(relativePath, &startPos, &endPos, getter_Copies(scheme));
    // if we have a fully qualified scheme then pass the relative path back as the result
    if (NS_SUCCEEDED(rv) && scheme.get())
    {
      *result = nsCRT::strdup(relativePath);
    }
    else
    {
      *result = nsnull;
      rv = NS_ERROR_FAILURE;
    }
  }

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
  if (!mAttachmentFileName.IsEmpty())
  {
    *aFileName = mAttachmentFileName.ToNewCString();
    return NS_OK;
  }
  else
	  return m_baseURL->GetFileName(aFileName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFileBaseName(char * *aFileBaseName)
{
	return m_baseURL->GetFileBaseName(aFileBaseName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileBaseName(const char * aFileBaseName)
{
	return m_baseURL->SetFileBaseName(aFileBaseName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFileExtension(char * *aFileExtension)
{
  if (!mAttachmentFileName.IsEmpty())
  {
    nsCAutoString extension;
    PRInt32 pos = mAttachmentFileName.RFindCharInSet(".");
    if (pos > 0)
      mAttachmentFileName.Right(extension,
                                mAttachmentFileName.Length() -
                                (pos + 1) /* skip the '.' */);
    *aFileExtension = extension.ToNewCString();
    return NS_OK;
  }
  else
	  return m_baseURL->GetFileExtension(aFileExtension);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileExtension(const char * aFileExtension)
{
	return m_baseURL->SetFileExtension(aFileExtension);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileName(const char * aFileName)
{
  mAttachmentFileName = aFileName;
  return NS_OK;
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

NS_IMETHODIMP nsMsgMailNewsUrl::GetEscapedQuery(char * *aQuery)
{
	return m_baseURL->GetEscapedQuery(aQuery);
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

NS_IMETHODIMP nsMsgMailNewsUrl::SetMemCacheEntry(nsICacheEntryDescriptor *memCacheEntry)
{
  m_memCacheEntry = memCacheEntry;
  return NS_OK;
}

NS_IMETHODIMP nsMsgMailNewsUrl:: GetMemCacheEntry(nsICacheEntryDescriptor **memCacheEntry)
{
  NS_ENSURE_ARG(memCacheEntry);
  nsresult rv = NS_OK;

  if (m_memCacheEntry)
  {
    *memCacheEntry = m_memCacheEntry;
    NS_ADDREF(*memCacheEntry);
  }
  else
  {
    *memCacheEntry = nsnull;
    return NS_ERROR_NULL_POINTER;
  }

  return rv;
}
