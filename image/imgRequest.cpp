/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgRequest.h"
#include "ImageLogging.h"

#include "imgLoader.h"
#include "imgRequestProxy.h"
#include "DecodePool.h"
#include "ProgressTracker.h"
#include "ImageFactory.h"
#include "Image.h"
#include "MultipartImage.h"
#include "RasterImage.h"

#include "nsIChannel.h"
#include "nsICacheInfoChannel.h"
#include "nsIDocument.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsIInputStream.h"
#include "nsIMultiPartChannel.h"
#include "nsIHttpChannel.h"
#include "nsIApplicationCache.h"
#include "nsIApplicationCacheChannel.h"
#include "nsMimeTypes.h"

#include "nsIInterfaceRequestorUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsIScriptSecurityManager.h"
#include "nsContentUtils.h"

#include "plstr.h" // PL_strcasestr(...)
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "imgIRequest.h"

using namespace mozilla;
using namespace mozilla::image;

#define LOG_TEST(level) (MOZ_LOG_TEST(gImgLog, (level)))

NS_IMPL_ISUPPORTS(imgRequest,
                  nsIStreamListener, nsIRequestObserver,
                  nsIThreadRetargetableStreamListener,
                  nsIChannelEventSink,
                  nsIInterfaceRequestor,
                  nsIAsyncVerifyRedirectCallback)

imgRequest::imgRequest(imgLoader* aLoader, const ImageCacheKey& aCacheKey)
 : mLoader(aLoader)
 , mCacheKey(aCacheKey)
 , mLoadId(nullptr)
 , mFirstProxy(nullptr)
 , mValidator(nullptr)
 , mInnerWindowId(0)
 , mCORSMode(imgIRequest::CORS_NONE)
 , mReferrerPolicy(mozilla::net::RP_Default)
 , mImageErrorCode(NS_OK)
 , mMutex("imgRequest")
 , mProgressTracker(new ProgressTracker())
 , mIsMultiPartChannel(false)
 , mGotData(false)
 , mIsInCache(false)
 , mDecodeRequested(false)
 , mNewPartPending(false)
 , mHadInsecureRedirect(false)
{ }

imgRequest::~imgRequest()
{
  if (mLoader) {
    mLoader->RemoveFromUncachedImages(this);
  }
  if (mURI) {
    nsAutoCString spec;
    mURI->GetSpec(spec);
    LOG_FUNC_WITH_PARAM(gImgLog, "imgRequest::~imgRequest()",
                        "keyuri", spec.get());
  } else
    LOG_FUNC(gImgLog, "imgRequest::~imgRequest()");
}

nsresult
imgRequest::Init(nsIURI *aURI,
                 nsIURI *aCurrentURI,
                 bool aHadInsecureRedirect,
                 nsIRequest *aRequest,
                 nsIChannel *aChannel,
                 imgCacheEntry *aCacheEntry,
                 nsISupports* aCX,
                 nsIPrincipal* aLoadingPrincipal,
                 int32_t aCORSMode,
                 ReferrerPolicy aReferrerPolicy)
{
  MOZ_ASSERT(NS_IsMainThread(), "Cannot use nsIURI off main thread!");

  LOG_FUNC(gImgLog, "imgRequest::Init");

  MOZ_ASSERT(!mImage, "Multiple calls to init");
  MOZ_ASSERT(aURI, "No uri");
  MOZ_ASSERT(aCurrentURI, "No current uri");
  MOZ_ASSERT(aRequest, "No request");
  MOZ_ASSERT(aChannel, "No channel");

  mProperties = do_CreateInstance("@mozilla.org/properties;1");

  // Use ImageURL to ensure access to URI data off main thread.
  mURI = new ImageURL(aURI);
  mCurrentURI = aCurrentURI;
  mRequest = aRequest;
  mChannel = aChannel;
  mTimedChannel = do_QueryInterface(mChannel);

  mLoadingPrincipal = aLoadingPrincipal;
  mCORSMode = aCORSMode;
  mReferrerPolicy = aReferrerPolicy;

  // If the original URI and the current URI are different, check whether the
  // original URI is secure. We deliberately don't take the current URI into
  // account, as it needs to be handled using more complicated rules than
  // earlier elements of the redirect chain.
  if (aURI != aCurrentURI) {
    bool isHttps = false;
    bool isChrome = false;
    bool schemeLocal = false;
    if (NS_FAILED(aURI->SchemeIs("https", &isHttps)) ||
        NS_FAILED(aURI->SchemeIs("chrome", &isChrome)) ||
        NS_FAILED(NS_URIChainHasFlags(
                  aURI,
                  nsIProtocolHandler::URI_IS_LOCAL_RESOURCE , &schemeLocal))  ||
        (!isHttps && !isChrome && !schemeLocal)) {
      mHadInsecureRedirect = true;
    }
  }

  // imgCacheValidator may have handled redirects before we were created, so we
  // allow the caller to let us know if any redirects were insecure.
  mHadInsecureRedirect = mHadInsecureRedirect || aHadInsecureRedirect;

  mChannel->GetNotificationCallbacks(getter_AddRefs(mPrevChannelSink));

  NS_ASSERTION(mPrevChannelSink != this,
               "Initializing with a channel that already calls back to us!");

  mChannel->SetNotificationCallbacks(this);

  mCacheEntry = aCacheEntry;
  mCacheEntry->UpdateLoadTime();

  SetLoadId(aCX);

  // Grab the inner window ID of the loading document, if possible.
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aCX);
  if (doc) {
    mInnerWindowId = doc->InnerWindowID();
  }

  return NS_OK;
}

