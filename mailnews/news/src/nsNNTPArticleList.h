/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsNNTPArticleList_h___
#define nsNNTPArticleList_h___

#include "nsINNTPArticleList.h"

/* XXX - temporary hack so this will compile */
typedef PRUint32 nsMsgKey;

class nsNNTPArticleList : public nsINNTPArticleList
#ifdef HAVE_CHANGELISTENER
 : public ChangeListener
#endif
{
public:
	nsNNTPArticleList();
	virtual ~nsNNTPArticleList();
	
    NS_DECL_ISUPPORTS
    NS_DECL_NSINNTPARTICLELIST

    // other stuff
protected:
	struct MSG_NewsKnown	m_idsOnServer;
#ifdef HAVE_PANES
	MSG_Pane				*m_pane;
#endif
  /* formerly m_groupName */
	nsINNTPNewsgroup		*m_newsgroup;
	const nsINNTPHost			*m_host;
#ifdef HAVE_NEWSDB
	NewsGroupDB				*m_newsDB;
#endif
#ifdef HAVE_IDARRAY
	IDArray					m_idsInDB;
#ifdef DEBUG_bienvenu
	IDArray					m_idsDeleted;
#endif
#endif
	PRInt32					m_dbIndex;
	nsMsgKey				m_highwater;
};

#endif /* nsNNTPArticleList_h___ */
