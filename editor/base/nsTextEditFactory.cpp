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

#include "nsTextEditFactory.h"
#include "nsITextEditor.h"
#include "nsTextEditor.h"
#include "nsEditor.h"
#include "nsEditorCID.h"
#include "nsRepository.h"

static NS_DEFINE_IID(kISupportsIID,    NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID,     NS_IFACTORY_IID);
static NS_DEFINE_IID(kITextEditorIID,  NS_ITEXTEDITOR_IID);
static NS_DEFINE_IID(kTextEditorCID,   NS_TEXTEDITOR_CID);
static NS_DEFINE_IID(kITextEditFactoryIID, NS_ITEXTEDITORFACTORY_IID);


nsresult
GetTextEditFactory(nsIFactory **aFactory, const nsCID & aClass)
{
  static nsCOMPtr<nsIFactory>  g_pNSIFactory;
  PR_EnterMonitor(getEditorMonitor());
  nsresult result = NS_ERROR_FAILURE;
  if (!g_pNSIFactory)
  {
    nsTextEditFactory *factory = new nsTextEditFactory(aClass);
    g_pNSIFactory = factory;
    if (factory)
      result = NS_OK;
  }
  result = g_pNSIFactory->QueryInterface(kIFactoryIID, (void **)aFactory);
  PR_ExitMonitor(getEditorMonitor());
  return result;
}

////////////////////////////////////////////////////////////////////////////
// from nsISupports 

NS_METHOD
nsTextEditFactory::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
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

NS_IMPL_ADDREF(nsTextEditFactory)
NS_IMPL_RELEASE(nsTextEditFactory)


////////////////////////////////////////////////////////////////////////////
// from nsIFactory:

NS_METHOD
nsTextEditFactory::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  *aResult  = nsnull;
  nsISupports *obj = nsnull;
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  if (aOuter && !aIID.Equals(kISupportsIID))
    return NS_NOINTERFACE;   // XXX right error?


  if (mCID.Equals(kTextEditorCID))
    obj = (nsISupports *)new nsTextEditor();
  //more class ids to support. here

  if (obj && NS_FAILED(obj->QueryInterface(aIID, (void**)aResult)) ) 
  {
    delete obj;
    return NS_NOINTERFACE;
  }
  return NS_OK;
}



NS_METHOD
nsTextEditFactory::LockFactory(PRBool aLock)
{
  return NS_OK;
}



////////////////////////////////////////////////////////////////////////////
// from nsTextEditFactory:

nsTextEditFactory::nsTextEditFactory(const nsCID &aClass)
:mCID(aClass)
{
  NS_INIT_REFCNT();
}

nsTextEditFactory::~nsTextEditFactory()
{
  //nsRepository::UnregisterFactory(mCID, (nsIFactory *)this); //we are out of ref counts anyway
}







