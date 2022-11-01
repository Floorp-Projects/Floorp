/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontInfoLoader.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/AppShutdown.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"  // for nsRunnable
#include "gfxPlatformFontList.h"

#ifdef XP_WIN
#  include <windows.h>
#endif

using namespace mozilla;
using services::GetObserverService;

#define LOG_FONTINIT(args) \
  MOZ_LOG(gfxPlatform::GetLog(eGfxLog_fontinit), LogLevel::Debug, args)
#define LOG_FONTINIT_ENABLED() \
  MOZ_LOG_TEST(gfxPlatform::GetLog(eGfxLog_fontinit), LogLevel::Debug)

void FontInfoData::Load() {
  TimeStamp start = TimeStamp::Now();

  uint32_t i, n = mFontFamiliesToLoad.Length();
  mLoadStats.families = n;
  for (i = 0; i < n && !mCanceled; i++) {
    // font file memory mapping sometimes causes exceptions - bug 1100949
    MOZ_SEH_TRY { LoadFontFamilyData(mFontFamiliesToLoad[i]); }
    MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
      gfxCriticalError() << "Exception occurred reading font data for "
                         << mFontFamiliesToLoad[i].get();
    }
  }

  mLoadTime = TimeStamp::Now() - start;
}

class FontInfoLoadCompleteEvent : public Runnable {
  virtual ~FontInfoLoadCompleteEvent() = default;

 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FontInfoLoadCompleteEvent, Runnable)

  explicit FontInfoLoadCompleteEvent(FontInfoData* aFontInfo)
      : mozilla::Runnable("FontInfoLoadCompleteEvent"), mFontInfo(aFontInfo) {}

  NS_IMETHOD Run() override;

 private:
  RefPtr<FontInfoData> mFontInfo;
};

class AsyncFontInfoLoader : public Runnable {
  virtual ~AsyncFontInfoLoader() = default;

 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(AsyncFontInfoLoader, Runnable)

  explicit AsyncFontInfoLoader(FontInfoData* aFontInfo)
      : mozilla::Runnable("AsyncFontInfoLoader"), mFontInfo(aFontInfo) {
    mCompleteEvent = new FontInfoLoadCompleteEvent(aFontInfo);
  }

  NS_IMETHOD Run() override;

 private:
  RefPtr<FontInfoData> mFontInfo;
  RefPtr<FontInfoLoadCompleteEvent> mCompleteEvent;
};

class ShutdownThreadEvent : public Runnable {
  virtual ~ShutdownThreadEvent() = default;

 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(ShutdownThreadEvent, Runnable)

