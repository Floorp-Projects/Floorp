/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
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

#include "nsIGuiManagerFactory.h"
#include "nsEditGuiManager.h"
#include "nsGuiManagerFactory.h"
#include "nsIEditor.h"
#include "nsRepository.h"
#include "COM_auto_ptr.h"

static NS_DEFINE_IID(kISupportsIID,          NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,           NS_IFACTORY_IID);
static NS_DEFINE_IID(kIGuiManagerFactoryIID, NS_IGUIMANAGERFACTORY_IID);



nsresult
getGuiManagerFactory(nsIFactory **aFactory)
{
  static COM_auto_ptr<nsIFactory>  g_pNSIFactory;

  nsresult result = NS_ERROR_FAILURE;
  if (!g_pNSIFactory)
  {
    nsGuiManagerFactory *factory = new nsGuiManagerFactory(getter_AddRefs(g_pNSIFactory));
    *aFactory = g_pNSIFactory;
    NS_IF_ADDREF(*aFactory);
    if (factory)
      result = NS_OK;
  }
  else
    result = g_pNSIFactory->QueryInterface(kIFactoryIID, (void **)aFactory);

  return result;
}

////////////////////////////////////////////////////////////////////////////
// from nsISupports 

NS_METHOD
nsGuiManagerFactory::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
  if (NULL == aInstancePtr) {
    NS_NOTREACHED("!nsEditGuiManager");
    return NS_ERROR_NULL_POINTER; 
  } 
  if (aIID.Equals(kIFactoryIID) ||
    aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*) this; 
    AddRef(); 
    return NS_OK; 
  }
  return NS_NOINTERFACE; 
}

NS_IMPL_ADDREF(nsGuiManagerFactory)
NS_IMPL_RELEASE(nsGuiManagerFactory)


////////////////////////////////////////////////////////////////////////////
// from nsIFactory:

NS_METHOD
nsGuiManagerFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsEditGuiManager *guimgr = NULL;
  *aResult  = NULL;
  
  if (aOuter && !aIID.Equals(kISupportsIID))
    return NS_NOINTERFACE;   // XXX right error?
  
  guimgr = new nsEditGuiManager();
  
  if (NS_FAILED(guimgr->QueryInterface(aIID,(void**)aResult))) {
    // then we're trying get a interface other than nsISupports and nsIEditGuiManager
    delete guimgr;
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}



NS_METHOD
nsGuiManagerFactory::LockFactory(PRBool aLock)
{
  return NS_OK;
}



////////////////////////////////////////////////////////////////////////////
// from nsFactory:

nsGuiManagerFactory::nsGuiManagerFactory(nsIFactory **aFactory)
{
  NS_INIT_REFCNT();
  nsresult     err         = NS_OK;
  if (aFactory)
  {
    err = this->QueryInterface(kIFactoryIID, (void**)aFactory); 
  }
}

nsGuiManagerFactory::~nsGuiManagerFactory()
{
  nsRepository::UnregisterFactory(kIGuiManagerFactoryIID, (nsIFactory *)this); //we are out of ref counts anyway
}
