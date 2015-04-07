/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/ClearOnShutdown.h"

#include "ImageLogging.h"
#include "nsPrintfCString.h"
#include "imgLoader.h"
#include "imgRequestProxy.h"

#include "nsCOMPtr.h"

#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsCORSListenerProxy.h"
#include "nsNetUtil.h"
#include "nsMimeTypes.h"
#include "nsStreamUtils.h"
#include "nsIHttpChannel.h"
#include "nsICachingChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIProgressEventSink.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIFileURL.h"
#include "nsCRT.h"
#include "nsINetworkPredictor.h"
#include "mozilla/dom/nsMixedContentBlocker.h"

#include "nsIApplicationCache.h"
#include "nsIApplicationCacheContainer.h"

#include "nsIMemoryReporter.h"
#include "Image.h"
#include "gfxPrefs.h"

// we want to explore making the document own the load group
// so we can associate the document URI with the load group.
// until this point, we have an evil hack:
#include "nsIHttpChannelInternal.h"
#include "nsILoadContext.h"
#include "nsILoadGroupChild.h"

using namespace mozilla;
using namespace mozilla::image;
using namespace mozilla::net;

MOZ_DEFINE_MALLOC_SIZE_OF(ImagesMallocSizeOf)

class imgMemoryReporter final : public nsIMemoryReporter
{
  ~imgMemoryReporter() { }

public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize) override
  {
    nsresult rv;
    nsTArray<ImageMemoryCounter> chrome;
    nsTArray<ImageMemoryCounter> content;
    nsTArray<ImageMemoryCounter> uncached;

    for (uint32_t i = 0; i < mKnownLoaders.Length(); i++) {
      mKnownLoaders[i]->mChromeCache.EnumerateRead(DoRecordCounter, &chrome);
      mKnownLoaders[i]->mCache.EnumerateRead(DoRecordCounter, &content);
      MutexAutoLock lock(mKnownLoaders[i]->mUncachedImagesMutex);
      mKnownLoaders[i]->
        mUncachedImages.EnumerateEntries(DoRecordCounterUncached, &uncached);
    }

    // Note that we only need to anonymize content image URIs.

    rv = ReportCounterArray(aHandleReport, aData, chrome, "images/chrome");
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ReportCounterArray(aHandleReport, aData, content,
                            "images/content", aAnonymize);
    NS_ENSURE_SUCCESS(rv, rv);

    // Uncached images may be content or chrome, so anonymize them.
    rv = ReportCounterArray(aHandleReport, aData, uncached,
                            "images/uncached", aAnonymize);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  static int64_t ImagesContentUsedUncompressedDistinguishedAmount()
  {
    size_t n = 0;
    for (uint32_t i = 0; i < imgLoader::sMemReporter->mKnownLoaders.Length();
         i++) {
      imgLoader::sMemReporter->mKnownLoaders[i]->
        mCache.EnumerateRead(DoRecordCounterUsedDecoded, &n);
    }
    return n;
  }

  void RegisterLoader(imgLoader* aLoader)
  {
    mKnownLoaders.AppendElement(aLoader);
  }

  void UnregisterLoader(imgLoader* aLoader)
  {
    mKnownLoaders.RemoveElement(aLoader);
  }

private:
  nsTArray<imgLoader*> mKnownLoaders;

  struct MemoryCounter
  {
    MemoryCounter()
      : mSource(0)
      , mDecodedHeap(0)
      , mDecodedNonHeap(0)
    { }

    void SetSource(size_t aCount) { mSource = aCount; }
    size_t Source() const { return mSource; }
    void SetDecodedHeap(size_t aCount) { mDecodedHeap = aCount; }
    size_t DecodedHeap() const { return mDecodedHeap; }
    void SetDecodedNonHeap(size_t aCount) { mDecodedNonHeap = aCount; }
    size_t DecodedNonHeap() const { return mDecodedNonHeap; }

    MemoryCounter& operator+=(const MemoryCounter& aOther)
    {
      mSource += aOther.mSource;
      mDecodedHeap += aOther.mDecodedHeap;
      mDecodedNonHeap += aOther.mDecodedNonHeap;
      return *this;
    }

  private:
    size_t mSource;
    size_t mDecodedHeap;
    size_t mDecodedNonHeap;
  };

  struct ImageMemoryCounter
  {
    ImageMemoryCounter(uint16_t aType, const nsACString& aURI, bool aIsUsed)
      : mURI(aURI)
      , mType(aType)
      , mIsUsed(aIsUsed)
    {
      MOZ_ASSERT(!mURI.IsEmpty(), "Should have a URI for all images");
    }

    nsCString& URI() { return mURI; }
    const nsCString& URI() const { return mURI; }
    uint16_t Type() const { return mType; }
    MemoryCounter& Values() { return mValues; }
    const MemoryCounter& Values() const { return mValues; }
    bool IsUsed() const { return mIsUsed; }

    bool IsNotable() const
    {
      const size_t NotableThreshold = 16 * 1024;
      size_t total = mValues.Source() + mValues.DecodedHeap()
                                      + mValues.DecodedNonHeap();
      return total >= NotableThreshold;
    }

  private:
    nsCString mURI;
    uint16_t mType;
    MemoryCounter mValues;
    bool mIsUsed;
  };

  struct MemoryTotal
  {
    MemoryTotal& operator+=(const ImageMemoryCounter& aImageCounter)
    {
      if (aImageCounter.Type() == imgIContainer::TYPE_RASTER) {
        if (aImageCounter.IsUsed()) {
          mUsedRasterCounter += aImageCounter.Values();
        } else {
          mUnusedRasterCounter += aImageCounter.Values();
        }
      } else if (aImageCounter.Type() == imgIContainer::TYPE_VECTOR) {
        if (aImageCounter.IsUsed()) {
          mUsedVectorCounter += aImageCounter.Values();
        } else {
          mUnusedVectorCounter += aImageCounter.Values();
        }
      } else {
        MOZ_CRASH("Unexpected image type");
      }

      return *this;
    }

    const MemoryCounter& UsedRaster() const { return mUsedRasterCounter; }
    const MemoryCounter& UnusedRaster() const { return mUnusedRasterCounter; }
    const MemoryCounter& UsedVector() const { return mUsedVectorCounter; }
    const MemoryCounter& UnusedVector() const { return mUnusedVectorCounter; }

  private:
    MemoryCounter mUsedRasterCounter;
    MemoryCounter mUnusedRasterCounter;
    MemoryCounter mUsedVectorCounter;
    MemoryCounter mUnusedVectorCounter;
  };

  // Reports all images of a single kind, e.g. all used chrome images.
  nsresult ReportCounterArray(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData,
                              const nsTArray<ImageMemoryCounter>& aCounterArray,
                              const char* aPathPrefix,
                              bool aAnonymize = false)
  {
    nsresult rv;
    MemoryTotal summaryTotal;
    MemoryTotal nonNotableTotal;

    // Report notable images, and compute total and non-notable aggregate sizes.
    for (uint32_t i = 0; i < aCounterArray.Length(); i++) {
      ImageMemoryCounter counter = aCounterArray[i];

      if (aAnonymize) {
        counter.URI().Truncate();
        counter.URI().AppendPrintf("<anonymized-%u>", i);
      } else {
        // The URI could be an extremely long data: URI. Truncate if needed.
        static const size_t max = 256;
        if (counter.URI().Length() > max) {
          counter.URI().Truncate(max);
          counter.URI().AppendLiteral(" (truncated)");
        }
        counter.URI().ReplaceChar('/', '\\');
      }

      summaryTotal += counter;

      if (counter.IsNotable()) {
        rv = ReportCounter(aHandleReport, aData, aPathPrefix, counter);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        nonNotableTotal += counter;
      }
    }

    // Report non-notable images in aggregate.
    rv = ReportTotal(aHandleReport, aData, /* aExplicit = */ true,
                     aPathPrefix, "<non-notable images>/", nonNotableTotal);
    NS_ENSURE_SUCCESS(rv, rv);

    // Report a summary in aggregate, outside of the explicit tree.
    rv = ReportTotal(aHandleReport, aData, /* aExplicit = */ false,
                     aPathPrefix, "", summaryTotal);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  static nsresult ReportCounter(nsIHandleReportCallback* aHandleReport,
                                nsISupports* aData,
                                const char* aPathPrefix,
                                const ImageMemoryCounter& aCounter)
  {
    nsAutoCString pathPrefix(NS_LITERAL_CSTRING("explicit/"));
    pathPrefix.Append(aPathPrefix);
    pathPrefix.Append(aCounter.Type() == imgIContainer::TYPE_RASTER
                        ? "/raster/"
                        : "/vector/");
    pathPrefix.Append(aCounter.IsUsed() ? "used/" : "unused/");
    pathPrefix.Append("image(");
    if (aCounter.URI().IsEmpty()) {
      pathPrefix.Append("<unknown URI>");
    } else {
      pathPrefix.Append(aCounter.URI());
    }
    pathPrefix.Append(")/");

    return ReportValues(aHandleReport, aData, pathPrefix, aCounter.Values());
  }

  static nsresult ReportTotal(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData,
                              bool aExplicit,
                              const char* aPathPrefix,
                              const char* aPathInfix,
                              const MemoryTotal& aTotal)
  {
    nsresult rv;

    nsAutoCString pathPrefix;
    if (aExplicit) {
      pathPrefix.Append("explicit/");
    }
    pathPrefix.Append(aPathPrefix);

    nsAutoCString rasterUsedPrefix(pathPrefix);
    rasterUsedPrefix.Append("/raster/used/");
    rasterUsedPrefix.Append(aPathInfix);
    rv = ReportValues(aHandleReport, aData, rasterUsedPrefix,
                      aTotal.UsedRaster());
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString rasterUnusedPrefix(pathPrefix);
    rasterUnusedPrefix.Append("/raster/unused/");
    rasterUnusedPrefix.Append(aPathInfix);
    rv = ReportValues(aHandleReport, aData, rasterUnusedPrefix,
                      aTotal.UnusedRaster());
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString vectorUsedPrefix(pathPrefix);
    vectorUsedPrefix.Append("/vector/used/");
    vectorUsedPrefix.Append(aPathInfix);
    rv = ReportValues(aHandleReport, aData, vectorUsedPrefix,
                      aTotal.UsedVector());
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoCString vectorUnusedPrefix(pathPrefix);
    vectorUnusedPrefix.Append("/vector/unused/");
    vectorUnusedPrefix.Append(aPathInfix);
    rv = ReportValues(aHandleReport, aData, vectorUnusedPrefix,
                      aTotal.UnusedVector());
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  static nsresult ReportValues(nsIHandleReportCallback* aHandleReport,
                               nsISupports* aData,
                               const nsACString& aPathPrefix,
                               const MemoryCounter& aCounter)
  {
    nsresult rv;

    rv = ReportValue(aHandleReport, aData, KIND_HEAP, aPathPrefix,
                     "source",
                     "Raster image source data and vector image documents.",
                     aCounter.Source());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ReportValue(aHandleReport, aData, KIND_HEAP, aPathPrefix,
                     "decoded-heap",
                     "Decoded image data which is stored on the heap.",
                     aCounter.DecodedHeap());
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ReportValue(aHandleReport, aData, KIND_NONHEAP, aPathPrefix,
                     "decoded-nonheap",
                     "Decoded image data which isn't stored on the heap.",
                     aCounter.DecodedNonHeap());
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  static nsresult ReportValue(nsIHandleReportCallback* aHandleReport,
                              nsISupports* aData,
                              int32_t aKind,
                              const nsACString& aPathPrefix,
                              const char* aPathSuffix,
                              const char* aDescription,
                              size_t aValue)
  {
    if (aValue == 0) {
      return NS_OK;
    }

    nsAutoCString desc(aDescription);
    nsAutoCString path(aPathPrefix);
    path.Append(aPathSuffix);

    return aHandleReport->Callback(EmptyCString(), path, aKind, UNITS_BYTES,
                                   aValue, desc, aData);
  }

  static PLDHashOperator DoRecordCounter(const nsACString&,
                                         imgCacheEntry* aEntry,
                                         void* aUserArg)
  {
    nsRefPtr<imgRequest> req = aEntry->GetRequest();
    RecordCounterForRequest(req,
                           static_cast<nsTArray<ImageMemoryCounter>*>(aUserArg),
                           !aEntry->HasNoProxies());
    return PL_DHASH_NEXT;
  }

  static PLDHashOperator
  DoRecordCounterUncached(nsPtrHashKey<imgRequest>* aEntry, void* aUserArg)
  {
    nsRefPtr<imgRequest> req = aEntry->GetKey();
    RecordCounterForRequest(req,
                           static_cast<nsTArray<ImageMemoryCounter>*>(aUserArg),
                           req->HasConsumers());
    return PL_DHASH_NEXT;
  }

  static void RecordCounterForRequest(imgRequest* aRequest,
                                      nsTArray<ImageMemoryCounter>* aArray,
                                      bool aIsUsed)
  {
    nsRefPtr<Image> image = aRequest->GetImage();
    if (!image) {
      return;
    }

    nsRefPtr<ImageURL> imageURL(image->GetURI());
    nsAutoCString spec;
    imageURL->GetSpec(spec);

    ImageMemoryCounter counter(image->GetType(), spec, aIsUsed);

    counter.Values().SetSource(image->
        SizeOfSourceWithComputedFallback(ImagesMallocSizeOf));
    counter.Values().SetDecodedHeap(image->
        SizeOfDecoded(gfxMemoryLocation::IN_PROCESS_HEAP, ImagesMallocSizeOf));
    counter.Values().SetDecodedNonHeap(image->
        SizeOfDecoded(gfxMemoryLocation::IN_PROCESS_NONHEAP, nullptr));

    aArray->AppendElement(counter);
  }

  static PLDHashOperator DoRecordCounterUsedDecoded(const nsACString&,
                                                    imgCacheEntry* aEntry,
                                                    void* aUserArg)
  {
    if (aEntry->HasNoProxies()) {
      return PL_DHASH_NEXT;
    }

    nsRefPtr<imgRequest> req = aEntry->GetRequest();
    nsRefPtr<Image> image = req->GetImage();
    if (!image) {
      return PL_DHASH_NEXT;
    }

    // Both this and EntryImageSizes measure images/content/raster/used/decoded
    // memory.  This function's measurement is secondary -- the result doesn't
    // go in the "explicit" tree -- so we use moz_malloc_size_of instead of
    // ImagesMallocSizeOf to prevent DMD from seeing it reported twice.
    auto n = static_cast<size_t*>(aUserArg);
    *n += image->SizeOfDecoded(gfxMemoryLocation::IN_PROCESS_HEAP,
                               moz_malloc_size_of);
    *n += image->SizeOfDecoded(gfxMemoryLocation::IN_PROCESS_NONHEAP, nullptr);
    return PL_DHASH_NEXT;
  }
};

NS_IMPL_ISUPPORTS(imgMemoryReporter, nsIMemoryReporter)

NS_IMPL_ISUPPORTS(nsProgressNotificationProxy,
                  nsIProgressEventSink,
                  nsIChannelEventSink,
                  nsIInterfaceRequestor)

NS_IMETHODIMP
nsProgressNotificationProxy::OnProgress(nsIRequest* request,
                                        nsISupports* ctxt,
                                        int64_t progress,
                                        int64_t progressMax)
{
  nsCOMPtr<nsILoadGroup> loadGroup;
  request->GetLoadGroup(getter_AddRefs(loadGroup));

  nsCOMPtr<nsIProgressEventSink> target;
  NS_QueryNotificationCallbacks(mOriginalCallbacks,
                                loadGroup,
                                NS_GET_IID(nsIProgressEventSink),
                                getter_AddRefs(target));
  if (!target) {
    return NS_OK;
  }
  return target->OnProgress(mImageRequest, ctxt, progress, progressMax);
}

NS_IMETHODIMP
nsProgressNotificationProxy::OnStatus(nsIRequest* request,
                                      nsISupports* ctxt,
                                      nsresult status,
                                      const char16_t* statusArg)
{
  nsCOMPtr<nsILoadGroup> loadGroup;
  request->GetLoadGroup(getter_AddRefs(loadGroup));

  nsCOMPtr<nsIProgressEventSink> target;
  NS_QueryNotificationCallbacks(mOriginalCallbacks,
                                loadGroup,
                                NS_GET_IID(nsIProgressEventSink),
                                getter_AddRefs(target));
  if (!target) {
    return NS_OK;
  }
  return target->OnStatus(mImageRequest, ctxt, status, statusArg);
}

NS_IMETHODIMP
nsProgressNotificationProxy::
  AsyncOnChannelRedirect(nsIChannel* oldChannel,
                         nsIChannel* newChannel,
                         uint32_t flags,
                         nsIAsyncVerifyRedirectCallback* cb)
{
  // Tell the original original callbacks about it too
  nsCOMPtr<nsILoadGroup> loadGroup;
  newChannel->GetLoadGroup(getter_AddRefs(loadGroup));
  nsCOMPtr<nsIChannelEventSink> target;
  NS_QueryNotificationCallbacks(mOriginalCallbacks,
                                loadGroup,
                                NS_GET_IID(nsIChannelEventSink),
                                getter_AddRefs(target));
  if (!target) {
      cb->OnRedirectVerifyCallback(NS_OK);
      return NS_OK;
  }

  // Delegate to |target| if set, reusing |cb|
  return target->AsyncOnChannelRedirect(oldChannel, newChannel, flags, cb);
}

NS_IMETHODIMP
nsProgressNotificationProxy::GetInterface(const nsIID& iid,
                                          void** result)
{
  if (iid.Equals(NS_GET_IID(nsIProgressEventSink))) {
    *result = static_cast<nsIProgressEventSink*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (iid.Equals(NS_GET_IID(nsIChannelEventSink))) {
    *result = static_cast<nsIChannelEventSink*>(this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (mOriginalCallbacks) {
    return mOriginalCallbacks->GetInterface(iid, result);
  }
  return NS_NOINTERFACE;
}

static void
NewRequestAndEntry(bool aForcePrincipalCheckForCacheEntry, imgLoader* aLoader,
                   imgRequest** aRequest, imgCacheEntry** aEntry)
{
  nsRefPtr<imgRequest> request = new imgRequest(aLoader);
  nsRefPtr<imgCacheEntry> entry =
    new imgCacheEntry(aLoader, request, aForcePrincipalCheckForCacheEntry);
  aLoader->AddToUncachedImages(request);
  request.forget(aRequest);
  entry.forget(aEntry);
}

static bool
ShouldRevalidateEntry(imgCacheEntry* aEntry,
                      nsLoadFlags aFlags,
                      bool aHasExpired)
{
  bool bValidateEntry = false;

  if (aFlags & nsIRequest::LOAD_BYPASS_CACHE) {
    return false;
  }

  if (aFlags & nsIRequest::VALIDATE_ALWAYS) {
    bValidateEntry = true;
  } else if (aEntry->GetMustValidate()) {
    bValidateEntry = true;
  } else if (aHasExpired) {
    // The cache entry has expired...  Determine whether the stale cache
    // entry can be used without validation...
    if (aFlags & (nsIRequest::VALIDATE_NEVER |
                  nsIRequest::VALIDATE_ONCE_PER_SESSION)) {
      // VALIDATE_NEVER and VALIDATE_ONCE_PER_SESSION allow stale cache
      // entries to be used unless they have been explicitly marked to
      // indicate that revalidation is necessary.
      bValidateEntry = false;

    } else if (!(aFlags & nsIRequest::LOAD_FROM_CACHE)) {
      // LOAD_FROM_CACHE allows a stale cache entry to be used... Otherwise,
      // the entry must be revalidated.
      bValidateEntry = true;
    }
  }

  return bValidateEntry;
}

/* Call content policies on cached images that went through a redirect */
static bool
ShouldLoadCachedImage(imgRequest* aImgRequest,
                      nsISupports* aLoadingContext,
                      nsIPrincipal* aLoadingPrincipal)
{
  /* Call content policies on cached images - Bug 1082837
   * Cached images are keyed off of the first uri in a redirect chain.
   * Hence content policies don't get a chance to test the intermediate hops
   * or the final desitnation.  Here we test the final destination using
   * mCurrentURI off of the imgRequest and passing it into content policies.
   * For Mixed Content Blocker, we do an additional check to determine if any
   * of the intermediary hops went through an insecure redirect with the
   * mHadInsecureRedirect flag
   */
  bool insecureRedirect = aImgRequest->HadInsecureRedirect();
  nsCOMPtr<nsIURI> contentLocation;
  aImgRequest->GetCurrentURI(getter_AddRefs(contentLocation));
  nsresult rv;

  int16_t decision = nsIContentPolicy::REJECT_REQUEST;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_IMAGE,
                                 contentLocation,
                                 aLoadingPrincipal,
                                 aLoadingContext,
                                 EmptyCString(), //mime guess
                                 nullptr, //aExtra
                                 &decision,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv) || !NS_CP_ACCEPTED(decision)) {
    return false;
  }

  // We call all Content Policies above, but we also have to call mcb
  // individually to check the intermediary redirect hops are secure.
  if (insecureRedirect) {
    if (!nsContentUtils::IsSystemPrincipal(aLoadingPrincipal)) {
      // Set the requestingLocation from the aLoadingPrincipal.
      nsCOMPtr<nsIURI> requestingLocation;
      if (aLoadingPrincipal) {
        rv = aLoadingPrincipal->GetURI(getter_AddRefs(requestingLocation));
        NS_ENSURE_SUCCESS(rv, false);
      }

      // reset the decision for mixed content blocker check
      decision = nsIContentPolicy::REJECT_REQUEST;
      rv = nsMixedContentBlocker::ShouldLoad(insecureRedirect,
                                             nsIContentPolicy::TYPE_IMAGE,
                                             contentLocation,
                                             requestingLocation,
                                             aLoadingContext,
                                             EmptyCString(), //mime guess
                                             nullptr,
                                             aLoadingPrincipal,
                                             &decision);
      if (NS_FAILED(rv) || !NS_CP_ACCEPTED(decision)) {
        return false;
      }
    }
  }

  return true;
}

// Returns true if this request is compatible with the given CORS mode on the
// given loading principal, and false if the request may not be reused due
// to CORS.  Also checks the Referrer Policy, since requests with different
// referrers/policies may generate different responses.
static bool
ValidateSecurityInfo(imgRequest* request, bool forcePrincipalCheck,
                     int32_t corsmode, nsIPrincipal* loadingPrincipal,
                     nsISupports* aCX, ReferrerPolicy referrerPolicy)
{
  // If the entry's Referrer Policy doesn't match, we can't use this request.
  if (referrerPolicy != request->GetReferrerPolicy()) {
    return false;
  }

  // If the entry's CORS mode doesn't match, or the CORS mode matches but the
  // document principal isn't the same, we can't use this request.
  if (request->GetCORSMode() != corsmode) {
    return false;
  } else if (request->GetCORSMode() != imgIRequest::CORS_NONE ||
             forcePrincipalCheck) {
    nsCOMPtr<nsIPrincipal> otherprincipal = request->GetLoadingPrincipal();

    // If we previously had a principal, but we don't now, we can't use this
    // request.
    if (otherprincipal && !loadingPrincipal) {
      return false;
    }

    if (otherprincipal && loadingPrincipal) {
      bool equals = false;
      otherprincipal->Equals(loadingPrincipal, &equals);
      if (!equals) {
        return false;
      }
    }
  }

  // Content Policy Check on Cached Images
  return ShouldLoadCachedImage(request, aCX, loadingPrincipal);
}

static nsresult
NewImageChannel(nsIChannel** aResult,
                // If aForcePrincipalCheckForCacheEntry is true, then we will
                // force a principal check even when not using CORS before
                // assuming we have a cache hit on a cache entry that we
                // create for this channel.  This is an out param that should
                // be set to true if this channel ends up depending on
                // aLoadingPrincipal and false otherwise.
                bool* aForcePrincipalCheckForCacheEntry,
                nsIURI* aURI,
                nsIURI* aInitialDocumentURI,
                nsIURI* aReferringURI,
                ReferrerPolicy aReferrerPolicy,
                nsILoadGroup* aLoadGroup,
                const nsCString& aAcceptHeader,
                nsLoadFlags aLoadFlags,
                nsContentPolicyType aPolicyType,
                nsIPrincipal* aLoadingPrincipal,
                nsISupports* aRequestingContext)
{
  nsresult rv;
  nsCOMPtr<nsIHttpChannel> newHttpChannel;

  nsCOMPtr<nsIInterfaceRequestor> callbacks;

  if (aLoadGroup) {
    // Get the notification callbacks from the load group for the new channel.
    //
    // XXX: This is not exactly correct, because the network request could be
    //      referenced by multiple windows...  However, the new channel needs
    //      something.  So, using the 'first' notification callbacks is better
    //      than nothing...
    //
    aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
  }

  // Pass in a nullptr loadgroup because this is the underlying network
  // request. This request may be referenced by several proxy image requests
  // (possibly in different documents).
  // If all of the proxy requests are canceled then this request should be
  // canceled too.
  //
  aLoadFlags |= nsIChannel::LOAD_CLASSIFY_URI;

  nsCOMPtr<nsIPrincipal> triggeringPrincipal = aLoadingPrincipal;
  bool isSandBoxed = false;
  // only inherit if we have a principal
  bool inherit = false;
  if (triggeringPrincipal) {
    inherit = nsContentUtils::
      ChannelShouldInheritPrincipal(triggeringPrincipal,
                                    aURI,
                                    false,  // aInheritForAboutBlank
                                    false); // aForceInherit
  } else {
    triggeringPrincipal = nsContentUtils::GetSystemPrincipal();
  }
  nsCOMPtr<nsINode> requestingNode = do_QueryInterface(aRequestingContext);
  nsSecurityFlags securityFlags = nsILoadInfo::SEC_NORMAL;
  if (inherit) {
    securityFlags |= nsILoadInfo::SEC_FORCE_INHERIT_PRINCIPAL;
  }

  // Note we are calling NS_NewChannelWithTriggeringPrincipal() here with a
  // node and a principal. This is for things like background images that are
  // specified by user stylesheets, where the document is being styled, but
  // the principal is that of the user stylesheet.
  if (requestingNode) {
    rv = NS_NewChannelWithTriggeringPrincipal(aResult,
                                              aURI,
                                              requestingNode,
                                              triggeringPrincipal,
                                              securityFlags,
                                              aPolicyType,
                                              nullptr,   // loadGroup
                                              callbacks,
                                              aLoadFlags);
  } else {
    // either we are loading something inside a document, in which case
    // we should always have a requestingNode, or we are loading something
    // outside a document, in which case the triggeringPrincipal
    // should always be the systemPrincipal.
    MOZ_ASSERT(nsContentUtils::IsSystemPrincipal(triggeringPrincipal));
    rv = NS_NewChannel(aResult,
                       aURI,
                       triggeringPrincipal,
                       securityFlags,
                       aPolicyType,
                       nullptr,   // loadGroup
                       callbacks,
                       aLoadFlags);
  }

  if (NS_FAILED(rv)) {
    return rv;
  }

  *aForcePrincipalCheckForCacheEntry = inherit && !isSandBoxed;

  // Initialize HTTP-specific attributes
  newHttpChannel = do_QueryInterface(*aResult);
  if (newHttpChannel) {
    newHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                     aAcceptHeader,
                                     false);

    nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
      do_QueryInterface(newHttpChannel);
    NS_ENSURE_TRUE(httpChannelInternal, NS_ERROR_UNEXPECTED);
    httpChannelInternal->SetDocumentURI(aInitialDocumentURI);
    newHttpChannel->SetReferrerWithPolicy(aReferringURI, aReferrerPolicy);
  }

  // Image channels are loaded by default with reduced priority.
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(*aResult);
  if (p) {
    uint32_t priority = nsISupportsPriority::PRIORITY_LOW;

    if (aLoadFlags & nsIRequest::LOAD_BACKGROUND) {
      ++priority; // further reduce priority for background loads
    }

    p->AdjustPriority(priority);
  }

  // Create a new loadgroup for this new channel, using the old group as
  // the parent. The indirection keeps the channel insulated from cancels,
  // but does allow a way for this revalidation to be associated with at
  // least one base load group for scheduling/caching purposes.

  nsCOMPtr<nsILoadGroup> loadGroup = do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  nsCOMPtr<nsILoadGroupChild> childLoadGroup = do_QueryInterface(loadGroup);
  if (childLoadGroup) {
    childLoadGroup->SetParentLoadGroup(aLoadGroup);
  }
  (*aResult)->SetLoadGroup(loadGroup);

  return NS_OK;
}

static uint32_t
SecondsFromPRTime(PRTime prTime)
{
  return uint32_t(int64_t(prTime) / int64_t(PR_USEC_PER_SEC));
}

imgCacheEntry::imgCacheEntry(imgLoader* loader, imgRequest* request,
                             bool forcePrincipalCheck)
 : mLoader(loader),
   mRequest(request),
   mDataSize(0),
   mTouchedTime(SecondsFromPRTime(PR_Now())),
   mExpiryTime(0),
   mMustValidate(false),
   // We start off as evicted so we don't try to update the cache. PutIntoCache
   // will set this to false.
   mEvicted(true),
   mHasNoProxies(true),
   mForcePrincipalCheck(forcePrincipalCheck)
{ }

imgCacheEntry::~imgCacheEntry()
{
  LOG_FUNC(GetImgLog(), "imgCacheEntry::~imgCacheEntry()");
}

void
imgCacheEntry::Touch(bool updateTime /* = true */)
{
  LOG_SCOPE(GetImgLog(), "imgCacheEntry::Touch");

  if (updateTime) {
    mTouchedTime = SecondsFromPRTime(PR_Now());
  }

  UpdateCache();
}

void
imgCacheEntry::UpdateCache(int32_t diff /* = 0 */)
{
  // Don't update the cache if we've been removed from it or it doesn't care
  // about our size or usage.
  if (!Evicted() && HasNoProxies()) {
    nsRefPtr<ImageURL> uri;
    mRequest->GetURI(getter_AddRefs(uri));
    mLoader->CacheEntriesChanged(uri, diff);
  }
}

void
imgCacheEntry::SetHasNoProxies(bool hasNoProxies)
{
#if defined(PR_LOGGING)
  nsRefPtr<ImageURL> uri;
  mRequest->GetURI(getter_AddRefs(uri));
  nsAutoCString spec;
  if (uri) {
    uri->GetSpec(spec);
  }
  if (hasNoProxies) {
    LOG_FUNC_WITH_PARAM(GetImgLog(), "imgCacheEntry::SetHasNoProxies true",
                        "uri", spec.get());
  } else {
    LOG_FUNC_WITH_PARAM(GetImgLog(), "imgCacheEntry::SetHasNoProxies false",
                        "uri", spec.get());
  }
#endif

  mHasNoProxies = hasNoProxies;
}

imgCacheQueue::imgCacheQueue()
 : mDirty(false),
   mSize(0)
{ }

void
imgCacheQueue::UpdateSize(int32_t diff)
{
  mSize += diff;
}

uint32_t
imgCacheQueue::GetSize() const
{
  return mSize;
}

#include <algorithm>
using namespace std;

void
imgCacheQueue::Remove(imgCacheEntry* entry)
{
  queueContainer::iterator it = find(mQueue.begin(), mQueue.end(), entry);
  if (it != mQueue.end()) {
    mSize -= (*it)->GetDataSize();
    mQueue.erase(it);
    MarkDirty();
  }
}

void
imgCacheQueue::Push(imgCacheEntry* entry)
{
  mSize += entry->GetDataSize();

  nsRefPtr<imgCacheEntry> refptr(entry);
  mQueue.push_back(refptr);
  MarkDirty();
}

already_AddRefed<imgCacheEntry>
imgCacheQueue::Pop()
{
  if (mQueue.empty()) {
    return nullptr;
  }
  if (IsDirty()) {
    Refresh();
  }

  nsRefPtr<imgCacheEntry> entry = mQueue[0];
  std::pop_heap(mQueue.begin(), mQueue.end(), imgLoader::CompareCacheEntries);
  mQueue.pop_back();

  mSize -= entry->GetDataSize();
  return entry.forget();
}

void
imgCacheQueue::Refresh()
{
  std::make_heap(mQueue.begin(), mQueue.end(), imgLoader::CompareCacheEntries);
  mDirty = false;
}

void
imgCacheQueue::MarkDirty()
{
  mDirty = true;
}

bool
imgCacheQueue::IsDirty()
{
  return mDirty;
}

uint32_t
imgCacheQueue::GetNumElements() const
{
  return mQueue.size();
}

imgCacheQueue::iterator
imgCacheQueue::begin()
{
  return mQueue.begin();
}

imgCacheQueue::const_iterator
imgCacheQueue::begin() const
{
  return mQueue.begin();
}

imgCacheQueue::iterator
imgCacheQueue::end()
{
  return mQueue.end();
}

imgCacheQueue::const_iterator
imgCacheQueue::end() const
{
  return mQueue.end();
}

nsresult
imgLoader::CreateNewProxyForRequest(imgRequest* aRequest,
                                    nsILoadGroup* aLoadGroup,
                                    imgINotificationObserver* aObserver,
                                    nsLoadFlags aLoadFlags,
                                    imgRequestProxy** _retval)
{
  LOG_SCOPE_WITH_PARAM(GetImgLog(), "imgLoader::CreateNewProxyForRequest",
                       "imgRequest", aRequest);

  /* XXX If we move decoding onto separate threads, we should save off the
     calling thread here and pass it off to |proxyRequest| so that it call
     proxy calls to |aObserver|.
   */

  nsRefPtr<imgRequestProxy> proxyRequest = new imgRequestProxy();

  /* It is important to call |SetLoadFlags()| before calling |Init()| because
     |Init()| adds the request to the loadgroup.
   */
  proxyRequest->SetLoadFlags(aLoadFlags);

  nsRefPtr<ImageURL> uri;
  aRequest->GetURI(getter_AddRefs(uri));

  // init adds itself to imgRequest's list of observers
  nsresult rv = proxyRequest->Init(aRequest, aLoadGroup, uri, aObserver);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  proxyRequest.forget(_retval);
  return NS_OK;
}

class imgCacheExpirationTracker final
  : public nsExpirationTracker<imgCacheEntry, 3>
{
  enum { TIMEOUT_SECONDS = 10 };
public:
  imgCacheExpirationTracker();

protected:
  void NotifyExpired(imgCacheEntry* entry);
};

imgCacheExpirationTracker::imgCacheExpirationTracker()
 : nsExpirationTracker<imgCacheEntry, 3>(TIMEOUT_SECONDS * 1000)
{ }

void
imgCacheExpirationTracker::NotifyExpired(imgCacheEntry* entry)
{
  // Hold on to a reference to this entry, because the expiration tracker
  // mechanism doesn't.
  nsRefPtr<imgCacheEntry> kungFuDeathGrip(entry);

#if defined(PR_LOGGING)
  nsRefPtr<imgRequest> req(entry->GetRequest());
  if (req) {
    nsRefPtr<ImageURL> uri;
    req->GetURI(getter_AddRefs(uri));
    nsAutoCString spec;
    uri->GetSpec(spec);
    LOG_FUNC_WITH_PARAM(GetImgLog(),
                       "imgCacheExpirationTracker::NotifyExpired",
                       "entry", spec.get());
  }
#endif

  // We can be called multiple times on the same entry. Don't do work multiple
  // times.
  if (!entry->Evicted()) {
    entry->Loader()->RemoveFromCache(entry);
  }

  entry->Loader()->VerifyCacheSizes();
}

double imgLoader::sCacheTimeWeight;
uint32_t imgLoader::sCacheMaxSize;
imgMemoryReporter* imgLoader::sMemReporter;

NS_IMPL_ISUPPORTS(imgLoader, imgILoader, nsIContentSniffer, imgICache,
                  nsISupportsWeakReference, nsIObserver)

static imgLoader* gSingleton = nullptr;
static imgLoader* gPBSingleton = nullptr;

imgLoader*
imgLoader::Singleton()
{
  if (!gSingleton) {
    gSingleton = imgLoader::Create();
  }
  return gSingleton;
}

imgLoader*
imgLoader::PBSingleton()
{
  if (!gPBSingleton) {
    gPBSingleton = imgLoader::Create();
    gPBSingleton->RespectPrivacyNotifications();
  }
  return gPBSingleton;
}

imgLoader::imgLoader()
: mUncachedImagesMutex("imgLoader::UncachedImages"), mRespectPrivacy(false)
{
  sMemReporter->AddRef();
  sMemReporter->RegisterLoader(this);
}

already_AddRefed<imgLoader>
imgLoader::GetInstance()
{
  static nsRefPtr<imgLoader> singleton;
  if (!singleton) {
    singleton = imgLoader::Create();
    if (!singleton) {
        return nullptr;
    }
    ClearOnShutdown(&singleton);
  }
  nsRefPtr<imgLoader> loader = singleton.get();
  return loader.forget();
}

static PLDHashOperator
ClearLoaderPointer(nsPtrHashKey<imgRequest>* aEntry, void* aUserArg)
{
  nsRefPtr<imgRequest> req = aEntry->GetKey();
  req->ClearLoader();

  return PL_DHASH_NEXT;
}

imgLoader::~imgLoader()
{
  ClearChromeImageCache();
  ClearImageCache();
  {
    // If there are any of our imgRequest's left they are in the uncached
    // images set, so clear their pointer to us.
    MutexAutoLock lock(mUncachedImagesMutex);
    mUncachedImages.EnumerateEntries(ClearLoaderPointer, nullptr);
  }
  sMemReporter->UnregisterLoader(this);
  sMemReporter->Release();
}

void
imgLoader::VerifyCacheSizes()
{
#ifdef DEBUG
  if (!mCacheTracker) {
    return;
  }

  uint32_t cachesize = mCache.Count() + mChromeCache.Count();
  uint32_t queuesize =
    mCacheQueue.GetNumElements() + mChromeCacheQueue.GetNumElements();
  uint32_t trackersize = 0;
  for (nsExpirationTracker<imgCacheEntry, 3>::Iterator it(mCacheTracker);
       it.Next(); ){
    trackersize++;
  }
  MOZ_ASSERT(queuesize == trackersize, "Queue and tracker sizes out of sync!");
  MOZ_ASSERT(queuesize <= cachesize, "Queue has more elements than cache!");
#endif
}

imgLoader::imgCacheTable & imgLoader::GetCache(nsIURI* aURI)
{
  MOZ_ASSERT(NS_IsMainThread(), "Cannot use nsIURI off main thread!");
  bool chrome = false;
  aURI->SchemeIs("chrome", &chrome);
  return chrome ? mChromeCache : mCache;
}

imgCacheQueue & imgLoader::GetCacheQueue(nsIURI* aURI)
{
  MOZ_ASSERT(NS_IsMainThread(), "Cannot use nsIURI off main thread!");
  bool chrome = false;
  aURI->SchemeIs("chrome", &chrome);
  return chrome ? mChromeCacheQueue : mCacheQueue;

}

imgLoader::imgCacheTable & imgLoader::GetCache(ImageURL* aURI)
{
  bool chrome = false;
  aURI->SchemeIs("chrome", &chrome);
  return chrome ? mChromeCache : mCache;
}

imgCacheQueue & imgLoader::GetCacheQueue(ImageURL* aURI)
{
  bool chrome = false;
  aURI->SchemeIs("chrome", &chrome);
  return chrome ? mChromeCacheQueue : mCacheQueue;
}

void imgLoader::GlobalInit()
{
  sCacheTimeWeight = gfxPrefs::ImageCacheTimeWeight() / 1000.0;
  int32_t cachesize = gfxPrefs::ImageCacheSize();
  sCacheMaxSize = cachesize > 0 ? cachesize : 0;

  sMemReporter = new imgMemoryReporter();
  RegisterStrongMemoryReporter(sMemReporter);
  RegisterImagesContentUsedUncompressedDistinguishedAmount(
    imgMemoryReporter::ImagesContentUsedUncompressedDistinguishedAmount);
}

nsresult
imgLoader::InitCache()
{
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return NS_ERROR_FAILURE;
  }

  os->AddObserver(this, "memory-pressure", false);
  os->AddObserver(this, "app-theme-changed", false);
  os->AddObserver(this, "chrome-flush-skin-caches", false);
  os->AddObserver(this, "chrome-flush-caches", false);
  os->AddObserver(this, "last-pb-context-exited", false);
  os->AddObserver(this, "profile-before-change", false);
  os->AddObserver(this, "xpcom-shutdown", false);

  mCacheTracker = new imgCacheExpirationTracker();

  return NS_OK;
}

nsresult
imgLoader::Init()
{
  InitCache();

  ReadAcceptHeaderPref();

  Preferences::AddWeakObserver(this, "image.http.accept");

    return NS_OK;
}

NS_IMETHODIMP
imgLoader::RespectPrivacyNotifications()
{
  mRespectPrivacy = true;
  return NS_OK;
}

NS_IMETHODIMP
imgLoader::Observe(nsISupports* aSubject, const char* aTopic,
                   const char16_t* aData)
{
  // We listen for pref change notifications...
  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) {
    if (!NS_strcmp(aData, MOZ_UTF16("image.http.accept"))) {
      ReadAcceptHeaderPref();
    }

  } else if (strcmp(aTopic, "memory-pressure") == 0) {
    MinimizeCaches();
  } else if (strcmp(aTopic, "app-theme-changed") == 0) {
    ClearImageCache();
    MinimizeCaches();
  } else if (strcmp(aTopic, "chrome-flush-skin-caches") == 0 ||
             strcmp(aTopic, "chrome-flush-caches") == 0) {
    MinimizeCaches();
    ClearChromeImageCache();
  } else if (strcmp(aTopic, "last-pb-context-exited") == 0) {
    if (mRespectPrivacy) {
      ClearImageCache();
      ClearChromeImageCache();
    }
  } else if (strcmp(aTopic, "profile-before-change") == 0 ||
             strcmp(aTopic, "xpcom-shutdown") == 0) {
    mCacheTracker = nullptr;

  } else {
  // (Nothing else should bring us here)
    MOZ_ASSERT(0, "Invalid topic received");
  }

  return NS_OK;
}

void imgLoader::ReadAcceptHeaderPref()
{
  nsAdoptingCString accept = Preferences::GetCString("image.http.accept");
  if (accept) {
    mAcceptHeader = accept;
  } else {
    mAcceptHeader =
        IMAGE_PNG "," IMAGE_WILDCARD ";q=0.8," ANY_WILDCARD ";q=0.5";
  }
}

/* void clearCache (in boolean chrome); */
NS_IMETHODIMP
imgLoader::ClearCache(bool chrome)
{
  if (chrome) {
    return ClearChromeImageCache();
  } else {
    return ClearImageCache();
  }
}

/* void removeEntry(in nsIURI uri); */
NS_IMETHODIMP
imgLoader::RemoveEntry(nsIURI* uri)
{
  if (RemoveFromCache(uri)) {
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

/* imgIRequest findEntry(in nsIURI uri); */
NS_IMETHODIMP
imgLoader::FindEntryProperties(nsIURI* uri, nsIProperties** _retval)
{
  nsRefPtr<imgCacheEntry> entry;
  nsAutoCString spec;
  imgCacheTable& cache = GetCache(uri);

  uri->GetSpec(spec);
  *_retval = nullptr;

  if (cache.Get(spec, getter_AddRefs(entry)) && entry) {
    if (mCacheTracker && entry->HasNoProxies()) {
      mCacheTracker->MarkUsed(entry);
    }

    nsRefPtr<imgRequest> request = entry->GetRequest();
    if (request) {
      nsCOMPtr<nsIProperties> properties = request->Properties();
      properties.forget(_retval);
    }
  }

  return NS_OK;
}

void
imgLoader::Shutdown()
{
  NS_IF_RELEASE(gSingleton);
  NS_IF_RELEASE(gPBSingleton);
}

nsresult
imgLoader::ClearChromeImageCache()
{
  return EvictEntries(mChromeCache);
}

nsresult
imgLoader::ClearImageCache()
{
  return EvictEntries(mCache);
}

void
imgLoader::MinimizeCaches()
{
  EvictEntries(mCacheQueue);
  EvictEntries(mChromeCacheQueue);
}

bool
imgLoader::PutIntoCache(nsIURI* key, imgCacheEntry* entry)
{
  imgCacheTable& cache = GetCache(key);

  nsAutoCString spec;
  key->GetSpec(spec);

  LOG_STATIC_FUNC_WITH_PARAM(GetImgLog(),
                             "imgLoader::PutIntoCache", "uri", spec.get());

  // Check to see if this request already exists in the cache and is being
  // loaded on a different thread. If so, don't allow this entry to be added to
  // the cache.
  nsRefPtr<imgCacheEntry> tmpCacheEntry;
  if (cache.Get(spec, getter_AddRefs(tmpCacheEntry)) && tmpCacheEntry) {
    PR_LOG(GetImgLog(), PR_LOG_DEBUG,
           ("[this=%p] imgLoader::PutIntoCache -- Element already in the cache",
            nullptr));
    nsRefPtr<imgRequest> tmpRequest = tmpCacheEntry->GetRequest();

    // If it already exists, and we're putting the same key into the cache, we
    // should remove the old version.
    PR_LOG(GetImgLog(), PR_LOG_DEBUG,
           ("[this=%p] imgLoader::PutIntoCache -- Replacing cached element",
            nullptr));

    RemoveFromCache(key);
  } else {
    PR_LOG(GetImgLog(), PR_LOG_DEBUG,
           ("[this=%p] imgLoader::PutIntoCache --"
            " Element NOT already in the cache", nullptr));
  }

  cache.Put(spec, entry);

  // We can be called to resurrect an evicted entry.
  if (entry->Evicted()) {
    entry->SetEvicted(false);
  }

  // If we're resurrecting an entry with no proxies, put it back in the
  // tracker and queue.
  if (entry->HasNoProxies()) {
    nsresult addrv = NS_OK;

    if (mCacheTracker) {
      addrv = mCacheTracker->AddObject(entry);
    }

    if (NS_SUCCEEDED(addrv)) {
      imgCacheQueue& queue = GetCacheQueue(key);
      queue.Push(entry);
    }
  }

  nsRefPtr<imgRequest> request = entry->GetRequest();
  request->SetIsInCache(true);
  RemoveFromUncachedImages(request);

  return true;
}

bool
imgLoader::SetHasNoProxies(imgRequest* aRequest, imgCacheEntry* aEntry)
{
  nsRefPtr<ImageURL> uri;
  aRequest->GetURI(getter_AddRefs(uri));

#if defined(PR_LOGGING)
  nsAutoCString spec;
  uri->GetSpec(spec);

  LOG_STATIC_FUNC_WITH_PARAM(GetImgLog(),
                             "imgLoader::SetHasNoProxies", "uri", spec.get());
#endif

  aEntry->SetHasNoProxies(true);

  if (aEntry->Evicted()) {
    return false;
  }

  imgCacheQueue& queue = GetCacheQueue(uri);

  nsresult addrv = NS_OK;

  if (mCacheTracker) {
    addrv = mCacheTracker->AddObject(aEntry);
  }

  if (NS_SUCCEEDED(addrv)) {
    queue.Push(aEntry);
  }

  imgCacheTable& cache = GetCache(uri);
  CheckCacheLimits(cache, queue);

  return true;
}

bool
imgLoader::SetHasProxies(imgRequest* aRequest)
{
  VerifyCacheSizes();

  nsRefPtr<ImageURL> uri;
  aRequest->GetURI(getter_AddRefs(uri));

  imgCacheTable& cache = GetCache(uri);

  nsAutoCString spec;
  uri->GetSpec(spec);

  LOG_STATIC_FUNC_WITH_PARAM(GetImgLog(),
                             "imgLoader::SetHasProxies", "uri", spec.get());

  nsRefPtr<imgCacheEntry> entry;
  if (cache.Get(spec, getter_AddRefs(entry)) && entry) {
    // Make sure the cache entry is for the right request
    nsRefPtr<imgRequest> entryRequest = entry->GetRequest();
    if (entryRequest == aRequest && entry->HasNoProxies()) {
      imgCacheQueue& queue = GetCacheQueue(uri);
      queue.Remove(entry);

      if (mCacheTracker) {
        mCacheTracker->RemoveObject(entry);
      }

      entry->SetHasNoProxies(false);

      return true;
    }
  }

  return false;
}

void
imgLoader::CacheEntriesChanged(ImageURL* uri, int32_t sizediff /* = 0 */)
{
  imgCacheQueue& queue = GetCacheQueue(uri);
  queue.MarkDirty();
  queue.UpdateSize(sizediff);
}

void
imgLoader::CheckCacheLimits(imgCacheTable& cache, imgCacheQueue& queue)
{
  if (queue.GetNumElements() == 0) {
    NS_ASSERTION(queue.GetSize() == 0,
                 "imgLoader::CheckCacheLimits -- incorrect cache size");
  }

  // Remove entries from the cache until we're back at our desired max size.
  while (queue.GetSize() > sCacheMaxSize) {
    // Remove the first entry in the queue.
    nsRefPtr<imgCacheEntry> entry(queue.Pop());

    NS_ASSERTION(entry, "imgLoader::CheckCacheLimits -- NULL entry pointer");

#if defined(PR_LOGGING)
    nsRefPtr<imgRequest> req(entry->GetRequest());
    if (req) {
      nsRefPtr<ImageURL> uri;
      req->GetURI(getter_AddRefs(uri));
      nsAutoCString spec;
      uri->GetSpec(spec);
      LOG_STATIC_FUNC_WITH_PARAM(GetImgLog(),
                                 "imgLoader::CheckCacheLimits",
                                 "entry", spec.get());
    }
#endif

    if (entry) {
      RemoveFromCache(entry);
    }
  }
}

bool
imgLoader::ValidateRequestWithNewChannel(imgRequest* request,
                                         nsIURI* aURI,
                                         nsIURI* aInitialDocumentURI,
                                         nsIURI* aReferrerURI,
                                         ReferrerPolicy aReferrerPolicy,
                                         nsILoadGroup* aLoadGroup,
                                         imgINotificationObserver* aObserver,
                                         nsISupports* aCX,
                                         nsLoadFlags aLoadFlags,
                                         nsContentPolicyType aLoadPolicyType,
                                         imgRequestProxy** aProxyRequest,
                                         nsIPrincipal* aLoadingPrincipal,
                                         int32_t aCORSMode)
{
  // now we need to insert a new channel request object inbetween the real
  // request and the proxy that basically delays loading the image until it
  // gets a 304 or figures out that this needs to be a new request

  nsresult rv;

  // If we're currently in the middle of validating this request, just hand
  // back a proxy to it; the required work will be done for us.
  if (request->GetValidator()) {
    rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver,
                                  aLoadFlags, aProxyRequest);
    if (NS_FAILED(rv)) {
      return false;
    }

    if (*aProxyRequest) {
      imgRequestProxy* proxy = static_cast<imgRequestProxy*>(*aProxyRequest);

      // We will send notifications from imgCacheValidator::OnStartRequest().
      // In the mean time, we must defer notifications because we are added to
      // the imgRequest's proxy list, and we can get extra notifications
      // resulting from methods such as RequestDecode(). See bug 579122.
      proxy->SetNotificationsDeferred(true);

      // Attach the proxy without notifying
      request->GetValidator()->AddProxy(proxy);
    }

    return NS_SUCCEEDED(rv);

  } else {
    // We will rely on Necko to cache this request when it's possible, and to
    // tell imgCacheValidator::OnStartRequest whether the request came from its
    // cache.
    nsCOMPtr<nsIChannel> newChannel;
    bool forcePrincipalCheck;
    rv = NewImageChannel(getter_AddRefs(newChannel),
                         &forcePrincipalCheck,
                         aURI,
                         aInitialDocumentURI,
                         aReferrerURI,
                         aReferrerPolicy,
                         aLoadGroup,
                         mAcceptHeader,
                         aLoadFlags,
                         aLoadPolicyType,
                         aLoadingPrincipal,
                         aCX);
    if (NS_FAILED(rv)) {
      return false;
    }

    nsRefPtr<imgRequestProxy> req;
    rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver,
                                  aLoadFlags, getter_AddRefs(req));
    if (NS_FAILED(rv)) {
      return false;
    }

    // Make sure that OnStatus/OnProgress calls have the right request set...
    nsRefPtr<nsProgressNotificationProxy> progressproxy =
        new nsProgressNotificationProxy(newChannel, req);
    if (!progressproxy) {
      return false;
    }

    nsRefPtr<imgCacheValidator> hvc =
      new imgCacheValidator(progressproxy, this, request, aCX,
                            forcePrincipalCheck);

    // Casting needed here to get past multiple inheritance.
    nsCOMPtr<nsIStreamListener> listener =
      do_QueryInterface(static_cast<nsIThreadRetargetableStreamListener*>(hvc));
    NS_ENSURE_TRUE(listener, false);

    // We must set the notification callbacks before setting up the
    // CORS listener, because that's also interested inthe
    // notification callbacks.
    newChannel->SetNotificationCallbacks(hvc);

    if (aCORSMode != imgIRequest::CORS_NONE) {
      bool withCredentials = aCORSMode == imgIRequest::CORS_USE_CREDENTIALS;
      nsRefPtr<nsCORSListenerProxy> corsproxy =
        new nsCORSListenerProxy(listener, aLoadingPrincipal, withCredentials);
      rv = corsproxy->Init(newChannel);
      if (NS_FAILED(rv)) {
        return false;
      }

      listener = corsproxy;
    }

    request->SetValidator(hvc);

    // We will send notifications from imgCacheValidator::OnStartRequest().
    // In the mean time, we must defer notifications because we are added to
    // the imgRequest's proxy list, and we can get extra notifications
    // resulting from methods such as RequestDecode(). See bug 579122.
    req->SetNotificationsDeferred(true);

    // Add the proxy without notifying
    hvc->AddProxy(req);

    mozilla::net::PredictorLearn(aURI, aInitialDocumentURI,
        nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE, aLoadGroup);

    rv = newChannel->AsyncOpen(listener, nullptr);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return false;
    }

    req.forget(aProxyRequest);
    return true;
  }
}

