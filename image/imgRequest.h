/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_imgRequest_h
#define mozilla_image_imgRequest_h

#include "nsIChannelEventSink.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsIPrincipal.h"

#include "nsCOMPtr.h"
#include "nsProxyRelease.h"
#include "nsStringGlue.h"
#include "nsError.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "mozilla/Mutex.h"
#include "mozilla/net/ReferrerPolicy.h"
#include "ImageCacheKey.h"

class imgCacheValidator;
class imgLoader;
class imgRequestProxy;
class imgCacheEntry;
class nsIApplicationCache;
class nsIProperties;
class nsIRequest;
class nsITimedChannel;
class nsIURI;

namespace mozilla {
namespace image {
class Image;
class ImageURL;
class ProgressTracker;
} // namespace image
} // namespace mozilla

struct NewPartResult;

class imgRequest final : public nsIStreamListener,
                         public nsIThreadRetargetableStreamListener,
                         public nsIChannelEventSink,
                         public nsIInterfaceRequestor,
                         public nsIAsyncVerifyRedirectCallback
{
  typedef mozilla::image::Image Image;
  typedef mozilla::image::ImageCacheKey ImageCacheKey;
  typedef mozilla::image::ImageURL ImageURL;
  typedef mozilla::image::ProgressTracker ProgressTracker;
  typedef mozilla::net::ReferrerPolicy ReferrerPolicy;

public:
  imgRequest(imgLoader* aLoader, const ImageCacheKey& aCacheKey);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSITHREADRETARGETABLESTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

  nsresult Init(nsIURI* aURI,
                nsIURI* aCurrentURI,
                bool aHadInsecureRedirect,
                nsIRequest* aRequest,
                nsIChannel* aChannel,
                imgCacheEntry* aCacheEntry,
                nsISupports* aCX,
                nsIPrincipal* aLoadingPrincipal,
                int32_t aCORSMode,
                ReferrerPolicy aReferrerPolicy);

  void ClearLoader();

  // Callers must call imgRequestProxy::Notify later.
  void AddProxy(imgRequestProxy* proxy);

  nsresult RemoveProxy(imgRequestProxy* proxy, nsresult aStatus);

  // Cancel, but also ensure that all work done in Init() is undone. Call this
  // only when the channel has failed to open, and so calling Cancel() on it
  // won't be sufficient.
  void CancelAndAbort(nsresult aStatus);

  // Called or dispatched by cancel for main thread only execution.
  void ContinueCancel(nsresult aStatus);

  // Called or dispatched by EvictFromCache for main thread only execution.
  void ContinueEvict();

  // Request that we start decoding the image as soon as data becomes available.
  void StartDecoding();

  inline uint64_t InnerWindowID() const {
    return mInnerWindowId;
  }

  // Set the cache validation information (expiry time, whether we must
  // validate, etc) on the cache entry based on the request information.
  // If this function is called multiple times, the information set earliest
  // wins.
  static void SetCacheValidation(imgCacheEntry* aEntry, nsIRequest* aRequest);

  // Check if application cache of the original load is different from
  // application cache of the new load.  Also lack of application cache
  // on one of the loads is considered a change of a loading cache since
  // HTTP cache may contain a different data then app cache.
  bool CacheChanged(nsIRequest* aNewRequest);

  bool GetMultipart() const;

  // Returns whether we went through an insecure (non-HTTPS) redirect at some
  // point during loading. This does not consider the current URI.
  bool HadInsecureRedirect() const;

  // The CORS mode for which we loaded this image.
  int32_t GetCORSMode() const { return mCORSMode; }

  // The Referrer Policy in effect when loading this image.
  ReferrerPolicy GetReferrerPolicy() const { return mReferrerPolicy; }

  // The principal for the document that loaded this image. Used when trying to
  // validate a CORS image load.
  already_AddRefed<nsIPrincipal> GetLoadingPrincipal() const
  {
    nsCOMPtr<nsIPrincipal> principal = mLoadingPrincipal;
    return principal.forget();
  }

  // Return the ProgressTracker associated with this imgRequest. It may live
  // in |mProgressTracker| or in |mImage.mProgressTracker|, depending on whether
  // mImage has been instantiated yet.
  already_AddRefed<ProgressTracker> GetProgressTracker() const;

  /// Returns the Image associated with this imgRequest, if it's ready.
  already_AddRefed<Image> GetImage() const;

  // Get the current principal of the image. No AddRefing.
  inline nsIPrincipal* GetPrincipal() const { return mPrincipal.get(); }

  /// Get the ImageCacheKey associated with this request.
  const ImageCacheKey& CacheKey() const { return mCacheKey; }

  // Resize the cache entry to 0 if it exists
  void ResetCacheEntry();

  // OK to use on any thread.
  nsresult GetURI(ImageURL** aURI);
  nsresult GetCurrentURI(nsIURI** aURI);
  bool IsChrome() const;

  nsresult GetImageErrorCode(void);

  /// Returns true if we've received any data.
  bool HasTransferredData() const;

  /// Returns a non-owning pointer to this imgRequest's MIME type.
  const char* GetMimeType() const { return mContentType.get(); }

  /// @return the priority of the underlying network request, or
  /// PRIORITY_NORMAL if it doesn't support nsISupportsPriority.
  int32_t Priority() const;

  /// Adjust the priority of the underlying network request by @aDelta on behalf
  /// of @aProxy.
  void AdjustPriority(imgRequestProxy* aProxy, int32_t aDelta);

  /// Returns a weak pointer to the underlying request.
  nsIRequest* GetRequest() const { return mRequest; }

  nsITimedChannel* GetTimedChannel() const { return mTimedChannel; }

  nsresult GetSecurityInfo(nsISupports** aSecurityInfoOut);

  imgCacheValidator* GetValidator() const { return mValidator; }
  void SetValidator(imgCacheValidator* aValidator) { mValidator = aValidator; }

  void* LoadId() const { return mLoadId; }
  void SetLoadId(void* aLoadId) { mLoadId = aLoadId; }

  /// Reset the cache entry after we've dropped our reference to it. Used by
  /// imgLoader when our cache entry is re-requested after we've dropped our
  /// reference to it.
  void SetCacheEntry(imgCacheEntry* aEntry);

  /// Returns whether we've got a reference to the cache entry.
  bool HasCacheEntry() const;

  /// Set whether this request is stored in the cache. If it isn't, regardless
  /// of whether this request has a non-null mCacheEntry, this imgRequest won't
  /// try to update or modify the image cache.
  void SetIsInCache(bool aCacheable);

  void EvictFromCache();
  void RemoveFromCache();

  // Sets properties for this image; will dispatch to main thread if needed.
  void SetProperties(const nsACString& aContentType,
                     const nsACString& aContentDisposition);

  nsIProperties* Properties() const { return mProperties; }

  bool HasConsumers() const;

private:
  friend class FinishPreparingForNewPartRunnable;

  virtual ~imgRequest();

  void FinishPreparingForNewPart(const NewPartResult& aResult);

  void Cancel(nsresult aStatus);

  // Update the cache entry size based on the image container.
  void UpdateCacheEntrySize();

  /// Returns true if StartDecoding() was called.
  bool IsDecodeRequested() const;

  // Weak reference to parent loader; this request cannot outlive its owner.
  imgLoader* mLoader;
  nsCOMPtr<nsIRequest> mRequest;
  // The original URI we were loaded with. This is the same as the URI we are
  // keyed on in the cache. We store a string here to avoid off main thread
  // refcounting issues with nsStandardURL.
  RefPtr<ImageURL> mURI;
  // The URI of the resource we ended up loading after all redirects, etc.
  nsCOMPtr<nsIURI> mCurrentURI;
  // The principal of the document which loaded this image. Used when
  // validating for CORS.
  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
  // The principal of this image.
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIProperties> mProperties;
  nsCOMPtr<nsISupports> mSecurityInfo;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIInterfaceRequestor> mPrevChannelSink;
  nsCOMPtr<nsIApplicationCache> mApplicationCache;

  nsCOMPtr<nsITimedChannel> mTimedChannel;

  nsCString mContentType;

  /* we hold on to this to this so long as we have observers */
  RefPtr<imgCacheEntry> mCacheEntry;

  /// The key under which this imgRequest is stored in the image cache.
  ImageCacheKey mCacheKey;

  void* mLoadId;

  /// Raw pointer to the first proxy that was added to this imgRequest. Use only
  /// pointer comparisons; there's no guarantee this will remain valid.
  void* mFirstProxy;

  imgCacheValidator* mValidator;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mNewRedirectChannel;

  // The ID of the inner window origin, used for error reporting.
  uint64_t mInnerWindowId;

  // The CORS mode (defined in imgIRequest) this image was loaded with. By
  // default, imgIRequest::CORS_NONE.
  int32_t mCORSMode;

  // The Referrer Policy (defined in ReferrerPolicy.h) used for this image.
  ReferrerPolicy mReferrerPolicy;

  nsresult mImageErrorCode;

  mutable mozilla::Mutex mMutex;

  // Member variables protected by mMutex. Note that *all* flags in our bitfield
  // are protected by mMutex; if you're adding a new flag that isn'protected, it
  // must not be a part of this bitfield.
  RefPtr<ProgressTracker> mProgressTracker;
  RefPtr<Image> mImage;
  bool mIsMultiPartChannel : 1;
  bool mGotData : 1;
  bool mIsInCache : 1;
  bool mDecodeRequested : 1;
  bool mNewPartPending : 1;
  bool mHadInsecureRedirect : 1;
};

#endif // mozilla_image_imgRequest_h
