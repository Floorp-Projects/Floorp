/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "imgRequest.h"
#include "ImageLogging.h"

/* We end up pulling in windows.h because we eventually hit gfxWindowsSurface;
 * windows.h defines LoadImage, so we have to #undef it or imgLoader::LoadImage
 * gets changed.
 * This #undef needs to be in multiple places because we don't always pull
 * headers in in the same order.
 */
#undef LoadImage

#include "imgLoader.h"
#include "imgRequestProxy.h"
#include "RasterImage.h"
#include "VectorImage.h"

#include "imgILoader.h"

#include "netCore.h"

#include "nsIChannel.h"
#include "nsICachingChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIMultiPartChannel.h"
#include "nsIHttpChannel.h"

#include "nsIComponentManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIScriptSecurityManager.h"

#include "nsICacheVisitor.h"

#include "nsString.h"
#include "nsXPIDLString.h"
#include "plstr.h" // PL_strcasestr(...)
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"

#include "mozilla/Preferences.h"

#include "DiscardTracker.h"
#include "nsAsyncRedirectVerifyHelper.h"

#define SVG_MIMETYPE "image/svg+xml"

using namespace mozilla;
using namespace mozilla::image;

static bool gInitializedPrefCaches = false;
static bool gDecodeOnDraw = false;
static bool gDiscardable = false;

static void
InitPrefCaches()
{
  Preferences::AddBoolVarCache(&gDiscardable, "image.mem.discardable");
  Preferences::AddBoolVarCache(&gDecodeOnDraw, "image.mem.decodeondraw");
  gInitializedPrefCaches = true;
}

#if defined(PR_LOGGING)
PRLogModuleInfo *gImgLog = PR_NewLogModule("imgRequest");
#endif

NS_IMPL_ISUPPORTS5(imgRequest,
                   nsIStreamListener, nsIRequestObserver,
                   nsIChannelEventSink,
                   nsIInterfaceRequestor,
                   nsIAsyncVerifyRedirectCallback)

imgRequest::imgRequest(imgLoader* aLoader)
 : mLoader(aLoader)
 , mStatusTracker(new imgStatusTracker(nullptr, this))
 , mValidator(nullptr)
 , mImageSniffers("image-sniffing-services")
 , mInnerWindowId(0)
 , mCORSMode(imgIRequest::CORS_NONE)
 , mDecodeRequested(false)
 , mIsMultiPartChannel(false)
 , mGotData(false)
 , mIsInCache(false)
 , mResniffMimeType(false)
{
  // Register our pref observers if we haven't yet.
  if (NS_UNLIKELY(!gInitializedPrefCaches)) {
    InitPrefCaches();
  }
}

imgRequest::~imgRequest()
{
  if (mURI) {
    nsAutoCString spec;
    mURI->GetSpec(spec);
    LOG_FUNC_WITH_PARAM(gImgLog, "imgRequest::~imgRequest()", "keyuri", spec.get());
  } else
    LOG_FUNC(gImgLog, "imgRequest::~imgRequest()");
}

nsresult imgRequest::Init(nsIURI *aURI,
                          nsIURI *aCurrentURI,
                          nsIRequest *aRequest,
                          nsIChannel *aChannel,
                          imgCacheEntry *aCacheEntry,
                          void *aLoadId,
                          nsIPrincipal* aLoadingPrincipal,
                          int32_t aCORSMode)
{
  LOG_FUNC(gImgLog, "imgRequest::Init");

  NS_ABORT_IF_FALSE(!mImage, "Multiple calls to init");
  NS_ABORT_IF_FALSE(aURI, "No uri");
  NS_ABORT_IF_FALSE(aCurrentURI, "No current uri");
  NS_ABORT_IF_FALSE(aRequest, "No request");
  NS_ABORT_IF_FALSE(aChannel, "No channel");

  mProperties = do_CreateInstance("@mozilla.org/properties;1");

  mURI = aURI;
  mCurrentURI = aCurrentURI;
  mRequest = aRequest;
  mChannel = aChannel;
  mTimedChannel = do_QueryInterface(mChannel);

  mLoadingPrincipal = aLoadingPrincipal;
  mCORSMode = aCORSMode;

  mChannel->GetNotificationCallbacks(getter_AddRefs(mPrevChannelSink));

  NS_ASSERTION(mPrevChannelSink != this,
               "Initializing with a channel that already calls back to us!");

  mChannel->SetNotificationCallbacks(this);

  mCacheEntry = aCacheEntry;

  SetLoadId(aLoadId);

  return NS_OK;
}

