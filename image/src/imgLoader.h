/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef imgLoader_h__
#define imgLoader_h__

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"

#include "imgILoader.h"
#include "imgICache.h"
#include "nsWeakReference.h"
#include "nsIContentSniffer.h"
#include "nsRefPtrHashtable.h"
#include "nsExpirationTracker.h"
#include "nsAutoPtr.h"
#include "imgRequest.h"
#include "nsIProgressEventSink.h"
#include "nsIChannel.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "imgIRequest.h"

class imgLoader;
class imgRequestProxy;
class imgINotificationObserver;
class nsILoadGroup;
class imgCacheExpirationTracker;
class imgMemoryReporter;
class nsIChannelPolicy;

namespace mozilla {
namespace image {
class ImageURL;
}
}

class imgCacheEntry
{
public:
  imgCacheEntry(imgLoader* loader, imgRequest *request, bool aForcePrincipalCheck);
  ~imgCacheEntry();

  nsrefcnt AddRef()
  {
    NS_PRECONDITION(int32_t(mRefCnt) >= 0, "illegal refcnt");
    NS_ABORT_IF_FALSE(_mOwningThread.GetThread() == PR_GetCurrentThread(), "imgCacheEntry addref isn't thread-safe!");
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "imgCacheEntry", sizeof(*this));
    return mRefCnt;
  }

  nsrefcnt Release()
  {
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    NS_ABORT_IF_FALSE(_mOwningThread.GetThread() == PR_GetCurrentThread(), "imgCacheEntry release isn't thread-safe!");
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "imgCacheEntry");
    if (mRefCnt == 0) {
      mRefCnt = 1; /* stabilize */
      delete this;
      return 0;
    }
    return mRefCnt;
  }

  uint32_t GetDataSize() const
  {
    return mDataSize;
  }
  void SetDataSize(uint32_t aDataSize)
  {
    int32_t oldsize = mDataSize;
    mDataSize = aDataSize;
    UpdateCache(mDataSize - oldsize);
  }

  int32_t GetTouchedTime() const
  {
    return mTouchedTime;
  }
  void SetTouchedTime(int32_t time)
  {
    mTouchedTime = time;
    Touch(/* updateTime = */ false);
  }

  int32_t GetExpiryTime() const
  {
    return mExpiryTime;
  }
  void SetExpiryTime(int32_t aExpiryTime)
  {
    mExpiryTime = aExpiryTime;
    Touch();
  }

  bool GetMustValidate() const
  {
    return mMustValidate;
  }
  void SetMustValidate(bool aValidate)
  {
    mMustValidate = aValidate;
    Touch();
  }

  already_AddRefed<imgRequest> GetRequest() const
  {
    nsRefPtr<imgRequest> req = mRequest;
    return req.forget();
  }

  bool Evicted() const
  {
    return mEvicted;
  }

  nsExpirationState *GetExpirationState()
  {
    return &mExpirationState;
  }

  bool HasNoProxies() const
  {
    return mHasNoProxies;
  }

  bool ForcePrincipalCheck() const
  {
    return mForcePrincipalCheck;
  }

  imgLoader* Loader() const
  {
    return mLoader;
  }

private: // methods
  friend class imgLoader;
  friend class imgCacheQueue;
  void Touch(bool updateTime = true);
  void UpdateCache(int32_t diff = 0);
  void SetEvicted(bool evict)
  {
    mEvicted = evict;
  }
  void SetHasNoProxies(bool hasNoProxies);

  // Private, unimplemented copy constructor.
  imgCacheEntry(const imgCacheEntry &);

private: // data
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  imgLoader* mLoader;
  nsRefPtr<imgRequest> mRequest;
  uint32_t mDataSize;
  int32_t mTouchedTime;
  int32_t mExpiryTime;
  nsExpirationState mExpirationState;
  bool mMustValidate : 1;
  bool mEvicted : 1;
  bool mHasNoProxies : 1;
  bool mForcePrincipalCheck : 1;
};

