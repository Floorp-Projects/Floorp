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

#include "msgCore.h"    // precompiled header...

#ifdef XP_PC
#include <windows.h>    // for InterlockedIncrement
#endif

#include "nsIURL.h"
#include "nsPop3URL.h"
#include "nsPop3Protocol.h"
#include "nsINetService.h"  /* XXX: NS_FALSE */

#include "nsString.h"
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsCRT.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);

nsPop3URL::nsPop3URL(nsISupports* aContainer, nsIURLGroup* aGroup)
{
    NS_INIT_REFCNT();

	// nsIPop3URL specific code...
    m_pop3Sink = nsnull;
	m_errorMessage = nsnull;
	
	// nsINetLibUrl specific state
    m_URL_s = nsnull;
 
	// nsIURL specific state
    m_protocol = nsnull;
    m_host = nsnull;
    m_file = nsnull;
    m_ref = nsnull;
    m_port = POP3_PORT;
    m_spec = nsnull;
    m_search = nsnull;

	m_runningUrl = PR_FALSE;

	nsComponentManager::CreateInstance(kUrlListenerManagerCID, nsnull, nsIUrlListenerManager::GetIID(), 
									   (void **) &m_urlListeners);
 
    m_container = aContainer;
    NS_IF_ADDREF(m_container);
}
 
nsPop3URL::~nsPop3URL()
{
    NS_IF_RELEASE(m_container);
	NS_IF_RELEASE(m_urlListeners);
	PR_FREEIF(m_errorMessage);

    PR_FREEIF(m_spec);
    PR_FREEIF(m_protocol);
    PR_FREEIF(m_host);
    PR_FREEIF(m_file);
    PR_FREEIF(m_ref);
    PR_FREEIF(m_search);
    if (nsnull != m_URL_s) 
	{
//        NET_DropURLStruct(m_URL_s);
    }
}
  
NS_IMPL_THREADSAFE_ADDREF(nsPop3URL);
NS_IMPL_THREADSAFE_RELEASE(nsPop3URL);

nsresult nsPop3URL::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
 
    if (aIID.Equals(nsIPop3URL::GetIID()) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsIPop3URL*)this);
        AddRef();
        return NS_OK;
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
// Begin nsIPop3URL specific support
////////////////////////////////////////////////////////////////////////////////////