imgStatusTracker&
imgRequest::GetStatusTracker()
{
  if (mImage && mGotData) {
    NS_ABORT_IF_FALSE(!mStatusTracker,
                      "Should have given mStatusTracker to mImage");
    return mImage->GetStatusTracker();
  } else {
    NS_ABORT_IF_FALSE(mStatusTracker,
                      "Should have mStatusTracker until we create mImage");
    return *mStatusTracker;
  }
}

void imgRequest::SetCacheEntry(imgCacheEntry *entry)
{
  mCacheEntry = entry;
}

bool imgRequest::HasCacheEntry() const
{
  return mCacheEntry != nullptr;
}

void imgRequest::ResetCacheEntry()
{
  if (HasCacheEntry()) {
    mCacheEntry->SetDataSize(0);
  }
}

void imgRequest::AddProxy(imgRequestProxy *proxy)
{
  NS_PRECONDITION(proxy, "null imgRequestProxy passed in");
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::AddProxy", "proxy", proxy);

  // If we're empty before adding, we have to tell the loader we now have
  // proxies.
  if (GetStatusTracker().ConsumerCount() == 0) {
    NS_ABORT_IF_FALSE(mURI, "Trying to SetHasProxies without key uri.");
    mLoader->SetHasProxies(mURI);
  }

  GetStatusTracker().AddConsumer(proxy);
}

nsresult imgRequest::RemoveProxy(imgRequestProxy *proxy, nsresult aStatus)
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
  imgStatusTracker& statusTracker = GetStatusTracker();
  if (!statusTracker.RemoveConsumer(proxy, aStatus, !aNotify))
    return NS_OK;

  if (statusTracker.ConsumerCount() == 0) {
    // If we have no observers, there's nothing holding us alive. If we haven't
    // been cancelled and thus removed from the cache, tell the image loader so
    // we can be evicted from the cache.
    if (mCacheEntry) {
      NS_ABORT_IF_FALSE(mURI, "Removing last observer without key uri.");

      mLoader->SetHasNoProxies(mURI, mCacheEntry);
    } 
#if defined(PR_LOGGING)
    else {
      nsAutoCString spec;
      mURI->GetSpec(spec);
      LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::RemoveProxy no cache entry", "uri", spec.get());
    }
#endif

    /* If |aStatus| is a failure code, then cancel the load if it is still in progress.
       Otherwise, let the load continue, keeping 'this' in the cache with no observers.
       This way, if a proxy is destroyed without calling cancel on it, it won't leak
       and won't leave a bad pointer in the observer list.
     */
    if (statusTracker.IsLoading() && NS_FAILED(aStatus)) {
      LOG_MSG(gImgLog, "imgRequest::RemoveProxy", "load in progress.  canceling");

      this->Cancel(NS_BINDING_ABORTED);
    }

    /* break the cycle from the cache entry. */
    mCacheEntry = nullptr;
  }

  // If a proxy is removed for a reason other than its owner being
  // changed, remove the proxy from the loadgroup.
  if (aStatus != NS_IMAGELIB_CHANGING_OWNER)
    proxy->RemoveFromLoadGroup(true);

  return NS_OK;
}

void imgRequest::CancelAndAbort(nsresult aStatus)
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

void imgRequest::Cancel(nsresult aStatus)
{
  /* The Cancel() method here should only be called by this class. */

  LOG_SCOPE(gImgLog, "imgRequest::Cancel");

  imgStatusTracker& statusTracker = GetStatusTracker();

  statusTracker.MaybeUnblockOnload();

  statusTracker.RecordCancel();

  RemoveFromCache();

  if (mRequest && statusTracker.IsLoading())
    mRequest->Cancel(aStatus);
}

