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
				nsMsgHdr();
	virtual		~nsMsgHdr();
	nsrefcnt	AddRef(void);                                       
    nsrefcnt	Release(void);   
	nsresult	GetProperty(const char *propertyName, nsString &resultProperty);
	uint16		GetNumReferences();
	nsresult	GetStringReference(PRInt32 refNum, nsString &resultReference);
	time_t		GetDate() {return m_date;}

	MessageKey	GetArticleNum();
	MessageKey  GetMessageKey();
	MessageKey	GetThreadId();

			// this is almost always the m_messageKey, except for the first message.
			// NeoAccess doesn't allow fID's of 0.
			virtual PRUint32 GetMessageOffset() {return m_messageKey;}
protected:
	nsrefcnt mRefCnt;                                                         

	MessageKey	m_threadId; 
	MessageKey	m_messageKey; 	//news: article number, mail mbox offset, imap uid...
	time_t 		m_date;                         
	PRUint32		m_messageSize;	// lines for news articles, bytes for mail messages
	PRUint32		m_flags;
	PRUint16		m_numReferences;	// x-ref header for threading

  // nsMsgHdrs will have to know what db and row they belong to, since they are really
// just a wrapper around the msg row in the mdb. This could cause problems,
// though I hope not.
	nsMsgDatabase		*m_mdb;
	mdbRow				*m_mdbRow;
};

#endif

