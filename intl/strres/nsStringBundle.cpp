/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsStringBundle.h"
#include "nsID.h"
#include "nsString.h"
#include "nsIStringBundle.h"
#include "nsStringBundleService.h"
#include "nsStringBundleTextOverride.h"
#include "nsISupportsPrimitives.h"
#include "nsIMutableArray.h"
#include "nsArrayEnumerator.h"
#include "nscore.h"
#include "nsMemory.h"
#include "nsNetUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIInputStream.h"
#include "nsIURI.h"
#include "nsIObserverService.h"
#include "nsCOMArray.h"
#include "nsTextFormatter.h"
#include "nsIErrorService.h"
#include "nsICategoryManager.h"
#include "nsContentUtils.h"

// for async loading
#ifdef ASYNC_LOADING
#include "nsIBinaryInputStream.h"
#include "nsIStringStream.h"
#endif

using namespace mozilla;

static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);

NS_IMPL_ISUPPORTS(nsStringBundle, nsIStringBundle)

nsStringBundle::nsStringBundle(const char* aURLSpec,
                               nsIStringBundleOverride* aOverrideStrings) :
  mPropertiesURL(aURLSpec),
  mOverrideStrings(aOverrideStrings),
  mReentrantMonitor("nsStringBundle.mReentrantMonitor"),
  mAttemptedLoad(false),
  mLoaded(false)
{
}

nsStringBundle::~nsStringBundle()
{
}

NS_IMETHODIMP
nsStringBundle::AsyncPreload()
{
  return NS_IdleDispatchToCurrentThread(
    NewIdleRunnableMethod("nsStringBundle::LoadProperties",
                          this,
                          &nsStringBundle::LoadProperties));
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

  // whitelist check for local schemes
  nsCString scheme;
  uri->GetScheme(scheme);
  if (!scheme.EqualsLiteral("chrome") && !scheme.EqualsLiteral("jar") &&
      !scheme.EqualsLiteral("resource") && !scheme.EqualsLiteral("file") &&
      !scheme.EqualsLiteral("data")) {
    return NS_ERROR_ABORT;
  }

  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     uri,
                     nsContentUtils::GetSystemPrincipal(),
                     nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                     nsIContentPolicy::TYPE_OTHER);

  if (NS_FAILED(rv)) return rv;

  // It's a string bundle.  We expect a text/plain type, so set that as hint
  channel->SetContentType(NS_LITERAL_CSTRING("text/plain"));

  nsCOMPtr<nsIInputStream> in;
  rv = channel->Open2(getter_AddRefs(in));
  if (NS_FAILED(rv)) return rv;

  NS_ASSERTION(NS_SUCCEEDED(rv) && in, "Error in OpenBlockingStream");
  NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && in, NS_ERROR_FAILURE);

  static NS_DEFINE_CID(kPersistentPropertiesCID, NS_IPERSISTENTPROPERTIES_CID);
  mProps = do_CreateInstance(kPersistentPropertiesCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mAttemptedLoad = mLoaded = true;
  rv = mProps->Load(in);

  mLoaded = NS_SUCCEEDED(rv);

  return rv;
}

NS_IMETHODIMP
nsStringBundle::GetStringFromID(int32_t aID, nsAString& aResult)
{
  nsAutoCString idStr;
  idStr.AppendInt(aID, 10);
  return GetStringFromName(idStr.get(), aResult);
}

NS_IMETHODIMP
nsStringBundle::GetStringFromAUTF8Name(const nsACString& aName,
                                       nsAString& aResult)
{
  return GetStringFromName(PromiseFlatCString(aName).get(), aResult);
}

NS_IMETHODIMP
nsStringBundle::GetStringFromName(const char* aName, nsAString& aResult)
{
  NS_ENSURE_ARG_POINTER(aName);

  nsresult rv = LoadProperties();
  if (NS_FAILED(rv)) return rv;

  ReentrantMonitorAutoEnter automon(mReentrantMonitor);

  // try override first
  if (mOverrideStrings) {
    rv = mOverrideStrings->GetStringFromName(mPropertiesURL,
                                             nsDependentCString(aName),
                                             aResult);
    if (NS_SUCCEEDED(rv)) return rv;
  }

  return mProps->GetStringProperty(nsDependentCString(aName), aResult);
}

