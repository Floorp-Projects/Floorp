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
#include "nsIPersistentProperties2.h"
#include "nsIStringBundle.h"
#include "nscore.h"
#include "nsILocale.h"
#include "nsMemory.h"
#include "plstr.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIGenericFactory.h"
#include "nsIMemory.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
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
#include "nsTextFormatter.h"
#include "nsIErrorService.h"

#include "nsAcceptLang.h" // for nsIAcceptLang

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);

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

  static nsresult FormatString(const PRUnichar *formatStr,
                               const PRUnichar **aParams, PRUint32 aLength,
                               PRUnichar **aResult);
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
  if (!mProps)
    return NS_OK;
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
  if (!mProps)
    return NS_OK;
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

NS_IMETHODIMP
nsStringBundle::FormatStringFromID(PRInt32 aID,
                                   const PRUnichar **aParams,
                                   PRUint32 aLength,
                                   PRUnichar ** aResult)
{
  nsAutoString idStr;
  idStr.AppendInt(aID, 10);

  return FormatStringFromName(idStr.GetUnicode(), aParams, aLength, aResult);
}

// this function supports at most 10 parameters.. see below for why
NS_IMETHODIMP
nsStringBundle::FormatStringFromName(const PRUnichar *aName,
                                     const PRUnichar **aParams,
                                     PRUint32 aLength,
                                     PRUnichar **aResult)
{
  nsresult rv;
  nsAutoString formatStr;
  rv = GetStringFromName(nsAutoString(aName), formatStr);
  if (NS_FAILED(rv)) return rv;

  return FormatString(formatStr.GetUnicode(), aParams, aLength, aResult);
}
                                     

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStringBundle, nsIStringBundle)