bool
imgLoader::ValidateEntry(imgCacheEntry* aEntry,
                         nsIURI* aURI,
                         nsIURI* aInitialDocumentURI,
                         nsIURI* aReferrerURI,
                         ReferrerPolicy aReferrerPolicy,
                         nsILoadGroup* aLoadGroup,
                         imgINotificationObserver* aObserver,
                         nsISupports* aCX,
                         nsLoadFlags aLoadFlags,
                         nsContentPolicyType aLoadPolicyType,
                         bool aCanMakeNewChannel,
                         imgRequestProxy** aProxyRequest,
                         nsIPrincipal* aLoadingPrincipal,
                         int32_t aCORSMode)
{
  LOG_SCOPE(GetImgLog(), "imgLoader::ValidateEntry");

  bool hasExpired;
  uint32_t expirationTime = aEntry->GetExpiryTime();
  if (expirationTime <= SecondsFromPRTime(PR_Now())) {
    hasExpired = true;
  } else {
    hasExpired = false;
  }

  nsresult rv;

  // Special treatment for file URLs - aEntry has expired if file has changed
  nsCOMPtr<nsIFileURL> fileUrl(do_QueryInterface(aURI));
  if (fileUrl) {
    uint32_t lastModTime = aEntry->GetTouchedTime();

    nsCOMPtr<nsIFile> theFile;
    rv = fileUrl->GetFile(getter_AddRefs(theFile));
    if (NS_SUCCEEDED(rv)) {
      PRTime fileLastMod;
      rv = theFile->GetLastModifiedTime(&fileLastMod);
      if (NS_SUCCEEDED(rv)) {
        // nsIFile uses millisec, NSPR usec
        fileLastMod *= 1000;
        hasExpired = SecondsFromPRTime((PRTime)fileLastMod) > lastModTime;
      }
    }
  }

  nsRefPtr<imgRequest> request(aEntry->GetRequest());

  if (!request) {
    return false;
  }

  if (!ValidateSecurityInfo(request, aEntry->ForcePrincipalCheck(),
                            aCORSMode, aLoadingPrincipal,
                            aCX, aReferrerPolicy))
    return false;

  // data URIs are immutable and by their nature can't leak data, so we can
  // just return true in that case.  Doing so would mean that shift-reload
  // doesn't reload data URI documents/images though (which is handy for
  // debugging during gecko development) so we make an exception in that case.
  nsAutoCString scheme;
  aURI->GetScheme(scheme);
  if (scheme.EqualsLiteral("data") &&
      !(aLoadFlags & nsIRequest::LOAD_BYPASS_CACHE)) {
    return true;
  }

  bool validateRequest = false;

  // If the request's loadId is the same as the aCX, then it is ok to use
  // this one because it has already been validated for this context.
  //
  // XXX: nullptr seems to be a 'special' key value that indicates that NO
  //      validation is required.
  //
  void *key = (void*) aCX;
  if (request->LoadId() != key) {
    // If we would need to revalidate this entry, but we're being told to
    // bypass the cache, we don't allow this entry to be used.
    if (aLoadFlags & nsIRequest::LOAD_BYPASS_CACHE) {
      return false;
    }

    // Determine whether the cache aEntry must be revalidated...
    validateRequest = ShouldRevalidateEntry(aEntry, aLoadFlags, hasExpired);

    PR_LOG(GetImgLog(), PR_LOG_DEBUG,
           ("imgLoader::ValidateEntry validating cache entry. "
            "validateRequest = %d", validateRequest));
#if defined(PR_LOGGING)
  } else if (!key) {
    nsAutoCString spec;
    aURI->GetSpec(spec);

    PR_LOG(GetImgLog(), PR_LOG_DEBUG,
           ("imgLoader::ValidateEntry BYPASSING cache validation for %s "
            "because of NULL LoadID", spec.get()));
#endif
  }

  // We can't use a cached request if it comes from a different
  // application cache than this load is expecting.
  nsCOMPtr<nsIApplicationCacheContainer> appCacheContainer;
  nsCOMPtr<nsIApplicationCache> requestAppCache;
  nsCOMPtr<nsIApplicationCache> groupAppCache;
  if ((appCacheContainer = do_GetInterface(request->GetRequest()))) {
    appCacheContainer->GetApplicationCache(getter_AddRefs(requestAppCache));
  }
  if ((appCacheContainer = do_QueryInterface(aLoadGroup))) {
    appCacheContainer->GetApplicationCache(getter_AddRefs(groupAppCache));
  }

  if (requestAppCache != groupAppCache) {
    PR_LOG(GetImgLog(), PR_LOG_DEBUG,
           ("imgLoader::ValidateEntry - Unable to use cached imgRequest "
            "[request=%p] because of mismatched application caches\n",
            address_of(request)));
    return false;
  }

  if (validateRequest && aCanMakeNewChannel) {
    LOG_SCOPE(GetImgLog(),
              "imgLoader::ValidateRequest |cache hit| must validate");

    return ValidateRequestWithNewChannel(request, aURI, aInitialDocumentURI,
                                         aReferrerURI, aReferrerPolicy,
                                         aLoadGroup, aObserver,
                                         aCX, aLoadFlags, aLoadPolicyType,
                                         aProxyRequest, aLoadingPrincipal,
                                         aCORSMode);
  }

  return !validateRequest;
}

