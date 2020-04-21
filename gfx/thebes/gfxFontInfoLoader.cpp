/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontInfoLoader.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "nsXPCOM.h"        // for gXPCOMThreadsShutDown
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

static bool sFontLoaderShutdownObserved = false;

NS_IMETHODIMP
gfxFontInfoLoader::ShutdownObserver::Observe(nsISupports* aSubject,
                                             const char* aTopic,
                                             const char16_t* someData) {
  if (!nsCRT::strcmp(aTopic, "quit-application") ||
      !nsCRT::strcmp(aTopic, "xpcom-shutdown")) {
    mLoader->CancelLoader();
    sFontLoaderShutdownObserved = true;
  } else {
    MOZ_ASSERT_UNREACHABLE("unexpected notification topic");
  }
  return NS_OK;
}

void gfxFontInfoLoader::StartLoader(uint32_t aDelay, uint32_t aInterval) {
  mInterval = aInterval;

  NS_ASSERTION(!mFontInfo, "fontinfo should be null when starting font loader");

  // sanity check
  if (mState != stateInitial && mState != stateTimerOff &&
      mState != stateTimerOnDelay) {
    CancelLoader();
  }

  // set up timer
  if (!mTimer) {
    mTimer = NS_NewTimer();
    if (!mTimer) {
      NS_WARNING("Failure to create font info loader timer");
      return;
    }
  }

  AddShutdownObserver();

  // delay? ==> start async thread after a delay
  if (aDelay) {
    NS_ASSERTION(!sFontLoaderShutdownObserved,
                 "Bug 1508626 - Setting delay timer for font loader after "
                 "shutdown observed");
    NS_ASSERTION(!gXPCOMThreadsShutDown,
                 "Bug 1508626 - Setting delay timer for font loader after "
                 "shutdown but before observer");
    mState = stateTimerOnDelay;
    mTimer->InitWithNamedFuncCallback(DelayedStartCallback, this, aDelay,
                                      nsITimer::TYPE_ONE_SHOT,
                                      "gfxFontInfoLoader::StartLoader");
    return;
  }

  NS_ASSERTION(
      !sFontLoaderShutdownObserved,
      "Bug 1508626 - Initializing font loader after shutdown observed");
  NS_ASSERTION(!gXPCOMThreadsShutDown,
               "Bug 1508626 - Initializing font loader after shutdown but "
               "before observer");

  mFontInfo = CreateFontInfoData();

  // initialize
  InitLoader();

  // start async load
  nsresult rv = NS_NewNamedThread("Font Loader",
                                  getter_AddRefs(mFontLoaderThread), nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  mState = stateAsyncLoad;

  nsCOMPtr<nsIRunnable> loadEvent = new AsyncFontInfoLoader(mFontInfo);

  mFontLoaderThread->Dispatch(loadEvent.forget(), NS_DISPATCH_NORMAL);

  if (LOG_FONTINIT_ENABLED()) {
    LOG_FONTINIT(
        ("(fontinit) fontloader started (fontinfo: %p)\n", mFontInfo.get()));
  }
}

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

  // try to load all font data immediately
  if (LoadFontInfo()) {
    CancelLoader();
    return;
  }

  NS_ASSERTION(!sFontLoaderShutdownObserved,
               "Bug 1508626 - Finalize with interval timer for font loader "
               "after shutdown observed");
  NS_ASSERTION(!gXPCOMThreadsShutDown,
               "Bug 1508626 - Finalize with interval timer for font loader "
               "after shutdown but before observer");

  // not all work completed ==> run load on interval
  mState = stateTimerOnInterval;
  mTimer->InitWithNamedFuncCallback(LoadFontInfoCallback, this, mInterval,
                                    nsITimer::TYPE_REPEATING_SLACK,
                                    "gfxFontInfoLoader::FinalizeLoader");
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

void gfxFontInfoLoader::LoadFontInfoTimerFire() {
  if (mState == stateTimerOnDelay) {
    NS_ASSERTION(!sFontLoaderShutdownObserved,
                 "Bug 1508626 - Setting interval timer for font loader after "
                 "shutdown observed");
    NS_ASSERTION(!gXPCOMThreadsShutDown,
                 "Bug 1508626 - Setting interval timer for font loader after "
                 "shutdown but before observer");

    mState = stateTimerOnInterval;
    mTimer->SetDelay(mInterval);
  }

  bool done = LoadFontInfo();
  if (done) {
    CancelLoader();
  }
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