  explicit ShutdownThreadEvent(nsIThread* aThread)
      : mozilla::Runnable("ShutdownThreadEvent"), mThread(aThread) {}
  NS_IMETHOD Run() override {
    mThread->Shutdown();
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIThread> mThread;
};

// runs on main thread after async font info loading is done
nsresult FontInfoLoadCompleteEvent::Run() {
  gfxFontInfoLoader* loader =
      static_cast<gfxFontInfoLoader*>(gfxPlatformFontList::PlatformFontList());

  loader->FinalizeLoader(mFontInfo);

  return NS_OK;
}

// runs on separate thread
nsresult AsyncFontInfoLoader::Run() {
  // load platform-specific font info
  mFontInfo->Load();

  // post a completion event that transfer the data to the fontlist
  NS_DispatchToMainThread(mCompleteEvent);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(gfxFontInfoLoader::ShutdownObserver, nsIObserver)

NS_IMETHODIMP
gfxFontInfoLoader::ShutdownObserver::Observe(nsISupports* aSubject,
                                             const char* aTopic,
                                             const char16_t* someData) {
  if (!nsCRT::strcmp(aTopic, "quit-application") ||
      !nsCRT::strcmp(aTopic, "xpcom-shutdown")) {
    mLoader->CancelLoader();
  } else {
    MOZ_ASSERT_UNREACHABLE("unexpected notification topic");
  }
  return NS_OK;
}

// StartLoader is usually called at startup with a (prefs-derived) delay value,
// so that the async loader runs shortly after startup, to avoid competing for
// disk i/o etc with other more critical operations.
// However, it may be called with aDelay=0 if we find that the font info (e.g.
// localized names) is needed for layout. In this case we start the loader
// immediately; however, it is still an async process and we may use fallback
// fonts to satisfy layout until it completes.
void gfxFontInfoLoader::StartLoader(uint32_t aDelay) {
  if (aDelay == 0 && (mState == stateTimerOff || mState == stateAsyncLoad)) {
    // We were asked to load (async) without delay, but have already started,
    // so just return and let the loader proceed.
    return;
  }

  // We observe for "quit-application" above, so avoid initialization after it.
  if (NS_WARN_IF(AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdown))) {
    MOZ_ASSERT(!aDelay, "Delayed gfxFontInfoLoader startup after AppShutdown?");
    return;
  }

  // sanity check
  if (mState != stateInitial && mState != stateTimerOff &&
      mState != stateTimerOnDelay) {
    CancelLoader();
  }

  // Create mFontInfo when we're initially called to set up the delay, rather
  // than when called by the DelayedStartCallback, because on the initial call
  // we know we'll be holding the gfxPlatformFontList lock.
  if (!mFontInfo) {
    mFontInfo = CreateFontInfoData();
    if (!mFontInfo) {
      // The platform doesn't want anything loaded, so just bail out.
      mState = stateTimerOff;
      return;
    }
  }

  AddShutdownObserver();

  // Caller asked for a delay? ==> start async thread after a delay
  if (aDelay) {
    // Set up delay timer, or if there is already a timer in place, just
    // leave it to do its thing. (This can happen if a StartLoader runnable
    // was posted to the main thread from the InitFontList thread, but then
    // before it had a chance to run and call StartLoader, the main thread
    // re-initialized the list due to a platform notification and called
    // StartLoader directly.)
    if (mTimer) {
      return;
    }
    mTimer = NS_NewTimer();
    mTimer->InitWithNamedFuncCallback(DelayedStartCallback, this, aDelay,
                                      nsITimer::TYPE_ONE_SHOT,
                                      "gfxFontInfoLoader::StartLoader");
    mState = stateTimerOnDelay;
    return;
  }

  // Either we've been called back by the DelayedStartCallback when its timer
  // fired, or a layout caller has passed aDelay=0 to ask the loader to run
  // without further delay.

  // Cancel the delay timer, if any.
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  // initialize
  InitLoader();

  // start async load
  nsresult rv = NS_NewNamedThread("Font Loader",
                                  getter_AddRefs(mFontLoaderThread), nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  PRThread* prThread;
  if (NS_SUCCEEDED(mFontLoaderThread->GetPRThread(&prThread))) {
    PR_SetThreadPriority(prThread, PR_PRIORITY_LOW);
  }

  mState = stateAsyncLoad;

  nsCOMPtr<nsIRunnable> loadEvent = new AsyncFontInfoLoader(mFontInfo);

  mFontLoaderThread->Dispatch(loadEvent.forget(), NS_DISPATCH_NORMAL);

  if (LOG_FONTINIT_ENABLED()) {
    LOG_FONTINIT(
        ("(fontinit) fontloader started (fontinfo: %p)\n", mFontInfo.get()));
  }
}

class FinalizeLoaderRunnable : public Runnable {
  virtual ~FinalizeLoaderRunnable() = default;

 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(FinalizeLoaderRunnable, Runnable)

  explicit FinalizeLoaderRunnable(gfxFontInfoLoader* aLoader)
      : mozilla::Runnable("FinalizeLoaderRunnable"), mLoader(aLoader) {}

  NS_IMETHOD Run() override {
    nsresult rv;
    if (mLoader->LoadFontInfo()) {
      mLoader->CancelLoader();
      rv = NS_OK;
    } else {
      nsCOMPtr<nsIRunnable> runnable = this;
      rv = NS_DispatchToCurrentThreadQueue(
          runnable.forget(), PR_INTERVAL_NO_TIMEOUT, EventQueuePriority::Idle);
    }
    return rv;
  }

 private:
  gfxFontInfoLoader* mLoader;
};

void gfxFontInfoLoader::FinalizeLoader(FontInfoData* aFontInfo) {
  // Avoid loading data if loader has already been canceled.
  // This should mean that CancelLoader() ran and the Load
  // thread has already Shutdown(), and likely before processing
  // the Shutdown event it handled the load event and sent back
  // our Completion event, thus we end up here.
  if (mState != stateAsyncLoad || mFontInfo != aFontInfo) {
    return;
  }

  mLoadTime = mFontInfo->mLoadTime;

  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIRunnable> runnable = new FinalizeLoaderRunnable(this);
  if (NS_FAILED(NS_DispatchToCurrentThreadQueue(runnable.forget(),
                                                PR_INTERVAL_NO_TIMEOUT,
                                                EventQueuePriority::Idle))) {
    NS_WARNING("Failed to finalize async font info");
  }
}

void gfxFontInfoLoader::CancelLoader() {
  if (mState == stateInitial) {
    return;
  }
  mState = stateTimerOff;
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  if (mFontInfo)  // null during any initial delay
    mFontInfo->mCanceled = true;
  if (mFontLoaderThread) {
    NS_DispatchToMainThread(new ShutdownThreadEvent(mFontLoaderThread));
    mFontLoaderThread = nullptr;
  }
  RemoveShutdownObserver();
  CleanupLoader();
}

gfxFontInfoLoader::~gfxFontInfoLoader() {
  RemoveShutdownObserver();
  MOZ_COUNT_DTOR(gfxFontInfoLoader);
}

void gfxFontInfoLoader::AddShutdownObserver() {
  if (mObserver) {
    return;
  }

  nsCOMPtr<nsIObserverService> obs = GetObserverService();
  if (obs) {
    mObserver = new ShutdownObserver(this);
    obs->AddObserver(mObserver, "quit-application", false);
    obs->AddObserver(mObserver, "xpcom-shutdown", false);
  }
}

void gfxFontInfoLoader::RemoveShutdownObserver() {
  if (mObserver) {
    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (obs) {
      obs->RemoveObserver(mObserver, "quit-application");
      obs->RemoveObserver(mObserver, "xpcom-shutdown");
      mObserver = nullptr;
    }
  }
}
