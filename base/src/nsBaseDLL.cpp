/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#include "nsBaseDLL.h"
#include "nscore.h"
#include "nsIProperties.h"
#include "nsProperties.h"
#include "nsRepository.h"

PRInt32 gLockCount = 0;

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

extern "C" NS_EXPORT nsresult
NSRegisterSelf(const char* path)
{
  nsresult ret;

  ret = nsRepository::RegisterFactory(kPropertiesCID, path, PR_TRUE,
    PR_TRUE);
  if (NS_FAILED(ret)) {
    return ret;
  }

  return ret;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(const char* path)
{
  nsresult ret;

  ret = nsRepository::UnregisterFactory(kPropertiesCID, path);
  if (NS_FAILED(ret)) {
    return ret;
  }

  return ret;
}

extern "C" NS_EXPORT nsresult
NSGetFactory(const nsCID& aClass, nsISupports* aServMgr, nsIFactory** aFactory)
{
  nsresult  res;

  if (!aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aClass.Equals(kPropertiesCID)) {
    nsPropertiesFactory *propsFactory = new nsPropertiesFactory();
    if (!propsFactory) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    res = propsFactory->QueryInterface(kIFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = nsnull;
      delete propsFactory;
    }

    return res;
  }

  return NS_NOINTERFACE;
}
