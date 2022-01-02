/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaShutdownManager.h"

#include "MediaDecoder.h"
#include "mozilla/Logging.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIWritablePropertyBag2.h"

namespace mozilla {

#undef LOGW

extern LazyLogModule gMediaDecoderLog;
#define DECODER_LOG(type, msg) MOZ_LOG(gMediaDecoderLog, type, msg)
#define LOGW(...) NS_WARNING(nsPrintfCString(__VA_ARGS__).get())

NS_IMPL_ISUPPORTS(MediaShutdownManager, nsIAsyncShutdownBlocker)

MediaShutdownManager::MediaShutdownManager() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(sInitPhase == NotInited);
}

MediaShutdownManager::~MediaShutdownManager() { MOZ_ASSERT(NS_IsMainThread()); }

// Note that we don't use ClearOnShutdown() on this StaticRefPtr, as that
// may interfere with our shutdown listener.
StaticRefPtr<MediaShutdownManager> MediaShutdownManager::sInstance;

MediaShutdownManager::InitPhase MediaShutdownManager::sInitPhase =
    MediaShutdownManager::NotInited;

MediaShutdownManager& MediaShutdownManager::Instance() {
  MOZ_ASSERT(NS_IsMainThread());
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  if (!sInstance) {
    MOZ_CRASH_UNSAFE_PRINTF("sInstance is null. sInitPhase=%d",
                            int(sInitPhase));
  }
#endif
  return *sInstance;
}

void MediaShutdownManager::InitStatics() {
  MOZ_ASSERT(NS_IsMainThread());
  if (sInitPhase != NotInited) {
    return;
  }

  sInstance = new MediaShutdownManager();
  MOZ_DIAGNOSTIC_ASSERT(sInstance);

  nsCOMPtr<nsIAsyncShutdownClient> barrier = media::GetShutdownBarrier();

  if (!barrier) {
    LOGW("Failed to get barrier, cannot add shutdown blocker!");
    sInitPhase = InitFailed;
    return;
  }

  nsresult rv =
      barrier->AddBlocker(sInstance, NS_LITERAL_STRING_FROM_CSTRING(__FILE__),
                          __LINE__, u"MediaShutdownManager shutdown"_ns);
  if (NS_FAILED(rv)) {
    LOGW("Failed to add shutdown blocker! rv=%x", uint32_t(rv));
    sInitPhase = InitFailed;
    return;
  }
  sInitPhase = InitSucceeded;
}

void MediaShutdownManager::RemoveBlocker() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(sInitPhase == XPCOMShutdownStarted);
  MOZ_ASSERT(mDecoders.Count() == 0);
  nsCOMPtr<nsIAsyncShutdownClient> barrier = media::GetShutdownBarrier();
  // xpcom should still be available because we blocked shutdown by having a
  // blocker. Until it completely shuts down we should still be able to get
  // the barrier.
  MOZ_RELEASE_ASSERT(
      barrier,
      "Failed to get shutdown barrier, cannot remove shutdown blocker!");
  barrier->RemoveBlocker(this);
  // Clear our singleton reference. This will probably delete
  // this instance, so don't deref |this| clearing sInstance.
  sInitPhase = XPCOMShutdownEnded;
  sInstance = nullptr;
  DECODER_LOG(LogLevel::Debug, ("MediaShutdownManager::BlockShutdown() end."));
}

nsresult MediaShutdownManager::Register(MediaDecoder* aDecoder) {
  MOZ_ASSERT(NS_IsMainThread());
  if (sInitPhase == InitFailed) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (sInitPhase == XPCOMShutdownStarted) {
    return NS_ERROR_ABORT;
  }
  // Don't call Register() after you've Unregistered() all the decoders,
  // that's not going to work.
  MOZ_ASSERT(!mDecoders.Contains(aDecoder));
  mDecoders.Insert(aDecoder);
  MOZ_ASSERT(mDecoders.Contains(aDecoder));
  MOZ_ASSERT(mDecoders.Count() > 0);
  return NS_OK;
}

void MediaShutdownManager::Unregister(MediaDecoder* aDecoder) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!mDecoders.EnsureRemoved(aDecoder)) {
    return;
  }
  if (sInitPhase == XPCOMShutdownStarted && mDecoders.Count() == 0) {
    RemoveBlocker();
  }
}

NS_IMETHODIMP
MediaShutdownManager::GetName(nsAString& aName) {
  aName = u"MediaShutdownManager: shutdown"_ns;
  return NS_OK;
}

NS_IMETHODIMP
MediaShutdownManager::GetState(nsIPropertyBag** aBagOut) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aBagOut);

  nsCOMPtr<nsIWritablePropertyBag2> propertyBag =
      do_CreateInstance("@mozilla.org/hash-property-bag;1");

  if (NS_WARN_IF(!propertyBag)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsresult rv = propertyBag->SetPropertyAsInt32(
      u"sInitPhase"_ns, static_cast<int32_t>(sInitPhase));

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString decoderInfo;
  for (const auto& key : mDecoders) {
    // Grab the full extended type for the decoder. This can be used to help
    // indicate problems with specific decoders by associating type -> decoder.
    decoderInfo.Append(key->ContainerType().ExtendedType().OriginalString());
    decoderInfo.Append(", ");
  }

  rv = propertyBag->SetPropertyAsACString(u"decoderInfo"_ns, decoderInfo);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  propertyBag.forget(aBagOut);

  return NS_OK;
}

NS_IMETHODIMP
MediaShutdownManager::BlockShutdown(nsIAsyncShutdownClient*) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(sInitPhase == InitSucceeded);
  MOZ_DIAGNOSTIC_ASSERT(sInstance);

  DECODER_LOG(LogLevel::Debug,
              ("MediaShutdownManager::BlockShutdown() start..."));

  // Set this flag to ensure no Register() is allowed when Shutdown() begins.
  sInitPhase = XPCOMShutdownStarted;

  auto oldCount = mDecoders.Count();
  if (oldCount == 0) {
    RemoveBlocker();
    return NS_OK;
  }

  // Iterate over the decoders and shut them down.
  for (const auto& key : mDecoders) {
    key->NotifyXPCOMShutdown();
    // Check MediaDecoder::Shutdown doesn't call Unregister() synchronously in
    // order not to corrupt our hashtable traversal.
    MOZ_ASSERT(mDecoders.Count() == oldCount);
  }

  return NS_OK;
}

}  // namespace mozilla

// avoid redefined macro in unified build
#undef LOGW
