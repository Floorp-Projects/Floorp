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

#define NS_IMPL_IDS
#include "nsID.h"

#include "nsIProperties.h"
#include "nsIStringBundle.h"
#include "nscore.h"
#include "nsINetService.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsRepository.h"
#include "nsString.h"

extern "C" PRInt32 gLockCount = 0;

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_IID(kIStringBundleIID, NS_ISTRINGBUNDLE_IID);
NS_DEFINE_IID(kIStringBundleFactoryIID, NS_ISTRINGBUNDLEFACTORY_IID);
NS_DEFINE_IID(kStringBundleFactoryCID, NS_STRINGBUNDLEFACTORY_CID);

static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kIPropertiesIID, NS_IPROPERTIES_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);

class nsStringBundle : public nsISupports
{
public:
  nsStringBundle(nsIURL* aURL, const nsString& aLocale, nsresult* aResult);
  ~nsStringBundle();

  NS_DECL_ISUPPORTS

  NS_IMETHOD GetStringFromID(PRInt32 aID, nsString& aResult);
  NS_IMETHOD GetStringFromName(const nsString& aName, nsString& aResult);

  nsIProperties* mProps;
};

nsStringBundle::nsStringBundle(nsIURL* aURL, const nsString& aLocale,
  nsresult* aResult)
{
  mProps = nsnull;

  nsINetService* pNetService = nsnull;
  *aResult = nsServiceManager::GetService(kNetServiceCID,
    kINetServiceIID, (nsISupports**) &pNetService);
  if (NS_FAILED(*aResult)) {
    printf("cannot get net service\n");
    return;
  }
  nsIInputStream *in = nsnull;
  *aResult = pNetService->OpenBlockingStream(aURL, nsnull, &in);
  if (NS_FAILED(*aResult)) {
    printf("cannot open stream\n");
    return;
  }
  *aResult = nsRepository::CreateInstance(kPropertiesCID, NULL,
    kIPropertiesIID, (void**) &mProps);
  if (NS_FAILED(*aResult)) {
    printf("create nsIProperties failed\n");
    return;
  }
  *aResult = mProps->Load(in);
}

nsStringBundle::~nsStringBundle()
{
  NS_IF_RELEASE(mProps);
}

NS_IMPL_ISUPPORTS(nsStringBundle, NS_ISTRINGBUNDLE_IID)

NS_IMETHODIMP
nsStringBundle::GetStringFromID(PRInt32 aID, nsString& aResult)
{
  nsAutoString name("");
  name.Append(aID, 10);
  nsresult ret = mProps->GetProperty(name, aResult);

  return ret;
}

NS_IMETHODIMP
nsStringBundle::GetStringFromName(const nsString& aName, nsString& aResult)
{
  nsresult ret = mProps->GetProperty(aName, aResult);

  return ret;
}

class nsStringBundleFactory : public nsIStringBundleFactory
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstance(nsISupports* aOuter, REFNSIID aIID, void** aResult);
  NS_IMETHOD LockFactory(PRBool aLock);

  NS_IMETHOD CreateBundle(nsIURL* aURL, const nsString& aLocale,
    nsIStringBundle** aResult);
};

NS_IMPL_ISUPPORTS(nsStringBundleFactory, NS_ISTRINGBUNDLEFACTORY_IID)

NS_IMETHODIMP
nsStringBundleFactory::CreateInstance(nsISupports* aOuter, REFNSIID aIID,
  void** aResult)
{
  return NS_OK;
}

NS_IMETHODIMP
nsStringBundleFactory::LockFactory(PRBool aLock)
{
  return NS_OK;
}

NS_IMETHODIMP
nsStringBundleFactory::CreateBundle(nsIURL* aURL, const nsString& aLocale,
  nsIStringBundle** aResult)
{
  nsresult ret = NS_OK;
  nsStringBundle* bundle = new nsStringBundle(aURL, aLocale, &ret);
  if (!bundle) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (NS_FAILED(ret)) {
    delete bundle;
    return ret;
  }
  ret = bundle->QueryInterface(kIStringBundleIID, (void**) aResult);
  if (NS_FAILED(ret)) {
    delete bundle;
  }

  return ret;
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(const char* path)
{
  nsresult ret;

  ret = nsRepository::RegisterFactory(kStringBundleFactoryCID, path,
    PR_TRUE, PR_TRUE);
  if (NS_FAILED(ret)) {
    return ret;
  }

  return ret;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(const char* path)
{
  nsresult ret;

  ret = nsRepository::UnregisterFactory(kStringBundleFactoryCID, path);
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

  if (aClass.Equals(kStringBundleFactoryCID)) {
    nsStringBundleFactory* factory = new nsStringBundleFactory();
    if (!factory) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    res = factory->QueryInterface(kIStringBundleFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = nsnull;
      delete factory;
    }

    return res;
  }

  return NS_NOINTERFACE;
}
