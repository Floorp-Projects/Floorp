/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifndef __nsMsgSearchTerm_h
#define __nsMsgSearchTerm_h
//---------------------------------------------------------------------------
// nsMsgSearchTerm specifies one criterion, e.g. name contains phil
//---------------------------------------------------------------------------

// perhaps this should go in its own header file, if this class gets
// its own cpp file, nsMsgSearchTerm.cpp
#include "nsIMsgSearchSession.h"
#include "nsIMsgSearchScopeTerm.h"
#include "nsIMsgSearchTerm.h"

#define EMPTY_MESSAGE_LINE(buf) (buf[0] == nsCRT::CR || buf[0] == nsCRT::LF || buf[0] == '\0')

class nsMsgSearchTerm : public nsIMsgSearchTerm
{
public:
	nsMsgSearchTerm();
#if 0
	nsMsgSearchTerm (nsMsgSearchAttribute, nsMsgSearchOperator, nsIMsgSearchValue *, PRBool, char * arbitraryHeader); // the bool is true if AND, PR_FALSE if OR
#endif
	nsMsgSearchTerm (nsMsgSearchAttribValue, nsMsgSearchOpValue, nsIMsgSearchValue *, nsMsgSearchBooleanOperator, const char * arbitraryHeader);

	virtual ~nsMsgSearchTerm ();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIMSGSEARCHTERM

    
	void StripQuotedPrintable (unsigned char*);
	PRInt32 GetNextIMAPOfflineMsgLine (char * buf, int bufferSize, int msgOffset, nsIMsgDBHdr * msg, nsIMsgDatabase * db);


    //	nsresult MatchBody (nsIMsgSearchScopeTerm*, PRUint32 offset, PRUint32 length, const char *charset, 
    //						nsIMsgDBHdr * msg, nsIMsgDatabase * db, PRBool *pResult);
    //	nsresult MatchArbitraryHeader (nsIMsgSearchScopeTerm *,
    //                                   PRUint32 offset,
    //                                   PRUint32 length,
    //                                   const char *charset,
    //                                   nsIMsgDBHdr * msg,
    //                                   nsIMsgDatabase *db,
    //                                   const char * headers, /* NULL terminated header list for msgs being filtered. Ignored unless ForFilters */
    //                                   PRUint32 headersSize, /* size of the NULL terminated list of headers */
    //                                   PRBool ForFilters /* true if we are filtering */,
    //								   PRBool *pResult);
	// nsresult MatchDate (PRTime, PRBool *result);
	// nsresult MatchStatus (PRUint32, PRBool *result);
	// nsresult MatchPriority (nsMsgPriorityValue, PRBool *result);
	// nsresult MatchSize (PRUint32, PRBool *result);
    //	nsresult MatchRfc822String(const char *, const char *charset, PRBool *pResult);
	// nsresult MatchAge (PRTime, PRBool *result);
    
	nsresult DeStream (char *, PRInt16 length);
	nsresult DeStreamNew (char *, PRInt16 length);

	nsresult GetLocalTimes (PRTime, PRTime, PRExplodedTime &, PRExplodedTime &);

	PRBool IsBooleanOpAND() { return m_booleanOp == nsMsgSearchBooleanOp::BooleanAND ? PR_TRUE : PR_FALSE;}
	nsMsgSearchBooleanOperator GetBooleanOp() {return m_booleanOp;}
	// maybe should return nsString &   ??
	const char * GetArbitraryHeader() {return m_arbitraryHeader.get();}

	static char *	EscapeQuotesInStr(const char *str);

	nsCOMPtr<nsIMsgHeaderParser> m_headerAddressParser;

	nsMsgSearchAttribValue m_attribute;
	nsMsgSearchOpValue m_operator;
	nsMsgSearchValue m_value;
	nsMsgSearchBooleanOperator m_booleanOp;  // boolean operator to be applied to this search term and the search term which precedes it.
	nsCString m_arbitraryHeader;         // user specified string for the name of the arbitrary header to be used in the search
									  // only has a value when m_attribute = attribOtherHeader!!!!
protected:
	nsresult MatchString (const char *stringToMatch, const char *charset,
                          PRBool *pResult);
	nsresult		OutputValue(nsCString &outputStr);
	nsMsgSearchAttribValue ParseAttribute(char *inStream);
	nsMsgSearchOpValue	ParseOperator(char *inStream);
	nsresult		ParseValue(char *inStream);
	nsresult		InitHeaderAddressParser();

};

#endif
