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

 /* This is a stub event sink for a NNTP Newsgroup introduced by mscott to test
   the NNTP protocol */

#include "nscore.h"
#include "plstr.h"
#include "prmem.h"
#include <stdio.h>

#include "nsISupports.h" /* interface nsISupports */

#include "nsINNTPNewsgroup.h"
#include "nsNNTPArticleSet.h"

static NS_DEFINE_IID(kINNTPNewsgroupIID, NS_INNTPNEWSGROUP_IID);

class nsNNTPNewsgroupStub : public nsINNTPNewsgroup 
{

 public: 
	 nsNNTPNewsgroupStub();
	 virtual ~nsNNTPNewsgroupStub();
	 
	 NS_DECL_ISUPPORTS
  
	 NS_IMETHOD GetName(char * *aName) const;
	 NS_IMETHOD SetName(char * aName);
  
	 NS_IMETHOD GetPrettyName(char * *aPrettyName) const;
	 NS_IMETHOD SetPrettyName(char * aPrettyName);
 
	 NS_IMETHOD GetPassword(char * *aPassword) const;
	 NS_IMETHOD SetPassword(char * aPassword);
 
	 NS_IMETHOD GetUsername(char * *aUsername) const;
	 NS_IMETHOD SetUsername(char * aUsername);
  
	 NS_IMETHOD GetNeedsExtraInfo(PRBool *aNeedsExtraInfo) const;
	 NS_IMETHOD SetNeedsExtraInfo(PRBool aNeedsExtraInfo);
 
	 NS_IMETHOD IsOfflineArticle(PRInt32 num, PRBool *_retval);

	 NS_IMETHOD GetCategory(PRBool *aCategory) const;
	 NS_IMETHOD SetCategory(PRBool aCategory);
  
	 NS_IMETHOD GetSubscribed(PRBool *aSubscribed) const;
	 NS_IMETHOD SetSubscribed(PRBool aSubscribed);
  
	 NS_IMETHOD GetWantNewTotals(PRBool *aWantNewTotals) const;
	 NS_IMETHOD SetWantNewTotals(PRBool aWantNewTotals);
  
	 NS_IMETHOD GetNewsgroupList(nsINNTPNewsgroupList * *aNewsgroupList) const;
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

	 nsINNTPNewsgroupList * m_newsgroupList;
};

nsNNTPNewsgroupStub::nsNNTPNewsgroupStub()
{
	NS_INIT_REFCNT();
	m_groupName = nsnull;
	m_prettyName = nsnull;
	m_password = nsnull;
	m_userName = nsnull;
	m_newsgroupList = nsnull;
	m_needsExtraInfo = PR_FALSE;
	m_category = PR_FALSE;
}

nsNNTPNewsgroupStub::~nsNNTPNewsgroupStub()
{
	printf("destroying newsgroup: %s", m_groupName ? m_groupName : "");
	NS_IF_RELEASE(m_newsgroupList);
	PR_FREEIF(m_groupName);
	PR_FREEIF(m_password);
	PR_FREEIF(m_userName);
	PR_FREEIF(m_prettyName);
}

NS_IMPL_ISUPPORTS(nsNNTPNewsgroupStub, kINNTPNewsgroupIID);

