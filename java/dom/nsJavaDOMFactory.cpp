/* 
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
the License for the specific language governing rights and limitations
under the License.

The Initial Developer of the Original Code is Sun Microsystems,
Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
Inc. All Rights Reserved. 
*/

#include "pratom.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"

#include "nsJavaDOMCID.h"
#include "nsJavaDOMImpl.h"
#include "nsJavaDOMFactory.h"

static PRInt32 gLockCount = 0;

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_IID(kJavaDOMFactoryCID, NS_JAVADOMFACTORY_CID);
static NS_DEFINE_IID(kJavaDOMFactoryIID, NS_JAVADOMFACTORY_IID);

NS_IMPL_ISUPPORTS(nsJavaDOMFactory, kJavaDOMFactoryIID)

nsJavaDOMFactory::nsJavaDOMFactory() 
{
  NS_INIT_REFCNT();
}

nsresult nsJavaDOMFactory::CreateInstance(nsISupports* aOuter,
					  const nsIID& aIID,
					  void** aResult) 
{
  if (aOuter != NULL)
    return NS_ERROR_NO_AGGREGATION;

  if (aResult == NULL)
    return NS_ERROR_NULL_POINTER;  

  *aResult = NULL;  
  
  nsISupports* inst = new nsJavaDOMImpl();
  if (inst == NULL)
    return NS_ERROR_OUT_OF_MEMORY;  

  nsresult res = inst->QueryInterface(aIID, aResult);
  if (NS_FAILED(res))
    delete inst;

  return res;
}

nsresult nsJavaDOMFactory::LockFactory(PRBool aLock) 
{ 
  if (aLock)
    PR_AtomicIncrement(&gLockCount);
  else
    PR_AtomicDecrement(&gLockCount);
  return NS_OK;
}

extern "C"
NS_EXPORT nsresult NSGetFactory(nsISupports* serviceMgr,
				const nsCID& aClass,
				const char* aClassName,
				const char* aProgID,
				nsIFactory** aFactory)
{
  if (aFactory == NULL)
    return NS_ERROR_NULL_POINTER;
  
  *aFactory = NULL;
  nsISupports *inst = NULL;
  static NS_DEFINE_IID(kJavaDOMFactoryCID, NS_JAVADOMFACTORY_CID);
  if (aClass.Equals(kJavaDOMFactoryCID))
    inst = new nsJavaDOMFactory();
  else
    return NS_ERROR_ILLEGAL_VALUE;

  if (inst == NULL)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult res = inst->QueryInterface(kJavaDOMFactoryIID, 
				      (void**) aFactory);
  if (res != NS_OK)
    delete inst;

  return res;
}

extern "C" 
NS_EXPORT PRBool NSCanUnload(nsISupports* serviceMgr) 
{
  return (PRBool) (gLockCount == 0);
}

extern "C"
NS_EXPORT nsresult NSRegisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult rv;
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv))
    return rv;

  nsIComponentManager* cm = nsnull;
  rv = servMgr->GetService(kComponentManagerCID,
                           nsIComponentManager::GetIID(),
                           (nsISupports**)&cm);
  if (NS_FAILED(rv))
    return rv;

  rv = cm->RegisterComponent(kJavaDOMFactoryCID, NULL, NULL,
			     aPath, PR_TRUE, PR_TRUE);
  return rv;
}

extern "C"
NS_EXPORT nsresult NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult rv;
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv))
    return rv;

  nsIComponentManager* cm = nsnull;
  rv = servMgr->GetService(kComponentManagerCID,
                           nsIComponentManager::GetIID(),
                           (nsISupports**)&cm);
  if (NS_FAILED(rv))
    return rv;

  rv = cm->UnregisterComponent(kJavaDOMFactoryCID, aPath);
  return rv;
}
