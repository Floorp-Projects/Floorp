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
#include "nsMsgLocalCID.h"
#include "pratom.h"
#include "nsRepository.h"
#include "nsIRDFMSGFolderDataSource.h"


static NS_DEFINE_CID(kCMsgLocalFactory, NS_MSGLOCALDATASOURCE_CID);

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

    nsMsgLocalFactory(const nsCID &aClass); 

    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
    NS_IMETHOD LockFactory(PRBool aLock);   

  protected:
    virtual ~nsMsgLocalFactory();   

  private:  
    nsCID     mClassID;
};   

nsMsgLocalFactory::nsMsgLocalFactory(const nsCID &aClass)   
{   
	NS_INIT_REFCNT();
	mClassID = aClass;
}   

nsMsgLocalFactory::~nsMsgLocalFactory()   
{
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   

nsresult nsMsgLocalFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
  if (aResult == NULL)  
    return NS_ERROR_NULL_POINTER;  

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  // we support two interfaces....nsISupports and nsFactory.....
  if (aIID.Equals(::nsISupports::IID()))    
    *aResult = (void *)(nsISupports*)this;   
  else if (aIID.Equals(nsIFactory::IID()))   
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
	nsresult res = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	nsISupports *inst = nsnull;

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
	// do they want a local datasource ?
	if (mClassID.Equals(kCMsgLocalFactory))
	{
		res = NS_NewRDFMSGFolderDataSource((nsIRDFDataSource **) &inst);
		if (NS_FAILED(res))  // was there a problem creating the object ?
		  return res;   
	}

	// End of checking the interface ID code....
	if (inst)
	{
		// so we now have the class that supports the desired interface...we need to turn around and
		// query for our desired interface.....
		res = inst->QueryInterface(aIID, aResult);
		NS_RELEASE(inst);
		if (res != NS_OK)  // if the query interface failed for some reason, then the object did not get ref counted...delete it.
			delete inst; 
	}
	else
		res = NS_ERROR_OUT_OF_MEMORY;

  return res;  
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
extern "C" NS_EXPORT nsresult NSGetFactory(const nsCID &aClass,
                                           nsISupports *serviceMgr,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

	// If we decide to implement multiple factories in the msg.dll, then we need to check the class
	// type here and create the appropriate factory instead of always creating a nsMsgFactory...
	*aFactory = new nsMsgLocalFactory(aClass);

	if (aFactory)
		return (*aFactory)->QueryInterface(nsIFactory::IID(), (void**)aFactory); // they want a Factory Interface so give it to them
	else
		return NS_ERROR_OUT_OF_MEMORY;
}

extern "C" NS_EXPORT PRBool NSCanUnload() 
{
    return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(const char* path)
{
  nsresult ret;

  ret = nsRepository::RegisterFactory(kCMsgLocalFactory, path, PR_TRUE,
    PR_TRUE);
  if (NS_FAILED(ret)) {
    return ret;
  }

  return ret;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(const char* path)
{
  nsresult ret;

  ret = nsRepository::UnregisterFactory(kCMsgLocalFactory, path);
  if (NS_FAILED(ret)) {
    return ret;
  }

  return ret;
}

