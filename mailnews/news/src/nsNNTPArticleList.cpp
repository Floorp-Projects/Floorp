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

#include "nsCOMPtr.h"
#include "nsNNTPArticleList.h"
#include "nsIMsgFolder.h"

NS_IMPL_ISUPPORTS1(nsNNTPArticleList, nsINNTPArticleList)

nsNNTPArticleList::nsNNTPArticleList()
{
    NS_INIT_REFCNT();
}

nsresult
nsNNTPArticleList::Initialize(nsIMsgNewsFolder *newsFolder)
{
    nsresult rv;
    NS_ENSURE_ARG_POINTER(newsFolder);
    
    m_dbIndex = 0;

    m_newsFolder = newsFolder;

    nsCOMPtr <nsIMsgFolder> folder = do_QueryInterface(m_newsFolder, &rv);
    NS_ENSURE_SUCCESS(rv,rv);

    rv = folder->GetMsgDatabase(nsnull /* msgWindow */, getter_AddRefs(m_newsDB));
    NS_ENSURE_SUCCESS(rv,rv);
    if (!m_newsDB) return NS_ERROR_UNEXPECTED;

    rv = m_newsDB->ListAllKeys(m_idsInDB);
    NS_ENSURE_SUCCESS(rv,rv);

    return NS_OK;
}

nsNNTPArticleList::~nsNNTPArticleList()
{
  if (m_newsDB) {
		m_newsDB->Commit(nsMsgDBCommitType::kSessionCommit);
        m_newsDB->Close(PR_TRUE);
        m_newsDB = nsnull;
  }

  m_newsFolder = nsnull;
}

nsresult
nsNNTPArticleList::AddArticleKey(PRInt32 key)
{
#ifdef DEBUG_seth
	m_idsOnServer.Add(key);
#endif

	if (m_dbIndex < m_idsInDB.GetSize())
	{
		PRInt32 idInDBToCheck = m_idsInDB.GetAt(m_dbIndex);
		// if there are keys in the database that aren't in the newsgroup
		// on the server, remove them. We probably shouldn't do this if
		// we have a copy of the article offline.
		while (idInDBToCheck < key)
		{
            m_newsFolder->RemoveMessage(idInDBToCheck);
#ifdef DEBUG_seth
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

nsresult
nsNNTPArticleList::FinishAddingArticleKeys()
{
#ifdef DEBUG_seth
	// make sure none of the deleted turned up on the idsOnServer list
	for (PRUint32 i = 0; i < m_idsDeleted.GetSize(); i++) {
		NS_ASSERTION(m_idsOnServer.FindIndex((nsMsgKey)(m_idsDeleted.GetAt(i)), 0) == nsMsgViewIndex_None, "a deleted turned up on the idsOnServer list");
    }
#endif
	return NS_OK;
}