bool
imgLoader::RemoveFromCache(nsIURI* aKey)
{
  MOZ_ASSERT(NS_IsMainThread(), "Cannot use nsIURI off main thread!");

  if (!aKey) return false;

  imgCacheTable& cache = GetCache(aKey);
  imgCacheQueue& queue = GetCacheQueue(aKey);

  nsAutoCString spec;
  aKey->GetSpec(spec);

  return RemoveFromCache(spec, cache, queue);
}

bool
imgLoader::RemoveFromCache(ImageURL* aKey)
{
  if (!aKey) return false;

  imgCacheTable& cache = GetCache(aKey);
  imgCacheQueue& queue = GetCacheQueue(aKey);

  nsAutoCString spec;
  aKey->GetSpec(spec);

  return RemoveFromCache(spec, cache, queue);
}

bool
imgLoader::RemoveFromCache(nsCString& spec,
                           imgCacheTable& cache,
                           imgCacheQueue& queue)
{
  LOG_STATIC_FUNC_WITH_PARAM(GetImgLog(),
                             "imgLoader::RemoveFromCache", "uri", spec.get());

  nsRefPtr<imgCacheEntry> entry;
  if (cache.Get(spec, getter_AddRefs(entry)) && entry) {
    cache.Remove(spec);

    MOZ_ASSERT(!entry->Evicted(), "Evicting an already-evicted cache entry!");

    // Entries with no proxies are in the tracker.
    if (entry->HasNoProxies()) {
      if (mCacheTracker) {
        mCacheTracker->RemoveObject(entry);
      }
      queue.Remove(entry);
    }

    entry->SetEvicted(true);

    nsRefPtr<imgRequest> request = entry->GetRequest();
    request->SetIsInCache(false);
    AddToUncachedImages(request);

    return true;

  } else {
    return false;
  }
}

