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

#include "nsIURI.h"
#include "nsIMailboxUrl.h"
#include "nsMailboxUrl.h"

#include "nsString.h"
#include "nsEscape.h"
#include "nsCRT.h"
#include "nsLocalUtils.h"
#include "nsIMsgDatabase.h"
#include "nsMsgDBCID.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgHdr.h"

#include "nsXPIDLString.h"

// we need this because of an egcs 1.0 (and possibly gcc) compiler bug
// that doesn't allow you to call ::nsISupports::GetIID() inside of a class
// that multiply inherits from nsISupports
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);
static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);

// this is totally lame and MUST be removed by M6
// the real fix is to attach the URI to the URL as it runs through netlib
// then grab it and use it on the other side
#include "nsIMsgMailSession.h"
#include "nsCOMPtr.h"
#include "nsMsgBaseCID.h"

static NS_DEFINE_CID(kMsgMailSessionCID, NS_MSGMAILSESSION_CID);

static char *nsMailboxGetURI(const char *nativepath)
{

    nsresult rv;
    char *uri = nsnull;

    NS_WITH_SERVICE(nsIMsgMailSession, session, kMsgMailSessionCID, &rv);
    if (NS_FAILED(rv)) return nsnull;

    nsCOMPtr<nsIMsgAccountManager> accountManager;
    rv = session->GetAccountManager(getter_AddRefs(accountManager));
    if (NS_FAILED(rv)) return nsnull;

    nsCOMPtr<nsISupportsArray> serverArray;
    accountManager->GetAllServers(getter_AddRefs(serverArray));

    // do a char*->fileSpec->char* conversion to normalize the path
    nsFilePath filePath(nativepath);
    
    PRUint32 cnt;
    rv = serverArray->Count(&cnt);
    if (NS_FAILED(rv)) return nsnull;
    PRInt32 count = cnt;
    PRInt32 i;
    for (i=0; i<count; i++) {

        nsISupports* serverSupports = serverArray->ElementAt(i);
        nsCOMPtr<nsIMsgIncomingServer> server =
            do_QueryInterface(serverSupports);
        NS_RELEASE(serverSupports);

        if (!server) continue;

        // get the path string, convert it to an nsFilePath
        char *nativeServerPath;
        rv = server->GetLocalPath(&nativeServerPath);
        if (NS_FAILED(rv)) continue;
        
        nsFileSpec spec(nativeServerPath);
        nsFilePath serverPath(spec);
        PL_strfree(nativeServerPath);
        
        // check if filepath begins with serverPath
        PRInt32 len = PL_strlen(serverPath);
        if (PL_strncasecmp(serverPath, filePath, len) == 0) {
            nsXPIDLCString serverURI;
            rv = server->GetServerURI(getter_Copies(serverURI));
            if (NS_FAILED(rv)) continue;
            
            // the relpath is just past the serverpath
            const char *relpath = nativepath + len;
            // skip past leading / if any
            while (*relpath == '/') relpath++;
            uri = PR_smprintf("%s/%s", (const char*)serverURI, relpath);

            break;
        }
    }
    return uri;
}


// helper function for parsing the search field of a url
char * extractAttributeValue(const char * searchString, const char * attributeName);

nsMailboxUrl::nsMailboxUrl()
{
	m_mailboxAction = nsIMailboxUrl::ActionParseMailbox;
	m_filePath = nsnull;
	m_messageID = nsnull;
	m_messageKey = 0;
	m_messageSize = 0;
	m_messageFileSpec = nsnull;
}
 
nsMailboxUrl::~nsMailboxUrl()
{
	delete m_filePath;

	PR_FREEIF(m_messageID);
}

NS_IMPL_ADDREF_INHERITED(nsMailboxUrl, nsMsgMailNewsUrl)
NS_IMPL_RELEASE_INHERITED(nsMailboxUrl, nsMsgMailNewsUrl)
  
