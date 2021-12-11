/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* code for loading in @font-face defined font data */

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"

#include "nsFontFaceLoader.h"

#include "nsError.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_layout.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "FontFaceSet.h"
#include "nsPresContext.h"
#include "nsIHttpChannel.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsContentPolicyUtils.h"
#include "nsNetCID.h"

#include "mozilla/gfx/2D.h"

using namespace mozilla;
using namespace mozilla::dom;

#define LOG(args) \
  MOZ_LOG(gfxUserFontSet::GetUserFontsLog(), mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() \
  MOZ_LOG_TEST(gfxUserFontSet::GetUserFontsLog(), LogLevel::Debug)

static uint32_t GetFallbackDelay() {
  return Preferences::GetInt("gfx.downloadable_fonts.fallback_delay", 3000);
}

static uint32_t GetShortFallbackDelay() {
  return Preferences::GetInt("gfx.downloadable_fonts.fallback_delay_short",
                             100);
}

nsFontFaceLoader::nsFontFaceLoader(gfxUserFontEntry* aUserFontEntry,
                                   uint32_t aSrcIndex,
                                   FontFaceSet* aFontFaceSet,
                                   nsIChannel* aChannel)
    : mUserFontEntry(aUserFontEntry),
      mFontFaceSet(aFontFaceSet),
      mChannel(aChannel),
      mStreamLoader(nullptr),
      mSrcIndex(aSrcIndex) {
  MOZ_ASSERT(mFontFaceSet,
             "We should get a valid FontFaceSet from the caller!");

  const gfxFontFaceSrc& src = aUserFontEntry->SourceAt(mSrcIndex);
  MOZ_ASSERT(src.mSourceType == gfxFontFaceSrc::eSourceType_URL);

  mFontURI = src.mURI->get();
  mStartTime = TimeStamp::Now();

  // We add an explicit load block rather than just rely on the network
  // request's block, since we need to do some OMT work after the load
  // is finished before we unblock load.
  mFontFaceSet->Document()->BlockOnload();
}

nsFontFaceLoader::~nsFontFaceLoader() {
  MOZ_DIAGNOSTIC_ASSERT(!mInLoadTimerCallback);
  MOZ_DIAGNOSTIC_ASSERT(!mInStreamComplete);
  if (mUserFontEntry) {
    mUserFontEntry->mLoader = nullptr;
  }
  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }
  if (mFontFaceSet) {
    mFontFaceSet->RemoveLoader(this);
    mFontFaceSet->Document()->UnblockOnload(false);
  }
}

void nsFontFaceLoader::StartedLoading(nsIStreamLoader* aStreamLoader) {
  int32_t loadTimeout;
  StyleFontDisplay fontDisplay = GetFontDisplay();
  if (fontDisplay == StyleFontDisplay::Auto ||
      fontDisplay == StyleFontDisplay::Block) {
    loadTimeout = GetFallbackDelay();
  } else {
    loadTimeout = GetShortFallbackDelay();
  }

  if (loadTimeout > 0) {
    NS_NewTimerWithFuncCallback(
        getter_AddRefs(mLoadTimer), LoadTimerCallback, static_cast<void*>(this),
        loadTimeout, nsITimer::TYPE_ONE_SHOT, "LoadTimerCallback",
        mFontFaceSet->Document()->EventTargetFor(TaskCategory::Other));
  } else {
    mUserFontEntry->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
  }
  mStreamLoader = aStreamLoader;
}