#include <vector>

#define NS_IMGLOADER_CID \
{ /* c1354898-e3fe-4602-88a7-c4520c21cb4e */         \
     0xc1354898,                                     \
     0xe3fe,                                         \
     0x4602,                                         \
    {0x88, 0xa7, 0xc4, 0x52, 0x0c, 0x21, 0xcb, 0x4e} \
}

class imgCacheQueue
{
public:
  imgCacheQueue();
  void Remove(imgCacheEntry *);
  void Push(imgCacheEntry *);
  void MarkDirty();
  bool IsDirty();
  already_AddRefed<imgCacheEntry> Pop();
  void Refresh();
  uint32_t GetSize() const;
  void UpdateSize(int32_t diff);
  uint32_t GetNumElements() const;
  typedef std::vector<nsRefPtr<imgCacheEntry> > queueContainer;
  typedef queueContainer::iterator iterator;
  typedef queueContainer::const_iterator const_iterator;

  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;

private:
  queueContainer mQueue;
  bool mDirty;
  uint32_t mSize;
};

class imgLoader MOZ_FINAL : public imgILoader,
                            public nsIContentSniffer,
                            public imgICache,
                            public nsSupportsWeakReference,
                            public nsIObserver
{
  virtual ~imgLoader();

public:
  typedef mozilla::image::ImageURL ImageURL;
  typedef nsRefPtrHashtable<nsCStringHashKey, imgCacheEntry> imgCacheTable;
  typedef nsTHashtable<nsPtrHashKey<imgRequest>> imgSet;
  typedef mozilla::Mutex Mutex;

  NS_DECL_ISUPPORTS
  NS_DECL_IMGILOADER
  NS_DECL_NSICONTENTSNIFFER
  NS_DECL_IMGICACHE
  NS_DECL_NSIOBSERVER

  static imgLoader* Singleton();
  static imgLoader* PBSingleton();

  imgLoader();

  nsresult Init();

  static imgLoader* Create()
  {
      // Unfortunately, we rely on XPCOM module init happening
      // before imgLoader creation. For now, it's easier
      // to just call CallCreateInstance() which will init
      // the image module instead of calling new imgLoader
      // directly.
      imgILoader *loader;
      CallCreateInstance("@mozilla.org/image/loader;1", &loader);
      // There's only one imgLoader implementation so we
      // can safely cast to it.
      return static_cast<imgLoader*>(loader);
  }

  static already_AddRefed<imgLoader> GetInstance();

  nsresult LoadImage(nsIURI *aURI,
                     nsIURI *aInitialDocumentURI,
                     nsIURI *aReferrerURI,
                     nsIPrincipal* aLoadingPrincipal,
                     nsILoadGroup *aLoadGroup,
                     imgINotificationObserver *aObserver,
                     nsISupports *aCX,
                     nsLoadFlags aLoadFlags,
                     nsISupports *aCacheKey,
                     nsIChannelPolicy *aPolicy,
                     const nsAString& initiatorType,
                     imgRequestProxy **_retval);
  nsresult LoadImageWithChannel(nsIChannel *channel,
                                imgINotificationObserver *aObserver,
                                nsISupports *aCX,
                                nsIStreamListener **listener,
                                imgRequestProxy **_retval);

  static nsresult GetMimeTypeFromContent(const char* aContents, uint32_t aLength, nsACString& aContentType);
  // exported for use by mimei.cpp in libxul sdk builds
  static NS_EXPORT_(bool) SupportImageWithMimeType(const char* aMimeType);

  static void GlobalInit(); // for use by the factory
  static void Shutdown(); // for use by the factory

  nsresult ClearChromeImageCache();
  nsresult ClearImageCache();
  void MinimizeCaches();

  nsresult InitCache();

  bool RemoveFromCache(nsIURI *aKey);
  bool RemoveFromCache(ImageURL *aKey);
  bool RemoveFromCache(nsCString &spec,
                       imgCacheTable &cache,
                       imgCacheQueue &queue);
  bool RemoveFromCache(imgCacheEntry *entry);

  bool PutIntoCache(nsIURI *key, imgCacheEntry *entry);

  void AddToUncachedImages(imgRequest* aRequest);
  void RemoveFromUncachedImages(imgRequest* aRequest);

  // Returns true if we should prefer evicting cache entry |two| over cache
  // entry |one|.
  // This mixes units in the worst way, but provides reasonable results.
  inline static bool CompareCacheEntries(const nsRefPtr<imgCacheEntry> &one,
                                         const nsRefPtr<imgCacheEntry> &two)
  {
    if (!one)
      return false;
    if (!two)
      return true;

    const double sizeweight = 1.0 - sCacheTimeWeight;

    // We want large, old images to be evicted first (depending on their
    // relative weights). Since a larger time is actually newer, we subtract
    // time's weight, so an older image has a larger weight.
    double oneweight = double(one->GetDataSize()) * sizeweight -
                       double(one->GetTouchedTime()) * sCacheTimeWeight;
    double twoweight = double(two->GetDataSize()) * sizeweight -
                       double(two->GetTouchedTime()) * sCacheTimeWeight;

    return oneweight < twoweight;
  }

  void VerifyCacheSizes();

  // The image loader maintains a hash table of all imgCacheEntries. However,
  // only some of them will be evicted from the cache: those who have no
  // imgRequestProxies watching their imgRequests.
  //
  // Once an imgRequest has no imgRequestProxies, it should notify us by
  // calling HasNoObservers(), and null out its cache entry pointer.
  //
  // Upon having a proxy start observing again, it should notify us by calling
  // HasObservers(). The request's cache entry will be re-set before this
  // happens, by calling imgRequest::SetCacheEntry() when an entry with no
  // observers is re-requested.
  bool SetHasNoProxies(imgRequest *aRequest, imgCacheEntry *aEntry);
  bool SetHasProxies(imgRequest *aRequest);

private: // methods

  bool ValidateEntry(imgCacheEntry *aEntry, nsIURI *aKey,
                       nsIURI *aInitialDocumentURI, nsIURI *aReferrerURI,
                       nsILoadGroup *aLoadGroup,
                       imgINotificationObserver *aObserver, nsISupports *aCX,
                       nsLoadFlags aLoadFlags, bool aCanMakeNewChannel,
                       imgRequestProxy **aProxyRequest,
                       nsIChannelPolicy *aPolicy,
                       nsIPrincipal* aLoadingPrincipal,
                       int32_t aCORSMode);

  bool ValidateRequestWithNewChannel(imgRequest *request, nsIURI *aURI,
                                       nsIURI *aInitialDocumentURI,
                                       nsIURI *aReferrerURI,
                                       nsILoadGroup *aLoadGroup,
                                       imgINotificationObserver *aObserver,
                                       nsISupports *aCX, nsLoadFlags aLoadFlags,
                                       imgRequestProxy **aProxyRequest,
                                       nsIChannelPolicy *aPolicy,
                                       nsIPrincipal* aLoadingPrincipal,
                                       int32_t aCORSMode);

  nsresult CreateNewProxyForRequest(imgRequest *aRequest, nsILoadGroup *aLoadGroup,
                                    imgINotificationObserver *aObserver,
                                    nsLoadFlags aLoadFlags, imgRequestProxy **_retval);

  void ReadAcceptHeaderPref();

  nsresult EvictEntries(imgCacheTable &aCacheToClear);
  nsresult EvictEntries(imgCacheQueue &aQueueToClear);

  imgCacheTable &GetCache(nsIURI *aURI);
  imgCacheQueue &GetCacheQueue(nsIURI *aURI);
  imgCacheTable &GetCache(ImageURL *aURI);
  imgCacheQueue &GetCacheQueue(ImageURL *aURI);
  void CacheEntriesChanged(ImageURL *aURI, int32_t sizediff = 0);
  void CheckCacheLimits(imgCacheTable &cache, imgCacheQueue &queue);

private: // data
  friend class imgCacheEntry;
  friend class imgMemoryReporter;

  imgCacheTable mCache;
  imgCacheQueue mCacheQueue;

  imgCacheTable mChromeCache;
  imgCacheQueue mChromeCacheQueue;

  // Hash set of every imgRequest for this loader that isn't in mCache or
  // mChromeCache. The union over all imgLoader's of mCache, mChromeCache, and
  // mUncachedImages should be every imgRequest that is alive. These are weak
  // pointers so we rely on the imgRequest destructor to remove itself.
  imgSet mUncachedImages;
  // The imgRequest can have refs to them held on non-main thread, so we need
  // a mutex because we modify the uncached images set from the imgRequest
  // destructor.
  Mutex mUncachedImagesMutex;

  static double sCacheTimeWeight;
  static uint32_t sCacheMaxSize;
  static imgMemoryReporter* sMemReporter;

  nsCString mAcceptHeader;

  nsAutoPtr<imgCacheExpirationTracker> mCacheTracker;
  bool mRespectPrivacy;
};



