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
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef _nsMsgSearchNews_h__
#include "nsMsgSearchAdapter.h"

//-----------------------------------------------------------------------------
//---------- Adapter class for searching online (news) folders ----------------
//-----------------------------------------------------------------------------

class nsMsgSearchNews : public nsMsgSearchAdapter
{
public:
	nsMsgSearchNews (nsMsgSearchScopeTerm *scope, nsMsgSearchTermArray &termList);
	virtual ~nsMsgSearchNews ();

	NS_IMETHOD ValidateTerms ();
	NS_IMETHOD Search ();
	NS_IMETHOD GetEncoding (char **result);

	virtual nsresult Encode (nsCString *outEncoding);
	virtual char *EncodeTerm (nsMsgSearchTerm *);
	char *EncodeValue (const char *);
	
	nsresult AddResultElement (nsIMsgDBHdr *);
    PRBool DuplicateHit(PRUint32 artNum) ;
    void CollateHits ();
    static int CompareArticleNumbers (const void *v1, const void *v2, void *data);
    void ReportHit (nsIMsgDBHdr *pHeaders, const char *location);

protected:
	nsCString m_encoding;
	PRBool m_ORSearch; // set to true if any of the search terms contains an OR for a boolean operator. 

	nsMsgKeyArray m_candidateHits;
	nsMsgKeyArray m_hits;

	static const char *m_kNntpFrom;
	static const char *m_kNntpSubject;
	static const char *m_kTermSeparator;
	static const char *m_kUrlPrefix;
};



class nsMsgSearchNewsEx : public nsMsgSearchNews
{
public:
	nsMsgSearchNewsEx (nsMsgSearchScopeTerm *scope, nsMsgSearchTermArray &termList);
	virtual ~nsMsgSearchNewsEx ();

	NS_IMETHOD ValidateTerms ();
	NS_IMETHOD Search ();
	virtual nsresult Encode (nsCString *pEncoding /*out*/);

	nsresult SaveProfile (const char *profileName);

protected:
	static const char *m_kSearchTemplate;
	static const char *m_kProfileTemplate;
};



#endif

