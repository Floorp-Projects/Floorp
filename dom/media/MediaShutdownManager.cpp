/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Logging.h"
#include "mozilla/StaticPtr.h"
#include "nsContentUtils.h"

#include "MediaDecoder.h"
#include "MediaShutdownManager.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
#define DECODER_LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)

NS_IMPL_ISUPPORTS(MediaShutdownManager, nsIAsyncShutdownBlocker)

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

static nsCOMPtr<nsIAsyncShutdownClient>
GetShutdownBarrier()
{
  nsCOMPtr<nsIAsyncShutdownService> svc = services::GetAsyncShutdown();
  MOZ_RELEASE_ASSERT(svc);

  nsCOMPtr<nsIAsyncShutdownClient> barrier;
  nsresult rv = svc->GetProfileBeforeChange(getter_AddRefs(barrier));
  if (!barrier) {
    // We are probably in a content process. We need to do cleanup at
    // XPCOM shutdown in leakchecking builds.
    rv = svc->GetXpcomWillShutdown(getter_AddRefs(barrier));
  }
  MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
  MOZ_RELEASE_ASSERT(barrier);
  return barrier.forget();
}

void
MediaShutdownManager::EnsureCorrectShutdownObserverState()
{
  bool needShutdownObserver = mDecoders.Count() > 0;
  if (needShutdownObserver != mIsObservingShutdown) {
    mIsObservingShutdown = needShutdownObserver;
    if (mIsObservingShutdown) {
      nsresult rv = GetShutdownBarrier()->AddBlocker(
        this, NS_LITERAL_STRING(__FILE__), __LINE__,
        NS_LITERAL_STRING("MediaShutdownManager shutdown"));
      MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    } else {
      GetShutdownBarrier()->RemoveBlocker(this);
      // Clear our singleton reference. This will probably delete
      // this instance, so don't deref |this| clearing sInstance.
      sInstance = nullptr;
      DECODER_LOG(LogLevel::Debug, ("MediaShutdownManager::BlockShutdown() end."));
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
MediaShutdownManager::GetName(nsAString& aName)
{
  aName = NS_LITERAL_STRING("MediaShutdownManager: shutdown");
  return NS_OK;
}

NS_IMETHODIMP
MediaShutdownManager::GetState(nsIPropertyBag**)
{
  return NS_OK;
}

NS_IMETHODIMP
MediaShutdownManager::BlockShutdown(nsIAsyncShutdownClient*)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(sInstance);

  DECODER_LOG(LogLevel::Debug, ("MediaShutdownManager::BlockShutdown() start..."));

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

  return NS_OK;
}

} // namespace mozilla