/**
 * proxy stream listener class used to handle multipart/x-mixed-replace
 */

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"

class ProxyListener : public nsIStreamListener
                    , public nsIThreadRetargetableStreamListener
{
public:
  explicit ProxyListener(nsIStreamListener *dest);

  /* additional members */
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

private:
  virtual ~ProxyListener();

  nsCOMPtr<nsIStreamListener> mDestListener;
};

/**
 * A class that implements nsIProgressEventSink and forwards all calls to it to
 * the original notification callbacks of the channel. Also implements
 * nsIInterfaceRequestor and gives out itself for nsIProgressEventSink calls,
 * and forwards everything else to the channel's notification callbacks.
 */
class nsProgressNotificationProxy MOZ_FINAL
  : public nsIProgressEventSink
  , public nsIChannelEventSink
  , public nsIInterfaceRequestor
{
  public:
    nsProgressNotificationProxy(nsIChannel* channel,
                                imgIRequest* proxy)
        : mImageRequest(proxy) {
      channel->GetNotificationCallbacks(getter_AddRefs(mOriginalCallbacks));
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROGRESSEVENTSINK
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR
  private:
    ~nsProgressNotificationProxy() {}

    nsCOMPtr<nsIInterfaceRequestor> mOriginalCallbacks;
    nsCOMPtr<nsIRequest> mImageRequest;
};

/**
 * validate checker
 */

#include "nsCOMArray.h"

class imgCacheValidator : public nsIStreamListener,
                          public nsIThreadRetargetableStreamListener,
                          public nsIChannelEventSink,
                          public nsIInterfaceRequestor,
                          public nsIAsyncVerifyRedirectCallback
{
public:
  imgCacheValidator(nsProgressNotificationProxy* progress, imgLoader* loader,
                    imgRequest *request, void *aContext, bool forcePrincipalCheckForCacheEntry);

  void AddProxy(imgRequestProxy *aProxy);

  NS_DECL_ISUPPORTS
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

private:
  virtual ~imgCacheValidator();

  nsCOMPtr<nsIStreamListener> mDestListener;
  nsRefPtr<nsProgressNotificationProxy> mProgressProxy;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mRedirectChannel;

  nsRefPtr<imgRequest> mRequest;
  nsCOMArray<imgIRequest> mProxies;

  nsRefPtr<imgRequest> mNewRequest;
  nsRefPtr<imgCacheEntry> mNewEntry;

  void *mContext;

  imgLoader* mImgLoader;
};

#endif  // imgLoader_h__