nsresult nsPop3URL::SetPop3Sink(nsIPop3Sink* aPop3Sink)
{
    NS_LOCK_INSTANCE();
    if (aPop3Sink)
    {
        NS_IF_RELEASE(m_pop3Sink);
        m_pop3Sink = aPop3Sink;
        NS_ADDREF(m_pop3Sink);
    }
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetPop3Sink(nsIPop3Sink** aPop3Sink) const
{
    NS_LOCK_INSTANCE();
    if (aPop3Sink)
    {
        *aPop3Sink = m_pop3Sink;
        NS_IF_ADDREF(m_pop3Sink);
    }
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::SetErrorMessage (char * errorMessage)
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
nsresult nsPop3URL::GetErrorMessage (char ** errorMessage) const
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
// End nsIPop3URL specific support
////////////////////////////////////////////////////////////////////////////////////

// url listener registration details...
	
nsresult nsPop3URL::RegisterListener (nsIUrlListener * aUrlListener)
{
	nsresult rv = NS_OK;
	if (m_urlListeners)
		rv = m_urlListeners->RegisterListener(aUrlListener);
	return rv;
}
	
nsresult nsPop3URL::UnRegisterListener (nsIUrlListener * aUrlListener)
{
	nsresult rv = NS_OK;
	if (m_urlListeners)
		rv = m_urlListeners->UnRegisterListener(aUrlListener);
	return rv;
}

nsresult nsPop3URL::GetUrlState(PRBool * aRunningUrl)
{
	if (aRunningUrl)
		*aRunningUrl = m_runningUrl;

	return NS_OK;
}

nsresult nsPop3URL::SetUrlState(PRBool aRunningUrl, nsresult aExitCode)
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

////////////////////////////////////////////////////////////////////////////////////
// Begin nsINetlibURL support
////////////////////////////////////////////////////////////////////////////////////

NS_METHOD nsPop3URL::SetURLInfo(URL_Struct *URL_s)
{
    nsresult result = NS_OK;
  
    /* Hook us up with the world. */
    m_URL_s = URL_s;
//    NET_HoldURLStruct(URL_s);
    return result;
}
  
NS_METHOD nsPop3URL::GetURLInfo(URL_Struct_** aResult) const
{
  nsresult rv;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    /* XXX: Should the URL be reference counted here?? */
    *aResult = m_URL_s;
    rv = NS_OK;
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////////
// End nsINetlibURL support
////////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////

// XXX recode to use nsString api's
// XXX don't bother with port numbers
// XXX don't bother with ref's
// XXX null pointer checks are incomplete

nsresult nsPop3URL::ParseURL(const nsString& aSpec, const nsIURL* aURL)
{
    // XXX hack!
    char* cSpec = aSpec.ToNewCString();

    const char* uProtocol = nsnull;
    const char* uHost = nsnull;
    const char* uFile = nsnull;
    PRUint32 uPort;
    if (nsnull != aURL) {
        nsresult rslt = aURL->GetProtocol(&uProtocol);
        if (rslt != NS_OK) return rslt;
        rslt = aURL->GetHost(&uHost);
        if (rslt != NS_OK) return rslt;
        rslt = aURL->GetFile(&uFile);
        if (rslt != NS_OK) return rslt;
        rslt = aURL->GetHostPort(&uPort);
        if (rslt != NS_OK) return rslt;
    }

    NS_LOCK_INSTANCE();

    PR_FREEIF(m_protocol);
    PR_FREEIF(m_host);
    PR_FREEIF(m_file);
    PR_FREEIF(m_ref);
    PR_FREEIF(m_search);
    m_port = POP3_PORT;

    if (nsnull == cSpec) {
        if (nsnull == aURL) {
            NS_UNLOCK_INSTANCE();
            return NS_ERROR_ILLEGAL_VALUE;
        }
        m_protocol = (nsnull != uProtocol) ? PL_strdup(uProtocol) : nsnull;
        m_host = (nsnull != uHost) ? PL_strdup(uHost) : nsnull;
        m_port = uPort;
        m_file = (nsnull != uFile) ? PL_strdup(uFile) : nsnull;

        NS_UNLOCK_INSTANCE();
        return NS_OK;
    }

    // Strip out reference and search info
    char* ref = strpbrk(cSpec, "#?");
    if (nsnull != ref) {
        char* search = nsnull;
        if ('#' == *ref) {
            search = PL_strchr(ref + 1, '?');
            if (nsnull != search) {
                *search++ = '\0';
            }

            PRIntn hashLen = PL_strlen(ref + 1);
            if (0 != hashLen) {
                m_ref = (char*) PR_Malloc(hashLen + 1);
                PL_strcpy(m_ref, ref + 1);
            }      
        }
        else {
            search = ref + 1;
        }

        if (nsnull != search) {
            // The rest is the search
            PRIntn searchLen = PL_strlen(search);
            if (0 != searchLen) {
                m_search = (char*) PR_Malloc(searchLen + 1);
                PL_strcpy(m_search, search);
            }      
        }

        // XXX Terminate string at start of reference or search
        *ref = '\0';
    }

    // The URL is considered absolute if and only if it begins with a
    // protocol spec. A protocol spec is an alphanumeric string of 1 or
    // more characters that is terminated with a colon.
    PRBool isAbsolute = PR_FALSE;
    char* cp;
    char* ap = cSpec;
    char ch;
    while (0 != (ch = *ap)) {
        if (((ch >= 'a') && (ch <= 'z')) ||
            ((ch >= 'A') && (ch <= 'Z')) ||
            ((ch >= '0') && (ch <= '9'))) {
            ap++;
            continue;
        }
        if ((ch == ':') && (ap - cSpec >= 2)) {
            isAbsolute = PR_TRUE;
            cp = ap;
            break;
        }
        break;
    }

    if (!isAbsolute) {
        // relative spec
        if (nsnull == aURL) {
            delete [] cSpec;

            NS_UNLOCK_INSTANCE();
            return NS_ERROR_ILLEGAL_VALUE;
        }

        // keep protocol and host
        m_protocol = (nsnull != uProtocol) ? PL_strdup(uProtocol) : nsnull;
        m_host = (nsnull != uHost) ? PL_strdup(uHost) : nsnull;
        m_port = uPort;

        // figure out file name
        PRInt32 len = PL_strlen(cSpec) + 1;
        if ((len > 1) && (cSpec[0] == '/')) {
            // Relative spec is absolute to the server
            m_file = PL_strdup(cSpec);
        } else {
            if (cSpec[0] != '\0') {
                // Strip out old tail component and put in the new one
                char* dp = PL_strrchr(uFile, '/');
                if (!dp) {
                    delete [] cSpec;
                    NS_UNLOCK_INSTANCE();
                    return NS_ERROR_ILLEGAL_VALUE;
                }
                PRInt32 dirlen = (dp + 1) - uFile;
                m_file = (char*) PR_Malloc(dirlen + len);
                PL_strncpy(m_file, uFile, dirlen);
                PL_strcpy(m_file + dirlen, cSpec);
            }
            else {
                m_file = PL_strdup(uFile);
            }
        }

        /* Stolen from netlib's mkparse.c.
         *
         * modifies a url of the form   /foo/../foo1  ->  /foo1
         *                       and    /foo/./foo1   ->  /foo/foo1
         */
        char *fwdPtr = m_file;
        char *urlPtr = m_file;
    
        for(; *fwdPtr != '\0'; fwdPtr++)
        {
    
            if(*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '/')
            {
                /* remove ./ 
                 */	
                fwdPtr += 1;
            }
            else if(*fwdPtr == '/' && *(fwdPtr+1) == '.' && *(fwdPtr+2) == '.' && 
                    (*(fwdPtr+3) == '/' || *(fwdPtr+3) == '\0'))
            {
                /* remove foo/.. 
                 */	
                /* reverse the urlPtr to the previous slash
                 */
                if(urlPtr != m_file) 
                    urlPtr--; /* we must be going back at least one */
                for(;*urlPtr != '/' && urlPtr != m_file; urlPtr--)
                    ;  /* null body */
        
                /* forward the fwd_prt past the ../
                 */
                fwdPtr += 2;
            }
            else
            {
                /* copy the url incrementaly 
                 */
                *urlPtr++ = *fwdPtr;
            }
        }
    
        *urlPtr = '\0';  /* terminate the url */

        // Now that we've resolved the relative URL, we need to reconstruct
        // a URL spec from the components.
        ReconstructSpec();
    } 
	else 
	{
        // absolute spec

        PR_FREEIF(m_spec);
        PRInt32 slen = aSpec.Length();
        m_spec = (char *) PR_Malloc(slen + 1);
        aSpec.ToCString(m_spec, slen+1);

        // get protocol first
        PRInt32 plen = cp - cSpec;
        m_protocol = (char*) PR_Malloc(plen + 1);
        PL_strncpy(m_protocol, cSpec, plen);
        m_protocol[plen] = 0;
        cp++;                               // eat : in protocol

        // skip over one, two or three slashes
        if (*cp == '/') {
            cp++;
            if (*cp == '/') {
                cp++;
                if (*cp == '/') {
                    cp++;
                }
            }
        } 
		else 
		{
            delete [] cSpec;

            NS_UNLOCK_INSTANCE();
            return NS_ERROR_ILLEGAL_VALUE;
        }


#if defined(XP_UNIX) || defined (XP_MAC)
        // Always leave the top level slash for absolute file paths under Mac and UNIX.
        // The code above sometimes results in stripping all of slashes
        // off. This only happens when a previously stripped url is asked to be
        // parsed again. Under Win32 this is not a problem since file urls begin
        // with a drive letter not a slash. This problem show's itself when 
        // nested documents such as iframes within iframes are parsed.

        if (PL_strcmp(m_protocol, "file") == 0) {
            if (*cp != '/') {
                cp--;
            }
        }
#endif /* XP_UNIX */

        const char* cp0 = cp;
        if ((PL_strcmp(m_protocol, "resource") == 0) ||
            (PL_strcmp(m_protocol, "file") == 0)) {
            // resource/file url's do not have host names.
            // The remainder of the string is the file name
            PRInt32 flen = PL_strlen(cp);
            m_file = (char*) PR_Malloc(flen + 1);
            PL_strcpy(m_file, cp);
      
#ifdef NS_WIN32
            if (PL_strcmp(m_protocol, "file") == 0) {
                // If the filename starts with a "x|" where is an single
                // character then we assume it's a drive name and change the
                // vertical bar back to a ":"
                if ((flen >= 2) && (m_file[1] == '|')) {
                    m_file[1] = ':';
                }
            }
#endif /* NS_WIN32 */
        } else {
            // Host name follows protocol for http style urls
            cp = PL_strpbrk(cp, "/:");
      
            if (nsnull == cp) {
                // There is only a host name
                PRInt32 hlen = PL_strlen(cp0);
                m_host = (char*) PR_Malloc(hlen + 1);
                PL_strcpy(m_host, cp0);
            }
            else {
                PRInt32 hlen = cp - cp0;
                m_host = (char*) PR_Malloc(hlen + 1);
                PL_strncpy(m_host, cp0, hlen);        
                m_host[hlen] = 0;

                if (':' == *cp) {
                    // We have a port number
                    cp0 = cp+1;
                    cp = PL_strchr(cp, '/');
                    m_port = strtol(cp0, (char **)nsnull, 10);
                }
            }

            if (nsnull == cp) {
                // There is no file name
                // Set filename to "/"
                m_file = (char*) PR_Malloc(2);
                m_file[0] = '/';
                m_file[1] = 0;
            }
            else {
                // The rest is the file name
                PRInt32 flen = PL_strlen(cp);
                m_file = (char*) PR_Malloc(flen + 1);
                PL_strcpy(m_file, cp);
            }
        }
    }

//printf("protocol='%s' host='%s' file='%s'\n", m_protocol, m_host, m_file);
    delete [] cSpec;

    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

void nsPop3URL::ReconstructSpec(void)
{
    PR_FREEIF(m_spec);

    char portBuffer[10];
    if (-1 != m_port) {
        PR_snprintf(portBuffer, 10, ":%d", m_port);
    }
    else {
        portBuffer[0] = '\0';
    }

    PRInt32 plen = PL_strlen(m_protocol) + PL_strlen(m_host) +
        PL_strlen(portBuffer) + PL_strlen(m_file) + 4;
    if (m_ref) {
        plen += 1 + PL_strlen(m_ref);
    }
    if (m_search) {
        plen += 1 + PL_strlen(m_search);
    }

    m_spec = (char *) PR_Malloc(plen + 1);
    PR_snprintf(m_spec, plen, "%s://%s%s%s", 
                m_protocol, ((nsnull != m_host) ? m_host : ""), portBuffer,
                m_file);

    if (m_ref) {
        PL_strcat(m_spec, "#");
        PL_strcat(m_spec, m_ref);
    }
    if (m_search) {
        PL_strcat(m_spec, "?");
        PL_strcat(m_spec, m_search);
    }
}

////////////////////////////////////////////////////////////////////////////////

PRBool nsPop3URL::Equals(const nsIURL* aURL) const 
{
    PRBool bIsEqual;
    nsPop3URL* other;
    NS_LOCK_INSTANCE();
	// are they both Pop3 urls?? if yes...for now just compare the pointers until 
	// I figure out if we need to check any of the guts for equality....
    if (((nsIURL*)aURL)->QueryInterface(nsIPop3URL::GetIID(), (void**)&other) == NS_OK) {
        bIsEqual = other == this; // compare the pointers...
    }
    else
        bIsEqual = PR_FALSE;
    NS_UNLOCK_INSTANCE();
    return bIsEqual;
}

nsresult nsPop3URL::GetProtocol(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_protocol;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::SetProtocol(const char *aNewProtocol)
{
    NS_LOCK_INSTANCE();
    m_protocol = nsCRT::strdup(aNewProtocol);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetHost(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_host;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::SetHost(const char *aNewHost)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_host = nsCRT::strdup(aNewHost);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetFile(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_file;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::SetFile(const char *aNewFile)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_file = nsCRT::strdup(aNewFile);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetSpec(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_spec;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::SetSpec(const char *aNewSpec)
{
    // XXX is this right, or should we call ParseURL?
    nsresult rv = NS_OK;
//    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    rv = ParseURL(aNewSpec);
#if 0
    PR_FREEIF(m_spec);
    m_spec = nsCRT::strdup(aNewSpec);
#endif
    NS_UNLOCK_INSTANCE();
    return rv;
}

nsresult nsPop3URL::GetRef(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_ref;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::SetRef(const char *aNewRef)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_ref = nsCRT::strdup(aNewRef);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetHostPort(PRUint32 *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_port;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::SetHostPort(PRUint32 aNewPort)
{
    // NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_port = aNewPort;
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetSearch(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_search;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::SetSearch(const char *aNewSearch)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_search = nsCRT::strdup(aNewSearch);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetContainer(nsISupports* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_container;
    NS_IF_ADDREF(m_container);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}
  
nsresult nsPop3URL::SetContainer(nsISupports* container)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    NS_IF_RELEASE(m_container);
    m_container = container;
    NS_IF_ADDREF(m_container);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::GetContentLength(PRInt32 *len)
{
    NS_LOCK_INSTANCE();
    *len = m_URL_s->content_length;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// End of nsIURL support
////////////////////////////////////////////////////////////////////////////////////
 
////////////////////////////////////////////////////////////////////////////////////
// The following set of functions should become obsolete once we take them out of
// nsIURL.....
////////////////////////////////////////////////////////////////////////////////////
nsresult nsPop3URL::GetLoadAttribs(nsILoadAttribs* *result) const
{
    NS_LOCK_INSTANCE();
    *result = NULL;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}
  
nsresult nsPop3URL::SetLoadAttribs(nsILoadAttribs* aLoadAttribs)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    return NS_OK;
}

nsresult nsPop3URL::SetPostHeader(const char* name, const char* value)
{
    NS_LOCK_INSTANCE();
    // XXX
    PR_ASSERT(0);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::SetPostData(nsIInputStream* input)
{
    return NS_OK;
}

nsresult nsPop3URL::GetURLGroup(nsIURLGroup* *result) const
{
    return NS_OK;
}
  
nsresult nsPop3URL::SetURLGroup(nsIURLGroup* group)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    return NS_OK;
}

nsresult nsPop3URL::GetServerStatus(PRInt32 *status)
{
    NS_LOCK_INSTANCE();
    *status = m_URL_s->server_status;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsPop3URL::ToString(PRUnichar* *aString) const
{ 
	if (aString)
		*aString = nsnull; 
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// End of functions which should be made obsolete after modifying nsIURL
////////////////////////////////////////////////////////////////////////////////////
