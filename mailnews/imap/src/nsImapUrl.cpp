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
#include "nsImapUrl.h"

#include "nsINetService.h"
#include "nsIMsgMailSession.h"
#include "nsIIMAPHostSessionList.h"
#include "nsIMAPGenericParser.h"
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
static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);
static NS_DEFINE_CID(kCImapHostSessionListCID, NS_IIMAPHOSTSESSIONLIST_CID);

nsImapUrl::nsImapUrl()
{
    NS_INIT_REFCNT();

	m_errorMessage = nsnull;
	m_server = nsnull;
	
	// nsINetLibUrl specific state
    m_URL_s = nsnull;
 
	// nsIURL specific state
    m_protocol = nsnull;
    m_host = nsnull;
    m_port = IMAP_PORT;
    m_spec = nsnull;
    m_search = nsnull;
	
	m_imapLog = nsnull;
    m_imapMailfolder = nsnull;
    m_imapMessage = nsnull;
    m_imapExtension = nsnull;
    m_imapMiscellaneous = nsnull;
	m_listOfMessageIds = nsnull;
	m_sourceCanonicalFolderPathSubString = nsnull;
	m_destinationCanonicalFolderPathSubString = nsnull;
	m_listOfMessageIds = nsnull;
    m_tokenPlaceHolder = nsnull;

	m_runningUrl = PR_FALSE;
	m_idsAreUids = PR_FALSE;
	m_mimePartSelectorDetected = PR_FALSE;
	m_validUrl = PR_TRUE;	// assume the best.
	m_flags = 0;
	nsComponentManager::CreateInstance(kUrlListenerManagerCID, nsnull, nsIUrlListenerManager::GetIID(), 
									   (void **) &m_urlListeners);
}
 
nsImapUrl::~nsImapUrl()
{
	NS_IF_RELEASE(m_imapLog);
    NS_IF_RELEASE(m_imapMailfolder);
    NS_IF_RELEASE(m_imapMessage);
    NS_IF_RELEASE(m_imapExtension);
    NS_IF_RELEASE(m_imapMiscellaneous);
	NS_IF_RELEASE(m_server);

	NS_IF_RELEASE(m_urlListeners);
	PR_FREEIF(m_errorMessage);

    PR_FREEIF(m_spec);
    PR_FREEIF(m_protocol);
    PR_FREEIF(m_host);
    PR_FREEIF(m_search);
	PR_FREEIF(m_listOfMessageIds);

}
  
NS_IMPL_THREADSAFE_ADDREF(nsImapUrl);
NS_IMPL_THREADSAFE_RELEASE(nsImapUrl);

