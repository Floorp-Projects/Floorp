/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineParent.h"

#include <audiosessiontypes.h>
#include <intsafe.h>
#include <mfapi.h>

#include "MFMediaEngineExtension.h"
#include "MFMediaEngineUtils.h"
#include "MFMediaEngineStream.h"
#include "MFMediaSource.h"
#include "RemoteDecoderManagerParent.h"
#include "WMF.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WindowsVersion.h"

namespace mozilla {

#define LOG(msg, ...)                                                        \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug,                                \
          ("MFMediaEngineParent=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

using MediaEngineMap = nsTHashMap<nsUint64HashKey, MFMediaEngineParent*>;
static StaticAutoPtr<MediaEngineMap> sMediaEngines;

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

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

MFMediaEngineParent::MFMediaEngineParent(RemoteDecoderManagerParent* aManager,
                                         nsISerialEventTarget* aManagerThread)
    : mMediaEngineId(++sMediaEngineIdx),
      mManager(aManager),
      mManagerThread(aManagerThread) {
  MOZ_ASSERT(aManager);
  MOZ_ASSERT(aManagerThread);
  MOZ_ASSERT(mMediaEngineId != 0);
  MOZ_ASSERT(XRE_IsRDDProcess());
  LOG("Created MFMediaEngineParent");
  RegisterMediaEngine(this);
  mIPDLSelfRef = this;
  CreateMediaEngine();
}

MFMediaEngineParent::~MFMediaEngineParent() {
  LOG("Destoryed MFMediaEngineParent");
  DestroyEngineIfExists();
  UnregisterMedieEngine(this);
}

void MFMediaEngineParent::DestroyEngineIfExists(
    const Maybe<MediaResult>& aError) {
  LOG("DestroyEngineIfExists, hasError=%d", aError.isSome());
  mMediaEngineNotify = nullptr;
  mMediaEngineExtension = nullptr;
  mMediaSource = nullptr;
  if (mMediaEngine) {
    mMediaEngine->Shutdown();
    mMediaEngine = nullptr;
  }
  mMediaEngineEventListener.DisconnectIfExists();
  mRequestSampleListener.DisconnectIfExists();
  if (aError) {
    Unused << SendNotifyError(*aError);
  }
}

void MFMediaEngineParent::CreateMediaEngine() {
  LOG("CreateMediaEngine");
  auto errorExit = MakeScopeExit([&] {
    MediaResult error(NS_ERROR_DOM_MEDIA_FATAL_ERR, "Failed to create engine");
    DestroyEngineIfExists(Some(error));
  });

  if (!wmf::MediaFoundationInitializer::HasInitialized()) {
    NS_WARNING("Failed to initialize media foundation");
    return;
  }

  // TODO : init DXGIDeviceManager and a virtual window when media has video.

  // Create an attribute and set mandatory information that are required for a
  // media engine creation.
  ComPtr<IMFAttributes> creationAttributes;
  RETURN_VOID_IF_FAILED(wmf::MFCreateAttributes(&creationAttributes, 6));
  RETURN_VOID_IF_FAILED(
      MakeAndInitialize<MFMediaEngineNotify>(&mMediaEngineNotify));
  mMediaEngineEventListener = mMediaEngineNotify->MediaEngineEvent().Connect(
      mManagerThread, this, &MFMediaEngineParent::HandleMediaEngineEvent);
  RETURN_VOID_IF_FAILED(creationAttributes->SetUnknown(
      MF_MEDIA_ENGINE_CALLBACK, mMediaEngineNotify.Get()));
  RETURN_VOID_IF_FAILED(creationAttributes->SetUINT32(
      MF_MEDIA_ENGINE_AUDIO_CATEGORY, AudioCategory_Media));
  RETURN_VOID_IF_FAILED(
      MakeAndInitialize<MFMediaEngineExtension>(&mMediaEngineExtension));
  RETURN_VOID_IF_FAILED(creationAttributes->SetUnknown(
      MF_MEDIA_ENGINE_EXTENSION, mMediaEngineExtension.Get()));
  // TODO : SET MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS

  ComPtr<IMFMediaEngineClassFactory> factory;
  RETURN_VOID_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory,
                                         nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(&factory)));
  const bool isLowLatency =
      (StaticPrefs::media_wmf_low_latency_enabled() || IsWin10OrLater()) &&
      !StaticPrefs::media_wmf_low_latency_force_disabled();
  static const DWORD MF_MEDIA_ENGINE_DEFAULT = 0;
  RETURN_VOID_IF_FAILED(factory->CreateInstance(
      isLowLatency ? MF_MEDIA_ENGINE_REAL_TIME_MODE : MF_MEDIA_ENGINE_DEFAULT,
      creationAttributes.Get(), &mMediaEngine));

