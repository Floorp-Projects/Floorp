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

#include "nsEditFactory.h"
#include "nsIEditor.h"
#include "nsEditor.h"
#include "nsEditorCID.h"
#include "nsIComponentManager.h"

static NS_DEFINE_IID(kIEditFactoryIID, NS_IEDITORFACTORY_IID);
static NS_DEFINE_CID(kEditorCID,       NS_EDITOR_CID);



nsresult
GetEditFactory(nsIFactory **aFactory, const nsCID & aClass)
{
  
  // XXX Note static which never gets released, even on library unload.
  // XXX Was an nsCOMPtr but that caused a crash on exit,
  // XXX http://bugzilla.mozilla.org/show_bug.cgi?id=7938
  PR_EnterMonitor(GetEditorMonitor());

  nsEditFactory *factory = new nsEditFactory(aClass);
  if (!factory)
    return NS_ERROR_OUT_OF_MEMORY;
  nsCOMPtr<nsIFactory> pNSIFactory = do_QueryInterface(factory);
  if (!pNSIFactory)
    return NS_ERROR_NO_INTERFACE;

  nsresult result = pNSIFactory->QueryInterface(nsIFactory::GetIID(),
                                                (void **)aFactory);
  PR_ExitMonitor(GetEditorMonitor());
  return result;
}

////////////////////////////////////////////////////////////////////////////
// from nsISupports 

NS_METHOD
nsEditFactory::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
  if (nsnull == aInstancePtr) {
    NS_NOTREACHED("!nsEditor");
    return NS_ERROR_NULL_POINTER; 
  } 
  if (aIID.Equals(nsIFactory::GetIID()) ||
    aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = (void*) this; 
    AddRef();  
    return NS_OK; 
  }
  return NS_NOINTERFACE; 
}

NS_IMPL_ADDREF(nsEditFactory)
NS_IMPL_RELEASE(nsEditFactory)


////////////////////////////////////////////////////////////////////////////
// from nsIFactory:

NS_METHOD
nsEditFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  *aResult  = nsnull;
  nsISupports *obj = nsnull;
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  if (aOuter && !aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
    return NS_NOINTERFACE;   // XXX right error?


  if (mCID.Equals(kEditorCID))
    obj = (nsISupports *)(nsIEditor*)new nsEditor();
  //more class ids to support. here


  if (obj && NS_FAILED(obj->QueryInterface(aIID, (void**)aResult)) ) 
  {
    delete obj;
    return NS_NOINTERFACE;
  }
  return NS_OK;
}



NS_METHOD
nsEditFactory::LockFactory(PRBool aLock)
{
  return NS_OK;
}



////////////////////////////////////////////////////////////////////////////
// from nsEditFactory:

nsEditFactory::nsEditFactory(const nsCID &aClass)
:mCID(aClass)
{
  NS_INIT_REFCNT();
}

nsEditFactory::~nsEditFactory()
{
  //nsComponentManager::UnregisterFactory(mCID, (nsIFactory *)this); //we are out of ref counts anyway
}







