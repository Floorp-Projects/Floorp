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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "msgCore.h"
#include "nsMsgMailNewsUrl.h"
#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);

nsMsgMailNewsUrl::nsMsgMailNewsUrl()
{
    NS_INIT_REFCNT();
 
	// nsIURI specific state
	m_errorMessage = nsnull;
	m_runningUrl = PR_FALSE;

	nsComponentManager::CreateInstance(kUrlListenerManagerCID, nsnull, nsCOMTypeInfo<nsIUrlListenerManager>::GetIID(), (void **) getter_AddRefs(m_urlListeners));
	nsComponentManager::CreateInstance(kStandardUrlCID, nsnull, nsCOMTypeInfo<nsIURL>::GetIID(), (void **) getter_AddRefs(m_baseURL));
}
 
nsMsgMailNewsUrl::~nsMsgMailNewsUrl()
{
	PR_FREEIF(m_errorMessage);
}
  
NS_IMPL_ADDREF(nsMsgMailNewsUrl);
NS_IMPL_RELEASE(nsMsgMailNewsUrl);

nsresult nsMsgMailNewsUrl::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    if (aIID.Equals(nsCOMTypeInfo<nsIURI>::GetIID())) {
        *aInstancePtr = (void*) ((nsIURI*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
	if (aIID.Equals(nsCOMTypeInfo<nsIURL>::GetIID())) {
        *aInstancePtr = (void*) ((nsIURL*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
	if (aIID.Equals(nsCOMTypeInfo<nsIMsgMailNewsUrl>::GetIID()))
	{
		*aInstancePtr = (void *) ((nsIMsgMailNewsUrl*) this);
		NS_ADDREF_THIS();
		return NS_OK;
	}
 
	if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))    
	{
		*aInstancePtr = (void *) ((nsIMsgMailNewsUrl*) this);
		NS_ADDREF_THIS();
		return NS_OK;
	}
#if defined(NS_DEBUG)
    /*
     * Check for the debug-only interface indicating thread-safety
     */
    static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);
    if (aIID.Equals(kIsThreadsafeIID)) {
        return NS_OK;
    }
#endif
    return NS_NOINTERFACE;
}

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
	if (m_urlListeners)
	{
		if (m_runningUrl)
			m_urlListeners->OnStartRunningUrl(this);
		else
			m_urlListeners->OnStopRunningUrl(this, aExitCode);
	}

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

nsresult nsMsgMailNewsUrl::SetErrorMessage (char * errorMessage)
{
	if (errorMessage)
	{
		PR_FREEIF(m_errorMessage);
		m_errorMessage = errorMessage;
	}
	return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetErrorMessage (char ** errorMessage) const
{
	if (errorMessage)
	{
		if (m_errorMessage)
			*errorMessage = nsCRT::strdup(m_errorMessage);
		else
			*errorMessage = nsnull;
	}
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

NS_IMETHODIMP nsMsgMailNewsUrl::SetSpec(char * aSpec)
{
	return m_baseURL->SetSpec(aSpec);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetScheme(char * *aScheme)
{
	return m_baseURL->GetScheme(aScheme);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetScheme(char * aScheme)
{
	return m_baseURL->SetScheme(aScheme);
}


NS_IMETHODIMP nsMsgMailNewsUrl::GetPreHost(char * *aPreHost)
{
	return m_baseURL->GetPreHost(aPreHost);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetPreHost(char * aPreHost)
{
	return m_baseURL->SetPreHost(aPreHost);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetHost(char * *aHost)
{
	return m_baseURL->GetHost(aHost);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetHost(char * aHost)
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

NS_IMETHODIMP nsMsgMailNewsUrl::SetPath(char * aPath)
{
	return m_baseURL->SetPath(aPath);
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

NS_IMETHODIMP nsMsgMailNewsUrl::GetDirectory(char * *aDirectory)
{
	return m_baseURL->GetDirectory(aDirectory);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetDirectory(char *aDirectory)
{

	return m_baseURL->SetDirectory(aDirectory);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetFileName(char * *aFileName)
{
	return m_baseURL->GetFileName(aFileName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetFileName(char * aFileName)
{
	return m_baseURL->SetFileName(aFileName);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetQuery(char * *aQuery)
{
	return m_baseURL->GetQuery(aQuery);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetQuery(char *aQuery)
{
	return m_baseURL->SetQuery(aQuery);
}

NS_IMETHODIMP nsMsgMailNewsUrl::GetRef(char * *aRef)
{
	return m_baseURL->GetRef(aRef);
}

NS_IMETHODIMP nsMsgMailNewsUrl::SetRef(char *aRef)
{
	return m_baseURL->SetRef(aRef);
}

NS_IMETHODIMP nsMsgMailNewsUrl::DirFile(char **o_DirFile)
{
	return m_baseURL->DirFile(o_DirFile);
}