NS_IMETHODIMP nsImapUrl::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
 
    if (aIID.Equals(nsIImapUrl::GetIID()) || aIID.Equals(kISupportsIID)) 
	{
        *aInstancePtr = (void*) ((nsIImapUrl*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(nsIURL::GetIID())) 
	{
        *aInstancePtr = (void*) ((nsIURL*)this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(nsINetlibURL::GetIID())) 
	{
        *aInstancePtr = (void*) ((nsINetlibURL*)this);
        AddRef();
        return NS_OK;
    }
	if (aIID.Equals(nsIMsgMailNewsUrl::GetIID()))
	{
		*aInstancePtr = (void*) ((nsIMsgMailNewsUrl*) this);
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
// Begin nsIImapUrl specific support
////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsImapUrl::SetServer(nsIMsgIncomingServer * aServer)
{
	if (aServer)
	{
		NS_IF_RELEASE(m_server);
		m_server = aServer;
		NS_ADDREF(m_server);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsImapUrl::GetServer(nsIMsgIncomingServer **aServer)
{
	nsresult rv = NS_OK;

	if (aServer) // valid argument to return result in?
	{
		// if we weren't given an server, let's be creative and go fetch the default current
		// server. 
		if (!m_server)
		{
			nsIMsgMailSession * session = nsnull;
			rv = nsServiceManager::GetService(kMsgMailSessionCID, nsIMsgMailSession::GetIID(),
										 (nsISupports **) &session);
			if (NS_SUCCEEDED(rv) && session)
			{
				// store the server in m_server so we don't have to do this again.
				rv = session->GetCurrentServer(&m_server);
				nsServiceManager::ReleaseService(kMsgMailSessionCID, session);
			}
		}

		// if we were given a server then use it. 
		if (m_server)
		{
			*aServer = m_server;
			NS_ADDREF(m_server);
		}
		else
			*aServer = nsnull;
	} // if aMsgIdentity

	return rv;
}

NS_IMETHODIMP nsImapUrl::GetImapLog(nsIImapLog ** aImapLog)
{
	if (aImapLog)
	{
		*aImapLog = m_imapLog;
		NS_IF_ADDREF(m_imapLog);
	}

	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetImapLog(nsIImapLog  * aImapLog)
{
	if (aImapLog)
	{
		NS_IF_RELEASE(m_imapLog);
		m_imapLog = aImapLog;
		NS_ADDREF(m_imapLog);
	}

	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetImapMailfolder(nsIImapMailfolder **
                                           aImapMailfolder)
{
	if (aImapMailfolder)
	{
		*aImapMailfolder = m_imapMailfolder;
		NS_IF_ADDREF(m_imapMailfolder);
	}

	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetImapMailfolder(nsIImapMailfolder  * aImapMailfolder)
{
	if (aImapMailfolder)
	{
		NS_IF_RELEASE(m_imapMailfolder);
		m_imapMailfolder = aImapMailfolder;
		NS_ADDREF(m_imapMailfolder);
	}

	return NS_OK;
}
 
NS_IMETHODIMP nsImapUrl::GetImapMessage(nsIImapMessage ** aImapMessage)
{
	if (aImapMessage)
	{
		*aImapMessage = m_imapMessage;
		NS_IF_ADDREF(m_imapMessage);
	}

	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetImapMessage(nsIImapMessage  * aImapMessage)
{
	if (aImapMessage)
	{
		NS_IF_RELEASE(m_imapMessage);
		m_imapMessage = aImapMessage;
		NS_ADDREF(m_imapMessage);
	}

	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetImapExtension(nsIImapExtension ** aImapExtension)
{
	if (aImapExtension)
	{
		*aImapExtension = m_imapExtension;
		NS_IF_ADDREF(m_imapExtension);
	}

	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetImapExtension(nsIImapExtension  * aImapExtension)
{
	if (aImapExtension)
	{
		NS_IF_RELEASE(m_imapExtension);
		m_imapExtension = aImapExtension;
		NS_ADDREF(m_imapExtension);
	}

	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetImapMiscellaneous(nsIImapMiscellaneous **
                                              aImapMiscellaneous)
{
	if (aImapMiscellaneous)
	{
		*aImapMiscellaneous = m_imapMiscellaneous;
		NS_IF_ADDREF(m_imapMiscellaneous);
	}

	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetImapMiscellaneous(nsIImapMiscellaneous  *
                                              aImapMiscellaneous)
{
	if (aImapMiscellaneous)
	{
		NS_IF_RELEASE(m_imapMiscellaneous);
		m_imapMiscellaneous = aImapMiscellaneous;
		NS_ADDREF(m_imapMiscellaneous);
	}

	return NS_OK;
}

        
////////////////////////////////////////////////////////////////////////////////////
// End nsIImapUrl specific support
////////////////////////////////////////////////////////////////////////////////////

// url listener registration details...
	
NS_IMETHODIMP nsImapUrl::RegisterListener (nsIUrlListener * aUrlListener)
{
	NS_LOCK_INSTANCE();
	nsresult rv = NS_OK;
	if (m_urlListeners)
		rv = m_urlListeners->RegisterListener(aUrlListener);
    NS_UNLOCK_INSTANCE();
	return rv;
}
	
NS_IMETHODIMP nsImapUrl::UnRegisterListener (nsIUrlListener * aUrlListener)
{
	NS_LOCK_INSTANCE();
	nsresult rv = NS_OK;
	if (m_urlListeners)
		rv = m_urlListeners->UnRegisterListener(aUrlListener);
	NS_UNLOCK_INSTANCE();
	return rv;
}

NS_IMETHODIMP nsImapUrl::GetUrlState(PRBool * aRunningUrl)
{
	NS_LOCK_INSTANCE();
	if (aRunningUrl)
		*aRunningUrl = m_runningUrl;

	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetUrlState(PRBool aRunningUrl, nsresult aExitCode)
{
	NS_LOCK_INSTANCE();
	m_runningUrl = aRunningUrl;
	if (m_urlListeners)
	{
		if (m_runningUrl)
			m_urlListeners->OnStartRunningUrl(this);
		else
			m_urlListeners->OnStopRunningUrl(this, aExitCode);
	}
	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetErrorMessage (char * errorMessage)
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
NS_IMETHODIMP nsImapUrl::GetErrorMessage (char ** errorMessage) const
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
// Begin nsINetlibURL support
////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsImapUrl::SetURLInfo(URL_Struct *URL_s)
{
    nsresult result = NS_OK;
    /* Hook us up with the world. */
    m_URL_s = URL_s;
    return result;
}
  
NS_IMETHODIMP nsImapUrl::GetURLInfo(URL_Struct_** aResult) const
{
  nsresult rv;

  if (nsnull == aResult) 
    rv = NS_ERROR_NULL_POINTER;
  else 
  {
    /* XXX: Should the URL be reference counted here?? */
    *aResult = m_URL_s;
    rv = NS_OK;
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////////
// End nsINetlibURL support
////////////////////////////////////////////////////////////////////////////////////

nsresult nsImapUrl::ParseURL(const nsString& aSpec, const nsIURL* aURL)
{
    // XXX hack!
    char* cSpec = aSpec.ToNewCString();

    const char* uProtocol = nsnull;
    const char* uHost = nsnull;
    const char* uFile = nsnull;
    PRUint32 uPort;
    if (nsnull != aURL) 
	{
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
    PR_FREEIF(m_search);
    m_port = IMAP_PORT;

	// mscott -> eventually we'll replace all of this duplicate host and port parsing code with a url parser
	// class..this should come with N2 Landing...

    if (nsnull == cSpec) 
	{
        if (nsnull == aURL) 
		{
            NS_UNLOCK_INSTANCE();
            return NS_ERROR_ILLEGAL_VALUE;
        }
        
		m_protocol = (nsnull != uProtocol) ? PL_strdup(uProtocol) : nsnull;
        m_host = (nsnull != uHost) ? PL_strdup(uHost) : nsnull;
        m_port = uPort;

        NS_UNLOCK_INSTANCE();
        return NS_OK;
    }

    // The URL is considered absolute if and only if it begins with a
    // protocol spec. A protocol spec is an alphanumeric string of 1 or
    // more characters that is terminated with a colon.
    PRBool isAbsolute = PR_FALSE;
    char* cp;
	char *imapPartOfUrl;
    char* ap = cSpec;
    char ch;
    while (0 != (ch = *ap)) 
	{
        if (((ch >= 'a') && (ch <= 'z')) ||
            ((ch >= 'A') && (ch <= 'Z')) ||
            ((ch >= '0') && (ch <= '9'))) 
		{
            ap++;
            continue;
        }
        if ((ch == ':') && (ap - cSpec >= 2)) 
		{
            isAbsolute = PR_TRUE;
            cp = ap;
            break;
        }
        break;
    }

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
    if (*cp == '/') 
	{
		cp++;
        if (*cp == '/') 
		{
			cp++;
			if (*cp == '/') 
				cp++;
        }
	} 
	else 
	{
		delete [] cSpec;
		NS_UNLOCK_INSTANCE();
        return NS_ERROR_ILLEGAL_VALUE;
    }

	// Host name follows protocol for http style urls
	const char* cp0 = cp;
	cp = PL_strpbrk(cp, "/:");
	if (nsnull == cp) 
	{
		// There is only a host name
		PRInt32 hlen = PL_strlen(cp0);
        m_host = (char*) PR_Malloc(hlen + 1);
        PL_strcpy(m_host, cp0);
	}
    else 
	{
		PRInt32 hlen = cp - cp0;
        m_host = (char*) PR_Malloc(hlen + 1);
        PL_strncpy(m_host, cp0, hlen);        
        m_host[hlen] = 0;

		if (':' == *cp) 
		{
			// We have a port number
            cp0 = cp+1;
            cp = PL_strchr(cp, '/');
            m_port = strtol(cp0, (char **)nsnull, 10);
        }
		imapPartOfUrl = cp;
        cp = PL_strchr(cp, '?');
        if (cp)
        {
            cp++;
            PRInt32 slen = PL_strlen(cp);
            m_search = (char*) PR_Malloc(slen+1);
            PL_strcpy(m_search, cp);
        }
	}

	ParseImapPart(imapPartOfUrl);
    delete [] cSpec;

    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

void nsImapUrl::ReconstructSpec(void)
{
    PR_FREEIF(m_spec);

    char portBuffer[10];
    if (-1 != m_port)
        PR_snprintf(portBuffer, 10, ":%d", m_port);
    else
        portBuffer[0] = '\0';

    PRInt32 plen = PL_strlen(m_protocol) + PL_strlen(m_host) +
        PL_strlen(portBuffer) + 4;

    if (m_search)
        plen += 1 + PL_strlen(m_search);

    m_spec = (char *) PR_Malloc(plen + 1);
    PR_snprintf(m_spec, plen, "%s://%s%s", 
                m_protocol, ((nsnull != m_host) ? m_host : ""), portBuffer);

    if (m_search) 
	{
        PL_strcat(m_spec, "?");
        PL_strcat(m_spec, m_search);
    }
}

////////////////////////////////////////////////////////////////////////////////

PRBool nsImapUrl::Equals(const nsIURL* aURL) const 
{
    PRBool bIsEqual;
    nsImapUrl* other = nsnull;
    NS_LOCK_INSTANCE();
	// are they both imap urls?? if yes...for now just compare the pointers until 
	// I figure out if we need to check any of the guts for 
    if (((nsIURL*)aURL)->QueryInterface(nsIImapUrl::GetIID(), (void**)&other) == NS_OK)
        bIsEqual = other == this; // compare the pointers...
    else
        bIsEqual = PR_FALSE;
    NS_UNLOCK_INSTANCE();
    return bIsEqual;
}

NS_IMETHODIMP nsImapUrl::GetProtocol(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_protocol;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetProtocol(const char *aNewProtocol)
{
    NS_LOCK_INSTANCE();
    m_protocol = nsCRT::strdup(aNewProtocol);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetHost(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_host;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetHost(const char *aNewHost)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_host = nsCRT::strdup(aNewHost);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetFile(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = nsnull;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetFile(const char *aNewFile)
{
	// imap doesn't have a file portion to the url yet...
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetSpec(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_spec;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetSpec(const char *aNewSpec)
{
    // XXX is this right, or should we call ParseURL?
    nsresult rv = NS_OK;
    NS_LOCK_INSTANCE();
    rv = ParseURL(aNewSpec);
    NS_UNLOCK_INSTANCE();
    return rv;
}

NS_IMETHODIMP nsImapUrl::GetRef(const char* *result) const
{
    *result = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetRef(const char *aNewRef)
{
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetHostPort(PRUint32 *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_port;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetHostPort(PRUint32 aNewPort)
{
    NS_LOCK_INSTANCE();
    m_port = aNewPort;
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetSearch(const char* *result) const
{
    NS_LOCK_INSTANCE();
    *result = m_search;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetSearch(const char *aNewSearch)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    NS_LOCK_INSTANCE();
    m_search = nsCRT::strdup(aNewSearch);
    ReconstructSpec();
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetContainer(nsISupports* *result) const
{
    *result = nsnull;
    return NS_OK;
}
  
NS_IMETHODIMP nsImapUrl::SetContainer(nsISupports* container)
{
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetContentLength(PRInt32 *len)
{
    NS_LOCK_INSTANCE();
    *len = m_URL_s->content_length;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::CreateListOfMessageIdsString(char **aResult) 
{
	if (nsnull == aResult || !m_listOfMessageIds) 
		return  NS_ERROR_NULL_POINTER;

    NS_LOCK_INSTANCE();

	char *returnIdString = PL_strdup(m_listOfMessageIds);
	if (returnIdString)
	{
		// mime may have glommed a "&part=" for a part download
		// we return the entire message and let mime extract
		// the part. Pop and news work this way also.
		// this algorithm truncates the "&part" string.
		char *currentChar = returnIdString;
		while (*currentChar && (*currentChar != '&'))
			currentChar++;
		if (*currentChar == '&')
			*currentChar = 0;

		// we should also strip off anything after "/;section="
		// since that can specify an IMAP MIME part
		char *wherepart = PL_strstr(returnIdString, "/;section=");
		if (wherepart)
			*wherepart = 0;
	}
	*aResult = returnIdString;

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
NS_IMETHODIMP nsImapUrl::GetLoadAttribs(nsILoadAttribs* *result) const
{
    NS_LOCK_INSTANCE();
    *result = NULL;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}
  
NS_IMETHODIMP nsImapUrl::SetLoadAttribs(nsILoadAttribs* aLoadAttribs)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetPostHeader(const char* name, const char* value)
{
    NS_LOCK_INSTANCE();
    // XXX
    PR_ASSERT(0);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::SetPostData(nsIInputStream* input)
{
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetURLGroup(nsIURLGroup* *result) const
{
    return NS_OK;
}
  
NS_IMETHODIMP nsImapUrl::SetURLGroup(nsIURLGroup* group)
{
    NS_ASSERTION(m_URL_s == nsnull, "URL has already been opened");
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetServerStatus(PRInt32 *status)
{
    NS_LOCK_INSTANCE();
    *status = m_URL_s->server_status;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}


NS_IMETHODIMP nsImapUrl::ToString(PRUnichar* *aString) const
{ 
	if (aString)
		*aString = nsnull; 
	return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetImapPartToFetch(char **result) 
{
    NS_LOCK_INSTANCE();
	//  here's the old code:
#if 0
	char *wherepart = NULL, *rv = NULL;
	if (m_listOfMessageIds && (wherepart = PL_strstr(m_listOfMessageIds, "/;section=")) != NULL)
	{
		wherepart += 10; // XP_STRLEN("/;section=")
		if (wherepart)
		{
			char *wherelibmimepart = XP_STRSTR(wherepart, "&part=");
			int len = PL_strlen(m_listOfMessageIds), numCharsToCopy = 0;
			if (wherelibmimepart)
				numCharsToCopy = (wherelibmimepart - wherepart);
			else
				numCharsToCopy = PL_strlen(m_listOfMessageIds) - (wherepart - m_listOfMessageIds);
			if (numCharsToCopy)
			{
				rv = (char *) PR_Malloc(sizeof(char) * (numCharsToCopy + 1));
				if (rv)
				{
					XP_STRNCPY_SAFE(rv, wherepart, numCharsToCopy + 1);	// appends a \0
				}
			}
		}
	}
#endif // 0 
    NS_UNLOCK_INSTANCE();
    return NS_OK;

}

char nsImapUrl::GetOnlineSubDirSeparator()
{
	return m_onlineSubDirSeparator;
}

void nsImapUrl::SetOnlineSubDirSeparator(char onlineDirSeparator)
{
	m_onlineSubDirSeparator = onlineDirSeparator;
}


NS_IMETHODIMP nsImapUrl::MessageIdsAreUids(PRBool *result)
{
    NS_LOCK_INSTANCE();
	*result = m_idsAreUids;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::GetMsgFlags(imapMessageFlagsType *result)	// kAddMsgFlags or kSubtractMsgFlags only
{
    NS_LOCK_INSTANCE();
	*result = m_flags;
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

void nsImapUrl::ParseImapPart(char *imapPartOfUrl)
{
	m_tokenPlaceHolder = imapPartOfUrl;
	m_urlidSubString = m_tokenPlaceHolder ? nsIMAPGenericParser::Imapstrtok_r(nil, IMAP_URL_TOKEN_SEPARATOR, &m_tokenPlaceHolder) : (char *)NULL;
	if (!m_urlidSubString)
	{
		m_validUrl = FALSE;
		return;
	}
	
	if (!PL_strcasecmp(m_urlidSubString, "fetch"))
	{
		m_imapAction   					 = nsImapMsgFetch;
		ParseUidChoice();
		ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		ParseListofMessageIds();
	}
	else /* if (fInternal) no concept of internal - not sure there will be one */
	{
		if (!PL_strcasecmp(m_urlidSubString, "header"))
		{
			m_imapAction   					 = nsImapMsgHeader;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
		}
		else if (!PL_strcasecmp(m_urlidSubString, "deletemsg"))
		{
			m_imapAction   					 = nsImapDeleteMsg;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
		}
		else if (!PL_strcasecmp(m_urlidSubString, "deleteallmsgs"))
		{
			m_imapAction   					 = nsImapDeleteAllMsgs;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "addmsgflags"))
		{
			m_imapAction   					 = nsImapAddMsgFlags;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
			ParseMsgFlags();
		}
		else if (!PL_strcasecmp(m_urlidSubString, "subtractmsgflags"))
		{
			m_imapAction   					 = nsImapSubtractMsgFlags;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
			ParseMsgFlags();
		}
		else if (!PL_strcasecmp(m_urlidSubString, "setmsgflags"))
		{
			m_imapAction   					 = nsImapSetMsgFlags;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
			ParseMsgFlags();
		}
		else if (!PL_strcasecmp(m_urlidSubString, "onlinecopy"))
		{
			m_imapAction   					 = nsImapOnlineCopy;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
			ParseFolderPath(&m_destinationCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "onlinemove"))
		{
			m_imapAction   					 = nsImapOnlineMove;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
			ParseFolderPath(&m_destinationCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "onlinetoofflinecopy"))
		{
			m_imapAction   					 = nsImapOnlineToOfflineCopy;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
			ParseFolderPath(&m_destinationCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "onlinetoofflinemove"))
		{
			m_imapAction   					 = nsImapOnlineToOfflineMove;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
			ParseFolderPath(&m_destinationCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "offlinetoonlinecopy"))
		{
			m_imapAction   					 = nsImapOfflineToOnlineMove;
			ParseFolderPath(&m_destinationCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "search"))
		{
			m_imapAction   					 = nsImapSearch;
			ParseUidChoice();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseSearchCriteriaString();
		}
		else if (!PL_strcasecmp(m_urlidSubString, "test"))
		{
			m_imapAction   					 = nsImapTest;
		}
		else if (!PL_strcasecmp(m_urlidSubString, "select"))
		{
			m_imapAction   					 = nsImapSelectFolder;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			if (m_tokenPlaceHolder && *m_tokenPlaceHolder)
				ParseListofMessageIds();
			else
				m_listOfMessageIds = "";
		}
		else if (!PL_strcasecmp(m_urlidSubString, "liteselect"))
		{
			m_imapAction   					 = nsImapLiteSelectFolder;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "selectnoop"))
		{
			m_imapAction   					 = nsImapSelectNoopFolder;
			m_listOfMessageIds = "";
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "expunge"))
		{
			m_imapAction   					 = nsImapExpungeFolder;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			m_listOfMessageIds = "";		// no ids to UNDO
		}
		else if (!PL_strcasecmp(m_urlidSubString, "create"))
		{
			m_imapAction   					 = nsImapCreateFolder;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "discoverchildren"))
		{
			m_imapAction   					 = nsImapDiscoverChildrenUrl;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "discoverlevelchildren"))
		{
			m_imapAction   					 = nsImapDiscoverLevelChildrenUrl;
			ParseChildDiscoveryDepth();
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "discoverallboxes"))
		{
			m_imapAction   					 = nsImapDiscoverAllBoxesUrl;
		}
		else if (!PL_strcasecmp(m_urlidSubString, "discoverallandsubscribedboxes"))
		{
			m_imapAction   					 = nsImapDiscoverAllAndSubscribedBoxesUrl;
		}
		else if (!PL_strcasecmp(m_urlidSubString, "delete"))
		{
			m_imapAction   					 = nsImapDeleteFolder;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "rename"))
		{
			m_imapAction   					 = nsImapRenameFolder;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseFolderPath(&m_destinationCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "movefolderhierarchy"))
		{
			m_imapAction   					 = nsImapMoveFolderHierarchy;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			if (m_tokenPlaceHolder && *m_tokenPlaceHolder)	// handle promote to root
				ParseFolderPath(&m_destinationCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "list"))
		{
			m_imapAction   					 = nsImapLsubFolders;
			ParseFolderPath(&m_destinationCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "biff"))
		{
			m_imapAction   					 = nsImapBiff;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
			ParseListofMessageIds();
		}
		else if (!PL_strcasecmp(m_urlidSubString, "netscape"))
		{
			m_imapAction   					 = nsImapGetMailAccountUrl;
		}
		else if (!PL_strcasecmp(m_urlidSubString, "appendmsgfromfile"))
		{
			m_imapAction					 = nsImapAppendMsgFromFile;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "appenddraftfromfile"))
		{
			m_imapAction					 = nsImapAppendMsgFromFile;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "subscribe"))
		{
			m_imapAction					 = nsImapSubscribe;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "unsubscribe"))
		{
			m_imapAction					 = nsImapUnsubscribe;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "refreshacl"))
		{
			m_imapAction					= nsImapRefreshACL;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "refreshfolderurls"))
		{
			m_imapAction					= nsImapRefreshFolderUrls;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "refreshallacls"))
		{
			m_imapAction					= nsImapRefreshAllACLs;
		}
		else if (!PL_strcasecmp(m_urlidSubString, "listfolder"))
		{
			m_imapAction					= nsImapListFolder;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "upgradetosubscription"))
		{
			m_imapAction					= nsImapUpgradeToSubscription;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else if (!PL_strcasecmp(m_urlidSubString, "folderstatus"))
		{
			m_imapAction					= nsImapFolderStatus;
			ParseFolderPath(&m_sourceCanonicalFolderPathSubString);
		}
		else
		{
			m_validUrl = PR_FALSE;	
		}
	}
}


// Returns NULL if nothing was done.
// Otherwise, returns a newly allocated name.
char *nsImapUrl::AddOnlineDirectoryIfNecessary(const char *onlineMailboxName)
{
	char *rv = NULL;
#ifdef HAVE_PORT
	// If this host has an online server directory configured
	char *onlineDir = TIMAPHostInfo::GetOnlineDirForHost(GetUrlHost());
	if (onlineMailboxName && onlineDir)
	{
#ifdef DEBUG
		// This invariant should be maintained by libmsg when reading/writing the prefs.
		// We are only supporting online directories whose online delimiter is /
		// Therefore, the online directory must end in a slash.
		XP_ASSERT(onlineDir[XP_STRLEN(onlineDir) - 1] == '/');
#endif
		TIMAPNamespace *ns = TIMAPHostInfo::GetNamespaceForMailboxForHost(GetUrlHost(), onlineMailboxName);
		NS_ASSERTION(ns, "couldn't find namespace for host");
		if (ns && (PL_strlen(ns->GetPrefix()) == 0) && PL_strcasecmp(onlineMailboxName, "INBOX"))
		{
			// Also make sure that the first character in the mailbox name is not '/'.
			NS_ASSERTION(*onlineMailboxName != '/', "first char of onlinemailbox is //");

			// The namespace for this mailbox is the root ("").
			// Prepend the online server directory
			int finalLen = XP_STRLEN(onlineDir) + XP_STRLEN(onlineMailboxName) + 1;
			rv = (char *)XP_ALLOC(finalLen);
			if (rv)
			{
				XP_STRCPY(rv, onlineDir);
				XP_STRCAT(rv, onlineMailboxName);
			}
		}
	}
#endif // HAVE_PORT
	return rv;
}

// Converts from canonical format (hierarchy is indicated by '/' and all real slashes ('/') are escaped)
// to the real online name on the server.
char *nsImapUrl::AllocateServerPath(const char *canonicalPath, char onlineDelimiter /* = kOnlineHierarchySeparatorUnknown */)
{
	char *rv = NULL;
	char delimiterToUse = onlineDelimiter;
	if (onlineDelimiter == kOnlineHierarchySeparatorUnknown)
		delimiterToUse = GetOnlineSubDirSeparator();
	NS_ASSERTION(delimiterToUse != kOnlineHierarchySeparatorUnknown, "hierarchy separator unknown");
	if (canonicalPath)
		rv = ReplaceCharsInCopiedString(canonicalPath, '/', delimiterToUse);
	else
		rv = PL_strdup("");

	char *onlineNameAdded = AddOnlineDirectoryIfNecessary(rv);
	if (onlineNameAdded)
	{
		PR_FREEIF(rv);
		rv = onlineNameAdded;
	}

	return rv;

}


NS_IMETHODIMP nsImapUrl::AllocateCanonicalPath(const char *serverPath, char onlineDelimiter, char **allocatedPath ) 
{
    nsresult rv = NS_ERROR_NULL_POINTER;
    char *canonicalPath = nsnull;
	char delimiterToUse = onlineDelimiter;
    const char* hostName = nsnull;
    char* userName = nsnull;
    nsString aString;
	char *currentPath = (char *) serverPath;
    char *onlineDir = nsnull;

	NS_LOCK_INSTANCE();

    NS_WITH_SERVICE(nsIImapHostSessionList, hostSessionList,
                    kCImapHostSessionListCID, &rv);    

    *allocatedPath = nsnull;

	if (onlineDelimiter == kOnlineHierarchySeparatorUnknown ||
		onlineDelimiter == 0)
		delimiterToUse = GetOnlineSubDirSeparator();

	NS_ASSERTION (serverPath, "Oops... null serverPath");

	if (!serverPath)
		goto done;

    if (NS_FAILED(rv))
        goto done;

    GetHost(&hostName);
    m_server->GetUserName(&userName);

    hostSessionList->GetOnlineDirForHost(hostName, userName, aString); 
    // First we have to check to see if we should strip off an online server
    // subdirectory 
	// If this host has an online server directory configured
	onlineDir = aString.Length() > 0? aString.ToNewCString(): nsnull;

	if (currentPath && onlineDir)
	{
#ifdef DEBUG
		// This invariant should be maintained by libmsg when reading/writing the prefs.
		// We are only supporting online directories whose online delimiter is /
		// Therefore, the online directory must end in a slash.
		NS_ASSERTION (onlineDir[PL_strlen(onlineDir) - 1] == '/', 
                      "Oops... online dir not end in a slash");
#endif

		// By definition, the online dir must be at the root.
		int len = PL_strlen(onlineDir);
		if (!PL_strncmp(onlineDir, currentPath, len))
		{
			// This online path begins with the server sub directory
			currentPath += len;

			// This might occur, but it's most likely something not good.
			// Basically, it means we're doing something on the online sub directory itself.
			NS_ASSERTION (*currentPath, "Oops ... null currentPath");
			// Also make sure that the first character in the mailbox name is not '/'.
			NS_ASSERTION (*currentPath != '/', 
                          "Oops ... currentPath starts with a slash");
		}
	}

	if (!currentPath)
		goto done;

	// Now, start the conversion to canonical form.
	canonicalPath = ReplaceCharsInCopiedString(currentPath, delimiterToUse ,
                                               '/');
	
	// eat any escape characters for escaped dir separators
	if (canonicalPath)
	{
		char *currentEscapeSequence = PL_strstr(canonicalPath, "\\/");
		while (currentEscapeSequence)
		{
			PL_strcpy(currentEscapeSequence, currentEscapeSequence+1);
			currentEscapeSequence = PL_strstr(currentEscapeSequence+1, "\\/");
		}
        *allocatedPath = canonicalPath;
	}

done:
    PR_FREEIF(userName);
    PR_FREEIF(onlineDir);

    NS_UNLOCK_INSTANCE();
    return rv;
}

NS_IMETHODIMP  nsImapUrl::CreateServerSourceFolderPathString(char **result)
{
	if (!result)
	    return NS_ERROR_NULL_POINTER;
	NS_LOCK_INSTANCE();
	*result = AllocateServerPath(m_sourceCanonicalFolderPathSubString);

    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsImapUrl::CreateCanonicalSourceFolderPathString(char **result)
{
	if (!result)
	    return NS_ERROR_NULL_POINTER;
	NS_LOCK_INSTANCE();
	*result = PL_strdup(m_sourceCanonicalFolderPathSubString ? m_sourceCanonicalFolderPathSubString : "");
    NS_UNLOCK_INSTANCE();
	return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


char *nsImapUrl::ReplaceCharsInCopiedString(const char *stringToCopy, char oldChar, char newChar)
{	
	char oldCharString[2];
	*oldCharString = oldChar;
	*(oldCharString+1) = 0;
	
	char *translatedString = PL_strdup(stringToCopy);
	char *currentSeparator = strstr(translatedString, oldCharString);
	
	while(currentSeparator)
	{
		*currentSeparator = newChar;
		currentSeparator = strstr(currentSeparator+1, oldCharString);
	}

	return translatedString;
}


////////////////////////////////////////////////////////////////////////////////////
// End of functions which should be made obsolete after modifying nsIURL
////////////////////////////////////////////////////////////////////////////////////

void nsImapUrl::ParseFolderPath(char **resultingCanonicalPath)
{
	*resultingCanonicalPath = m_tokenPlaceHolder ? nsIMAPGenericParser::Imapstrtok_r(nil, IMAP_URL_TOKEN_SEPARATOR, &m_tokenPlaceHolder) : (char *)NULL;
	
	if (!*resultingCanonicalPath)
	{
		m_validUrl = PR_FALSE;
		return;
	}

	// The delimiter will be set for a given URL, but will not be statically available
	// from an arbitrary URL.  It is the creator's responsibility to fill in the correct
	// delimiter from the folder's namespace when creating the URL.
	char dirSeparator = *(*resultingCanonicalPath)++;
	if (dirSeparator != kOnlineHierarchySeparatorUnknown)
		SetOnlineSubDirSeparator( dirSeparator);
	
	// if dirSeparator == kOnlineHierarchySeparatorUnknown, then this must be a create
	// of a top level imap box.  If there is an online subdir, we will automatically
	// use its separator.  If there is not an online subdir, we don't need a separator.
	
}


void nsImapUrl::ParseSearchCriteriaString()
{
	m_searchCriteriaString = m_tokenPlaceHolder ? nsIMAPGenericParser::Imapstrtok_r(nil, IMAP_URL_TOKEN_SEPARATOR, &m_tokenPlaceHolder) : (char *)NULL;
	if (!m_searchCriteriaString)
		m_validUrl = FALSE;
}


void nsImapUrl::ParseChildDiscoveryDepth()
{
	char *discoveryDepth = m_tokenPlaceHolder ? nsIMAPGenericParser::Imapstrtok_r(nil, IMAP_URL_TOKEN_SEPARATOR, &m_tokenPlaceHolder) : (char *)NULL;
	if (!discoveryDepth)
	{
		m_validUrl = PR_FALSE;
		m_discoveryDepth = 0;
		return;
	}
	m_discoveryDepth = atoi(discoveryDepth);
}

void nsImapUrl::ParseUidChoice()
{
	char *uidChoiceString = m_tokenPlaceHolder ? nsIMAPGenericParser::Imapstrtok_r(nil, IMAP_URL_TOKEN_SEPARATOR, &m_tokenPlaceHolder) : (char *)NULL;
	if (!uidChoiceString)
		m_validUrl = FALSE;
	else
		m_idsAreUids = PL_strcmp(uidChoiceString, "UID") == 0;
}

void nsImapUrl::ParseMsgFlags()
{
	char *flagsPtr = m_tokenPlaceHolder ? nsIMAPGenericParser::Imapstrtok_r(nil, IMAP_URL_TOKEN_SEPARATOR, &m_tokenPlaceHolder) : (char *)NULL;
	if (flagsPtr)
	{
		// the url is encodes the flags byte as ascii 
		int intFlags = atoi(flagsPtr);
		m_flags = (imapMessageFlagsType) intFlags;	// cast here 
	}
	else
		m_flags = 0;
}

void nsImapUrl::ParseListofMessageIds()
{
	m_listOfMessageIds = m_tokenPlaceHolder ? nsIMAPGenericParser::Imapstrtok_r(nil, IMAP_URL_TOKEN_SEPARATOR, &m_tokenPlaceHolder) : (char *)NULL;
	if (!m_listOfMessageIds)
		m_validUrl = PR_FALSE;
	else
	{
		m_mimePartSelectorDetected = PL_strstr(m_listOfMessageIds, "&part=") != 0;
	}
}

