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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsIServiceManager.h"
#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "pratom.h"
#include "mdb.h"

// include files for components this factory creates...
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kCMorkFactory, NS_MORK_CID);
////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsMorkFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

  nsMorkFactory(const nsCID &aClass, const char* aClassName, const char* aProgID); 

  // nsIFactory methods   
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
  NS_IMETHOD LockFactory(PRBool aLock);   

protected:
  virtual ~nsMorkFactory();   

  nsCID mClassID;
  char* mClassName;
  char* mProgID;
};   

class nsMorkFactoryFactory : public nsIMdbFactoryFactory
{
public:
	nsMorkFactoryFactory();
	// nsISupports methods
	NS_DECL_ISUPPORTS 

	NS_IMETHOD GetMdbFactory(nsIMdbFactory **aFactory);

};

nsMorkFactory::nsMorkFactory(const nsCID &aClass, const char* aClassName, const char* aProgID)
  : mClassID(aClass), mClassName(nsCRT::strdup(aClassName)), mProgID(nsCRT::strdup(aProgID))
{   
	NS_INIT_REFCNT();
}   

nsMorkFactory::~nsMorkFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
  PL_strfree(mClassName);
  PL_strfree(mProgID);
}   

nsresult nsMorkFactory::QueryInterface(const nsIID &aIID, void **aResult)   
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

NS_IMPL_ADDREF(nsMorkFactory)
NS_IMPL_RELEASE(nsMorkFactory)

// OK, we're cheating here, since Mork doesn't support XPCOM, i.e., nsIMDBFactory
// doesn't inherit from nsISupports. I could create a wrapper interface for an nsIMDBFactory
// object that returns a real nsISupports object, and msgdb could use this object
// to get hold of the actual nsIMDBFactory interface object.
nsresult nsMorkFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
{  
	nsresult rv = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
	// do they want a mork factory  ?
	if (mClassID.Equals(kCMorkFactory)) 
	{
		*aResult = MakeMdbFactory();
	}
	return rv;
}  

nsresult nsMorkFactory::LockFactory(PRBool aLock)  
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
	*aFactory = new nsMorkFactory(aClass, aClassName, aProgID);

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
  nsresult rv = NS_OK;
  nsresult finalResult = NS_OK;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);

  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kCMorkFactory, nsnull, nsnull,
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv))finalResult = rv;

  return finalResult;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
  nsresult rv = NS_OK;
  nsresult finalResult = NS_OK;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kCMorkFactory, path);
  if (NS_FAILED(rv)) finalResult = rv;

  return finalResult;
}

static nsIMdbFactory *gMDBFactory = nsnull;

NS_IMPL_ADDREF(nsMorkFactoryFactory)
NS_IMPL_RELEASE(nsMorkFactoryFactory)

NS_IMETHODIMP
nsMorkFactoryFactory::QueryInterface(REFNSIID iid, void** result)
{
	if (! result)
		return NS_ERROR_NULL_POINTER;

	*result = nsnull;
    if(iid.Equals(nsIMdbFactoryFactory::GetIID()) ||
		iid.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
		*result = NS_STATIC_CAST(nsIMdbFactoryFactory*, this);
		AddRef();
		return NS_OK;
	}
    return NS_NOINTERFACE;
}



nsMorkFactoryFactory::nsMorkFactoryFactory()
{
	NS_INIT_REFCNT();
}

NS_IMETHODIMP nsMorkFactoryFactory::GetMdbFactory(nsIMdbFactory **aFactory)
{
	if (!gMDBFactory)
		gMDBFactory = MakeMdbFactory();
	*aFactory = gMDBFactory;
	return (gMDBFactory) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

