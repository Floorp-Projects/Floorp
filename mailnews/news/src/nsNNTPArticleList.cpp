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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "msgCore.h"    // precompiled header...

#include "nsNNTPNewsgroupList.h"
#include "nsINNTPHost.h"
#include "nsINNTPArticleList.h"
#include "nsMsgKeySet.h"
#include "nsNNTPArticleList.h"

NS_IMPL_ISUPPORTS1(nsNNTPArticleList, nsINNTPArticleList)

nsNNTPArticleList::nsNNTPArticleList()
{
    NS_INIT_REFCNT();
}

nsresult
nsNNTPArticleList::Initialize(nsINNTPHost * newsHost,
                              nsINNTPNewsgroup* newsgroup)
{
	m_host = newsHost;
    m_newsgroup = newsgroup;
#ifdef HAVE_PANES
	m_pane = pane;
#endif
#ifdef HAVE_NEWSDB
	m_newsDB = NULL;
#endif
	m_idsOnServer.set = nsMsgKeySet::Create();
#ifdef HAVE_PANES
	nsINNTPNewsgroup *newsFolder = m_pane->GetMaster()->FindNewsFolder(host, groupName, PR_FALSE);
	if (newsFolder)
	{
		char *url = newsFolder->BuildUrl(NULL, nsMsgKey_None);
#ifdef HAVE_NEWSDB
		if (url)
			NewsGroupDB::Open(url, m_pane->GetMaster(), &m_newsDB);
		if (m_newsDB)
			m_newsDB->ListAllIds(m_idsInDB);
#endif
		m_dbIndex = 0;

		PR_FREEIF(url);
	}
#endif
    return NS_MSG_SUCCESS;
}

nsNNTPArticleList::~nsNNTPArticleList()
{
#ifdef HAVE_NEWSDB
	if (m_newsDB)
		m_newsDB->Close();
#endif
}

nsresult
nsNNTPArticleList::AddArticleKey(PRInt32 key)
{
#ifdef DEBUG_NEWS
	char * groupname = nsnull;
	if (m_newsgroup)
		m_newsgroup->GetName(&groupname);
	printf("Adding article key %d for group %s.\n", key, groupname ? groupname : "unspecified");
	PR_FREEIF(groupname);
#endif
	m_idsOnServer.set->Add(key);
#ifdef HAVE_IDARRAY
	if (m_dbIndex < m_idsInDB.GetSize())
	{
		PRInt32 idInDBToCheck = m_idsInDB.GetAt(m_dbIndex);
		// if there are keys in the database that aren't in the newsgroup
		// on the server, remove them. We probably shouldn't do this if
		// we have a copy of the article offline.
		while (idInDBToCheck < key)
		{
#ifdef HAVE_NEWSDB
			m_newsDB->DeleteMessage(idInDBToCheck, NULL, PR_FALSE);
#ifdef DEBUG_bienvenu
			m_idsDeleted.Add(idInDBToCheck);
#endif
#endif
			if (m_dbIndex >= m_idsInDB.GetSize())
				break;
			idInDBToCheck = m_idsInDB.GetAt(++m_dbIndex);
		}
		if (idInDBToCheck == key)
			m_dbIndex++;
	}
#endif
	return 0;
}

nsresult
nsNNTPArticleList::FinishAddingArticleKeys()
{
	// make sure none of the deleted turned up on the idsOnServer list
#ifdef HAVE_NEWSDB
	for (PRInt32 i = 0; i < m_idsDeleted.GetSize(); i++)
		NS_ASSERTION((!m_idsOnServer.set->IsMember(m_idsDeleted.GetAt(i)), "a deleted turned up on the idsOnServer list");
#endif
	return 0;
}
