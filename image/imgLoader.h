/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_imgLoader_h
#define mozilla_image_imgLoader_h

#include "mozilla/Attributes.h"
#include "mozilla/Mutex.h"
#include "mozilla/UniquePtr.h"

#include "imgILoader.h"
#include "imgICache.h"
#include "nsWeakReference.h"
#include "nsIContentSniffer.h"
#include "nsRefPtrHashtable.h"
#include "nsExpirationTracker.h"
#include "ImageCacheKey.h"
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

namespace mozilla {
namespace image {}  // namespace image
}  // namespace mozilla

class imgCacheEntry {
 public:
  static uint32_t SecondsFromPRTime(PRTime prTime);

  imgCacheEntry(imgLoader* loader, imgRequest* request,
                bool aForcePrincipalCheck);
  ~imgCacheEntry();

  nsrefcnt AddRef() {
    MOZ_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");
    NS_ASSERT_OWNINGTHREAD(imgCacheEntry);
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "imgCacheEntry", sizeof(*this));
    return mRefCnt;
  }

  nsrefcnt Release() {
    MOZ_ASSERT(0 != mRefCnt, "dup release");
    NS_ASSERT_OWNINGTHREAD(imgCacheEntry);
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "imgCacheEntry");
    if (mRefCnt == 0) {
      mRefCnt = 1; /* stabilize */
      delete this;
      return 0;
    }
    return mRefCnt;
  }

  uint32_t GetDataSize() const { return mDataSize; }
  void SetDataSize(uint32_t aDataSize) {
    int32_t oldsize = mDataSize;
    mDataSize = aDataSize;
    UpdateCache(mDataSize - oldsize);
  }

  int32_t GetTouchedTime() const { return mTouchedTime; }
  void SetTouchedTime(int32_t time) {
    mTouchedTime = time;
    Touch(/* updateTime = */ false);
  }

  uint32_t GetLoadTime() const { return mLoadTime; }

  void UpdateLoadTime();

  int32_t GetExpiryTime() const { return mExpiryTime; }
  void SetExpiryTime(int32_t aExpiryTime) {
    mExpiryTime = aExpiryTime;
    Touch();
  }

  bool GetMustValidate() const { return mMustValidate; }
  void SetMustValidate(bool aValidate) {
    mMustValidate = aValidate;
    Touch();
  }

  already_AddRefed<imgRequest> GetRequest() const {
    RefPtr<imgRequest> req = mRequest;
    return req.forget();
  }

  bool Evicted() const { return mEvicted; }

  nsExpirationState* GetExpirationState() { return &mExpirationState; }

  bool HasNoProxies() const { return mHasNoProxies; }

  bool ForcePrincipalCheck() const { return mForcePrincipalCheck; }

  imgLoader* Loader() const { return mLoader; }

 private:  // methods
  friend class imgLoader;
  friend class imgCacheQueue;
  void Touch(bool updateTime = true);
  void UpdateCache(int32_t diff = 0);
  void SetEvicted(bool evict) { mEvicted = evict; }
  void SetHasNoProxies(bool hasNoProxies);

  // Private, unimplemented copy constructor.
  imgCacheEntry(const imgCacheEntry&);

 private:  // data
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  imgLoader* mLoader;
  RefPtr<imgRequest> mRequest;
  uint32_t mDataSize;
  int32_t mTouchedTime;
  uint32_t mLoadTime;
  int32_t mExpiryTime;
  nsExpirationState mExpirationState;
  bool mMustValidate : 1;
  bool mEvicted : 1;
  bool mHasNoProxies : 1;
  bool mForcePrincipalCheck : 1;
};

#include <vector>

#define NS_IMGLOADER_CID                             \
  { /* c1354898-e3fe-4602-88a7-c4520c21cb4e */       \
    0xc1354898, 0xe3fe, 0x4602, {                    \
      0x88, 0xa7, 0xc4, 0x52, 0x0c, 0x21, 0xcb, 0x4e \
    }                                                \
  }

