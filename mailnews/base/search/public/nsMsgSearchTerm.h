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


#ifndef __nsMsgSearchTerm_h
#define __nsMsgSearchTerm_h
//---------------------------------------------------------------------------
// nsMsgSearchTerm specifies one criterion, e.g. name contains phil
//---------------------------------------------------------------------------

// perhaps this should go in its own header file, if this class gets
// its own cpp file, nsMsgSearchTerm.cpp
#include "nsIMsgSearchSession.h"
#include "nsMsgSearchScopeTerm.h"

class nsMsgSearchTerm
{
public:
	nsMsgSearchTerm();
#if 0
	nsMsgSearchTerm (nsMsgSearchAttribute, nsMsgSearchOperator, nsMsgSearchValue *, PRBool, char * arbitraryHeader); // the bool is true if AND, PR_FALSE if OR
#endif
	nsMsgSearchTerm (nsMsgSearchAttribute, nsMsgSearchOperator, nsMsgSearchValue *, nsMsgSearchBooleanOperator, char * arbitraryHeader);

	virtual ~nsMsgSearchTerm ();

	void StripQuotedPrintable (unsigned char*);
	PRInt32 GetNextIMAPOfflineMsgLine (char * buf, int bufferSize, int msgOffset, nsIMessage * msg, nsIMsgDatabase * db);


	nsresult MatchBody (nsMsgSearchScopeTerm*, PRUint32 offset, PRUint32 length, const char *charset, 
						nsIMsgDBHdr * msg, nsIMsgDatabase * db, PRBool *pResult);
	nsresult MatchArbitraryHeader (nsMsgSearchScopeTerm *,
                                   PRUint32 offset,
                                   PRUint32 length,
                                   const char *charset,
                                   nsIMsgDBHdr * msg,
                                   nsIMsgDatabase *db,
                                   const char * headers, /* NULL terminated header list for msgs being filtered. Ignored unless ForFilters */
                                   PRUint32 headersSize, /* size of the NULL terminated list of headers */
                                   PRBool ForFilters /* true if we are filtering */,
								   PRBool *pResult);
	nsresult MatchString (nsCString *, const char *charset, PRBool body, PRBool *result);
	nsresult MatchDate (PRTime, PRBool *result);
	nsresult MatchStatus (PRUint32, PRBool *result);
	nsresult MatchPriority (nsMsgPriority, PRBool *result);
	nsresult MatchSize (PRUint32, PRBool *result);
	nsresult MatchRfc822String(const char *, const char *charset, PRBool *pResult);
	nsresult MatchAge (PRTime, PRBool *result);

	nsresult EnStreamNew (nsCString &stream);
	nsresult DeStream (char *, PRInt16 length);
	nsresult DeStreamNew (char *, PRInt16 length);

	nsresult GetLocalTimes (PRTime, PRTime, PRExplodedTime &, PRExplodedTime &);

	PRBool IsBooleanOpAND() { return m_booleanOp == nsMsgSearchBooleanOp::BooleanAND ? PR_TRUE : PR_FALSE;}
	nsMsgSearchBooleanOperator GetBooleanOp() {return m_booleanOp;}
	// maybe should return nsString &   ??
	const char * GetArbitraryHeader() {return m_arbitraryHeader.GetBuffer();}

	static char *	EscapeQuotesInStr(const char *str);
	PRBool MatchAllBeforeDeciding ();

	nsCOMPtr<nsIMsgHeaderParser> m_headerAddressParser;

	nsMsgSearchAttribute m_attribute;
	nsMsgSearchOperator m_operator;
	nsMsgSearchValue m_value;
	nsMsgSearchBooleanOperator m_booleanOp;  // boolean operator to be applied to this search term and the search term which precedes it.
	nsCString m_arbitraryHeader;         // user specified string for the name of the arbitrary header to be used in the search
									  // only has a value when m_attribute = attribOtherHeader!!!!
protected:
	nsresult		OutputValue(nsCString &outputStr);
	nsMsgSearchAttribute ParseAttribute(char *inStream);
	nsMsgSearchOperator	ParseOperator(char *inStream);
	nsresult		ParseValue(char *inStream);
	nsresult		InitHeaderAddressParser();

};

#endif
