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
#include "nsIUrlListenerManager.h"
#include "nsINetlibURL.h" /* this should be temporary until Network N2 project lands */
#include "nsINNTPNewsgroupPost.h"

class nsNntpUrl : public nsINntpUrl, public nsINetlibURL
{
public:
	// nsIMsgMailNewsUrl interface
	NS_IMETHOD SetUrlState(PRBool aRunningUrl, nsresult aExitCode);
	NS_IMETHOD GetUrlState(PRBool * aRunningUrl);

	NS_IMETHOD SetErrorMessage (char * errorMessage);
	NS_IMETHOD GetErrorMessage (char ** errorMessage) const;  // caller must free using PR_Free
	NS_IMETHOD RegisterListener (nsIUrlListener * aUrlListener);
	NS_IMETHOD UnRegisterListener (nsIUrlListener * aUrlListener);

	// nsIURL
    NS_IMETHOD_(PRBool) Equals(const nsIURL *aURL) const;
    NS_IMETHOD GetSpec(const char* *result) const;
    NS_IMETHOD SetSpec(const char* spec);
    NS_IMETHOD GetProtocol(const char* *result) const;
    NS_IMETHOD SetProtocol(const char* protocol);
    NS_IMETHOD GetHost(const char* *result) const;
    NS_IMETHOD SetHost(const char* host);
    NS_IMETHOD GetHostPort(PRUint32 *result) const;
    NS_IMETHOD SetHostPort(PRUint32 port);
    NS_IMETHOD GetFile(const char* *result) const;
    NS_IMETHOD SetFile(const char* file);
    NS_IMETHOD GetRef(const char* *result) const;
    NS_IMETHOD SetRef(const char* ref);
    NS_IMETHOD GetSearch(const char* *result) const;
    NS_IMETHOD SetSearch(const char* search);
    NS_IMETHOD GetContainer(nsISupports* *result) const;
    NS_IMETHOD SetContainer(nsISupports* container);	
    NS_IMETHOD GetLoadAttribs(nsILoadAttribs* *result) const;	// make obsolete
    NS_IMETHOD SetLoadAttribs(nsILoadAttribs* loadAttribs);	// make obsolete
    NS_IMETHOD GetURLGroup(nsIURLGroup* *result) const;	// make obsolete
    NS_IMETHOD SetURLGroup(nsIURLGroup* group);	// make obsolete
    NS_IMETHOD SetPostHeader(const char* name, const char* value);	// make obsolete
    NS_IMETHOD SetPostData(nsIInputStream* input);	// make obsolete
    NS_IMETHOD GetContentLength(PRInt32 *len);
    NS_IMETHOD GetServerStatus(PRInt32 *status);  // make obsolete
    NS_IMETHOD ToString(PRUnichar* *aString) const;
  
    // from nsINetlibURL:

    NS_IMETHOD GetURLInfo(URL_Struct_ **aResult) const;
    NS_IMETHOD SetURLInfo(URL_Struct_ *URL_s);

	// From nsINntpUrl
	NS_IMETHOD SetNntpHost (nsINNTPHost * newsHost);
	NS_IMETHOD GetNntpHost (nsINNTPHost ** newsHost) const;

	NS_IMETHOD SetNntpArticleList (nsINNTPArticleList * articleList);
	NS_IMETHOD GetNntpArticleList (nsINNTPArticleList ** articleList) const;

	NS_IMETHOD SetNewsgroup (nsINNTPNewsgroup * newsgroup);
	NS_IMETHOD GetNewsgroup (nsINNTPNewsgroup ** newsgroup) const;

	NS_IMETHOD SetOfflineNewsState (nsIMsgOfflineNewsState * offlineNews);
	NS_IMETHOD GetOfflineNewsState (nsIMsgOfflineNewsState ** offlineNews) const ;

	NS_IMETHOD SetNewsgroupList (nsINNTPNewsgroupList * newsgroupList);
	NS_IMETHOD GetNewsgroupList (nsINNTPNewsgroupList ** newsgroupList) const;

    NS_IMETHOD SetMessageToPost(nsINNTPNewsgroupPost *post);
    NS_IMETHOD GetMessageToPost(nsINNTPNewsgroupPost **post);

    // nsNntpUrl
    nsNntpUrl(nsISupports* aContainer, nsIURLGroup* aGroup);

    NS_DECL_ISUPPORTS

	// protocol specific code to parse a url...
    nsresult ParseURL(const nsString& aSpec, const nsIURL* aURL = nsnull);

protected:
    virtual ~nsNntpUrl();

    /* Here's our link to the netlib world.... */
    URL_Struct *m_URL_s;

    char		*m_spec;
    char		*m_protocol;
    char		*m_host;
    char		*m_file;
    char		*m_ref;
    char		*m_search;
	char		*m_errorMessage;

	PRBool		m_runningUrl;
    
    nsINNTPNewsgroupPost *m_newsgroupPost;

	// manager of all of current url listeners....
	nsIUrlListenerManager * m_urlListeners;
    
	PRInt32 m_port;
    nsISupports*    m_container;

	/* NNTP specific event sinks */
	nsINNTPHost				* m_newsHost;
	nsINNTPArticleList		* m_articleList;
	nsINNTPNewsgroup		* m_newsgroup;
	nsIMsgOfflineNewsState	* m_offlineNews;
	nsINNTPNewsgroupList	* m_newsgroupList;

	void ReconstructSpec(void);
};

#endif // nsNntpUrl_h__
