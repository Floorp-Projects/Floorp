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
#include "editor.h"
#include "nsRepository.h"


static NS_DEFINE_IID(kISupportsIID,    NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,     NS_IFACTORY_IID);
static NS_DEFINE_IID(kIEditorIID,      NS_IEDITOR_IID);
static NS_DEFINE_IID(kIEditFactoryIID, NS_IEDITORFACTORY_IID);



nsresult
getEditFactory(nsIFactory **aFactory)
{
  static COM_auto_ptr<nsIFactory>  g_pNSIFactory;
  PR_EnterMonitor(getEditorMonitor());
  nsresult result = NS_ERROR_FAILURE;
  if (!g_pNSIFactory)
  {
    nsEditFactory *factory = new nsEditFactory(getter_AddRefs(g_pNSIFactory));
    *aFactory = g_pNSIFactory;
    NS_IF_ADDREF(*aFactory);
    if (factory)
      result = NS_OK;
  }
  else
    result = g_pNSIFactory->QueryInterface(kIFactoryIID, (void **)aFactory);
  PR_ExitMonitor(getEditorMonitor());
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
  if (aIID.Equals(kIFactoryIID) ||
    aIID.Equals(kISupportsIID)) {
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
  nsEditor *editor = nsnull;
  *aResult  = nsnull;
  
  if (aOuter && !aIID.Equals(kISupportsIID))
    return NS_NOINTERFACE;   // XXX right error?
  editor = new nsEditor();
  if (editor->QueryInterface(aIID,
    (void**)aResult) != NS_OK) {
    // then we're trying get a interface other than nsISupports and
    // nsIEditor
    delete editor;
    return NS_ERROR_FAILURE;
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

nsEditFactory::nsEditFactory(nsIFactory **aFactory)
{
  NS_INIT_REFCNT();
  nsresult     err         = NS_OK;
  NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
  if (aFactory)
  {
    err = this->QueryInterface(kIFactoryIID, (void**)aFactory); 
  }
}

nsEditFactory::~nsEditFactory()
{
  nsRepository::UnregisterFactory(kIEditFactoryIID, (nsIFactory *)this); //we are out of ref counts anyway
}