nsresult imgRequest::GetURI(nsIURI **aURI)
{
  LOG_FUNC(gImgLog, "imgRequest::GetURI");

  if (mURI) {
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult imgRequest::GetSecurityInfo(nsISupports **aSecurityInfo)
{
  LOG_FUNC(gImgLog, "imgRequest::GetSecurityInfo");

  // Missing security info means this is not a security load
  // i.e. it is not an error when security info is missing
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  return NS_OK;
}

void imgRequest::RemoveFromCache()
{
  LOG_SCOPE(gImgLog, "imgRequest::RemoveFromCache");

  if (mIsInCache) {
    // mCacheEntry is nulled out when we have no more observers.
    if (mCacheEntry)
      mLoader->RemoveFromCache(mCacheEntry);
    else
      mLoader->RemoveFromCache(mURI);
  }

  mCacheEntry = nullptr;
}

int32_t imgRequest::Priority() const
{
  int32_t priority = nsISupportsPriority::PRIORITY_NORMAL;
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mRequest);
  if (p)
    p->GetPriority(&priority);
  return priority;
}

void imgRequest::AdjustPriority(imgRequestProxy *proxy, int32_t delta)
{
  // only the first proxy is allowed to modify the priority of this image load.
  //
  // XXX(darin): this is probably not the most optimal algorithm as we may want
  // to increase the priority of requests that have a lot of proxies.  the key
  // concern though is that image loads remain lower priority than other pieces
  // of content such as link clicks, CSS, and JS.
  //
  if (!GetStatusTracker().FirstConsumerIs(proxy))
    return;

  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mRequest);
  if (p)
    p->AdjustPriority(delta);
}

void imgRequest::SetIsInCache(bool incache)
{
  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequest::SetIsCacheable", "incache", incache);
  mIsInCache = incache;
}

void imgRequest::UpdateCacheEntrySize()
{
  if (mCacheEntry) {
    mCacheEntry->SetDataSize(mImage->SizeOfData());

#ifdef DEBUG_joe
    nsAutoCString url;
    mURI->GetSpec(url);
    printf("CACHEPUT: %d %s %d\n", time(NULL), url.get(), imageSize);
#endif
  }
}

void imgRequest::SetCacheValidation(imgCacheEntry* aCacheEntry, nsIRequest* aRequest)
{
  /* get the expires info */
  if (aCacheEntry) {
    nsCOMPtr<nsICachingChannel> cacheChannel(do_QueryInterface(aRequest));
    if (cacheChannel) {
      nsCOMPtr<nsISupports> cacheToken;
      cacheChannel->GetCacheToken(getter_AddRefs(cacheToken));
      if (cacheToken) {
        nsCOMPtr<nsICacheEntryInfo> entryDesc(do_QueryInterface(cacheToken));
        if (entryDesc) {
          uint32_t expiration;
          /* get the expiration time from the caching channel's token */
          entryDesc->GetExpirationTime(&expiration);

          // Expiration time defaults to 0. We set the expiration time on our
          // entry if it hasn't been set yet.
          if (aCacheEntry->GetExpiryTime() == 0)
            aCacheEntry->SetExpiryTime(expiration);
        }
      }
    }

    // Determine whether the cache entry must be revalidated when we try to use it.
    // Currently, only HTTP specifies this information...
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
      if (bMustRevalidate)
        aCacheEntry->SetMustValidate(bMustRevalidate);
    }

    // We always need to validate file URIs.
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    if (channel) {
      nsCOMPtr<nsIURI> uri;
      channel->GetURI(getter_AddRefs(uri));
      bool isfile = false;
      uri->SchemeIs("file", &isfile);
      if (isfile)
        aCacheEntry->SetMustValidate(isfile);
    }
  }
}

nsresult
imgRequest::LockImage()
{
  return mImage->LockImage();
}

nsresult
imgRequest::UnlockImage()
{
  return mImage->UnlockImage();
}

nsresult
imgRequest::RequestDecode()
{
  // If we've initialized our image, we can request a decode.
  if (mImage) {
    return mImage->RequestDecode();
  }

  // Otherwise, flag to do it when we get the image
  mDecodeRequested = true;

  return NS_OK;
}

nsresult
imgRequest::StartDecoding()
{
  // If we've initialized our image, we can request a decode.
  if (mImage) {
    return mImage->StartDecoding();
  }

  // Otherwise, flag to do it when we get the image
  mDecodeRequested = true;

  return NS_OK;
}

