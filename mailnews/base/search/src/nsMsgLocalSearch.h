/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#ifndef _nsMsgLocalSearch_H
#define _nsMsgLocalSearch_H

// inherit interface here
#include "nsIMsgSearchAdapter.h"

// inherit base implementation
#include "nsMsgSearchAdapter.h"
#include "nsISimpleEnumerator.h"

class nsIMsgDBHdr;
class nsMsgMailboxParser;
class nsIMsgSearchScopeTerm;
class nsIMsgFolder;

class nsMsgSearchOfflineMail : public nsMsgSearchAdapter, public nsIUrlListener
{
public:
	nsMsgSearchOfflineMail (nsIMsgSearchScopeTerm*, nsISupportsArray *);
	virtual ~nsMsgSearchOfflineMail ();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIURLLISTENER

  NS_IMETHOD ValidateTerms ();
  NS_IMETHOD Search (PRBool *aDone);
  NS_IMETHOD Abort ();
  NS_IMETHOD AddResultElement (nsIMsgDBHdr *);

	static nsresult  MatchTermsForFilter(nsIMsgDBHdr * msgToMatch,
                                         nsISupportsArray *termList,
                                         const char *defaultCharset,
                                         nsIMsgSearchScopeTerm *scope, 
                                         nsIMsgDatabase * db, 
                                         const char * headers,
                                         PRUint32 headerSize,
										 PRBool *pResult);

  static nsresult MatchTermsForSearch(nsIMsgDBHdr * msgTomatch,
                                      nsISupportsArray * termList,
                                      const char *defaultCharset,
                                      nsIMsgSearchScopeTerm *scope,
                                      nsIMsgDatabase *db, PRBool *pResult);

	virtual nsresult BuildSummaryFile ();
	virtual nsresult OpenSummaryFile ();
	nsresult SummaryFileError();

protected:
	static	nsresult MatchTerms(nsIMsgDBHdr *msgToMatch,
                                nsISupportsArray *termList,
                                const char *defaultCharset,
                                nsIMsgSearchScopeTerm *scope, 
                                nsIMsgDatabase * db, 
                                const char * headers,
                                PRUint32 headerSize,
                                PRBool ForFilters,
								PRBool *pResult);
	nsIMsgDatabase *m_db;
	nsCOMPtr<nsISimpleEnumerator> m_listContext;

	void CleanUpScope();
};

class nsMsgSearchIMAPOfflineMail : public nsMsgSearchOfflineMail
{
public:
	nsMsgSearchIMAPOfflineMail (nsIMsgSearchScopeTerm*, nsISupportsArray *);
	virtual ~nsMsgSearchIMAPOfflineMail ();

	NS_IMETHOD ValidateTerms ();
};



class nsMsgSearchOfflineNews : public nsMsgSearchOfflineMail
{
public:
	nsMsgSearchOfflineNews (nsIMsgSearchScopeTerm*, nsISupportsArray *);
	virtual ~nsMsgSearchOfflineNews ();
	NS_IMETHOD ValidateTerms ();

	virtual nsresult OpenSummaryFile ();
};



#endif

