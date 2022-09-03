/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineParent.h"

#include <audiosessiontypes.h>
#include <intsafe.h>
#include <mfapi.h>

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
  mMediaEngineNotify = nullptr;
  mMediaEngineExtension = nullptr;
  mMediaSource = nullptr;
  if (mMediaEngine) {
    mMediaEngine->Shutdown();
    mMediaEngine = nullptr;
  }
  mMediaEngineEventListener.DisconnectIfExists();
  mRequestSampleListener.DisconnectIfExists();
  if (mDXGIDeviceManager) {
    mDXGIDeviceManager = nullptr;
    wmf::MFUnlockDXGIDeviceManager();
  }
  if (mVirtualVideoWindow) {
    DestroyWindow(mVirtualVideoWindow);
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
  InitializeVirtualVideoWindow();

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
  // TODO : SET MF_MEDIA_ENGINE_CONTENT_PROTECTION_FLAGS
  if (mDXGIDeviceManager) {
    RETURN_VOID_IF_FAILED(creationAttributes->SetUnknown(
        MF_MEDIA_ENGINE_DXGI_MANAGER, mDXGIDeviceManager.Get()));
  }
  if (mVirtualVideoWindow) {
    RETURN_VOID_IF_FAILED(creationAttributes->SetUINT64(
        MF_MEDIA_ENGINE_OPM_HWND,
        reinterpret_cast<uint64_t>(mVirtualVideoWindow)));
  }

  ComPtr<IMFMediaEngineClassFactory> factory;
  RETURN_VOID_IF_FAILED(CoCreateInstance(CLSID_MFMediaEngineClassFactory,
                                         nullptr, CLSCTX_INPROC_SERVER,
                                         IID_PPV_ARGS(&factory)));
  const bool isLowLatency =
      StaticPrefs::media_wmf_low_latency_enabled() &&
      !StaticPrefs::media_wmf_low_latency_force_disabled();
  static const DWORD MF_MEDIA_ENGINE_DEFAULT = 0;
  RETURN_VOID_IF_FAILED(factory->CreateInstance(
      isLowLatency ? MF_MEDIA_ENGINE_REAL_TIME_MODE : MF_MEDIA_ENGINE_DEFAULT,
      creationAttributes.Get(), &mMediaEngine));

  // TODO : deal with encrypted content (set ContentProtectionManager and cdm
  // proxy)

  LOG("Created media engine successfully");
  mIsCreatedMediaEngine = true;
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

void MFMediaEngineParent::InitializeVirtualVideoWindow() {
  static ATOM sVideoWindowClass = 0;
  if (!sVideoWindowClass) {
    WNDCLASS wnd{};
    wnd.lpszClassName = L"MFMediaEngine";
    wnd.hInstance = nullptr;
    wnd.lpfnWndProc = DefWindowProc;
    sVideoWindowClass = RegisterClass(&wnd);
  }
  if (!sVideoWindowClass) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG("Failed to register video window class: %lX", hr);
    return;
  }
  mVirtualVideoWindow =
      CreateWindowEx(WS_EX_NOPARENTNOTIFY | WS_EX_LAYERED | WS_EX_TRANSPARENT |
                         WS_EX_NOREDIRECTIONBITMAP,
                     reinterpret_cast<wchar_t*>(sVideoWindowClass), L"",
                     WS_POPUP | WS_DISABLED | WS_CLIPSIBLINGS, 0, 0, 1, 1,
                     nullptr, nullptr, nullptr, nullptr);
  if (!mVirtualVideoWindow) {
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    LOG("Failed to create virtual window: %lX", hr);
    return;
  }
  LOG("Initialized virtual window");
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
    case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE:
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
    case MF_MEDIA_ENGINE_EVENT_PLAYING:
      Unused << SendNotifyEvent(aEvent.mEvent);
      break;
    case MF_MEDIA_ENGINE_EVENT_ENDED: {
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
  LOG("Create a media engine decoder for %s", TrackTypeToStr(aType));
  MOZ_ASSERT(mMediaSource);
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
    LOG("Play, in playback rate %f", mPlaybackRate);
    NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->Play()), IPC_OK());
    // The media engine has some undocumented behaviors for setting playback
    // rate, it will set the rate to 0 when it pauses, and set the rate to 1
    // when it starts. Therefore. we need to reset the targeted playback rate
    // everytime when we start playing if the rate is not 1.
    if (mPlaybackRate != 1.0) {
      NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->SetPlaybackRate(mPlaybackRate)),
                     IPC_OK());
    }
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
  NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->SetCurrentTime(aTargetTimeInSecond)),
                 IPC_OK());

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
  if (aPlaybackRate <= 0) {
    LOG("Not support zero or negative playback rate");
    return IPC_OK();
  }
  LOG("SetPlaybackRate=%f", aPlaybackRate);
  mPlaybackRate = aPlaybackRate;
  NS_ENSURE_TRUE(SUCCEEDED(mMediaEngine->SetPlaybackRate(mPlaybackRate)),
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

void MFMediaEngineParent::EnsureDcompSurfaceHandle() {
  AssertOnManagerThread();
  MOZ_ASSERT(mMediaEngine);
  MOZ_ASSERT(mMediaEngine->HasVideo());

  ComPtr<IMFMediaEngineEx> mediaEngineEx;
  RETURN_VOID_IF_FAILED(mMediaEngine.As(&mediaEngineEx));
  DWORD width, height;
  RETURN_VOID_IF_FAILED(mMediaEngine->GetNativeVideoSize(&width, &height));
  if (width != mDisplayWidth || height != mDisplayHeight) {
    // Update stream size before asking for a handle. If we don't update the
    // size, media engine will create the dcomp surface in a wrong size. If
    // the size isn't changed, then we don't need to recreate the surface.
    mDisplayWidth = width;
    mDisplayHeight = height;
    RECT rect = {0, 0, (LONG)mDisplayWidth, (LONG)mDisplayHeight};
    RETURN_VOID_IF_FAILED(mediaEngineEx->UpdateVideoStream(
        nullptr /* pSrc */, &rect, nullptr /* pBorderClr */));
    LOG("Updated video size for engine=[%lux%lu]", mDisplayWidth,
        mDisplayHeight);
  }

  if (!mIsEnableDcompMode) {
    RETURN_VOID_IF_FAILED(mediaEngineEx->EnableWindowlessSwapchainMode(true));
    LOG("Enabled dcomp swap chain mode");
    mIsEnableDcompMode = true;
  }

  HANDLE surfaceHandle = INVALID_HANDLE_VALUE;
  RETURN_VOID_IF_FAILED(mediaEngineEx->GetVideoSwapchainHandle(&surfaceHandle));
  if (surfaceHandle && surfaceHandle != INVALID_HANDLE_VALUE) {
    LOG("EnsureDcompSurfaceHandle, handle=%p, size=[%lux%lu]", surfaceHandle,
        width, height);
    mMediaSource->SetDCompSurfaceHandle(surfaceHandle);
  } else {
    NS_WARNING("SurfaceHandle is not ready yet");
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

}  // namespace mozilla