NS_IMETHODIMP
nsStringBundle::FormatStringFromID(int32_t aID,
                                   const char16_t **aParams,
                                   uint32_t aLength,
                                   nsAString& aResult)
{
  nsAutoCString idStr;
  idStr.AppendInt(aID, 10);
  return FormatStringFromName(idStr.get(), aParams, aLength, aResult);
}

// this function supports at most 10 parameters.. see below for why
NS_IMETHODIMP
nsStringBundle::FormatStringFromAUTF8Name(const nsACString& aName,
                                          const char16_t **aParams,
                                          uint32_t aLength,
                                          nsAString& aResult)
{
  return FormatStringFromName(PromiseFlatCString(aName).get(), aParams,
                              aLength, aResult);
}

// this function supports at most 10 parameters.. see below for why
NS_IMETHODIMP
nsStringBundle::FormatStringFromName(const char* aName,
                                     const char16_t** aParams,
                                     uint32_t aLength,
                                     nsAString& aResult)
{
  NS_ASSERTION(aParams && aLength, "FormatStringFromName() without format parameters: use GetStringFromName() instead");

  nsAutoString formatStr;
  nsresult rv = GetStringFromName(aName, formatStr);
  if (NS_FAILED(rv)) return rv;

  return FormatString(formatStr.get(), aParams, aLength, aResult);
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
      nsAutoCString key;
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
nsStringBundle::FormatString(const char16_t *aFormatStr,
                             const char16_t **aParams, uint32_t aLength,
                             nsAString& aResult)
{
  NS_ENSURE_ARG(aLength <= 10); // enforce 10-parameter limit

  // implementation note: you would think you could use vsmprintf
  // to build up an arbitrary length array.. except that there
  // is no way to build up a va_list at runtime!
  // Don't believe me? See:
  //   http://www.eskimo.com/~scs/C-faq/q15.13.html
  // -alecf
  nsTextFormatter::ssprintf(aResult, aFormatStr,
                            aLength >= 1 ? aParams[0] : nullptr,
                            aLength >= 2 ? aParams[1] : nullptr,
                            aLength >= 3 ? aParams[2] : nullptr,
                            aLength >= 4 ? aParams[3] : nullptr,
                            aLength >= 5 ? aParams[4] : nullptr,
                            aLength >= 6 ? aParams[5] : nullptr,
                            aLength >= 7 ? aParams[6] : nullptr,
                            aLength >= 8 ? aParams[7] : nullptr,
                            aLength >= 9 ? aParams[8] : nullptr,
                            aLength >= 10 ? aParams[9] : nullptr);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsExtensibleStringBundle, nsIStringBundle)

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

    nsAutoCString name;
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

NS_IMETHODIMP
nsExtensibleStringBundle::AsyncPreload()
{
  nsresult rv = NS_OK;
  const uint32_t size = mBundles.Count();
  for (uint32_t i = 0; i < size; ++i) {
    nsIStringBundle* bundle = mBundles[i];
    if (bundle) {
      nsresult rv2 = bundle->AsyncPreload();
      rv = NS_FAILED(rv) ? rv : rv2;
    }
  }
  return rv;
}

nsExtensibleStringBundle::~nsExtensibleStringBundle()
{
}

nsresult
nsExtensibleStringBundle::GetStringFromID(int32_t aID, nsAString& aResult)
{
  nsAutoCString idStr;
  idStr.AppendInt(aID, 10);
  return GetStringFromName(idStr.get(), aResult);
}

nsresult
nsExtensibleStringBundle::GetStringFromAUTF8Name(const nsACString& aName,
                                                 nsAString& aResult)
{
  return GetStringFromName(PromiseFlatCString(aName).get(), aResult);
}

nsresult
nsExtensibleStringBundle::GetStringFromName(const char* aName,
                                            nsAString& aResult)
{
  nsresult rv;
  const uint32_t size = mBundles.Count();
  for (uint32_t i = 0; i < size; ++i) {
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
nsExtensibleStringBundle::FormatStringFromID(int32_t aID,
                                             const char16_t ** aParams,
                                             uint32_t aLength,
                                             nsAString& aResult)
{
  nsAutoCString idStr;
  idStr.AppendInt(aID, 10);
  return FormatStringFromName(idStr.get(), aParams, aLength, aResult);
}

NS_IMETHODIMP
nsExtensibleStringBundle::FormatStringFromAUTF8Name(const nsACString& aName,
                                                    const char16_t ** aParams,
                                                    uint32_t aLength,
                                                    nsAString& aResult)
{
  return FormatStringFromName(PromiseFlatCString(aName).get(),
                              aParams, aLength, aResult);
}

NS_IMETHODIMP
nsExtensibleStringBundle::FormatStringFromName(const char* aName,
                                               const char16_t** aParams,
                                               uint32_t aLength,
                                               nsAString& aResult)
{
  nsAutoString formatStr;
  nsresult rv;
  rv = GetStringFromName(aName, formatStr);
  if (NS_FAILED(rv))
    return rv;

  return nsStringBundle::FormatString(formatStr.get(), aParams, aLength,
                                      aResult);
}

nsresult nsExtensibleStringBundle::GetSimpleEnumeration(nsISimpleEnumerator ** aResult)
{
  // XXX write me
  *aResult = nullptr;
  return NS_ERROR_NOT_IMPLEMENTED;
}

/////////////////////////////////////////////////////////////////////////////////////////

#define MAX_CACHED_BUNDLES 16

struct bundleCacheEntry_t final : public LinkedListElement<bundleCacheEntry_t> {
  nsCString mHashKey;
  nsCOMPtr<nsIStringBundle> mBundle;

  bundleCacheEntry_t()
  {
    MOZ_COUNT_CTOR(bundleCacheEntry_t);
  }

  ~bundleCacheEntry_t()
  {
    MOZ_COUNT_DTOR(bundleCacheEntry_t);
  }
};


nsStringBundleService::nsStringBundleService() :
  mBundleMap(MAX_CACHED_BUNDLES)
{
  mErrorService = do_GetService(kErrorServiceCID);
  NS_ASSERTION(mErrorService, "Couldn't get error service");
}

NS_IMPL_ISUPPORTS(nsStringBundleService,
                  nsIStringBundleService,
                  nsIObserver,
                  nsISupportsWeakReference)

nsStringBundleService::~nsStringBundleService()
{
  flushBundleCache();
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
    os->AddObserver(this, "intl:app-locales-changed", true);
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
                               const char16_t* aSomeData)
{
  if (strcmp("memory-pressure", aTopic) == 0 ||
      strcmp("profile-do-change", aTopic) == 0 ||
      strcmp("chrome-flush-caches", aTopic) == 0 ||
      strcmp("intl:app-locales-changed", aTopic) == 0)
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
  mBundleMap.Clear();

  while (!mBundleCache.isEmpty()) {
    delete mBundleCache.popFirst();
  }
}

NS_IMETHODIMP
nsStringBundleService::FlushBundles()
{
  flushBundleCache();
  return NS_OK;
}

void
nsStringBundleService::getStringBundle(const char *aURLSpec,
                                       nsIStringBundle **aResult)
{
  nsDependentCString key(aURLSpec);
  bundleCacheEntry_t* cacheEntry = mBundleMap.Get(key);

  if (cacheEntry) {
    // cache hit!
    // remove it from the list, it will later be reinserted
    // at the head of the list
    cacheEntry->remove();

  } else {

    // hasn't been cached, so insert it into the hash table
    RefPtr<nsStringBundle> bundle = new nsStringBundle(aURLSpec, mOverrideStrings);
    cacheEntry = insertIntoCache(bundle.forget(), key);
  }

  // at this point the cacheEntry should exist in the hashtable,
  // but is not in the LRU cache.
  // put the cache entry at the front of the list
  mBundleCache.insertFront(cacheEntry);

  // finally, return the value
  *aResult = cacheEntry->mBundle;
  NS_ADDREF(*aResult);
}

bundleCacheEntry_t *
nsStringBundleService::insertIntoCache(already_AddRefed<nsIStringBundle> aBundle,
                                       nsCString &aHashKey)
{
  bundleCacheEntry_t *cacheEntry;

  if (mBundleMap.Count() < MAX_CACHED_BUNDLES) {
    // cache not full - create a new entry
    cacheEntry = new bundleCacheEntry_t();
  } else {
    // cache is full
    // take the last entry in the list, and recycle it.
    cacheEntry = mBundleCache.getLast();

    // remove it from the hash table and linked list
    NS_ASSERTION(mBundleMap.Contains(cacheEntry->mHashKey),
                 "Element will not be removed!");
    mBundleMap.Remove(cacheEntry->mHashKey);
    cacheEntry->remove();
  }

  // at this point we have a new cacheEntry that doesn't exist
  // in the hashtable, so set up the cacheEntry
  cacheEntry->mHashKey = aHashKey;
  cacheEntry->mBundle = aBundle;

  // insert the entry into the cache and map, make it the MRU
  mBundleMap.Put(cacheEntry->mHashKey, cacheEntry);

  return cacheEntry;
}

NS_IMETHODIMP
nsStringBundleService::CreateBundle(const char* aURLSpec,
                                    nsIStringBundle** aResult)
{
  getStringBundle(aURLSpec,aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsStringBundleService::CreateExtensibleBundle(const char* aCategory,
                                              nsIStringBundle** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = nullptr;

  RefPtr<nsExtensibleStringBundle> bundle = new nsExtensibleStringBundle();

  nsresult res = bundle->Init(aCategory, this);
  if (NS_FAILED(res)) {
    return res;
  }

  bundle.forget(aResult);
  return NS_OK;
}

#define GLOBAL_PROPERTIES "chrome://global/locale/global-strres.properties"

nsresult
nsStringBundleService::FormatWithBundle(nsIStringBundle* bundle, nsresult aStatus,
                                        uint32_t argCount, char16_t** argArray,
                                        nsAString& result)
{
  nsresult rv;

  // try looking up the error message with the int key:
  uint16_t code = NS_ERROR_GET_CODE(aStatus);
  rv = bundle->FormatStringFromID(code, (const char16_t**)argArray, argCount, result);

  // If the int key fails, try looking up the default error message. E.g. print:
  //   An unknown error has occurred (0x804B0003).
  if (NS_FAILED(rv)) {
    nsAutoString statusStr;
    statusStr.AppendInt(static_cast<uint32_t>(aStatus), 16);
    const char16_t* otherArgArray[1];
    otherArgArray[0] = statusStr.get();
    uint16_t code = NS_ERROR_GET_CODE(NS_ERROR_FAILURE);
    rv = bundle->FormatStringFromID(code, otherArgArray, 1, result);
  }

  return rv;
}

NS_IMETHODIMP
nsStringBundleService::FormatStatusMessage(nsresult aStatus,
                                           const char16_t* aStatusArg,
                                           nsAString& result)
{
  nsresult rv;
  uint32_t i, argCount = 0;
  nsCOMPtr<nsIStringBundle> bundle;
  nsXPIDLCString stringBundleURL;

  // XXX hack for mailnews who has already formatted their messages:
  if (aStatus == NS_OK && aStatusArg) {
    result.Assign(aStatusArg);
    return NS_OK;
  }

  if (aStatus == NS_OK) {
    return NS_ERROR_FAILURE;       // no message to format
  }

  // format the arguments:
  const nsDependentString args(aStatusArg);
  argCount = args.CountChar(char16_t('\n')) + 1;
  NS_ENSURE_ARG(argCount <= 10); // enforce 10-parameter limit
  char16_t* argArray[10];

  // convert the aStatusArg into a char16_t array
  if (argCount == 1) {
    // avoid construction for the simple case:
    argArray[0] = (char16_t*)aStatusArg;
  }
  else if (argCount > 1) {
    int32_t offset = 0;
    for (i = 0; i < argCount; i++) {
      int32_t pos = args.FindChar('\n', offset);
      if (pos == -1)
        pos = args.Length();
      argArray[i] = ToNewUnicode(Substring(args, offset, pos - offset));
      if (argArray[i] == nullptr) {
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
    getStringBundle(stringBundleURL, getter_AddRefs(bundle));
    rv = FormatWithBundle(bundle, aStatus, argCount, argArray, result);
  }
  if (NS_FAILED(rv)) {
    getStringBundle(GLOBAL_PROPERTIES, getter_AddRefs(bundle));
    rv = FormatWithBundle(bundle, aStatus, argCount, argArray, result);
  }

done:
  if (argCount > 1) {
    for (i = 0; i < argCount; i++) {
      if (argArray[i])
        free(argArray[i]);
    }
  }
  return rv;
}
