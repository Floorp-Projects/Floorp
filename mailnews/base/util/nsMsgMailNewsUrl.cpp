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

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);

nsMsgMailNewsUrl::nsMsgMailNewsUrl()
{
    NS_INIT_REFCNT();

	// nsINetLibUrl specific state
    m_URL_s = nsnull;
 
	// nsIURL specific state
    m_protocol = nsnull;
    m_host = nsnull;
    m_file = nsnull;
    m_ref = nsnull;
    m_port = 0;
    m_spec = nsnull;
    m_search = nsnull;
	m_errorMessage = nsnull;
	m_runningUrl = PR_FALSE;
	nsComponentManager::CreateInstance(kUrlListenerManagerCID, nsnull, nsIUrlListenerManager::GetIID(), (void **) getter_AddRefs(m_urlListeners));
 
    m_container = nsnull;
}
 
nsMsgMailNewsUrl::~nsMsgMailNewsUrl()
{
    NS_IF_RELEASE(m_container);
	PR_FREEIF(m_errorMessage);

    PR_FREEIF(m_spec);
    PR_FREEIF(m_protocol);
    PR_FREEIF(m_host);
    PR_FREEIF(m_file);
    PR_FREEIF(m_ref);
    PR_FREEIF(m_search);
}
  
NS_IMPL_THREADSAFE_ADDREF(nsMsgMailNewsUrl);
NS_IMPL_THREADSAFE_RELEASE(nsMsgMailNewsUrl);

nsresult nsMsgMailNewsUrl::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    if (aIID.Equals(nsIURL::GetIID())) {
        *aInstancePtr = (void*) ((nsIURL*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(nsINetlibURL::GetIID())) {
        *aInstancePtr = (void*) ((nsINetlibURL*)this);
        AddRef();
        return NS_OK;
    }
	if (aIID.Equals(nsIMsgMailNewsUrl::GetIID()))
	{
		*aInstancePtr = (void *) ((nsIMsgMailNewsUrl*) this);
		AddRef();
		return NS_OK;
	}
 
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
	NS_LOCK_INSTANCE();
	if (errorMessage)
	{
		PR_FREEIF(m_errorMessage);
		m_errorMessage = errorMessage;
	}
	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

// caller must free using PR_FREE
nsresult nsMsgMailNewsUrl::GetErrorMessage (char ** errorMessage) const
{
	NS_LOCK_INSTANCE();
	if (errorMessage)
	{
		if (m_errorMessage)
			*errorMessage = nsCRT::strdup(m_errorMessage);
		else
			*errorMessage = nsnull;
	}
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// End nsIMsgMailNewsUrl specific support
////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
// Begin nsINetlibURL support
////////////////////////////////////////////////////////////////////////////////////

NS_METHOD nsMsgMailNewsUrl::SetURLInfo(URL_Struct *URL_s)
{
    nsresult result = NS_OK;
  
    /* Hook us up with the world. */
    m_URL_s = URL_s;
    return result;
}
  
NS_METHOD nsMsgMailNewsUrl::GetURLInfo(URL_Struct_** aResult) const
{
  nsresult rv;

  if (nsnull == aResult)
    rv = NS_ERROR_NULL_POINTER;
  else 
  {
    *aResult = m_URL_s;
    rv = NS_OK;
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////////
// End nsINetlibURL support
////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIURL support
////////////////////////////////////////////////////////////////////////////////////

nsresult nsMsgMailNewsUrl::GetProtocol(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_protocol;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetProtocol(const char *aNewProtocol)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_protocol = nsCRT::strdup(aNewProtocol);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetHost(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_host;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetHost(const char *aNewHost)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_host = nsCRT::strdup(aNewHost);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetFile(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_file;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetFile(const char *aNewFile)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_file = nsCRT::strdup(aNewFile);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetSpec(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_spec;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetSpec(const char *aNewSpec)
{
    // XXX is this right, or should we call ParseURL?
    nsresult rv = NS_OK;
//    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    rv = ParseUrl(aNewSpec);
#if 0
    PR_FREEIF(m_spec);
    m_spec = nsCRT::strdup(aNewSpec);
#endif
    NS_UNLOCK_INSTANCE();
    return rv;
}

nsresult nsMsgMailNewsUrl::GetRef(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_ref;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetRef(const char *aNewRef)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_ref = nsCRT::strdup(aNewRef);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetHostPort(PRUint32 *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_port;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetHostPort(PRUint32 aNewPort)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_port = aNewPort;
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetSearch(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_search;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetSearch(const char *aNewSearch)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_search = nsCRT::strdup(aNewSearch);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetContainer(nsISupports* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_container;
    NS_IF_ADDREF(m_container);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}
  
nsresult nsMsgMailNewsUrl::SetContainer(nsISupports* container)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    NS_IF_RELEASE(m_container);
    m_container = container;
    NS_IF_ADDREF(m_container);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetContentLength(PRInt32 *len)
{
    NS_LOCK_INSTANCE();
    *len = m_URL_s->content_length;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetPostHeader(const char* name, const char* value)
{
    NS_LOCK_INSTANCE();
    // XXX
    PR_ASSERT(0);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::SetPostData(nsIInputStream* input)
{
    return NS_OK;
}


PRBool nsMsgMailNewsUrl::Equals(const nsIURL* aURL) const 
{
    PRBool bIsEqual;
    nsIMsgMailNewsUrl * other;
    NS_LOCK_INSTANCE();
	// for now just compare the pointers until 
	// I figure out if we need to check any of the guts for equality....
    if (((nsIURL*)aURL)->QueryInterface(nsIMsgMailNewsUrl::GetIID(), (void**)&other) == NS_OK) {
        bIsEqual = other == (nsIMsgMailNewsUrl *) this; // compare the pointers...
    }
    else
        bIsEqual = PR_FALSE;
    NS_UNLOCK_INSTANCE();
    return bIsEqual;
}

////////////////////////////////////////////////////////////////////////////////////
// End of nsIURL support
////////////////////////////////////////////////////////////////////////////////////
 
////////////////////////////////////////////////////////////////////////////////////
// The following set of functions should become obsolete once we take them out of
// nsIURL.....
////////////////////////////////////////////////////////////////////////////////////
nsresult nsMsgMailNewsUrl::GetLoadAttribs(nsILoadAttribs* *result) const
{
    NS_LOCK_INSTANCE();
    *result = NULL;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}
  
nsresult nsMsgMailNewsUrl::SetLoadAttribs(nsILoadAttribs* aLoadAttribs)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetURLGroup(nsIURLGroup* *result) const
{
    return NS_OK;
}
  
nsresult nsMsgMailNewsUrl::SetURLGroup(nsIURLGroup* group)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::GetServerStatus(PRInt32 *status)
{
    NS_LOCK_INSTANCE();
    *status = m_URL_s->server_status;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsMsgMailNewsUrl::ToString(PRUnichar* *aString) const
{ 
	if (aString)
		*aString = nsnull; 
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// End of functions which should be made obsolete after modifying nsIURL
////////////////////////////////////////////////////////////////////////////////////

