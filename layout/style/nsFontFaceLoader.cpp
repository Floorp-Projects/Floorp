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
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "FontFaceSet.h"
#include "nsPresContext.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIHttpChannel.h"
#include "nsIContentPolicy.h"
#include "nsIThreadRetargetableRequest.h"
#include "nsContentPolicyUtils.h"
#include "nsNetCID.h"

#include "mozilla/gfx/2D.h"

using namespace mozilla;
using namespace mozilla::dom;

#define LOG(args) MOZ_LOG(gfxUserFontSet::GetUserFontsLog(), mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(gfxUserFontSet::GetUserFontsLog(), \
                                  LogLevel::Debug)

static uint32_t
GetFallbackDelay()
{
  return Preferences::GetInt("gfx.downloadable_fonts.fallback_delay", 3000);
}

static uint32_t
GetShortFallbackDelay()
{
  return Preferences::GetInt("gfx.downloadable_fonts.fallback_delay_short", 100);
}

nsFontFaceLoader::nsFontFaceLoader(gfxUserFontEntry* aUserFontEntry,
                                   nsIURI* aFontURI,
                                   FontFaceSet* aFontFaceSet,
                                   nsIChannel* aChannel)
  : mUserFontEntry(aUserFontEntry),
    mFontURI(aFontURI),
    mFontFaceSet(aFontFaceSet),
    mChannel(aChannel)
{
  MOZ_ASSERT(mFontFaceSet,
             "We should get a valid FontFaceSet from the caller!");
  mStartTime = TimeStamp::Now();
}

nsFontFaceLoader::~nsFontFaceLoader()
{
  if (mUserFontEntry) {
    mUserFontEntry->mLoader = nullptr;
  }
  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }
  if (mFontFaceSet) {
    mFontFaceSet->RemoveLoader(this);
  }
}

void
nsFontFaceLoader::StartedLoading(nsIStreamLoader* aStreamLoader)
{
  int32_t loadTimeout;
  uint8_t fontDisplay = GetFontDisplay();
  if (fontDisplay == NS_FONT_DISPLAY_AUTO ||
      fontDisplay == NS_FONT_DISPLAY_BLOCK) {
    loadTimeout = GetFallbackDelay();
  } else {
    loadTimeout = GetShortFallbackDelay();
  }

  if (loadTimeout > 0) {
    NS_NewTimerWithFuncCallback(getter_AddRefs(mLoadTimer),
                                LoadTimerCallback,
                                static_cast<void*>(this),
                                loadTimeout,
                                nsITimer::TYPE_ONE_SHOT,
                                "LoadTimerCallback",
                                mFontFaceSet->Document()->EventTargetFor(TaskCategory::Other));
  } else {
    mUserFontEntry->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
  }
  mStreamLoader = aStreamLoader;
}

/* static */ void
nsFontFaceLoader::LoadTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsFontFaceLoader* loader = static_cast<nsFontFaceLoader*>(aClosure);

  if (!loader->mFontFaceSet) {
    // We've been canceled
    return;
  }

  gfxUserFontEntry* ufe = loader->mUserFontEntry.get();
  uint8_t fontDisplay = loader->GetFontDisplay();

  // Depending upon the value of the font-display descriptor for the font,
  // their may be one or two timeouts associated with each font. The LOADING_SLOWLY
  // state indicates that the fallback font is shown. The LOADING_TIMED_OUT
  // state indicates that the fallback font is shown *and* the downloaded font
  // resource will not replace the fallback font when the load completes.

  bool updateUserFontSet = true;
  switch (fontDisplay) {
    case NS_FONT_DISPLAY_AUTO:
    case NS_FONT_DISPLAY_BLOCK:
      // If the entry is loading, check whether it's >75% done; if so,
      // we allow another timeout period before showing a fallback font.
      if (ufe->mFontDataLoadingState == gfxUserFontEntry::LOADING_STARTED) {
        int64_t contentLength;
        uint32_t numBytesRead;
        if (NS_SUCCEEDED(loader->mChannel->GetContentLength(&contentLength)) &&
            contentLength > 0 &&
            contentLength < UINT32_MAX &&
            NS_SUCCEEDED(loader->mStreamLoader->GetNumBytesRead(&numBytesRead)) &&
            numBytesRead > 3 * (uint32_t(contentLength) >> 2))
        {
          // More than 3/4 the data has been downloaded, so allow 50% extra
          // time and hope the remainder will arrive before the additional
          // time expires.
          ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_ALMOST_DONE;
          uint32_t delay;
          loader->mLoadTimer->GetDelay(&delay);
          loader->mLoadTimer->InitWithNamedFuncCallback(
            LoadTimerCallback,
            static_cast<void*>(loader),
            delay >> 1,
            nsITimer::TYPE_ONE_SHOT,
            "nsFontFaceLoader::LoadTimerCallback");
          updateUserFontSet = false;
          LOG(("userfonts (%p) 75%% done, resetting timer\n", loader));
        }
      }
      if (updateUserFontSet) {
        ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
      }
      break;
    case NS_FONT_DISPLAY_SWAP:
      ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
      break;
    case NS_FONT_DISPLAY_FALLBACK: {
      if (ufe->mFontDataLoadingState == gfxUserFontEntry::LOADING_STARTED) {
        ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
      } else {
        ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_TIMED_OUT;
        updateUserFontSet = false;
      }
      break;
    }
    case NS_FONT_DISPLAY_OPTIONAL:
      ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_TIMED_OUT;
      break;

    default:
      NS_NOTREACHED("strange font-display value");
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
             loader, ctx, fontDisplay));
      }
    }
  }
}

