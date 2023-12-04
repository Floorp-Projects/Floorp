/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineParent.h"

#include <audiosessiontypes.h>
#include <intsafe.h>
#include <mfapi.h>

#ifdef MOZ_WMF_CDM
#  include "MFCDMParent.h"
#  include "MFContentProtectionManager.h"
#endif

#include "MFMediaEngineExtension.h"
#include "MFMediaEngineVideoStream.h"
#include "MFMediaEngineUtils.h"
#include "MFMediaEngineStream.h"
#include "MFMediaSource.h"
#include "RemoteDecoderManagerParent.h"
#include "WMF.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/RemoteDecodeUtils.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/gfx/DeviceManagerDx.h"

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
  MOZ_ASSERT(XRE_IsUtilityProcess());
  MOZ_ASSERT(GetCurrentSandboxingKind() ==
             ipc::SandboxingKind::MF_MEDIA_ENGINE_CDM);
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
  ENGINE_MARKER("MFMediaEngineParent::DestroyEngineIfExists");
  mMediaEngineNotify = nullptr;
  mMediaEngineExtension = nullptr;
  if (mMediaSource) {
    mMediaSource->ShutdownTaskQueue();
    mMediaSource = nullptr;
  }
#ifdef MOZ_WMF_CDM
  if (mContentProtectionManager) {
    mContentProtectionManager->Shutdown();
    mContentProtectionManager = nullptr;
  }
#endif
  if (mMediaEngine) {
    LOG_IF_FAILED(mMediaEngine->Shutdown());
    mMediaEngine = nullptr;
  }
  mMediaEngineEventListener.DisconnectIfExists();
  mRequestSampleListener.DisconnectIfExists();
  if (mDXGIDeviceManager) {
    mDXGIDeviceManager = nullptr;
    wmf::MFUnlockDXGIDeviceManager();
  }
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

  InitializeDXGIDeviceManager();

  // Create an attribute and set mandatory information that are required for
  // a media engine creation.
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
  RETURN_VOID_IF_FAILED(
      creationAttributes->SetUINT32(MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS,
                                    MF_MEDIA_ENGINE_ENABLE_PROTECTED_CONTENT));
  if (mDXGIDeviceManager) {
    RETURN_VOID_IF_FAILED(creationAttributes->SetUnknown(
        MF_MEDIA_ENGINE_DXGI_MANAGER, mDXGIDeviceManager.Get()));
  }

  ComPtr<IMFMediaEngineClassFactory> factory;
  RETURN_VOID_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory,
                                         nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(&factory)));
  const bool isLowLatency = StaticPrefs::media_wmf_low_latency_enabled();
  static const DWORD MF_MEDIA_ENGINE_DEFAULT = 0;
  RETURN_VOID_IF_FAILED(factory->CreateInstance(
      isLowLatency ? MF_MEDIA_ENGINE_REAL_TIME_MODE : MF_MEDIA_ENGINE_DEFAULT,
      creationAttributes.Get(), &mMediaEngine));

  LOG("Created media engine successfully");
  mIsCreatedMediaEngine = true;
  ENGINE_MARKER("MFMediaEngineParent::CreatedMediaEngine");
  errorExit.release();
}

void MFMediaEngineParent::InitializeDXGIDeviceManager() {
  auto* deviceManager = gfx::DeviceManagerDx::Get();
  if (!deviceManager) {
    return;
  }
  RefPtr<ID3D11Device> d3d11Device = deviceManager->CreateMediaEngineDevice();
  if (!d3d11Device) {
    return;
  }

  auto errorExit = MakeScopeExit([&] {
    mDXGIDeviceManager = nullptr;
    wmf::MFUnlockDXGIDeviceManager();
  });
  UINT deviceResetToken;
  RETURN_VOID_IF_FAILED(
      wmf::MFLockDXGIDeviceManager(&deviceResetToken, &mDXGIDeviceManager));
  RETURN_VOID_IF_FAILED(
      mDXGIDeviceManager->ResetDevice(d3d11Device.get(), deviceResetToken));
  LOG("Initialized DXGI manager");
  errorExit.release();
}

#ifndef ENSURE_EVENT_DISPATCH_DURING_PLAYING
#  define ENSURE_EVENT_DISPATCH_DURING_PLAYING(event)        \
    do {                                                     \
      if (mMediaEngine->IsPaused()) {                        \
        LOG("Ignore incorrect '%s' during pausing!", event); \
        return;                                              \
      }                                                      \
    } while (false)
