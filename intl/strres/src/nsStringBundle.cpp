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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#define NS_IMPL_IDS
#include "nsID.h"
#include "nsFileSpec.h"
#include "nsString.h"
#include "nsIPersistentProperties.h"
#include "nsIStringBundle.h"
#include "nscore.h"
#include "nsILocale.h"
#include "nsIAllocator.h"
#include "plstr.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "pratom.h"
#include "prclist.h"
#include "plarena.h"
#include "prlog.h"              // for PR_ASSERT
#include "prmem.h"
#include "nsIServiceManager.h"
#include "nsIModule.h"
#include "nsIRegistry.h"
#include "nsISupportsArray.h"
#include "nsHashtable.h"
#include "nsAutoLock.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

// XXX investigate need for proper locking in this module
//static PRInt32 gLockCount = 0;

NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);


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
  nsresult OpenInputStream(nsString& aURLStr, nsIInputStream*& in);
public:
  static nsresult GetLangCountry(nsILocale* aLocale, nsString& lang, nsString& country);
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
                                                NS_GET_IID(nsIPersistentProperties), (void**) &mProps);
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
  NS_ENSURE_TRUE(mProps, NS_ERROR_UNEXPECTED);
  nsAutoCMonitor(this);
  nsAutoString name;
  name.AppendInt(aID, 10);
  nsresult ret = mProps->GetStringProperty(name, aResult);

#ifdef DEBUG_tao_
  char *s = aResult.ToNewCString();
  printf("\n** GetStringFromID: aResult=%s, len=%d\n", s?s:"null", 
         aResult.Length());
  delete s;
#endif /* DEBUG_tao_ */

  return ret;
}

nsresult
nsStringBundle::GetStringFromName(const nsString& aName, nsString& aResult)
{
  NS_ENSURE_TRUE(mProps, NS_ERROR_FAILURE);
  nsresult ret = mProps->GetStringProperty(aName, aResult);
#ifdef DEBUG_tao_
  char *s = aResult.ToNewCString(),
       *ss = aName.ToNewCString();
  printf("\n** GetStringFromName: aName=%s, aResult=%s, len=%d\n", 
         ss?ss:"null", s?s:"null", aResult.Length());
  delete s;
#endif /* DEBUG_tao_ */
  return ret;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStringBundle, nsIStringBundle)

/* void GetStringFromID (in long aID, out wstring aResult); */
NS_IMETHODIMP
nsStringBundle::GetStringFromID(PRInt32 aID, PRUnichar **aResult)
{
  *aResult = nsnull;
  nsString tmpstr;

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
  nsAutoCMonitor(this);
  *aResult = nsnull;
  nsString tmpstr;
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
  nsAutoCMonitor(this);
  if (!elements)
    return NS_ERROR_INVALID_POINTER;

  nsresult ret = mProps->EnumerateProperties(elements);

  return ret;
}

NS_IMETHODIMP
nsStringBundle::GetSimpleEnumeration(nsISimpleEnumerator** elements)
{
  if (!elements)
    return NS_ERROR_INVALID_POINTER;

  nsresult ret = mProps->SimpleEnumerateProperties(elements);

  return ret;
}


