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

/********************************************************************************************************
 
   Interface for representing Messenger folders.
 
*********************************************************************************************************/

#ifndef nsMessage_h__
#define nsMessage_h__

#include "msgCore.h"
#include "nsIMessage.h" /* include the interface we are going to support */
#include "nsRDFResource.h"
#include "nsIMsgHdr.h"
#include "nsCOMPtr.h"


class NS_MSG_BASE nsMessage: public nsRDFResource, public nsIDBMessage
{
public: 
	nsMessage(void);
	virtual ~nsMessage(void);

	NS_DECL_ISUPPORTS_INHERITED

	//nsIMsgHdr
	NS_IMETHOD GetProperty(const char *propertyName, nsString &resultProperty);
    NS_IMETHOD SetProperty(const char *propertyName, nsString &propertyStr);
    NS_IMETHOD GetUint32Property(const char *propertyName, PRUint32 *pResult);
    NS_IMETHOD SetUint32Property(const char *propertyName, PRUint32 propertyVal);
    NS_IMETHOD GetNumReferences(PRUint16 *result);
    NS_IMETHOD GetStringReference(PRInt32 refNum, nsCString &resultReference);
    NS_IMETHOD GetDate(PRTime *result);
    NS_IMETHOD SetDate(PRTime date);
    NS_IMETHOD SetMessageId(const char *messageId);
    NS_IMETHOD SetReferences(const char *references);
    NS_IMETHOD SetCCList(const char *ccList);
    NS_IMETHOD SetRecipients(const char *recipients, PRBool recipientsIsNewsgroup);
	NS_IMETHOD SetRecipientsArray(const char *names, const char *addresses, PRUint32 numAddresses);
    NS_IMETHOD SetCCListArray(const char *names, const char *addresses, PRUint32 numAddresses);
    NS_IMETHOD SetAuthor(const char *author);
    NS_IMETHOD SetSubject(const char *subject);
    NS_IMETHOD SetStatusOffset(PRUint32 statusOffset);

	NS_IMETHOD GetAuthor(nsString *resultAuthor);
	NS_IMETHOD GetSubject(nsString *resultSubject);
	NS_IMETHOD GetRecipients(nsString *resultRecipients);
	NS_IMETHOD GetCCList(nsString *ccList);
	NS_IMETHOD GetMessageId(nsCString *resultMessageId);

	NS_IMETHOD GetMime2DecodedAuthor(nsString *resultAuthor);
	NS_IMETHOD GetMime2DecodedSubject(nsString *resultSubject);
	NS_IMETHOD GetMime2DecodedRecipients(nsString *resultRecipients);

	NS_IMETHOD GetAuthorCollationKey(nsString *resultAuthor);
	NS_IMETHOD GetSubjectCollationKey(nsString *resultSubject);
	NS_IMETHOD GetRecipientsCollationKey(nsString *resultRecipients);

    // flag handling routines
    NS_IMETHOD GetFlags(PRUint32 *result);
    NS_IMETHOD SetFlags(PRUint32 flags);
    NS_IMETHOD OrFlags(PRUint32 flags, PRUint32 *result);
    NS_IMETHOD AndFlags(PRUint32 flags, PRUint32 *result);

	// Mark message routines
	NS_IMETHOD MarkRead(PRBool bRead);

    NS_IMETHOD GetMessageKey(nsMsgKey *result);
    NS_IMETHOD GetThreadId(nsMsgKey *result);
    NS_IMETHOD SetThreadId(nsMsgKey inKey);
    NS_IMETHOD SetMessageKey(nsMsgKey inKey);
    NS_IMETHOD GetMessageSize(PRUint32 *result);
    NS_IMETHOD SetMessageSize(PRUint32 messageSize);
    NS_IMETHOD GetLineCount(PRUint32 *result);
    NS_IMETHOD SetLineCount(PRUint32 lineCount);
    NS_IMETHOD SetPriority(nsMsgPriority priority);
    NS_IMETHOD SetPriority(const char *priority);
    NS_IMETHOD GetMessageOffset(PRUint32 *result);
    NS_IMETHOD GetStatusOffset(PRUint32 *result); 
	NS_IMETHOD GetCharSet(nsString *result);
    NS_IMETHOD GetPriority(nsMsgPriority *result);
    NS_IMETHOD GetThreadParent(nsMsgKey *result);
    NS_IMETHOD SetThreadParent(nsMsgKey inKey);

	//nsIMessage
	NS_IMETHOD GetMsgFolder(nsIMsgFolder **folder);
	NS_IMETHOD SetMsgFolder(nsIMsgFolder *folder);

	NS_IMETHOD SetMsgDBHdr(nsIMsgDBHdr *hdr);
	NS_IMETHOD GetMsgDBHdr(nsIMsgDBHdr **hdr);

protected:
	nsIMsgFolder *mFolder;
	nsCOMPtr<nsIMsgDBHdr> mMsgHdr;

};

#endif //nsMessage_h__