/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequest::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartRequest");

  // Figure out if we're multipart
  nsCOMPtr<nsIMultiPartChannel> mpchan(do_QueryInterface(aRequest));
  if (mpchan)
      mIsMultiPartChannel = true;

  // If we're not multipart, we shouldn't have an image yet
  NS_ABORT_IF_FALSE(mIsMultiPartChannel || !mImage,
                    "Already have an image for non-multipart request");

  // If we're multipart and about to load another image, signal so we can
  // detect the mime type in OnDataAvailable.
  if (mIsMultiPartChannel && mImage) {
    mResniffMimeType = true;
    if (mImage->GetType() == imgIContainer::TYPE_RASTER) {
        // Tell the RasterImage to reinitialize itself. We have to do this in
        // OnStartRequest so that its state machine is always in a consistent
        // state.
        // Note that if our MIME type changes, mImage will be replaced with a
        // new object.
        static_cast<RasterImage*>(mImage.get())->NewSourceData();
      }
  }

  /*
   * If mRequest is null here, then we need to set it so that we'll be able to
   * cancel it if our Cancel() method is called.  Note that this can only
   * happen for multipart channels.  We could simply not null out mRequest for
   * non-last parts, if GetIsLastPart() were reliable, but it's not.  See
   * https://bugzilla.mozilla.org/show_bug.cgi?id=339610
   */
  if (!mRequest) {
    NS_ASSERTION(mpchan,
                 "We should have an mRequest here unless we're multipart");
    nsCOMPtr<nsIChannel> chan;
    mpchan->GetBaseChannel(getter_AddRefs(chan));
    mRequest = chan;
  }

  GetStatusTracker().OnStartRequest();

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel)
    channel->GetSecurityInfo(getter_AddRefs(mSecurityInfo));

  /* Get our principal */
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  if (chan) {
    nsCOMPtr<nsIScriptSecurityManager> secMan =
      do_GetService("@mozilla.org/scriptsecuritymanager;1");
    if (secMan) {
      nsresult rv = secMan->GetChannelPrincipal(chan,
                                                getter_AddRefs(mPrincipal));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  SetCacheValidation(mCacheEntry, aRequest);

  // Shouldn't we be dead already if this gets hit?  Probably multipart/x-mixed-replace...
  if (GetStatusTracker().ConsumerCount() == 0) {
    this->Cancel(NS_IMAGELIB_ERROR_FAILURE);
  }

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status); */
NS_IMETHODIMP imgRequest::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status)
{
  LOG_FUNC(gImgLog, "imgRequest::OnStopRequest");

  bool lastPart = true;
  nsCOMPtr<nsIMultiPartChannel> mpchan(do_QueryInterface(aRequest));
  if (mpchan)
    mpchan->GetIsLastPart(&lastPart);

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

  // Tell the image that it has all of the source data. Note that this can
  // trigger a failure, since the image might be waiting for more non-optional
  // data and this is the point where we break the news that it's not coming.
  if (mImage) {
    nsresult rv;
    if (mImage->GetType() == imgIContainer::TYPE_RASTER) {
      // Notify the image
      rv = static_cast<RasterImage*>(mImage.get())->SourceDataComplete();
    } else { // imageType == imgIContainer::TYPE_VECTOR
      nsCOMPtr<nsIStreamListener> imageAsStream = do_QueryInterface(mImage);
      NS_ABORT_IF_FALSE(imageAsStream,
                        "SVG-typed Image failed QI to nsIStreamListener");
      rv = imageAsStream->OnStopRequest(aRequest, ctxt, status);
    }

    // If we got an error in the SourceDataComplete() / OnStopRequest() call,
    // we don't want to proceed as if nothing bad happened. However, we also
    // want to give precedence to failure status codes from necko, since
    // presumably they're more meaningful.
    if (NS_FAILED(rv) && NS_SUCCEEDED(status))
      status = rv;
  }

  imgStatusTracker& statusTracker = GetStatusTracker();
  statusTracker.RecordStopRequest(lastPart, status);

  // If the request went through, update the cache entry size. Otherwise,
  // cancel the request, which removes us from the cache.
  if (mImage && NS_SUCCEEDED(status)) {
    // We update the cache entry size here because this is where we finish
    // loading compressed source data, which is part of our size calculus.
    UpdateCacheEntrySize();
  }
  else {
    // stops animations, removes from cache
    this->Cancel(status);
  }

  GetStatusTracker().OnStopRequest(lastPart, status);

  mTimedChannel = nullptr;
  return NS_OK;
}