void
imgRequest::ClearLoader() {
  mLoader = nullptr;
}

already_AddRefed<ProgressTracker>
imgRequest::GetProgressTracker() const
{
  MutexAutoLock lock(mMutex);

  if (mImage) {
    MOZ_ASSERT(!mProgressTracker,
               "Should have given mProgressTracker to mImage");
    return mImage->GetProgressTracker();
  } else {
    MOZ_ASSERT(mProgressTracker,
               "Should have mProgressTracker until we create mImage");
    RefPtr<ProgressTracker> progressTracker = mProgressTracker;
    MOZ_ASSERT(progressTracker);
    return progressTracker.forget();
  }
}

void imgRequest::SetCacheEntry(imgCacheEntry* entry)
{
  mCacheEntry = entry;
}

bool
imgRequest::HasCacheEntry() const
{
  return mCacheEntry != nullptr;
}

void
imgRequest::ResetCacheEntry()
{
  if (HasCacheEntry()) {
    mCacheEntry->SetDataSize(0);
  }
}

void
imgRequest::AddProxy(imgRequestProxy* proxy)
{
  NS_PRECONDITION(proxy, "null imgRequestProxy passed in");
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::AddProxy", "proxy", proxy);

  if (!mFirstProxy) {
    // Save a raw pointer to the first proxy we see, for use in the network
    // priority logic.
    mFirstProxy = proxy;
  }

  // If we're empty before adding, we have to tell the loader we now have
  // proxies.
  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  if (progressTracker->ObserverCount() == 0) {
    MOZ_ASSERT(mURI, "Trying to SetHasProxies without key uri.");
    if (mLoader) {
      mLoader->SetHasProxies(this);
    }
  }

  progressTracker->AddObserver(proxy);
}

nsresult
imgRequest::RemoveProxy(imgRequestProxy* proxy, nsresult aStatus)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::RemoveProxy", "proxy", proxy);

  // This will remove our animation consumers, so after removing
  // this proxy, we don't end up without proxies with observers, but still
  // have animation consumers.
  proxy->ClearAnimationConsumers();

  // Let the status tracker do its thing before we potentially call Cancel()
  // below, because Cancel() may result in OnStopRequest being called back
  // before Cancel() returns, leaving the image in a different state then the
  // one it was in at this point.
  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  if (!progressTracker->RemoveObserver(proxy)) {
    return NS_OK;
  }

  if (progressTracker->ObserverCount() == 0) {
    // If we have no observers, there's nothing holding us alive. If we haven't
    // been cancelled and thus removed from the cache, tell the image loader so
    // we can be evicted from the cache.
    if (mCacheEntry) {
      MOZ_ASSERT(mURI, "Removing last observer without key uri.");

      if (mLoader) {
        mLoader->SetHasNoProxies(this, mCacheEntry);
      }
    } else if (MOZ_LOG_TEST(gImgLog, LogLevel::Debug)) {
      nsAutoCString spec;
      mURI->GetSpec(spec);
      LOG_MSG_WITH_PARAM(gImgLog,
                         "imgRequest::RemoveProxy no cache entry",
                         "uri", spec.get());
    }

    /* If |aStatus| is a failure code, then cancel the load if it is still in
       progress.  Otherwise, let the load continue, keeping 'this' in the cache
       with no observers.  This way, if a proxy is destroyed without calling
       cancel on it, it won't leak and won't leave a bad pointer in the observer
       list.
     */
    if (!(progressTracker->GetProgress() & FLAG_LAST_PART_COMPLETE) &&
        NS_FAILED(aStatus)) {
      LOG_MSG(gImgLog, "imgRequest::RemoveProxy",
              "load in progress.  canceling");

      this->Cancel(NS_BINDING_ABORTED);
    }

    /* break the cycle from the cache entry. */
    mCacheEntry = nullptr;
  }

  // If a proxy is removed for a reason other than its owner being
  // changed, remove the proxy from the loadgroup.
  if (aStatus != NS_IMAGELIB_CHANGING_OWNER) {
    proxy->RemoveFromLoadGroup(true);
  }

  return NS_OK;
}

void
imgRequest::CancelAndAbort(nsresult aStatus)
{
  LOG_SCOPE(gImgLog, "imgRequest::CancelAndAbort");

  Cancel(aStatus);

  // It's possible for the channel to fail to open after we've set our
  // notification callbacks. In that case, make sure to break the cycle between
  // the channel and us, because it won't.
  if (mChannel) {
    mChannel->SetNotificationCallbacks(mPrevChannelSink);
    mPrevChannelSink = nullptr;
  }
}