class imgCacheQueue {
 public:
  imgCacheQueue();
  void Remove(imgCacheEntry*);
  void Push(imgCacheEntry*);
  void MarkDirty();
  bool IsDirty();
  already_AddRefed<imgCacheEntry> Pop();
  void Refresh();
  uint32_t GetSize() const;
  void UpdateSize(int32_t diff);
  uint32_t GetNumElements() const;
  bool Contains(imgCacheEntry* aEntry) const;
  typedef nsTArray<RefPtr<imgCacheEntry>> queueContainer;
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

enum class AcceptedMimeTypes : uint8_t {
  IMAGES,
  IMAGES_AND_DOCUMENTS,
};

class imgLoader final : public imgILoader,
                        public nsIContentSniffer,
                        public imgICache,
                        public nsSupportsWeakReference,
                        public nsIObserver {
  virtual ~imgLoader();

 public:
  typedef mozilla::image::ImageCacheKey ImageCacheKey;
  typedef nsRefPtrHashtable<nsGenericHashKey<ImageCacheKey>, imgCacheEntry>
      imgCacheTable;
  typedef nsTHashtable<nsPtrHashKey<imgRequest>> imgSet;
  typedef mozilla::Mutex Mutex;

  NS_DECL_ISUPPORTS
  NS_DECL_IMGILOADER
  NS_DECL_NSICONTENTSNIFFER
  NS_DECL_IMGICACHE
  NS_DECL_NSIOBSERVER

  /**
   * Get the normal image loader instance that is used by gecko code, creating
   * it if necessary.
   */
  static imgLoader* NormalLoader();

  /**
   * Get the Private Browsing image loader instance that is used by gecko code,
   * creating it if necessary.
   */
  static imgLoader* PrivateBrowsingLoader();

  /**
   * Gecko code should use NormalLoader() or PrivateBrowsingLoader() to get the
   * appropriate image loader.
   *
   * This constructor is public because the XPCOM module code that creates
   * instances of "@mozilla.org/image/loader;1" / "@mozilla.org/image/cache;1"
   * for nsIComponentManager.createInstance()/nsIServiceManager.getService()
   * calls (now only made by add-ons) needs access to it.
   *
   * XXX We would like to get rid of the nsIServiceManager.getService (and
   * nsIComponentManager.createInstance) method of creating imgLoader objects,
   * but there are add-ons that are still using it.  These add-ons don't
   * actually do anything useful with the loaders that they create since nobody
   * who creates an imgLoader using this method actually QIs to imgILoader and
   * loads images.  They all just QI to imgICache and either call clearCache()
   * or findEntryProperties().  Since they're doing this on an imgLoader that
   * has never loaded images, these calls are useless.  It seems likely that
   * the code that is doing this is just legacy code left over from a time when
   * there was only one imgLoader instance for the entire process.  (Nowadays
   * the correct method to get an imgILoader/imgICache is to call
   * imgITools::getImgCacheForDocument/imgITools::getImgLoaderForDocument.)
   * All the same, even though what these add-ons are doing is a no-op,
   * removing the nsIServiceManager.getService method of creating/getting an
   * imgLoader objects would cause an exception in these add-ons that could
   * break things.
   */
  imgLoader();
  nsresult Init();

  [[nodiscard]] nsresult LoadImage(
      nsIURI* aURI, nsIURI* aInitialDocumentURI, nsIReferrerInfo* aReferrerInfo,
      nsIPrincipal* aLoadingPrincipal, uint64_t aRequestContextID,
      nsILoadGroup* aLoadGroup, imgINotificationObserver* aObserver,
      nsINode* aContext, mozilla::dom::Document* aLoadingDocument,
      nsLoadFlags aLoadFlags, nsISupports* aCacheKey,
      nsContentPolicyType aContentPolicyType, const nsAString& initiatorType,
      bool aUseUrgentStartForChannel, imgRequestProxy** _retval);

  [[nodiscard]] nsresult LoadImageWithChannel(
      nsIChannel* channel, imgINotificationObserver* aObserver,
      nsISupports* aCX, nsIStreamListener** listener,
      imgRequestProxy** _retval);

  static nsresult GetMimeTypeFromContent(const char* aContents,
                                         uint32_t aLength,
                                         nsACString& aContentType);

  /**
   * Returns true if the given mime type may be interpreted as an image.
   *
   * Some MIME types may be interpreted as both images and documents. (At the
   * moment only "image/svg+xml" falls into this category, but there may be more
   * in the future.) Callers which want this function to return true for such
   * MIME types should pass AcceptedMimeTypes::IMAGES_AND_DOCUMENTS for
   * @aAccept.
   *
   * @param aMimeType The MIME type to evaluate.
   * @param aAcceptedMimeTypes Which kinds of MIME types to treat as images.
   */
  static bool SupportImageWithMimeType(
      const char* aMimeType,
      AcceptedMimeTypes aAccept = AcceptedMimeTypes::IMAGES);

  static void GlobalInit();  // for use by the factory
  static void Shutdown();    // for use by the factory
  static void ShutdownMemoryReporter();

  nsresult ClearChromeImageCache();
  nsresult ClearImageCache();
  void MinimizeCaches();

  nsresult InitCache();

  bool RemoveFromCache(const ImageCacheKey& aKey);

  // Enumeration describing if a given entry is in the cache queue or not.
  // There are some cases we know the entry is definitely not in the queue.
  enum class QueueState { MaybeExists, AlreadyRemoved };

  bool RemoveFromCache(imgCacheEntry* entry,
                       QueueState aQueueState = QueueState::MaybeExists);

  bool PutIntoCache(const ImageCacheKey& aKey, imgCacheEntry* aEntry);

  void AddToUncachedImages(imgRequest* aRequest);
  void RemoveFromUncachedImages(imgRequest* aRequest);

  // Returns true if we should prefer evicting cache entry |two| over cache
  // entry |one|.
  // This mixes units in the worst way, but provides reasonable results.
  inline static bool CompareCacheEntries(const RefPtr<imgCacheEntry>& one,
                                         const RefPtr<imgCacheEntry>& two) {
    if (!one) {
      return false;
    }
    if (!two) {
      return true;
    }

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
  bool SetHasNoProxies(imgRequest* aRequest, imgCacheEntry* aEntry);
  bool SetHasProxies(imgRequest* aRequest);

 private:  // methods
  static already_AddRefed<imgLoader> CreateImageLoader();

  bool PreferLoadFromCache(nsIURI* aURI) const;

  bool ValidateEntry(imgCacheEntry* aEntry, nsIURI* aKey,
                     nsIURI* aInitialDocumentURI,
                     nsIReferrerInfo* aReferrerInfo, nsILoadGroup* aLoadGroup,
                     imgINotificationObserver* aObserver, nsISupports* aCX,
                     mozilla::dom::Document* aLoadingDocument,
                     nsLoadFlags aLoadFlags,
                     nsContentPolicyType aContentPolicyType,
                     bool aCanMakeNewChannel, bool* aNewChannelCreated,
                     imgRequestProxy** aProxyRequest,
                     nsIPrincipal* aLoadingPrincipal, int32_t aCORSMode);

  bool ValidateRequestWithNewChannel(
      imgRequest* request, nsIURI* aURI, nsIURI* aInitialDocumentURI,
      nsIReferrerInfo* aReferrerInfo, nsILoadGroup* aLoadGroup,
      imgINotificationObserver* aObserver, nsISupports* aCX,
      mozilla::dom::Document* aLoadingDocument, uint64_t aInnerWindowId,
      nsLoadFlags aLoadFlags, nsContentPolicyType aContentPolicyType,
      imgRequestProxy** aProxyRequest, nsIPrincipal* aLoadingPrincipal,
      int32_t aCORSMode, bool* aNewChannelCreated);

  nsresult CreateNewProxyForRequest(imgRequest* aRequest,
                                    nsILoadGroup* aLoadGroup,
                                    mozilla::dom::Document* aLoadingDocument,
                                    imgINotificationObserver* aObserver,
                                    nsLoadFlags aLoadFlags,
                                    imgRequestProxy** _retval);

  nsresult EvictEntries(imgCacheTable& aCacheToClear);
  nsresult EvictEntries(imgCacheQueue& aQueueToClear);

  imgCacheTable& GetCache(bool aForChrome);
  imgCacheTable& GetCache(const ImageCacheKey& aKey);
  imgCacheQueue& GetCacheQueue(bool aForChrome);
  imgCacheQueue& GetCacheQueue(const ImageCacheKey& aKey);
  void CacheEntriesChanged(bool aForChrome, int32_t aSizeDiff = 0);
  void CheckCacheLimits(imgCacheTable& cache, imgCacheQueue& queue);

 private:  // data
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

  mozilla::UniquePtr<imgCacheExpirationTracker> mCacheTracker;
  bool mRespectPrivacy;
};

/**
 * proxy stream listener class used to handle multipart/x-mixed-replace
 */

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"

class ProxyListener : public nsIStreamListener,
                      public nsIThreadRetargetableStreamListener {
 public:
  explicit ProxyListener(nsIStreamListener* dest);

  /* additional members */
  NS_DECL_THREADSAFE_ISUPPORTS
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
class nsProgressNotificationProxy final : public nsIProgressEventSink,
                                          public nsIChannelEventSink,
                                          public nsIInterfaceRequestor {
 public:
  nsProgressNotificationProxy(nsIChannel* channel, imgIRequest* proxy)
      : mImageRequest(proxy) {
    channel->GetNotificationCallbacks(getter_AddRefs(mOriginalCallbacks));
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIPROGRESSEVENTSINK
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
 private:
  ~nsProgressNotificationProxy() = default;

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
                          public nsIAsyncVerifyRedirectCallback {
 public:
  imgCacheValidator(nsProgressNotificationProxy* progress, imgLoader* loader,
                    imgRequest* aRequest, nsISupports* aContext,
                    uint64_t aInnerWindowId,
                    bool forcePrincipalCheckForCacheEntry);

  void AddProxy(imgRequestProxy* aProxy);
  void RemoveProxy(imgRequestProxy* aProxy);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

 private:
  void UpdateProxies(bool aCancelRequest, bool aSyncNotify);
  virtual ~imgCacheValidator();

  nsCOMPtr<nsIStreamListener> mDestListener;
  RefPtr<nsProgressNotificationProxy> mProgressProxy;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mRedirectChannel;

  RefPtr<imgRequest> mRequest;
  AutoTArray<RefPtr<imgRequestProxy>, 4> mProxies;

  RefPtr<imgRequest> mNewRequest;
  RefPtr<imgCacheEntry> mNewEntry;

  nsCOMPtr<nsISupports> mContext;
  uint64_t mInnerWindowId;

  imgLoader* mImgLoader;

  bool mHadInsecureRedirect;
};

#endif  // mozilla_image_imgLoader_h
