/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineParent.h"

#include "MFMediaEngineAudioStream.h"
#include "MFMediaEngineUtils.h"
#include "MFMediaEngineVideoStream.h"
#include "RemoteDecoderManagerParent.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {

#define LOG(msg, ...)                                                        \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug,                                \
          ("MFMediaEngineParent=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

using MediaEngineMap = nsTHashMap<nsUint64HashKey, MFMediaEngineParent*>;
static StaticAutoPtr<MediaEngineMap> sMediaEngines;

StaticMutex sMediaEnginesLock;

static void RegisterMediaEngine(MFMediaEngineParent* aMediaEngine) {
  MOZ_ASSERT(aMediaEngine);
  StaticMutexAutoLock lock(sMediaEnginesLock);
  if (!sMediaEngines) {
    sMediaEngines = new MediaEngineMap();
  }
  sMediaEngines->InsertOrUpdate(aMediaEngine->Id(), aMediaEngine);
}

static void UnregisterMedieEngine(MFMediaEngineParent* aMediaEngine) {
  StaticMutexAutoLock lock(sMediaEnginesLock);
  if (sMediaEngines) {
    sMediaEngines->Remove(aMediaEngine->Id());
  }
}

/* static */
MFMediaEngineParent* MFMediaEngineParent::GetMediaEngineById(uint64_t aId) {
  StaticMutexAutoLock lock(sMediaEnginesLock);
  return sMediaEngines->Get(aId);
}

MFMediaEngineParent::MFMediaEngineParent(RemoteDecoderManagerParent* aManager)
    : mMediaEngineId(++sMediaEngineIdx), mManager(aManager) {
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(mMediaEngineId != 0);
  MOZ_ASSERT(XRE_IsRDDProcess());
  LOG("Created MFMediaEngineParent");
  RegisterMediaEngine(this);
  mIPDLSelfRef = this;
}

MFMediaEngineParent::~MFMediaEngineParent() {
  LOG("Destoryed MFMediaEngineParent");
  UnregisterMedieEngine(this);
}

MFMediaEngineStream* MFMediaEngineParent::GetMediaEngineStream(
    TrackType aType, const CreateDecoderParams& aParam) {
  LOG("Create a media engine decoder for %s", TrackTypeToStr(aType));
  // TODO : make those streams associated with their media engine and source.
  if (aType == TrackType::kAudioTrack) {
    return new MFMediaEngineAudioStream(aParam);
  }
  MOZ_ASSERT(aType == TrackType::kVideoTrack);
  return new MFMediaEngineVideoStream(aParam);
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvInitMediaEngine(
    const MediaEngineInfoIPDL& aInfo, InitMediaEngineResolver&& aResolver) {
  AssertOnManagerThread();
  aResolver(mMediaEngineId);
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvNotifyMediaInfo(
    const MediaInfoIPDL& aInfo) {
  AssertOnManagerThread();
  // TODO : implement this by using media engine
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvPlay() {
  AssertOnManagerThread();
  // TODO : implement this by using media engine
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvPause() {
  AssertOnManagerThread();
  // TODO : implement this by using media engine
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSeek(
    double aTargetTimeInSecond) {
  AssertOnManagerThread();
  // TODO : implement this by using media engine
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSetVolume(double aVolume) {
  AssertOnManagerThread();
  // TODO : implement this by using media engine
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSetPlaybackRate(
    double aPlaybackRate) {
  AssertOnManagerThread();
  // TODO : implement this by using media engine
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSetLooping(bool aLooping) {
  AssertOnManagerThread();
  // TODO : implement this by using media engine
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvNotifyEndOfStream(
    TrackInfo::TrackType aType) {
  AssertOnManagerThread();
  // TODO : implement this by using media engine
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvShutdown() {
  AssertOnManagerThread();
  LOG("Shutdown");
  return IPC_OK();
}

void MFMediaEngineParent::Destroy() {
  AssertOnManagerThread();
  mIPDLSelfRef = nullptr;
}

void MFMediaEngineParent::AssertOnManagerThread() const {
  MOZ_ASSERT(mManager->OnManagerThread());
}

#undef LOG

}  // namespace mozilla
