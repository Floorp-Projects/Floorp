/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "msgCore.h"    // precompiled header...

#include "nsIURI.h"
#include "nsIMailboxUrl.h"
#include "nsMailboxUrl.h"

#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsEscape.h"
#include "nsCRT.h"
#include "nsLocalUtils.h"
#include "nsIMsgDatabase.h"
#include "nsMsgDBCID.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgHdr.h"

#include "nsXPIDLString.h"
#include "nsIRDFService.h"
#include "rdf.h"
#include "nsIMsgFolder.h"
#include "prprf.h"
#include "nsISupportsObsolete.h"
#include "nsIMsgMailSession.h"

static NS_DEFINE_CID(kCMailDB, NS_MAILDB_CID);

// this is totally lame and MUST be removed by M6
// the real fix is to attach the URI to the URL as it runs through netlib
// then grab it and use it on the other side
#include "nsCOMPtr.h"
#include "nsMsgBaseCID.h"
#include "nsIMsgAccountManager.h"
#include "nsMsgUtils.h"


// helper function for parsing the search field of a url
char * extractAttributeValue(const char * searchString, const char * attributeName);

nsMailboxUrl::nsMailboxUrl()
{
  m_mailboxAction = nsIMailboxUrl::ActionParseMailbox;
  m_filePath = nsnull;
  m_messageID = nsnull;
  m_messageKey = nsMsgKey_None;
  m_messageSize = 0;
  m_messageFileSpec = nsnull;
  m_addDummyEnvelope = PR_FALSE;
  m_canonicalLineEnding = PR_FALSE;
  m_curMsgIndex = 0;
}
 
nsMailboxUrl::~nsMailboxUrl()
{
    delete m_filePath;
    PR_Free(m_messageID);
}

NS_IMPL_ADDREF_INHERITED(nsMailboxUrl, nsMsgMailNewsUrl)
NS_IMPL_RELEASE_INHERITED(nsMailboxUrl, nsMsgMailNewsUrl)

NS_INTERFACE_MAP_BEGIN(nsMailboxUrl)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMailboxUrl)
   NS_INTERFACE_MAP_ENTRY(nsIMailboxUrl)
   NS_INTERFACE_MAP_ENTRY(nsIMsgMessageUrl)
   NS_INTERFACE_MAP_ENTRY(nsIMsgI18NUrl)
NS_INTERFACE_MAP_END_INHERITING(nsMsgMailNewsUrl)

////////////////////////////////////////////////////////////////////////////////////
// Begin nsIMailboxUrl specific support
////////////////////////////////////////////////////////////////////////////////////
nsresult nsMailboxUrl::SetMailboxParser(nsIStreamListener * aMailboxParser)
{
  if (aMailboxParser)
    m_mailboxParser = aMailboxParser;
  return NS_OK;
}

nsresult nsMailboxUrl::GetMailboxParser(nsIStreamListener ** aConsumer)
{
  NS_ENSURE_ARG_POINTER(aConsumer);
  
  NS_IF_ADDREF(*aConsumer = m_mailboxParser);
  return  NS_OK;
}

nsresult nsMailboxUrl::SetMailboxCopyHandler(nsIStreamListener * aMailboxCopyHandler)
{
  if (aMailboxCopyHandler)
    m_mailboxCopyHandler = aMailboxCopyHandler;
  return NS_OK;
}

nsresult nsMailboxUrl::GetMailboxCopyHandler(nsIStreamListener ** aMailboxCopyHandler)
{
  NS_ENSURE_ARG_POINTER(aMailboxCopyHandler);
  
  if (aMailboxCopyHandler)
  {
    *aMailboxCopyHandler = m_mailboxCopyHandler;
    NS_IF_ADDREF(*aMailboxCopyHandler);
  }
  
  return  NS_OK;
}

nsresult nsMailboxUrl::GetFileSpec(nsFileSpec ** aFilePath)
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

