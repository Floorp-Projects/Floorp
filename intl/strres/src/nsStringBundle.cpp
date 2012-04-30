/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsStringBundle.h"
#include "nsID.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIStringBundle.h"
#include "nsStringBundleService.h"
#include "nsStringBundle.h"
#include "nsStringBundleTextOverride.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIMutableArray.h"
#include "nsArrayEnumerator.h"
#include "nscore.h"
#include "nsHashtable.h"
#include "nsMemory.h"
#include "plstr.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsIMemory.h"
#include "nsIObserverService.h"
#include "pratom.h"
#include "prmem.h"
#include "nsCOMArray.h"
#include "nsTextFormatter.h"
#include "nsIErrorService.h"
#include "nsICategoryManager.h"

#include "nsPrintfCString.h"
// for async loading
#ifdef ASYNC_LOADING
#include "nsIBinaryInputStream.h"
#include "nsIStringStream.h"
#endif

#include "prenv.h"
#include "nsCRT.h"

using namespace mozilla;

static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);
static NS_DEFINE_CID(kPersistentPropertiesCID, NS_IPERSISTENTPROPERTIES_CID);

nsStringBundle::~nsStringBundle()
{
}

nsStringBundle::nsStringBundle(const char* aURLSpec,
                               nsIStringBundleOverride* aOverrideStrings) :
  mPropertiesURL(aURLSpec),
  mOverrideStrings(aOverrideStrings),
  mReentrantMonitor("nsStringBundle.mReentrantMonitor"),
  mAttemptedLoad(false),
  mLoaded(false)
{
}

nsresult
nsStringBundle::LoadProperties()
{
  // this is different than mLoaded, because we only want to attempt
  // to load once
  // we only want to load once, but if we've tried once and failed,
  // continue to throw an error!
  if (mAttemptedLoad) {
    if (mLoaded)
      return NS_OK;
      
    return NS_ERROR_UNEXPECTED;
  }
  
  mAttemptedLoad = true;

  nsresult rv;

  // do it synchronously
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), mPropertiesURL);
  if (NS_FAILED(rv)) return rv;

  // We don't use NS_OpenURI because we want to tweak the channel
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel), uri);
  if (NS_FAILED(rv)) return rv;

  // It's a string bundle.  We expect a text/plain type, so set that as hint
  channel->SetContentType(NS_LITERAL_CSTRING("text/plain"));
  
  nsCOMPtr<nsIInputStream> in;
  rv = channel->Open(getter_AddRefs(in));
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(NS_SUCCEEDED(rv) && in, "Error in OpenBlockingStream");
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && in, NS_ERROR_FAILURE);
    
  mProps = do_CreateInstance(kPersistentPropertiesCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  mAttemptedLoad = mLoaded = true;
  rv = mProps->Load(in);

  mLoaded = NS_SUCCEEDED(rv);
  
  return rv;
}


nsresult
nsStringBundle::GetStringFromID(PRInt32 aID, nsAString& aResult)
{  
  ReentrantMonitorAutoEnter automon(mReentrantMonitor);
  nsCAutoString name;
  name.AppendInt(aID, 10);

  nsresult rv;
  
  // try override first
  if (mOverrideStrings) {
    rv = mOverrideStrings->GetStringFromName(mPropertiesURL,
                                             name,
                                             aResult);
    if (NS_SUCCEEDED(rv)) return rv;
  }
  
  rv = mProps->GetStringProperty(name, aResult);

#ifdef DEBUG_tao_
  char *s = ToNewCString(aResult);
  printf("\n** GetStringFromID: aResult=%s, len=%d\n", s?s:"null", 
         aResult.Length());
  if (s) nsMemory::Free(s);
#endif /* DEBUG_tao_ */

  return rv;
}

nsresult
nsStringBundle::GetStringFromName(const nsAString& aName,
                                  nsAString& aResult)
{
  nsresult rv;

  // try override first
  if (mOverrideStrings) {
    rv = mOverrideStrings->GetStringFromName(mPropertiesURL,
                                             NS_ConvertUTF16toUTF8(aName),
                                             aResult);
    if (NS_SUCCEEDED(rv)) return rv;
  }
  
  rv = mProps->GetStringProperty(NS_ConvertUTF16toUTF8(aName), aResult);
#ifdef DEBUG_tao_
  char *s = ToNewCString(aResult),
       *ss = ToNewCString(aName);
  printf("\n** GetStringFromName: aName=%s, aResult=%s, len=%d\n", 
         ss?ss:"null", s?s:"null", aResult.Length());
  if (s)  nsMemory::Free(s);
  if (ss) nsMemory::Free(ss);
#endif /* DEBUG_tao_ */
  return rv;
}

