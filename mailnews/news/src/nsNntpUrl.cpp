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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "msgCore.h"    // precompiled header...
#include "prlog.h"

#include "nsIURL.h"
#include "nsNntpUrl.h"

#include "nsString.h"
#include "nsReadableUtils.h"
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
#include "nsMsgNewsCID.h"
#include "nsIRDFService.h"
#include "rdf.h"
#include "nsIMsgFolder.h"
#include "nsINntpService.h"
#include "nsIMsgMessageService.h"

static NS_DEFINE_CID(kCNewsDB, NS_NEWSDB_CID);
    
nsNntpUrl::nsNntpUrl()
{
  m_newsgroupPost = nsnull;
  m_newsAction = nsINntpUrl::ActionGetNewNews;
  m_addDummyEnvelope = PR_FALSE;
  m_canonicalLineEnding = PR_FALSE;
  m_filePath = nsnull;
  m_getOldMessages = PR_FALSE;
}
         
nsNntpUrl::~nsNntpUrl()
{
  NS_IF_RELEASE(m_newsgroupPost);
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
NS_IMETHODIMP nsNntpUrl::SetSpec(const char * aSpec)
{
	nsresult rv = nsMsgMailNewsUrl::SetSpec(aSpec);
	if (NS_SUCCEEDED(rv))
		ParseUrl(aSpec);
	return rv;
}

nsresult nsNntpUrl::ParseUrl(const char * aSpec)
{
  char * partString = PL_strcasestr(aSpec, "?part=");
  if (partString)
    m_newsAction = nsINntpUrl::ActionFetchPart;
  else
    m_newsAction = nsINntpUrl::ActionFetchArticle;

  // XXX to do: add more smart detection code for setting the action type based on the contents of the url
  // string.
  return NS_OK;
}

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

NS_IMETHODIMP nsNntpUrl::SetUri(const char * aURI)
{
  mURI = aURI;
  return NS_OK;
}

// from nsIMsgMessageUrl
NS_IMETHODIMP nsNntpUrl::GetUri(char ** aURI)
{
  nsresult rv = NS_OK;

  // if we have been given a uri to associate with this url, then use it
  // otherwise try to reconstruct a URI on the fly....
  if (mURI.IsEmpty()) {
    nsXPIDLCString spec;
    rv = GetSpec(getter_Copies(spec));
    NS_ENSURE_SUCCESS(rv,rv);
    mURI = (const char *)spec;
  }
  
  *aURI = ToNewCString(mURI);
  if (!*aURI) return NS_ERROR_OUT_OF_MEMORY; 
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
  nsresult rv;
  
  nsCOMPtr <nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  nsCOMPtr <nsIMsgMessageService> msgService = do_QueryInterface(nntpService, &rv);
  NS_ENSURE_SUCCESS(rv,rv);

  if (mOriginalSpec.IsEmpty()) {
    // this can happen when viewing a news://host/message-id url
    return NS_ERROR_FAILURE;
  }

  rv = msgService->MessageURIToMsgHdr(mOriginalSpec.get(), aMsgHdr);
  NS_ENSURE_SUCCESS(rv,rv);
  
  return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::IsUrlType(PRUint32 type, PRBool *isType)
{
	NS_ENSURE_ARG(isType);

	switch(type)
	{
		case nsIMsgMailNewsUrl::eDisplay:
			*isType = (m_newsAction == nsINntpUrl::ActionFetchArticle);
			break;
		default:
			*isType = PR_FALSE;
	};				

	return NS_OK;

}

NS_IMETHODIMP
nsNntpUrl::GetOriginalSpec(char **aSpec)
{
    NS_ENSURE_ARG_POINTER(aSpec);
    *aSpec = nsCRT::strdup((const char *)mOriginalSpec);
    if (!*aSpec) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsNntpUrl::SetOriginalSpec(const char *aSpec)
{
    mOriginalSpec = aSpec;
    return NS_OK;
}

nsresult nsNntpUrl::GetMsgFolder(nsIMsgFolder **msgFolder)
{
   nsresult rv;

   if (mOriginalSpec.IsEmpty()) {
    // this could be a autosubscribe url (news://host/group)
    // or a message id url (news://host/message-id)
    // either way, we won't have a msgFolder for you
    return NS_ERROR_FAILURE;
   }

   nsCOMPtr <nsINntpService> nntpService = do_GetService(NS_NNTPSERVICE_CONTRACTID, &rv);
   NS_ENSURE_SUCCESS(rv,rv);

   nsMsgKey msgKey;
   // XXX should we find the first "?" in the mOriginalSpec, cut there, and pass that in?
   rv = nntpService->DecomposeNewsURI(mOriginalSpec.get(), msgFolder, &msgKey);
   NS_ENSURE_SUCCESS(rv,rv);
   return NS_OK;
}

NS_IMETHODIMP 
nsNntpUrl::GetFolderCharset(PRUnichar **aCharacterSet)
{
  nsCOMPtr<nsIMsgFolder> folder;
  nsresult rv = GetMsgFolder(getter_AddRefs(folder));
  // don't assert here.  this can happen if there is no message folder
  // like when we display a news://host/message-id url
  if (NS_FAILED(rv)) return rv;

  NS_ENSURE_TRUE(folder, NS_ERROR_FAILURE);
  rv = folder->GetCharset(aCharacterSet);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsNntpUrl::GetFolderCharsetOverride(PRBool * aCharacterSetOverride)
{
  nsCOMPtr<nsIMsgFolder> folder;
  nsresult rv = GetMsgFolder(getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ENSURE_TRUE(folder, NS_ERROR_FAILURE);
  rv = folder->GetCharsetOverride(aCharacterSetOverride);
  NS_ENSURE_SUCCESS(rv,rv);
  return rv;
}

NS_IMETHODIMP nsNntpUrl::GetCharsetOverRide(PRUnichar ** aCharacterSet)
{
  if (!mCharsetOverride.IsEmpty())
    *aCharacterSet = ToNewUnicode(mCharsetOverride); 
  else
    *aCharacterSet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsNntpUrl::SetCharsetOverRide(const PRUnichar * aCharacterSet)
{
  mCharsetOverride = aCharacterSet;
  return NS_OK;
}
