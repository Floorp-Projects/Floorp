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

#include "msgCore.h" // for pre-compiled headers...
#include "nsIServiceManager.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIMAPHostSessionList.h"
#include "nsImapIncomingServer.h"
#include "nsImapService.h"
#include "pratom.h"
#include "nsCOMPtr.h"
#include "nsImapMailFolder.h"
#include "nsImapMessage.h"

// include files for components this factory creates...
#include "nsImapUrl.h"
#include "nsImapProtocol.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kCImapUrl, NS_IMAPURL_CID);
static NS_DEFINE_CID(kCImapProtocol, NS_IMAPPROTOCOL_CID);
static NS_DEFINE_CID(kCImapHostSessionList, NS_IIMAPHOSTSESSIONLIST_CID);
static NS_DEFINE_CID(kCImapIncomingServer, NS_IMAPINCOMINGSERVER_CID);
static NS_DEFINE_CID(kCImapService, NS_IMAPSERVICE_CID);
static NS_DEFINE_CID(kCImapResource, NS_IMAPRESOURCE_CID);
static NS_DEFINE_CID(kCImapMessageResource, NS_IMAPMESSAGERESOURCE_CID);


////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsImapFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

	nsImapFactory(const nsCID &aClass, const char* aClassName, const char* aProgID); 

	// nsIFactory methods   
	NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
	NS_IMETHOD LockFactory(PRBool aLock);   

protected:
	virtual ~nsImapFactory();   

	nsCID mClassID;
	char* mClassName;
	char* mProgID;
};   

nsImapFactory::nsImapFactory(const nsCID &aClass, const char* aClassName, const char* aProgID)
  : mClassID(aClass), mClassName(nsCRT::strdup(aClassName)), mProgID(nsCRT::strdup(aProgID))
{   
	NS_INIT_REFCNT();
}   

nsImapFactory::~nsImapFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
	delete[] mClassName;
	delete[] mProgID;
}   

nsresult nsImapFactory::QueryInterface(const nsIID &aIID, void **aResult)   
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


NS_IMPL_ADDREF(nsImapFactory)
NS_IMPL_RELEASE(nsImapFactory)

nsresult nsImapFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
{  
	nsresult rv = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	nsISupports *inst = nsnull;

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
	// do they want a local datasource ?
	if (mClassID.Equals(kCImapUrl)) 
	{
		inst = NS_STATIC_CAST(nsIImapUrl*, new nsImapUrl());
	}
	else if (mClassID.Equals(kCImapProtocol))
	{
		inst = NS_STATIC_CAST(nsIImapProtocol *, new nsImapProtocol());
	}
	else if (mClassID.Equals(kCImapHostSessionList))
	{
		inst = NS_STATIC_CAST(nsIImapHostSessionList *, new nsIMAPHostSessionList());
	}
	else if (mClassID.Equals(kCImapIncomingServer))
	{
		return NS_NewImapIncomingServer(aIID, aResult);
	}
	else if (mClassID.Equals(kCImapService))
	{
		inst = NS_STATIC_CAST(nsIImapService *, new nsImapService());
	}
	else if (mClassID.Equals(kCImapMessageResource)) 
 	{
 		inst = NS_STATIC_CAST(nsIMessage*, new nsImapMessage());
 	}
	else if (mClassID.Equals(kCImapResource))
	{
		inst = NS_STATIC_CAST(nsIMsgImapMailFolder *, new nsImapMailFolder());
	}
	if (inst == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	rv = inst->QueryInterface(aIID, aResult);
	if (NS_FAILED(rv))
		delete inst;
	return rv;
}  

nsresult nsImapFactory::LockFactory(PRBool aLock)  
{  
	if (aLock)
		PR_AtomicIncrement(&g_LockCount); 
	else
		PR_AtomicDecrement(&g_LockCount); 

  return NS_OK;
}  

// return the proper factory to the caller. 
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports* aServMgr,
                                           const nsCID &aClass,
                                           const char *aClassName,
                                           const char *aProgID,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

	*aFactory = new nsImapFactory(aClass, aClassName, aProgID);

	if (aFactory)
		return (*aFactory)->QueryInterface(nsIFactory::GetIID(), (void**)aFactory); // they want a Factory Interface so give it to them
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr) 
{
    return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

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

	rv = compMgr->RegisterComponent(kCImapUrl, nsnull, nsnull,
                                    path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->RegisterComponent(kCImapProtocol, nsnull, nsnull,
									path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->RegisterComponent(kCImapHostSessionList, nsnull, nsnull,
									path, PR_TRUE, PR_TRUE);
	if (NS_FAILED(rv)) goto done;

	
	rv = compMgr->RegisterComponent(kCImapIncomingServer,
									"Imap Incoming Server",
									"component://netscape/messenger/server&type=imap",
									path, PR_TRUE, PR_TRUE);

	if (NS_FAILED(rv)) goto done;

  // register our RDF resource factories:
  rv = compMgr->RegisterComponent(kCImapResource,
                                  "Mail/News Imap Resource Factory",
                                  NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "imap",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;


//  rv = compMgr->RegisterComponent(kImapServiceCID, nsnull, 
//								  "component://netscape/messenger/messageservice;type=imap_message", 
  //                                path, PR_TRUE, PR_TRUE);

  if (NS_FAILED(rv)) goto done;

	rv = compMgr->RegisterComponent(kCImapService, nsnull, nsnull,
									path, PR_TRUE, PR_TRUE);

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
	rv = servMgr->GetService(kComponentManagerCID, 
                             nsIComponentManager::GetIID(), 
                             (nsISupports**)&compMgr);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterFactory(kCImapUrl, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterFactory(kCImapProtocol, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterFactory(kCImapHostSessionList, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterFactory(kCImapIncomingServer, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterFactory(kCImapService, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterComponent(kCImapMessageResource, path);
	if (NS_FAILED(rv)) goto done;

	rv = compMgr->UnregisterFactory(kCImapResource, path);
	if (NS_FAILED(rv)) goto done;

done:
	(void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
	return rv;
}
