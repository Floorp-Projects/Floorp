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

#ifndef nsNntpUrl_h__
#define nsNntpUrl_h__

#include "nsINntpUrl.h"
#include "nsMsgMailNewsUrl.h"
#include "nsINNTPNewsgroupPost.h"
#include "nsFileSpec.h"

class nsNntpUrl : public nsINntpUrl, public nsMsgMailNewsUrl, public nsIMsgUriUrl
{
public:
	// From nsINntpUrl
	NS_IMETHOD SetNntpHost (nsINNTPHost * newsHost);
	NS_IMETHOD GetNntpHost (nsINNTPHost ** newsHost);

	NS_IMETHOD SetNntpArticleList (nsINNTPArticleList * articleList);
	NS_IMETHOD GetNntpArticleList (nsINNTPArticleList ** articleList);

	NS_IMETHOD SetNewsgroup (nsINNTPNewsgroup * newsgroup);
	NS_IMETHOD GetNewsgroup (nsINNTPNewsgroup ** newsgroup);

	NS_IMETHOD SetOfflineNewsState (nsIMsgOfflineNewsState * offlineNews);
	NS_IMETHOD GetOfflineNewsState (nsIMsgOfflineNewsState ** offlineNews);

	NS_IMETHOD SetNewsgroupList (nsINNTPNewsgroupList * newsgroupList);
	NS_IMETHOD GetNewsgroupList (nsINNTPNewsgroupList ** newsgroupList);

    NS_IMETHOD SetMessageToPost(nsINNTPNewsgroupPost *post);
    NS_IMETHOD GetMessageToPost(nsINNTPNewsgroupPost **post);
    
    NS_IMETHOD GetMessageHeader(nsIMsgDBHdr ** aMsgHdr);
    
    // this should be an IDL attribute
    NS_IMETHOD SetMessageKey(nsMsgKey aKey);
    NS_IMETHOD GetMessageKey(nsMsgKey * aKey);

    // this should be an IDL attribute
    NS_IMETHOD SetNewsgroupName(char * aNewsgroupName);
    NS_IMETHOD GetNewsgroupName(char ** aNewsgroupName);
     
	// from nsIMsgUriUrl
	NS_IMETHOD GetURI(char ** aURI); 

    // nsNntpUrl
    nsNntpUrl();
    virtual ~nsNntpUrl();

    NS_DECL_ISUPPORTS_INHERITED

protected:  
    nsINNTPNewsgroupPost *m_newsgroupPost;
    
    nsFileSpec	*m_filePath; 

	/* NNTP specific event sinks */
	nsINNTPHost				* m_newsHost;
	nsINNTPArticleList		* m_articleList;
	nsINNTPNewsgroup		* m_newsgroup;
	nsIMsgOfflineNewsState	* m_offlineNews;
	nsINNTPNewsgroupList	* m_newsgroupList;
    nsMsgKey	              m_messageKey;
    char *                    m_newsgroupName;	
};

#endif // nsNntpUrl_h__