struct mimetype_closure
{
  imgRequest* request;
  nsACString* newType;
};

/* prototype for these defined below */
static NS_METHOD sniff_mimetype_callback(nsIInputStream* in, void* closure, const char* fromRawSegment,
                                         uint32_t toOffset, uint32_t count, uint32_t *writeCount);

/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long long sourceOffset, in unsigned long count); */
NS_IMETHODIMP
imgRequest::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt,
                            nsIInputStream *inStr, uint64_t sourceOffset,
                            uint32_t count)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::OnDataAvailable", "count", count);

  NS_ASSERTION(aRequest, "imgRequest::OnDataAvailable -- no request!");

  nsresult rv;

  if (!mGotData || mResniffMimeType) {
    LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable |First time through... finding mimetype|");

    mGotData = true;

    mimetype_closure closure;
    nsAutoCString newType;
    closure.request = this;
    closure.newType = &newType;

    /* look at the first few bytes and see if we can tell what the data is from that
     * since servers tend to lie. :(
     */
    uint32_t out;
    inStr->ReadSegments(sniff_mimetype_callback, &closure, count, &out);

#ifdef DEBUG
    /* NS_WARNING if the content type from the channel isn't the same if the sniffing */
#endif

    nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
    if (newType.IsEmpty()) {
      LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable |sniffing of mimetype failed|");

      rv = NS_ERROR_FAILURE;
      if (chan) {
        rv = chan->GetContentType(newType);
      }

      if (NS_FAILED(rv)) {
        PR_LOG(gImgLog, PR_LOG_ERROR,
               ("[this=%p] imgRequest::OnDataAvailable -- Content type unavailable from the channel\n",
                this));

        this->Cancel(NS_IMAGELIB_ERROR_FAILURE);

        return NS_BINDING_ABORTED;
      }

      LOG_MSG(gImgLog, "imgRequest::OnDataAvailable", "Got content type from the channel");
    }

    // If we're a regular image and this is the first call to OnDataAvailable,
    // this will always be true. If we've resniffed our MIME type (i.e. we're a
    // multipart/x-mixed-replace image), we have to be able to switch our image
    // type and decoder.
    // We always reinitialize for SVGs, because they have no way of
    // reinitializing themselves.
    if (mContentType != newType || newType.EqualsLiteral(SVG_MIMETYPE)) {
      mContentType = newType;

      // If we've resniffed our MIME type and it changed, we need to create a
      // new status tracker to give to the image, because we don't have one of
      // our own any more.
      if (mResniffMimeType) {
        NS_ABORT_IF_FALSE(mIsMultiPartChannel, "Resniffing a non-multipart image");
        imgStatusTracker* freshTracker = new imgStatusTracker(nullptr, this);
        freshTracker->AdoptConsumers(&GetStatusTracker());
        mStatusTracker = freshTracker;
      }

      mResniffMimeType = false;

      /* now we have mimetype, so we can infer the image type that we want */
      if (mContentType.EqualsLiteral(SVG_MIMETYPE)) {
        mImage = new VectorImage(mStatusTracker.forget());
      } else {
        mImage = new RasterImage(mStatusTracker.forget());
      }
      mImage->SetInnerWindowID(mInnerWindowId);

      GetStatusTracker().OnDataAvailable();

      /* set our mimetype as a property */
      nsCOMPtr<nsISupportsCString> contentType(do_CreateInstance("@mozilla.org/supports-cstring;1"));
      if (contentType) {
        contentType->SetData(mContentType);
        mProperties->Set("type", contentType);
      }

      /* set our content disposition as a property */
      nsAutoCString disposition;
      if (chan) {
        chan->GetContentDispositionHeader(disposition);
      }
      if (!disposition.IsEmpty()) {
        nsCOMPtr<nsISupportsCString> contentDisposition(do_CreateInstance("@mozilla.org/supports-cstring;1"));
        if (contentDisposition) {
          contentDisposition->SetData(disposition);
          mProperties->Set("content-disposition", contentDisposition);
        }
      }

      LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::OnDataAvailable", "content type", mContentType.get());

      //
      // Figure out our Image initialization flags
      //

      // We default to the static globals
      bool isDiscardable = gDiscardable;
      bool doDecodeOnDraw = gDecodeOnDraw;

      // We want UI to be as snappy as possible and not to flicker. Disable discarding
      // and decode-on-draw for chrome URLS
      bool isChrome = false;
      rv = mURI->SchemeIs("chrome", &isChrome);
      if (NS_SUCCEEDED(rv) && isChrome)
        isDiscardable = doDecodeOnDraw = false;

      // We don't want resources like the "loading" icon to be discardable or
      // decode-on-draw either.
      bool isResource = false;
      rv = mURI->SchemeIs("resource", &isResource);
      if (NS_SUCCEEDED(rv) && isResource)
        isDiscardable = doDecodeOnDraw = false;

      // For multipart/x-mixed-replace, we basically want a direct channel to the
      // decoder. Disable both for this case as well.
      if (mIsMultiPartChannel)
        isDiscardable = doDecodeOnDraw = false;

      // We have all the information we need
      uint32_t imageFlags = Image::INIT_FLAG_NONE;
      if (isDiscardable)
        imageFlags |= Image::INIT_FLAG_DISCARDABLE;
      if (doDecodeOnDraw)
        imageFlags |= Image::INIT_FLAG_DECODE_ON_DRAW;
      if (mIsMultiPartChannel)
        imageFlags |= Image::INIT_FLAG_MULTIPART;

      // Get our URI string
      nsAutoCString uriString;
      rv = mURI->GetSpec(uriString);
      if (NS_FAILED(rv))
        uriString.Assign("<unknown image URI>");

      // Initialize the image that we created above. For RasterImages, this
      // instantiates a decoder behind the scenes, so if we don't have a decoder
      // for this mimetype we'll find out about it here.
      rv = mImage->Init(GetStatusTracker().GetDecoderObserver(),
                        mContentType.get(), uriString.get(), imageFlags);

      // We allow multipart images to fail to initialize without cancelling the
      // load because subsequent images might be fine.
      if (NS_FAILED(rv) && !mIsMultiPartChannel) { // Probably bad mimetype

        this->Cancel(rv);
        return NS_BINDING_ABORTED;
      }

      if (mImage->GetType() == imgIContainer::TYPE_RASTER) {
        /* Use content-length as a size hint for http channels. */
        nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
        if (httpChannel) {
          nsAutoCString contentLength;
          rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("content-length"),
                                              contentLength);
          if (NS_SUCCEEDED(rv)) {
            int32_t len = contentLength.ToInteger(&rv);

            // Pass anything usable on so that the RasterImage can preallocate
            // its source buffer
            if (len > 0) {
              uint32_t sizeHint = (uint32_t) len;
              sizeHint = NS_MIN<uint32_t>(sizeHint, 20000000); /* Bound by something reasonable */
              RasterImage* rasterImage = static_cast<RasterImage*>(mImage.get());
              rv = rasterImage->SetSourceSizeHint(sizeHint);
              if (NS_FAILED(rv)) {
                // Flush memory, try to get some back, and try again
                rv = nsMemory::HeapMinimize(true);
                nsresult rv2 = rasterImage->SetSourceSizeHint(sizeHint);
                // If we've still failed at this point, things are going downhill
                if (NS_FAILED(rv) || NS_FAILED(rv2)) {
                  NS_WARNING("About to hit OOM in imagelib!");
                }
              }
            }
          }
        }
      }

      if (mImage->GetType() == imgIContainer::TYPE_RASTER) {
        // If we were waiting on the image to do something, now's our chance.
        if (mDecodeRequested) {
          mImage->StartDecoding();
        }
      } else { // mImage->GetType() == imgIContainer::TYPE_VECTOR
        nsCOMPtr<nsIStreamListener> imageAsStream = do_QueryInterface(mImage);
        NS_ABORT_IF_FALSE(imageAsStream,
                          "SVG-typed Image failed QI to nsIStreamListener");
        imageAsStream->OnStartRequest(aRequest, nullptr);
      }
    }
  }

  if (mImage->GetType() == imgIContainer::TYPE_RASTER) {
    // WriteToRasterImage always consumes everything it gets
    // if it doesn't run out of memory
    uint32_t bytesRead;
    rv = inStr->ReadSegments(RasterImage::WriteToRasterImage,
                             static_cast<void*>(mImage),
                             count, &bytesRead);
    NS_ABORT_IF_FALSE(bytesRead == count || mImage->HasError(),
  "WriteToRasterImage should consume everything or the image must be in error!");
  } else { // mImage->GetType() == imgIContainer::TYPE_VECTOR
    nsCOMPtr<nsIStreamListener> imageAsStream = do_QueryInterface(mImage);
    rv = imageAsStream->OnDataAvailable(aRequest, ctxt, inStr,
                                        sourceOffset, count);
  }
  if (NS_FAILED(rv)) {
    PR_LOG(gImgLog, PR_LOG_WARNING,
           ("[this=%p] imgRequest::OnDataAvailable -- "
            "copy to RasterImage failed\n", this));
    this->Cancel(NS_IMAGELIB_ERROR_FAILURE);
    return NS_BINDING_ABORTED;
  }

  return NS_OK;
}