#endif

void MFMediaEngineParent::HandleMediaEngineEvent(
    MFMediaEngineEventWrapper aEvent) {
  AssertOnManagerThread();
  LOG("Received media engine event %s", MediaEngineEventToStr(aEvent.mEvent));
  ENGINE_MARKER_TEXT(
      "MFMediaEngineParent::HandleMediaEngineEvent",
      nsPrintfCString("%s", MediaEngineEventToStr(aEvent.mEvent)));
  switch (aEvent.mEvent) {
    case MF_MEDIA_ENGINE_EVENT_ERROR: {
      MOZ_ASSERT(aEvent.mParam1 && aEvent.mParam2);
      auto error = static_cast<MF_MEDIA_ENGINE_ERR>(*aEvent.mParam1);
      auto result = static_cast<HRESULT>(*aEvent.mParam2);
      NotifyError(error, result);
      break;
    }
    case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE: {
      if (mMediaEngine->HasVideo()) {
        NotifyVideoResizing();
      }
      break;
    }
    case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY: {
      if (mMediaEngine->HasVideo()) {
        EnsureDcompSurfaceHandle();
      }
      [[fallthrough]];
    }
    case MF_MEDIA_ENGINE_EVENT_LOADEDDATA:
    case MF_MEDIA_ENGINE_EVENT_WAITING:
    case MF_MEDIA_ENGINE_EVENT_SEEKED:
    case MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED:
    case MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED:
      Unused << SendNotifyEvent(aEvent.mEvent);
      break;
    case MF_MEDIA_ENGINE_EVENT_PLAYING:
      ENSURE_EVENT_DISPATCH_DURING_PLAYING(
          MediaEngineEventToStr(aEvent.mEvent));
      Unused << SendNotifyEvent(aEvent.mEvent);
      break;
    case MF_MEDIA_ENGINE_EVENT_ENDED: {
      ENSURE_EVENT_DISPATCH_DURING_PLAYING(
          MediaEngineEventToStr(aEvent.mEvent));
      Unused << SendNotifyEvent(aEvent.mEvent);
      UpdateStatisticsData();
      break;
    }
    case MF_MEDIA_ENGINE_EVENT_TIMEUPDATE: {
      auto currentTimeInSeconds = mMediaEngine->GetCurrentTime();
      Unused << SendUpdateCurrentTime(currentTimeInSeconds);
      UpdateStatisticsData();
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
  ENGINE_MARKER_TEXT(
      "MFMediaEngineParent::NotifyError",
      nsPrintfCString("%s, hr=%lx", MFMediaEngineErrorToStr(aError), aResult));
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
      MediaResult error(NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR,
                        "Source not supported");
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
  // Has been shutdowned.
  if (!mMediaSource) {
    return nullptr;
  }
  LOG("Create a media engine decoder for %s", TrackTypeToStr(aType));
  if (aType == TrackType::kAudioTrack) {
    auto* stream = mMediaSource->GetAudioStream();
    return new MFMediaEngineStreamWrapper(stream, stream->GetTaskQueue(),
                                          aParam);
  }
  MOZ_ASSERT(aType == TrackType::kVideoTrack);
  auto* stream = mMediaSource->GetVideoStream();
  stream->AsVideoStream()->SetKnowsCompositor(aParam.mKnowsCompositor);
  stream->AsVideoStream()->SetConfig(aParam.mConfig);
  return new MFMediaEngineStreamWrapper(stream, stream->GetTaskQueue(), aParam);
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
                      "Failed to create media source");
    DestroyEngineIfExists(Some(error));
  });

  // Create media source and set it to the media engine.
  NS_ENSURE_TRUE(
      SUCCEEDED(MakeAndInitialize<MFMediaSource>(
          &mMediaSource, aInfo.audioInfo(), aInfo.videoInfo(), mManagerThread)),
      IPC_OK());

  const bool isEncryted = mMediaSource->IsEncrypted();
  ENGINE_MARKER("MFMediaEngineParent,CreatedMediaSource");
  nsPrintfCString message(
      "Created the media source, audio=%s, video=%s, encrypted-audio=%s, "
      "encrypted-video=%s, isEncrypted=%d",
      aInfo.audioInfo() ? aInfo.audioInfo()->mMimeType.BeginReading() : "none",
      aInfo.videoInfo() ? aInfo.videoInfo()->mMimeType.BeginReading() : "none",
      aInfo.audioInfo() && aInfo.audioInfo()->mCrypto.IsEncrypted() ? "yes"
                                                                    : "no",
      aInfo.videoInfo() && aInfo.videoInfo()->mCrypto.IsEncrypted() ? "yes"
                                                                    : "no",
      isEncryted);
  LOG("%s", message.get());

  if (aInfo.videoInfo()) {
    ComPtr<IMFMediaEngineEx> mediaEngineEx;
    RETURN_PARAM_IF_FAILED(mMediaEngine.As(&mediaEngineEx), IPC_OK());
    RETURN_PARAM_IF_FAILED(mediaEngineEx->EnableWindowlessSwapchainMode(true),
                           IPC_OK());
    LOG("Enabled dcomp swap chain mode");
    ENGINE_MARKER("MFMediaEngineParent,EnabledSwapChain");
  }

  mRequestSampleListener = mMediaSource->RequestSampleEvent().Connect(
      mManagerThread, this, &MFMediaEngineParent::HandleRequestSample);
  errorExit.release();

