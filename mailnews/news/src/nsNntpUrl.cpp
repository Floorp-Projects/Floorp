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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "msgCore.h"    // precompiled header...
#include "prlog.h"

#include "nsIURL.h"
#include "nsNntpUrl.h"

#include "nsString.h"
#include "prmem.h"
#include "plstr.h"
#include "prprf.h"
#include "nsCRT.h"
#include "nsNewsUtils.h"

#include "nntpCore.h"

#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIMsgDatabase.h"
#include "nsMsgDBCID.h"
#include "nsIRDFService.h"
#include "rdf.h"
#include "nsIMsgFolder.h"
#include "nsIMessage.h"


static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
    
nsNntpUrl::nsNntpUrl()
{
	// nsINntpUrl specific code...
	m_newsHost = nsnull;
	m_articleList = nsnull;
	m_newsgroup = nsnull;
	m_offlineNews = nsnull;
	m_newsgroupList = nsnull;
  m_newsgroupPost = nsnull;
  m_newsgroupName = nsnull;
  m_messageKey = nsMsgKey_None;
	m_newsAction = nsINntpUrl::ActionGetNewNews;
  m_addDummyEnvelope = PR_FALSE;
  m_canonicalLineEnding = PR_FALSE;
	m_filePath = nsnull;
	m_getOldMessages = PR_FALSE;
}
 
nsNntpUrl::~nsNntpUrl()
{
	NS_IF_RELEASE(m_newsHost);
	NS_IF_RELEASE(m_articleList);
	NS_IF_RELEASE(m_newsgroup);
	NS_IF_RELEASE(m_offlineNews);
	NS_IF_RELEASE(m_newsgroupList);
  NS_IF_RELEASE(m_newsgroupPost);
  PR_FREEIF(m_newsgroupName);
}
  
NS_IMPL_ADDREF_INHERITED(nsNntpUrl, nsMsgMailNewsUrl)
NS_IMPL_RELEASE_INHERITED(nsNntpUrl, nsMsgMailNewsUrl)

NS_INTERFACE_MAP_BEGIN(nsNntpUrl)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsINntpUrl)
   NS_INTERFACE_MAP_ENTRY(nsINntpUrl)
   NS_INTERFACE_MAP_ENTRY(nsIMsgMessageUrl)
   NS_INTERFACE_MAP_ENTRY(nsIMsgI18NUrl)
NS_INTERFACE_MAP_END_INHERITING(nsMsgMailNewsUrl)

