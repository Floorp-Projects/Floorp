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

#ifndef _nsMsgHdr_H
#define _nsMsgHdr_H

#include "nsIMessage.h"
#include "nsRDFResource.h"
#include "nsString.h"
#include "MailNewsTypes.h"
#include "xp.h"
#include "mdb.h"

class nsMsgDatabase;
class nsString2;

class nsMsgHdr : public nsRDFResource, public nsIMessage {
public:

	friend class nsMsgDatabase;
    ////////////////////////////////////////////////////////////////////////////
    // nsIMessage methods:
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
	NS_IMETHOD GetMessageId(nsString &resultMessageId);

	NS_IMETHOD GetMime2EncodedAuthor(nsString &resultAuthor);
	NS_IMETHOD GetMime2EncodedSubject(nsString &resultSubject);
	NS_IMETHOD GetMime2EncodedRecipients(nsString &resultRecipients);

	NS_IMETHOD GetAuthorCollationKey(nsString &resultAuthor);
	NS_IMETHOD GetSubjectCollationKey(nsString &resultSubject);
	NS_IMETHOD GetRecipientsCollationKey(nsString &resultRecipients);

	NS_IMETHOD GetCCList(nsString &ccList);
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

    ////////////////////////////////////////////////////////////////////////////
    // nsMsgHdr methods:
    nsMsgHdr(nsMsgDatabase *db, nsIMdbRow *dbRow);
	nsMsgHdr();

    void		Init();
	void		Init(nsMsgDatabase *db, nsIMdbRow *dbRow);
    virtual		~nsMsgHdr();

    NS_DECL_ISUPPORTS_INHERITED

    nsIMdbRow		*GetMDBRow() {return m_mdbRow;}
protected:
    nsresult	SetStringColumn(const char *str, mdb_token token);
    nsresult	SetUInt32Column(PRUint32 value, mdb_token token);
    nsresult	GetUInt32Column(mdb_token token, PRUint32 *pvalue);

	// reference and threading stuff.
	const char*	GetNextReference(const char *startNextRef, nsString2 &reference);
	const char* GetPrevReference(const char *prevRef, nsString2 &reference);

    nsMsgKey	m_threadId; 
    nsMsgKey	m_messageKey; 	//news: article number, mail mbox offset, imap uid...
    time_t  		m_date;                         
    PRUint32		m_messageSize;	// lines for news articles, bytes for mail messages
    PRUint32		m_statusOffset;	// offset in a local mail message of the mozilla status hdr
    PRUint32		m_flags;
    PRUint16		m_numReferences;	// x-ref header for threading
    PRInt16			m_csID;			// cs id of message
	nsString		m_charSet;		// OK, charset of headers, since cs id's aren't supported.
    nsMsgPriority	m_priority;

    // nsMsgHdrs will have to know what db and row they belong to, since they are really
    // just a wrapper around the msg row in the mdb. This could cause problems,
    // though I hope not.
    nsMsgDatabase	*m_mdb;
    nsIMdbRow		*m_mdbRow;
};

#endif

