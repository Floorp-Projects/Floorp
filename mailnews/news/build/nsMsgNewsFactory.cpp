/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsIFactory.h"
#include "nsISupports.h"
#include "msgCore.h"
#include "nsMsgBaseCID.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"

#include "nsNewsFolder.h"
#include "nsMsgNewsCID.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsNntpUrl.h"
#include "nsNntpService.h"
#include "nsNntpIncomingServer.h"
#include "nsNewsMessage.h"
#include "nsNNTPNewsgroup.h"
#include "nsNNTPNewsgroupPost.h"
#include "nsNNTPNewsgroupList.h"
#include "nsNNTPArticleList.h"
#include "nsNNTPHost.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kNntpUrlCID, NS_NNTPURL_CID);
static NS_DEFINE_CID(kNntpServiceCID, NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kNewsFolderResourceCID, NS_NEWSFOLDERRESOURCE_CID);
static NS_DEFINE_CID(kNntpIncomingServerCID, NS_NNTPINCOMINGSERVER_CID);
static NS_DEFINE_CID(kNewsMessageResourceCID, NS_NEWSMESSAGERESOURCE_CID);
static NS_DEFINE_CID(kNNTPNewsgroupCID, NS_NNTPNEWSGROUP_CID);
static NS_DEFINE_CID(kNNTPNewsgroupPostCID, NS_NNTPNEWSGROUPPOST_CID);
static NS_DEFINE_CID(kNNTPNewsgroupListCID, NS_NNTPNEWSGROUPLIST_CID);
static NS_DEFINE_CID(kNNTPArticleListCID, NS_NNTPARTICLELIST_CID);
static NS_DEFINE_CID(kNNTPHostCID, NS_NNTPHOST_CID);

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsMsgNewsFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

	nsMsgNewsFactory(const nsCID &aClass,
                   const char* aClassName,
                   const char* aProgID);

	// nsIFactory methods   
	NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
	NS_IMETHOD LockFactory(PRBool aLock);   

protected:
	virtual ~nsMsgNewsFactory();   

	nsCID mClassID;
	char* mClassName;
	char* mProgID;
};   

nsMsgNewsFactory::nsMsgNewsFactory(const nsCID &aClass,
                           const char* aClassName,
                           const char* aProgID)
  : mClassID(aClass),
    mClassName(nsCRT::strdup(aClassName)),
    mProgID(nsCRT::strdup(aProgID))
{
	NS_INIT_REFCNT();
}   

nsMsgNewsFactory::~nsMsgNewsFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
  
	PL_strfree(mClassName);
	PL_strfree(mProgID);
}   

nsresult nsMsgNewsFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  
	
	// Always NULL result, in case of failure   
	*aResult = NULL;   

	// we support two interfaces....nsISupports and nsFactory.....
	if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))    
		*aResult = (void *)(nsISupports*)this;   
	else if (aIID.Equals(nsIFactory::GetIID()))   
		*aResult = (void *)(nsIFactory*)this;   

	if (*aResult == NULL)
		return NS_NOINTERFACE;

	AddRef(); // Increase reference count for caller   
	return NS_OK;   
}   

NS_IMPL_ADDREF(nsMsgNewsFactory)
NS_IMPL_RELEASE(nsMsgNewsFactory)

nsresult nsMsgNewsFactory::CreateInstance(nsISupports * /* aOuter */,
                                          const nsIID &aIID,
                                          void **aResult)  
{  
	nsresult rv = NS_OK;
  

	if (aResult == nsnull)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = nsnull;  
  
	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
  
	if (mClassID.Equals(kNntpUrlCID)) 
	{		
    nsNntpUrl *url = new nsNntpUrl();
    if (url)
      rv = url->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && url) 
      delete url;
	}
  else if (mClassID.Equals(kNntpServiceCID))
	{
    nsNntpService *service = new nsNntpService();
    if (service)
      rv = service->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && service) 
      delete service;
	}
  else if (mClassID.Equals(kNNTPNewsgroupPostCID))
	{
    nsNNTPNewsgroupPost *newsgroupPost = new nsNNTPNewsgroupPost();
    if (newsgroupPost)
      rv = newsgroupPost->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && newsgroupPost) 
      delete newsgroupPost;
	}
  else if (mClassID.Equals(kNNTPArticleListCID))
	{
    nsNNTPArticleList *articleList = new nsNNTPArticleList();
    if (articleList)
      rv = articleList->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && articleList) 
      delete articleList;
	}
  else if (mClassID.Equals(kNNTPNewsgroupListCID))
	{
    nsNNTPNewsgroupList *newsgroupList = new nsNNTPNewsgroupList();
    if (newsgroupList)
      rv = newsgroupList->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && newsgroupList) 
      delete newsgroupList;
	}
  else if (mClassID.Equals(kNNTPNewsgroupCID))
	{
    nsNNTPNewsgroup *newsgroup = new nsNNTPNewsgroup();
    if (newsgroup)
      rv = newsgroup->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && newsgroup) 
      delete newsgroup;
	}
 	else if (mClassID.Equals(kNewsFolderResourceCID)) 
	{
    nsMsgNewsFolder *folder = new nsMsgNewsFolder();
    if (folder)
      rv = folder->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && folder) 
      delete folder;
	}
 	else if (mClassID.Equals(kNNTPHostCID)) 
	{
    nsNNTPHost *host = new nsNNTPHost();
    if (host)
      rv = host->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && host) 
      delete host;
	}  
	else if (mClassID.Equals(kNntpIncomingServerCID)) 
	{
    nsNntpIncomingServer *server = new nsNntpIncomingServer();
    if (server)
      rv = server->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && server) 
      delete server;
	}
	else if (mClassID.Equals(kNewsMessageResourceCID)) 
 	{
    nsNewsMessage *message = new nsNewsMessage();
    if (message)
      rv = message->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;
    
    if (NS_FAILED(rv) && message) 
      delete message;
 	}
	else
		return NS_NOINTERFACE;

	return rv;
}  

