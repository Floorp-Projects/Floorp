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
#include "nsMsgLocalCID.h"
#include "pratom.h"

// include files for components this factory creates...
#include "nsMailboxUrl.h"
#include "nsMSGFolderDataSource.h"
#include "nsMailboxService.h"
#include "nsLocalMailFolder.h"
#include "nsParseMailbox.h"
#include "nsPop3Service.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kCMailboxUrl, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kCMailboxParser, NS_MAILBOXPARSER_CID);
static NS_DEFINE_CID(kCMailboxService, NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kMailNewsDatasourceCID, NS_MAILNEWSDATASOURCE_CID);
static NS_DEFINE_CID(kMailNewsResourceCID, NS_MAILNEWSRESOURCE_CID);
static NS_DEFINE_CID(kMailNewsMessageResourceCID, NS_MAILNEWSMESSAGERESOURCE_CID);
static NS_DEFINE_CID(kPop3ServiceCID, NS_POP3SERVICE_CID);

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsMsgLocalFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

  nsMsgLocalFactory(const nsCID &aClass, const char* aClassName, const char* aProgID); 

  // nsIFactory methods   
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
  NS_IMETHOD LockFactory(PRBool aLock);   

protected:
  virtual ~nsMsgLocalFactory();   

  nsCID mClassID;
  char* mClassName;
  char* mProgID;
};   

nsMsgLocalFactory::nsMsgLocalFactory(const nsCID &aClass, const char* aClassName, const char* aProgID)
  : mClassID(aClass), mClassName(nsCRT::strdup(aClassName)), mProgID(nsCRT::strdup(aProgID))
{   
	NS_INIT_REFCNT();
}   

nsMsgLocalFactory::~nsMsgLocalFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
  delete[] mClassName;
  delete[] mProgID;
}   

nsresult nsMsgLocalFactory::QueryInterface(const nsIID &aIID, void **aResult)   
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

NS_IMPL_ADDREF(nsMsgLocalFactory)
NS_IMPL_RELEASE(nsMsgLocalFactory)

nsresult nsMsgLocalFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
{  
	nsresult rv = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	nsISupports *inst = nsnull;

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
	// do they want a local datasource ?
	if (mClassID.Equals(kCMailboxUrl)) 
	{
		inst = NS_STATIC_CAST(nsIMailboxUrl*, new nsMailboxUrl(nsnull, nsnull));
	}
	else if (mClassID.Equals(kCMailboxParser)) 
	{
		inst = new nsMsgMailboxParser();
	}
	else if (mClassID.Equals(kCMailboxService)) 
	{
		inst = new nsMailboxService();
	}
	else if (mClassID.Equals(kPop3ServiceCID))
	{
		inst = new nsPop3Service();
	}
	else if (mClassID.Equals(kMailNewsDatasourceCID)) 
	{
		inst = NS_STATIC_CAST(nsIRDFMSGFolderDataSource*, new nsMSGFolderDataSource());
	}
	else if (mClassID.Equals(kMailNewsResourceCID)) 
	{
		inst = NS_STATIC_CAST(nsIMsgLocalMailFolder*, new nsMsgLocalMailFolder());
	}
	else if (mClassID.Equals(kMailNewsMessageResourceCID)) 
	{
		inst = NS_STATIC_CAST(nsIMessage*, new nsMsgHdr());
	}
	
	if (inst == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

  rv = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(rv))
    delete inst;
  return rv;
}  

nsresult nsMsgLocalFactory::LockFactory(PRBool aLock)  
{  
	if (aLock) { 
		PR_AtomicIncrement(&g_LockCount); 
	} else { 
		PR_AtomicDecrement(&g_LockCount); 
	} 

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

	// If we decide to implement multiple factories in the msg.dll, then we need to check the class
	// type here and create the appropriate factory instead of always creating a nsMsgFactory...
	*aFactory = new nsMsgLocalFactory(aClass, aClassName, aProgID);

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

  rv = compMgr->RegisterComponent(kCMailboxUrl, nsnull, nsnull,
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kCMailboxService, nsnull, nsnull, 
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kCMailboxParser, nsnull, nsnull,
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kPop3ServiceCID, nsnull, nsnull,
								  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  // register our RDF datasources:
  rv = compMgr->RegisterComponent(kMailNewsDatasourceCID, 
                                  "Mail/News Data Source",
                                  NS_RDF_DATASOURCE_PROGID_PREFIX "mailnews",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  // register our RDF resource factories:
  rv = compMgr->RegisterComponent(kMailNewsResourceCID,
                                  "Mail/News Resource Factory",
                                  NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "mailbox",
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->RegisterComponent(kMailNewsMessageResourceCID,
                                  "Mail/News Resource Factory",
                                  NS_RDF_RESOURCE_FACTORY_PROGID_PREFIX "mailbox_message",
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
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterFactory(kCMailboxUrl, path);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterFactory(kCMailboxService, path);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterFactory(kPop3ServiceCID, path);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterFactory(kCMailboxParser, path);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kMailNewsDatasourceCID, path);
  if (NS_FAILED(rv)) goto done;

  rv = compMgr->UnregisterComponent(kMailNewsResourceCID, path);
  if (NS_FAILED(rv)) goto done;

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}