bool
imgLoader::RemoveFromCache(imgCacheEntry* entry)
{
  LOG_STATIC_FUNC(GetImgLog(), "imgLoader::RemoveFromCache entry");

  nsRefPtr<imgRequest> request = entry->GetRequest();
  if (request) {
    nsRefPtr<ImageURL> key;
    if (NS_SUCCEEDED(request->GetURI(getter_AddRefs(key))) && key) {
      imgCacheTable& cache = GetCache(key);
      imgCacheQueue& queue = GetCacheQueue(key);
      nsAutoCString spec;
      key->GetSpec(spec);

      LOG_STATIC_FUNC_WITH_PARAM(GetImgLog(),
                                 "imgLoader::RemoveFromCache", "entry's uri",
                                 spec.get());

      cache.Remove(spec);

      if (entry->HasNoProxies()) {
        LOG_STATIC_FUNC(GetImgLog(),
                        "imgLoader::RemoveFromCache removing from tracker");
        if (mCacheTracker) {
          mCacheTracker->RemoveObject(entry);
        }
        queue.Remove(entry);
      }

      entry->SetEvicted(true);
      request->SetIsInCache(false);
      AddToUncachedImages(request);

      return true;
    }
  }

  return false;
}

static PLDHashOperator
EnumEvictEntries(const nsACString&,
                 nsRefPtr<imgCacheEntry>& aData,
                 void* data)
{
  nsTArray<nsRefPtr<imgCacheEntry> >* entries =
    reinterpret_cast<nsTArray<nsRefPtr<imgCacheEntry> > *>(data);

  entries->AppendElement(aData);

  return PL_DHASH_NEXT;
}

