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

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kNntpUrlCID, NS_NNTPURL_CID);
static NS_DEFINE_CID(kNntpServiceCID, NS_NNTPSERVICE_CID);
static NS_DEFINE_CID(kNewsFolderResourceCID, NS_NEWSFOLDERRESOURCE_CID);
static NS_DEFINE_CID(kNntpIncomingServerCID, NS_NNTPINCOMINGSERVER_CID);

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsMsgNewsFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

	nsMsgNewsFactory(const nsCID &aClass,
		             const char* aClassName,
                     const char* aProgID,
					 nsISupports*);

	// nsIFactory methods   
	NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
	NS_IMETHOD LockFactory(PRBool aLock);   

protected:
	virtual ~nsMsgNewsFactory();   

	nsCID mClassID;
	char* mClassName;
	char* mProgID;
	nsIServiceManager* mServiceManager;
};   

nsMsgNewsFactory::nsMsgNewsFactory(const nsCID &aClass,
                           const char* aClassName,
                           const char* aProgID,
                           nsISupports *compMgrSupports)
  : mClassID(aClass),
    mClassName(nsCRT::strdup(aClassName)),
    mProgID(nsCRT::strdup(aProgID))
{
	NS_INIT_REFCNT();

#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFactory::nsMsgNewsFactory()\n");
#endif

	compMgrSupports->QueryInterface(nsIServiceManager::GetIID(), (void **)&mServiceManager);
}   

nsMsgNewsFactory::~nsMsgNewsFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
  
	NS_IF_RELEASE(mServiceManager);
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
	if (aIID.Equals(::nsISupports::GetIID()))    
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

nsresult nsMsgNewsFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **aResult)  
{  
#ifdef DEBUG_sspitzer
  printf("nsMsgNewsFactory::CreateInstance()\n");
#endif
	nsresult rv = NS_OK;

	if (aResult == nsnull)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = nsnull;  
  
	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!

	// do they want a news datasource ?
	if (mClassID.Equals(kNntpUrlCID)) 
	{		
    nsNntpUrl * nntpUrl = new nsNntpUrl(nsnull, nsnull);
		if (nntpUrl)
			rv = nntpUrl->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;
		
		if (NS_FAILED(rv) && nntpUrl)
			delete nntpUrl;
	}
	else if (mClassID.Equals(kNntpServiceCID))
	{
		nsNntpService *nntpService = new nsNntpService();
    if (nntpService)
      rv = nntpService->QueryInterface(aIID, aResult);
    else
      rv = NS_ERROR_OUT_OF_MEMORY;

    if (NS_FAILED(rv) && nntpService)
      delete nntpService;
	}
 	else if (mClassID.Equals(kNewsFolderResourceCID)) 
	{
		nsMsgNewsFolder *newsFolder = new nsMsgNewsFolder();
		if (newsFolder)
			rv = newsFolder->QueryInterface(aIID, aResult);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;

		if (NS_FAILED(rv) && newsFolder)
			delete newsFolder;
  } 
  else if (mClassID.Equals(kNntpIncomingServerCID)) {
    rv = NS_NewNntpIncomingServer(nsISupports::GetIID(), aResult);
  }
  else
    rv = NS_NOINTERFACE;

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
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;
	
	*aFactory = new nsMsgNewsFactory(aClass, aClassName, aProgID, aServMgr);
	if (aFactory)
		return (*aFactory)->QueryInterface(nsIFactory::GetIID(),(void**)aFactory);
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr) 
{
	return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

////////////////////////////////////////////////////////////////////////////////

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	nsIComponentManager* compMgr;
	rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kNntpUrlCID, nsnull, nsnull, path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->RegisterComponent(kNntpServiceCID, nsnull, nsnull, path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kNewsFolderResourceCID,
                                  "News Folder Resource Factory",
                                  NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "news",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
#ifdef NS_DEBUG
	printf("news registering from %s\n",path);
#endif

  rv = compMgr->RegisterComponent(kNntpIncomingServerCID,
                                  "Nntp Incoming Server",
                                  "component://netscape/messenger/server&type=nntp",
                                  path, PR_TRUE, PR_TRUE);
                                  
  if (NS_FAILED(rv)) goto done;

	done:
	(void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
	return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult rv;

	nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
	if (NS_FAILED(rv)) return rv;

	nsIComponentManager* compMgr;
	rv = servMgr->GetService(kComponentManagerCID, nsIComponentManager::GetIID(), (nsISupports**)&compMgr);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kNntpUrlCID, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterComponent(kNntpServiceCID, path);
	if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kNewsFolderResourceCID, path);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kNntpIncomingServerCID, path);
  if (NS_FAILED(rv)) goto done;

done:
	(void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
	return rv;
}

////////////////////////////////////////////////////////////////////////////////
