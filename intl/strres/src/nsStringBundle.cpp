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

#include "nsString2.h"
#include "nsIProperties.h"
#include "nsIStringBundle.h"
#include "nscore.h"
#include "nsILocale.h"
#include "nsIAllocator.h"
#include "plstr.h"

#ifndef NECKO
#include "nsINetService.h"
#else
#include "nsNeckoUtil.h"
#endif // NECKO

#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "pratom.h"
#include "prmem.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static PRInt32 gLockCount = 0;

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_DEFINE_IID(kIStringBundleIID, NS_ISTRINGBUNDLE_IID);
NS_DEFINE_IID(kIStringBundleServiceIID, NS_ISTRINGBUNDLESERVICE_IID);
NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

#ifndef NECKO
static NS_DEFINE_IID(kINetServiceIID, NS_INETSERVICE_IID);
static NS_DEFINE_IID(kNetServiceCID, NS_NETSERVICE_CID);
#endif // NECKO

static NS_DEFINE_IID(kIPersistentPropertiesIID, NS_IPERSISTENTPROPERTIES_IID);

class nsStringBundle : public nsIStringBundle
{
public:
  nsStringBundle(const char* aURLSpec, nsILocale* aLocale, nsresult* aResult);
  virtual ~nsStringBundle();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLE

  nsIPersistentProperties* mProps;

protected:
  //
  // functional decomposition of the funitions repeatively called 
  //
  nsresult GetStringFromID(PRInt32 aID, nsString& aResult);
  nsresult GetStringFromName(const nsString& aName, nsString& aResult);

  nsresult GetInputStream(const char* aURLSpec, nsILocale* aLocale, nsIInputStream*& in);
  nsresult OpenInputStream(nsString2& aURLStr, nsIInputStream*& in);
  nsresult GetLangCountry(nsILocale* aLocale, nsString2& lang, nsString2& country);
 };

nsStringBundle::nsStringBundle(const char* aURLSpec, nsILocale* aLocale, nsresult* aResult)
{  NS_INIT_REFCNT();

  mProps = nsnull;

  nsIInputStream *in = nsnull;
  *aResult = GetInputStream(aURLSpec,aLocale, in);
   
  if (!in) {
#ifdef NS_DEBUG
    if ( NS_OK == *aResult)
      printf("OpenBlockingStream returned success value, but pointer is NULL\n");
#endif
    *aResult = NS_ERROR_UNEXPECTED;
    return;
  }

  *aResult = nsComponentManager::CreateInstance(kPersistentPropertiesCID, NULL,
                                                kIPersistentPropertiesIID, (void**) &mProps);
  if (NS_FAILED(*aResult)) {
#ifdef NS_DEBUG
    printf("create nsIPersistentProperties failed\n");
#endif
    return;
  }
  *aResult = mProps->Load(in);
  NS_RELEASE(in);
}

nsStringBundle::~nsStringBundle()
{
  NS_IF_RELEASE(mProps);
}

nsresult
nsStringBundle::GetStringFromID(PRInt32 aID, nsString& aResult)
{
  nsAutoString name("");
  name.Append(aID, 10);
  nsresult ret = mProps->GetProperty(name, aResult);

#ifdef DEBUG_tao
  char *s = aResult.ToNewCString();
  printf("\n** GetStringFromID: aResult=%s, len=%d\n", s?s:"null", 
         aResult.Length());
  delete s;
#endif /* DEBUG_tao */

  return ret;
}

nsresult
nsStringBundle::GetStringFromName(const nsString& aName, nsString& aResult)
{
  nsresult ret = mProps->GetProperty(aName, aResult);
#ifdef DEBUG_tao
  char *s = aResult.ToNewCString(),
       *ss = aName.ToNewCString();
  printf("\n** GetStringFromName: aName=%s, aResult=%s, len=%d\n", 
         ss?ss:"null", s?s:"null", aResult.Length());
  delete s;
#endif /* DEBUG_tao */
  return ret;
}

NS_IMPL_ISUPPORTS(nsStringBundle, nsIStringBundle::GetIID())

/* void GetStringFromID (in long aID, out wstring aResult); */
NS_IMETHODIMP
nsStringBundle::GetStringFromID(PRInt32 aID, PRUnichar **aResult)
{
  *aResult = nsnull;
  nsString tmpstr("");

  nsresult ret = GetStringFromID(aID, tmpstr);
  PRInt32 len =  tmpstr.Length()+1;
  if (NS_FAILED(ret) || !len) {
    return ret;
  }

  *aResult = (PRUnichar *) PR_Calloc(len, sizeof(PRUnichar));
  *aResult = (PRUnichar *) memcpy(*aResult, tmpstr.GetUnicode(), sizeof(PRUnichar)*len);
  (*aResult)[len-1] = '\0';
  return ret;
}

