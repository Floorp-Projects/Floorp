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

#ifndef _nsMsgFilter_H_
#define _nsMsgFilter_H_

#include "nscore.h"
#include "nsISupports.h"
#include "nsIMsgFilter.h"
#include "nsMsgSearchArray.h"

class nsMsgRuleAction
{
public:
	nsMsgRuleAction();
	virtual ~nsMsgRuleAction();
        nsMsgRuleActionType      m_type;
		// this used to be a union - why bother?
        nsMsgPriority	m_priority;  /* priority to set rule to */
        nsCString		m_folderName;    /* Or some folder identifier, if such a thing is invented */
        nsCString		m_originalServerPath;
} ;


class nsMsgFilter : public nsIMsgFilter
{
public:
  NS_DECL_ISUPPORTS
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGFILTER_IID; return iid; }

	nsMsgFilter();
	virtual ~nsMsgFilter ();

	NS_IMETHOD GetFilterType(nsMsgFilterTypeType *filterType);
	NS_IMETHOD GetEnabled(PRBool *enabled);
	NS_IMETHOD SetEnabled(PRBool enabled) {m_enabled = enabled; return NS_OK;}
	NS_IMETHOD GetFilterName(char **name);	
	NS_IMETHOD SetFilterName(char *name);
	NS_IMETHOD GetFilterDesc(char **description);
	NS_IMETHOD SetFilterDesc(char *description);

	NS_IMETHOD AddTerm(     
		nsMsgSearchAttribute attrib,    /* attribute for this term                */
		nsMsgSearchOperator op,         /* operator e.g. opContains               */
		nsMsgSearchValue *value,        /* value e.g. "Dogbert"                   */
		PRBool BooleanAND, 	       /* PR_TRUE if AND is the boolean operator. PR_FALSE if OR is the boolean operators */
		const char * arbitraryHeader);       /* arbitrary header specified by user. ignored unless attrib = attribOtherHeader */

	NS_IMETHOD GetNumTerms(PRInt32 *numTerms);

	NS_IMETHOD GetTerm(PRInt32 termIndex, 
		nsMsgSearchAttribute *attrib,    /* attribute for this term                */
		nsMsgSearchOperator *op,         /* operator e.g. opContains               */
		nsMsgSearchValue *value,         /* value e.g. "Dogbert"                   */
		PRBool *BooleanAnd,				/* PR_TRUE if AND is the boolean operator. PR_FALSE if OR is the boolean operator */
		char ** arbitraryHeader);        /* arbitrary header specified by user. ignore unless attrib = attribOtherHeader */

	NS_IMETHOD SetScope(nsMsgSearchScopeTerm *scope);
	NS_IMETHOD GetScope(nsMsgSearchScopeTerm **scope);

	/* if type is acChangePriority, value is a pointer to priority.
	   If type is acMoveToFolder, value is pointer to folder name.
	   Otherwise, value is ignored.
	*/
	NS_IMETHOD SetAction(nsMsgRuleActionType type, void *value);
	NS_IMETHOD GetAction(nsMsgRuleActionType *type, void **value) ;
	NS_IMETHOD MatchHdr(nsIMsgDBHdr	*msgHdr, nsIMsgFolder *folder, nsIMsgDatabase *db, const char *headers, PRUint32 headersSize, PRBool *pResult) ;
	NS_IMETHOD LogRuleHit(nsOutputStream *stream, nsIMsgDBHdr *header);


	nsMsgFilterTypeType	GetType() {return m_type;}
	void			SetType(nsMsgFilterTypeType	type) {m_type = type;}
	PRBool			GetEnabled() {return m_enabled;}
	nsresult		GetName(nsCString *name);
	nsresult		SetName(nsCString *name);
	nsresult		SetDescription(nsCString *desc);
	nsresult		GetDescription(nsCString *desc);
	void			SetFilterScript(nsCString *filterName) ;
	void			SetFilterList(nsMsgFilterList *filterList) ;
	PRBool			IsRule() 
						{return (m_type & (nsMsgFilterType::InboxRule |
                                           nsMsgFilterType::NewsRule)) != 0;}
	PRBool			IsScript() {return (m_type &
                                        (nsMsgFilterType::InboxJavaScript |
                                         nsMsgFilterType::NewsJavaScript)) != 0;}

	// filing routines.
	nsresult		SaveToTextFile(nsIOFileStream *stream);
	nsresult		SaveRule();

	PRInt16			GetVersion() {return (m_filterList) ? m_filterList->GetVersion() : 0;}
	nsMsgFilterList	*GetFilterList() {return m_filterList;}
    void            SetDontFileMe(PRBool bDontFileMe) {m_dontFileMe = bDontFileMe;}
    nsMsgSearchTermArray* GetTermList() {return &m_termList;}       /* linked list of criteria terms */
#ifdef DEBUG
	void	Dump();
#endif

	nsresult		ConvertMoveToFolderValue(nsCString &relativePath);
static	const char *GetActionStr(nsMsgRuleActionType action);
static	nsresult GetActionFilingStr(nsMsgRuleActionType action, nsCString &actionStr);
static nsMsgRuleActionType GetActionForFilingStr(nsCString &actionStr);
	nsMsgRuleAction      m_action;
protected:
	nsMsgFilterTypeType m_type;
	PRBool			m_enabled;
	nsCString		m_filterName;
	nsCString		m_scriptFileName;	// iff this filter is a script.
	nsCString		m_description;
    PRBool         m_dontFileMe;

	nsMsgFilterList *m_filterList;	/* owning filter list */
    nsMsgSearchTermArray m_termList;       /* linked list of criteria terms */
    nsMsgSearchScopeTerm       *m_scope;         /* default for mail rules is inbox, but news rules could
have a newsgroup - LDAP would be invalid */

};

#endif
