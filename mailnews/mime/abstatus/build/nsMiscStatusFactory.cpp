/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIRegistry.h"
#include "msgCore.h"
#include "nsCOMPtr.h"
#include "pratom.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsMimeMiscStatus.h"
#include "nsIMimeMiscStatus.h"
#include "nsIMimeEmitter.h"

#define     STATUS_ID_NAME    "ABOOK"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kMimeMiscStatusCID, NS_MIME_MISC_STATUS_CID);
static NS_DEFINE_CID(kRegistryCID, NS_REGISTRY_CID);


////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsMimeMiscStatusFactory : public nsIFactory
{   
public:
   // nsISupports methods
   NS_DECL_ISUPPORTS 

   nsMimeMiscStatusFactory(const nsCID &aClass,
               const char* aClassName,
               const char* aContractID,
               nsISupports*);

   // nsIFactory methods   
   NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
   NS_IMETHOD LockFactory(PRBool aLock);   

protected:
	virtual ~nsMimeMiscStatusFactory();   

	nsCID mClassID;
	char* mClassName;
	char* mContractID;
};   

nsMimeMiscStatusFactory::nsMimeMiscStatusFactory(const nsCID &aClass,
                           const char* aClassName,
                           const char* aContractID,
                           nsISupports *compMgrSupports)
  : mClassID(aClass),
    mClassName(nsCRT::strdup(aClassName)),
    mContractID(nsCRT::strdup(aContractID))
{
	NS_INIT_REFCNT();
}   

nsMimeMiscStatusFactory::~nsMimeMiscStatusFactory()   
{
	PL_strfree(mClassName);
	PL_strfree(mContractID);
}   

nsresult nsMimeMiscStatusFactory::QueryInterface(const nsIID &aIID, void **aResult)   
{   
  if (aResult == NULL)  
    return NS_ERROR_NULL_POINTER;  

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  // we support two interfaces....nsISupports and nsFactory.....
  if (aIID.Equals(NS_GET_IID(nsISupports)))    
    *aResult = (void *)(nsISupports*)this;   
  else if (aIID.Equals(NS_GET_IID(nsIFactory)))   
    *aResult = (void *)(nsIFactory*)this;   

  if (*aResult == NULL)
    return NS_NOINTERFACE;

  AddRef(); // Increase reference count for caller   
  return NS_OK;   
}   

NS_IMPL_ADDREF(nsMimeMiscStatusFactory)
NS_IMPL_RELEASE(nsMimeMiscStatusFactory)

nsresult nsMimeMiscStatusFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **aResult)  
{  
	nsresult res = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	//nsISupports *inst = nsnull;

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
	// do they want a mime emitter interface ?
	if (mClassID.Equals(kMimeMiscStatusCID)) 
		res = NS_NewMimeMiscStatus(aIID, aResult);
	return res;  
}  

nsresult
nsMimeMiscStatusFactory::LockFactory(PRBool aLock)  
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
                                           const char *aContractID,
                                           nsIFactory **aFactory)
{
	if (nsnull == aFactory)
		return NS_ERROR_NULL_POINTER;

  *aFactory = new nsMimeMiscStatusFactory(aClass, aClassName, aContractID, aServMgr);
  if (aFactory)
    return (*aFactory)->QueryInterface(NS_GET_IID(nsIFactory),
                                       (void**)aFactory);
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
  nsresult rv = NS_OK;
  nsresult finalResult = NS_OK;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCAutoString actualContractID(NS_IMIME_MISC_STATUS_KEY);
  actualContractID.Append(STATUS_ID_NAME);
  rv = compMgr->RegisterComponent(kMimeMiscStatusCID,
                                  "Mime Misc Status",
                                  actualContractID,
                                  path, PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) finalResult = rv;

  // register the TestConverter with the registry. One contractid registration
  // per conversion ability.
  NS_WITH_SERVICE(nsIRegistry, registry, kRegistryCID, &rv);
  if (NS_SUCCEEDED(rv))
  {  
    // open the registry
    rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    if (NS_SUCCEEDED(rv)) 
    {
      // set the key
      nsRegistryKey key, key1;  
      rv = NS_OK;
      if (NS_FAILED(registry->GetSubtree(nsIRegistry::Common, NS_IMIME_MISC_STATUS_KEY, &key)))
        rv = registry->AddSubtree(nsIRegistry::Common, NS_IMIME_MISC_STATUS_KEY, &key);
      if (NS_SUCCEEDED(rv)) 
      {
        rv = registry->AddSubtreeRaw(key, STATUS_ID_NAME, &key1);
      }
    }
  }

  return finalResult;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
	nsresult rv;
	nsresult finalResult = NS_OK;

	NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
	if (NS_FAILED(rv)) return rv;

	rv = compMgr->UnregisterComponent(kMimeMiscStatusCID, path);
	if (NS_FAILED(rv)) finalResult = rv;

	return finalResult;
}

////////////////////////////////////////////////////////////////////////////////
