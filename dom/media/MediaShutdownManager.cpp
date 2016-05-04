/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaShutdownManager.h"
#include "nsContentUtils.h"
#include "mozilla/StaticPtr.h"
#include "MediaDecoder.h"
#include "mozilla/Logging.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
#define DECODER_LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)

NS_IMPL_ISUPPORTS(MediaShutdownManager, nsIObserver)

MediaShutdownManager::MediaShutdownManager()
  : mIsObservingShutdown(false)
  , mIsDoingXPCOMShutDown(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(MediaShutdownManager);
}

MediaShutdownManager::~MediaShutdownManager()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_DTOR(MediaShutdownManager);
}

// Note that we don't use ClearOnShutdown() on this StaticRefPtr, as that
// may interfere with our shutdown listener.
StaticRefPtr<MediaShutdownManager> MediaShutdownManager::sInstance;

MediaShutdownManager&
MediaShutdownManager::Instance()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInstance) {
    sInstance = new MediaShutdownManager();
  }
  return *sInstance;
}

void
MediaShutdownManager::EnsureCorrectShutdownObserverState()
{
  bool needShutdownObserver = mDecoders.Count() > 0;
  if (needShutdownObserver != mIsObservingShutdown) {
    mIsObservingShutdown = needShutdownObserver;
    if (mIsObservingShutdown) {
      nsContentUtils::RegisterShutdownObserver(this);
    } else {
      nsContentUtils::UnregisterShutdownObserver(this);
      // Clear our singleton reference. This will probably delete
      // this instance, so don't deref |this| clearing sInstance.
      sInstance = nullptr;
    }
  }
}

void
MediaShutdownManager::Register(MediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(!mIsDoingXPCOMShutDown);
  // Don't call Register() after you've Unregistered() all the decoders,
  // that's not going to work.
  MOZ_ASSERT(!mDecoders.Contains(aDecoder));
  mDecoders.PutEntry(aDecoder);
  MOZ_ASSERT(mDecoders.Contains(aDecoder));
  MOZ_ASSERT(mDecoders.Count() > 0);
  EnsureCorrectShutdownObserverState();
}

void
MediaShutdownManager::Unregister(MediaDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mDecoders.Contains(aDecoder));
  mDecoders.RemoveEntry(aDecoder);
  EnsureCorrectShutdownObserverState();
}

NS_IMETHODIMP
MediaShutdownManager::Observe(nsISupports *aSubjet,
                              const char *aTopic,
                              const char16_t *someData)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Shutdown();
  }
  return NS_OK;
}

void
MediaShutdownManager::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance);

  DECODER_LOG(LogLevel::Debug, ("MediaShutdownManager::Shutdown() start..."));

  // Set this flag to ensure no Register() is allowed when Shutdown() begins.
  mIsDoingXPCOMShutDown = true;

  DebugOnly<uint32_t> oldCount = mDecoders.Count();
  MOZ_ASSERT(oldCount > 0);

  // Iterate over the decoders and shut them down.
  for (auto iter = mDecoders.Iter(); !iter.Done(); iter.Next()) {
    iter.Get()->GetKey()->Shutdown();
    // Check MediaDecoder::Shutdown doesn't call Unregister() synchronously in
    // order not to corrupt our hashtable traversal.
    MOZ_ASSERT(mDecoders.Count() == oldCount);
  }

  // Spin the loop until all decoders are unregistered
  // which will then clear |sInstance|.
  while (sInstance) {
    NS_ProcessNextEvent(NS_GetCurrentThread(), true);
  }

  // Note: Don't access |this| which might be deleted after clearing sInstance.

  DECODER_LOG(LogLevel::Debug, ("MediaShutdownManager::Shutdown() end."));
}

} // namespace mozilla