/* static */
void nsFontFaceLoader::LoadTimerCallback(nsITimer* aTimer, void* aClosure) {
  nsFontFaceLoader* loader = static_cast<nsFontFaceLoader*>(aClosure);

  MOZ_DIAGNOSTIC_ASSERT(!loader->mInLoadTimerCallback);
  MOZ_DIAGNOSTIC_ASSERT(!loader->mInStreamComplete);
  AutoRestore<bool> scope{loader->mInLoadTimerCallback};
  loader->mInLoadTimerCallback = true;

  if (!loader->mFontFaceSet) {
    // We've been canceled
    return;
  }

  gfxUserFontEntry* ufe = loader->mUserFontEntry.get();
  StyleFontDisplay fontDisplay = loader->GetFontDisplay();

  // Depending upon the value of the font-display descriptor for the font,
  // their may be one or two timeouts associated with each font. The
  // LOADING_SLOWLY state indicates that the fallback font is shown. The
  // LOADING_TIMED_OUT state indicates that the fallback font is shown *and* the
  // downloaded font resource will not replace the fallback font when the load
  // completes.

  bool updateUserFontSet = true;
  switch (fontDisplay) {
    case StyleFontDisplay::Auto:
    case StyleFontDisplay::Block:
      // If the entry is loading, check whether it's >75% done; if so,
      // we allow another timeout period before showing a fallback font.
      if (ufe->mFontDataLoadingState == gfxUserFontEntry::LOADING_STARTED) {
        int64_t contentLength;
        uint32_t numBytesRead;
        if (NS_SUCCEEDED(loader->mChannel->GetContentLength(&contentLength)) &&
            contentLength > 0 && contentLength < UINT32_MAX &&
            NS_SUCCEEDED(
                loader->mStreamLoader->GetNumBytesRead(&numBytesRead)) &&
            numBytesRead > 3 * (uint32_t(contentLength) >> 2)) {
          // More than 3/4 the data has been downloaded, so allow 50% extra
          // time and hope the remainder will arrive before the additional
          // time expires.
          ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_ALMOST_DONE;
          uint32_t delay;
          loader->mLoadTimer->GetDelay(&delay);
          loader->mLoadTimer->InitWithNamedFuncCallback(
              LoadTimerCallback, static_cast<void*>(loader), delay >> 1,
              nsITimer::TYPE_ONE_SHOT, "nsFontFaceLoader::LoadTimerCallback");
          updateUserFontSet = false;
          LOG(("userfonts (%p) 75%% done, resetting timer\n", loader));
        }
      }
      if (updateUserFontSet) {
        ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
      }
      break;
    case StyleFontDisplay::Swap:
      ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
      break;
    case StyleFontDisplay::Fallback: {
      if (ufe->mFontDataLoadingState == gfxUserFontEntry::LOADING_STARTED) {
        ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
      } else {
        ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_TIMED_OUT;
        updateUserFontSet = false;
      }
      break;
    }
    case StyleFontDisplay::Optional:
      ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_TIMED_OUT;
      break;

    default:
      MOZ_ASSERT_UNREACHABLE("strange font-display value");
      break;
  }

  // If the font is not 75% loaded, or if we've already timed out once
  // before, we mark this entry as "loading slowly", so the fallback
  // font will be used in the meantime, and tell the context to refresh.
  if (updateUserFontSet) {
    nsTArray<gfxUserFontSet*> fontSets;
    ufe->GetUserFontSets(fontSets);
    for (gfxUserFontSet* fontSet : fontSets) {
      nsPresContext* ctx = FontFaceSet::GetPresContextFor(fontSet);
      if (ctx) {
        fontSet->IncrementGeneration();
        ctx->UserFontSetUpdated(ufe);
        LOG(("userfonts (%p) timeout reflow for pres context %p display %d\n",
             loader, ctx, static_cast<int>(fontDisplay)));
      }
    }
  }
}

NS_IMPL_ISUPPORTS(nsFontFaceLoader, nsIStreamLoaderObserver, nsIRequestObserver)