#ifdef MOZ_WMF_CDM
  if (isEncryted && !mContentProtectionManager) {
    // We will set the source later when the CDM proxy is ready.
    return IPC_OK();
  }

  if (isEncryted && mContentProtectionManager) {
    auto* proxy = mContentProtectionManager->GetCDMProxy();
    MOZ_ASSERT(proxy);
    mMediaSource->SetCDMProxy(proxy);
  }
#endif

  SetMediaSourceOnEngine();
  return IPC_OK();
}

void MFMediaEngineParent::SetMediaSourceOnEngine() {
  AssertOnManagerThread();
  MOZ_ASSERT(mMediaSource);

  auto errorExit = MakeScopeExit([&] {
    MediaResult error(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      "Failed to set media source");
    DestroyEngineIfExists(Some(error));
  });

  mMediaEngineExtension->SetMediaSource(mMediaSource.Get());

  // We use the source scheme in order to let the media engine to load our
  // custom source.
  RETURN_VOID_IF_FAILED(
      mMediaEngine->SetSource(SysAllocString(L"MFRendererSrc")));

  LOG("Finished setup our custom media source to the media engine");
  ENGINE_MARKER("MFMediaEngineParent,FinishedSetupMediaSource");
  errorExit.release();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvPlay() {
  AssertOnManagerThread();
  if (!mMediaEngine) {
    LOG("Engine has been shutdowned!");
    return IPC_OK();
  }
  LOG("Play, expected playback rate %f, default playback rate=%f",
      mPlaybackRate, mMediaEngine->GetDefaultPlaybackRate());
  ENGINE_MARKER("MFMediaEngineParent,Play");
  NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->Play()), IPC_OK());
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvPause() {
  AssertOnManagerThread();
  if (!mMediaEngine) {
    LOG("Engine has been shutdowned!");
    return IPC_OK();
  }
  LOG("Pause");
  ENGINE_MARKER("MFMediaEngineParent,Pause");
  NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->Pause()), IPC_OK());
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSeek(
    double aTargetTimeInSecond) {
  AssertOnManagerThread();
  if (!mMediaEngine) {
    return IPC_OK();
  }

  // If the target time is already equal to the current time, the media engine
  // doesn't perform seek internally so we won't be able to receive `seeked`
  // event. Therefore, we simply return `seeked` here because we're already in
  // the target time.
  const auto currentTimeInSeconds = mMediaEngine->GetCurrentTime();
  if (currentTimeInSeconds == aTargetTimeInSecond) {
    Unused << SendNotifyEvent(MF_MEDIA_ENGINE_EVENT_SEEKED);
    return IPC_OK();
  }

  LOG("Seek to %f", aTargetTimeInSecond);
  ENGINE_MARKER_TEXT("MFMediaEngineParent,Seek",
                     nsPrintfCString("%f", aTargetTimeInSecond));
  NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->SetCurrentTime(aTargetTimeInSecond)),
                 IPC_OK());

  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSetCDMProxyId(
    uint64_t aProxyId) {
  if (!mMediaEngine) {
    return IPC_OK();
  }
#ifdef MOZ_WMF_CDM
  LOG("SetCDMProxy, Id=%" PRIu64, aProxyId);
  MFCDMParent* cdmParent = MFCDMParent::GetCDMById(aProxyId);
  MOZ_DIAGNOSTIC_ASSERT(cdmParent);
  RETURN_PARAM_IF_FAILED(
      MakeAndInitialize<MFContentProtectionManager>(&mContentProtectionManager),
      IPC_OK());

  ComPtr<IMFMediaEngineProtectedContent> protectedMediaEngine;
  RETURN_PARAM_IF_FAILED(mMediaEngine.As(&protectedMediaEngine), IPC_OK());
  RETURN_PARAM_IF_FAILED(protectedMediaEngine->SetContentProtectionManager(
                             mContentProtectionManager.Get()),
                         IPC_OK());

  RefPtr<MFCDMProxy> proxy = cdmParent->GetMFCDMProxy();
  if (!proxy) {
    LOG("Failed to get MFCDMProxy!");
    return IPC_OK();
  }

  RETURN_PARAM_IF_FAILED(mContentProtectionManager->SetCDMProxy(proxy),
                         IPC_OK());
  // TODO : is it possible to set CDM proxy before creating media source? If so,
  // handle that as well.
  if (mMediaSource) {
    mMediaSource->SetCDMProxy(proxy);
    SetMediaSourceOnEngine();
  }
  LOG("Set CDM Proxy successfully on the media engine!");
#endif
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSetVolume(double aVolume) {
  AssertOnManagerThread();
  if (mMediaEngine) {
    LOG("SetVolume=%f", aVolume);
    ENGINE_MARKER_TEXT("MFMediaEngineParent,SetVolume",
                       nsPrintfCString("%f", aVolume));
    NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->SetVolume(aVolume)), IPC_OK());
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineParent::RecvSetPlaybackRate(
    double aPlaybackRate) {
  AssertOnManagerThread();
  if (aPlaybackRate <= 0) {
    LOG("Not support zero or negative playback rate");
    return IPC_OK();
  }
  LOG("SetPlaybackRate=%f", aPlaybackRate);
  ENGINE_MARKER_TEXT("MFMediaEngineParent,SetPlaybackRate",
                     nsPrintfCString("%f", aPlaybackRate));
  mPlaybackRate = aPlaybackRate;
  NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->SetPlaybackRate(mPlaybackRate)),
                 IPC_OK());
  // The media Engine uses the default playback rate to determine the playback
  // rate when calling `play()`. So if we don't change default playback rate
  // together, the playback rate would fallback to 1 after pausing or
  // seeking, which would be different from our expected playback rate.
  NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->SetDefaultPlaybackRate(mPlaybackRate)),
                 IPC_OK());
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
  ENGINE_MARKER("MFMediaEngineParent,Shutdown");
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