class imgRequestMainThreadCancel : public Runnable
{
public:
  imgRequestMainThreadCancel(imgRequest* aImgRequest, nsresult aStatus)
    : mImgRequest(aImgRequest)
    , mStatus(aStatus)
  {
    MOZ_ASSERT(!NS_IsMainThread(), "Create me off main thread only!");
    MOZ_ASSERT(aImgRequest);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "I should be running on the main thread!");
    mImgRequest->ContinueCancel(mStatus);
    return NS_OK;
  }
private:
  RefPtr<imgRequest> mImgRequest;
  nsresult mStatus;
};

void
imgRequest::Cancel(nsresult aStatus)
{
  /* The Cancel() method here should only be called by this class. */
  LOG_SCOPE(gImgLog, "imgRequest::Cancel");

  if (NS_IsMainThread()) {
    ContinueCancel(aStatus);
  } else {
    NS_DispatchToMainThread(new imgRequestMainThreadCancel(this, aStatus));
  }
}

void
imgRequest::ContinueCancel(nsresult aStatus)
{
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  progressTracker->SyncNotifyProgress(FLAG_HAS_ERROR | FLAG_ONLOAD_UNBLOCKED);

  RemoveFromCache();

  if (mRequest && !(progressTracker->GetProgress() & FLAG_LAST_PART_COMPLETE)) {
     mRequest->Cancel(aStatus);
  }
}

class imgRequestMainThreadEvict : public Runnable
{
public:
  explicit imgRequestMainThreadEvict(imgRequest* aImgRequest)
    : mImgRequest(aImgRequest)
  {
    MOZ_ASSERT(!NS_IsMainThread(), "Create me off main thread only!");
    MOZ_ASSERT(aImgRequest);
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread(), "I should be running on the main thread!");
    mImgRequest->ContinueEvict();
    return NS_OK;
  }
private:
  RefPtr<imgRequest> mImgRequest;
};

// EvictFromCache() is written to allowed to get called from any thread
void
imgRequest::EvictFromCache()
{
  /* The EvictFromCache() method here should only be called by this class. */
  LOG_SCOPE(gImgLog, "imgRequest::EvictFromCache");

  if (NS_IsMainThread()) {
    ContinueEvict();
  } else {
    NS_DispatchToMainThread(new imgRequestMainThreadEvict(this));
  }
}

// Helper-method used by EvictFromCache()
void
imgRequest::ContinueEvict()
{
  MOZ_ASSERT(NS_IsMainThread());

  RemoveFromCache();
}

void
imgRequest::StartDecoding()
{
  MutexAutoLock lock(mMutex);
  mDecodeRequested = true;
}

bool
imgRequest::IsDecodeRequested() const
{
  MutexAutoLock lock(mMutex);
  return mDecodeRequested;
}

