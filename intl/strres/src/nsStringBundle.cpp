/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define NS_IMPL_IDS
#include "nsID.h"
#include "nsFileSpec.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIPersistentProperties2.h"
#include "nsIStringBundle.h"
#include "nscore.h"
#include "nsMemory.h"
#include "plstr.h"
#include "nsNetUtil.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
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
#include "nsIModule.h"
#include "nsIRegistry.h"
#include "nsISupportsArray.h"
#include "nsHashtable.h"
#include "nsAutoLock.h"
#include "nsTextFormatter.h"
#include "nsIErrorService.h"

// for async loading
#ifdef ASYNC_LOADING
#include "nsIBinaryInputStream.h"
#include "nsIStringStream.h"
#endif

// eventQ
#include "nsIEventQueueService.h"

#include "prenv.h"

static NS_DEFINE_CID(kErrorServiceCID, NS_ERRORSERVICE_CID);

// XXX investigate need for proper locking in this module
//static PRInt32 gLockCount = 0;

NS_DEFINE_IID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

class nsStringBundle : public nsIStringBundle
#ifdef ASYNC_LOADING
                     , public nsIStreamLoaderObserver
#endif
{
public:
  // init version
  nsStringBundle(const char* aURLSpec);
  nsresult LoadProperties();
  virtual ~nsStringBundle();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTRINGBUNDLE
#ifdef ASYNC_LOADING
  NS_DECL_NSISTREAMLOADEROBSERVER
#endif

  nsCOMPtr<nsIPersistentProperties> mProps;

protected:
  //
  // functional decomposition of the funitions repeatively called 
  //
  nsresult GetStringFromID(PRInt32 aID, nsAString& aResult);
  nsresult GetStringFromName(const nsAReadableString& aName, nsAString& aResult);

private:
  nsCString              mPropertiesURL;
  PRPackedBool                 mAttemptedLoad;
  PRPackedBool                 mLoaded;

public:
  static nsresult FormatString(const PRUnichar *formatStr,
                               const PRUnichar **aParams, PRUint32 aLength,
                               PRUnichar **aResult);
 };

nsStringBundle::~nsStringBundle()
{
}

nsStringBundle::nsStringBundle(const char* aURLSpec) :
  mPropertiesURL(aURLSpec),
  mAttemptedLoad(PR_FALSE),
  mLoaded(PR_FALSE)
{
  NS_INIT_REFCNT();
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
    else
      return NS_ERROR_UNEXPECTED;
  }
  
  mAttemptedLoad = PR_TRUE;

  nsresult rv;

  // to be turned on once we figure out how to ensure we're loading on
  // the main thread
#ifdef ASYNC_LOADING

  // load the string bundle asynchronously on another thread,
  // but make sure we block this thread so that LoadProperties() is
  // synchronous
  
  // create an event queue for this thread.
  nsCOMPtr<nsIEventQueueService> service = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  nsCOMPtr<nsIEventQueue> currentThreadQ;
  rv = service->PushThreadEventQueue(getter_AddRefs(currentThreadQ));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), mPropertiesURL.get());
  if (NS_FAILED(rv)) return rv;

  //   create and loader, then wait
  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), 
                          uri, 
                          this /*the load observer*/);
 NS_ASSERTION(NS_SUCCEEDED(rv), "\n-->nsStringBundle::Init: NS_NewStreamLoader failed...\n");

 // process events until we're finished.
 PLEvent *event;
 while (!mLoaded) {
   rv = currentThreadQ->WaitForEvent(&event);
   NS_ASSERTION(NS_SUCCEEDED(rv), "\n-->nsStringBundle::Init: currentThreadQ->WaitForEvent failed...\n");
   if (NS_FAILED(rv)) break;

   rv = currentThreadQ->HandleEvent(event);
   NS_ASSERTION(NS_SUCCEEDED(rv), "\n-->nsStringBundle::Init: currentThreadQ->HandleEvent failed...\n");
   if (NS_FAILED(rv)) break;
 }
  
 rv = service->PopThreadEventQueue(currentThreadQ);