Maybe<gfx::IntSize> MFMediaEngineParent::DetectVideoSizeChange() {
  AssertOnManagerThread();
  MOZ_ASSERT(mMediaEngine);
  MOZ_ASSERT(mMediaEngine->HasVideo());

  DWORD width, height;
  RETURN_PARAM_IF_FAILED(mMediaEngine->GetNativeVideoSize(&width, &height),
                         Nothing());
  if (width != mDisplayWidth || height != mDisplayHeight) {
    ENGINE_MARKER_TEXT("MFMediaEngineParent,VideoSizeChange",
                       nsPrintfCString("%lux%lu", width, height));
    LOG("Updated video size [%lux%lu] -> [%lux%lu] ", mDisplayWidth,
        mDisplayHeight, width, height);
    mDisplayWidth = width;
    mDisplayHeight = height;
    return Some(gfx::IntSize{width, height});
  }
  return Nothing();
}

void MFMediaEngineParent::EnsureDcompSurfaceHandle() {
  AssertOnManagerThread();
  MOZ_ASSERT(mMediaEngine);
  MOZ_ASSERT(mMediaEngine->HasVideo());

  ComPtr<IMFMediaEngineEx> mediaEngineEx;
  RETURN_VOID_IF_FAILED(mMediaEngine.As(&mediaEngineEx));

  // Ensure that the width and height is already up-to-date.
  gfx::IntSize size{mDisplayWidth, mDisplayHeight};
  if (auto newSize = DetectVideoSizeChange()) {
    size = *newSize;
  }

  // Update stream size before asking for a handle. If we don't update the
  // size, media engine will create the dcomp surface in a wrong size.
  RECT rect = {0, 0, (LONG)size.width, (LONG)size.height};
  RETURN_VOID_IF_FAILED(mediaEngineEx->UpdateVideoStream(
      nullptr /* pSrc */, &rect, nullptr /* pBorderClr */));

  HANDLE surfaceHandle = INVALID_HANDLE_VALUE;
  RETURN_VOID_IF_FAILED(mediaEngineEx->GetVideoSwapchainHandle(&surfaceHandle));
  if (surfaceHandle && surfaceHandle != INVALID_HANDLE_VALUE) {
    LOG("EnsureDcompSurfaceHandle, handle=%p, size=[%dx%d]", surfaceHandle,
        size.width, size.height);
    mMediaSource->SetDCompSurfaceHandle(surfaceHandle, size);
  } else {
    NS_WARNING("SurfaceHandle is not ready yet");
  }
}