// nsIStreamLoaderObserver
NS_IMETHODIMP
nsFontFaceLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                   nsISupports* aContext, nsresult aStatus,
                                   uint32_t aStringLen,
                                   const uint8_t* aString) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!mInLoadTimerCallback);
  MOZ_DIAGNOSTIC_ASSERT(!mInStreamComplete);

  AutoRestore<bool> scope{mInStreamComplete};
  mInStreamComplete = true;

  DropChannel();

  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }

  if (!mFontFaceSet) {
    // We've been canceled
    return aStatus;
  }

  TimeStamp doneTime = TimeStamp::Now();
  TimeDuration downloadTime = doneTime - mStartTime;
  uint32_t downloadTimeMS = uint32_t(downloadTime.ToMilliseconds());
  Telemetry::Accumulate(Telemetry::WEBFONT_DOWNLOAD_TIME, downloadTimeMS);

  if (GetFontDisplay() == StyleFontDisplay::Fallback) {
    uint32_t loadTimeout = GetFallbackDelay();
    if (downloadTimeMS > loadTimeout &&
        (mUserFontEntry->mFontDataLoadingState ==
         gfxUserFontEntry::LOADING_SLOWLY)) {
      mUserFontEntry->mFontDataLoadingState =
          gfxUserFontEntry::LOADING_TIMED_OUT;
    }
  }

  if (LOG_ENABLED()) {
    if (NS_SUCCEEDED(aStatus)) {
      LOG(("userfonts (%p) download completed - font uri: (%s) time: %d ms\n",
           this, mFontURI->GetSpecOrDefault().get(), downloadTimeMS));
    } else {
      LOG(("userfonts (%p) download failed - font uri: (%s) error: %8.8" PRIx32
           "\n",
           this, mFontURI->GetSpecOrDefault().get(),
           static_cast<uint32_t>(aStatus)));
    }
  }

  if (NS_SUCCEEDED(aStatus)) {
    // for HTTP requests, check whether the request _actually_ succeeded;
    // the "request status" in aStatus does not necessarily indicate this,
    // because HTTP responses such as 404 (Not Found) will still result in
    // a success code and potentially an HTML error page from the server
    // as the resulting data. We don't want to use that as a font.
    nsCOMPtr<nsIRequest> request;
    nsCOMPtr<nsIHttpChannel> httpChannel;
    aLoader->GetRequest(getter_AddRefs(request));
    httpChannel = do_QueryInterface(request);
    if (httpChannel) {
      bool succeeded;
      nsresult rv = httpChannel->GetRequestSucceeded(&succeeded);
      if (NS_SUCCEEDED(rv) && !succeeded) {
        aStatus = NS_ERROR_NOT_AVAILABLE;
      }
    }
  }

  mFontFaceSet->GetUserFontSet()->RecordFontLoadDone(aStringLen, doneTime);

  // The userFontEntry is responsible for freeing the downloaded data
  // (aString) when finished with it; the pointer is no longer valid
  // after FontDataDownloadComplete returns.
  // This is called even in the case of a failed download (HTTP 404, etc),
  // as there may still be data to be freed (e.g. an error page),
  // and we need to load the next source.

  // FontDataDownloadComplete will load the platform font on a worker thread,
  // and will call FontLoadComplete when it has finished its work.
  mUserFontEntry->FontDataDownloadComplete(mSrcIndex, aString, aStringLen,
                                           aStatus, this);
  return NS_SUCCESS_ADOPTED_DATA;
}

nsresult nsFontFaceLoader::FontLoadComplete() {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mFontFaceSet) {
    // We've been canceled
    return NS_OK;
  }

  // when new font loaded, need to reflow
  nsTArray<gfxUserFontSet*> fontSets;
  mUserFontEntry->GetUserFontSets(fontSets);
  for (gfxUserFontSet* fontSet : fontSets) {
    nsPresContext* ctx = FontFaceSet::GetPresContextFor(fontSet);
    if (ctx) {
      // Update layout for the presence of the new font.  Since this is
      // asynchronous, reflows will coalesce.
      ctx->UserFontSetUpdated(mUserFontEntry);
      LOG(("userfonts (%p) reflow for pres context %p\n", this, ctx));
    }
  }

  MOZ_DIAGNOSTIC_ASSERT(mFontFaceSet);
  mFontFaceSet->RemoveLoader(this);
  mFontFaceSet->Document()->UnblockOnload(false);
  mFontFaceSet = nullptr;

  return NS_OK;
}

// nsIRequestObserver
NS_IMETHODIMP
nsFontFaceLoader::OnStartRequest(nsIRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIThreadRetargetableRequest> req = do_QueryInterface(aRequest);
  if (req) {
    nsCOMPtr<nsIEventTarget> sts =
        do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
    Unused << NS_WARN_IF(NS_FAILED(req->RetargetDeliveryTo(sts)));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFontFaceLoader::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  MOZ_ASSERT(NS_IsMainThread());
  DropChannel();
  return NS_OK;
}

void nsFontFaceLoader::Cancel() {
  MOZ_DIAGNOSTIC_ASSERT(!mInLoadTimerCallback);
  MOZ_DIAGNOSTIC_ASSERT(!mInStreamComplete);
  MOZ_DIAGNOSTIC_ASSERT(mFontFaceSet);

  mUserFontEntry->LoadCanceled();
  mUserFontEntry = nullptr;
  mFontFaceSet->Document()->UnblockOnload(false);
  mFontFaceSet = nullptr;
  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }
  if (nsCOMPtr<nsIChannel> channel = std::move(mChannel)) {
    channel->Cancel(NS_BINDING_ABORTED);
  }
}

StyleFontDisplay nsFontFaceLoader::GetFontDisplay() {
  if (!StaticPrefs::layout_css_font_display_enabled()) {
    return StyleFontDisplay::Auto;
  }
  return mUserFontEntry->GetFontDisplay();
}

#undef LOG
#undef LOG_ENABLED