#else

 // do it synchronously
 nsCOMPtr<nsIURI> uri;
 rv = NS_NewURI(getter_AddRefs(uri), mPropertiesURL);
 if (NS_FAILED(rv)) return rv;
    
 nsCOMPtr<nsIInputStream> in;
 rv = NS_OpenURI(getter_AddRefs(in), uri);
 if (NS_FAILED(rv)) return rv;
    
 NS_ASSERTION(NS_SUCCEEDED(rv) && in, "Error in OpenBlockingStream");
 NS_ENSURE_TRUE(NS_SUCCEEDED(rv) && in, NS_ERROR_FAILURE);
    
 mProps = do_CreateInstance(kPersistentPropertiesCID, &rv);
 NS_ENSURE_SUCCESS(rv, rv);
    
 mAttemptedLoad = mLoaded = PR_TRUE;
 rv = mProps->Load(in);

 mLoaded = NS_SUCCEEDED(rv);
#endif
  
  return rv;
}

#ifdef ASYNC_LOADING
NS_IMETHODIMP
nsStringBundle::OnStreamComplete(nsIStreamLoader* aLoader,
                                 nsISupports* context,
                                 nsresult aStatus,
                                 PRUint32 stringLen,
                                 const char* string)
{

  nsXPIDLCString uriSpec;
  if (NS_FAILED(aStatus)) {
  // print a load error on bad status
#if defined(DEBUG)
    if (aLoader) {
      nsCOMPtr<nsIRequest> request;
      aLoader->GetRequest(getter_AddRefs(request));
      nsCOMPtr<nsIChannel> channel(do_QueryInterface(request));

      if (channel) {
        nsCOMPtr<nsIURI> uri;
        channel->GetURI(getter_AddRefs(uri));
        if (uri) {
            uri->GetSpec(getter_Copies(uriSpec));
            printf("\n -->nsStringBundle::OnStreamComplete: Failed to load %s\n", 
                   uriSpec ? (const char *) uriSpec : "");
        }
      }
    }
#endif
    return aStatus;
  }

  nsresult rv;
  nsCOMPtr<nsISupports> stringStreamSupports;
  rv = NS_NewByteInputStream(getter_AddRefs(stringStreamSupports), 
                             string, stringLen);
  if (NS_FAILED(rv)) return rv;
      
  nsCOMPtr<nsIInputStream> in = do_QueryInterface(stringStreamSupports, &rv);
  if (NS_FAILED(rv)) return rv;

  mProps = do_CreateInstance(kPersistentPropertiesCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  // load the stream
  rv = mProps->Load(in);
  
  // 
  // notify
  if (NS_SUCCEEDED(rv)) {
    mLoaded = PR_TRUE;

    // observer notification
    nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
    if (os)
      (void) os->NotifyObservers((nsIStringBundle *) this, 
                        NS_STRBUNDLE_LOADED_TOPIC, 
                        nsnull);

#if defined(DEBUG_tao)
  printf("\n --> nsStringBundle::OnStreamComplete: end sending NOTIFICATIONS!!\n");
#endif
  }
  return rv;
}

#endif

nsresult
nsStringBundle::GetStringFromID(PRInt32 aID, nsAString& aResult)
{  
  nsAutoCMonitor(this);
  nsAutoString name;
  name.AppendInt(aID, 10);
  nsresult rv = mProps->GetStringProperty(name, aResult);

#ifdef DEBUG_tao_
  char *s = ToNewCString(aResult);
  printf("\n** GetStringFromID: aResult=%s, len=%d\n", s?s:"null", 
         aResult.Length());
  delete s;
#endif /* DEBUG_tao_ */

  return rv;
}

nsresult
nsStringBundle::GetStringFromName(const nsAReadableString& aName,
                                  nsAString& aResult)
{
  nsresult rv;

  rv = mProps->GetStringProperty(aName, aResult);
#ifdef DEBUG_tao_
  char *s = ToNewCString(aResult),
       *ss = ToNewCString(aName);
  printf("\n** GetStringFromName: aName=%s, aResult=%s, len=%d\n", 
         ss?ss:"null", s?s:"null", aResult.Length());
  delete s;
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
  nsresult rv;
  rv = LoadProperties();
  if (NS_FAILED(rv)) return rv;
  
  nsAutoString formatStr;
  rv = GetStringFromName(nsDependentString(aName), formatStr);
  if (NS_FAILED(rv)) return rv;

  return FormatString(formatStr.get(), aParams, aLength, aResult);
}
                                     

#ifdef ASYNC_LOADING
NS_IMPL_THREADSAFE_ISUPPORTS2(nsStringBundle, 
                              nsIStringBundle, nsIStreamLoaderObserver)
#else
NS_IMPL_THREADSAFE_ISUPPORTS1(nsStringBundle,
                              nsIStringBundle)
#endif

/* void GetStringFromID (in long aID, out wstring aResult); */
NS_IMETHODIMP
nsStringBundle::GetStringFromID(PRInt32 aID, PRUnichar **aResult)
{
  nsresult rv;
  rv = LoadProperties();
  if (NS_FAILED(rv)) return rv;
  
  *aResult = nsnull;
  nsAutoString tmpstr;

  nsresult ret = GetStringFromID(aID, tmpstr);
  if (NS_SUCCEEDED(ret))
    *aResult = ToNewUnicode(tmpstr);
  return ret;
}

/* void GetStringFromName (in wstring aName, out wstring aResult); */
NS_IMETHODIMP 
nsStringBundle::GetStringFromName(const PRUnichar *aName, PRUnichar **aResult)
{
  nsresult rv;
  rv = LoadProperties();
  if (NS_FAILED(rv)) return rv;

  nsAutoCMonitor(this);
  *aResult = nsnull;
  nsAutoString tmpstr;
  rv = GetStringFromName(nsDependentString(aName), tmpstr);
  if (NS_SUCCEEDED(rv))
    *aResult = ToNewUnicode(tmpstr);
  
  return rv;
}

NS_IMETHODIMP
nsStringBundle::GetSimpleEnumeration(nsISimpleEnumerator** elements)
{
  if (!elements)
    return NS_ERROR_INVALID_POINTER;

  return mProps->SimpleEnumerateProperties(elements);
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
  NS_DECL_NSISTRINGBUNDLE

  nsresult Init(const char * aRegistryKey);
private:
  
  nsISupportsArray * mBundle;
  PRBool             mLoaded;

public:

  nsExtensibleStringBundle();
  virtual ~nsExtensibleStringBundle();
};

NS_IMPL_ISUPPORTS1(nsExtensibleStringBundle, nsIStringBundle)

nsExtensibleStringBundle::nsExtensibleStringBundle()
                                                  :mBundle(NULL)
{
  NS_INIT_REFCNT();

  mLoaded = PR_FALSE;


}

nsresult
nsExtensibleStringBundle::Init(const char * aRegistryKey) 
{
  nsresult res = NS_OK;
  nsCOMPtr<nsIEnumerator> components;
  nsRegistryKey uconvKey, key;
  PRBool regOpen = PR_FALSE;

  // get the Bundle Service
  nsCOMPtr<nsIStringBundleService> sbServ(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &res));
  if (NS_FAILED(res)) return res;

  // get the registry
  nsCOMPtr<nsIRegistry> registry(do_GetService(NS_REGISTRY_CONTRACTID, &res));
  if (NS_FAILED(res)) return res;

  // open registry if necessary
  registry->IsOpen(&regOpen);
  if (!regOpen) {
    res = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    if (NS_FAILED(res)) return res;
  }

  // get subtree
  res = registry->GetSubtree(nsIRegistry::Common,  
    aRegistryKey, &uconvKey);
  if (NS_FAILED(res)) return res;

  // enumerate subtrees
  res = registry->EnumerateSubtrees(uconvKey, getter_AddRefs(components));
  if (NS_FAILED(res)) return res;
  res = components->First();
  if (NS_FAILED(res)) return res;

  // create the bundles array
  res = NS_NewISupportsArray(&mBundle);
  if (NS_FAILED(res)) return res;

  while (NS_OK != components->IsDone()) {
    nsCOMPtr<nsISupports> base;
    nsCOMPtr<nsIRegistryNode> node;
    nsXPIDLCString name;
    nsCOMPtr<nsIStringBundle> bundle;

    res = components->CurrentItem(getter_AddRefs(base));
    if (NS_FAILED(res)) return res;

    node = do_QueryInterface(base, &res);
    if (NS_FAILED(res)) {
      res = components->Next();
      if (NS_FAILED(res)) return res;
      continue;
    }

    res = node->GetKey(&key);
    if (NS_FAILED(res)) return res;

    res = registry->GetStringUTF8(key, "name", getter_Copies(name));
    if (NS_FAILED(res)) return res;

    res = sbServ->CreateBundle(name, getter_AddRefs(bundle));
    if (NS_FAILED(res)) {
      res = components->Next();
      if (NS_FAILED(res)) return res;
      continue;
    }

    res = mBundle->AppendElement(bundle);
    if (NS_FAILED(res)) return res;

    res = components->Next();
    if (NS_FAILED(res)) return res;
  }

  return res;
}

