/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsString.h"
#include "MailNewsTypes.h"
#include "xp.h"
#include "mdb.h"

class nsMsgDatabase;

// this could inherit from nsISupports mainly to get ref-counting semantics
// but I don't intend it to be a public interface. I'll just
// declare AddRef and Release as methods..

class nsMsgHdr
{
public:
//				nsMsgHdr();
				nsMsgHdr(nsMsgDatabase *db, mdbRow *dbRow);
	void		Init();

	virtual		~nsMsgHdr();
	nsrefcnt	AddRef(void);                                       
    nsrefcnt	Release(void);   
	nsresult	GetProperty(const char *propertyName, nsString &resultProperty);
	nsresult	SetProperty(const char *propertyName, nsString &propertyStr);
	nsresult	GetUint32Property(const char *propertyName, PRUint32 *pResult);
	nsresult	SetUint32Property(const char *propertyName, PRUint32 propertyVal);
	uint16		GetNumReferences();
	nsresult	GetStringReference(PRInt32 refNum, nsString &resultReference);
	time_t		GetDate();
	nsresult	SetDate(time_t date);
	nsresult	SetMessageId(const char *messageId);
	nsresult	SetReferences(const char *references);
	nsresult	SetCCList(const char *ccList);
	// rfc822 is false when recipients is a newsgroup list
	nsresult	SetRecipients(const char *recipients, PRBool rfc822 = PR_TRUE);
	nsresult	SetAuthor(const char *author);
	nsresult	SetSubject(const char *subject);

	nsresult	SetStatusOffset(PRUint32 statusOffset);

			// flag handling routines
	virtual PRUint32 GetFlags() {return m_flags;}
	void		SetFlags(PRUint32 flags);
	PRUint32	OrFlags(PRUint32 flags);
	PRUint32	AndFlags(PRUint32 flags);
	PRUint32	GetMozillaStatusFlags();

	MessageKey  GetMessageKey();
	MessageKey	GetThreadId();
	void		SetMessageKey(MessageKey inKey) {m_messageKey = inKey;}
	virtual	PRUint32 GetMessageSize() {return m_messageSize;}
	void		SetMessageSize(PRUint32 messageSize);
	void		SetLineCount(PRUint32 lineCount);
	void		SetPriority(MSG_PRIORITY priority) { m_priority = priority;}
	void		SetPriority(const char *priority);
			// this is almost always the m_messageKey, except for the first message.
			// NeoAccess doesn't allow fID's of 0.
			virtual PRUint32 GetMessageOffset() {return m_messageKey;}
			virtual PRUint32 GetStatusOffset(); 

			mdbRow		*GetMDBRow() {return m_mdbRow;}
protected:
	nsresult	SetStringColumn(const char *str, mdb_token token);
	nsresult	SetUInt32Column(PRUint32 value, mdb_token token);

	nsrefcnt mRefCnt;                                                         

	MessageKey	m_threadId; 
	MessageKey	m_messageKey; 	//news: article number, mail mbox offset, imap uid...
	time_t 		m_date;                         
	PRUint32		m_messageSize;	// lines for news articles, bytes for mail messages
	PRUint32		m_statusOffset;	// offset in a local mail message of the mozilla status hdr
	PRUint32		m_flags;
	PRUint16		m_numReferences;	// x-ref header for threading
	MSG_PRIORITY	m_priority;

  // nsMsgHdrs will have to know what db and row they belong to, since they are really
// just a wrapper around the msg row in the mdb. This could cause problems,
// though I hope not.
	nsMsgDatabase		*m_mdb;
	mdbRow				*m_mdbRow;
};

#endif

