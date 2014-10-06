/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* code for loading in @font-face defined font data */

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG /* Allow logging in the release build */
#endif /* MOZ_LOGGING */
#include "prlog.h"

#include "nsFontFaceLoader.h"

#include "nsError.h"
#include "nsContentUtils.h"
#include "mozilla/Preferences.h"
#include "FontFaceSet.h"
#include "nsPresContext.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsIHttpChannel.h"
#include "nsIContentPolicy.h"
#include "nsContentPolicyUtils.h"

#include "mozilla/gfx/2D.h"

using namespace mozilla;

#ifdef PR_LOGGING
static PRLogModuleInfo* 
GetFontDownloaderLog()
{
  static PRLogModuleInfo* sLog;
  if (!sLog)
    sLog = PR_NewLogModule("fontdownloader");
  return sLog;
}
#endif /* PR_LOGGING */

#define LOG(args) PR_LOG(GetFontDownloaderLog(), PR_LOG_DEBUG, args)
#define LOG_ENABLED() PR_LOG_TEST(GetFontDownloaderLog(), PR_LOG_DEBUG)


nsFontFaceLoader::nsFontFaceLoader(gfxUserFontEntry* aUserFontEntry,
                                   nsIURI* aFontURI,
                                   mozilla::dom::FontFaceSet* aFontFaceSet,
                                   nsIChannel* aChannel)
  : mUserFontEntry(aUserFontEntry),
    mFontURI(aFontURI),
    mFontFaceSet(aFontFaceSet),
    mChannel(aChannel)
{
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
  int32_t loadTimeout =
    Preferences::GetInt("gfx.downloadable_fonts.fallback_delay", 3000);
  if (loadTimeout > 0) {
    mLoadTimer = do_CreateInstance("@mozilla.org/timer;1");
    if (mLoadTimer) {
      mLoadTimer->InitWithFuncCallback(LoadTimerCallback,
                                       static_cast<void*>(this),
                                       loadTimeout,
                                       nsITimer::TYPE_ONE_SHOT);
    }
  } else {
    mUserFontEntry->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
  }
  mStreamLoader = aStreamLoader;
}

void
nsFontFaceLoader::LoadTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsFontFaceLoader* loader = static_cast<nsFontFaceLoader*>(aClosure);

  if (!loader->mFontFaceSet) {
    // We've been canceled
    return;
  }

  gfxUserFontEntry* ufe = loader->mUserFontEntry.get();
  bool updateUserFontSet = true;

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
      loader->mLoadTimer->InitWithFuncCallback(LoadTimerCallback,
                                               static_cast<void*>(loader),
                                               delay >> 1,
                                               nsITimer::TYPE_ONE_SHOT);
      updateUserFontSet = false;
      LOG(("fontdownloader (%p) 75%% done, resetting timer\n", loader));
    }
  }

  // If the font is not 75% loaded, or if we've already timed out once
  // before, we mark this entry as "loading slowly", so the fallback
  // font will be used in the meantime, and tell the context to refresh.
  if (updateUserFontSet) {
    ufe->mFontDataLoadingState = gfxUserFontEntry::LOADING_SLOWLY;
    nsPresContext* ctx = loader->mFontFaceSet->GetPresContext();
    NS_ASSERTION(ctx, "userfontset doesn't have a presContext?");
    if (ctx) {
      loader->mFontFaceSet->IncrementGeneration();
      ctx->UserFontSetUpdated();
      LOG(("fontdownloader (%p) timeout reflow\n", loader));
    }
  }
}

NS_IMPL_ISUPPORTS(nsFontFaceLoader, nsIStreamLoaderObserver)

NS_IMETHODIMP
nsFontFaceLoader::OnStreamComplete(nsIStreamLoader* aLoader,
                                   nsISupports* aContext,
                                   nsresult aStatus,
                                   uint32_t aStringLen,
                                   const uint8_t* aString)
{
  if (!mFontFaceSet) {
    // We've been canceled
    return aStatus;
  }

  mFontFaceSet->RemoveLoader(this);

#ifdef PR_LOGGING
  if (LOG_ENABLED()) {
    nsAutoCString fontURI;
    mFontURI->GetSpec(fontURI);
    if (NS_SUCCEEDED(aStatus)) {
      LOG(("fontdownloader (%p) download completed - font uri: (%s)\n", 
           this, fontURI.get()));
    } else {
      LOG(("fontdownloader (%p) download failed - font uri: (%s) error: %8.8x\n", 
           this, fontURI.get(), aStatus));
    }
  }
#endif

  nsPresContext* ctx = mFontFaceSet->GetPresContext();
  NS_ASSERTION(ctx && !ctx->PresShell()->IsDestroying(),
               "We should have been canceled already");

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

  // when new font loaded, need to reflow
  if (fontUpdate) {
    // Update layout for the presence of the new font.  Since this is
    // asynchronous, reflows will coalesce.
    ctx->UserFontSetUpdated();
    LOG(("fontdownloader (%p) reflow\n", this));
  }

  // done with font set
  mFontFaceSet = nullptr;
  if (mLoadTimer) {
    mLoadTimer->Cancel();
    mLoadTimer = nullptr;
  }

  return NS_SUCCESS_ADOPTED_DATA;
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

nsresult
nsFontFaceLoader::CheckLoadAllowed(nsIPrincipal* aSourcePrincipal,
                                   nsIURI* aTargetURI,
                                   nsISupports* aContext)
{
  nsresult rv;

  if (!aSourcePrincipal)
    return NS_OK;

  // check with the security manager
  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  rv = secMan->CheckLoadURIWithPrincipal(aSourcePrincipal, aTargetURI,
                                        nsIScriptSecurityManager::STANDARD);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // check content policy
  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_FONT,
                                 aTargetURI,
                                 aSourcePrincipal,
                                 aContext,
                                 EmptyCString(), // mime type
                                 nullptr,
                                 &shouldLoad,
                                 nsContentUtils::GetContentPolicy(),
                                 nsContentUtils::GetSecurityManager());

  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    return NS_ERROR_CONTENT_BLOCKED;
  }

  return NS_OK;
}