void MFMediaEngineParent::NotifyVideoResizing() {
  AssertOnManagerThread();
  if (auto newSize = DetectVideoSizeChange()) {
    Unused << SendNotifyResizing(newSize->width, newSize->height);
  }
}

void MFMediaEngineParent::UpdateStatisticsData() {
  AssertOnManagerThread();

  // Statistic data is only for video.
  if (!mMediaEngine->HasVideo()) {
    return;
  }

  ComPtr<IMFMediaEngineEx> mediaEngineEx;
  RETURN_VOID_IF_FAILED(mMediaEngine.As(&mediaEngineEx));

  struct scopePropVariant : public PROPVARIANT {
    scopePropVariant() { PropVariantInit(this); }
    ~scopePropVariant() { PropVariantClear(this); }
    scopePropVariant(scopePropVariant const&) = delete;
    scopePropVariant& operator=(scopePropVariant const&) = delete;
  };

  // https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/ne-mfmediaengine-mf_media_engine_statistic
  scopePropVariant renderedFramesProp, droppedFramesProp;
  RETURN_VOID_IF_FAILED(mediaEngineEx->GetStatistics(
      MF_MEDIA_ENGINE_STATISTIC_FRAMES_RENDERED, &renderedFramesProp));
  RETURN_VOID_IF_FAILED(mediaEngineEx->GetStatistics(
      MF_MEDIA_ENGINE_STATISTIC_FRAMES_DROPPED, &droppedFramesProp));

  const unsigned long renderedFrames = renderedFramesProp.ulVal;
  const unsigned long droppedFrames = droppedFramesProp.ulVal;

  // As the amount of rendered frame MUST increase monotonically. If the new
  // statistic data show the decreasing, which means the media engine has reset
  // the statistic data and started a new one. (That will happens after calling
  // flush internally)
  if (renderedFrames < mCurrentPlaybackStatisticData.renderedFrames()) {
    mPrevPlaybackStatisticData =
        StatisticData{mPrevPlaybackStatisticData.renderedFrames() +
                          mCurrentPlaybackStatisticData.renderedFrames(),
                      mPrevPlaybackStatisticData.droppedFrames() +
                          mCurrentPlaybackStatisticData.droppedFrames()};
    mCurrentPlaybackStatisticData = StatisticData{};
  }

  if (mCurrentPlaybackStatisticData.renderedFrames() != renderedFrames ||
      mCurrentPlaybackStatisticData.droppedFrames() != droppedFrames) {
    mCurrentPlaybackStatisticData =
        StatisticData{renderedFrames, droppedFrames};
    const uint64_t totalRenderedFrames =
        mPrevPlaybackStatisticData.renderedFrames() +
        mCurrentPlaybackStatisticData.renderedFrames();
    const uint64_t totalDroppedFrames =
        mPrevPlaybackStatisticData.droppedFrames() +
        mCurrentPlaybackStatisticData.droppedFrames();
    LOG("Update statistic data, rendered=%" PRIu64 ", dropped=%" PRIu64,
        totalRenderedFrames, totalDroppedFrames);
    Unused << SendUpdateStatisticData(
        StatisticData{totalRenderedFrames, totalDroppedFrames});
  }
}

#undef LOG
#undef RETURN_IF_FAILED
#undef ENSURE_EVENT_DISPATCH_DURING_PLAYING

}  // namespace mozilla