nsresult nsNNTPNewsgroupStub::GetName(char ** aName) const
{
	if (aName)
	{
		*aName = PL_strdup(m_groupName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::SetName(char *aName)
{
	if (aName)
	{
		printf("Setting newsgroup name to %s. \n", aName);
		m_groupName = PL_strdup(aName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::GetPrettyName(char ** aName) const
{
	if (aName)
	{
		*aName = PL_strdup(m_prettyName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::SetPrettyName(char *aName)
{
	if (aName)
	{
		printf("Setting pretty newsgroup name to %s. \n", aName);
		m_prettyName = PL_strdup(aName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::GetPassword(char ** aName) const
{
	if (aName)
	{
		*aName = PL_strdup(m_password);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::SetPassword(char *aName)
{
	if (aName)
	{
		printf("Setting password for newsgroup %s to %s. \n", m_groupName, aName);
		m_password = PL_strdup(aName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::GetUsername(char ** aUsername) const
{
	if (aUsername)
	{
		*aUsername = PL_strdup(m_userName);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::SetUsername(char *aUsername)
{
	if (aUsername)
	{
		printf("Setting username for newsgroup %s to %s. \n", m_groupName, aUsername);
		m_userName = PL_strdup(aUsername);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::GetNeedsExtraInfo(PRBool *aNeedsExtraInfo) const
{
	if (aNeedsExtraInfo)
	{
		*aNeedsExtraInfo = m_needsExtraInfo;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::SetNeedsExtraInfo(PRBool aNeedsExtraInfo)
{
	if (aNeedsExtraInfo)
	{
		printf("Setting needs extra info for newsgroup %s to %s. \n", m_groupName, aNeedsExtraInfo ? "TRUE" : "FALSE" );
		m_needsExtraInfo = aNeedsExtraInfo;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::IsOfflineArticle(PRInt32 num, PRBool *_retval)
{
	printf("Testing for offline article %d in %s. \n", num, m_groupName);
	if (_retval)
		*_retval = PR_FALSE;
	return NS_OK;

}

nsresult nsNNTPNewsgroupStub::GetCategory(PRBool *aCategory) const
{
	if (aCategory)
	{
		*aCategory = m_category;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::SetCategory(PRBool aCategory)
{
	if (aCategory)
	{
		printf("Setting is category for newsgroup %s to %s. \n", m_groupName, aCategory ? "TRUE" : "FALSE" );
		m_category = aCategory;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::GetSubscribed(PRBool *aSubscribed) const
{
	if (aSubscribed)
	{
		*aSubscribed = m_isSubscribed;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::SetSubscribed(PRBool aSubscribed)
{
	if (aSubscribed)
	{
		m_isSubscribed = aSubscribed;
	}

	return NS_OK;
}

 nsresult nsNNTPNewsgroupStub::GetWantNewTotals(PRBool *aWantNewTotals) const
{
	if (aWantNewTotals)
	{
		*aWantNewTotals = m_wantsNewTotals;
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::SetWantNewTotals(PRBool aWantNewTotals)
{
	if (aWantNewTotals)
	{
		printf("Setting wants new totals for newsgroup %s to %s. \n", m_groupName, aWantNewTotals ? "TRUE" : "FALSE" );
		m_wantsNewTotals = aWantNewTotals;
	}

	return NS_OK;
}
 
 nsresult nsNNTPNewsgroupStub::GetNewsgroupList(nsINNTPNewsgroupList * *aNewsgroupList) const
{
	if (aNewsgroupList)
	{
		*aNewsgroupList = m_newsgroupList;
		NS_IF_ADDREF(m_newsgroupList);
	}

	return NS_OK;
}

nsresult nsNNTPNewsgroupStub::SetNewsgroupList(nsINNTPNewsgroupList * aNewsgroupList)
{
	if (aNewsgroupList)
	{
		printf("Setting newsgroup list for newsgroup %s. \n", m_groupName);
		m_newsgroupList = aNewsgroupList;
		NS_IF_ADDREF(m_newsgroupList);
	}

	return NS_OK;
} 

nsresult nsNNTPNewsgroupStub::UpdateSummaryFromNNTPInfo(PRInt32 oldest, PRInt32 youngest, PRInt32 total_messages)
{
	printf("Updating summary with oldest= %d, youngest= %d, and total messages = %d. \n", oldest, youngest, total_messages);
	return NS_OK;
}

extern "C" {

nsresult NS_NewNewsgroup(nsINNTPNewsgroup **info,
                char *line,
                nsNNTPArticleSet *set,
                PRBool subscribed,
                nsINNTPHost *host,
                int depth)
{
	nsresult rv = NS_OK;
	nsNNTPNewsgroupStub * group = new nsNNTPNewsgroupStub();
	if (group)
	{
		group->SetSubscribed(subscribed);
		rv = group->QueryInterface(kINNTPNewsgroupIID, (void **) info);
	}

	return rv;
}
}