nsresult imgRequest::GetURI(ImageURL** aURI)
{
  MOZ_ASSERT(aURI);

  LOG_FUNC(gImgLog, "imgRequest::GetURI");

  if (mURI) {
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult
imgRequest::GetCurrentURI(nsIURI** aURI)
{
  MOZ_ASSERT(aURI);

  LOG_FUNC(gImgLog, "imgRequest::GetCurrentURI");

  if (mCurrentURI) {
    *aURI = mCurrentURI;
    NS_ADDREF(*aURI);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

bool
imgRequest::IsChrome() const
{
  bool isChrome = false;
  if (NS_WARN_IF(NS_FAILED(mURI->SchemeIs("chrome", &isChrome)))) {
    return false;
  }
  return isChrome;
}

nsresult
imgRequest::GetImageErrorCode()
{
  return mImageErrorCode;
}

nsresult
imgRequest::GetSecurityInfo(nsISupports** aSecurityInfo)
{
  LOG_FUNC(gImgLog, "imgRequest::GetSecurityInfo");

  // Missing security info means this is not a security load
  // i.e. it is not an error when security info is missing
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  return NS_OK;
}

void
imgRequest::RemoveFromCache()
{
  LOG_SCOPE(gImgLog, "imgRequest::RemoveFromCache");

  bool isInCache = false;

  {
    MutexAutoLock lock(mMutex);
    isInCache = mIsInCache;
  }

  if (isInCache && mLoader) {
    // mCacheEntry is nulled out when we have no more observers.
    if (mCacheEntry) {
      mLoader->RemoveFromCache(mCacheEntry);
    } else {
      mLoader->RemoveFromCache(mCacheKey);
    }
  }

  mCacheEntry = nullptr;
}

bool
imgRequest::HasConsumers() const
{
  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  return progressTracker && progressTracker->ObserverCount() > 0;
}

already_AddRefed<Image>
imgRequest::GetImage() const
{
  MutexAutoLock lock(mMutex);
  RefPtr<Image> image = mImage;
  return image.forget();
}

int32_t imgRequest::Priority() const
{
  int32_t priority = nsISupportsPriority::PRIORITY_NORMAL;
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mRequest);
  if (p) {
    p->GetPriority(&priority);
  }
  return priority;
}

void
imgRequest::AdjustPriority(imgRequestProxy* proxy, int32_t delta)
{
  // only the first proxy is allowed to modify the priority of this image load.
  //
  // XXX(darin): this is probably not the most optimal algorithm as we may want
  // to increase the priority of requests that have a lot of proxies.  the key
  // concern though is that image loads remain lower priority than other pieces
  // of content such as link clicks, CSS, and JS.
  //
  if (!mFirstProxy || proxy != mFirstProxy) {
    return;
  }

  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mChannel);
  if (p) {
    p->AdjustPriority(delta);
  }
}

bool
imgRequest::HasTransferredData() const
{
  MutexAutoLock lock(mMutex);
  return mGotData;
}

void
imgRequest::SetIsInCache(bool aInCache)
{
  LOG_FUNC_WITH_PARAM(gImgLog,
                      "imgRequest::SetIsCacheable", "aInCache", aInCache);
  MutexAutoLock lock(mMutex);
  mIsInCache = aInCache;
}

void
imgRequest::UpdateCacheEntrySize()
{
  if (!mCacheEntry) {
    return;
  }

  RefPtr<Image> image = GetImage();
  size_t size = image->SizeOfSourceWithComputedFallback(moz_malloc_size_of);
  mCacheEntry->SetDataSize(size);
}

void
imgRequest::SetCacheValidation(imgCacheEntry* aCacheEntry, nsIRequest* aRequest)
{
  /* get the expires info */
  if (aCacheEntry) {
    nsCOMPtr<nsICacheInfoChannel> cacheChannel(do_QueryInterface(aRequest));
    if (cacheChannel) {
      uint32_t expiration = 0;
      /* get the expiration time from the caching channel's token */
      if (NS_SUCCEEDED(cacheChannel->GetCacheTokenExpirationTime(&expiration))) {
        // Expiration time defaults to 0. We set the expiration time on our
        // entry if it hasn't been set yet.
        if (aCacheEntry->GetExpiryTime() == 0) {
          aCacheEntry->SetExpiryTime(expiration);
        }
      }
    }

    // Determine whether the cache entry must be revalidated when we try to use
    // it. Currently, only HTTP specifies this information...
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
    if (httpChannel) {
      bool bMustRevalidate = false;

      httpChannel->IsNoStoreResponse(&bMustRevalidate);

      if (!bMustRevalidate) {
        httpChannel->IsNoCacheResponse(&bMustRevalidate);
      }

      if (!bMustRevalidate) {
        nsAutoCString cacheHeader;

        httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Cache-Control"),
                                            cacheHeader);
        if (PL_strcasestr(cacheHeader.get(), "must-revalidate")) {
          bMustRevalidate = true;
        }
      }

      // Cache entries default to not needing to validate. We ensure that
      // multiple calls to this function don't override an earlier decision to
      // validate by making validation a one-way decision.
      if (bMustRevalidate) {
        aCacheEntry->SetMustValidate(bMustRevalidate);
      }
    }
  }
}

namespace {

already_AddRefed<nsIApplicationCache>
GetApplicationCache(nsIRequest* aRequest)
{
  nsresult rv;

  nsCOMPtr<nsIApplicationCacheChannel> appCacheChan =
    do_QueryInterface(aRequest);
  if (!appCacheChan) {
    return nullptr;
  }

  bool fromAppCache;
  rv = appCacheChan->GetLoadedFromApplicationCache(&fromAppCache);
  NS_ENSURE_SUCCESS(rv, nullptr);

  if (!fromAppCache) {
    return nullptr;
  }

  nsCOMPtr<nsIApplicationCache> appCache;
  rv = appCacheChan->GetApplicationCache(getter_AddRefs(appCache));
  NS_ENSURE_SUCCESS(rv, nullptr);

  return appCache.forget();
}

} // namespace

bool
imgRequest::CacheChanged(nsIRequest* aNewRequest)
{
  nsCOMPtr<nsIApplicationCache> newAppCache = GetApplicationCache(aNewRequest);

  // Application cache not involved at all or the same app cache involved
  // in both of the loads (original and new).
  if (newAppCache == mApplicationCache) {
    return false;
  }

  // In a rare case it may happen that two objects still refer
  // the same application cache version.
  if (newAppCache && mApplicationCache) {
    nsresult rv;

    nsAutoCString oldAppCacheClientId, newAppCacheClientId;
    rv = mApplicationCache->GetClientID(oldAppCacheClientId);
    NS_ENSURE_SUCCESS(rv, true);
    rv = newAppCache->GetClientID(newAppCacheClientId);
    NS_ENSURE_SUCCESS(rv, true);

    if (oldAppCacheClientId == newAppCacheClientId) {
      return false;
    }
  }

  // When we get here, app caches differ or app cache is involved
  // just in one of the loads what we also consider as a change
  // in a loading cache.
  return true;
}

bool
imgRequest::GetMultipart() const
{
  MutexAutoLock lock(mMutex);
  return mIsMultiPartChannel;
}

bool
imgRequest::HadInsecureRedirect() const
{
  MutexAutoLock lock(mMutex);
  return mHadInsecureRedirect;
}

/** nsIRequestObserver methods **/

NS_IMETHODIMP
imgRequest::OnStartRequest(nsIRequest* aRequest, nsISupports* ctxt)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartRequest");

  RefPtr<Image> image;

  // Figure out if we're multipart.
  nsCOMPtr<nsIMultiPartChannel> multiPartChannel = do_QueryInterface(aRequest);
  MOZ_ASSERT(multiPartChannel || !mIsMultiPartChannel,
             "Stopped being multipart?"); {
    MutexAutoLock lock(mMutex);
    mNewPartPending = true;
    image = mImage;
    mIsMultiPartChannel = bool(multiPartChannel);
  }

  // If we're not multipart, we shouldn't have an image yet.
  if (image && !multiPartChannel) {
    MOZ_ASSERT_UNREACHABLE("Already have an image for a non-multipart request");
    Cancel(NS_IMAGELIB_ERROR_FAILURE);
    return NS_ERROR_FAILURE;
  }

  /*
   * If mRequest is null here, then we need to set it so that we'll be able to
   * cancel it if our Cancel() method is called.  Note that this can only
   * happen for multipart channels.  We could simply not null out mRequest for
   * non-last parts, if GetIsLastPart() were reliable, but it's not.  See
   * https://bugzilla.mozilla.org/show_bug.cgi?id=339610
   */
  if (!mRequest) {
    MOZ_ASSERT(multiPartChannel, "Should have mRequest unless we're multipart");
    nsCOMPtr<nsIChannel> baseChannel;
    multiPartChannel->GetBaseChannel(getter_AddRefs(baseChannel));
    mRequest = baseChannel;
  }

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel) {
    channel->GetSecurityInfo(getter_AddRefs(mSecurityInfo));

    /* Get our principal */
    nsCOMPtr<nsIScriptSecurityManager>
      secMan = nsContentUtils::GetSecurityManager();
    if (secMan) {
      nsresult rv =
        secMan->GetChannelResultPrincipal(channel, getter_AddRefs(mPrincipal));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  SetCacheValidation(mCacheEntry, aRequest);

  mApplicationCache = GetApplicationCache(aRequest);

  // Shouldn't we be dead already if this gets hit?
  // Probably multipart/x-mixed-replace...
  RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
  if (progressTracker->ObserverCount() == 0) {
    this->Cancel(NS_IMAGELIB_ERROR_FAILURE);
  }

  // Try to retarget OnDataAvailable to a decode thread.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aRequest);
  nsCOMPtr<nsIThreadRetargetableRequest> retargetable =
    do_QueryInterface(aRequest);
  if (httpChannel && retargetable) {
    nsAutoCString mimeType;
    nsresult rv = httpChannel->GetContentType(mimeType);
    if (NS_SUCCEEDED(rv) && !mimeType.EqualsLiteral(IMAGE_SVG_XML)) {
      // Retarget OnDataAvailable to the DecodePool's IO thread.
      nsCOMPtr<nsIEventTarget> target =
        DecodePool::Singleton()->GetIOEventTarget();
      rv = retargetable->RetargetDeliveryTo(target);
    }
    MOZ_LOG(gImgLog, LogLevel::Warning,
           ("[this=%p] imgRequest::OnStartRequest -- "
            "RetargetDeliveryTo rv %d=%s\n",
            this, rv, NS_SUCCEEDED(rv) ? "succeeded" : "failed"));
  }

  return NS_OK;
}

NS_IMETHODIMP
imgRequest::OnStopRequest(nsIRequest* aRequest,
                          nsISupports* ctxt, nsresult status)
{
  LOG_FUNC(gImgLog, "imgRequest::OnStopRequest");
  MOZ_ASSERT(NS_IsMainThread(), "Can't send notifications off-main-thread");

  RefPtr<Image> image = GetImage();

  // XXXldb What if this is a non-last part of a multipart request?
  // xxx before we release our reference to mRequest, lets
  // save the last status that we saw so that the
  // imgRequestProxy will have access to it.
  if (mRequest) {
    mRequest = nullptr;  // we no longer need the request
  }

  // stop holding a ref to the channel, since we don't need it anymore
  if (mChannel) {
    mChannel->SetNotificationCallbacks(mPrevChannelSink);
    mPrevChannelSink = nullptr;
    mChannel = nullptr;
  }

  bool lastPart = true;
  nsCOMPtr<nsIMultiPartChannel> mpchan(do_QueryInterface(aRequest));
  if (mpchan) {
    mpchan->GetIsLastPart(&lastPart);
  }

  bool isPartial = false;
  if (image && (status == NS_ERROR_NET_PARTIAL_TRANSFER)) {
    isPartial = true;
    status = NS_OK; // fake happy face
  }

  // Tell the image that it has all of the source data. Note that this can
  // trigger a failure, since the image might be waiting for more non-optional
  // data and this is the point where we break the news that it's not coming.
  if (image) {
    nsresult rv = image->OnImageDataComplete(aRequest, ctxt, status, lastPart);

    // If we got an error in the OnImageDataComplete() call, we don't want to
    // proceed as if nothing bad happened. However, we also want to give
    // precedence to failure status codes from necko, since presumably they're
    // more meaningful.
    if (NS_FAILED(rv) && NS_SUCCEEDED(status)) {
      status = rv;
    }
  }

  // If the request went through, update the cache entry size. Otherwise,
  // cancel the request, which removes us from the cache.
  if (image && NS_SUCCEEDED(status) && !isPartial) {
    // We update the cache entry size here because this is where we finish
    // loading compressed source data, which is part of our size calculus.
    UpdateCacheEntrySize();

  } else if (isPartial) {
    // Remove the partial image from the cache.
    this->EvictFromCache();

  } else {
    mImageErrorCode = status;

    // if the error isn't "just" a partial transfer
    // stops animations, removes from cache
    this->Cancel(status);
  }

  if (!image) {
    // We have to fire the OnStopRequest notifications ourselves because there's
    // no image capable of doing so.
    Progress progress =
      LoadCompleteProgress(lastPart, /* aError = */ false, status);

    RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
    progressTracker->SyncNotifyProgress(progress);
  }

  mTimedChannel = nullptr;
  return NS_OK;
}

struct mimetype_closure
{
  nsACString* newType;
};

/* prototype for these defined below */
static nsresult
sniff_mimetype_callback(nsIInputStream* in, void* closure,
                        const char* fromRawSegment, uint32_t toOffset,
                        uint32_t count, uint32_t* writeCount);

/** nsThreadRetargetableStreamListener methods **/
NS_IMETHODIMP
imgRequest::CheckListenerChain()
{
  // TODO Might need more checking here.
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread!");
  return NS_OK;
}

/** nsIStreamListener methods **/

struct NewPartResult final
{
  explicit NewPartResult(Image* aExistingImage)
    : mImage(aExistingImage)
    , mIsFirstPart(!aExistingImage)
    , mSucceeded(false)
    , mShouldResetCacheEntry(false)
  { }

  nsAutoCString mContentType;
  nsAutoCString mContentDisposition;
  RefPtr<Image> mImage;
  const bool mIsFirstPart;
  bool mSucceeded;
  bool mShouldResetCacheEntry;
};

static NewPartResult
PrepareForNewPart(nsIRequest* aRequest, nsIInputStream* aInStr, uint32_t aCount,
                  ImageURL* aURI, bool aIsMultipart, Image* aExistingImage,
                  ProgressTracker* aProgressTracker, uint32_t aInnerWindowId)
{
  NewPartResult result(aExistingImage);

  mimetype_closure closure;
  closure.newType = &result.mContentType;

  // Look at the first few bytes and see if we can tell what the data is from
  // that since servers tend to lie. :(
  uint32_t out;
  aInStr->ReadSegments(sniff_mimetype_callback, &closure, aCount, &out);

  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  if (result.mContentType.IsEmpty()) {
    nsresult rv = chan ? chan->GetContentType(result.mContentType)
                       : NS_ERROR_FAILURE;
    if (NS_FAILED(rv)) {
      MOZ_LOG(gImgLog,
              LogLevel::Error, ("imgRequest::PrepareForNewPart -- "
                                "Content type unavailable from the channel\n"));
      return result;
    }
  }

  if (chan) {
    chan->GetContentDispositionHeader(result.mContentDisposition);
  }

  MOZ_LOG(gImgLog, LogLevel::Debug,
         ("imgRequest::PrepareForNewPart -- Got content type %s\n",
          result.mContentType.get()));

  // XXX If server lied about mimetype and it's SVG, we may need to copy
  // the data and dispatch back to the main thread, AND tell the channel to
  // dispatch there in the future.

  // Create the new image and give it ownership of our ProgressTracker.
  if (aIsMultipart) {
    // Create the ProgressTracker and image for this part.
    RefPtr<ProgressTracker> progressTracker = new ProgressTracker();
    RefPtr<Image> partImage =
      ImageFactory::CreateImage(aRequest, progressTracker, result.mContentType,
                                aURI, /* aIsMultipart = */ true,
                                aInnerWindowId);

    if (result.mIsFirstPart) {
      // First part for a multipart channel. Create the MultipartImage wrapper.
      MOZ_ASSERT(aProgressTracker, "Shouldn't have given away tracker yet");
      result.mImage =
        ImageFactory::CreateMultipartImage(partImage, aProgressTracker);
    } else {
      // Transition to the new part.
      auto multipartImage = static_cast<MultipartImage*>(aExistingImage);
      multipartImage->BeginTransitionToPart(partImage);

      // Reset our cache entry size so it doesn't keep growing without bound.
      result.mShouldResetCacheEntry = true;
    }
  } else {
    MOZ_ASSERT(!aExistingImage, "New part for non-multipart channel?");
    MOZ_ASSERT(aProgressTracker, "Shouldn't have given away tracker yet");

    // Create an image using our progress tracker.
    result.mImage =
      ImageFactory::CreateImage(aRequest, aProgressTracker, result.mContentType,
                                aURI, /* aIsMultipart = */ false,
                                aInnerWindowId);
  }

  MOZ_ASSERT(result.mImage);
  if (!result.mImage->HasError() || aIsMultipart) {
    // We allow multipart images to fail to initialize (which generally
    // indicates a bad content type) without cancelling the load, because
    // subsequent parts might be fine.
    result.mSucceeded = true;
  }

  return result;
}

class FinishPreparingForNewPartRunnable final : public Runnable
{
public:
  FinishPreparingForNewPartRunnable(imgRequest* aImgRequest,
                                    NewPartResult&& aResult)
    : mImgRequest(aImgRequest)
    , mResult(aResult)
  {
    MOZ_ASSERT(aImgRequest);
  }

  NS_IMETHOD Run() override
  {
    mImgRequest->FinishPreparingForNewPart(mResult);
    return NS_OK;
  }

private:
  RefPtr<imgRequest> mImgRequest;
  NewPartResult mResult;
};

void
imgRequest::FinishPreparingForNewPart(const NewPartResult& aResult)
{
  MOZ_ASSERT(NS_IsMainThread());

  mContentType = aResult.mContentType;

  SetProperties(aResult.mContentType, aResult.mContentDisposition);

  if (aResult.mIsFirstPart) {
    // Notify listeners that we have an image.
    RefPtr<ProgressTracker> progressTracker = GetProgressTracker();
    progressTracker->OnImageAvailable();
    MOZ_ASSERT(progressTracker->HasImage());
  }

  if (aResult.mShouldResetCacheEntry) {
    ResetCacheEntry();
  }

  if (IsDecodeRequested()) {
    aResult.mImage->StartDecoding();
  }
}

NS_IMETHODIMP
imgRequest::OnDataAvailable(nsIRequest* aRequest, nsISupports* aContext,
                            nsIInputStream* aInStr, uint64_t aOffset,
                            uint32_t aCount)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::OnDataAvailable",
                       "count", aCount);

  NS_ASSERTION(aRequest, "imgRequest::OnDataAvailable -- no request!");

  RefPtr<Image> image;
  RefPtr<ProgressTracker> progressTracker;
  bool isMultipart = false;
  bool newPartPending = false;

  // Retrieve and update our state.
  {
    MutexAutoLock lock(mMutex);
    mGotData = true;
    image = mImage;
    progressTracker = mProgressTracker;
    isMultipart = mIsMultiPartChannel;
    newPartPending = mNewPartPending;
    mNewPartPending = false;
  }

  // If this is a new part, we need to sniff its content type and create an
  // appropriate image.
  if (newPartPending) {
    NewPartResult result = PrepareForNewPart(aRequest, aInStr, aCount, mURI,
                                             isMultipart, image,
                                             progressTracker, mInnerWindowId);
    bool succeeded = result.mSucceeded;

    if (result.mImage) {
      image = result.mImage;

      // Update our state to reflect this new part.
      {
        MutexAutoLock lock(mMutex);
        mImage = image;
        mProgressTracker = nullptr;
      }

      // Some property objects are not threadsafe, and we need to send
      // OnImageAvailable on the main thread, so finish on the main thread.
      if (NS_IsMainThread()) {
        FinishPreparingForNewPart(result);
      } else {
        nsCOMPtr<nsIRunnable> runnable =
          new FinishPreparingForNewPartRunnable(this, Move(result));
        NS_DispatchToMainThread(runnable);
      }
    }

    if (!succeeded) {
      // Something went wrong; probably a content type issue.
      Cancel(NS_IMAGELIB_ERROR_FAILURE);
      return NS_BINDING_ABORTED;
    }
  }

  // Notify the image that it has new data.
  nsresult rv =
    image->OnImageDataAvailable(aRequest, aContext, aInStr, aOffset, aCount);

  if (NS_FAILED(rv)) {
    MOZ_LOG(gImgLog, LogLevel::Warning,
           ("[this=%p] imgRequest::OnDataAvailable -- "
            "copy to RasterImage failed\n", this));
    Cancel(NS_IMAGELIB_ERROR_FAILURE);
    return NS_BINDING_ABORTED;
  }

  return NS_OK;
}

