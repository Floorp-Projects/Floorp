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
#include "msgCore.h"
#include "nsCOMPtr.h"
#include "pratom.h"
#include "nsSMIMEStub.h"

/* Include all of the interfaces our factory can generate components for */
#include "nsIMimeContentTypeHandler.h"
#include "nsMimeContentTypeHandler.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kMimeContentTypeHandlerCID, NS_SMIME_CONTENT_TYPE_HANDLER_CID);

////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////
static PRInt32 g_InstanceCount = 0;
static PRInt32 g_LockCount = 0;

class nsSMIMEStubFactory : public nsIFactory
{   
public:
	// nsISupports methods
	NS_DECL_ISUPPORTS 

  nsSMIMEStubFactory(const nsCID &aClass,
               const char* aClassName,
               const char* aContractID,
               nsISupports*);

  // nsIFactory methods   
  NS_IMETHOD CreateInstance(nsISupports *aOuter, const nsIID &aIID, void **aResult);   
  NS_IMETHOD LockFactory(PRBool aLock);   

protected:
  virtual ~nsSMIMEStubFactory();   

  nsCID mClassID;
  char* mClassName;
  char* mContractID;
  nsIServiceManager* mServiceManager;
};   

nsSMIMEStubFactory::nsSMIMEStubFactory(const nsCID &aClass,
                           const char* aClassName,
                           const char* aContractID,
                           nsISupports *compMgrSupports)
  : mClassID(aClass),
    mClassName(nsCRT::strdup(aClassName)),
    mContractID(nsCRT::strdup(aContractID))
{
	NS_INIT_REFCNT();

  // store a copy of the 
  compMgrSupports->QueryInterface(NS_GET_IID(nsIServiceManager),
                                  (void **)&mServiceManager);
}   

nsSMIMEStubFactory::~nsSMIMEStubFactory()   
{
  NS_IF_RELEASE(mServiceManager);
  PL_strfree(mClassName);
  PL_strfree(mContractID);
}   

nsresult
nsSMIMEStubFactory::QueryInterface(const nsIID &aIID, void **aResult)   
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

NS_IMPL_ADDREF(nsSMIMEStubFactory)
NS_IMPL_RELEASE(nsSMIMEStubFactory)

nsresult
nsSMIMEStubFactory::CreateInstance(nsISupports *aOuter,
                             const nsIID &aIID,
                             void **aResult)  
{  
	nsresult res = NS_OK;

	if (aResult == NULL)  
		return NS_ERROR_NULL_POINTER;  

	*aResult = NULL;  
  
	nsCOMPtr <nsIMimeContentTypeHandler> inst;

	// ClassID check happens here
	// Whenever you add a new class that supports an interface, plug it in here!!!
	
	// do they want a mime emitter interface ?
	if (mClassID.Equals(kMimeContentTypeHandlerCID)) 
	{
		res = NS_NewMimeContentTypeHandler(getter_AddRefs(inst));
		if (NS_FAILED(res))  // was there a problem creating the object ?
		  return res;   
	}

	// End of checking the interface ID code....
	if (inst) 
	{
		// so we now have the class that supports the desired interface...we need to turn around and
		// query for our desired interface.....
		res = inst->QueryInterface(aIID, aResult);
	}
	else
		res = NS_ERROR_OUT_OF_MEMORY;

  return res;  
}  

nsresult
nsSMIMEStubFactory::LockFactory(PRBool aLock)  
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

  *aFactory = new nsSMIMEStubFactory(aClass, aClassName, aContractID, aServMgr);
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

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  char *cType = {"@mozilla.org/mimecth;1?type="SMIME_CONTENT_TYPE};
  rv = compMgr->RegisterComponent(kMimeContentTypeHandlerCID,
                                       "MIME SMIMEStubed Mail Handler",
                                       cType,
                                       path, PR_TRUE, PR_TRUE);
  return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
  nsresult rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kMimeContentTypeHandlerCID, path);

  return rv;
}

////////////////////////////////////////////////////////////////////////////////