nsresult
imgLoader::EvictEntries(imgCacheTable& aCacheToClear)
{
  LOG_STATIC_FUNC(GetImgLog(), "imgLoader::EvictEntries table");

  // We have to make a temporary, since RemoveFromCache removes the element
  // from the queue, invalidating iterators.
  nsTArray<nsRefPtr<imgCacheEntry> > entries;
  aCacheToClear.Enumerate(EnumEvictEntries, &entries);

  for (uint32_t i = 0; i < entries.Length(); ++i) {
    if (!RemoveFromCache(entries[i])) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

nsresult
imgLoader::EvictEntries(imgCacheQueue& aQueueToClear)
{
  LOG_STATIC_FUNC(GetImgLog(), "imgLoader::EvictEntries queue");

  // We have to make a temporary, since RemoveFromCache removes the element
  // from the queue, invalidating iterators.
  nsTArray<nsRefPtr<imgCacheEntry> > entries(aQueueToClear.GetNumElements());
  for (imgCacheQueue::const_iterator i = aQueueToClear.begin();
       i != aQueueToClear.end(); ++i) {
    entries.AppendElement(*i);
  }

  for (uint32_t i = 0; i < entries.Length(); ++i) {
    if (!RemoveFromCache(entries[i])) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

void
imgLoader::AddToUncachedImages(imgRequest* aRequest)
{
  MutexAutoLock lock(mUncachedImagesMutex);
  mUncachedImages.PutEntry(aRequest);
}

void
imgLoader::RemoveFromUncachedImages(imgRequest* aRequest)
{
  MutexAutoLock lock(mUncachedImagesMutex);
  mUncachedImages.RemoveEntry(aRequest);
}


#define LOAD_FLAGS_CACHE_MASK    (nsIRequest::LOAD_BYPASS_CACHE | \
                                  nsIRequest::LOAD_FROM_CACHE)

#define LOAD_FLAGS_VALIDATE_MASK (nsIRequest::VALIDATE_ALWAYS |   \
                                  nsIRequest::VALIDATE_NEVER |    \
                                  nsIRequest::VALIDATE_ONCE_PER_SESSION)

NS_IMETHODIMP
imgLoader::LoadImageXPCOM(nsIURI* aURI,
                          nsIURI* aInitialDocumentURI,
                          nsIURI* aReferrerURI,
                          const nsAString& aReferrerPolicy,
                          nsIPrincipal* aLoadingPrincipal,
                          nsILoadGroup* aLoadGroup,
                          imgINotificationObserver* aObserver,
                          nsISupports* aCX,
                          nsLoadFlags aLoadFlags,
                          nsISupports* aCacheKey,
                          nsContentPolicyType aContentPolicyType,
                          imgIRequest** _retval)
{
    // Optional parameter, so defaults to 0 (== TYPE_INVALID)
    if (!aContentPolicyType) {
      aContentPolicyType = nsIContentPolicy::TYPE_IMAGE;
    }
    imgRequestProxy* proxy;
    ReferrerPolicy refpol = ReferrerPolicyFromString(aReferrerPolicy);
    nsresult rv = LoadImage(aURI,
                            aInitialDocumentURI,
                            aReferrerURI,
                            refpol,
                            aLoadingPrincipal,
                            aLoadGroup,
                            aObserver,
                            aCX,
                            aLoadFlags,
                            aCacheKey,
                            aContentPolicyType,
                            EmptyString(),
                            &proxy);
    *_retval = proxy;
    return rv;
}

// imgIRequest loadImage(in nsIURI aURI,
//                       in nsIURI aInitialDocumentURL,
//                       in nsIURI aReferrerURI,
//                       in nsIPrincipal aLoadingPrincipal,
//                       in nsILoadGroup aLoadGroup,
//                       in imgINotificationObserver aObserver,
//                       in nsISupports aCX,
//                       in nsLoadFlags aLoadFlags,
//                       in nsISupports cacheKey);
nsresult
imgLoader::LoadImage(nsIURI* aURI,
                     nsIURI* aInitialDocumentURI,
                     nsIURI* aReferrerURI,
                     ReferrerPolicy aReferrerPolicy,
                     nsIPrincipal* aLoadingPrincipal,
                     nsILoadGroup* aLoadGroup,
                     imgINotificationObserver* aObserver,
                     nsISupports* aCX,
                     nsLoadFlags aLoadFlags,
                     nsISupports* aCacheKey,
                     nsContentPolicyType aContentPolicyType,
                     const nsAString& initiatorType,
                     imgRequestProxy** _retval)
{
  VerifyCacheSizes();

  NS_ASSERTION(aURI, "imgLoader::LoadImage -- NULL URI pointer");

  if (!aURI) {
    return NS_ERROR_NULL_POINTER;
  }

  nsAutoCString spec;
  aURI->GetSpec(spec);
  LOG_SCOPE_WITH_PARAM(GetImgLog(), "imgLoader::LoadImage", "aURI", spec.get());

  *_retval = nullptr;

  nsRefPtr<imgRequest> request;

  nsresult rv;
  nsLoadFlags requestFlags = nsIRequest::LOAD_NORMAL;

#ifdef DEBUG
  bool isPrivate = false;

  if (aLoadGroup) {
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
    if (callbacks) {
      nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(callbacks);
      isPrivate = loadContext && loadContext->UsePrivateBrowsing();
    }
  }
  MOZ_ASSERT(isPrivate == mRespectPrivacy);
#endif

  // Get the default load flags from the loadgroup (if possible)...
  if (aLoadGroup) {
    aLoadGroup->GetLoadFlags(&requestFlags);
  }
  //
  // Merge the default load flags with those passed in via aLoadFlags.
  // Currently, *only* the caching, validation and background load flags
  // are merged...
  //
  // The flags in aLoadFlags take precedence over the default flags!
  //
  if (aLoadFlags & LOAD_FLAGS_CACHE_MASK) {
    // Override the default caching flags...
    requestFlags = (requestFlags & ~LOAD_FLAGS_CACHE_MASK) |
                   (aLoadFlags & LOAD_FLAGS_CACHE_MASK);
  }
  if (aLoadFlags & LOAD_FLAGS_VALIDATE_MASK) {
    // Override the default validation flags...
    requestFlags = (requestFlags & ~LOAD_FLAGS_VALIDATE_MASK) |
                   (aLoadFlags & LOAD_FLAGS_VALIDATE_MASK);
  }
  if (aLoadFlags & nsIRequest::LOAD_BACKGROUND) {
    // Propagate background loading...
    requestFlags |= nsIRequest::LOAD_BACKGROUND;
  }

  int32_t corsmode = imgIRequest::CORS_NONE;
  if (aLoadFlags & imgILoader::LOAD_CORS_ANONYMOUS) {
    corsmode = imgIRequest::CORS_ANONYMOUS;
  } else if (aLoadFlags & imgILoader::LOAD_CORS_USE_CREDENTIALS) {
    corsmode = imgIRequest::CORS_USE_CREDENTIALS;
  }

  nsRefPtr<imgCacheEntry> entry;

  // Look in the cache for our URI, and then validate it.
  // XXX For now ignore aCacheKey. We will need it in the future
  // for correctly dealing with image load requests that are a result
  // of post data.
  imgCacheTable& cache = GetCache(aURI);

  if (cache.Get(spec, getter_AddRefs(entry)) && entry) {
    if (ValidateEntry(entry, aURI, aInitialDocumentURI, aReferrerURI,
                      aReferrerPolicy, aLoadGroup, aObserver, aCX,
                      requestFlags, aContentPolicyType, true, _retval,
                      aLoadingPrincipal, corsmode)) {
      request = entry->GetRequest();

      // If this entry has no proxies, its request has no reference to the
      // entry.
      if (entry->HasNoProxies()) {
        LOG_FUNC_WITH_PARAM(GetImgLog(),
          "imgLoader::LoadImage() adding proxyless entry", "uri", spec.get());
        MOZ_ASSERT(!request->HasCacheEntry(),
          "Proxyless entry's request has cache entry!");
        request->SetCacheEntry(entry);

        if (mCacheTracker) {
          mCacheTracker->MarkUsed(entry);
        }
      }

      entry->Touch();

#ifdef DEBUG_joe
      printf("CACHEGET: %d %s %d\n", time(nullptr), spec.get(),
             entry->SizeOfData());
#endif
    } else {
      // We can't use this entry. We'll try to load it off the network, and if
      // successful, overwrite the old entry in the cache with a new one.
      entry = nullptr;
    }
  }

  // Keep the channel in this scope, so we can adjust its notificationCallbacks
  // later when we create the proxy.
  nsCOMPtr<nsIChannel> newChannel;
  // If we didn't get a cache hit, we need to load from the network.
  if (!request) {
    LOG_SCOPE(GetImgLog(), "imgLoader::LoadImage |cache miss|");

    bool forcePrincipalCheck;
    rv = NewImageChannel(getter_AddRefs(newChannel),
                         &forcePrincipalCheck,
                         aURI,
                         aInitialDocumentURI,
                         aReferrerURI,
                         aReferrerPolicy,
                         aLoadGroup,
                         mAcceptHeader,
                         requestFlags,
                         aContentPolicyType,
                         aLoadingPrincipal,
                         aCX);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(NS_UsePrivateBrowsing(newChannel) == mRespectPrivacy);

    NewRequestAndEntry(forcePrincipalCheck, this, getter_AddRefs(request),
                       getter_AddRefs(entry));

    PR_LOG(GetImgLog(), PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Created new imgRequest"
            " [request=%p]\n", this, request.get()));

    nsCOMPtr<nsILoadGroup> channelLoadGroup;
    newChannel->GetLoadGroup(getter_AddRefs(channelLoadGroup));
    request->Init(aURI, aURI, /* aHadInsecureRedirect = */ false,
                  channelLoadGroup, newChannel, entry, aCX,
                  aLoadingPrincipal, corsmode, aReferrerPolicy);

    // Add the initiator type for this image load
    nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(newChannel);
    if (timedChannel) {
      timedChannel->SetInitiatorType(initiatorType);
    }

    // create the proxy listener
    nsCOMPtr<nsIStreamListener> pl = new ProxyListener(request.get());

    // See if we need to insert a CORS proxy between the proxy listener and the
    // request.
    nsCOMPtr<nsIStreamListener> listener = pl;
    if (corsmode != imgIRequest::CORS_NONE) {
      PR_LOG(GetImgLog(), PR_LOG_DEBUG,
             ("[this=%p] imgLoader::LoadImage -- Setting up a CORS load",
              this));
      bool withCredentials = corsmode == imgIRequest::CORS_USE_CREDENTIALS;

      nsRefPtr<nsCORSListenerProxy> corsproxy =
        new nsCORSListenerProxy(pl, aLoadingPrincipal, withCredentials);
      rv = corsproxy->Init(newChannel);
      if (NS_FAILED(rv)) {
        PR_LOG(GetImgLog(), PR_LOG_DEBUG,
               ("[this=%p] imgLoader::LoadImage -- nsCORSListenerProxy "
                "creation failed: 0x%x\n", this, rv));
        request->CancelAndAbort(rv);
        return NS_ERROR_FAILURE;
      }

      listener = corsproxy;
    }

    PR_LOG(GetImgLog(), PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Calling channel->AsyncOpen()\n",
            this));

    mozilla::net::PredictorLearn(aURI, aInitialDocumentURI,
        nsINetworkPredictor::LEARN_LOAD_SUBRESOURCE, aLoadGroup);

    nsresult openRes = newChannel->AsyncOpen(listener, nullptr);

    if (NS_FAILED(openRes)) {
      PR_LOG(GetImgLog(), PR_LOG_DEBUG,
             ("[this=%p] imgLoader::LoadImage -- AsyncOpen() failed: 0x%x\n",
              this, openRes));
      request->CancelAndAbort(openRes);
      return openRes;
    }

    // Try to add the new request into the cache.
    PutIntoCache(aURI, entry);
  } else {
    LOG_MSG_WITH_PARAM(GetImgLog(),
                       "imgLoader::LoadImage |cache hit|", "request", request);
  }


  // If we didn't get a proxy when validating the cache entry, we need to
  // create one.
  if (!*_retval) {
    // ValidateEntry() has three return values: "Is valid," "might be valid --
    // validating over network", and "not valid." If we don't have a _retval,
    // we know ValidateEntry is not validating over the network, so it's safe
    // to SetLoadId here because we know this request is valid for this context.
    //
    // Note, however, that this doesn't guarantee the behaviour we want (one
    // URL maps to the same image on a page) if we load the same image in a
    // different tab (see bug 528003), because its load id will get re-set, and
    // that'll cause us to validate over the network.
    request->SetLoadId(aCX);

    LOG_MSG(GetImgLog(), "imgLoader::LoadImage", "creating proxy request.");
    rv = CreateNewProxyForRequest(request, aLoadGroup, aObserver,
                                  requestFlags, _retval);
    if (NS_FAILED(rv)) {
      return rv;
    }

    imgRequestProxy* proxy = *_retval;

    // Make sure that OnStatus/OnProgress calls have the right request set, if
    // we did create a channel here.
    if (newChannel) {
      nsCOMPtr<nsIInterfaceRequestor> requestor(
          new nsProgressNotificationProxy(newChannel, proxy));
      if (!requestor) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      newChannel->SetNotificationCallbacks(requestor);
    }

    // Note that it's OK to add here even if the request is done.  If it is,
    // it'll send a OnStopRequest() to the proxy in imgRequestProxy::Notify and
    // the proxy will be removed from the loadgroup.
    proxy->AddToLoadGroup();

    // If we're loading off the network, explicitly don't notify our proxy,
    // because necko (or things called from necko, such as imgCacheValidator)
    // are going to call our notifications asynchronously, and we can't make it
    // further asynchronous because observers might rely on imagelib completing
    // its work between the channel's OnStartRequest and OnStopRequest.
    if (!newChannel) {
      proxy->NotifyListener();
    }

    return rv;
  }

  NS_ASSERTION(*_retval, "imgLoader::LoadImage -- no return value");

  return NS_OK;
}