nsresult nsMsgNewsFactory::LockFactory(PRBool aLock)  
{  
	if (aLock)
		PR_AtomicIncrement(&g_LockCount); 
	else
		PR_AtomicDecrement(&g_LockCount);

	return NS_OK;
}  

////////////////////////////////////////////////////////////////////////////////

// return the proper factory to the caller. 
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* /* aServMgr */,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;
	
	*aFactory = new nsMsgNewsFactory(aClass, aClassName, aProgID);
	if (aFactory)
		return (*aFactory)->QueryInterface(nsIFactory::GetIID(),(void**)aFactory);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* /* aServMgr */) 
{
	return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult rv = NS_OK;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;
  
	rv = compMgr->RegisterComponent(kNntpUrlCID,
                                  "NNTP Url",
                                  "component://netscape/messenger/nntpurl",
                                  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kNntpServiceCID, "NNTP Service", 
									"component://netscape/messenger/nntpservice", 
									path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kNntpServiceCID, "NNTP News Service", 
                                  "component://netscape/messenger/messageservice;type=news", 
                                  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kNntpServiceCID, "NNTP News Message Service",
                                  "component://netscape/messenger/messageservice;type=news_message", 
                                  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kNntpServiceCID,  
                                    "NNTP Protocol Handler",
                                    NS_NETWORK_PROTOCOL_PROGID_PREFIX "news",
                                    path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kNewsFolderResourceCID,
                                  "News Folder Resource Factory",
                                  NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "news",
                                  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;
  

	rv = compMgr->RegisterComponent(kNewsMessageResourceCID,
									"News Resource Factory",
									NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "news_message",
									path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kNntpIncomingServerCID,
									"Nntp Incoming Server",
									"component://netscape/messenger/server&type=nntp",
									path, PR_TRUE, PR_TRUE);
                                  
	if (NS_FAILED(rv)) return rv;
  
	rv = compMgr->RegisterComponent(kNNTPNewsgroupCID,
                                  "NNTP Newsgroup",
                                  "component://netscape/messenger/nntpnewsgroup",
                                  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kNNTPNewsgroupPostCID,
                                  "NNTP Newsgroup Post",
                                  "component://netscape/messenger/nntpnewsgrouppost",
                                  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kNNTPNewsgroupListCID,
                                  "NNTP Newsgroup List",
                                  "component://netscape/messenger/nntpnewsgrouplist",
                                  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kNNTPArticleListCID,
                                  "NNTP Article List",
                                  "component://netscape/messenger/nntparticlelist",
                                  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kNNTPHostCID,
                                  "NNTP Host",
                                  "component://netscape/messenger/nntphost",
                                  path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) return rv;
  
	return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult rv = NS_OK;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	NS_WITH_SERVICE(nsIComponentManager,compMgr,kComponentManagerCID,&rv); 
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kNntpUrlCID, path);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kNntpServiceCID, path);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kNewsFolderResourceCID, path);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kNntpIncomingServerCID, path);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kNewsMessageResourceCID, path);
	if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kNNTPNewsgroupCID, path);
	if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kNNTPNewsgroupPostCID, path);
	if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kNNTPNewsgroupListCID, path);
	if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kNNTPArticleListCID, path);
	if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kNNTPHostCID, path);
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