NS_IMETHODIMP
nsStringBundle::FormatStringFromID(PRInt32 aID,
                                   const PRUnichar **aParams,
                                   PRUint32 aLength,
                                   PRUnichar ** aResult)
{
  nsAutoString idStr;
  idStr.AppendInt(aID, 10);

  return FormatStringFromName(idStr.get(), aParams, aLength, aResult);
}

// this function supports at most 10 parameters.. see below for why
NS_IMETHODIMP
nsStringBundle::FormatStringFromName(const PRUnichar *aName,
                                     const PRUnichar **aParams,
                                     PRUint32 aLength,
                                     PRUnichar **aResult)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ASSERTION(aParams && aLength, "FormatStringFromName() without format parameters: use GetStringFromName() instead");
  NS_ENSURE_ARG_POINTER(aResult);

  nsresult rv;
  rv = LoadProperties();
  if (NS_FAILED(rv)) return rv;
  
  nsAutoString formatStr;
  rv = GetStringFromName(nsDependentString(aName), formatStr);
  if (NS_FAILED(rv)) return rv;

  return FormatString(formatStr.get(), aParams, aLength, aResult);
}
                                     

NS_IMPL_THREADSAFE_ISUPPORTS1(nsStringBundle,
                              nsIStringBundle)

/* void GetStringFromID (in long aID, out wstring aResult); */
NS_IMETHODIMP
nsStringBundle::GetStringFromID(PRInt32 aID, PRUnichar **aResult)
{
  nsresult rv;
  rv = LoadProperties();
  if (NS_FAILED(rv)) return rv;
  
  *aResult = nsnull;
  nsAutoString tmpstr;

  rv = GetStringFromID(aID, tmpstr);
  NS_ENSURE_SUCCESS(rv, rv);

  *aResult = ToNewUnicode(tmpstr);
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

/* void GetStringFromName (in wstring aName, out wstring aResult); */
NS_IMETHODIMP 
nsStringBundle::GetStringFromName(const PRUnichar *aName, PRUnichar **aResult)
{
  NS_ENSURE_ARG_POINTER(aName);
  NS_ENSURE_ARG_POINTER(aResult);

  nsresult rv;
  rv = LoadProperties();
  if (NS_FAILED(rv)) return rv;

  ReentrantMonitorAutoEnter automon(mReentrantMonitor);
  *aResult = nsnull;
  nsAutoString tmpstr;
  rv = GetStringFromName(nsDependentString(aName), tmpstr);
  if (NS_FAILED(rv))
  {
#if 0
    // it is not uncommon for apps to request a string name which may not exist
    // so be quiet about it. 
    NS_WARNING("String missing from string bundle");
    printf("  '%s' missing from bundle %s\n", NS_ConvertUTF16toUTF8(aName).get(), mPropertiesURL.get());
#endif
    return rv;
  }

  *aResult = ToNewUnicode(tmpstr);
  NS_ENSURE_TRUE(*aResult, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

nsresult
nsStringBundle::GetCombinedEnumeration(nsIStringBundleOverride* aOverrideStrings,
                                       nsISimpleEnumerator** aResult)
{
  nsCOMPtr<nsISupports> supports;
  nsCOMPtr<nsIPropertyElement> propElement;
  
  nsresult rv;

  nsCOMPtr<nsIMutableArray> resultArray =
    do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // first, append the override elements
  nsCOMPtr<nsISimpleEnumerator> overrideEnumerator;
  rv = aOverrideStrings->EnumerateKeysInBundle(mPropertiesURL,
                                               getter_AddRefs(overrideEnumerator));
  
  bool hasMore;
  rv = overrideEnumerator->HasMoreElements(&hasMore);
  NS_ENSURE_SUCCESS(rv, rv);
  while (hasMore) {

    rv = overrideEnumerator->GetNext(getter_AddRefs(supports));
    if (NS_SUCCEEDED(rv))
      resultArray->AppendElement(supports, false);

    rv = overrideEnumerator->HasMoreElements(&hasMore);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // ok, now we have the override elements in resultArray
  nsCOMPtr<nsISimpleEnumerator> propEnumerator;
  rv = mProps->Enumerate(getter_AddRefs(propEnumerator));
  if (NS_FAILED(rv)) {
    // no elements in mProps anyway, just return what we have
    return NS_NewArrayEnumerator(aResult, resultArray);
  }

  // second, append all the elements that are in mProps
  do {
    rv = propEnumerator->GetNext(getter_AddRefs(supports));
    if (NS_SUCCEEDED(rv) &&
        (propElement = do_QueryInterface(supports, &rv))) {

      // now check if its in the override bundle
      nsCAutoString key;
      propElement->GetKey(key);

      nsAutoString value;
      rv = aOverrideStrings->GetStringFromName(mPropertiesURL, key, value);

      // if it isn't there, then it is safe to append
      if (NS_FAILED(rv))
        resultArray->AppendElement(propElement, false);
    }

    rv = propEnumerator->HasMoreElements(&hasMore);
    NS_ENSURE_SUCCESS(rv, rv);
  } while (hasMore);

  return resultArray->Enumerate(aResult);
}
                                

NS_IMETHODIMP
nsStringBundle::GetSimpleEnumeration(nsISimpleEnumerator** elements)
{
  if (!elements)
    return NS_ERROR_INVALID_POINTER;

  nsresult rv;
  rv = LoadProperties();
  if (NS_FAILED(rv)) return rv;
  
  if (mOverrideStrings)
      return GetCombinedEnumeration(mOverrideStrings, elements);
  
  return mProps->Enumerate(elements);
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
  PRUnichar *text = 
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

  if (!text) {
    *aResult = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // nsTextFormatter does not use the shared nsMemory allocator.
  // Instead it is required to free the memory it allocates using
  // nsTextFormatter::smprintf_free.  Let's instead use nsMemory based
  // allocation for the result that we give out and free the string
  // returned by smprintf ourselves!
  *aResult = NS_strdup(text);
  nsTextFormatter::smprintf_free(text);

  return *aResult ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMPL_ISUPPORTS1(nsExtensibleStringBundle, nsIStringBundle)

nsExtensibleStringBundle::nsExtensibleStringBundle()
{
  mLoaded = false;
}

nsresult
nsExtensibleStringBundle::Init(const char * aCategory,
                               nsIStringBundleService* aBundleService) 
{

  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  rv = catman->EnumerateCategory(aCategory, getter_AddRefs(enumerator));
  if (NS_FAILED(rv)) return rv;

  bool hasMore;
  while (NS_SUCCEEDED(enumerator->HasMoreElements(&hasMore)) && hasMore) {
    nsCOMPtr<nsISupports> supports;
    rv = enumerator->GetNext(getter_AddRefs(supports));
    if (NS_FAILED(rv))
      continue;

    nsCOMPtr<nsISupportsCString> supStr = do_QueryInterface(supports, &rv);
    if (NS_FAILED(rv))
      continue;

    nsCAutoString name;
    rv = supStr->GetData(name);
    if (NS_FAILED(rv))
      continue;

    nsCOMPtr<nsIStringBundle> bundle;
    rv = aBundleService->CreateBundle(name.get(), getter_AddRefs(bundle));
    if (NS_FAILED(rv))
      continue;

    mBundles.AppendObject(bundle);
  }

  return rv;
}

nsExtensibleStringBundle::~nsExtensibleStringBundle() 
{
}

nsresult nsExtensibleStringBundle::GetStringFromID(PRInt32 aID, PRUnichar ** aResult)
{
  nsresult rv;
  const PRUint32 size = mBundles.Count();
  for (PRUint32 i = 0; i < size; ++i) {
    nsIStringBundle *bundle = mBundles[i];
    if (bundle) {
      rv = bundle->GetStringFromID(aID, aResult);
      if (NS_SUCCEEDED(rv))
        return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult nsExtensibleStringBundle::GetStringFromName(const PRUnichar *aName,
                                                     PRUnichar ** aResult)
{
  nsresult rv;
  const PRUint32 size = mBundles.Count();
  for (PRUint32 i = 0; i < size; ++i) {
    nsIStringBundle* bundle = mBundles[i];
    if (bundle) {
      rv = bundle->GetStringFromName(aName, aResult);
      if (NS_SUCCEEDED(rv))
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
  return FormatStringFromName(idStr.get(), aParams, aLength, aResult);
}

NS_IMETHODIMP
nsExtensibleStringBundle::FormatStringFromName(const PRUnichar *aName,
                                               const PRUnichar ** aParams,
                                               PRUint32 aLength,
                                               PRUnichar ** aResult)
{
  nsXPIDLString formatStr;
  nsresult rv;
  rv = GetStringFromName(aName, getter_Copies(formatStr));
  if (NS_FAILED(rv))
    return rv;

  return nsStringBundle::FormatString(formatStr, aParams, aLength, aResult);
}

nsresult nsExtensibleStringBundle::GetSimpleEnumeration(nsISimpleEnumerator ** aResult)
{
  // XXX write me
  *aResult = NULL;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/////////////////////////////////////////////////////////////////////////////////////////

#define MAX_CACHED_BUNDLES 16

struct bundleCacheEntry_t {
  PRCList list;
  nsCStringKey *mHashKey;
  // do not use a nsCOMPtr - this is a struct not a class!
  nsIStringBundle* mBundle;
};


nsStringBundleService::nsStringBundleService() :
  mBundleMap(MAX_CACHED_BUNDLES, true)
{
#ifdef DEBUG_tao_
  printf("\n++ nsStringBundleService::nsStringBundleService ++\n");
#endif

  PR_INIT_CLIST(&mBundleCache);
  PL_InitArenaPool(&mCacheEntryPool, "srEntries",
                   sizeof(bundleCacheEntry_t)*MAX_CACHED_BUNDLES,
                   sizeof(bundleCacheEntry_t));

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
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    os->AddObserver(this, "memory-pressure", true);
    os->AddObserver(this, "profile-do-change", true);
    os->AddObserver(this, "chrome-flush-caches", true);
    os->AddObserver(this, "xpcom-category-entry-added", true);
  }

  // instantiate the override service, if there is any.
  // at some point we probably want to make this a category, and
  // support multiple overrides
  mOverrideStrings = do_GetService(NS_STRINGBUNDLETEXTOVERRIDE_CONTRACTID);
  
  return NS_OK;
}

NS_IMETHODIMP
nsStringBundleService::Observe(nsISupports* aSubject,
                               const char* aTopic,
                               const PRUnichar* aSomeData)
{
  if (strcmp("memory-pressure", aTopic) == 0 ||
      strcmp("profile-do-change", aTopic) == 0 ||
      strcmp("chrome-flush-caches", aTopic) == 0)
  {
    flushBundleCache();
  }
  else if (strcmp("xpcom-category-entry-added", aTopic) == 0 &&
           NS_LITERAL_STRING("xpcom-autoregistration").Equals(aSomeData)) 
  {
    mOverrideStrings = do_GetService(NS_STRINGBUNDLETEXTOVERRIDE_CONTRACTID);
  }
  
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
                                       nsIStringBundle **aResult)
{
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
    nsStringBundle* bundle = new nsStringBundle(aURLSpec, mOverrideStrings);
    if (!bundle) return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(bundle);
    
    cacheEntry = insertIntoCache(bundle, &completeKey);
    NS_RELEASE(bundle);         // cache should now be holding a ref
                                // in the cacheEntry
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
#ifdef DEBUG_alecf
    NS_WARNING(nsPrintfCString("Booting %s to make room for %s\n",
                               cacheEntry->mHashKey->GetString(),
                               aHashKey->GetString()).get());
#endif
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
nsStringBundleService::CreateBundle(const char* aURLSpec, 
                                    nsIStringBundle** aResult)
{
#ifdef DEBUG_tao_
  printf("\n++ nsStringBundleService::CreateBundle ++\n");
  printf("\n** nsStringBundleService::CreateBundle: %s\n", aURLSpec ? aURLSpec : "null");
#endif

  return getStringBundle(aURLSpec,aResult);
}

NS_IMETHODIMP
nsStringBundleService::CreateExtensibleBundle(const char* aCategory,
                                              nsIStringBundle** aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;

  nsresult res;

  nsExtensibleStringBundle * bundle = new nsExtensibleStringBundle();
  if (!bundle) return NS_ERROR_OUT_OF_MEMORY;

  res = bundle->Init(aCategory, this);
  if (NS_FAILED(res)) {
    delete bundle;
    return res;
  }

  res = bundle->QueryInterface(NS_GET_IID(nsIStringBundle), (void**) aResult);
  if (NS_FAILED(res)) delete bundle;

  return res;
}

#define GLOBAL_PROPERTIES "chrome://global/locale/global-strres.properties"

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
    rv = bundle->FormatStringFromName(NS_ConvertASCIItoUTF16(key).get(),
                                      (const PRUnichar**)argArray, 
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
    otherArgArray[0] = statusStr.get();
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
    NS_ENSURE_TRUE(*result, NS_ERROR_OUT_OF_MEMORY);
    return NS_OK;
  }

  if (aStatus == NS_OK) {
    return NS_ERROR_FAILURE;       // no message to format
  }

  // format the arguments:
  const nsDependentString args(aStatusArg);
  argCount = args.CountChar(PRUnichar('\n')) + 1;
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
      PRInt32 pos = args.FindChar('\n', offset);
      if (pos == -1) 
        pos = args.Length();
      argArray[i] = ToNewUnicode(Substring(args, offset, pos - offset));
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
    rv = getStringBundle(stringBundleURL, getter_AddRefs(bundle));
    if (NS_SUCCEEDED(rv)) {
      rv = FormatWithBundle(bundle, aStatus, argCount, argArray, result);
    }
  }
  if (NS_FAILED(rv)) {
    rv = getStringBundle(GLOBAL_PROPERTIES, getter_AddRefs(bundle));
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