/* void GetStringFromID (in long aID, out wstring aResult); */
NS_IMETHODIMP
nsStringBundle::GetStringFromID(PRInt32 aID, PRUnichar **aResult)
{
  *aResult = nsnull;
  nsAutoString tmpstr;

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
  nsAutoString tmpstr;
  nsAutoString nameStr(aName);
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
  nsAutoString  strFile2;

#if 1
   /* plan A: don't fallback; use aURLSpec: xxx.pro -> xxx.pro
   */
   strFile2.AssignWithConversion(aURLSpec);
   ret = OpenInputStream(strFile2, in);
#else
  nsAutoString   lc_lang;
  nsAutoString   lc_country;

  if (NS_OK == (ret = GetLangCountry(aLocale, lc_lang, lc_country))) {
    
    /* find the place to concatenate locale name 
     */
    PRInt32   count = 0;
    nsAutoString strFile(aURLSpec);
    PRInt32   mylen = strFile.Length();
    nsAutoString fileLeft;
 
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
    nsAutoString fileRight;
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
  nsAutoString	  lc_name;
  nsAutoString  	category; category.AssignWithConversion("NSILOCALE_MESSAGES");
  nsresult	  result	 = aLocale->GetCategory(category.GetUnicode(), &lc_name_unichar);
  lc_name.Assign(lc_name_unichar);
  nsMemory::Free(lc_name_unichar);

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

nsresult
nsStringBundle::FormatString(const PRUnichar *aFormatStr,
                             const PRUnichar **aParams, PRUint32 aLength,
                             PRUnichar **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_ARG(aLength <= 10); // enforce 10-parameter limit

  // implementation note: you would think you could use vsmprintf
  // to build up an arbitrary length array.. except that there
  // is no way to build up a va_list at runtime!
  // Don't believe me? See:
  //   http://www.eskimo.com/~scs/C-faq/q15.13.html
  // -alecf
  *aResult = 
    nsTextFormatter::smprintf(aFormatStr,
                              aLength >= 1 ? aParams[0] : nsnull,
                              aLength >= 2 ? aParams[1] : nsnull,
                              aLength >= 3 ? aParams[2] : nsnull,
                              aLength >= 4 ? aParams[3] : nsnull,
                              aLength >= 5 ? aParams[4] : nsnull,
                              aLength >= 6 ? aParams[5] : nsnull,
                              aLength >= 7 ? aParams[6] : nsnull,
                              aLength >= 8 ? aParams[7] : nsnull,
                              aLength >= 9 ? aParams[8] : nsnull,
                              aLength >= 10 ? aParams[9] : nsnull);
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
  NS_DECL_NSISTRINGBUNDLE
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
  res = nsServiceManager::GetService(NS_REGISTRY_CONTRACTID, 
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
    nsServiceManager::ReleaseService(NS_REGISTRY_CONTRACTID, registry);
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
    nsCOMPtr<nsIStringBundle> bundle;
    res = mBundle->QueryElementAt(i, NS_GET_IID(nsIStringBundle), getter_AddRefs(bundle));
    if (NS_SUCCEEDED(res)) {
        res = bundle->GetStringFromID(aID, aResult);
        if (NS_SUCCEEDED(res))
            return NS_OK;
    }
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
    nsCOMPtr<nsIStringBundle> bundle;
    res = mBundle->QueryElementAt(i, NS_GET_IID(nsIStringBundle), getter_AddRefs(bundle));
    if (NS_SUCCEEDED(res)) {
        res = bundle->GetStringFromName(aName, aResult);
        if (NS_SUCCEEDED(res))
            return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsExtensibleStringBundle::FormatStringFromID(PRInt32 aID,
                                             const PRUnichar ** aParams,
                                             PRUint32 aLength,
                                             PRUnichar ** aResult)
{
  nsAutoString idStr;
  idStr.AppendInt(aID, 10);
  return FormatStringFromName(idStr.GetUnicode(), aParams, aLength, aResult);
}

NS_IMETHODIMP
nsExtensibleStringBundle::FormatStringFromName(const PRUnichar *aName,
                                               const PRUnichar ** aParams,
                                               PRUint32 aLength,
                                               PRUnichar ** aResult)
{
  nsXPIDLString formatStr;
  GetStringFromName(aName, getter_Copies(formatStr));

  return nsStringBundle::FormatString(formatStr, aParams, aLength, aResult);
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
  nsCStringKey *mHashKey;
  // do not use a nsCOMPtr - this is a struct not a class!
  nsIStringBundle* mBundle;
};

class nsStringBundleService : public nsIStringBundleService,
                              public nsIObserver,
                              public nsSupportsWeakReference
{
public:
  nsStringBundleService();
  virtual ~nsStringBundleService();

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLESERVICE
  NS_DECL_NSIOBSERVER
    
private:
  nsresult getStringBundle(const char *aUrl, nsILocale* aLocale,
                           nsIStringBundle** aResult);
  nsresult FormatWithBundle(nsIStringBundle* bundle, nsresult aStatus, 
                            PRUint32 argCount, PRUnichar** argArray,
                            PRUnichar* *result);

  void flushBundleCache();
  
  bundleCacheEntry_t *insertIntoCache(nsIStringBundle *aBundle,
                                      nsCStringKey *aHashKey);

  static void recycleEntry(bundleCacheEntry_t*);
  
  nsHashtable mBundleMap;
  PRCList mBundleCache;
  PLArenaPool mCacheEntryPool;

  // reuse the same uri structure over and over
  nsCOMPtr<nsIURI>              mScratchUri;
  nsCOMPtr<nsIErrorService>     mErrorService;
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

  mScratchUri = do_CreateInstance(kStandardUrlCID);
  NS_ASSERTION(mScratchUri, "Couldn't create scratch URI");
  mErrorService = do_GetService(kErrorServiceCID);
  NS_ASSERTION(mErrorService, "Couldn't get error service");
}

NS_IMPL_THREADSAFE_ISUPPORTS3(nsStringBundleService,
                              nsIStringBundleService,
                              nsIObserver,
                              nsISupportsWeakReference)

nsStringBundleService::~nsStringBundleService()
{
  flushBundleCache();
  PL_FinishArenaPool(&mCacheEntryPool);
}

nsresult
nsStringBundleService::Init()
{
  nsCOMPtr<nsIObserverService> os = do_GetService(NS_OBSERVERSERVICE_CONTRACTID);
  if (os)
    os->AddObserver(this, NS_MEMORY_PRESSURE_TOPIC);

  return NS_OK;
}

NS_IMETHODIMP
nsStringBundleService::Observe(nsISupports* aSubject,
                               const PRUnichar* aTopic,
                               const PRUnichar* aSomeData)
{
  if (nsCRT::strcmp(NS_MEMORY_PRESSURE_TOPIC, aTopic) == 0)
    flushBundleCache();
  return NS_OK;
}

void
nsStringBundleService::flushBundleCache()
{
  // release all bundles in the cache
  mBundleMap.Reset();
  
  PRCList *current = PR_LIST_HEAD(&mBundleCache);
  while (current != &mBundleCache) {
    bundleCacheEntry_t *cacheEntry = (bundleCacheEntry_t*)current;

    recycleEntry(cacheEntry);
    PRCList *oldItem = current;
    current = PR_NEXT_LINK(current);
    
    // will be freed in PL_FreeArenaPool
    PR_REMOVE_LINK(oldItem);
  }
  PL_FreeArenaPool(&mCacheEntryPool);
}

NS_IMETHODIMP
nsStringBundleService::FlushBundles()
{
  flushBundleCache();
  return NS_OK;
}

nsresult
nsStringBundleService::getStringBundle(const char *aURLSpec,
                                       nsILocale* aLocale,
                                       nsIStringBundle **aResult)
{
  nsresult ret;

  nsXPIDLCString newSpec;

  nsCStringKey completeKey(aURLSpec);

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
                                       nsCStringKey* aHashKey)
{
  bundleCacheEntry_t *cacheEntry;
  
  if (mBundleMap.Count() < MAX_CACHED_BUNDLES) {
    // cache not full - create a new entry
    
    void *cacheEntryArena;
    PL_ARENA_ALLOCATE(cacheEntryArena, &mCacheEntryPool, sizeof(bundleCacheEntry_t));
    cacheEntry = (bundleCacheEntry_t*)cacheEntryArena;
      
  } else {
    // cache is full
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

  cacheEntry->mHashKey = (nsCStringKey*)aHashKey->Clone();
  
  // insert the entry into the cache and map, make it the MRU
  mBundleMap.Put(cacheEntry->mHashKey, cacheEntry);

  return cacheEntry;
}

void
nsStringBundleService::recycleEntry(bundleCacheEntry_t *aEntry)
{
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
    nsAutoString aURLStr(aURLSpec);
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

#define GLOBAL_PROPERTIES "chrome://global/locale/xpcom.properties"

nsresult
nsStringBundleService::FormatWithBundle(nsIStringBundle* bundle, nsresult aStatus,
                                        PRUint32 argCount, PRUnichar** argArray,
                                        PRUnichar* *result)
{
  nsresult rv;
  nsXPIDLCString key;

  // then find a key into the string bundle for that particular error:
  rv = mErrorService->GetErrorStringBundleKey(aStatus, getter_Copies(key));

  // first try looking up the error message with the string key:
  if (NS_SUCCEEDED(rv)) {
    nsAutoString name; name.AssignWithConversion(key);
    rv = bundle->FormatStringFromName(name.GetUnicode(), (const PRUnichar**)argArray, 
                                      argCount, result);
  }

  // if the string key fails, try looking up the error message with the int key:
  if (NS_FAILED(rv)) {
    PRUint16 code = NS_ERROR_GET_CODE(aStatus);
    rv = bundle->FormatStringFromID(code, (const PRUnichar**)argArray, argCount, result);
  }

  // If the int key fails, try looking up the default error message. E.g. print:
  //   An unknown error has occurred (0x804B0003).
  if (NS_FAILED(rv)) {
    nsAutoString statusStr; statusStr.AppendInt(aStatus, 16);
    const PRUnichar* otherArgArray[1];
    otherArgArray[0] = statusStr.GetUnicode();
    PRUint16 code = NS_ERROR_GET_CODE(NS_ERROR_FAILURE);
    rv = bundle->FormatStringFromID(code, otherArgArray, 1, result);
  }

  return rv;
}

NS_IMETHODIMP
nsStringBundleService::FormatStatusMessage(nsresult aStatus,
                                           const PRUnichar* aStatusArg,
                                           PRUnichar* *result)
{
  nsresult rv;
  PRUint32 i, argCount = 0;
  nsCOMPtr<nsIStringBundle> bundle;
  nsXPIDLCString stringBundleURL;

  // XXX hack for mailnews who has already formatted their messages:
  if (aStatus == NS_OK && aStatusArg) {
    *result = nsCRT::strdup(aStatusArg);
    return NS_OK;
  }

  if (aStatus == NS_OK) {
    return NS_ERROR_FAILURE;       // no message to format
  }

  // format the arguments:
  nsAutoString args(aStatusArg);
  argCount = args.CountChar('\n') + 1;
  NS_ENSURE_ARG(argCount <= 10); // enforce 10-parameter limit
  PRUnichar* argArray[10];

  // convert the aStatusArg into a PRUnichar array
  if (argCount == 1) {
    // avoid construction for the simple case:
    argArray[0] = (PRUnichar*)aStatusArg;
  }
  else if (argCount > 1) {
    PRInt32 offset = 0;
    for (i = 0; i < argCount; i++) {
      PRInt32 pos = args.FindChar('\n', PR_FALSE, offset);
      if (pos == -1) 
        pos = args.Length();
      nsAutoString arg;
      args.Mid(arg, offset, pos);
      argArray[i] = arg.ToNewUnicode();
      if (argArray[i] == nsnull) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        argCount = i - 1; // don't try to free uninitialized memory
        goto done;
      }
      offset = pos + 1;
    }
  }

  // find the string bundle for the error's module:
  rv = mErrorService->GetErrorStringBundle(NS_ERROR_GET_MODULE(aStatus), 
                                           getter_Copies(stringBundleURL));
  if (NS_SUCCEEDED(rv)) {
    rv = getStringBundle(stringBundleURL, nsnull, getter_AddRefs(bundle));
    if (NS_SUCCEEDED(rv)) {
      rv = FormatWithBundle(bundle, aStatus, argCount, argArray, result);
    }
  }
  if (NS_FAILED(rv)) {
    rv = getStringBundle(GLOBAL_PROPERTIES, nsnull, getter_AddRefs(bundle));
    if (NS_SUCCEEDED(rv)) {
      rv = FormatWithBundle(bundle, aStatus, argCount, argArray, result);
    }
  }

done:
  if (argCount > 1) {
    for (i = 0; i < argCount; i++) {
      if (argArray[i])
        nsMemory::Free(argArray[i]);
    }
  }
  return rv;
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

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStringBundleService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAcceptLang)

static nsModuleComponentInfo components[] =
{
  { "String Bundle", NS_STRINGBUNDLESERVICE_CID, NS_STRINGBUNDLE_CONTRACTID, nsStringBundleServiceConstructor},
  { "Accept Language", NS_ACCEPTLANG_CID, NS_ACCEPTLANG_CONTRACTID, nsAcceptLangConstructor}
};

NS_IMPL_NSGETMODULE("nsStringBundleModule", components)