NS_IMPL_ISUPPORTS(nsFontFaceLoader,
                  nsIStreamLoaderObserver,
                  nsIRequestObserver)

// nsIStreamLoaderObserver
NS_IMETHODIMP
nsFontFaceLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                   nsISupports* aContext,
                                   nsresult aStatus,
                                   uint32_t aStringLen,
                                   const uint8_t* aString)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mFontFaceSet) {
    // We've been canceled
    return aStatus;
  }

  mFontFaceSet->RemoveLoader(this);

  TimeStamp doneTime = TimeStamp::Now();
  TimeDuration downloadTime = doneTime - mStartTime;
  uint32_t downloadTimeMS = uint32_t(downloadTime.ToMilliseconds());
  Telemetry::Accumulate(Telemetry::WEBFONT_DOWNLOAD_TIME, downloadTimeMS);

  if (GetFontDisplay() == NS_FONT_DISPLAY_FALLBACK) {
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
      LOG(("userfonts (%p) download failed - font uri: (%s) error: %8.8" PRIx32 "\n",
           this, mFontURI->GetSpecOrDefault().get(), static_cast<uint32_t>(aStatus)));
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

  // The userFontEntry is responsible for freeing the downloaded data
  // (aString) when finished with it; the pointer is no longer valid
  // after FontDataDownloadComplete returns.
  // This is called even in the case of a failed download (HTTP 404, etc),
  // as there may still be data to be freed (e.g. an error page),
  // and we need to load the next source.
  bool fontUpdate =
    mUserFontEntry->FontDataDownloadComplete(aString, aStringLen, aStatus);

  mFontFaceSet->GetUserFontSet()->RecordFontLoadDone(aStringLen, doneTime);

  // when new font loaded, need to reflow
  if (fontUpdate) {
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
  }

  // done with font set
  mFontFaceSet = nullptr;
  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }

  return NS_SUCCESS_ADOPTED_DATA;
}

// nsIRequestObserver
NS_IMETHODIMP
nsFontFaceLoader::OnStartRequest(nsIRequest* aRequest,
                                 nsISupports* aContext)
{
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
nsFontFaceLoader::OnStopRequest(nsIRequest* aRequest,
                                nsISupports* aContext,
                                nsresult aStatusCode)
{
  MOZ_ASSERT(NS_IsMainThread());
  return NS_OK;
}

void
nsFontFaceLoader::Cancel()
{
  mUserFontEntry->mFontDataLoadingState = gfxUserFontEntry::NOT_LOADING;
  mUserFontEntry->mLoader = nullptr;
  mFontFaceSet = nullptr;
  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }
  mChannel->Cancel(NS_BINDING_ABORTED);
}

uint8_t
nsFontFaceLoader::GetFontDisplay()
{
  uint8_t fontDisplay = NS_FONT_DISPLAY_AUTO;
  if (StaticPrefs::layout_css_font_display_enabled()) {
    fontDisplay = mUserFontEntry->GetFontDisplay();
  }
  return fontDisplay;
}
