/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsMessage.h"

nsMessage::nsMessage(void)
  : nsRDFResource(), mFolder(nsnull),
    mMsgHdr(nsnull)
{


}

nsMessage::~nsMessage(void)
{
	NS_IF_RELEASE(mFolder);
	NS_IF_RELEASE(mMsgHdr);
}


NS_IMPL_ISUPPORTS_INHERITED(nsMessage, nsRDFResource, nsIMessage)

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

NS_IMETHODIMP nsMessage::GetStringReference(PRInt32 refNum, nsString2 &resultReference)
{
	if(mMsgHdr)
		return mMsgHdr->GetStringReference(refNum, resultReference);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetDate(time_t *result)
{
	if(mMsgHdr)
		return mMsgHdr->GetDate(result);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetDate(time_t date)
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

NS_IMETHODIMP nsMessage::SetCCList(const char *ccList)
{
	if(mMsgHdr)
		return mMsgHdr->SetCCList(ccList);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetRecipients(const char *recipients, PRBool recipientsIsNewsgroup)
{
	if(mMsgHdr)
		return mMsgHdr->SetRecipients(recipients, recipientsIsNewsgroup);
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


NS_IMETHODIMP nsMessage::GetAuthor(nsString &resultAuthor)
{
	if(mMsgHdr)
		return mMsgHdr->GetAuthor(resultAuthor);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetSubject(nsString &resultSubject)
{
	if(mMsgHdr)
		return mMsgHdr->GetSubject(resultSubject);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetRecipients(nsString &resultRecipients)
{
	if(mMsgHdr)
		return mMsgHdr->GetRecipients(resultRecipients);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetCCList(nsString &ccList)
{
	if(mMsgHdr)
		return mMsgHdr->GetCCList(ccList);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetMessageId(nsString &resultMessageId)
{
	if(mMsgHdr)
		return mMsgHdr->GetMessageId(resultMessageId);
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMessage::GetMime2EncodedAuthor(nsString &resultAuthor)
{
	if(mMsgHdr)
		return mMsgHdr->GetMime2EncodedAuthor(resultAuthor);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetMime2EncodedSubject(nsString &resultSubject)
{
	if(mMsgHdr)
		return mMsgHdr->GetMime2EncodedSubject(resultSubject);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetMime2EncodedRecipients(nsString &resultRecipients)
{
	if(mMsgHdr)
		return mMsgHdr->GetMime2EncodedRecipients(resultRecipients);
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMessage::GetAuthorCollationKey(nsString &resultAuthor)
{
	if(mMsgHdr)
		return mMsgHdr->GetAuthorCollationKey(resultAuthor);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetSubjectCollationKey(nsString &resultSubject)
{
	if(mMsgHdr)
		return mMsgHdr->GetSubjectCollationKey(resultSubject);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::GetRecipientsCollationKey(nsString &resultRecipients)
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

NS_IMETHODIMP nsMessage::SetPriority(nsMsgPriority priority)
{
	if(mMsgHdr)
		return mMsgHdr->SetPriority(priority);
	else
		return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsMessage::SetPriority(const char *priority)
{
	if(mMsgHdr)
		return mMsgHdr->SetPriority(priority);
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

NS_IMETHODIMP nsMessage::GetCharSet(nsString &result)
{
	if(mMsgHdr)
		return mMsgHdr->GetCharSet(result);
	else
		return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsMessage::GetMsgFolder(nsIMsgFolder **folder)
{
	if(!folder)
		return NS_ERROR_NULL_POINTER;
	*folder = mFolder;
	if(mFolder)
		NS_ADDREF(mFolder);
	return NS_OK;
}


NS_IMETHODIMP nsMessage::SetMsgFolder(nsIMsgFolder *folder)
{
	NS_IF_RELEASE(mFolder);
	mFolder = folder;
	if(mFolder)
		NS_ADDREF(mFolder);
	return NS_OK;
}


NS_IMETHODIMP nsMessage::SetMsgDBHdr(nsIMsgDBHdr *hdr)
{
	NS_IF_RELEASE(mMsgHdr);
	mMsgHdr = hdr;
	if(mMsgHdr)
		NS_ADDREF(mMsgHdr);
	return NS_OK;
}

NS_IMETHODIMP nsMessage::GetMsgDBHdr(nsIMsgDBHdr **hdr)
{
	*hdr = mMsgHdr;
	if(mMsgHdr)
		NS_ADDREF(mMsgHdr);
	return NS_OK;
}