  // TODO : set DComp mode for video

  // TODO : deal with encrypted content (set ContentProtectionManager and cdm
  // proxy)

  LOG("Created media engine successfully");
  mIsCreatedMediaEngine = true;
  errorExit.release();
}

void MFMediaEngineParent::HandleMediaEngineEvent(
    MFMediaEngineEventWrapper aEvent) {
  AssertOnManagerThread();
  switch (aEvent.mEvent) {
    case MF_MEDIA_ENGINE_EVENT_ERROR: {
      MOZ_ASSERT(aEvent.mParam1 && aEvent.mParam2);
      auto error = static_cast<MF_MEDIA_ENGINE_ERR>(*aEvent.mParam1);
      auto result = static_cast<HRESULT>(*aEvent.mParam2);
      NotifyError(error, result);
      break;
    }
    case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY:
    case MF_MEDIA_ENGINE_EVENT_LOADEDDATA:
    case MF_MEDIA_ENGINE_EVENT_WAITING:
    case MF_MEDIA_ENGINE_EVENT_SEEKED:
    case MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED:
    case MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED:
    case MF_MEDIA_ENGINE_EVENT_ENDED:
    case MF_MEDIA_ENGINE_EVENT_PLAYING:
      Unused << SendNotifyEvent(aEvent.mEvent);
      break;
    case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE: {
      auto currentTimeInSeconds = mMediaEngine->GetCurrentTime();
      Unused << SendUpdateCurrentTime(currentTimeInSeconds);
      break;
    }
    default:
      LOG("Unhandled event=%s", MediaEngineEventToStr(aEvent.mEvent));
      break;
  }
}

void MFMediaEngineParent::NotifyError(MF_MEDIA_ENGINE_ERR aError,
                                      HRESULT aResult) {
  // TODO : handle HRESULT 0x8004CD12, DRM_E_TEE_INVALID_HWDRM_STATE, which can
  // happen during OS sleep/resume, or moving video to different graphics
  // adapters.
  if (aError == MF_MEDIA_ENGINE_ERR_NOERROR) {
    return;
  }
  LOG("Notify error '%s', hr=%lx", MFMediaEngineErrorToStr(aError), aResult);
  switch (aError) {
    case MF_MEDIA_ENGINE_ERR_ABORTED:
    case MF_MEDIA_ENGINE_ERR_NETWORK:
      // We ignore these two because we fetch data by ourselves.
      return;
    case MF_MEDIA_ENGINE_ERR_DECODE: {
      MediaResult error(NS_ERROR_DOM_MEDIA_DECODE_ERR, "Decoder error");
      Unused << SendNotifyError(error);
      return;
    }
    case MF_MEDIA_ENGINE_ERR_SRC_NOT_SUPPORTED: {
      MediaResult error(NS_ERROR_DOM_MEDIA_FATAL_ERR, "Source not supported");
      Unused << SendNotifyError(error);
      return;
    }
    case MF_MEDIA_ENGINE_ERR_ENCRYPTED: {
      MediaResult error(NS_ERROR_DOM_MEDIA_FATAL_ERR, "Encrypted error");
      Unused << SendNotifyError(error);
      return;
    }
    default:
      MOZ_ASSERT_UNREACHABLE("Unsupported error");
      return;
  }
}