/* imgIRequest
loadImageWithChannelXPCOM(in nsIChannel channel,
                          in imgINotificationObserver aObserver,
                          in nsISupports cx,
                          out nsIStreamListener); */
NS_IMETHODIMP
imgLoader::LoadImageWithChannelXPCOM(nsIChannel* channel,
                                     imgINotificationObserver* aObserver,
                                     nsISupports* aCX,
                                     nsIStreamListener** listener,
                                     imgIRequest** _retval)
{
    nsresult result;
    imgRequestProxy* proxy;
    result = LoadImageWithChannel(channel,
                                  aObserver,
                                  aCX,
                                  listener,
                                  &proxy);
    *_retval = proxy;
    return result;
}

nsresult
imgLoader::LoadImageWithChannel(nsIChannel* channel,
                                imgINotificationObserver* aObserver,
                                nsISupports* aCX,
                                nsIStreamListener** listener,
                                imgRequestProxy** _retval)
{
  NS_ASSERTION(channel,
               "imgLoader::LoadImageWithChannel -- NULL channel pointer");

  MOZ_ASSERT(NS_UsePrivateBrowsing(channel) == mRespectPrivacy);

  nsRefPtr<imgRequest> request;

  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));

  nsLoadFlags requestFlags = nsIRequest::LOAD_NORMAL;
  channel->GetLoadFlags(&requestFlags);

  nsRefPtr<imgCacheEntry> entry;

  if (requestFlags & nsIRequest::LOAD_BYPASS_CACHE) {
    RemoveFromCache(uri);
  } else {
    // Look in the cache for our URI, and then validate it.
    // XXX For now ignore aCacheKey. We will need it in the future
    // for correctly dealing with image load requests that are a result
    // of post data.
    imgCacheTable& cache = GetCache(uri);
    nsAutoCString spec;

    uri->GetSpec(spec);

    if (cache.Get(spec, getter_AddRefs(entry)) && entry) {
      // We don't want to kick off another network load. So we ask
      // ValidateEntry to only do validation without creating a new proxy. If
      // it says that the entry isn't valid any more, we'll only use the entry
      // we're getting if the channel is loading from the cache anyways.
      //
      // XXX -- should this be changed? it's pretty much verbatim from the old
      // code, but seems nonsensical.
      //
      // Since aCanMakeNewChannel == false, we don't need to pass content policy
      // type/principal/etc
      if (ValidateEntry(entry, uri, nullptr, nullptr, RP_Default,
                        nullptr, aObserver, aCX, requestFlags,
                        nsIContentPolicy::TYPE_INVALID, false, nullptr,
                        nullptr, imgIRequest::CORS_NONE)) {
        request = entry->GetRequest();
      } else {
        nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(channel));
        bool bUseCacheCopy;

        if (cacheChan) {
          cacheChan->IsFromCache(&bUseCacheCopy);
        } else {
          bUseCacheCopy = false;
        }

        if (!bUseCacheCopy) {
          entry = nullptr;
        } else {
          request = entry->GetRequest();
        }
      }

      if (request && entry) {
        // If this entry has no proxies, its request has no reference to
        // the entry.
        if (entry->HasNoProxies()) {
          LOG_FUNC_WITH_PARAM(GetImgLog(),
            "imgLoader::LoadImageWithChannel() adding proxyless entry",
            "uri", spec.get());
          MOZ_ASSERT(!request->HasCacheEntry(),
            "Proxyless entry's request has cache entry!");
          request->SetCacheEntry(entry);

          if (mCacheTracker) {
            mCacheTracker->MarkUsed(entry);
          }
        }
      }
    }
  }

  nsCOMPtr<nsILoadGroup> loadGroup;
  channel->GetLoadGroup(getter_AddRefs(loadGroup));

  // Filter out any load flags not from nsIRequest
  requestFlags &= nsIRequest::LOAD_REQUESTMASK;

  nsresult rv = NS_OK;
  if (request) {
    // we have this in our cache already.. cancel the current (document) load

    // this should fire an OnStopRequest
    channel->Cancel(NS_ERROR_PARSED_DATA_CACHED);

    *listener = nullptr; // give them back a null nsIStreamListener

    rv = CreateNewProxyForRequest(request, loadGroup, aObserver,
                                  requestFlags, _retval);
    static_cast<imgRequestProxy*>(*_retval)->NotifyListener();
  } else {
    // Default to doing a principal check because we don't know who
    // started that load and whether their principal ended up being
    // inherited on the channel.
    NewRequestAndEntry(true, this, getter_AddRefs(request),
                       getter_AddRefs(entry));

    // We use originalURI here to fulfil the imgIRequest contract on GetURI.
    nsCOMPtr<nsIURI> originalURI;
    channel->GetOriginalURI(getter_AddRefs(originalURI));

    // No principal specified here, because we're not passed one.
    // In LoadImageWithChannel, the redirects that may have been
    // assoicated with this load would have gone through necko.
    // We only have the final URI in ImageLib and hence don't know
    // if the request went through insecure redirects.  But if it did,
    // the necko cache should have handled that (since all necko cache hits
    // including the redirects will go through content policy).  Hence, we
    // can set aHadInsecureRedirect to false here.
    request->Init(originalURI, uri, /* aHadInsecureRedirect = */ false,
                  channel, channel, entry, aCX, nullptr,
                  imgIRequest::CORS_NONE, RP_Default);

    nsRefPtr<ProxyListener> pl =
      new ProxyListener(static_cast<nsIStreamListener*>(request.get()));
    pl.forget(listener);

    // Try to add the new request into the cache.
    PutIntoCache(originalURI, entry);

    rv = CreateNewProxyForRequest(request, loadGroup, aObserver,
                                  requestFlags, _retval);

    // Explicitly don't notify our proxy, because we're loading off the
    // network, and necko (or things called from necko, such as
    // imgCacheValidator) are going to call our notifications asynchronously,
    // and we can't make it further asynchronous because observers might rely
    // on imagelib completing its work between the channel's OnStartRequest and
    // OnStopRequest.
  }

  return rv;
}

