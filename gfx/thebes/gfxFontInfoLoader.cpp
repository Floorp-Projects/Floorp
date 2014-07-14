/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontInfoLoader.h"
#include "nsCRT.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"              // for nsRunnable
#include "gfxPlatformFontList.h"

using namespace mozilla;
using mozilla::services::GetObserverService;

void
FontInfoData::Load()
{
    TimeStamp start = TimeStamp::Now();

    uint32_t i, n = mFontFamiliesToLoad.Length();
    mLoadStats.families = n;
    for (i = 0; i < n; i++) {
        LoadFontFamilyData(mFontFamiliesToLoad[i]);
    }

    mLoadTime = TimeStamp::Now() - start;
}

class FontInfoLoadCompleteEvent : public nsRunnable {
    virtual ~FontInfoLoadCompleteEvent() {}

    NS_DECL_ISUPPORTS_INHERITED

    FontInfoLoadCompleteEvent(FontInfoData *aFontInfo) :
        mFontInfo(aFontInfo)
    {}

    NS_IMETHOD Run();

    nsRefPtr<FontInfoData> mFontInfo;
};

class AsyncFontInfoLoader : public nsRunnable {
    virtual ~AsyncFontInfoLoader() {}

    NS_DECL_ISUPPORTS_INHERITED

    AsyncFontInfoLoader(FontInfoData *aFontInfo) :
        mFontInfo(aFontInfo)
    {
        mCompleteEvent = new FontInfoLoadCompleteEvent(aFontInfo);
    }

    NS_IMETHOD Run();

    nsRefPtr<FontInfoData> mFontInfo;
    nsRefPtr<FontInfoLoadCompleteEvent> mCompleteEvent;
};

// runs on main thread after async font info loading is done
nsresult
FontInfoLoadCompleteEvent::Run()
{
    gfxFontInfoLoader *loader =
        static_cast<gfxFontInfoLoader*>(gfxPlatformFontList::PlatformFontList());

    loader->FinalizeLoader(mFontInfo);

    mFontInfo = nullptr;
    return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED0(FontInfoLoadCompleteEvent, nsRunnable);

// runs on separate thread
nsresult
AsyncFontInfoLoader::Run()
{
    // load platform-specific font info
    mFontInfo->Load();

    // post a completion event that transfer the data to the fontlist
    NS_DispatchToMainThread(mCompleteEvent);
    mFontInfo = nullptr;

    return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED0(AsyncFontInfoLoader, nsRunnable);

NS_IMPL_ISUPPORTS(gfxFontInfoLoader::ShutdownObserver, nsIObserver)

NS_IMETHODIMP
gfxFontInfoLoader::ShutdownObserver::Observe(nsISupports *aSubject,
                                             const char *aTopic,
                                             const char16_t *someData)
{
    if (!nsCRT::strcmp(aTopic, "quit-application")) {
        mLoader->CancelLoader();
    } else {
        NS_NOTREACHED("unexpected notification topic");
    }
    return NS_OK;
}

void
gfxFontInfoLoader::StartLoader(uint32_t aDelay, uint32_t aInterval)
{
    mInterval = aInterval;

    // sanity check
    if (mState != stateInitial &&
        mState != stateTimerOff &&
        mState != stateTimerOnDelay) {
        CancelLoader();
    }

    // set up timer
    if (!mTimer) {
        mTimer = do_CreateInstance("@mozilla.org/timer;1");
        if (!mTimer) {
            NS_WARNING("Failure to create font info loader timer");
            return;
        }
    }

    AddShutdownObserver();

    // delay? ==> start async thread after a delay
    if (aDelay) {
        mState = stateTimerOnDelay;
        mTimer->InitWithFuncCallback(DelayedStartCallback, this, aDelay,
                                     nsITimer::TYPE_ONE_SHOT);
        return;
    }

    mFontInfo = CreateFontInfoData();

    // initialize
    InitLoader();

    // start async load
    mState = stateAsyncLoad;
    nsresult rv = NS_NewNamedThread("Font Loader",
                                    getter_AddRefs(mFontLoaderThread),
                                    nullptr);
    if (NS_FAILED(rv)) {
        return;
    }

    nsCOMPtr<nsIRunnable> loadEvent = new AsyncFontInfoLoader(mFontInfo);

    mFontLoaderThread->Dispatch(loadEvent, NS_DISPATCH_NORMAL);
}

void
gfxFontInfoLoader::FinalizeLoader(FontInfoData *aFontInfo)
{
    // avoid loading data if loader has already been canceled
    if (mState != stateAsyncLoad) {
        return;
    }

    mLoadTime = mFontInfo->mLoadTime;

    // try to load all font data immediately
    if (LoadFontInfo()) {
        CancelLoader();
        return;
    }

    // not all work completed ==> run load on interval
    mState = stateTimerOnInterval;
    mTimer->InitWithFuncCallback(LoadFontInfoCallback, this, mInterval,
                                 nsITimer::TYPE_REPEATING_SLACK);
}

void
gfxFontInfoLoader::CancelLoader()
{
    if (mState == stateInitial) {
        return;
    }
    mState = stateTimerOff;
    if (mTimer) {
        mTimer->Cancel();
        mTimer = nullptr;
    }
    if (mFontLoaderThread) {
        mFontLoaderThread->Shutdown();
        mFontLoaderThread = nullptr;
    }
    RemoveShutdownObserver();
    CleanupLoader();
}

void
gfxFontInfoLoader::LoadFontInfoTimerFire()
{
    if (mState == stateTimerOnDelay) {
        mState = stateTimerOnInterval;
        mTimer->SetDelay(mInterval);
    }

    bool done = LoadFontInfo();
    if (done) {
        CancelLoader();
    }
}

gfxFontInfoLoader::~gfxFontInfoLoader()
{
    RemoveShutdownObserver();
}

void
gfxFontInfoLoader::AddShutdownObserver()
{
    if (mObserver) {
        return;
    }

    nsCOMPtr<nsIObserverService> obs = GetObserverService();
    if (obs) {
        mObserver = new ShutdownObserver(this);
        obs->AddObserver(mObserver, "quit-application", false);
    }
}

void
gfxFontInfoLoader::RemoveShutdownObserver()
{
    if (mObserver) {
        nsCOMPtr<nsIObserverService> obs = GetObserverService();
        if (obs) {
            obs->RemoveObserver(mObserver, "quit-application");
            mObserver = nullptr;
        }
    }
}