static NS_METHOD sniff_mimetype_callback(nsIInputStream* in,
                                         void* data,
                                         const char* fromRawSegment,
                                         uint32_t toOffset,
                                         uint32_t count,
                                         uint32_t *writeCount)
{
  mimetype_closure* closure = static_cast<mimetype_closure*>(data);

  NS_ASSERTION(closure, "closure is null!");

  if (count > 0)
    closure->request->SniffMimeType(fromRawSegment, count, *closure->newType);

  *writeCount = 0;
  return NS_ERROR_FAILURE;
}

void
imgRequest::SniffMimeType(const char *buf, uint32_t len, nsACString& newType)
{
  imgLoader::GetMimeTypeFromContent(buf, len, newType);

  // The vast majority of the time, imgLoader will find a gif/jpeg/png image
  // and fill newType with the sniffed MIME type.
  if (!newType.IsEmpty())
    return;

  // When our sniffing fails, we want to query registered image decoders
  // to see if they can identify the image. If we always trusted the server
  // to send the right MIME, images sent as text/plain would not be rendered.
  const nsCOMArray<nsIContentSniffer>& sniffers = mImageSniffers.GetEntries();
  uint32_t length = sniffers.Count();
  for (uint32_t i = 0; i < length; ++i) {
    nsresult rv =
      sniffers[i]->GetMIMETypeFromContent(nullptr, (const uint8_t *) buf, len, newType);
    if (NS_SUCCEEDED(rv) && !newType.IsEmpty()) {
      return;
    }
  }
}


