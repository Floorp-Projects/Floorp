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


#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsMsgAppCore.h"

#include "nsRepository.h"

#include "pratom.h"

static NS_DEFINE_CID(kCMsgAppCoreCID, NS_MSGAPPCORE_CID);

static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;


class nsMessengerFactory : public nsIFactory {
public:
    // nsISupports methods
    NS_DECL_ISUPPORTS 
    
    nsMessengerFactory(const nsCID &aClass); 
    
    // nsIFactory methods   
    NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
    NS_IMETHOD LockFactory(PRBool aLock);   
    
protected:
    virtual ~nsMessengerFactory();   
    
private:  
    nsCID     mClassID;
};   

nsMessengerFactory::nsMessengerFactory(const nsCID &aClass)   
{   
	NS_INIT_REFCNT();
    PR_AtomicIncrement(&g_InstanceCount);
	mClassID = aClass;
}   

nsMessengerFactory::~nsMessengerFactory()   
{   
    PR_AtomicDecrement(&g_InstanceCount);
	NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");   
}   


nsresult
nsMessengerFactory::QueryInterface(const nsIID &aIID, void **aResult)   
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

NS_IMPL_ADDREF(nsMessengerFactory)
NS_IMPL_RELEASE(nsMessengerFactory)

nsresult
nsMessengerFactory::CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult)  
{  
	nsresult res = NS_OK;
	nsISupports *inst = nsnull;
    
	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  

    // get the interface this factory instance is supposed to create
    if (mClassID.Equals(kCMsgAppCoreCID)) {
        res = NS_NewMsgAppCore((nsIDOMMsgAppCore **)&inst);
    }

    // get the requested interface on the newly created class
    if (NS_SUCCEEDED(res)) {
        res = inst->QueryInterface(aIID, aResult);
        NS_RELEASE(inst);
    }
        
    return res;
}


nsresult nsMessengerFactory::LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
    return NS_OK;
}  



//
// begin DLL exports
//
nsresult
NSGetFactory(const nsCID &aClass,
             nsISupports *serviceMgr,
             nsIFactory **aFactory)
{
#ifdef DEBUG_alecf
    printf("messenger: NSGetFactory()\n");
#endif
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

    // for now this factory implements only the messenger CID.
    *aFactory = new nsMessengerFactory(aClass);

	if (aFactory)
		return (*aFactory)->QueryInterface(nsIFactory::IID(), (void**)aFactory); // they want a Factory Interface so give it to them
	else
		return NS_ERROR_OUT_OF_MEMORY;
}


PRBool
NSCanUnload()
{
#ifdef DEBUG_alecf
    printf("messenger: NSCanUnload()\n");
#endif
    return PRBool(g_InstanceCount == 0 && g_LockCount == 0);
}

nsresult
NSRegisterSelf(const char *fullpath)
{
#ifdef DEBUG_alecf
    printf("messenger: NSRegisterSelf()\n");
#endif
    return nsRepository::RegisterFactory(kCMsgAppCoreCID, fullpath,
                                         PR_TRUE, PR_TRUE);
}
    
nsresult
NSUnregisterSelf(const char *fullpath)
{
#ifdef DEBUG_alecf
    printf("messenger: NSUnregisterSelf()\n");
#endif
    return nsRepository::UnregisterFactory(kCMsgAppCoreCID, fullpath);
}