nsExtensibleStringBundle::~nsExtensibleStringBundle() 
{
  NS_IF_RELEASE(mBundle);
}

nsresult nsExtensibleStringBundle::GetStringFromID(PRInt32 aID, PRUnichar ** aResult)
{
  nsresult rv;
  
  PRUint32 size, i;

  rv = mBundle->Count(&size);
  if (NS_FAILED(rv)) return rv;

  for (i = 0; i < size; i++) {
    nsCOMPtr<nsIStringBundle> bundle;
    rv = mBundle->QueryElementAt(i, NS_GET_IID(nsIStringBundle), getter_AddRefs(bundle));
    if (NS_SUCCEEDED(rv)) {
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
  return FormatStringFromName(idStr.get(), aParams, aLength, aResult);
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
  nsresult getStringBundle(const char *aUrl, nsIStringBundle** aResult);
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
  nsCOMPtr<nsIObserverService> os = do_GetService("@mozilla.org/observer-service;1");
  if (os)
    os->AddObserver(this, "memory-pressure", PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsStringBundleService::Observe(nsISupports* aSubject,
                               const char* aTopic,
                               const PRUnichar* aSomeData)
{
  if (nsCRT::strcmp("memory-pressure", aTopic) == 0)
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
    nsStringBundle* bundle = new nsStringBundle(aURLSpec);
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
  {
    printf("\n** nsStringBundleService::CreateBundle: %s\n",
           aURLSpec ? aURLSpec : "null");
    delete s;
  }
#endif

  return getStringBundle(aURLSpec,aResult);
}
  
NS_IMETHODIMP
nsStringBundleService::CreateAsyncBundle(const char* aURLSpec, nsIStringBundle** aResult)
{
  return getStringBundle(aURLSpec, aResult);
}

NS_IMETHODIMP
nsStringBundleService::CreateExtensibleBundle(const char* aRegistryKey, 
                                              nsIStringBundle** aResult)
{
  if (aResult == NULL) return NS_ERROR_NULL_POINTER;

  nsresult res = NS_OK;

  nsExtensibleStringBundle * bundle = new nsExtensibleStringBundle();
  if (!bundle) return NS_ERROR_OUT_OF_MEMORY;

  res = bundle->Init(aRegistryKey);
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
    rv = bundle->FormatStringFromName(NS_ConvertASCIItoUCS2(key).get(),
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
    return NS_OK;
  }

  if (aStatus == NS_OK) {
    return NS_ERROR_FAILURE;       // no message to format
  }

  // format the arguments:
  const nsAutoString args(aStatusArg);
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
      PRInt32 pos = args.FindChar('\n', PR_FALSE, offset);
      if (pos == -1) 
        pos = args.Length();
      nsAutoString arg;
      args.Mid(arg, offset, pos);
      argArray[i] = ToNewUnicode(arg);
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


NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStringBundleService, Init)
static nsModuleComponentInfo components[] =
{
  { "String Bundle", NS_STRINGBUNDLESERVICE_CID, NS_STRINGBUNDLE_CONTRACTID, nsStringBundleServiceConstructor},
};

NS_IMPL_NSGETMODULE(nsStringBundleModule, components)

