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

#ifndef _nsMsgSearchAdapter_H_
#define _nsMsgSearchAdapter_H_

#include "nsMsgSearchCore.h"
#include "nsIMsgSearchAdapter.h"
#include "nsMsgSearchArray.h"

//-----------------------------------------------------------------------------
// These Adapter classes contain the smarts to convert search criteria from 
// the canonical structures in msg_srch.h into whatever format is required
// by their protocol. 
//
// There is a separate Adapter class for area (pop, imap, nntp, ldap) to contain
// the special smarts for that protocol.
//-----------------------------------------------------------------------------

class nsMsgSearchAdapter : public nsIMsgSearchAdapter
{
public:
	nsMsgSearchAdapter (nsMsgSearchScopeTerm*, nsMsgSearchTermArray&);
	virtual ~nsMsgSearchAdapter ();

	NS_IMETHOD ValidateTerms ();
	NS_IMETHOD Search () { return NS_OK; }
	NS_IMETHOD SendUrl () { return NS_OK; }
	NS_IMETHOD OpenResultElement (nsMsgResultElement *);
	NS_IMETHOD ModifyResultElement (nsMsgResultElement*, nsMsgSearchValue*);
	NS_IMETHOD GetEncoding (char **encoding) { return NS_OK; }

	NS_IMETHOD FindTargetFolder (const nsMsgResultElement*, nsIMsgFolder **aFolder);
	NS_IMETHOD Abort ();

	nsMsgSearchScopeTerm		*m_scope;
	nsMsgSearchTermArray &m_searchTerms;

	PRBool m_abortCalled;

	static nsresult EncodeImap (char **ppEncoding, 
									   nsMsgSearchTermArray &searchTerms,  
									   PRInt16 src_csid, 
									   PRInt16 dest_csid,
									   PRBool reallyDredd = PR_FALSE);
	
	static nsresult EncodeImapValue(char *encoding, const char *value, PRBool useQuotes, PRBool reallyDredd);

	static char *TryToConvertCharset(char *sourceStr, PRInt16 src_csid, PRInt16 dest_csid, PRBool useMIME2Style);
	static char *GetImapCharsetParam(PRInt16 dest_csid);
	void GetSearchCSIDs(PRInt16& src_csid, PRInt16& dst_csid);

	// This stuff lives in the base class because the IMAP search syntax 
	// is used by the Dredd SEARCH command as well as IMAP itself
	static const char *m_kImapBefore;
	static const char *m_kImapBody;
	static const char *m_kImapCC;
	static const char *m_kImapFrom;
	static const char *m_kImapNot;
	static const char *m_kImapOr;
	static const char *m_kImapSince;
	static const char *m_kImapSubject;
	static const char *m_kImapTo;
	static const char *m_kImapHeader;
	static const char *m_kImapAnyText;
	static const char *m_kImapKeyword;
	static const char *m_kNntpKeywords;
	static const char *m_kImapSentOn;
	static const char *m_kImapSeen;
	static const char *m_kImapAnswered;
	static const char *m_kImapNotSeen;
	static const char *m_kImapNotAnswered;
	static const char *m_kImapCharset;
	static const char *m_kImapUnDeleted;

protected:
	typedef enum _msg_TransformType
   	{
		kOverwrite,    /* "John Doe" -> "John*Doe",   simple contains   */
		kInsert,       /* "John Doe" -> "John* Doe",  name completion   */
		kSurround      /* "John Doe" -> "John* *Doe", advanced contains */
	} msg_TransformType;

	char *TransformSpacesToStars (const char *, msg_TransformType transformType);
	nsresult OpenNewsResultInUnknownGroup (nsMsgResultElement*);

	static nsresult EncodeImapTerm (nsMsgSearchTerm *, PRBool reallyDredd, PRInt16 src_csid, PRInt16 dest_csid, char **ppOutTerm);
};

#endif
