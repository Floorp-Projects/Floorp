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

#ifndef _nsMsgHdr_H
#define _nsMsgHdr_H

#include "nsIMsgHdr.h"
#include "nsString.h"
#include "MailNewsTypes.h"
#include "mdb.h"

class nsMsgDatabase;
class nsCString;

class nsMsgHdr : public nsIMsgDBHdr {
public:
    NS_DECL_NSIMSGHDR
	friend class nsMsgDatabase;
    ////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////////////////////////////
    // nsMsgHdr methods:
    nsMsgHdr(nsMsgDatabase *db, nsIMdbRow *dbRow);
    virtual ~nsMsgHdr();

    virtual nsresult    GetRawFlags(PRUint32 *result);
    void                Init();
    virtual nsresult    InitCachedValues();
    virtual nsresult    InitFlags();

    NS_DECL_ISUPPORTS

    nsIMdbRow   *GetMDBRow() {return m_mdbRow;}
    PRBool      IsParentOf(nsIMsgDBHdr *possibleChild);
    PRBool      IsAncestorOf(nsIMsgDBHdr *possibleChild);
protected:
    nsresult	SetStringColumn(const char *str, mdb_token token);
    nsresult	SetUInt32Column(PRUint32 value, mdb_token token);
    nsresult	GetUInt32Column(mdb_token token, PRUint32 *pvalue, PRUint32 defaultValue = 0);
    nsresult    BuildRecipientsFromArray(const char *names, const char *addresses, PRUint32 numAddresses, nsCAutoString& allRecipients);

    // reference and threading stuff.
    nsresult	ParseReferences(const char *references);
    const char*	GetNextReference(const char *startNextRef, nsCString &reference);
    const char* GetPrevReference(const char *prevRef, nsCString &reference);

    nsMsgKey	m_threadId; 
    nsMsgKey	m_messageKey; 	//news: article number, mail mbox offset, imap uid...
    nsMsgKey	m_threadParent;	// message this is a reply to, in thread.
    PRTime      m_date;                         
    PRUint32    m_messageSize;	// lines for news articles, bytes for mail messages
    PRUint32    m_statusOffset;	// offset in a local mail message of the mozilla status hdr
    PRUint32    m_flags;
    PRUint16    m_numReferences;	// x-ref header for threading
    nsCStringArray      m_references;  // avoid parsing references every time we want one
    nsMsgPriorityValue  m_priority;

    // nsMsgHdrs will have to know what db and row they belong to, since they are really
    // just a wrapper around the msg row in the mdb. This could cause problems,
    // though I hope not.
    nsMsgDatabase *m_mdb;
    nsIMdbRow     *m_mdbRow;
    PRUint32      m_initedValues;
};

#endif

