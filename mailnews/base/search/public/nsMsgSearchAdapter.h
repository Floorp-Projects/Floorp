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

#ifndef _nsMsgSearchAdapter_H_
#define _nsMsgSearchAdapter_H_

#include "nsMsgSearchCore.h"
#include "nsIMsgSearchAdapter.h"
#include "nsMsgSearchArray.h"

class nsINNTPHost;

//-----------------------------------------------------------------------------
// These Adapter classes contain the smarts to convert search criteria from 
// the canonical structures in msg_srch.h into whatever format is required
// by their protocol. 
//
// There is a separate Adapter class for area (pop, imap, nntp, ldap) to contain
// the special smarts for that protocol.
//-----------------------------------------------------------------------------

inline PRBool IsStringAttribute (nsMsgSearchAttribute a)
{
	return ! (a == nsMsgSearchAttrib::Priority || a == nsMsgSearchAttrib::Date || 
		a == nsMsgSearchAttrib::MsgStatus || a == nsMsgSearchAttrib::MessageKey ||
		a == nsMsgSearchAttrib::Size || a == nsMsgSearchAttrib::AgeInDays ||
		a == nsMsgSearchAttrib::FolderInfo);
}

class nsMsgSearchAdapter : public nsIMsgSearchAdapter
{
public:
	nsMsgSearchAdapter (nsMsgSearchScopeTerm*, nsMsgSearchTermArray&);
	virtual ~nsMsgSearchAdapter ();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGSEARCHADAPTER
//	NS_IMETHOD ValidateTerms ();
//	NS_IMETHOD Search () { return NS_OK; }
//	NS_IMETHOD SendUrl () { return NS_OK; }
//	NS_IMETHOD OpenResultElement (nsMsgResultElement *);
//	NS_IMETHOD ModifyResultElement (nsMsgResultElement*, nsMsgSearchValue*);
//	NS_IMETHOD GetEncoding (char **encoding) { return NS_OK; }

//	NS_IMETHOD FindTargetFolder (const nsMsgResultElement*, nsIMsgFolder **aFolder);
//	NS_IMETHOD Abort ();

	nsMsgSearchScopeTerm		*m_scope;
	nsMsgSearchTermArray &m_searchTerms;

	PRBool m_abortCalled;

	static nsresult EncodeImap (char **ppEncoding, 
									   nsMsgSearchTermArray &searchTerms,  
									   const PRUnichar *srcCharset, 
									   const PRUnichar *destCharset,
									   PRBool reallyDredd = PR_FALSE);
	
	static nsresult EncodeImapValue(char *encoding, const char *value, PRBool useQuotes, PRBool reallyDredd);

	static char *TryToConvertCharset(char *sourceStr, const PRUnichar *srcCharset, const PRUnichar *destCharset, PRBool useMIME2Style);
	static char *GetImapCharsetParam(const PRUnichar *destCharset);
	void GetSearchCharsets(nsString &srcCharset, nsString &destCharset);
  static char *EscapeSearchUrl (const char *nntpCommand);
  static char *EscapeImapSearchProtocol(const char *imapCommand);
  static char *EscapeQuoteImapSearchProtocol(const char *imapCommand);
  static char *UnEscapeSearchUrl (const char *commandSpecificData);

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

	static nsresult EncodeImapTerm (nsMsgSearchTerm *, PRBool reallyDredd, const PRUnichar *srcCharset, const PRUnichar *destCharset, char **ppOutTerm);
};

//-----------------------------------------------------------------------------
// Validity checking for attrib/op pairs. We need to know what operations are
// legal in three places:
//   1. when the FE brings up the dialog box and needs to know how to build
//      the menus and enable their items
//   2. when the FE fires off a search, we need to check their lists for
//      correctness
//   3. for on-the-fly capability negotion e.g. with XSEARCH-capable news 
//      servers
//-----------------------------------------------------------------------------

class nsMsgSearchValidityTable 
{
public:
	nsMsgSearchValidityTable ();
	void SetAvailable (int attrib, int op, PRBool);
	void SetEnabled (int attrib, int op, PRBool);
	void SetValidButNotShown (int attrib, int op, PRBool);
	PRBool GetAvailable (int attrib, int op);
	PRBool GetEnabled (int attrib, int op);
	PRBool GetValidButNotShown (int attrib, int op);
	nsresult ValidateTerms (nsMsgSearchTermArray&);
	int GetNumAvailAttribs();   // number of attribs with at least one available operator
								  
protected:
	int m_numAvailAttribs;        // number of rows with at least one available operator
	typedef struct vtBits
	{
		PRUint16 bitEnabled : 1;
		PRUint16 bitAvailable : 1;
		PRUint16 bitValidButNotShown : 1;
	} vtBits;
  vtBits m_table [nsMsgSearchAttrib::kNumMsgSearchAttributes][nsMsgSearchOp::kNumMsgSearchOperators];
};

// Using getters and setters seems a little nicer then dumping the 2-D array
// syntax all over the code
inline void nsMsgSearchValidityTable::SetAvailable (int a, int o, PRBool b)
{ m_table [a][o].bitAvailable = b; }
inline void nsMsgSearchValidityTable::SetEnabled (int a, int o, PRBool b)
{ m_table [a][o].bitEnabled = b; }
inline void nsMsgSearchValidityTable::SetValidButNotShown (int a, int o, PRBool b)
{ m_table [a][o].bitValidButNotShown = b; }

inline PRBool nsMsgSearchValidityTable::GetAvailable (int a, int o)
{ return m_table [a][o].bitAvailable; }
inline PRBool nsMsgSearchValidityTable::GetEnabled (int a, int o)
{ return m_table [a][o].bitEnabled; }
inline PRBool nsMsgSearchValidityTable::GetValidButNotShown (int a, int o)
{ return m_table [a][o].bitValidButNotShown; }

class nsMsgSearchValidityManager
{
public:
	nsMsgSearchValidityManager ();
	~nsMsgSearchValidityManager ();

	nsresult GetTable (int, nsMsgSearchValidityTable**);
  
	nsresult PostProcessValidityTable (nsINNTPHost*);

	enum {
	  onlineMail,
	  onlineMailFilter,
	  offlineMail,
	  localNews,
	  news,
	  newsEx,
	};

protected:

	// There's one global validity manager that everyone uses. You *could* do 
	// this with static members of the adapter classes, but having a dedicated
	// object makes cleanup of these tables (at shutdown-time) automagic.

	nsMsgSearchValidityTable *m_offlineMailTable;
	nsMsgSearchValidityTable *m_onlineMailTable;
	nsMsgSearchValidityTable *m_onlineMailFilterTable;
	nsMsgSearchValidityTable *m_newsTable;
	nsMsgSearchValidityTable *m_newsExTable;
	nsMsgSearchValidityTable *m_localNewsTable; // used for local news searching or offline news searching...

	nsresult NewTable (nsMsgSearchValidityTable **);

	nsresult InitOfflineMailTable ();
	nsresult InitOnlineMailTable ();
	nsresult InitOnlineMailFilterTable ();
	nsresult InitNewsTable ();
	nsresult InitLocalNewsTable(); 
	nsresult InitNewsExTable (nsINNTPHost *host = nsnull);

	void EnableLdapAttribute (nsMsgSearchAttribute, PRBool enabled = TRUE);
};

extern nsMsgSearchValidityManager gValidityMgr;



#endif