nsresult nsMailboxUrl::QueryInterface(const nsIID &aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) 
	{
        return NS_ERROR_NULL_POINTER;
    }
 
    if (aIID.Equals(nsIMailboxUrl::GetIID()) || aIID.Equals(kISupportsIID)) 
	{
        *aInstancePtr = (void*) ((nsIMailboxUrl*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
	if (aIID.Equals(nsIMsgUriUrl::GetIID()))
	{
		*aInstancePtr = (void *) ((nsIMsgUriUrl *) this);
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
 
    return nsMsgMailNewsUrl::QueryInterface(aIID, aInstancePtr);
}

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIMailboxUrl specific support
////////////////////////////////////////////////////////////////////////////////////
nsresult nsMailboxUrl::SetMailboxParser(nsIStreamListener * aMailboxParser)
{
	NS_LOCK_INSTANCE();
	if (aMailboxParser)
		m_mailboxParser = dont_QueryInterface(aMailboxParser);
	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

nsresult nsMailboxUrl::GetMailboxParser(nsIStreamListener ** aConsumer)
{
	NS_LOCK_INSTANCE();
	if (aConsumer)
	{
		*aConsumer = m_mailboxParser;
		NS_IF_ADDREF(*aConsumer);
	}
	NS_UNLOCK_INSTANCE();
	return  NS_OK;
}

nsresult nsMailboxUrl::SetMailboxCopyHandler(nsIStreamListener * aMailboxCopyHandler)
{
	NS_LOCK_INSTANCE();
	if (aMailboxCopyHandler)
		m_mailboxCopyHandler = dont_QueryInterface(aMailboxCopyHandler);
	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

nsresult nsMailboxUrl::GetMailboxCopyHandler(nsIStreamListener ** aMailboxCopyHandler)
{
	NS_LOCK_INSTANCE();
	if (aMailboxCopyHandler)
	{
		*aMailboxCopyHandler = m_mailboxCopyHandler;
		NS_IF_ADDREF(*aMailboxCopyHandler);
	}
	NS_UNLOCK_INSTANCE();
	return  NS_OK;
}

nsresult nsMailboxUrl::GetFilePath(nsFileSpec ** aFilePath)
{
	if (aFilePath)
		*aFilePath = m_filePath;
	return NS_OK;
}

nsresult nsMailboxUrl::GetMessageKey(nsMsgKey* aMessageKey)
{
	*aMessageKey = m_messageKey;
	return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::GetMessageSize(PRUint32 * aMessageSize)
{
	if (aMessageSize)
	{
		*aMessageSize = m_messageSize;
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

nsresult nsMailboxUrl::SetMessageSize(PRUint32 aMessageSize)
{
	m_messageSize = aMessageSize;
	return NS_OK;
}


NS_IMETHODIMP nsMailboxUrl::GetURI(char ** aURI)
{
	// function not implemented yet....
	// when I get scott's function to take a path and a message id and turn it into
	// a URI then we can add that code here.....

	if (aURI)
	{
		nsFileSpec * filePath = nsnull;
		GetFilePath(&filePath);
		if (filePath)
		{
            char * baseuri = nsMailboxGetURI(m_file);
			char * uri = nsnull;
			nsFileSpec folder = *filePath;
			nsBuildLocalMessageURI(baseuri, m_messageKey, &uri);
            PL_strfree(baseuri);
			*aURI = uri;
		}
		else
			*aURI = nsnull;

	}

	return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::GetMessageHeader(nsIMsgDBHdr ** aMsgHdr)
{
	nsresult rv = NS_OK;
	if (aMsgHdr)
	{
		nsCOMPtr<nsIMsgDatabase> mailDBFactory;
		nsCOMPtr<nsIMsgDatabase> mailDB;

		rv = nsComponentManager::CreateInstance(kCMailDB, nsnull, nsIMsgDatabase::GetIID(), 
														 (void **) getter_AddRefs(mailDBFactory));
		nsCOMPtr <nsIFileSpec> dbFileSpec;
		NS_NewFileSpecWithSpec(*m_filePath, getter_AddRefs(dbFileSpec));

		if (NS_SUCCEEDED(rv) && mailDBFactory)
			rv = mailDBFactory->Open(dbFileSpec, PR_FALSE, PR_FALSE, (nsIMsgDatabase **) getter_AddRefs(mailDB));
		if (NS_SUCCEEDED(rv) && mailDB) // did we get a db back?
			rv = mailDB->GetMsgHdrForKey(m_messageKey, aMsgHdr);
	}
	else
		rv = NS_ERROR_NULL_POINTER;

	return rv;
}

NS_IMETHODIMP nsMailboxUrl::SetMessageFile(nsIFileSpec * aFileSpec)
{
	m_messageFileSpec = dont_QueryInterface(aFileSpec);
	return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::GetMessageFile(nsIFileSpec ** aFileSpec)
{
	if (aFileSpec)
	{
		*aFileSpec = m_messageFileSpec;
		NS_IF_ADDREF(*aFileSpec);
	}
	return NS_OK;
}

#if 0   // mscott - i can remove this function once I move the funcionality elsewhere...
NS_IMETHODIMP nsMailboxUrl::SetURLInfo(URL_Struct *URL_s)
{
	nsresult rv = nsMsgMailNewsUrl::SetURLInfo(URL_s);
	if (m_mailboxAction == nsIMailboxUrl::ActionDisplayMessage || m_mailboxAction == nsIMailboxUrl::ActionCopyMessage
		|| m_mailboxAction == nsIMailboxUrl::ActionMoveMessage || m_mailboxAction == 	nsIMailboxUrl::ActionSaveMessageToDisk 
		|| m_mailboxAction == nsIMailboxUrl::ActionAppendMessageToDisk)
	{
		// set the byte field range for the url struct...
		char * byteRange = PR_smprintf("bytes=%d-%d", m_messageKey, m_messageKey+m_messageSize - 1);
		m_URL_s->range_header = byteRange;
	}

    return rv;
}
#endif
////////////////////////////////////////////////////////////////////////////////////
// End nsIMailboxUrl specific support
////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// possible search part phrases include: MessageID=id&number=MessageKey

nsresult nsMailboxUrl::ParseSearchPart()
{
	nsXPIDLCString searchPart;
	nsresult rv = GetQuery(getter_Copies(searchPart));
	// add code to this function to decompose everything past the '?'.....
	if (NS_SUCCEEDED(rv) && searchPart)
	{
		char * messageKey = extractAttributeValue(searchPart, "number=");
		m_messageID = extractAttributeValue(searchPart,"messageid=");
		if (messageKey)
			m_messageKey = atol(messageKey); // convert to a long...
		if (messageKey || m_messageID)
			// the action for this mailbox must be a display message...
			m_mailboxAction = nsIMailboxUrl::ActionDisplayMessage;
		PR_FREEIF(messageKey);
	}
	else
		m_mailboxAction = nsIMailboxUrl::ActionParseMailbox;

	return rv;
}

// warning: don't assume when parsing the url that the protocol part is "news"...
nsresult nsMailboxUrl::ParseUrl(const nsString& aSpec)
{
	if (m_filePath)
		delete m_filePath;
	DirFile(getter_Copies(m_file));
	ParseSearchPart();
	m_filePath = new nsFileSpec(nsFilePath(m_file));
    return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::SetSpec(char * aSpec)
{
	nsresult rv = nsMsgMailNewsUrl::SetSpec(aSpec);
	if (NS_SUCCEEDED(rv))
		rv = ParseUrl("");
	return rv;
}

// takes a string like ?messageID=fooo&number=MsgKey and returns a new string 
// containing just the attribute value. i.e you could pass in this string with
// an attribute name of messageID and I'll return fooo. Use PR_Free to delete
// this string...

// Assumption: attribute pairs in the string are separated by '&'.
char * extractAttributeValue(const char * searchString, const char * attributeName)
{
	char * attributeValue = nsnull;

	if (searchString && attributeName)
	{
		// search the string for attributeName
		PRUint32 attributeNameSize = PL_strlen(attributeName);
		char * startOfAttribute = PL_strcasestr(searchString, attributeName);
		if (startOfAttribute)
		{
			startOfAttribute += attributeNameSize; // skip over the attributeName
			if (startOfAttribute) // is there something after the attribute name
			{
				char * endofAttribute = startOfAttribute ? PL_strchr(startOfAttribute, '&') : nsnull;
				if (startOfAttribute && endofAttribute) // is there text after attribute value
					attributeValue = PL_strndup(startOfAttribute, endofAttribute - startOfAttribute);
				else // there is nothing left so eat up rest of line.
					attributeValue = PL_strdup(startOfAttribute);

				// now unescape the string...
				if (attributeValue)
					attributeValue = nsUnescape(attributeValue); // unescape the string...
			} // if we have a attribute value

		} // if we have a attribute name
	} // if we got non-null search string and attribute name values

	return attributeValue;
}
