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
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 07/05/2000       IBM Corp.      created this file, modeled after its unix counter part: nsPosixLocaleFactory.cpp.
 *
 */

#include "nsOS2Locale.h"
#include "nsOS2LocaleFactory.h"
#include "nsLocaleCID.h"

NS_DEFINE_IID(kIOS2LocaleIID, NS_IOS2LOCALE_IID);
NS_DEFINE_IID(kOS2LocaleFactoryCID, NS_OS2LOCALEFACTORY_CID);
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);


nsOS2LocaleFactory::nsOS2LocaleFactory()
{
	NS_INIT_REFCNT();
}

nsOS2LocaleFactory::~nsOS2LocaleFactory()
{

}

NS_IMETHODIMP
nsOS2LocaleFactory::CreateInstance(nsISupports* aOuter, REFNSIID aIID,
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
  } else if (aIID.Equals(kIOS2LocaleIID))
  {

	nsOS2Locale *localeImpl = new nsOS2Locale();
	if(localeImpl)
      localeImpl->AddRef();
	*aResult = (void*)localeImpl;
	
  }

  if (*aResult == NULL) {   
    return NS_NOINTERFACE;   
  }   


  return NS_OK;   
}

nsresult nsOS2LocaleFactory::QueryInterface(const nsIID &aIID,   
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
nsOS2LocaleFactory::LockFactory(PRBool	aBool)
{
	return NS_OK;
}

NS_IMPL_ADDREF(nsOS2LocaleFactory);
NS_IMPL_RELEASE(nsOS2LocaleFactory);