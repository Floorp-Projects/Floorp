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

#ifndef _nsMsgSearchAdapter_H_
#define _nsMsgSearchAdapter_H_

#include "nsMsgSearchCore.h"

#include "nsIMsgSearchAdapter.h"
#include "nsIMsgSearchValidityTable.h"
#include "nsIMsgSearchValidityManager.h"
#include "nsIMsgSearchTerm.h"
#include "nsMsgSearchArray.h"
#include "nsINntpIncomingServer.h"

class nsIMsgSearchScopeTerm;

//-----------------------------------------------------------------------------
// These Adapter classes contain the smarts to convert search criteria from 
// the canonical structures in msg_srch.h into whatever format is required
// by their protocol. 
//
// There is a separate Adapter class for area (pop, imap, nntp, ldap) to contain
// the special smarts for that protocol.
//-----------------------------------------------------------------------------

inline PRBool IsStringAttribute (nsMsgSearchAttribValue a)
{
	return ! (a == nsMsgSearchAttrib::Priority || a == nsMsgSearchAttrib::Date || 
		a == nsMsgSearchAttrib::MsgStatus || a == nsMsgSearchAttrib::MessageKey ||
		a == nsMsgSearchAttrib::Size || a == nsMsgSearchAttrib::AgeInDays ||
		a == nsMsgSearchAttrib::FolderInfo);
}

class nsMsgSearchAdapter : public nsIMsgSearchAdapter
{
public:
	nsMsgSearchAdapter (nsIMsgSearchScopeTerm*, nsISupportsArray *);
	virtual ~nsMsgSearchAdapter ();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMSGSEARCHADAPTER

	nsIMsgSearchScopeTerm		*m_scope;
    nsCOMPtr<nsISupportsArray> m_searchTerms;       /* linked list of criteria terms */

	PRBool m_abortCalled;

	static nsresult EncodeImap (char **ppEncoding, 
									   nsISupportsArray *searchTerms,  
									   const PRUnichar *srcCharset, 
									   const PRUnichar *destCharset,
									   PRBool reallyDredd = PR_FALSE);
	
	static nsresult EncodeImapValue(char *encoding, const char *value, PRBool useQuotes, PRBool reallyDredd);

	static char *GetImapCharsetParam(const PRUnichar *destCharset);
	void GetSearchCharsets(nsString &srcCharset, nsString &destCharset);
  static PRUnichar *EscapeSearchUrl (const PRUnichar *nntpCommand);
  static PRUnichar *EscapeImapSearchProtocol(const PRUnichar *imapCommand);
  static PRUnichar *EscapeQuoteImapSearchProtocol(const PRUnichar *imapCommand);
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

	static nsresult EncodeImapTerm (nsIMsgSearchTerm *, PRBool reallyDredd, const PRUnichar *srcCharset, const PRUnichar *destCharset, char **ppOutTerm);
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

class nsMsgSearchValidityTable : public nsIMsgSearchValidityTable
{
public:
	nsMsgSearchValidityTable ();
    NS_DECL_NSIMSGSEARCHVALIDITYTABLE
    NS_DECL_ISUPPORTS
								  
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
inline nsresult nsMsgSearchValidityTable::SetAvailable (int a, int o, PRBool b)
{ m_table [a][o].bitAvailable = b; return NS_OK;}
inline nsresult nsMsgSearchValidityTable::SetEnabled (int a, int o, PRBool b)
{ m_table [a][o].bitEnabled = b; return NS_OK; }
inline nsresult nsMsgSearchValidityTable::SetValidButNotShown (int a, int o, PRBool b)
{ m_table [a][o].bitValidButNotShown = b; return NS_OK;}

inline nsresult nsMsgSearchValidityTable::GetAvailable (int a, int o, PRBool *aResult)
{ *aResult = m_table [a][o].bitAvailable; return NS_OK;}
inline nsresult nsMsgSearchValidityTable::GetEnabled (int a, int o, PRBool *aResult)
{  *aResult = m_table [a][o].bitEnabled; return NS_OK;}
inline nsresult nsMsgSearchValidityTable::GetValidButNotShown (int a, int o, PRBool *aResult)
{  *aResult = m_table [a][o].bitValidButNotShown; return NS_OK;}

class nsMsgSearchValidityManager : public nsIMsgSearchValidityManager
{
public:
	nsMsgSearchValidityManager ();
  
protected:
  virtual ~nsMsgSearchValidityManager ();

public:
    NS_DECL_NSIMSGSEARCHVALIDITYMANAGER
    NS_DECL_ISUPPORTS
  
	nsresult GetTable (int, nsMsgSearchValidityTable**);
  
	nsresult PostProcessValidityTable (nsINntpIncomingServer *);

protected:

	// There's one global validity manager that everyone uses. You *could* do 
	// this with static members of the adapter classes, but having a dedicated
	// object makes cleanup of these tables (at shutdown-time) automagic.

    nsCOMPtr<nsIMsgSearchValidityTable> m_offlineMailTable;
    nsCOMPtr<nsIMsgSearchValidityTable> m_onlineMailTable;
    nsCOMPtr<nsIMsgSearchValidityTable> m_onlineMailFilterTable;
    nsCOMPtr<nsIMsgSearchValidityTable> m_newsTable;
    nsCOMPtr<nsIMsgSearchValidityTable> m_newsExTable;
    nsCOMPtr<nsIMsgSearchValidityTable> m_localNewsTable; // used for local news searching or offline news searching...

	nsresult NewTable (nsIMsgSearchValidityTable **);

	nsresult InitOfflineMailTable ();
	nsresult InitOnlineMailTable ();
	nsresult InitOnlineMailFilterTable ();
	nsresult InitNewsTable ();
	nsresult InitLocalNewsTable(); 
	nsresult InitNewsExTable (nsINntpIncomingServer *host = nsnull);

	void EnableLdapAttribute (nsMsgSearchAttribValue, PRBool enabled = PR_TRUE);
};

#endif