void
imgRequest::SetProperties(const nsACString& aContentType,
                          const nsACString& aContentDisposition)
{
  /* set our mimetype as a property */
  nsCOMPtr<nsISupportsCString> contentType =
    do_CreateInstance("@mozilla.org/supports-cstring;1");
  if (contentType) {
    contentType->SetData(aContentType);
    mProperties->Set("type", contentType);
  }

  /* set our content disposition as a property */
  if (!aContentDisposition.IsEmpty()) {
    nsCOMPtr<nsISupportsCString> contentDisposition =
      do_CreateInstance("@mozilla.org/supports-cstring;1");
    if (contentDisposition) {
      contentDisposition->SetData(aContentDisposition);
      mProperties->Set("content-disposition", contentDisposition);
    }
  }
}

static nsresult
sniff_mimetype_callback(nsIInputStream* in,
                        void* data,
                        const char* fromRawSegment,
                        uint32_t toOffset,
                        uint32_t count,
                        uint32_t* writeCount)
{
  mimetype_closure* closure = static_cast<mimetype_closure*>(data);

  NS_ASSERTION(closure, "closure is null!");

  if (count > 0) {
    imgLoader::GetMimeTypeFromContent(fromRawSegment, count, *closure->newType);
  }

  *writeCount = 0;
  return NS_ERROR_FAILURE;
}