/* void GetStringFromName ([const] in wstring aName, out wstring aResult); */
NS_IMETHODIMP 
nsStringBundle::GetStringFromName(const PRUnichar *aName, PRUnichar **aResult)
{
  *aResult = nsnull;
  nsString tmpstr("");
  nsString nameStr(aName);
  nsresult ret = GetStringFromName(nameStr, tmpstr);
  PRInt32 len =  tmpstr.Length()+1;
  if (NS_FAILED(ret) || !len) {
    return ret;
  }

  *aResult = (PRUnichar *) PR_Calloc(len, sizeof(PRUnichar));
  *aResult = (PRUnichar *) memcpy(*aResult, tmpstr.GetUnicode(), sizeof(PRUnichar)*len);
  (*aResult)[len-1] = '\0';
  
  return ret;
}

NS_IMETHODIMP
nsStringBundle::GetEnumeration(nsIBidirectionalEnumerator** elements)
{
	if (!elements)
		return NS_ERROR_INVALID_POINTER;

	nsresult ret = mProps->EnumerateProperties(elements);

	return ret;
}

nsresult
nsStringBundle::GetInputStream(const char* aURLSpec, nsILocale* aLocale, nsIInputStream*& in) 
{
  in = nsnull;

  nsresult ret = NS_OK;

  /* locale binding */
  nsString2  strFile2;
  nsString   lc_lang;
  nsString   lc_country;

  if (NS_OK == (ret = GetLangCountry(aLocale, lc_lang, lc_country))) {
    
    /* find the place to concatenate locale name 
     */
    PRInt32   count = 0;
    nsString2 strFile(aURLSpec);
    PRInt32   mylen = strFile.Length();
    nsString2 fileLeft;
 
    /* assume the name always ends with this
     */
    PRInt32 dot = strFile.RFindChar('.');
    count = strFile.Left(fileLeft, (dot>0)?dot:mylen);
    strFile2 += fileLeft;

    /* insert lang-country code
     */
    strFile2 += "_";
    strFile2 += lc_lang;
    if (lc_country.Length() > 0) {
      strFile2 += "_";
      strFile2 += lc_country;;
    }

    /* insert it
     */   
    nsString2 fileRight;
    if (dot > 0) {
      count = strFile.Right(fileRight, mylen-dot);
      strFile2 += fileRight;
    }
    ret = OpenInputStream(strFile2, in);
    if ((NS_FAILED(ret) || !in) && lc_country.Length() && lc_lang.Length()) {
      /* plan A: fallback to lang only
       */
      strFile2 = fileLeft;
      strFile2 += "_";
      strFile2 += lc_lang;
      strFile2 += fileRight;
      ret = OpenInputStream(strFile2, in);
    }/* plan A */   
  }/* locale */

  if (NS_FAILED(ret) || !in) {
    /* plan B: fallback to aURLSpec
     */
    strFile2 = aURLSpec;
    ret = OpenInputStream(strFile2, in);
  }
  return ret;
}

nsresult
nsStringBundle::OpenInputStream(nsString2& aURLStr, nsIInputStream*& in) 
{
#ifdef DEBUG_tao
  {
    char *s = aURLStr.ToNewCString();
    printf("\n** nsStringBundle::OpenInputStream: %s\n", s?s:"null");
    delete s;
  }
#endif
  nsresult ret;
#ifndef NECKO
  nsINetService* pNetService = nsnull;

  ret = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID, (nsISupports**) &pNetService);
  /* get the url
   */
  nsIURI    *url = nsnull;

  ret = pNetService->CreateURL(&url, aURLStr, nsnull, nsnull,
                               nsnull);
  if (NS_FAILED(ret)) {
#ifdef DEBUG_tao
    char *s = aURLStr.ToNewCString();
    printf("\n** cannot create URL for %s\n", s?s:"null");
    delete s;
#endif
    return ret;
  }
  //
  ret = pNetService->OpenBlockingStream(url, nsnull, &in);
  NS_RELEASE(url);
#ifdef DEBUG_tao
  if (NS_FAILED(ret) || !in) {
    char *s = aURLStr.ToNewCString();
    printf("\n** cannot open stream: %s\n", s?s:"null");
    delete s;
  }
#endif

#else // NECKO
  nsIURI* uri;
  ret = NS_NewURI(&uri, aURLStr);
  if (NS_FAILED(ret)) return ret;

  ret = NS_OpenURI(&in, uri);
  NS_RELEASE(uri);
#endif // NECKO
  return ret;
}

