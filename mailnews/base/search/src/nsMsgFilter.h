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

#ifndef _nsMsgFilter_H_
#define _nsMsgFilter_H_

#include "nscore.h"
#include "nsISupports.h"
#include "nsIMsgFilter.h"
#include "nsMsgSearchArray.h"
#include "nsIMsgSearchScopeTerm.h"

class nsMsgRuleAction
{
public:
	nsMsgRuleAction();
	virtual ~nsMsgRuleAction();
        nsMsgRuleActionType      m_type;
		// this used to be a union - why bother?
        nsMsgPriorityValue	m_priority;  /* priority to set rule to */
        nsCString		m_folderUri;    /* Or some folder identifier, if such a thing is invented */
        nsCString		m_originalServerPath;
} ;


class nsMsgFilter : public nsIMsgFilter
{
public:
  NS_DECL_ISUPPORTS
    static const nsIID& GetIID() { static nsIID iid = NS_IMSGFILTER_IID; return iid; }

	nsMsgFilter();
	virtual ~nsMsgFilter ();

  NS_DECL_NSIMSGFILTER

	nsMsgFilterTypeType	GetType() {return m_type;}
	void			SetType(nsMsgFilterTypeType	type) {m_type = type;}
	PRBool			GetEnabled() {return m_enabled;}
	void			SetFilterScript(nsCString *filterName) ;
    void            SetFilterList(nsIMsgFilterList* filterList);
	PRBool			IsRule() 
						{return (m_type & (nsMsgFilterType::InboxRule |
                                           nsMsgFilterType::NewsRule)) != 0;}
	PRBool			IsScript() {return (m_type &
                                        (nsMsgFilterType::InboxJavaScript |
                                         nsMsgFilterType::NewsJavaScript)) != 0;}

	// filing routines.
	nsresult		SaveToTextFile(nsIOFileStream *stream);
	nsresult		SaveRule();

	PRInt16			GetVersion();
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

	nsIMsgFilterList *m_filterList;	/* owning filter list */
    nsMsgSearchTermArray m_termList;       /* linked list of criteria terms */
    nsCOMPtr<nsIMsgSearchScopeTerm> m_scope;         /* default for mail rules is inbox, but news rules could
have a newsgroup - LDAP would be invalid */

};

#endif
