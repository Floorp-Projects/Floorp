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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsNNTPNewsgroupListState.h"
#include "nsMsgNewsHost.h"


class ListNewsGroupArticleKeysState
#ifdef HAVE_CHANGELISTENER
 : public ChangeListener
#endif
{
public:
	nsNNTPArticleList();
                                  /* , MSG_Pane *pane); */
	~nsNNTPArticleList();
    
    // nsINNTPArticleKeysState
    NS_IMETHOD Init(const char *newsHost, const nsIMsgNewsgroup *newsgroup);
	NS_IMETHOD AddArticleKey(PRInt32 key);
	NS_IMETHOD FinishAddingArticleKeys();

    // other stuff
protected:
	struct MSG_NewsKnown	m_idsOnServer;
#ifdef HAVE_PANES
	MSG_Pane				*m_pane;
#endif
	const char *			m_groupName;
	nsIMsgNewsHost			*m_host;
#ifdef HAVE_NEWSDB
	NewsGroupDB				*m_newsDB;
#endif
	IDArray					m_idsInDB;
#ifdef DEBUG_bienvenu
	IDArray					m_idsDeleted;
#endif
	int32					m_dbIndex;
	MessageKey				m_highwater;
};

nsNNTPArticleList::nsNNTPArticleList(const char* newshost,
                                                             const nsIMsgNewsgroup* newsgroup)
{
    NS_INIT_REFCNT();
    Initialize(newsHost, newsgroup);
}

ListNewsgroupArticleKeysState::Initialize(const char* newsHost,
                                          const nsIMsgNewsgroup* newsgroup)
{
	m_host = host;
	m_groupName = groupName;
#ifdef HAVE_PANES
	m_pane = pane;
#endif
#ifdef HAVE_NEWSDB
	m_newsDB = NULL;
#endif
	m_idsOnServer.set = msg_NewsArtSet::Create();
#ifdef HAVE_PANES
	MSG_FolderInfoNews *newsFolder = m_pane->GetMaster()->FindNewsFolder(host, groupName, FALSE);
	if (newsFolder)
	{
		char *url = newsFolder->BuildUrl(NULL, MSG_MESSAGEKEYNONE);
#ifdef HAVE_NEWSDB
		if (url)
			NewsGroupDB::Open(url, m_pane->GetMaster(), &m_newsDB);
		if (m_newsDB)
			m_newsDB->ListAllIds(m_idsInDB);
#endif
		m_dbIndex = 0;

		FREEIF(url);
	}
#endif
}

nsNNTPArticleList::~nsNNTPArticleList()
{
#ifdef HAVE_NEWSDB
	if (m_newsDB)
		m_newsDB->Close();
#endif
}

int nsNNTPArticleList::AddArticleKey(int32 key)
{
	m_idsOnServer.set->Add(key);
	if (m_dbIndex < m_idsInDB.GetSize())
	{
		int32 idInDBToCheck = m_idsInDB.GetAt(m_dbIndex);
		// if there are keys in the database that aren't in the newsgroup
		// on the server, remove them. We probably shouldn't do this if
		// we have a copy of the article offline.
		while (idInDBToCheck < key)
		{
#ifdef HAVE_NEWSDB
			m_newsDB->DeleteMessage(idInDBToCheck, NULL, FALSE);
#endif
#ifdef DEBUG_bienvenu
			m_idsDeleted.Add(idInDBToCheck);
#endif
			if (m_dbIndex >= m_idsInDB.GetSize())
				break;
			idInDBToCheck = m_idsInDB.GetAt(++m_dbIndex);
		}
		if (idInDBToCheck == key)
			m_dbIndex++;
	}
	return 0;
}

int nsNNTPArticleList::FinishAddingArticleKeys()
{
	// make sure none of the deleted turned up on the idsOnServer list
#ifdef DEBUG_bienvenu
	for (int32 i = 0; i < m_idsDeleted.GetSize(); i++)
		XP_ASSERT (!m_idsOnServer.set->IsMember(m_idsDeleted.GetAt(i)));
#endif
	return 0;
}

