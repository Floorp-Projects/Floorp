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


class nsMessage: public nsRDFResource, public nsIMessage
{
public: 
	nsMessage(void);
	virtual ~nsMessage(void);

	NS_DECL_ISUPPORTS_INHERITED

	//nsIMessageHdr
	NS_IMETHOD GetProperty(const char *propertyName, nsString &resultProperty);
    NS_IMETHOD SetProperty(const char *propertyName, nsString &propertyStr);
    NS_IMETHOD GetUint32Property(const char *propertyName, PRUint32 *pResult);
    NS_IMETHOD SetUint32Property(const char *propertyName, PRUint32 propertyVal);
    NS_IMETHOD GetNumReferences(PRUint16 *result);
    NS_IMETHOD GetStringReference(PRInt32 refNum, nsString2 &resultReference);
    NS_IMETHOD GetDate(time_t *result);
    NS_IMETHOD SetDate(time_t date);
    NS_IMETHOD SetMessageId(const char *messageId);
    NS_IMETHOD SetReferences(const char *references);
    NS_IMETHOD SetCCList(const char *ccList);
    NS_IMETHOD SetRecipients(const char *recipients, PRBool recipientsIsNewsgroup);
	NS_IMETHOD SetRecipientsArray(const char *names, const char *addresses, PRUint32 numAddresses);
    NS_IMETHOD SetCCListArray(const char *names, const char *addresses, PRUint32 numAddresses);
    NS_IMETHOD SetAuthor(const char *author);
    NS_IMETHOD SetSubject(const char *subject);
    NS_IMETHOD SetStatusOffset(PRUint32 statusOffset);

	NS_IMETHOD GetAuthor(nsString &resultAuthor);
	NS_IMETHOD GetSubject(nsString &resultSubject);
	NS_IMETHOD GetRecipients(nsString &resultRecipients);
	NS_IMETHOD GetCCList(nsString &ccList);
	NS_IMETHOD GetMessageId(nsString &resultMessageId);

	NS_IMETHOD GetMime2EncodedAuthor(nsString &resultAuthor);
	NS_IMETHOD GetMime2EncodedSubject(nsString &resultSubject);
	NS_IMETHOD GetMime2EncodedRecipients(nsString &resultRecipients);

	NS_IMETHOD GetAuthorCollationKey(nsString &resultAuthor);
	NS_IMETHOD GetSubjectCollationKey(nsString &resultSubject);
	NS_IMETHOD GetRecipientsCollationKey(nsString &resultRecipients);

    // flag handling routines
    NS_IMETHOD GetFlags(PRUint32 *result);
    NS_IMETHOD SetFlags(PRUint32 flags);
    NS_IMETHOD OrFlags(PRUint32 flags, PRUint32 *result);
    NS_IMETHOD AndFlags(PRUint32 flags, PRUint32 *result);

    NS_IMETHOD GetMessageKey(nsMsgKey *result);
    NS_IMETHOD GetThreadId(nsMsgKey *result);
    NS_IMETHOD SetThreadId(nsMsgKey inKey);
    NS_IMETHOD SetMessageKey(nsMsgKey inKey);
    NS_IMETHOD GetMessageSize(PRUint32 *result);
    NS_IMETHOD SetMessageSize(PRUint32 messageSize);
    NS_IMETHOD SetLineCount(PRUint32 lineCount);
    NS_IMETHOD SetPriority(nsMsgPriority priority);
    NS_IMETHOD SetPriority(const char *priority);
    NS_IMETHOD GetMessageOffset(PRUint32 *result);
    NS_IMETHOD GetStatusOffset(PRUint32 *result); 
	NS_IMETHOD GetCharSet(nsString &result);

	//nsIMessage
	NS_IMETHOD GetMsgFolder(nsIMsgFolder **folder);

	//other useful methods
	NS_IMETHOD SetMsgFolder(nsIMsgFolder *folder);

	NS_IMETHOD SetMsgDBHdr(nsIMsgDBHdr *hdr);
	NS_IMETHOD GetMsgDBHdr(nsIMsgDBHdr **hdr);

protected:
	nsIMsgFolder *mFolder;
	nsIMsgDBHdr *mMsgHdr;

};

#endif //nsMessage_h__