nsresult 
nsStringBundle::GetLangCountry(nsILocale* aLocale, nsString2& lang, nsString2& country)
{
  if (!aLocale) {
    return NS_ERROR_FAILURE;
  }

  PRUnichar *lc_name_unichar;
  nsString	  lc_name;
  nsString  	catagory("NSILOCALE_MESSAGES");
  nsresult	  result	 = aLocale->GetCategory(catagory.GetUnicode(), &lc_name_unichar);
  lc_name.SetString(lc_name_unichar);

  NS_ASSERTION(result==NS_OK,"nsStringBundle::GetLangCountry: locale.GetCatagory failed");
  NS_ASSERTION(lc_name.Length()>0,"nsStringBundle::GetLangCountry: locale.GetCatagory failed");

  PRInt32   dash = lc_name.FindCharInSet("-");
  if (dash > 0) {
    /* 
     */
    PRInt32 count = 0;
    count = lc_name.Left(lang, dash);
    count = lc_name.Right(country, (lc_name.Length()-dash-1));
  }
  else
    lang = lc_name;

  return NS_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////


class nsStringBundleService : public nsIStringBundleService
{
public:
  nsStringBundleService();
  virtual ~nsStringBundleService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLESERVICE
};

nsStringBundleService::nsStringBundleService()
{
#ifdef DEBUG_tao
  printf("\n++ nsStringBundleService::nsStringBundleService ++\n");
#endif
  NS_INIT_REFCNT();
}

nsStringBundleService::~nsStringBundleService()
{
}

NS_IMPL_ISUPPORTS(nsStringBundleService, nsIStringBundleService::GetIID())

NS_IMETHODIMP
nsStringBundleService::CreateBundle(const char* aURLSpec, nsILocale* aLocale,
                                    nsIStringBundle** aResult)
{
#ifdef DEBUG_tao
  printf("\n++ nsStringBundleService::CreateBundle ++\n");
  {
    nsString2 aURLStr(aURLSpec);
    char *s = aURLStr.ToNewCString();
    printf("\n** nsStringBundleService::CreateBundle: %s\n", s?s:"null");
    delete s;
  }
#endif
  nsresult ret = NS_OK;
  nsStringBundle* bundle = new nsStringBundle(aURLSpec, aLocale, &ret);
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

/* void CreateXPCBundle ([const] in string aURLSpec, [const] in wstring aLocaleName, out nsIStringBundle aResult); */
NS_IMETHODIMP 
nsStringBundleService::CreateXPCBundle(const char *aURLSpec, const PRUnichar *aLocaleName, nsIStringBundle **aResult)
{

#ifdef DEBUG_tao
  printf("\n++ nsStringBundleService::CreateXPCBundle ++\n");
  {
    nsString2 aURLStr(aURLSpec);
    char *s = aURLStr.ToNewCString();
    printf("\n** nsStringBundleService::XPCCreateBundle: %s\n", s?s:"null");
    delete s;
  }
#endif
  nsresult ret = NS_OK;
  nsStringBundle* bundle = new nsStringBundle(aURLSpec, nsnull, &ret);
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

class nsStringBundleServiceFactory : public nsIFactory
{
public:
  nsStringBundleServiceFactory();
  virtual ~nsStringBundleServiceFactory();

  NS_DECL_ISUPPORTS

  NS_IMETHOD CreateInstance(nsISupports* aOuter, REFNSIID aIID, void** aResult);
  NS_IMETHOD LockFactory(PRBool aLock);
};

nsStringBundleServiceFactory::nsStringBundleServiceFactory()
{
#ifdef DEBUG_tao
  printf("\n++ nsStringBundleServiceFactory::nsStringBundleServiceFactory ++\n");
#endif
  NS_INIT_REFCNT();
}

nsStringBundleServiceFactory::~nsStringBundleServiceFactory()
{
}

NS_IMPL_ISUPPORTS(nsStringBundleServiceFactory, kIFactoryIID)

NS_IMETHODIMP
nsStringBundleServiceFactory::CreateInstance(nsISupports* aOuter,
  REFNSIID aIID, void** aResult)
{
#ifdef DEBUG_tao
  printf("\n++ nsStringBundleServiceFactory::CreateInstance ++\n");
#endif
  nsStringBundleService* service = new nsStringBundleService();
  if (!service) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult ret = service->QueryInterface(aIID, aResult);
  if (NS_FAILED(ret)) {
    delete service;
    return ret;
  }

  return ret;
}

NS_IMETHODIMP
nsStringBundleServiceFactory::LockFactory(PRBool aLock)
{
#ifdef DEBUG_tao
  printf("\n++ nsStringBundleServiceFactory::LockFactory ++\n");
#endif
  if (aLock) {
    PR_AtomicIncrement(&gLockCount);
  }
  else {
    PR_AtomicDecrement(&gLockCount);
  }

  return NS_OK;
}

extern "C" NS_EXPORT nsresult
NSRegisterSelf(nsISupports* aServMgr, const char* path)
{
#ifdef DEBUG_tao
  printf("\n++  str bunlde  NSRegisterSelf ++\n");
#endif
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kStringBundleServiceCID, 
                                  "String Bundle", 
                                  NS_STRINGBUNDLE_PROGID, 
                                  path,
                                  PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult
NSUnregisterSelf(nsISupports* aServMgr, const char* path)
{
#ifdef DEBUG_tao
  printf("\n++  str bunlde  NSUnregisterSelf ++\n");
#endif
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kStringBundleServiceCID, path);
  if (NS_FAILED(rv)) goto done;

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* aServMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
#ifdef DEBUG_tao
  printf("\n++  str bunlde  NSGetFactory ++\n");
#endif
  nsresult  res;

  if (!aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aClass.Equals(kStringBundleServiceCID)) {
    nsStringBundleServiceFactory* factory = new nsStringBundleServiceFactory();
    if (!factory) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    res = factory->QueryInterface(kIFactoryIID, (void**) aFactory);
    if (NS_FAILED(res)) {
      *aFactory = nsnull;
      delete factory;
    }

    return res;
  }

  return NS_NOINTERFACE;
}
