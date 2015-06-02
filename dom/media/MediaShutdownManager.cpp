/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaShutdownManager.h"
#include "nsContentUtils.h"
#include "mozilla/StaticPtr.h"
#include "MediaDecoder.h"
#include "SharedThreadPool.h"
#include "mozilla/Logging.h"

namespace mozilla {

extern PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)

NS_IMPL_ISUPPORTS(MediaShutdownManager, nsIObserver)

MediaShutdownManager::MediaShutdownManager()
  : mIsObservingShutdown(false),
    mIsDoingXPCOMShutDown(false)
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
  MOZ_ASSERT(!mIsDoingXPCOMShutDown);
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
  if (!mIsDoingXPCOMShutDown) {
    mDecoders.RemoveEntry(aDecoder);
    EnsureCorrectShutdownObserverState();
  }
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

static PLDHashOperator
ShutdownMediaDecoder(nsRefPtrHashKey<MediaDecoder>* aEntry, void*)
{
  aEntry->GetKey()->Shutdown();
  return PL_DHASH_REMOVE;
}

void
MediaShutdownManager::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance);

  DECODER_LOG(LogLevel::Debug, ("MediaShutdownManager::Shutdown() start..."));

  // Mark that we're shutting down, so that Unregister(*) calls don't remove
  // hashtable entries. If Unregsiter(*) was to remove from the hash table,
  // the iterations over the hashtables below would be disrupted.
  mIsDoingXPCOMShutDown = true;

  // Iterate over the decoders and shut them down, and remove them from the
  // hashtable.
  mDecoders.EnumerateEntries(ShutdownMediaDecoder, nullptr);

  // Ensure all media shared thread pools are shutdown. This joins with all
  // threads in the state machine thread pool, the decoder thread pool, and
  // any others.
  SharedThreadPool::SpinUntilShutdown();

  // Remove the MediaShutdownManager instance from the shutdown observer
  // list.
  nsContentUtils::UnregisterShutdownObserver(this);

  // Clear the singleton instance. The only remaining reference should be the
  // reference that the observer service used to call us with. The
  // MediaShutdownManager will be deleted once the observer service cleans
  // up after it finishes its notifications.
  sInstance = nullptr;

  DECODER_LOG(LogLevel::Debug, ("MediaShutdownManager::Shutdown() end."));
}

} // namespace mozilla
