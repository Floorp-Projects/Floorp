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

#ifndef _nsMsgLocalSearch_H
#define _nsMsgLocalSearch_H

// inherit interface here
#include "nsIMsgSearchAdapter.h"

// inherit base implementation
#include "nsMsgSearchAdapter.h"
class nsIMsgDBHdr;
class nsMsgMailboxParser;

class nsMsgSearchOfflineMail : public nsMsgSearchAdapter
{
public:
	nsMsgSearchOfflineMail (nsMsgSearchScopeTerm*, nsMsgSearchTermArray&);
	virtual ~nsMsgSearchOfflineMail ();

	NS_IMETHOD ValidateTerms ();
	NS_IMETHOD Search ();
	NS_IMETHOD Abort ();
	static nsresult  MatchTermsForFilter(nsIMsgDBHdr * msgToMatch,
                                         nsMsgSearchTermArray &termList,
                                         nsMsgSearchScopeTerm *scope, 
                                         nsIMsgDatabase * db, 
                                         const char * headers,
                                         PRUint32 headerSize,
										 PRBool *pResult);

	static nsresult MatchTermsForSearch(nsIMsgDBHdr * msgTomatch, nsMsgSearchTermArray & termList, nsMsgSearchScopeTerm *scope,
                                                nsIMsgDatabase *db, PRBool *pResult);

	virtual nsresult BuildSummaryFile ();
	virtual nsresult OpenSummaryFile ();
	nsresult SummaryFileError();

	nsresult AddResultElement (nsIMsgDBHdr *);


protected:
	static	nsresult MatchTerms(nsIMsgDBHdr *msgToMatch,
                                nsMsgSearchTermArray &termList,
                                nsMsgSearchScopeTerm *scope, 
                                nsIMsgDatabase * db, 
                                const char * headers,
                                PRUint32 headerSize,
                                PRBool ForFilters,
								PRBool *pResult);
	struct ListContext *m_cursor;
	nsIMsgDatabase *m_db;
	struct ListContext *m_listContext;

	enum
	{
		kOpenFolderState,
		kParseMoreState,
		kCloseFolderState,
		kDoneState
	};
	int m_parserState;
	nsMsgMailboxParser *m_mailboxParser;
	void CleanUpScope();
};

class nsMsgSearchIMAPOfflineMail : public nsMsgSearchOfflineMail
{
public:
	nsMsgSearchIMAPOfflineMail (nsMsgSearchScopeTerm*, nsMsgSearchTermArray&);
	virtual ~nsMsgSearchIMAPOfflineMail ();

	NS_IMETHOD ValidateTerms ();
};



class nsMsgSearchOfflineNews : public nsMsgSearchOfflineMail
{
public:
	nsMsgSearchOfflineNews (nsMsgSearchScopeTerm*, nsMsgSearchTermArray&);
	virtual ~nsMsgSearchOfflineNews ();
	NS_IMETHOD ValidateTerms ();

	virtual nsresult OpenSummaryFile ();
};



#endif

