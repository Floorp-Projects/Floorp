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
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nscore.h"
#include "plstr.h"
#include "prmem.h"
#include <stdio.h>

#include "nsISupports.h" /* interface nsISupports */

#include "nsNNTPNewsgroup.h"

#include "nsCOMPtr.h"
#include "nslog.h"

NS_IMPL_LOG(nsNNTPNewsgroupLog)
#define PRINTF NS_LOG_PRINTF(nsNNTPNewsgroupLog)
#define FLUSH  NS_LOG_FLUSH(nsNNTPNewsgroupLog)

nsNNTPNewsgroup::nsNNTPNewsgroup()
{
	NS_INIT_REFCNT();
	m_groupName = nsnull;
	m_prettyName = nsnull;
	m_needsExtraInfo = PR_FALSE;
	m_category = PR_FALSE;
}

nsNNTPNewsgroup::~nsNNTPNewsgroup()
{
#ifdef DEBUG_NEWS
	PRINTF("destroying newsgroup: %s", m_groupName ? m_groupName : "");
#endif
	PR_FREEIF(m_groupName);
	PR_FREEIF(m_prettyName);
}

NS_IMPL_ISUPPORTS(nsNNTPNewsgroup, NS_GET_IID(nsINNTPNewsgroup));

nsresult nsNNTPNewsgroup::GetName(char ** aName)
{
	if (aName)
	{
		*aName = PL_strdup(m_groupName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetName(const char *aName)
{
	if (aName)
	{
#ifdef DEBUG_NEWS
		PRINTF("Setting newsgroup name to %s. \n", aName);
#endif
		m_groupName = PL_strdup(aName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::GetPrettyName(char ** aName)
{
	if (aName)
	{
		*aName = PL_strdup(m_prettyName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetPrettyName(const char *aName)
{
	if (aName)
	{
#ifdef DEBUG_NEWS
		PRINTF("Setting pretty newsgroup name to %s. \n", aName);
#endif
		m_prettyName = PL_strdup(aName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::GetNeedsExtraInfo(PRBool *aNeedsExtraInfo)
{
	if (aNeedsExtraInfo)
	{
		*aNeedsExtraInfo = m_needsExtraInfo;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetNeedsExtraInfo(PRBool aNeedsExtraInfo)
{
	if (aNeedsExtraInfo)
	{
#ifdef DEBUG_NEWS
		PRINTF("Setting needs extra info for newsgroup %s to %s. \n", m_groupName, aNeedsExtraInfo ? "TRUE" : "FALSE" );
#endif
		m_needsExtraInfo = aNeedsExtraInfo;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::IsOfflineArticle(PRInt32 num, PRBool *_retval)
{
#ifdef DEBUG_NEWS
	PRINTF("Testing for offline article %d in %s. \n", num, m_groupName);
#endif
	if (_retval)
		*_retval = PR_FALSE;
	return NS_OK;

}

nsresult nsNNTPNewsgroup::GetCategory(PRBool *aCategory)
{
	if (aCategory)
	{
		*aCategory = m_category;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetCategory(PRBool aCategory)
{
	if (aCategory)
	{
#ifdef DEBUG_NEWS
		PRINTF("Setting is category for newsgroup %s to %s. \n", m_groupName, aCategory ? "TRUE" : "FALSE" );
#endif
		m_category = aCategory;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::GetSubscribed(PRBool *aSubscribed)
{
	if (aSubscribed)
	{
		*aSubscribed = m_isSubscribed;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetSubscribed(PRBool aSubscribed)
{
    m_isSubscribed = aSubscribed;

	return NS_OK;
}

 nsresult nsNNTPNewsgroup::GetWantNewTotals(PRBool *aWantNewTotals)
{
	if (aWantNewTotals)
	{
		*aWantNewTotals = m_wantsNewTotals;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetWantNewTotals(PRBool aWantNewTotals)
{
	if (aWantNewTotals)
	{
#ifdef DEBUG_NEWS
		PRINTF("Setting wants new totals for newsgroup %s to %s. \n", m_groupName, aWantNewTotals ? "TRUE" : "FALSE" );
#endif
		m_wantsNewTotals = aWantNewTotals;
	}

	return NS_OK;
}
 
 nsresult nsNNTPNewsgroup::GetNewsgroupList(nsINNTPNewsgroupList * *aNewsgroupList)
{
	if (aNewsgroupList)
	{
		*aNewsgroupList = m_newsgroupList;
		NS_IF_ADDREF(*aNewsgroupList);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetNewsgroupList(nsINNTPNewsgroupList * aNewsgroupList)
{
	if (aNewsgroupList)
	{
#ifdef DEBUG_NEWS
		PRINTF("Setting newsgroup list for newsgroup %s. \n", m_groupName);
#endif
		m_newsgroupList = aNewsgroupList;
		NS_IF_ADDREF(aNewsgroupList);
	}

	return NS_OK;
} 

nsresult nsNNTPNewsgroup::UpdateSummaryFromNNTPInfo(PRInt32 oldest, PRInt32 youngest, PRInt32 total_messages)
{
#ifdef DEBUG_NEWS
	PRINTF("Updating summary with oldest= %d, youngest= %d, and total messages = %d. \n", oldest, youngest, total_messages);
#endif
	return NS_OK;
}

nsresult nsNNTPNewsgroup::Initialize(const char *name,
                                     nsMsgKeySet *set,
                                     PRBool subscribed)
{
#ifdef DEBUG_NEWS
	PRINTF("nsNNTPNewsgroup::Intialize(name = %s, subscribed = %s)\n",name?name:"(null)", subscribed?"TRUE":"FALSE");
#endif
	nsresult rv = NS_OK;

    rv = SetSubscribed(subscribed);
    if (NS_FAILED(rv)) return rv;
    
	if (name) {
        rv = SetName((char *)name);
        if (NS_FAILED(rv)) return rv;
	}

    if (set) {
        NS_ASSERTION(0, "not doing anything with set in nsNNTPNewsgroup::Initialize() yet...");
    }
    
	return rv;
}