/** nsIInterfaceRequestor methods **/

NS_IMETHODIMP
imgRequest::GetInterface(const nsIID & aIID, void **aResult)
{
  if (!mPrevChannelSink || aIID.Equals(NS_GET_IID(nsIChannelEventSink)))
    return QueryInterface(aIID, aResult);

  NS_ASSERTION(mPrevChannelSink != this, 
               "Infinite recursion - don't keep track of channel sinks that are us!");
  return mPrevChannelSink->GetInterface(aIID, aResult);
}

/** nsIChannelEventSink methods **/
NS_IMETHODIMP
imgRequest::AsyncOnChannelRedirect(nsIChannel *oldChannel,
                                   nsIChannel *newChannel, uint32_t flags,
                                   nsIAsyncVerifyRedirectCallback *callback)
{
  NS_ASSERTION(mRequest && mChannel, "Got a channel redirect after we nulled out mRequest!");
  NS_ASSERTION(mChannel == oldChannel, "Got a channel redirect for an unknown channel!");
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

#if defined(PR_LOGGING)
  nsAutoCString oldspec;
  if (mCurrentURI)
    mCurrentURI->GetSpec(oldspec);
  LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::OnChannelRedirect", "old", oldspec.get());
#endif

  // make sure we have a protocol that returns data rather than opens
  // an external application, e.g. mailto:
  mChannel->GetURI(getter_AddRefs(mCurrentURI));
  bool doesNotReturnData = false;
  nsresult rv =
    NS_URIChainHasFlags(mCurrentURI, nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA,
                        &doesNotReturnData);

  if (NS_SUCCEEDED(rv) && doesNotReturnData)
    rv = NS_ERROR_ABORT;

  if (NS_FAILED(rv)) {
    mRedirectCallback->OnRedirectVerifyCallback(rv);
    mRedirectCallback = nullptr;
    return NS_OK;
  }

  mRedirectCallback->OnRedirectVerifyCallback(NS_OK);
  mRedirectCallback = nullptr;
  return NS_OK;
}
