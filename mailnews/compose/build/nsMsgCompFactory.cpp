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

#include "msgCore.h"

#include "nsIFactory.h"
#include "nsISupports.h"
#include "pratom.h"
#include "nsMsgCompCID.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsISmtpService.h"
#include "nsSmtpService.h"
#include "nsMsgComposeFact.h"
#include "nsMsgCompFieldsFact.h"
#include "nsMsgSendFact.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_CID(kCMsgComposeCID, NS_MSGCOMPOSE_CID);
static NS_DEFINE_CID(kCMsgCompFieldsCID, NS_MSGCOMPFIELDS_CID);
static NS_DEFINE_CID(kCMsgSendCID, NS_MSGSEND_CID);
static NS_DEFINE_CID(kCSmtpServiceCID, NS_SMTPSERVICE_CID);

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsMsgComposeFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

    nsMsgComposeFactory(const nsCID &aClass); 

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
    NS_IMETHOD LockFactory(PRBool aLock);   

  protected:
    virtual ~nsMsgComposeFactory();   

  private:  
    nsCID     mClassID;
};   

nsMsgComposeFactory::nsMsgComposeFactory(const nsCID &aClass)   
{   
	NS_INIT_REFCNT();
	mClassID = aClass;
}   

nsMsgComposeFactory::~nsMsgComposeFactory()   
{   
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsMsgComposeFactory::QueryInterface(const nsIID &aIID, void **aResult)   
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

NS_IMPL_ADDREF(nsMsgComposeFactory)
NS_IMPL_RELEASE(nsMsgComposeFactory)

nsresult nsMsgComposeFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
{  
	nsresult res = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	nsISupports *inst = nsnull;

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!

	// mscott --> I've noticed that all of the "convience" functions (NS_*) used to 
	// create components are adding a ref count to the returned component...then at the end of
	// CreateInstance we ref count again when we query interface for the desired interface. 
	// That's why we need to be careful and make sure we release the first ref count before returning...
	// So, if you add a component here and it doesn't ref count when you create it then you might
	// have a problem....
	
	if (mClassID.Equals(kCSmtpServiceCID))
	{
		nsSmtpService * smtpService = new nsSmtpService();
		// okay now turn around and give inst a handle on it....
		smtpService->QueryInterface(kISupportsIID, (void **) &inst);
	}
	// do they want an Message Compose interface ?
	if (mClassID.Equals(kCMsgComposeCID))
	{
		res = NS_NewMsgCompose((nsIMsgCompose **) &inst);
		if (res != NS_OK)  // was there a problem creating the object ?
		  return res;   
	}

	// do they want an Message Compose Fields interface ?
	if (mClassID.Equals(kCMsgCompFieldsCID))
	{
		res = NS_NewMsgCompFields((nsIMsgCompFields **) &inst);
		if (res != NS_OK)  // was there a problem creating the object ?
		  return res;   
	}

	// do they want an Message Send interface ?
	if (mClassID.Equals(kCMsgSendCID))
	{
		res = NS_NewMsgSend((nsIMsgSend **) &inst);
		if (res != NS_OK)  // was there a problem creating the object ?
		  return res;  
	}

	// End of checking the interface ID code....
	if (inst)
	{
		// so we now have the class that supports the desired interface...we need to turn around and
		// query for our desired interface.....
		res = inst->QueryInterface(aIID, aResult);
		NS_RELEASE(inst); // because creating picked up an extra ref count...
		if (res != NS_OK)  // if the query interface failed for some reason, then the object did not get ref counted...delete it.
			delete inst;
	}
	else
		res = NS_ERROR_OUT_OF_MEMORY;

  return res;  
}  

nsresult nsMsgComposeFactory::LockFactory(PRBool aLock)  
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

	// If we decide to implement multiple factories in the msg.dll, then we need to check the class
	// type here and create the appropriate factory instead of always creating a nsMsgComposeFactory...
	*aFactory = new nsMsgComposeFactory(aClass);

	if (aFactory)
		return (*aFactory)->QueryInterface(nsIFactory::GetIID(), (void**)aFactory); // they want a Factory Interface so give it to them
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

extern "C" NS_EXPORT PRBool NSCanUnload(nsISupports* aServMgr) 
{
    return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char* path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

	rv = compMgr->RegisterComponent(kCMsgComposeCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
	rv = compMgr->RegisterComponent(kCSmtpServiceCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
	rv = compMgr->RegisterComponent(kCMsgCompFieldsCID, NULL, NULL, path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
	rv = compMgr->RegisterComponent(kCMsgSendCID, NULL, NULL, path, PR_TRUE, PR_TRUE);

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

	rv = compMgr->UnregisterComponent(kCMsgComposeCID, path);
  if (NS_FAILED(rv)) goto done;
	rv = compMgr->UnregisterComponent(kCSmtpServiceCID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kCMsgCompFieldsCID, path);
  if (NS_FAILED(rv)) goto done;
	rv = compMgr->UnregisterComponent(kCMsgSendCID, path);

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}