bool
imgLoader::SupportImageWithMimeType(const char* aMimeType,
                                    AcceptedMimeTypes aAccept
                                      /* = AcceptedMimeTypes::IMAGES */)
{
  nsAutoCString mimeType(aMimeType);
  ToLowerCase(mimeType);

  if (aAccept == AcceptedMimeTypes::IMAGES_AND_DOCUMENTS &&
      mimeType.EqualsLiteral("image/svg+xml")) {
    return true;
  }

  return Image::GetDecoderType(mimeType.get()) != Image::eDecoderType_unknown;
}

NS_IMETHODIMP
imgLoader::GetMIMETypeFromContent(nsIRequest* aRequest,
                                  const uint8_t* aContents,
                                  uint32_t aLength,
                                  nsACString& aContentType)
{
  return GetMimeTypeFromContent((const char*)aContents, aLength, aContentType);
}

/* static */
nsresult
imgLoader::GetMimeTypeFromContent(const char* aContents,
                                  uint32_t aLength,
                                  nsACString& aContentType)
{
  /* Is it a GIF? */
  if (aLength >= 6 && (!nsCRT::strncmp(aContents, "GIF87a", 6) ||
                       !nsCRT::strncmp(aContents, "GIF89a", 6))) {
    aContentType.AssignLiteral(IMAGE_GIF);

  /* or a PNG? */
  } else if (aLength >= 8 && ((unsigned char)aContents[0]==0x89 &&
                              (unsigned char)aContents[1]==0x50 &&
                              (unsigned char)aContents[2]==0x4E &&
                              (unsigned char)aContents[3]==0x47 &&
                              (unsigned char)aContents[4]==0x0D &&
                              (unsigned char)aContents[5]==0x0A &&
                              (unsigned char)aContents[6]==0x1A &&
                              (unsigned char)aContents[7]==0x0A)) {
    aContentType.AssignLiteral(IMAGE_PNG);

  /* maybe a JPEG (JFIF)? */
  /* JFIF files start with SOI APP0 but older files can start with SOI DQT
   * so we test for SOI followed by any marker, i.e. FF D8 FF
   * this will also work for SPIFF JPEG files if they appear in the future.
   *
   * (JFIF is 0XFF 0XD8 0XFF 0XE0 <skip 2> 0X4A 0X46 0X49 0X46 0X00)
   */
  } else if (aLength >= 3 &&
             ((unsigned char)aContents[0])==0xFF &&
             ((unsigned char)aContents[1])==0xD8 &&
             ((unsigned char)aContents[2])==0xFF) {
    aContentType.AssignLiteral(IMAGE_JPEG);

  /* or how about ART? */
  /* ART begins with JG (4A 47). Major version offset 2.
   * Minor version offset 3. Offset 4 must be nullptr.
   */
  } else if (aLength >= 5 &&
             ((unsigned char) aContents[0])==0x4a &&
             ((unsigned char) aContents[1])==0x47 &&
             ((unsigned char) aContents[4])==0x00 ) {
    aContentType.AssignLiteral(IMAGE_ART);

  } else if (aLength >= 2 && !nsCRT::strncmp(aContents, "BM", 2)) {
    aContentType.AssignLiteral(IMAGE_BMP);

  // ICOs always begin with a 2-byte 0 followed by a 2-byte 1.
  // CURs begin with 2-byte 0 followed by 2-byte 2.
  } else if (aLength >= 4 && (!memcmp(aContents, "\000\000\001\000", 4) ||
                              !memcmp(aContents, "\000\000\002\000", 4))) {
    aContentType.AssignLiteral(IMAGE_ICO);

  } else {
    /* none of the above?  I give up */
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

/**
 * proxy stream listener class used to handle multipart/x-mixed-replace
 */

#include "nsIRequest.h"
#include "nsIStreamConverterService.h"

NS_IMPL_ISUPPORTS(ProxyListener,
                  nsIStreamListener,
                  nsIThreadRetargetableStreamListener,
                  nsIRequestObserver)

ProxyListener::ProxyListener(nsIStreamListener* dest) :
  mDestListener(dest)
{
  /* member initializers and constructor code */
}

ProxyListener::~ProxyListener()
{
  /* destructor code */
}


/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
ProxyListener::OnStartRequest(nsIRequest* aRequest, nsISupports* ctxt)
{
  if (!mDestListener) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel) {
    // We need to set the initiator type for the image load
    nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(channel);
    if (timedChannel) {
      nsAutoString type;
      timedChannel->GetInitiatorType(type);
      if (type.IsEmpty()) {
        timedChannel->SetInitiatorType(NS_LITERAL_STRING("img"));
      }
    }

    nsAutoCString contentType;
    nsresult rv = channel->GetContentType(contentType);

    if (!contentType.IsEmpty()) {
     /* If multipart/x-mixed-replace content, we'll insert a MIME decoder
        in the pipeline to handle the content and pass it along to our
        original listener.
      */
      if (NS_LITERAL_CSTRING("multipart/x-mixed-replace").Equals(contentType)) {

        nsCOMPtr<nsIStreamConverterService> convServ(
          do_GetService("@mozilla.org/streamConverters;1", &rv));
        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsIStreamListener> toListener(mDestListener);
          nsCOMPtr<nsIStreamListener> fromListener;

          rv = convServ->AsyncConvertData("multipart/x-mixed-replace",
                                          "*/*",
                                          toListener,
                                          nullptr,
                                          getter_AddRefs(fromListener));
          if (NS_SUCCEEDED(rv)) {
            mDestListener = fromListener;
          }
        }
      }
    }
  }

  return mDestListener->OnStartRequest(aRequest, ctxt);
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt,
                       in nsresult status); */
