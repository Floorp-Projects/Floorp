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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nscore.h"
#include "plstr.h"
#include "prmem.h"
#include <stdio.h>

#include "nsISupports.h" /* interface nsISupports */

#include "nsINNTPNewsgroup.h"
#include "nsINNTPHost.h"
#include "nsMsgKeySet.h"

#include "nsCOMPtr.h"

class nsNNTPNewsgroup : public nsINNTPNewsgroup 
{

 public: 
	 nsNNTPNewsgroup();
	 virtual ~nsNNTPNewsgroup();
	 
	 NS_DECL_ISUPPORTS
  
	 NS_IMETHOD GetName(char * *aName);
	 NS_IMETHOD SetName(char * aName);
  
	 NS_IMETHOD GetPrettyName(char * *aPrettyName);
	 NS_IMETHOD SetPrettyName(char * aPrettyName);
 
	 NS_IMETHOD GetPassword(char * *aPassword);
	 NS_IMETHOD SetPassword(char * aPassword);
 
	 NS_IMETHOD GetUsername(char * *aUsername);
	 NS_IMETHOD SetUsername(char * aUsername);
  
	 NS_IMETHOD GetNeedsExtraInfo(PRBool *aNeedsExtraInfo);
	 NS_IMETHOD SetNeedsExtraInfo(PRBool aNeedsExtraInfo);
 
	 NS_IMETHOD IsOfflineArticle(PRInt32 num, PRBool *_retval);

	 NS_IMETHOD GetCategory(PRBool *aCategory);
	 NS_IMETHOD SetCategory(PRBool aCategory);
  
	 NS_IMETHOD GetSubscribed(PRBool *aSubscribed);
	 NS_IMETHOD SetSubscribed(PRBool aSubscribed);
  
	 NS_IMETHOD GetWantNewTotals(PRBool *aWantNewTotals);
	 NS_IMETHOD SetWantNewTotals(PRBool aWantNewTotals);
  
	 NS_IMETHOD GetNewsgroupList(nsINNTPNewsgroupList * *aNewsgroupList);
	 NS_IMETHOD SetNewsgroupList(nsINNTPNewsgroupList * aNewsgroupList);

	 NS_IMETHOD UpdateSummaryFromNNTPInfo(PRInt32 oldest, PRInt32 youngest, PRInt32 total_messages);

 protected:
	 char * m_groupName;
	 char * m_prettyName;
	 char * m_password;
	 char * m_userName;
	 
	 PRBool	m_isSubscribed;
	 PRBool m_wantsNewTotals;
	 PRBool m_needsExtraInfo;
	 PRBool m_category;

	 nsCOMPtr<nsINNTPNewsgroupList> m_newsgroupList;
};

nsNNTPNewsgroup::nsNNTPNewsgroup()
{
	NS_INIT_REFCNT();
	m_groupName = nsnull;
	m_prettyName = nsnull;
	m_password = nsnull;
	m_userName = nsnull;
	m_needsExtraInfo = PR_FALSE;
	m_category = PR_FALSE;
}

nsNNTPNewsgroup::~nsNNTPNewsgroup()
{
#ifdef DEBUG_sspitzer
	printf("destroying newsgroup: %s", m_groupName ? m_groupName : "");
#endif
	PR_FREEIF(m_groupName);
	PR_FREEIF(m_password);
	PR_FREEIF(m_userName);
	PR_FREEIF(m_prettyName);
}

NS_IMPL_ISUPPORTS(nsNNTPNewsgroup, nsINNTPNewsgroup::GetIID());

nsresult nsNNTPNewsgroup::GetName(char ** aName)
{
	if (aName)
	{
		*aName = PL_strdup(m_groupName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetName(char *aName)
{
	if (aName)
	{
#ifdef DEBUG_sspitzer
		printf("Setting newsgroup name to %s. \n", aName);
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

nsresult nsNNTPNewsgroup::SetPrettyName(char *aName)
{
	if (aName)
	{
#ifdef DEBUG_sspitzer
		printf("Setting pretty newsgroup name to %s. \n", aName);
#endif
		m_prettyName = PL_strdup(aName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::GetPassword(char ** aName)
{
	if (aName)
	{
		*aName = PL_strdup(m_password);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetPassword(char *aName)
{
	if (aName)
	{
#ifdef DEBUG_sspitzer
		printf("Setting password for newsgroup %s to %s. \n", m_groupName, aName);
#endif
		m_password = PL_strdup(aName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::GetUsername(char ** aUsername)
{
	if (aUsername)
	{
		*aUsername = PL_strdup(m_userName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::SetUsername(char *aUsername)
{
	if (aUsername)
	{
#ifdef DEBUG_sspitzer
		printf("Setting username for newsgroup %s to %s. \n", m_groupName, aUsername);
#endif
		m_userName = PL_strdup(aUsername);
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
#ifdef DEBUG_sspitzer
		printf("Setting needs extra info for newsgroup %s to %s. \n", m_groupName, aNeedsExtraInfo ? "TRUE" : "FALSE" );
#endif
		m_needsExtraInfo = aNeedsExtraInfo;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroup::IsOfflineArticle(PRInt32 num, PRBool *_retval)
{
#ifdef DEBUG_sspitzer
	printf("Testing for offline article %d in %s. \n", num, m_groupName);
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
#ifdef DEBUG_sspitzer
		printf("Setting is category for newsgroup %s to %s. \n", m_groupName, aCategory ? "TRUE" : "FALSE" );
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
	if (aSubscribed)
	{
		m_isSubscribed = aSubscribed;
	}

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
#ifdef DEBUG_sspitzer
		printf("Setting wants new totals for newsgroup %s to %s. \n", m_groupName, aWantNewTotals ? "TRUE" : "FALSE" );
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
#ifdef DEBUG_sspitzer
		printf("Setting newsgroup list for newsgroup %s. \n", m_groupName);
#endif
		m_newsgroupList = aNewsgroupList;
		NS_IF_ADDREF(aNewsgroupList);
	}

	return NS_OK;
} 

nsresult nsNNTPNewsgroup::UpdateSummaryFromNNTPInfo(PRInt32 oldest, PRInt32 youngest, PRInt32 total_messages)
{
#ifdef DEBUG_sspitzer
	printf("Updating summary with oldest= %d, youngest= %d, and total messages = %d. \n", oldest, youngest, total_messages);
#endif
	return NS_OK;
}

extern "C" {

nsresult NS_NewNewsgroup(nsINNTPNewsgroup **info,
                char *line,
		nsMsgKeySet *set,
                PRBool subscribed,
                nsINNTPHost *host,
                int depth)
{
	nsresult rv = NS_OK;
	nsNNTPNewsgroup * group = new nsNNTPNewsgroup();
	if (group) {
		group->SetSubscribed(subscribed);
		rv = group->QueryInterface(nsINNTPNewsgroup::GetIID(), (void **) info);
	}

#ifdef DEBUG_sspitzer
	printf("NS_NewNewsgroup(%s)\n",line?line:"(null)");
#endif

	if (line) {
		group->SetName(line);
	}

	return rv;
}

} /* extern "C" */