nsresult
nsStringBundle::GetInputStream(const char* aURLSpec, nsILocale* aLocale, nsIInputStream*& in) 
{
  in = nsnull;

  nsresult ret = NS_OK;

  /* locale binding */
  nsString  strFile2;

#if 1
   /* plan A: don't fallback; use aURLSpec: xxx.pro -> xxx.pro
   */
   strFile2.AssignWithConversion(aURLSpec);
   ret = OpenInputStream(strFile2, in);
#else
  nsString   lc_lang;
  nsString   lc_country;

  if (NS_OK == (ret = GetLangCountry(aLocale, lc_lang, lc_country))) {
    
    /* find the place to concatenate locale name 
     */
    PRInt32   count = 0;
    nsString strFile(aURLSpec);
    PRInt32   mylen = strFile.Length();
    nsString fileLeft;
 
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
    nsString fileRight;
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
#endif 
  return ret;
}

nsresult
nsStringBundle::OpenInputStream(nsString& aURLStr, nsIInputStream*& in) 
{
#ifdef DEBUG_tao_
  {
    char *s = aURLStr.ToNewCString();
    printf("\n** nsStringBundle::OpenInputStream: %s\n", s?s:"null");
    delete s;
  }
#endif
  nsresult ret;
  nsIURI* uri;
  ret = NS_NewURI(&uri, aURLStr);
  if (NS_FAILED(ret)) return ret;

  ret = NS_OpenURI(&in, uri);
  NS_RELEASE(uri);
  return ret;
}

nsresult 
nsStringBundle::GetLangCountry(nsILocale* aLocale, nsString& lang, nsString& country)
{
  if (!aLocale) {
    return NS_ERROR_FAILURE;
  }

  PRUnichar *lc_name_unichar;
  nsString	  lc_name;
  nsString  	category; category.AssignWithConversion("NSILOCALE_MESSAGES");
  nsresult	  result	 = aLocale->GetCategory(category.GetUnicode(), &lc_name_unichar);
  lc_name.Assign(lc_name_unichar);
  nsAllocator::Free(lc_name_unichar);

  NS_ASSERTION(NS_SUCCEEDED(result),"nsStringBundle::GetLangCountry: locale.GetCategory failed");
  NS_ASSERTION(lc_name.Length()>0,"nsStringBundle::GetLangCountry: locale.GetCategory failed");

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

/**
 * An extesible implementation of the StringBudle interface.
 *
 * @created         28/Dec/1999
 * @author  Catalin Rotaru [CATA]
 */
class nsExtensibleStringBundle : public nsIStringBundle
{
  NS_DECL_ISUPPORTS

private:
  
  nsISupportsArray * mBundle;

public:

  nsExtensibleStringBundle(const char * aRegistryKey, nsILocale * aLocale, 
      nsresult * aResult);
  virtual ~nsExtensibleStringBundle();

  //--------------------------------------------------------------------------
  // Interface nsIStringBundle [declaration]

  NS_IMETHOD GetStringFromID(PRInt32 aID, PRUnichar ** aResult);
  NS_IMETHOD GetStringFromName(const PRUnichar *aName, PRUnichar ** aResult);
  NS_IMETHOD GetEnumeration(nsIBidirectionalEnumerator ** aResult); 
  NS_IMETHOD GetSimpleEnumeration(nsISimpleEnumerator** elements);
};

NS_IMPL_ISUPPORTS(nsExtensibleStringBundle, NS_GET_IID(nsIStringBundle));

nsExtensibleStringBundle::nsExtensibleStringBundle(const char * aRegistryKey, 
                                                  nsILocale * aLocale, 
                                                  nsresult * aResult)
                                                  :mBundle(NULL)
{
  NS_INIT_REFCNT();

  nsresult res = NS_OK;
  nsIEnumerator * components = NULL;
  nsIRegistry * registry = NULL;
  nsRegistryKey uconvKey, key;
  nsIStringBundleService * sbServ = NULL;
  PRBool regOpen = PR_FALSE;

  // get the Bundle Service
  res = nsServiceManager::GetService(kStringBundleServiceCID, 
      NS_GET_IID(nsIStringBundleService), (nsISupports **)&sbServ);
  if (NS_FAILED(res)) goto done;

  // get the registry
  res = nsServiceManager::GetService(NS_REGISTRY_PROGID, 
    NS_GET_IID(nsIRegistry), (nsISupports**)&registry);
  if (NS_FAILED(res)) goto done;

  // open registry if necessary
  registry->IsOpen(&regOpen);
  if (!regOpen) {
    res = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    if (NS_FAILED(res)) goto done;
  }

  // get subtree
  res = registry->GetSubtree(nsIRegistry::Common,  
    aRegistryKey, &uconvKey);
  if (NS_FAILED(res)) goto done;

  // enumerate subtrees
  res = registry->EnumerateSubtrees(uconvKey, &components);
  if (NS_FAILED(res)) goto done;
  res = components->First();
  if (NS_FAILED(res)) goto done;

  // create the bundles array
  res = NS_NewISupportsArray(&mBundle);
  if (NS_FAILED(res)) goto done;

  while (NS_OK != components->IsDone()) {
    nsISupports * base = NULL;
    nsIRegistryNode * node = NULL;
    char * name = NULL;
    nsIStringBundle * bundle = NULL;

    res = components->CurrentItem(&base);
    if (NS_FAILED(res)) goto done1;

    res = base->QueryInterface(NS_GET_IID(nsIRegistryNode), (void**)&node);
    if (NS_FAILED(res)) goto done1;

    res = node->GetKey(&key);
    if (NS_FAILED(res)) goto done1;

    res = registry->GetStringUTF8(key, "name", &name);
    if (NS_FAILED(res)) goto done1;

    res = sbServ->CreateBundle(name, aLocale, &bundle);
    if (NS_FAILED(res)) goto done1;

    res = mBundle->AppendElement(bundle);
    if (NS_FAILED(res)) goto done1;

    // printf("Name = %s\n", name);

done1:
    NS_IF_RELEASE(base);
    NS_IF_RELEASE(node);
    NS_IF_RELEASE(bundle);

    if (name != NULL) nsCRT::free(name);

    res = components->Next();
    if (NS_FAILED(res)) break; // this is NOT supposed to fail!
  }

  // finish and clean up
done:
  if (registry != NULL) {
    nsServiceManager::ReleaseService(NS_REGISTRY_PROGID, registry);
  }
  if (sbServ != NULL) nsServiceManager::ReleaseService(
      kStringBundleServiceCID, sbServ);

  NS_IF_RELEASE(components);

  *aResult = res;
}

nsExtensibleStringBundle::~nsExtensibleStringBundle() 
{
  NS_IF_RELEASE(mBundle);
}

nsresult nsExtensibleStringBundle::GetStringFromID(PRInt32 aID, PRUnichar ** aResult)
{
  nsresult res = NS_OK;
  PRUint32 size, i;

  res = mBundle->Count(&size);
  if (NS_FAILED(res)) return res;

  for (i = 0; i < size; i++) {
    nsISupports * sBundle = NULL;
    nsIStringBundle * bundle = NULL;

    res = mBundle->GetElementAt(i, &sBundle);
    if (NS_FAILED(res)) goto done;

    res = sBundle->QueryInterface(NS_GET_IID(nsIStringBundle), (void **)(&bundle));
    if (NS_FAILED(res)) goto done;

    res = bundle->GetStringFromID(aID, aResult);
    if (NS_FAILED(res)) goto done;

done:
    NS_IF_RELEASE(bundle);
    NS_IF_RELEASE(sBundle);

    if (NS_SUCCEEDED(res)) return res;
  }

  return NS_ERROR_FAILURE;
}

nsresult nsExtensibleStringBundle::GetStringFromName(const PRUnichar *aName, 
                                                     PRUnichar ** aResult)
{
  nsresult res = NS_OK;
  PRUint32 size, i;

  res = mBundle->Count(&size);
  if (NS_FAILED(res)) return res;

  for (i = 0; i < size; i++) {
    nsISupports * sBundle = NULL;
    nsIStringBundle * bundle = NULL;

    res = mBundle->GetElementAt(i, &sBundle);
    if (NS_FAILED(res)) goto done;

    res = sBundle->QueryInterface(NS_GET_IID(nsIStringBundle), (void **)(&bundle));
    if (NS_FAILED(res)) goto done;

    res = bundle->GetStringFromName(aName, aResult);
    if (NS_FAILED(res)) goto done;

done:
    NS_IF_RELEASE(bundle);
    NS_IF_RELEASE(sBundle);

    if (NS_SUCCEEDED(res)) return res;
  }

  return NS_ERROR_FAILURE;
}

nsresult nsExtensibleStringBundle::GetEnumeration(nsIBidirectionalEnumerator ** aResult)
{
  // XXX write me
  *aResult = NULL;
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult nsExtensibleStringBundle::GetSimpleEnumeration(nsISimpleEnumerator ** aResult)
{
  // XXX write me
  *aResult = NULL;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/////////////////////////////////////////////////////////////////////////////////////////

#define MAX_CACHED_BUNDLES 10

struct bundleCacheEntry_t {
  PRCList list;
  nsOpaqueKey *mHashKey;
  // do not use a nsCOMPtr - this is a struct not a class!
  nsIStringBundle* mBundle;
};

class nsStringBundleService : public nsIStringBundleService
{
public:
  nsStringBundleService();
  virtual ~nsStringBundleService();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLESERVICE
    
private:
  nsresult getStringBundle(const char *aUrl, nsILocale* aLocale,
                           nsIStringBundle** aResult);
  
  bundleCacheEntry_t *insertIntoCache(nsIStringBundle *aBundle,
                                      nsOpaqueKey *aHashKey);

  static void recycleEntry(bundleCacheEntry_t*);
  
  nsHashtable mBundleMap;
  PRCList mBundleCache;
  PLArenaPool mCacheEntryPool;
};

nsStringBundleService::nsStringBundleService() :
  mBundleMap(MAX_CACHED_BUNDLES, PR_TRUE)
{
#ifdef DEBUG_tao_
  printf("\n++ nsStringBundleService::nsStringBundleService ++\n");
#endif
  NS_INIT_REFCNT();

  PR_INIT_CLIST(&mBundleCache);
  PL_InitArenaPool(&mCacheEntryPool, "srEntries",
                   sizeof(bundleCacheEntry_t)*MAX_CACHED_BUNDLES,
                   sizeof(bundleCacheEntry_t));
}

nsStringBundleService::~nsStringBundleService()
{
  PRCList *current = PR_LIST_HEAD(&mBundleCache);
  while (current != &mBundleCache) {
    bundleCacheEntry_t *cacheEntry = (bundleCacheEntry_t*)current;

    recycleEntry(cacheEntry);
    
    current = PR_NEXT_LINK(current);
  }
  
  PL_FinishArenaPool(&mCacheEntryPool);
}

NS_IMPL_THREADSAFE_ISUPPORTS(nsStringBundleService, NS_GET_IID(nsIStringBundleService))

nsresult
nsStringBundleService::getStringBundle(const char *aURLSpec,
                                       nsILocale* aLocale,
                                       nsIStringBundle **aResult)
{
  nsresult ret;
  
  nsAutoString lc_country;
  nsAutoString lc_lang;
  nsStringBundle::GetLangCountry(aLocale, lc_lang, lc_country);
  
  nsStringKey urlKey(aURLSpec);
  nsStringKey countryKey(lc_country);
  nsStringKey langKey(lc_lang);

  // create a "composite" key by stuffing all the keys into an array
  // and hashing that.
  
  PRUint32 opaqueValue[3];
  opaqueValue[0] = urlKey.HashValue();
  opaqueValue[1] = countryKey.HashValue();
  opaqueValue[2] = langKey.HashValue();

  nsOpaqueKey completeKey((char *)opaqueValue, sizeof(PRUint32)*3);

  bundleCacheEntry_t* cacheEntry =
    (bundleCacheEntry_t*)mBundleMap.Get(&completeKey);
  
  if (cacheEntry) {
    // cache hit!
    // remove it from the list, it will later be reinserted
    // at the head of the list
    PR_REMOVE_LINK((PRCList*)cacheEntry);
    
  } else {

    // hasn't been cached, so insert it into the hash table
    nsStringBundle* bundle = new nsStringBundle(aURLSpec, nsnull, &ret);
    if (!bundle) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    cacheEntry = insertIntoCache(bundle, &completeKey);
  }

  // at this point the cacheEntry should exist in the hashtable,
  // but is not in the LRU cache.
  // put the cache entry at the front of the list
  
  PR_INSERT_LINK((PRCList *)cacheEntry, &mBundleCache);

  // finally, return the value
  *aResult = cacheEntry->mBundle;
  NS_ADDREF(*aResult);

  return NS_OK;
}

bundleCacheEntry_t *
nsStringBundleService::insertIntoCache(nsIStringBundle* aBundle,
                                       nsOpaqueKey* aHashKey)
{
  bundleCacheEntry_t *cacheEntry;
  
  if (mBundleMap.Count() < MAX_CACHED_BUNDLES) {
    // create a new entry
    
    void *cacheEntryArena;
    PL_ARENA_ALLOCATE(cacheEntryArena, &mCacheEntryPool, sizeof(bundleCacheEntry_t));
    cacheEntry = (bundleCacheEntry_t*)cacheEntryArena;
      
  } else {
    // take the last entry in the list, and recycle it.
    cacheEntry = (bundleCacheEntry_t*)PR_LIST_TAIL(&mBundleCache);
      
    // remove it from the hash table and linked list
    NS_ASSERTION(mBundleMap.Exists(cacheEntry->mHashKey),
                 "Element will not be removed!");
    mBundleMap.Remove(cacheEntry->mHashKey);
    PR_REMOVE_LINK((PRCList*)cacheEntry);

    // free up excess memory
    recycleEntry(cacheEntry);
  }
    
  // at this point we have a new cacheEntry that doesn't exist
  // in the hashtable, so set up the cacheEntry
  cacheEntry->mBundle = aBundle;
  NS_ADDREF(cacheEntry->mBundle);

  // need to make a copy of it for the hashkey
  char* opaqueValue = (char*)PR_Malloc(sizeof (PRUint32)*3);
  nsCRT::memcpy(opaqueValue, aHashKey->GetKey(), sizeof(PRUint32)*3);
  
  cacheEntry->mHashKey = new nsOpaqueKey(opaqueValue,
                                         aHashKey->GetKeyLength());

  // insert the entry into the cache and map, make it the MRU
  mBundleMap.Put(cacheEntry->mHashKey, cacheEntry);

  return cacheEntry;
}

void
nsStringBundleService::recycleEntry(bundleCacheEntry_t *aEntry)
{
  PR_Free(NS_CONST_CAST(char *,aEntry->mHashKey->GetKey()));
  delete aEntry->mHashKey;
  NS_RELEASE(aEntry->mBundle);
}

NS_IMETHODIMP
nsStringBundleService::CreateBundle(const char* aURLSpec, nsILocale* aLocale,
                                    nsIStringBundle** aResult)
{
#ifdef DEBUG_tao_
  printf("\n++ nsStringBundleService::CreateBundle ++\n");
  {
    nsString aURLStr(aURLSpec);
    char *s = aURLStr.ToNewCString();
    printf("\n** nsStringBundleService::CreateBundle: %s\n", s?s:"null");
    delete s;
  }
#endif

  
  return getStringBundle(aURLSpec, aLocale, aResult);
}

NS_IMETHODIMP
nsStringBundleService::CreateExtensibleBundle(const char* aRegistryKey, 
                                              nsILocale* aLocale,
                                              nsIStringBundle** aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;

  nsExtensibleStringBundle * bundle = new nsExtensibleStringBundle(
      aRegistryKey, aLocale, &res);
  if (!bundle) return NS_ERROR_OUT_OF_MEMORY;
  if (NS_FAILED(res)) {
    delete bundle;
    return res;
  }

  res = bundle->QueryInterface(NS_GET_IID(nsIStringBundle), (void**) aResult);
  if (NS_FAILED(res)) delete bundle;

  return res;
}


NS_IMETHODIMP
NS_NewStringBundleService(nsISupports* aOuter, const nsIID& aIID,
                          void** aResult)
{
  nsresult rv;

  if (!aResult) {                                                  
    return NS_ERROR_INVALID_POINTER;                             
  }                                                                
  if (aOuter) {                                                    
    *aResult = nsnull;                                           
    return NS_ERROR_NO_AGGREGATION;                              
  }                                                                
  nsStringBundleService * inst = new nsStringBundleService();
  if (inst == NULL) {                                             
    *aResult = nsnull;                                           
    return NS_ERROR_OUT_OF_MEMORY;
  }                                                                
  rv = inst->QueryInterface(aIID, aResult);                        
  if (NS_FAILED(rv)) {
    delete inst;
    *aResult = nsnull;                                           
  }                                                              
  return rv;                                                     
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsStringBundleService)

static nsModuleComponentInfo components[] =
{
   { "String Bundle", NS_STRINGBUNDLESERVICE_CID, NS_STRINGBUNDLE_PROGID, nsStringBundleServiceConstructor}
};

NS_IMPL_NSGETMODULE("nsStringBundleModule", components)

