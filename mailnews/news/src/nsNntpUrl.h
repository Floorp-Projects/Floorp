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

#ifndef nsNntpUrl_h__
#define nsNntpUrl_h__

#include "nsINntpUrl.h"
#include "nsMsgMailNewsUrl.h"
#include "nsINNTPNewsgroupPost.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"

class nsNntpUrl : public nsINntpUrl, public nsMsgMailNewsUrl, public nsIMsgMessageUrl
{
public:
    NS_DECL_NSINNTPURL
    NS_DECL_NSIMSGMESSAGEURL

    // nsNntpUrl
    nsNntpUrl();
    virtual ~nsNntpUrl();

    NS_DECL_ISUPPORTS_INHERITED

protected:  
    nsINNTPNewsgroupPost *m_newsgroupPost;
	virtual const char * GetUserName() { return m_userName.GetBuffer();}
	nsNewsAction m_newsAction; // the action this url represents...parse mailbox, display messages, etc.
    
    nsFileSpec	*m_filePath; 
	nsCString m_userName;
    
    // used by save message to disk
	nsCOMPtr<nsIFileSpec> m_messageFileSpec;
    PRBool                m_addDummyEnvelope;

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