/** nsIInterfaceRequestor methods **/

NS_IMETHODIMP
imgRequest::GetInterface(const nsIID & aIID, void** aResult)
{
  if (!mPrevChannelSink || aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    return QueryInterface(aIID, aResult);
  }

  NS_ASSERTION(mPrevChannelSink != this,
    "Infinite recursion - don't keep track of channel sinks that are us!");
  return mPrevChannelSink->GetInterface(aIID, aResult);
}

/** nsIChannelEventSink methods **/
NS_IMETHODIMP
imgRequest::AsyncOnChannelRedirect(nsIChannel* oldChannel,
                                   nsIChannel* newChannel, uint32_t flags,
                                   nsIAsyncVerifyRedirectCallback* callback)
{
  NS_ASSERTION(mRequest && mChannel,
               "Got a channel redirect after we nulled out mRequest!");
  NS_ASSERTION(mChannel == oldChannel,
               "Got a channel redirect for an unknown channel!");
  NS_ASSERTION(newChannel, "Got a redirect to a NULL channel!");

  SetCacheValidation(mCacheEntry, oldChannel);

  // Prepare for callback
  mRedirectCallback = callback;
  mNewRedirectChannel = newChannel;

  nsCOMPtr<nsIChannelEventSink> sink(do_GetInterface(mPrevChannelSink));
  if (sink) {
    nsresult rv = sink->AsyncOnChannelRedirect(oldChannel, newChannel, flags,
                                               this);
    if (NS_FAILED(rv)) {
        mRedirectCallback = nullptr;
        mNewRedirectChannel = nullptr;
    }
    return rv;
  }

  (void) OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
imgRequest::OnRedirectVerifyCallback(nsresult result)
{
  NS_ASSERTION(mRedirectCallback, "mRedirectCallback not set in callback");
  NS_ASSERTION(mNewRedirectChannel, "mNewRedirectChannel not set in callback");

  if (NS_FAILED(result)) {
      mRedirectCallback->OnRedirectVerifyCallback(result);
      mRedirectCallback = nullptr;
      mNewRedirectChannel = nullptr;
      return NS_OK;
  }

  mChannel = mNewRedirectChannel;
  mTimedChannel = do_QueryInterface(mChannel);
  mNewRedirectChannel = nullptr;

  if (LOG_TEST(LogLevel::Debug)) {
    LOG_MSG_WITH_PARAM(gImgLog,
                       "imgRequest::OnChannelRedirect", "old",
                       mCurrentURI ? mCurrentURI->GetSpecOrDefault().get()
                                   : "");
  }

  // If the previous URI is a non-HTTPS URI, record that fact for later use by
  // security code, which needs to know whether there is an insecure load at any
  // point in the redirect chain.
  bool isHttps = false;
  bool isChrome = false;
  bool schemeLocal = false;
  if (NS_FAILED(mCurrentURI->SchemeIs("https", &isHttps)) ||
      NS_FAILED(mCurrentURI->SchemeIs("chrome", &isChrome)) ||
      NS_FAILED(NS_URIChainHasFlags(mCurrentURI,
                                    nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                                    &schemeLocal))  ||
      (!isHttps && !isChrome && !schemeLocal)) {
    MutexAutoLock lock(mMutex);

    // The csp directive upgrade-insecure-requests performs an internal redirect
    // to upgrade all requests from http to https before any data is fetched from
    // the network. Do not pollute mHadInsecureRedirect in case of such an internal
    // redirect.
    nsCOMPtr<nsILoadInfo> loadInfo = mChannel->GetLoadInfo();
    bool upgradeInsecureRequests = loadInfo ? loadInfo->GetUpgradeInsecureRequests()
                                            : false;
    if (!upgradeInsecureRequests) {
      mHadInsecureRedirect = true;
    }
  }

  // Update the current URI.
  mChannel->GetURI(getter_AddRefs(mCurrentURI));

  if (LOG_TEST(LogLevel::Debug)) {
    LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::OnChannelRedirect", "new",
                       mCurrentURI ? mCurrentURI->GetSpecOrDefault().get()
                                   : "");
  }

  // Make sure we have a protocol that returns data rather than opens an
  // external application, e.g. 'mailto:'.
  bool doesNotReturnData = false;
  nsresult rv =
    NS_URIChainHasFlags(mCurrentURI,
                        nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA,
                        &doesNotReturnData);

  if (NS_SUCCEEDED(rv) && doesNotReturnData) {
    rv = NS_ERROR_ABORT;
  }

  if (NS_FAILED(rv)) {
    mRedirectCallback->OnRedirectVerifyCallback(rv);
    mRedirectCallback = nullptr;
    return NS_OK;
  }

  mRedirectCallback->OnRedirectVerifyCallback(NS_OK);
  mRedirectCallback = nullptr;
  return NS_OK;
}