////////////////////////////////////////////////////////////////////////////////////
// Begin nsINntpUrl specific support
////////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsNntpUrl::SetGetOldMessages(PRBool aGetOldMessages)
{
	m_getOldMessages = aGetOldMessages;
	return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::GetGetOldMessages(PRBool * aGetOldMessages)
{
	NS_ENSURE_ARG(aGetOldMessages);
	*aGetOldMessages = m_getOldMessages;
	return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::GetNewsAction(nsNewsAction *aNewsAction)
{
	if (aNewsAction)
		*aNewsAction = m_newsAction;
	return NS_OK;
}


NS_IMETHODIMP nsNntpUrl::SetNewsAction(nsNewsAction aNewsAction)
{
	m_newsAction = aNewsAction;
	return NS_OK;
}

nsresult nsNntpUrl::SetNntpHost (nsINNTPHost * newsHost)
{
	NS_LOCK_INSTANCE();
	if (newsHost)
	{
		NS_IF_RELEASE(m_newsHost);
		m_newsHost = newsHost;
		NS_ADDREF(m_newsHost);
	}
	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

nsresult nsNntpUrl::GetNntpHost (nsINNTPHost ** newsHost)
{
    NS_LOCK_INSTANCE();
	if (newsHost)
	{
		*newsHost = m_newsHost;
		NS_IF_ADDREF(m_newsHost);
	}
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsNntpUrl::SetNntpArticleList (nsINNTPArticleList * articleList)
{
	NS_LOCK_INSTANCE();
	if (articleList)
	{
		NS_IF_RELEASE(m_articleList);
		m_articleList = articleList;
		NS_ADDREF(m_articleList);
	}
	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

nsresult nsNntpUrl::GetNntpArticleList (nsINNTPArticleList ** articleList)
{
	NS_LOCK_INSTANCE();
	if (articleList)
	{
		*articleList = m_articleList;
		NS_IF_ADDREF(m_articleList);
	}
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsNntpUrl::SetNewsgroup (nsINNTPNewsgroup * newsgroup)
{
	NS_LOCK_INSTANCE();
	if (newsgroup)
	{
		NS_IF_RELEASE(m_newsgroup);
		m_newsgroup = newsgroup;
		NS_ADDREF(m_newsgroup);
	}
	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

nsresult nsNntpUrl::GetNewsgroup (nsINNTPNewsgroup ** newsgroup)
{
	NS_LOCK_INSTANCE();
	if (newsgroup)
	{
		*newsgroup = m_newsgroup;
		NS_IF_ADDREF(m_newsgroup);
	}
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsNntpUrl::SetOfflineNewsState (nsIMsgOfflineNewsState * offlineNews)
{
	NS_LOCK_INSTANCE();
	if (offlineNews)
	{
		NS_IF_RELEASE(m_offlineNews);
		m_offlineNews = offlineNews;
		NS_ADDREF(m_offlineNews);
	}
	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

nsresult nsNntpUrl::GetOfflineNewsState (nsIMsgOfflineNewsState ** offlineNews) 
{
	NS_LOCK_INSTANCE();
	if (offlineNews)
	{
		*offlineNews = m_offlineNews;
		NS_IF_ADDREF(m_offlineNews);
	}
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsNntpUrl::SetNewsgroupList (nsINNTPNewsgroupList * newsgroupList)
{
	NS_LOCK_INSTANCE();
	if (newsgroupList)
	{
		NS_IF_RELEASE(m_newsgroupList);
		m_newsgroupList = newsgroupList;
		NS_IF_ADDREF(m_newsgroupList);
	}
	NS_UNLOCK_INSTANCE();
	return NS_OK;
}

nsresult nsNntpUrl::GetNewsgroupList (nsINNTPNewsgroupList ** newsgroupList) 
{
	NS_LOCK_INSTANCE();
	if (newsgroupList)
	{
		*newsgroupList = m_newsgroupList;
		NS_IF_ADDREF(m_newsgroupList);
	}
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::SetUri(const char * aURI)
{
  mURI= aURI;
  return NS_OK;
}

// from nsIMsgMessageUrl
NS_IMETHODIMP nsNntpUrl::GetUri(char ** aURI)
{	
	nsresult rv = NS_OK;
  // if we have been given a uri to associate with this url, then use it
  // otherwise try to reconstruct a URI on the fly....

  if (!mURI.IsEmpty())
    *aURI = mURI.ToNewCString();
	else
	{
		nsXPIDLCString spec;
		GetSpec(getter_Copies(spec));
		char * baseMessageURI;
		nsCreateNewsBaseMessageURI(spec, &baseMessageURI);
		nsCAutoString uriStr;

		rv = nsBuildNewsMessageURI(baseMessageURI, m_messageKey, uriStr);
		nsCRT::free(baseMessageURI);
		if (NS_FAILED(rv)) return rv;
		*aURI = uriStr.ToNewCString();
		return NS_OK;
	}

  return rv;
}

NS_IMPL_GETSET(nsNntpUrl, AddDummyEnvelope, PRBool, m_addDummyEnvelope);
NS_IMPL_GETSET(nsNntpUrl, CanonicalLineEnding, PRBool, m_canonicalLineEnding);

NS_IMETHODIMP nsNntpUrl::SetMessageFile(nsIFileSpec * aFileSpec)
{
	m_messageFileSpec = dont_QueryInterface(aFileSpec);
	return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::GetMessageFile(nsIFileSpec ** aFileSpec)
{
	if (aFileSpec)
	{
		*aFileSpec = m_messageFileSpec;
		NS_IF_ADDREF(*aFileSpec);
	}
	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////////
// End nsINntpUrl specific support
////////////////////////////////////////////////////////////////////////////////////

nsresult nsNntpUrl::SetMessageToPost(nsINNTPNewsgroupPost *post)
{
    NS_LOCK_INSTANCE();
    NS_IF_RELEASE(m_newsgroupPost);
    m_newsgroupPost=post;
    if (m_newsgroupPost) NS_ADDREF(m_newsgroupPost);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

nsresult nsNntpUrl::GetMessageToPost(nsINNTPNewsgroupPost **aPost)
{
    NS_LOCK_INSTANCE();
    if (!aPost) return NS_ERROR_NULL_POINTER;
    *aPost = m_newsgroupPost;
    if (*aPost) NS_ADDREF(*aPost);
    NS_UNLOCK_INSTANCE();
    return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::GetMessageHeader(nsIMsgDBHdr ** aMsgHdr)
{
    nsresult rv = NS_OK;
    nsFileSpec pathResult;
    
    if (!aMsgHdr) return NS_ERROR_NULL_POINTER;

    if (!m_newsgroupName) return NS_ERROR_FAILURE;

    nsXPIDLCString hostName;
    rv = GetHost(getter_Copies(hostName));
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString userName;
    rv = GetPreHost(getter_Copies(userName));
    if (NS_FAILED(rv)) return rv; 

    nsCString newsgroupURI(kNewsMessageRootURI);
    newsgroupURI.Append("/");
    if (userName && (userName != (const char *)"")) {
	newsgroupURI.Append(userName);
	newsgroupURI.Append("@");
    }
    newsgroupURI.Append(hostName);
    newsgroupURI.Append("/");
    newsgroupURI.Append(m_newsgroupName);
    
    rv = nsNewsURI2Path(kNewsMessageRootURI, newsgroupURI.GetBuffer(), pathResult);
    if (NS_FAILED(rv)) {
        return rv;
    }
    
    nsCOMPtr<nsIMsgDatabase> newsDBFactory;
    nsCOMPtr<nsIMsgDatabase> newsDB;
    
    rv = nsComponentManager::CreateInstance(kCNewsDB, nsnull, NS_GET_IID(nsIMsgDatabase), getter_AddRefs(newsDBFactory));
    if (NS_FAILED(rv) || (!newsDBFactory)) {
        return rv;
    }
    
	nsCOMPtr <nsIFileSpec> dbFileSpec;
	NS_NewFileSpecWithSpec(pathResult, getter_AddRefs(dbFileSpec));
    rv = newsDBFactory->Open(dbFileSpec, PR_TRUE, PR_FALSE, getter_AddRefs(newsDB));
    
    if (NS_FAILED(rv) || (!newsDB)) {
        return rv;
    }
    
    rv = newsDB->GetMsgHdrForKey(m_messageKey, aMsgHdr);
    if (NS_FAILED(rv) || (!aMsgHdr)) {
        return rv;
    }
  
	return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::SetNewsgroupName(const char * aNewsgroupName)
{
    if (!aNewsgroupName) return NS_ERROR_NULL_POINTER;

    PR_FREEIF(m_newsgroupName);
    
    m_newsgroupName = PL_strdup(aNewsgroupName);
    if (!m_newsgroupName) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        return NS_OK;
    }    
}

NS_IMETHODIMP nsNntpUrl::GetNewsgroupName(char ** aNewsgroupName)
{
    if (!aNewsgroupName) return NS_ERROR_NULL_POINTER;

    if (!m_newsgroupName) {
		*aNewsgroupName = nsnull;
		return NS_OK;
	}

    *aNewsgroupName = PL_strdup(m_newsgroupName);
    if (!aNewsgroupName) {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    else {
        return NS_OK;
    }
}     

NS_IMETHODIMP nsNntpUrl::SetMessageKey(nsMsgKey aKey)
{
    m_messageKey = aKey;
    return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::GetMessageKey(nsMsgKey * aKey)
{
    *aKey = m_messageKey;
    return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::IsUrlType(PRUint32 type, PRBool *isType)
{
	NS_ENSURE_ARG(isType);

	switch(type)
	{
		case nsIMsgMailNewsUrl::eDisplay:
			*isType = (m_newsAction == nsINntpUrl::ActionDisplayArticle);
			break;
		default:
			*isType = PR_FALSE;
	};				

	return NS_OK;

}

NS_IMETHODIMP
nsNntpUrl::GetOriginalSpec(char **aSpec)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNntpUrl::SetOriginalSpec(const char *aSpec)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsNntpUrl::GetFolderCharset(PRUnichar ** aCharacterSet)
{

  // news folders don't have charset associated with them..
  *aCharacterSet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::GetCharsetOverRide(PRUnichar ** aCharacterSet)
{
  if (!mCharsetOverride.IsEmpty())
    *aCharacterSet = mCharsetOverride.ToNewUnicode(); 
  else
    *aCharacterSet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::SetCharsetOverRide(const PRUnichar * aCharacterSet)
{
  mCharsetOverride = aCharacterSet;
  return NS_OK;
}