NS_IMETHODIMP
ProxyListener::OnStopRequest(nsIRequest* aRequest,
                             nsISupports* ctxt,
                             nsresult status)
{
  if (!mDestListener) {
    return NS_ERROR_FAILURE;
  }

  return mDestListener->OnStopRequest(aRequest, ctxt, status);
}

/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt,
                         in nsIInputStream inStr,
                         in unsigned long long sourceOffset,
                         in unsigned long count); */
NS_IMETHODIMP
ProxyListener::OnDataAvailable(nsIRequest* aRequest, nsISupports* ctxt,
                               nsIInputStream* inStr, uint64_t sourceOffset,
                               uint32_t count)
{
  if (!mDestListener) {
    return NS_ERROR_FAILURE;
  }

  return mDestListener->OnDataAvailable(aRequest, ctxt, inStr,
                                        sourceOffset, count);
}

/** nsThreadRetargetableStreamListener methods **/
NS_IMETHODIMP
ProxyListener::CheckListenerChain()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread!");
  nsresult rv = NS_OK;
  nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
    do_QueryInterface(mDestListener, &rv);
  if (retargetableListener) {
    rv = retargetableListener->CheckListenerChain();
  }
  PR_LOG(GetImgLog(), PR_LOG_DEBUG,
         ("ProxyListener::CheckListenerChain %s [this=%p listener=%p rv=%x]",
          (NS_SUCCEEDED(rv) ? "success" : "failure"),
          this, (nsIStreamListener*)mDestListener, rv));
  return rv;
}

/**
 * http validate class.  check a channel for a 304
 */

NS_IMPL_ISUPPORTS(imgCacheValidator, nsIStreamListener, nsIRequestObserver,
                  nsIThreadRetargetableStreamListener,
                  nsIChannelEventSink, nsIInterfaceRequestor,
                  nsIAsyncVerifyRedirectCallback)

imgCacheValidator::imgCacheValidator(nsProgressNotificationProxy* progress,
                                     imgLoader* loader, imgRequest* request,
                                     nsISupports* aContext,
                                     bool forcePrincipalCheckForCacheEntry)
 : mProgressProxy(progress),
   mRequest(request),
   mContext(aContext),
   mImgLoader(loader),
   mHadInsecureRedirect(false)
{
  NewRequestAndEntry(forcePrincipalCheckForCacheEntry, loader,
                     getter_AddRefs(mNewRequest),
                     getter_AddRefs(mNewEntry));
}

imgCacheValidator::~imgCacheValidator()
{
  if (mRequest) {
    mRequest->SetValidator(nullptr);
  }
}

void
imgCacheValidator::AddProxy(imgRequestProxy* aProxy)
{
  // aProxy needs to be in the loadgroup since we're validating from
  // the network.
  aProxy->AddToLoadGroup();

  mProxies.AppendObject(aProxy);
}

/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP
imgCacheValidator::OnStartRequest(nsIRequest* aRequest, nsISupports* ctxt)
{
  // We may be holding on to a document, so ensure that it's released.
  nsCOMPtr<nsISupports> context = mContext.forget();

  // If for some reason we don't still have an existing request (probably
  // because OnStartRequest got delivered more than once), just bail.
  if (!mRequest) {
    MOZ_ASSERT_UNREACHABLE("OnStartRequest delivered more than once?");
    aRequest->Cancel(NS_BINDING_ABORTED);
    return NS_ERROR_FAILURE;
  }

  // If this request is coming from cache and has the same URI as our
  // imgRequest, the request all our proxies are pointing at is valid, and all
  // we have to do is tell them to notify their listeners.
  nsCOMPtr<nsICachingChannel> cacheChan(do_QueryInterface(aRequest));
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (cacheChan && channel && !mRequest->CacheChanged(aRequest)) {
    bool isFromCache = false;
    cacheChan->IsFromCache(&isFromCache);

    nsCOMPtr<nsIURI> channelURI;
    channel->GetURI(getter_AddRefs(channelURI));

    nsCOMPtr<nsIURI> currentURI;
    mRequest->GetCurrentURI(getter_AddRefs(currentURI));

    bool sameURI = false;
    if (channelURI && currentURI) {
      channelURI->Equals(currentURI, &sameURI);
    }

    if (isFromCache && sameURI) {
      uint32_t count = mProxies.Count();
      for (int32_t i = count-1; i>=0; i--) {
        imgRequestProxy* proxy = static_cast<imgRequestProxy*>(mProxies[i]);

        // Proxies waiting on cache validation should be deferring
        // notifications. Undefer them.
        MOZ_ASSERT(proxy->NotificationsDeferred(),
                   "Proxies waiting on cache validation should be "
                   "deferring notifications!");
        proxy->SetNotificationsDeferred(false);

        // Notify synchronously, because we're already in OnStartRequest, an
        // asynchronously-called function.
        proxy->SyncNotifyListener();
      }

      // We don't need to load this any more.
      aRequest->Cancel(NS_BINDING_ABORTED);

      mRequest->SetLoadId(context);
      mRequest->SetValidator(nullptr);

      mRequest = nullptr;

      mNewRequest = nullptr;
      mNewEntry = nullptr;

      return NS_OK;
    }
  }

  // We can't load out of cache. We have to create a whole new request for the
  // data that's coming in off the channel.
  nsCOMPtr<nsIURI> uri;
  {
    nsRefPtr<ImageURL> imageURL;
    mRequest->GetURI(getter_AddRefs(imageURL));
    uri = imageURL->ToIURI();
  }

#if defined(PR_LOGGING)
  nsAutoCString spec;
  uri->GetSpec(spec);
  LOG_MSG_WITH_PARAM(GetImgLog(),
                     "imgCacheValidator::OnStartRequest creating new request",
                     "uri", spec.get());
#endif

  int32_t corsmode = mRequest->GetCORSMode();
  ReferrerPolicy refpol = mRequest->GetReferrerPolicy();
  nsCOMPtr<nsIPrincipal> loadingPrincipal = mRequest->GetLoadingPrincipal();

  // Doom the old request's cache entry
  mRequest->RemoveFromCache();

  mRequest->SetValidator(nullptr);
  mRequest = nullptr;

  // We use originalURI here to fulfil the imgIRequest contract on GetURI.
  nsCOMPtr<nsIURI> originalURI;
  channel->GetOriginalURI(getter_AddRefs(originalURI));
  mNewRequest->Init(originalURI, uri, mHadInsecureRedirect, aRequest, channel,
                    mNewEntry, context, loadingPrincipal, corsmode, refpol);

  mDestListener = new ProxyListener(mNewRequest);

  // Try to add the new request into the cache. Note that the entry must be in
  // the cache before the proxies' ownership changes, because adding a proxy
  // changes the caching behaviour for imgRequests.
  mImgLoader->PutIntoCache(originalURI, mNewEntry);

  uint32_t count = mProxies.Count();
  for (int32_t i = count-1; i>=0; i--) {
    imgRequestProxy* proxy = static_cast<imgRequestProxy*>(mProxies[i]);
    proxy->ChangeOwner(mNewRequest);

    // Notify synchronously, because we're already in OnStartRequest, an
    // asynchronously-called function.
    proxy->SetNotificationsDeferred(false);
    proxy->SyncNotifyListener();
  }

  mNewRequest = nullptr;
  mNewEntry = nullptr;

  return mDestListener->OnStartRequest(aRequest, ctxt);
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt,
                       in nsresult status); */
NS_IMETHODIMP
imgCacheValidator::OnStopRequest(nsIRequest* aRequest,
                                 nsISupports* ctxt,
                                 nsresult status)
{
  // Be sure we've released the document that we may have been holding on to.
  mContext = nullptr;

  if (!mDestListener) {
    return NS_OK;
  }

  return mDestListener->OnStopRequest(aRequest, ctxt, status);
}

/** nsIStreamListener methods **/


/* void
   onDataAvailable (in nsIRequest request, in nsISupports ctxt,
   in nsIInputStream inStr, in unsigned long long sourceOffset,
   in unsigned long count); */
NS_IMETHODIMP
imgCacheValidator::OnDataAvailable(nsIRequest* aRequest, nsISupports* ctxt,
                                   nsIInputStream* inStr,
                                   uint64_t sourceOffset, uint32_t count)
{
  if (!mDestListener) {
    // XXX see bug 113959
    uint32_t _retval;
    inStr->ReadSegments(NS_DiscardSegment, nullptr, count, &_retval);
    return NS_OK;
  }

  return mDestListener->OnDataAvailable(aRequest, ctxt, inStr, sourceOffset,
                                        count);
}

/** nsIThreadRetargetableStreamListener methods **/

NS_IMETHODIMP
imgCacheValidator::CheckListenerChain()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main thread!");
  nsresult rv = NS_OK;
  nsCOMPtr<nsIThreadRetargetableStreamListener> retargetableListener =
    do_QueryInterface(mDestListener, &rv);
  if (retargetableListener) {
    rv = retargetableListener->CheckListenerChain();
  }
  PR_LOG(GetImgLog(), PR_LOG_DEBUG,
         ("[this=%p] imgCacheValidator::CheckListenerChain -- rv %d=%s",
          this, NS_SUCCEEDED(rv) ? "succeeded" : "failed", rv));
  return rv;
}

/** nsIInterfaceRequestor methods **/

NS_IMETHODIMP
imgCacheValidator::GetInterface(const nsIID& aIID, void** aResult)
{
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    return QueryInterface(aIID, aResult);
  }

  return mProgressProxy->GetInterface(aIID, aResult);
}

// These functions are materially the same as the same functions in imgRequest.
// We duplicate them because we're verifying whether cache loads are necessary,
// not unconditionally loading.

/** nsIChannelEventSink methods **/
NS_IMETHODIMP
imgCacheValidator::
  AsyncOnChannelRedirect(nsIChannel* oldChannel,
                         nsIChannel* newChannel,
                         uint32_t flags,
                         nsIAsyncVerifyRedirectCallback* callback)
{
  // Note all cache information we get from the old channel.
  mNewRequest->SetCacheValidation(mNewEntry, oldChannel);

  // If the previous URI is a non-HTTPS URI, record that fact for later use by
  // security code, which needs to know whether there is an insecure load at any
  // point in the redirect chain.
  nsCOMPtr<nsIURI> oldURI;
  bool isHttps = false;
  bool isChrome = false;
  bool schemeLocal = false;
  if (NS_FAILED(oldChannel->GetURI(getter_AddRefs(oldURI))) ||
      NS_FAILED(oldURI->SchemeIs("https", &isHttps)) ||
      NS_FAILED(oldURI->SchemeIs("chrome", &isChrome)) ||
      NS_FAILED(NS_URIChainHasFlags(oldURI,
                                    nsIProtocolHandler::URI_IS_LOCAL_RESOURCE,
                                    &schemeLocal))  ||
      (!isHttps && !isChrome && !schemeLocal)) {
    mHadInsecureRedirect = true;
  }

  // Prepare for callback
  mRedirectCallback = callback;
  mRedirectChannel = newChannel;

  return mProgressProxy->AsyncOnChannelRedirect(oldChannel, newChannel, flags,
                                                this);
}

NS_IMETHODIMP
imgCacheValidator::OnRedirectVerifyCallback(nsresult aResult)
{
  // If we've already been told to abort, just do so.
  if (NS_FAILED(aResult)) {
      mRedirectCallback->OnRedirectVerifyCallback(aResult);
      mRedirectCallback = nullptr;
      mRedirectChannel = nullptr;
      return NS_OK;
  }

  // make sure we have a protocol that returns data rather than opens
  // an external application, e.g. mailto:
  nsCOMPtr<nsIURI> uri;
  mRedirectChannel->GetURI(getter_AddRefs(uri));
  bool doesNotReturnData = false;
  NS_URIChainHasFlags(uri, nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA,
                      &doesNotReturnData);

  nsresult result = NS_OK;

  if (doesNotReturnData) {
    result = NS_ERROR_ABORT;
  }

  mRedirectCallback->OnRedirectVerifyCallback(result);
  mRedirectCallback = nullptr;
  mRedirectChannel = nullptr;
  return NS_OK;
}
