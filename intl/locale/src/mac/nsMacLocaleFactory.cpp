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

#include "nsIMacLocale.h"
#include "nsMacLocale.h"
#include "nsMacLocaleFactory.h"
#include "nsLocaleCID.h"

NS_DEFINE_IID(kIMacLocaleIID, NS_IMACLOCALE_IID);
NS_DEFINE_IID(kMacLocaleFactoryCID, NS_MACLOCALEFACTORY_CID);
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);


nsMacLocaleFactory::nsMacLocaleFactory()
{
	NS_INIT_REFCNT();
}

nsMacLocaleFactory::~nsMacLocaleFactory()
{

}

NS_IMETHODIMP
nsMacLocaleFactory::CreateInstance(nsISupports* aOuter, REFNSIID aIID,
		void** aResult)
{
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  if (aIID.Equals(kISupportsIID))
  {   
    *aResult = (void *)(nsISupports*)this;   
	NS_ADDREF_THIS(); // Increase reference count for caller   
  } else if (aIID.Equals(kIFactoryIID))
  {   
    *aResult = (void *)(nsIFactory*)this;   
	NS_ADDREF_THIS(); // Increase reference count for caller   
  } else if (aIID.Equals(kIMacLocaleIID))
  {

    nsMacLocale *localeImpl = new nsMacLocale();
    if(localeImpl)
      localeImpl->AddRef();
    *aResult = (void*)localeImpl;
	
  }

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   


  return NS_OK;   
}

nsresult nsMacLocaleFactory::QueryInterface(const nsIID &aIID,   
                                      void **aResult)   
{   
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   

  if (aIID.Equals(kISupportsIID)) {   
    *aResult = (void *)(nsISupports*)this;   
  } else if (aIID.Equals(kIFactoryIID)) {   
    *aResult = (void *)(nsIFactory*)this;   
  }   

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   

  NS_ADDREF_THIS(); // Increase reference count for caller   
  return NS_OK;   
}   


NS_IMETHODIMP
nsMacLocaleFactory::LockFactory(PRBool	aBool)
{
	return NS_OK;
}


nsrefcnt
nsMacLocaleFactory::AddRef()   
{   
  return ++mRefCnt;   
}   

nsrefcnt
nsMacLocaleFactory::Release()   
{   
  if (--mRefCnt == 0) {   
    delete this;   
    return 0; // Don't access mRefCnt after deleting!   
  }   
  return mRefCnt;   
}  
