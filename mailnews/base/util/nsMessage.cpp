/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsMessage.h"
#include "nsIMsgFolder.h"

// temp dependancy on RDF
#include "nsRDFCID.h"
#include "nsIRDFService.h"

static NS_DEFINE_CID(kRDFServiceCID, NS_RDFSERVICE_CID);

nsMessage::nsMessage(void)
  : nsRDFResource(),
    mFolder(nsnull),
    mMsgKey(0),
    mMsgKeyValid(PR_FALSE)
{

}

nsMessage::~nsMessage(void)
{
	//Member variables are either nsCOMPtr's or ptrs we don't want to own.
}

NS_IMPL_ADDREF_INHERITED(nsMessage, nsRDFResource)
NS_IMPL_RELEASE_INHERITED(nsMessage, nsRDFResource)
NS_IMPL_QUERY_INTERFACE_INHERITED2(nsMessage,
                                   nsRDFResource,
                                   nsIMessage,
                                   nsIDBMessage)

NS_IMETHODIMP
nsMessage::Init(const char* aURI)
{

	return nsRDFResource::Init(aURI);
}


NS_IMETHODIMP nsMessage::GetProperty(const char *propertyName, nsString &resultProperty)
{
	if(mMsgHdr)
		return mMsgHdr->GetProperty(propertyName, resultProperty);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetProperty(const char *propertyName, nsString &propertyStr)
{
	if(mMsgHdr)
		return mMsgHdr->SetProperty(propertyName, propertyStr);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetUint32Property(const char *propertyName, PRUint32 *pResult)
{
	if(mMsgHdr)
		return mMsgHdr->GetUint32Property(propertyName, pResult);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetUint32Property(const char *propertyName, PRUint32 propertyVal)
{
	if(mMsgHdr)
		return mMsgHdr->SetUint32Property(propertyName, propertyVal);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetNumReferences(PRUint16 *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetNumReferences(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetStringReference(PRInt32 refNum, nsCString &resultReference)
{
	if(mMsgHdr)
		return mMsgHdr->GetStringReference(refNum, resultReference);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetDate(PRTime *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetDate(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetDate(PRTime date)
{
	if(mMsgHdr)
		return mMsgHdr->SetDate(date);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetMessageId(const char *messageId)
{
	if(mMsgHdr)
		return mMsgHdr->SetMessageId(messageId);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetReferences(const char *references)
{
	if(mMsgHdr)
		return mMsgHdr->SetReferences(references);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetCcList(const char *ccList)
{
	if(mMsgHdr)
		return mMsgHdr->SetCcList(ccList);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetRecipients(const char *recipients)
{
	if(mMsgHdr)
		return mMsgHdr->SetRecipients(recipients);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetRecipientsIsNewsgroup(PRBool aIsNewsgroup)
{
	if(mMsgHdr)
    return mMsgHdr->SetRecipientsIsNewsgroup(aIsNewsgroup);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetRecipientsArray(const char *names, const char *addresses, PRUint32 numAddresses)
{
	if(mMsgHdr)
		return mMsgHdr->SetRecipientsArray(names, addresses, numAddresses);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetCCListArray(const char *names, const char *addresses, PRUint32 numAddresses)
{
	if(mMsgHdr)
		return mMsgHdr->SetCCListArray(names, addresses, numAddresses);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetAuthor(const char *author)
{
	if(mMsgHdr)
		return mMsgHdr->SetAuthor(author);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetSubject(const char *subject)
{
	if(mMsgHdr)
		return mMsgHdr->SetSubject(subject);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetStatusOffset(PRUint32 statusOffset)
{
	if(mMsgHdr)
		return mMsgHdr->SetStatusOffset(statusOffset);
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMessage::GetAuthor(char* *resultAuthor)
{
	if(mMsgHdr)
		return mMsgHdr->GetAuthor(resultAuthor);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetSubject(char* *resultSubject)
{
	if(mMsgHdr)
		return mMsgHdr->GetSubject(resultSubject);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetRecipients(char* *resultRecipients)
{
	if(mMsgHdr)
		return mMsgHdr->GetRecipients(resultRecipients);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetRecipientsIsNewsgroup(PRBool* aIsNewsgroup)
{
	if(mMsgHdr)
		return mMsgHdr->GetRecipientsIsNewsgroup(aIsNewsgroup);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetCcList(char **ccList)
{
	if(mMsgHdr)
		return mMsgHdr->GetCcList(ccList);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetMessageId(char **resultMessageId)
{
	if(mMsgHdr)
		return mMsgHdr->GetMessageId(resultMessageId);
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMessage::GetMime2DecodedAuthor(PRUnichar **resultAuthor)
{
	if(mMsgHdr)
		return mMsgHdr->GetMime2DecodedAuthor(resultAuthor);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetMime2DecodedSubject(PRUnichar* *resultSubject)
{
	if(mMsgHdr)
		return mMsgHdr->GetMime2DecodedSubject(resultSubject);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetMime2DecodedRecipients(PRUnichar* *resultRecipients)
{
	if(mMsgHdr)
		return mMsgHdr->GetMime2DecodedRecipients(resultRecipients);
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMessage::GetAuthorCollationKey(PRUnichar* *resultAuthor)
{
	if(mMsgHdr)
		return mMsgHdr->GetAuthorCollationKey(resultAuthor);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetSubjectCollationKey(PRUnichar* *resultSubject)
{
	if(mMsgHdr)
		return mMsgHdr->GetSubjectCollationKey(resultSubject);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetRecipientsCollationKey(PRUnichar* *resultRecipients)
{
	if(mMsgHdr)
		return mMsgHdr->GetRecipientsCollationKey(resultRecipients);
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMessage::GetFlags(PRUint32 *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetFlags(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetFlags(PRUint32 flags)
{
	if(mMsgHdr)
		return mMsgHdr->SetFlags(flags);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::OrFlags(PRUint32 flags, PRUint32 *result)
{
	if(mMsgHdr)
		return mMsgHdr->OrFlags(flags, result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::AndFlags(PRUint32 flags, PRUint32 *result)
{
	if(mMsgHdr)
		return mMsgHdr->AndFlags(flags, result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::MarkRead(PRBool bRead)
{
	if(mMsgHdr)
		return mMsgHdr->MarkRead(bRead);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::MarkFlagged(PRBool bFlagged)
{
	if(mMsgHdr)
		return mMsgHdr->MarkFlagged(bFlagged);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetMessageKey(nsMsgKey *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetMessageKey(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetThreadId(nsMsgKey *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetThreadId(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetThreadId(nsMsgKey inKey)
{
	if(mMsgHdr)
		return mMsgHdr->SetThreadId(inKey);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetMessageKey(nsMsgKey inKey)
{
	if(mMsgHdr)
		return mMsgHdr->SetMessageKey(inKey);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetMessageSize(PRUint32 *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetMessageSize(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetLineCount(PRUint32 *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetLineCount(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetMessageSize(PRUint32 messageSize)
{
	if(mMsgHdr)
		return mMsgHdr->SetMessageSize(messageSize);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetLineCount(PRUint32 lineCount)
{
	if(mMsgHdr)
		return mMsgHdr->SetLineCount(lineCount);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetPriority(nsMsgPriorityValue priority)
{
	if(mMsgHdr)
		return mMsgHdr->SetPriority(priority);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetPriorityString(const char *priority)
{
	if(mMsgHdr)
		return mMsgHdr->SetPriorityString(priority);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetPriority(nsMsgPriorityValue *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetPriority(result);
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMessage::GetMessageOffset(PRUint32 *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetMessageOffset(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetStatusOffset(PRUint32 *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetStatusOffset(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetCharset(char **aCharset)
{
	if(mMsgHdr)
		return mMsgHdr->GetCharset(aCharset);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetCharset(const char *aCharset)
{
	if(mMsgHdr)
		return mMsgHdr->SetCharset(aCharset);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetThreadParent(nsMsgKey *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetThreadParent(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetThreadParent(nsMsgKey inKey)
{
	if(mMsgHdr)
		return mMsgHdr->SetThreadParent(inKey);
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMessage::GetMsgFolder(nsIMsgFolder **aFolder)
{
  NS_ENSURE_ARG_POINTER(aFolder);

  nsresult rv;
	nsCOMPtr<nsIMsgFolder> folder = do_QueryReferent(mFolder, &rv);

  if (!folder) {
    // quick hack to determine the msgfolder
    nsCAutoString folderURI(mURI);

    // get rid of _message from schema (what a hack)
    PRInt32 messageStart = folderURI.Find("_message");
    if (messageStart)
      folderURI.Cut(messageStart, nsCRT::strlen("_message"));

    // chop # off the end
    PRInt32 folderEnd = folderURI.FindChar('#');
    if (folderEnd>0)
      folderURI.Truncate(folderEnd);

    // now go through RDF
    nsCOMPtr<nsIRDFService> rdfService =
      do_GetService(kRDFServiceCID, &rv);
    nsCOMPtr<nsIRDFResource> folderResource;
    rv = rdfService->GetResource((const char*)folderURI,
                                 getter_AddRefs(folderResource));
    NS_ENSURE_SUCCESS(rv, rv);
      
    folder = do_QueryInterface(folderResource, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    SetMsgFolder(folder);
  }
  
  *aFolder = folder;
  NS_IF_ADDREF(*aFolder);

  return NS_OK;
}


NS_IMETHODIMP nsMessage::SetMsgFolder(nsIMsgFolder *folder)
{
	mFolder = getter_AddRefs(NS_GetWeakReference(folder));
	return NS_OK;
}


NS_IMETHODIMP nsMessage::SetMsgDBHdr(nsIMsgDBHdr *hdr)
{
	mMsgHdr = dont_QueryInterface(hdr);
	return NS_OK;
}

NS_IMETHODIMP nsMessage::GetMsgDBHdr(nsIMsgDBHdr **hdr)
{
	*hdr = mMsgHdr;
	if(*hdr)
	{
		NS_ADDREF(*hdr);
		return NS_OK;
	}
	else
		return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP nsMessage::GetMsgKey(nsMsgKey *aMsgKey)
{
  nsCAutoString uriStr(mURI);
  if (mMsgKeyValid) {
    *aMsgKey = mMsgKey;
    return NS_OK;
  }
    

	PRInt32 keySeparator = uriStr.FindChar('#');
	if(keySeparator != -1)
	{
    PRInt32 keyEndSeparator = uriStr.FindCharInSet("?&", 
                                                   keySeparator); 
		nsCAutoString keyStr;
    if (keyEndSeparator != -1)
        uriStr.Mid(keyStr, keySeparator+1, 
                   keyEndSeparator-(keySeparator+1));
    else
        uriStr.Right(keyStr, uriStr.Length() - (keySeparator + 1));
    
		PRInt32 errorCode;
		mMsgKey = keyStr.ToInteger(&errorCode);

    if (NS_SUCCEEDED(errorCode))
      mMsgKeyValid=PR_TRUE;
    *aMsgKey = mMsgKey;
    return (nsresult)errorCode;
	}

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetMessageType(PRUint32 *aMessageType)
{
  NS_ENSURE_ARG_POINTER(aMessageType);
  *aMessageType = mMessageType;
  return NS_OK;
}

NS_IMETHODIMP nsMessage::SetMessageType(PRUint32 aMessageType)
{
  mMessageType = aMessageType;
  return NS_OK;
}