NS_IMETHODIMP nsMailboxUrl::SetUri(const char * aURI)
{
  mURI= aURI;
  return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::GetUri(char ** aURI)
{
  // if we have been given a uri to associate with this url, then use it
  // otherwise try to reconstruct a URI on the fly....
  
  if (!mURI.IsEmpty())
    *aURI = ToNewCString(mURI);
  else
  {
    nsFileSpec * filePath = nsnull;
    GetFileSpec(&filePath);
    if (filePath)
    {
      nsCAutoString baseUri;
      // we blow off errors here so that we can open attachments
      // in .eml files.
      (void) MsgMailboxGetURI(m_file, baseUri);
      char * baseMessageURI;
      nsCreateLocalBaseMessageURI(baseUri.get(), &baseMessageURI);
      char * uri = nsnull;
      nsCAutoString uriStr;
      nsFileSpec folder = *filePath;
      nsBuildLocalMessageURI(baseMessageURI, m_messageKey, uriStr);
      nsCRT::free(baseMessageURI);
      uri = ToNewCString(uriStr);
      *aURI = uri;
    }
    else
      *aURI = nsnull;
  }
  
  return NS_OK;
}

nsresult nsMailboxUrl::GetMsgHdrForKey(nsMsgKey  msgKey, nsIMsgDBHdr ** aMsgHdr)
{
  nsresult rv = NS_OK;
  if (aMsgHdr && m_filePath)
  {
    nsCOMPtr<nsIMsgDatabase> mailDBFactory;
    nsCOMPtr<nsIMsgDatabase> mailDB;
    
    nsCOMPtr<nsIMsgDBService> msgDBService = do_GetService(NS_MSGDB_SERVICE_CONTRACTID, &rv);
    nsCOMPtr <nsIFileSpec> dbFileSpec;
    NS_NewFileSpecWithSpec(*m_filePath, getter_AddRefs(dbFileSpec));
    
    if (msgDBService)
      rv = msgDBService->OpenMailDBFromFileSpec(dbFileSpec, PR_FALSE, PR_FALSE, (nsIMsgDatabase **) getter_AddRefs(mailDB));
    if (NS_SUCCEEDED(rv) && mailDB) // did we get a db back?
      rv = mailDB->GetMsgHdrForKey(msgKey, aMsgHdr);
    else
    {
      if (!m_msgWindow)
      {
        nsCOMPtr<nsIMsgMailSession> mailSession = do_GetService(NS_MSGMAILSESSION_CONTRACTID, &rv);
        NS_ENSURE_SUCCESS(rv, rv);
        mailSession->GetTopmostMsgWindow(getter_AddRefs(m_msgWindow));
      }
      // maybe this is .eml file we're trying to read. See if we can get a header from the header sink.
      if (m_msgWindow)
      {
        nsCOMPtr <nsIMsgHeaderSink> headerSink;
        m_msgWindow->GetMsgHeaderSink(getter_AddRefs(headerSink));
        if (headerSink)
          return headerSink->GetDummyMsgHeader(aMsgHdr);
      }
    }
  }
  else
    rv = NS_ERROR_NULL_POINTER;
  
  return rv;
}

NS_IMETHODIMP nsMailboxUrl::GetMessageHeader(nsIMsgDBHdr ** aMsgHdr)
{
  return GetMsgHdrForKey(m_messageKey, aMsgHdr);
}

NS_IMPL_GETSET(nsMailboxUrl, AddDummyEnvelope, PRBool, m_addDummyEnvelope)
NS_IMPL_GETSET(nsMailboxUrl, CanonicalLineEnding, PRBool, m_canonicalLineEnding)

NS_IMETHODIMP
nsMailboxUrl::GetOriginalSpec(char **aSpec)
{
    if (!aSpec || !m_originalSpec) return NS_ERROR_NULL_POINTER;
    *aSpec = nsCRT::strdup(m_originalSpec);
    return NS_OK;
}

NS_IMETHODIMP
nsMailboxUrl::SetOriginalSpec(const char *aSpec)
{
    m_originalSpec.Adopt(aSpec ? nsCRT::strdup(aSpec) : 0);
    return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::SetMessageFile(nsIFileSpec * aFileSpec)
{
  m_messageFileSpec = aFileSpec;
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

NS_IMETHODIMP nsMailboxUrl::IsUrlType(PRUint32 type, PRBool *isType)
{
  NS_ENSURE_ARG(isType);
  
  switch(type)
  {
    case nsIMsgMailNewsUrl::eCopy:
      *isType = (m_mailboxAction == nsIMailboxUrl::ActionCopyMessage);
      break;
    case nsIMsgMailNewsUrl::eMove:
      *isType = (m_mailboxAction == nsIMailboxUrl::ActionMoveMessage);
      break;
    case nsIMsgMailNewsUrl::eDisplay:
      *isType = (m_mailboxAction == nsIMailboxUrl::ActionFetchMessage);
      break;
    default:
      *isType = PR_FALSE;
  };				
  
  return NS_OK;
  
}

////////////////////////////////////////////////////////////////////////////////////
// End nsIMailboxUrl specific support
////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// possible search part phrases include: MessageID=id&number=MessageKey

nsresult nsMailboxUrl::ParseSearchPart()
{
  nsCAutoString searchPart;
  nsresult rv = GetQuery(searchPart);
  // add code to this function to decompose everything past the '?'.....
  if (NS_SUCCEEDED(rv) && !searchPart.IsEmpty())
  {
    // the action for this mailbox must be a display message...
    char * msgPart = extractAttributeValue(searchPart.get(), "part=");
    if (msgPart)  // if we have a part in the url then we must be fetching just the part.
      m_mailboxAction = nsIMailboxUrl::ActionFetchPart;
    else
      m_mailboxAction = nsIMailboxUrl::ActionFetchMessage;
    
    char * messageKey = extractAttributeValue(searchPart.get(), "number=");
    m_messageID = extractAttributeValue(searchPart.get(),"messageid=");
    if (messageKey)
      m_messageKey = atol(messageKey); // convert to a long...
    
    PR_Free(msgPart);
    PR_Free(messageKey);
  }
  else
    m_mailboxAction = nsIMailboxUrl::ActionParseMailbox;
  
  return rv;
}

// warning: don't assume when parsing the url that the protocol part is "news"...
nsresult nsMailboxUrl::ParseUrl()
{
  delete m_filePath;
  GetFilePath(m_file);
  ParseSearchPart();
  // ### fix me.
  // this hack is to avoid asserting on every local message loaded because the security manager
  // is creating an empty "mailbox://" uri for every message.
  if (strlen(m_file) < 2)
    m_filePath = nsnull;
  else
    m_filePath = new nsFileSpec(nsFilePath(nsUnescape((char *) (const char *)m_file)));
  return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::SetSpec(const nsACString &aSpec)
{
  nsresult rv = nsMsgMailNewsUrl::SetSpec(aSpec);
  if (NS_SUCCEEDED(rv))
    rv = ParseUrl();
  return rv;
}

NS_IMETHODIMP nsMailboxUrl::SetQuery(const nsACString &aQuery)
{
  nsresult rv = nsMsgMailNewsUrl::SetQuery(aQuery);
  if (NS_SUCCEEDED(rv))
    rv = ParseUrl();
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

// nsIMsgI18NUrl support

nsresult nsMailboxUrl::GetFolder(nsIMsgFolder **msgFolder)
{
  // if we have a RDF URI, then try to get the folder for that URI and then ask the folder
  // for it's charset....

  nsXPIDLCString uri;
  GetUri(getter_Copies(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  nsCOMPtr<nsIMsgDBHdr> msg; 
  GetMsgDBHdrFromURI(uri, getter_AddRefs(msg));
  NS_ENSURE_TRUE(msg, NS_ERROR_FAILURE);
  nsresult rv = msg->GetFolder(msgFolder);
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ENSURE_TRUE(msgFolder, NS_ERROR_FAILURE);

  return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::GetFolderCharset(char ** aCharacterSet)
{
  nsCOMPtr<nsIMsgFolder> folder;
  nsresult rv = GetFolder(getter_AddRefs(folder));
#if 0
  if (NS_FAILED(rv))
  {
    nsCOMPtr<nsIPrefLocalizedString> pls;
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv,rv);
    rv = prefBranch->GetComplexValue("mailnews.view_default_charset",
      NS_GET_IID(nsIPrefLocalizedString), getter_AddRefs(pls));
    if (NS_SUCCEEDED(rv)) 
    {
      nsXPIDLString ucsval;
      pls->ToString(getter_Copies(ucsval));
      if (ucsval)
        *aCharacterSet = ToNewCString(ucsval);
      return rv;
    }
  }
#endif
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ENSURE_TRUE(folder, NS_ERROR_FAILURE);
  folder->GetCharset(aCharacterSet);
  return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::GetFolderCharsetOverride(PRBool * aCharacterSetOverride)
{
  nsCOMPtr<nsIMsgFolder> folder;
  nsresult rv = GetFolder(getter_AddRefs(folder));
  NS_ENSURE_SUCCESS(rv,rv);
  NS_ENSURE_TRUE(folder, NS_ERROR_FAILURE);
  folder->GetCharsetOverride(aCharacterSetOverride);

  return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::GetCharsetOverRide(char ** aCharacterSet)
{
  if (!mCharsetOverride.IsEmpty())
    *aCharacterSet = ToNewCString(mCharsetOverride); 
  else
    *aCharacterSet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::SetCharsetOverRide(const char * aCharacterSet)
{
  mCharsetOverride = aCharacterSet;
  return NS_OK;
}

/* void setMoveCopyMsgKeys (out nsMsgKey keysToFlag, in long numKeys); */
NS_IMETHODIMP nsMailboxUrl::SetMoveCopyMsgKeys(nsMsgKey *keysToFlag, PRInt32 numKeys)
{
  m_keys.RemoveAll();
  m_keys.Add(keysToFlag, numKeys);
  if (m_keys.GetSize() > 0 && m_messageKey == nsMsgKey_None)
    m_messageKey = m_keys.GetAt(0);
  return NS_OK;
}

NS_IMETHODIMP nsMailboxUrl::GetMoveCopyMsgHdrForIndex(PRUint32 msgIndex, nsIMsgDBHdr **msgHdr)
{
  NS_ENSURE_ARG(msgHdr);
  if (msgIndex < m_keys.GetSize())
  {
    nsMsgKey nextKey = m_keys.GetAt(msgIndex);
    return GetMsgHdrForKey(nextKey, msgHdr);
  }
  return NS_MSG_MESSAGE_NOT_FOUND;
}

NS_IMETHODIMP nsMailboxUrl::GetNumMoveCopyMsgs(PRUint32 *numMsgs)
{
  NS_ENSURE_ARG(numMsgs);
  *numMsgs = m_keys.GetSize();
  return NS_OK;
}


