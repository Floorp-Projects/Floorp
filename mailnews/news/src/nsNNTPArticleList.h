/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