MFMediaEngineStreamWrapper* MFMediaEngineParent::GetMediaEngineStream(
    TrackType aType, const CreateDecoderParams& aParam) {
  LOG("Create a media engine decoder for %s", TrackTypeToStr(aType));
  MOZ_ASSERT(mMediaSource);
  if (aType == TrackType::kAudioTrack) {
    auto* stream = mMediaSource->GetAudioStream();
    return new MFMediaEngineStreamWrapper(stream, stream->GetTaskQueue(),
                                          aParam);
  }
  MOZ_ASSERT(aType == TrackType::kVideoTrack);
  // TODO : This is for testing. Remove this pref after finishing the video
  // output implementation. Our first step is to make audio playback work.
  if (StaticPrefs::media_wmf_media_engine_video_output_enabled()) {
    auto* stream = mMediaSource->GetVideoStream();
    return new MFMediaEngineStreamWrapper(stream, stream->GetTaskQueue(),
                                          aParam);
  }
  return nullptr;
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvInitMediaEngine(
    const MediaEngineInfoIPDL& aInfo, InitMediaEngineResolver&& aResolver) {
  AssertOnManagerThread();
  if (!mIsCreatedMediaEngine) {
    aResolver(0);
    return IPC_OK();
  }
  // Metadata preload is controlled by content process side before creating
  // media engine.
  if (aInfo.preload()) {
    // TODO : really need this?
    Unused << mMediaEngine->SetPreload(MF_MEDIA_ENGINE_PRELOAD_AUTOMATIC);
  }
  aResolver(mMediaEngineId);
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvNotifyMediaInfo(
    const MediaInfoIPDL& aInfo) {
  AssertOnManagerThread();
  MOZ_ASSERT(mIsCreatedMediaEngine, "Hasn't created media engine?");
  MOZ_ASSERT(!mMediaSource);

  LOG("RecvNotifyMediaInfo");

  auto errorExit = MakeScopeExit([&] {
    MediaResult error(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      "Failed to setup media source");
    DestroyEngineIfExists(Some(error));
  });

  // Create media source and set it to the media engine.
  NS_ENSURE_TRUE(SUCCEEDED(MakeAndInitialize<MFMediaSource>(
                     &mMediaSource, aInfo.audioInfo(), aInfo.videoInfo())),
                 IPC_OK());
  mMediaEngineExtension->SetMediaSource(mMediaSource.Get());

  // We use the source scheme in order to let the media engine to load our
  // custom source.
  NS_ENSURE_TRUE(
      SUCCEEDED(mMediaEngine->SetSource(SysAllocString(L"MFRendererSrc"))),
      IPC_OK());

  LOG("Finished setup our custom media source to the media engine");
  mRequestSampleListener = mMediaSource->RequestSampleEvent().Connect(
      mManagerThread, this, &MFMediaEngineParent::HandleRequestSample);

  errorExit.release();
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvPlay() {
  AssertOnManagerThread();
  if (mMediaEngine) {
    LOG("Play");
    NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->Play()), IPC_OK());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvPause() {
  AssertOnManagerThread();
  if (mMediaEngine) {
    LOG("Pause");
    NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->Pause()), IPC_OK());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSeek(
    double aTargetTimeInSecond) {
  AssertOnManagerThread();
  if (mMediaEngine) {
    LOG("Seek to %f", aTargetTimeInSecond);
    NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->SetCurrentTime(aTargetTimeInSecond)),
                   IPC_OK());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSetVolume(double aVolume) {
  AssertOnManagerThread();
  if (mMediaEngine) {
    LOG("SetVolume=%f", aVolume);
    NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->SetVolume(aVolume)), IPC_OK());
  }
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
  // We handle looping by seeking back to the head by ourselves, so we don't
  // rely on the media engine for looping.
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvNotifyEndOfStream(
    TrackInfo::TrackType aType) {
  AssertOnManagerThread();
  MOZ_ASSERT(mMediaSource);
  LOG("NotifyEndOfStream, type=%s", TrackTypeToStr(aType));
  mMediaSource->NotifyEndOfStream(aType);
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvShutdown() {
  AssertOnManagerThread();
  LOG("Shutdown");
  DestroyEngineIfExists();
  return IPC_OK();
}

void MFMediaEngineParent::Destroy() {
  AssertOnManagerThread();
  mIPDLSelfRef = nullptr;
}

void MFMediaEngineParent::HandleRequestSample(const SampleRequest& aRequest) {
  AssertOnManagerThread();
  MOZ_ASSERT(aRequest.mType == TrackInfo::TrackType::kAudioTrack ||
             aRequest.mType == TrackInfo::TrackType::kVideoTrack);
  Unused << SendRequestSample(aRequest.mType, aRequest.mIsEnough);
}

void MFMediaEngineParent::AssertOnManagerThread() const {
  MOZ_ASSERT(mManagerThread->IsOnCurrentThread());
}

#undef LOG
#undef RETURN_IF_FAILED

}  // namespace mozilla
