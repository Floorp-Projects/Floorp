/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFVideoMFTManager.h"

#include <psapi.h>
#include <winsdkver.h>
#include <algorithm>
#include "DXVA2Manager.h"
#include "GMPUtils.h"  // For SplitAt. TODO: Move SplitAt to a central place.
#include "IMFYCbCrImage.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "MediaInfo.h"
#include "MediaTelemetryConstants.h"
#include "VideoUtils.h"
#include "WMFDecoderModule.h"
#include "WMFUtils.h"
#include "gfx2DGlue.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "nsWindowsHelpers.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

using mozilla::layers::Image;
using mozilla::layers::IMFYCbCrImage;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;
using mozilla::media::TimeUnit;

#if WINVER_MAXVER < 0x0A00
// Windows 10+ SDK has VP80 and VP90 defines
const GUID MFVideoFormat_VP80 = {
    0x30385056,
    0x0000,
    0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

const GUID MFVideoFormat_VP90 = {
    0x30395056,
    0x0000,
    0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};
#endif

#if !defined(__MINGW32__) && _WIN32_WINNT < _WIN32_WINNT_WIN8
const GUID MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT = {
    0x851745d5,
    0xc3d6,
    0x476d,
    {0x95, 0x27, 0x49, 0x8e, 0xf2, 0xd1, 0xd, 0x18}};
const GUID MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT_PROGRESSIVE = {
    0xf5523a5,
    0x1cb2,
    0x47c5,
    {0xa5, 0x50, 0x2e, 0xeb, 0x84, 0xb4, 0xd1, 0x4a}};
const GUID MF_SA_D3D11_BINDFLAGS = {
    0xeacf97ad,
    0x065c,
    0x4408,
    {0xbe, 0xe3, 0xfd, 0xcb, 0xfd, 0x12, 0x8b, 0xe2}};
const GUID MF_SA_D3D11_SHARED_WITHOUT_MUTEX = {
    0x39dbd44d,
    0x2e44,
    0x4931,
    {0xa4, 0xc8, 0x35, 0x2d, 0x3d, 0xc4, 0x21, 0x15}};
#endif

namespace mozilla {

static bool IsWin7H264Decoder4KCapable() {
  WCHAR systemPath[MAX_PATH + 1];
  if (!ConstructSystem32Path(L"msmpeg2vdec.dll", systemPath, MAX_PATH + 1)) {
    // Cannot build path -> Assume it's the old DLL or it's missing.
    return false;
  }

  DWORD zero;
  DWORD infoSize = GetFileVersionInfoSizeW(systemPath, &zero);
  if (infoSize == 0) {
    // Can't get file info -> Assume it's the old DLL or it's missing.
    return false;
  }
  auto infoData = MakeUnique<unsigned char[]>(infoSize);
  VS_FIXEDFILEINFO* vInfo;
  UINT vInfoLen;
  if (GetFileVersionInfoW(systemPath, 0, infoSize, infoData.get()) &&
      VerQueryValueW(infoData.get(), L"\\", (LPVOID*)&vInfo, &vInfoLen)) {
    uint64_t version = uint64_t(vInfo->dwFileVersionMS) << 32 |
                       uint64_t(vInfo->dwFileVersionLS);
    // 12.0.9200.16426 & later allow for >1920x1088 resolutions.
    const uint64_t minimum =
        (uint64_t(12) << 48) | (uint64_t(9200) << 16) | uint64_t(16426);
    return version >= minimum;
  }
  // Can't get file version -> Assume it's the old DLL.
  return false;
}

LayersBackend GetCompositorBackendType(
    layers::KnowsCompositor* aKnowsCompositor) {
  if (aKnowsCompositor) {
    return aKnowsCompositor->GetCompositorBackendType();
  }
  return LayersBackend::LAYERS_NONE;
}

WMFVideoMFTManager::WMFVideoMFTManager(
    const VideoInfo& aConfig, layers::KnowsCompositor* aKnowsCompositor,
    layers::ImageContainer* aImageContainer, float aFramerate,
    const CreateDecoderParams::OptionSet& aOptions, bool aDXVAEnabled)
    : mVideoInfo(aConfig),
      mImageSize(aConfig.mImage),
      mStreamType(GetStreamTypeFromMimeType(aConfig.mMimeType)),
      mDecodedImageSize(aConfig.mImage),
      mVideoStride(0),
      mColorSpace(aConfig.mColorSpace),
      mColorRange(aConfig.mColorRange),
      mImageContainer(aImageContainer),
      mKnowsCompositor(aKnowsCompositor),
      mDXVAEnabled(aDXVAEnabled &&
                   !aOptions.contains(
                       CreateDecoderParams::Option::HardwareDecoderNotAllowed)),
      mZeroCopyNV12Texture(false),
      mFramerate(aFramerate),
      mLowLatency(aOptions.contains(CreateDecoderParams::Option::LowLatency))
// mVideoStride, mVideoWidth, mVideoHeight, mUseHwAccel are initialized in
// Init().
{
  MOZ_COUNT_CTOR(WMFVideoMFTManager);

  // The V and U planes are stored 16-row-aligned, so we need to add padding
  // to the row heights to ensure the Y'CbCr planes are referenced properly.
  // This value is only used with software decoder.
  if (mDecodedImageSize.height % 16 != 0) {
    mDecodedImageSize.height += 16 - (mDecodedImageSize.height % 16);
  }
}

WMFVideoMFTManager::~WMFVideoMFTManager() {
  MOZ_COUNT_DTOR(WMFVideoMFTManager);
}

/* static */
const GUID& WMFVideoMFTManager::GetMediaSubtypeGUID() {
  MOZ_ASSERT(StreamTypeIsVideo(mStreamType));
  switch (mStreamType) {
    case WMFStreamType::H264:
      return MFVideoFormat_H264;
    case WMFStreamType::VP8:
      return MFVideoFormat_VP80;
    case WMFStreamType::VP9:
      return MFVideoFormat_VP90;
    case WMFStreamType::AV1:
      return MFVideoFormat_AV1;
    default:
      return GUID_NULL;
  };
}

bool WMFVideoMFTManager::InitializeDXVA() {
  // If we use DXVA but aren't running with a D3D layer manager then the
  // readback of decoded video frames from GPU to CPU memory grinds painting
  // to a halt, and makes playback performance *worse*.
  if (!mDXVAEnabled) {
    mDXVAFailureReason.AssignLiteral(
        "Hardware video decoding disabled or blacklisted");
    return false;
  }
  MOZ_ASSERT(!mDXVA2Manager);
  if (!mKnowsCompositor || !mKnowsCompositor->SupportsD3D11()) {
    mDXVAFailureReason.AssignLiteral("Unsupported layers backend");
    return false;
  }

  if (!XRE_IsRDDProcess() && !XRE_IsGPUProcess()) {
    mDXVAFailureReason.AssignLiteral(
        "DXVA only supported in RDD or GPU process");
    return false;
  }

  bool d3d11 = true;
  if (!StaticPrefs::media_wmf_dxva_d3d11_enabled()) {
    mDXVAFailureReason = nsPrintfCString(
        "D3D11: %s is false",
        StaticPrefs::GetPrefName_media_wmf_dxva_d3d11_enabled());
    d3d11 = false;
  }
  if (!IsWin8OrLater()) {
    mDXVAFailureReason.AssignLiteral("D3D11: Requires Windows 8 or later");
    d3d11 = false;
  }

  if (d3d11) {
    mDXVAFailureReason.AppendLiteral("D3D11: ");
    mDXVA2Manager.reset(
        DXVA2Manager::CreateD3D11DXVA(mKnowsCompositor, mDXVAFailureReason));
    if (mDXVA2Manager) {
      return true;
    }
  }

  // Try again with d3d9, but record the failure reason
  // into a new var to avoid overwriting the d3d11 failure.
  nsAutoCString d3d9Failure;
  mDXVA2Manager.reset(
      DXVA2Manager::CreateD3D9DXVA(mKnowsCompositor, d3d9Failure));
  // Make sure we include the messages from both attempts (if applicable).
  if (!d3d9Failure.IsEmpty()) {
    mDXVAFailureReason.AppendLiteral("; D3D9: ");
    mDXVAFailureReason.Append(d3d9Failure);
  }

  return mDXVA2Manager != nullptr;
}

MediaResult WMFVideoMFTManager::ValidateVideoInfo() {
  NS_ENSURE_TRUE(StreamTypeIsVideo(mStreamType),
                 MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                             RESULT_DETAIL("Invalid stream type")));
  switch (mStreamType) {
    case WMFStreamType::H264:
      if (!StaticPrefs::media_wmf_allow_unsupported_resolutions()) {
        // The WMF H.264 decoder is documented to have a minimum resolution
        // 48x48 pixels for resolution, but we won't enable hw decoding for the
        // resolution < 132 pixels. It's assumed the software decoder doesn't
        // have this limitation, but it still might have maximum resolution
        // limitation.
        // https://msdn.microsoft.com/en-us/library/windows/desktop/dd797815(v=vs.85).aspx
        const bool Is4KCapable =
            IsWin8OrLater() || IsWin7H264Decoder4KCapable();
        static const int32_t MAX_H264_PIXEL_COUNT =
            Is4KCapable ? 4096 * 2304 : 1920 * 1088;
        const CheckedInt32 pixelCount =
            CheckedInt32(mVideoInfo.mImage.width) * mVideoInfo.mImage.height;

        if (!pixelCount.isValid() ||
            pixelCount.value() > MAX_H264_PIXEL_COUNT) {
          mIsValid = false;
          return MediaResult(
              NS_ERROR_DOM_MEDIA_FATAL_ERR,
              RESULT_DETAIL("Can't decode H.264 stream because its "
                            "resolution is out of the maximum limitation"));
        }
      }
      break;
    default:
      break;
  }

  return NS_OK;
}

MediaResult WMFVideoMFTManager::Init() {
  MediaResult result = ValidateVideoInfo();
  if (NS_FAILED(result)) {
    return result;
  }

  result = InitInternal();
  if (NS_SUCCEEDED(result) && mDXVA2Manager) {
    // If we had some failures but eventually made it work,
    // make sure we preserve the messages.
    if (mDXVA2Manager->IsD3D11()) {
      mDXVAFailureReason.AppendLiteral("Using D3D11 API");
    } else {
      mDXVAFailureReason.AppendLiteral("Using D3D9 API");
    }
  }

  return result;
}

MediaResult WMFVideoMFTManager::InitInternal() {
  // The H264 SanityTest uses a 132x132 videos to determine if DXVA can be used.
  // so we want to use the software decoder for videos with lower resolutions.
  static const int MIN_H264_HW_WIDTH = 132;
  static const int MIN_H264_HW_HEIGHT = 132;

  mUseHwAccel = false;  // default value; changed if D3D setup succeeds.
  bool useDxva = true;

  if (mStreamType == WMFStreamType::H264 &&
      (mVideoInfo.ImageRect().width <= MIN_H264_HW_WIDTH ||
       mVideoInfo.ImageRect().height <= MIN_H264_HW_HEIGHT)) {
    useDxva = false;
    mDXVAFailureReason = nsPrintfCString(
        "H264 video resolution too low: %" PRIu32 "x%" PRIu32,
        mVideoInfo.ImageRect().width, mVideoInfo.ImageRect().height);
  }

  if (useDxva) {
    useDxva = InitializeDXVA();
  }

  RefPtr<MFTDecoder> decoder = new MFTDecoder();
  HRESULT hr = WMFDecoderModule::CreateMFTDecoder(mStreamType, decoder);
  NS_ENSURE_TRUE(SUCCEEDED(hr),
                 MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                             RESULT_DETAIL("Can't create the MFT decoder.")));

  RefPtr<IMFAttributes> attr(decoder->GetAttributes());
  UINT32 aware = 0;
  if (attr) {
    attr->GetUINT32(MF_SA_D3D_AWARE, &aware);
    attr->SetUINT32(CODECAPI_AVDecNumWorkerThreads,
                    WMFDecoderModule::GetNumDecoderThreads());
    bool lowLatency =
        (StaticPrefs::media_wmf_low_latency_enabled() || IsWin10OrLater()) &&
        !StaticPrefs::media_wmf_low_latency_force_disabled();
    if (mLowLatency || lowLatency) {
      hr = attr->SetUINT32(CODECAPI_AVLowLatencyMode, TRUE);
      if (SUCCEEDED(hr)) {
        LOG("Enabling Low Latency Mode");
      } else {
        LOG("Couldn't enable Low Latency Mode");
      }
    }

    if (gfxVars::HwDecodedVideoZeroCopy() && mKnowsCompositor &&
        mKnowsCompositor->UsingHardwareWebRender() && mDXVA2Manager &&
        mDXVA2Manager->SupportsZeroCopyNV12Texture()) {
      mZeroCopyNV12Texture = true;
      const int kOutputBufferSize = 10;

      // Each picture buffer can store a sample, plus one in
      // pending_output_samples_. The decoder adds this number to the number of
      // reference pictures it expects to need and uses that to determine the
      // array size of the output texture.
      const int kMaxOutputSamples = kOutputBufferSize + 1;
      attr->SetUINT32(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT_PROGRESSIVE,
                      kMaxOutputSamples);
      attr->SetUINT32(MF_SA_MINIMUM_OUTPUT_SAMPLE_COUNT, kMaxOutputSamples);
    }
  }

  if (useDxva) {
    if (aware) {
      // TODO: Test if I need this anywhere... Maybe on Vista?
      // hr = attr->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE);
      // NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
      MOZ_ASSERT(mDXVA2Manager);
      ULONG_PTR manager = ULONG_PTR(mDXVA2Manager->GetDXVADeviceManager());
      hr = decoder->SendMFTMessage(MFT_MESSAGE_SET_D3D_MANAGER, manager);
      if (SUCCEEDED(hr)) {
        mUseHwAccel = true;
      } else {
        mDXVAFailureReason = nsPrintfCString(
            "MFT_MESSAGE_SET_D3D_MANAGER failed with code %lX", hr);
      }
    } else {
      mDXVAFailureReason.AssignLiteral(
          "Decoder returned false for MF_SA_D3D_AWARE");
    }
  }

  if (!mDXVAFailureReason.IsEmpty()) {
    // DXVA failure reason being set can mean that D3D11 failed, or that DXVA is
    // entirely disabled.
    LOG("DXVA failure: %s", mDXVAFailureReason.get());
  }

  if (!mUseHwAccel) {
    if (mDXVA2Manager) {
      // Either mDXVAEnabled was set to false prior the second call to
      // InitInternal() due to CanUseDXVA() returning false, or
      // MFT_MESSAGE_SET_D3D_MANAGER failed
      mDXVA2Manager.reset();
    }
    if (mStreamType == WMFStreamType::VP9 ||
        mStreamType == WMFStreamType::VP8 ||
        mStreamType == WMFStreamType::AV1) {
      return MediaResult(
          NS_ERROR_DOM_MEDIA_FATAL_ERR,
          RESULT_DETAIL("Use VP8/VP9/AV1 MFT only if HW acceleration "
                        "is available."));
    }
    Telemetry::Accumulate(Telemetry::MEDIA_DECODER_BACKEND_USED,
                          uint32_t(media::MediaDecoderBackend::WMFSoftware));
  }

  mDecoder = decoder;
  hr = SetDecoderMediaTypes();
  NS_ENSURE_TRUE(
      SUCCEEDED(hr),
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Fail to set the decoder media types.")));

  RefPtr<IMFMediaType> inputType;
  hr = mDecoder->GetInputMediaType(inputType);
  NS_ENSURE_TRUE(
      SUCCEEDED(hr),
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Fail to get the input media type.")));

  RefPtr<IMFMediaType> outputType;
  hr = mDecoder->GetOutputMediaType(outputType);
  NS_ENSURE_TRUE(
      SUCCEEDED(hr),
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Fail to get the output media type.")));

  if (mUseHwAccel && !CanUseDXVA(inputType, outputType)) {
    LOG("DXVA manager determined that the input type was unsupported in "
        "hardware, retrying init without DXVA.");
    mDXVAEnabled = false;
    // DXVA initialization with current decoder actually failed,
    // re-do initialization.
    return InitInternal();
  }

  LOG("Video Decoder initialized, Using DXVA: %s",
      (mUseHwAccel ? "Yes" : "No"));

  if (mUseHwAccel) {
    hr = mDXVA2Manager->ConfigureForSize(
        outputType,
        mColorSpace.refOr(
            DefaultColorSpace({mImageSize.width, mImageSize.height})),
        mColorRange, mVideoInfo.ImageRect().width,
        mVideoInfo.ImageRect().height);
    NS_ENSURE_TRUE(SUCCEEDED(hr),
                   MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                               RESULT_DETAIL("Fail to configure image size for "
                                             "DXVA2Manager.")));
  } else {
    GetDefaultStride(outputType, mVideoInfo.ImageRect().width, &mVideoStride);
  }
  LOG("WMFVideoMFTManager frame geometry stride=%u picture=(%d, %d, %d, %d) "
      "display=(%d,%d)",
      mVideoStride, mVideoInfo.ImageRect().x, mVideoInfo.ImageRect().y,
      mVideoInfo.ImageRect().width, mVideoInfo.ImageRect().height,
      mVideoInfo.mDisplay.width, mVideoInfo.mDisplay.height);

  if (!mUseHwAccel) {
    RefPtr<ID3D11Device> device = gfx::DeviceManagerDx::Get()->GetImageDevice();
    if (device) {
      mIMFUsable = true;
    }
  }
  return MediaResult(NS_OK);
}

HRESULT
WMFVideoMFTManager::SetDecoderMediaTypes() {
  // Setup the input/output media types.
  RefPtr<IMFMediaType> inputType;
  HRESULT hr = wmf::MFCreateMediaType(getter_AddRefs(inputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetGUID(MF_MT_SUBTYPE, GetMediaSubtypeGUID());
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetUINT32(MF_MT_INTERLACE_MODE,
                            MFVideoInterlace_MixedInterlaceOrProgressive);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = MFSetAttributeSize(inputType, MF_MT_FRAME_SIZE,
                          mVideoInfo.ImageRect().width,
                          mVideoInfo.ImageRect().height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  UINT32 fpsDenominator = 1000;
  UINT32 fpsNumerator = static_cast<uint32_t>(mFramerate * fpsDenominator);
  hr = MFSetAttributeRatio(inputType, MF_MT_FRAME_RATE, fpsNumerator,
                           fpsDenominator);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaType> outputType;
  hr = wmf::MFCreateMediaType(getter_AddRefs(outputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = MFSetAttributeSize(outputType, MF_MT_FRAME_SIZE,
                          mVideoInfo.ImageRect().width,
                          mVideoInfo.ImageRect().height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = MFSetAttributeRatio(outputType, MF_MT_FRAME_RATE, fpsNumerator,
                           fpsDenominator);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  GUID outputSubType = [&]() {
    switch (mVideoInfo.mColorDepth) {
      case gfx::ColorDepth::COLOR_8:
        return mUseHwAccel ? MFVideoFormat_NV12 : MFVideoFormat_YV12;
      case gfx::ColorDepth::COLOR_10:
        return MFVideoFormat_P010;
      case gfx::ColorDepth::COLOR_12:
      case gfx::ColorDepth::COLOR_16:
        return MFVideoFormat_P016;
      default:
        MOZ_ASSERT_UNREACHABLE("Unexpected color depth");
    }
  }();
  hr = outputType->SetGUID(MF_MT_SUBTYPE, outputSubType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  if (mZeroCopyNV12Texture) {
    RefPtr<IMFAttributes> attr(mDecoder->GetOutputStreamAttributes());
    if (attr) {
      hr = attr->SetUINT32(MF_SA_D3D11_SHARED_WITHOUT_MUTEX, TRUE);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      hr = attr->SetUINT32(MF_SA_D3D11_BINDFLAGS,
                           D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_DECODER);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    }
  }

  return mDecoder->SetMediaTypes(inputType, outputType);
}

HRESULT
WMFVideoMFTManager::Input(MediaRawData* aSample) {
  if (!mIsValid) {
    return E_FAIL;
  }

  if (!mDecoder) {
    // This can happen during shutdown.
    return E_FAIL;
  }

  RefPtr<IMFSample> inputSample;
  HRESULT hr = mDecoder->CreateInputSample(
      aSample->Data(), uint32_t(aSample->Size()),
      aSample->mTime.ToMicroseconds(), aSample->mDuration.ToMicroseconds(),
      &inputSample);
  NS_ENSURE_TRUE(SUCCEEDED(hr) && inputSample != nullptr, hr);

  if (!mColorSpace && aSample->mTrackInfo) {
    // The colorspace definition is found in the H264 SPS NAL, available out of
    // band, while for VP9 it's only available within the VP9 bytestream.
    // The info would have been updated by the MediaChangeMonitor.
    mColorSpace = aSample->mTrackInfo->GetAsVideoInfo()->mColorSpace;
    mColorRange = aSample->mTrackInfo->GetAsVideoInfo()->mColorRange;
  }
  mLastDuration = aSample->mDuration;

  // Forward sample data to the decoder.
  return mDecoder->Input(inputSample);
}

// The MFTransforms we use for decoding H264 and AV1 video will silently fall
// back to software decoding (even if we've negotiated DXVA) if the GPU
// doesn't support decoding the given codec and resolution. It will then upload
// the software decoded frames into d3d textures to preserve behaviour.
//
// Unfortunately this seems to cause corruption (see bug 1193547) and is
// slow because the upload is done into a non-shareable texture and requires
// us to copy it.
//
// This code tests if the given codec and resolution can be supported directly
// on the GPU, and makes sure we only ask the MFT for DXVA if it can be
// supported properly.
//
// Ideally we'd know the framerate during initialization and would also ensure
// that new decoders are created if the resolution changes. Then we could move
// this check into Init and consolidate the main thread blocking code.
bool WMFVideoMFTManager::CanUseDXVA(IMFMediaType* aInputType,
                                    IMFMediaType* aOutputType) {
  MOZ_ASSERT(mDXVA2Manager);
  // Check if we're able to use hardware decoding for the current codec config.
  return mDXVA2Manager->SupportsConfig(mVideoInfo, aInputType, aOutputType);
}

TimeUnit WMFVideoMFTManager::GetSampleDurationOrLastKnownDuration(
    IMFSample* aSample) const {
  TimeUnit duration = GetSampleDuration(aSample);
  if (!duration.IsValid()) {
    // WMF returned a non-success code (likely duration unknown, but the API
    // also allows for other, unspecified codes).
    LOG("Got unknown sample duration -- bad return code. Using mLastDuration.");
  } else if (duration == TimeUnit::Zero()) {
    // Duration is zero. WMF uses this to indicate an unknown duration.
    LOG("Got unknown sample duration -- zero duration returned. Using "
        "mLastDuration.");
  } else if (duration.IsNegative()) {
    // A negative duration will cause issues up the stack. It's also unclear
    // why this would happen, but the API allows for it by returning a signed
    // int, so we handle it here.
    LOG("Got negative sample duration: %f seconds. Using mLastDuration "
        "instead.",
        duration.ToSeconds());
  } else {
    // We got a duration without any problems.
    return duration;
  }

  return mLastDuration;
}

HRESULT
WMFVideoMFTManager::CreateBasicVideoFrame(IMFSample* aSample,
                                          int64_t aStreamOffset,
                                          VideoData** aOutVideoData) {
  NS_ENSURE_TRUE(aSample, E_POINTER);
  NS_ENSURE_TRUE(aOutVideoData, E_POINTER);

  *aOutVideoData = nullptr;

  HRESULT hr;
  RefPtr<IMFMediaBuffer> buffer;

  // Must convert to contiguous buffer to use IMD2DBuffer interface.
  hr = aSample->ConvertToContiguousBuffer(getter_AddRefs(buffer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // Try and use the IMF2DBuffer interface if available, otherwise fallback
  // to the IMFMediaBuffer interface. Apparently IMF2DBuffer is more efficient,
  // but only some systems (Windows 8?) support it.
  BYTE* data = nullptr;
  LONG stride = 0;
  RefPtr<IMF2DBuffer> twoDBuffer;
  hr = buffer->QueryInterface(
      static_cast<IMF2DBuffer**>(getter_AddRefs(twoDBuffer)));
  if (SUCCEEDED(hr)) {
    hr = twoDBuffer->Lock2D(&data, &stride);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  } else {
    hr = buffer->Lock(&data, nullptr, nullptr);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    stride = mVideoStride;
  }

  const GUID& subType = mDecoder->GetOutputMediaSubType();
  MOZ_DIAGNOSTIC_ASSERT(subType == MFVideoFormat_YV12 ||
                        subType == MFVideoFormat_P010 ||
                        subType == MFVideoFormat_P016);
  const gfx::ColorDepth colorDepth = subType == MFVideoFormat_YV12
                                         ? gfx::ColorDepth::COLOR_8
                                         : gfx::ColorDepth::COLOR_16;

  // YV12, planar format (3 planes): [YYYY....][VVVV....][UUUU....]
  // i.e., Y, then V, then U.
  // P010, P016 planar format (2 planes) [YYYY....][UVUV...]
  // See
  // https://docs.microsoft.com/en-us/windows/desktop/medfound/10-bit-and-16-bit-yuv-video-formats
  VideoData::YCbCrBuffer b;

  uint32_t videoWidth = mImageSize.width;
  uint32_t videoHeight = mImageSize.height;

  // Y (Y') plane
  b.mPlanes[0].mData = data;
  b.mPlanes[0].mStride = stride;
  b.mPlanes[0].mHeight = videoHeight;
  b.mPlanes[0].mWidth = videoWidth;
  b.mPlanes[0].mSkip = 0;

  MOZ_DIAGNOSTIC_ASSERT(mDecodedImageSize.height % 16 == 0,
                        "decoded height must be 16 bytes aligned");
  uint32_t y_size = stride * mDecodedImageSize.height;
  uint32_t v_size = stride * mDecodedImageSize.height / 4;
  uint32_t halfStride = (stride + 1) / 2;
  uint32_t halfHeight = (videoHeight + 1) / 2;
  uint32_t halfWidth = (videoWidth + 1) / 2;

  if (subType == MFVideoFormat_YV12) {
    // U plane (Cb)
    b.mPlanes[1].mData = data + y_size + v_size;
    b.mPlanes[1].mStride = halfStride;
    b.mPlanes[1].mHeight = halfHeight;
    b.mPlanes[1].mWidth = halfWidth;
    b.mPlanes[1].mSkip = 0;

    // V plane (Cr)
    b.mPlanes[2].mData = data + y_size;
    b.mPlanes[2].mStride = halfStride;
    b.mPlanes[2].mHeight = halfHeight;
    b.mPlanes[2].mWidth = halfWidth;
    b.mPlanes[2].mSkip = 0;
  } else {
    // U plane (Cb)
    b.mPlanes[1].mData = data + y_size;
    b.mPlanes[1].mStride = stride;
    b.mPlanes[1].mHeight = halfHeight;
    b.mPlanes[1].mWidth = halfWidth;
    b.mPlanes[1].mSkip = 1;

    // V plane (Cr)
    b.mPlanes[2].mData = data + y_size + sizeof(short);
    b.mPlanes[2].mStride = stride;
    b.mPlanes[2].mHeight = halfHeight;
    b.mPlanes[2].mWidth = halfWidth;
    b.mPlanes[2].mSkip = 1;
  }

  b.mChromaSubsampling = gfx::ChromaSubsampling::HALF_WIDTH_AND_HEIGHT;

  // YuvColorSpace
  b.mYUVColorSpace =
      mColorSpace.refOr(DefaultColorSpace({videoWidth, videoHeight}));
  b.mColorDepth = colorDepth;
  b.mColorRange = mColorRange;

  TimeUnit pts = GetSampleTime(aSample);
  NS_ENSURE_TRUE(pts.IsValid(), E_FAIL);
  TimeUnit duration = GetSampleDurationOrLastKnownDuration(aSample);
  NS_ENSURE_TRUE(duration.IsValid(), E_FAIL);
  gfx::IntRect pictureRegion =
      mVideoInfo.ScaledImageRect(videoWidth, videoHeight);

  if (colorDepth != gfx::ColorDepth::COLOR_8 || !mKnowsCompositor ||
      !mKnowsCompositor->SupportsD3D11() || !mIMFUsable) {
    RefPtr<VideoData> v = VideoData::CreateAndCopyData(
        mVideoInfo, mImageContainer, aStreamOffset, pts, duration, b, false,
        TimeUnit::FromMicroseconds(-1), pictureRegion, mKnowsCompositor);
    if (twoDBuffer) {
      twoDBuffer->Unlock2D();
    } else {
      buffer->Unlock();
    }
    v.forget(aOutVideoData);
    return S_OK;
  }

  RefPtr<layers::PlanarYCbCrImage> image =
      new IMFYCbCrImage(buffer, twoDBuffer, mKnowsCompositor, mImageContainer);

  VideoData::SetVideoDataToImage(image, mVideoInfo, b, pictureRegion, false);

  RefPtr<VideoData> v = VideoData::CreateFromImage(
      mVideoInfo.mDisplay, aStreamOffset, pts, duration, image.forget(), false,
      TimeUnit::FromMicroseconds(-1));

  v.forget(aOutVideoData);
  return S_OK;
}

HRESULT
WMFVideoMFTManager::CreateD3DVideoFrame(IMFSample* aSample,
                                        int64_t aStreamOffset,
                                        VideoData** aOutVideoData) {
  NS_ENSURE_TRUE(aSample, E_POINTER);
  NS_ENSURE_TRUE(aOutVideoData, E_POINTER);
  NS_ENSURE_TRUE(mDXVA2Manager, E_ABORT);
  NS_ENSURE_TRUE(mUseHwAccel, E_ABORT);

  *aOutVideoData = nullptr;
  HRESULT hr;

  gfx::IntRect pictureRegion =
      mVideoInfo.ScaledImageRect(mImageSize.width, mImageSize.height);
  RefPtr<Image> image;
  if (mZeroCopyNV12Texture && mDXVA2Manager->SupportsZeroCopyNV12Texture()) {
    hr = mDXVA2Manager->WrapTextureWithImage(aSample, pictureRegion,
                                             getter_AddRefs(image));
  } else {
    hr = mDXVA2Manager->CopyToImage(aSample, pictureRegion,
                                    getter_AddRefs(image));
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  }
  NS_ENSURE_TRUE(image, E_FAIL);

  TimeUnit pts = GetSampleTime(aSample);
  NS_ENSURE_TRUE(pts.IsValid(), E_FAIL);
  TimeUnit duration = GetSampleDurationOrLastKnownDuration(aSample);
  NS_ENSURE_TRUE(duration.IsValid(), E_FAIL);
  RefPtr<VideoData> v = VideoData::CreateFromImage(
      mVideoInfo.mDisplay, aStreamOffset, pts, duration, image.forget(), false,
      TimeUnit::FromMicroseconds(-1));

  NS_ENSURE_TRUE(v, E_FAIL);
  v.forget(aOutVideoData);

  return S_OK;
}

// Blocks until decoded sample is produced by the decoder.
HRESULT
WMFVideoMFTManager::Output(int64_t aStreamOffset, RefPtr<MediaData>& aOutData) {
  RefPtr<IMFSample> sample;
  HRESULT hr;
  aOutData = nullptr;
  int typeChangeCount = 0;

  // Loop until we decode a sample, or an unexpected error that we can't
  // handle occurs.
  while (true) {
    hr = mDecoder->Output(&sample);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      MOZ_ASSERT(!sample);
      // Video stream output type change, probably geometric aperture change or
      // pixel type.
      // We must reconfigure the decoder output type.

      // Attempt to find an appropriate OutputType, trying in order:
      // if HW accelerated: NV12, P010, P016
      // if SW: YV12, P010, P016
      if (FAILED(
              (hr = (mDecoder->FindDecoderOutputTypeWithSubtype(
                   mUseHwAccel ? MFVideoFormat_NV12 : MFVideoFormat_YV12)))) &&
          FAILED((hr = mDecoder->FindDecoderOutputTypeWithSubtype(
                      MFVideoFormat_P010))) &&
          FAILED((hr = mDecoder->FindDecoderOutputTypeWithSubtype(
                      MFVideoFormat_P016)))) {
        LOG("No suitable output format found");
        return hr;
      }

      RefPtr<IMFMediaType> outputType;
      hr = mDecoder->GetOutputMediaType(outputType);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      if (mUseHwAccel) {
        hr = mDXVA2Manager->ConfigureForSize(
            outputType,
            mColorSpace.refOr(
                DefaultColorSpace({mImageSize.width, mImageSize.height})),
            mColorRange, mVideoInfo.ImageRect().width,
            mVideoInfo.ImageRect().height);
        NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
      } else {
        // The stride may have changed, recheck for it.
        hr = GetDefaultStride(outputType, mVideoInfo.ImageRect().width,
                              &mVideoStride);
        NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

        UINT32 width = 0, height = 0;
        hr = MFGetAttributeSize(outputType, MF_MT_FRAME_SIZE, &width, &height);
        NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
        NS_ENSURE_TRUE(width <= MAX_VIDEO_WIDTH, E_FAIL);
        NS_ENSURE_TRUE(height <= MAX_VIDEO_HEIGHT, E_FAIL);
        mDecodedImageSize = gfx::IntSize(width, height);
      }
      // Catch infinite loops, but some decoders perform at least 2 stream
      // changes on consecutive calls, so be permissive.
      // 100 is arbitrarily > 2.
      NS_ENSURE_TRUE(typeChangeCount < 100, MF_E_TRANSFORM_STREAM_CHANGE);
      // Loop back and try decoding again...
      ++typeChangeCount;
      continue;
    }

    if (SUCCEEDED(hr)) {
      if (!sample) {
        LOG("Video MFTDecoder returned success but no output!");
        // On some machines/input the MFT returns success but doesn't output
        // a video frame. If we detect this, try again, but only up to a
        // point; after 250 failures, give up. Note we count all failures
        // over the life of the decoder, as we may end up exiting with a
        // NEED_MORE_INPUT and coming back to hit the same error. So just
        // counting with a local variable (like typeChangeCount does) may
        // not work in this situation.
        ++mNullOutputCount;
        if (mNullOutputCount > 250) {
          LOG("Excessive Video MFTDecoder returning success but no output; "
              "giving up");
          mGotExcessiveNullOutput = true;
          return E_FAIL;
        }
        continue;
      }
      TimeUnit pts = GetSampleTime(sample);
      TimeUnit duration = GetSampleDurationOrLastKnownDuration(sample);

      // AV1 MFT fix: Sample duration after seeking is always equal to the
      // sample time, for some reason. Set it to last duration instead.
      if (mStreamType == WMFStreamType::AV1 && duration == pts) {
        LOG("Video sample duration (%" PRId64 ") matched timestamp (%" PRId64
            "), setting to previous sample duration (%" PRId64 ") instead.",
            pts.ToMicroseconds(), duration.ToMicroseconds(),
            mLastDuration.ToMicroseconds());
        duration = mLastDuration;
        sample->SetSampleDuration(UsecsToHNs(duration.ToMicroseconds()));
      }

      if (!pts.IsValid() || !duration.IsValid()) {
        return E_FAIL;
      }
      if (mSeekTargetThreshold.isSome()) {
        if ((pts + duration) < mSeekTargetThreshold.ref()) {
          LOG("Dropping video frame which pts (%" PRId64 " + %" PRId64
              ") is smaller than seek target (%" PRId64 ").",
              pts.ToMicroseconds(), duration.ToMicroseconds(),
              mSeekTargetThreshold->ToMicroseconds());
          // It is necessary to clear the pointer to release the previous output
          // buffer.
          sample = nullptr;
          continue;
        }
        mSeekTargetThreshold.reset();
      }
      break;
    }
    // Else unexpected error so bail.
    NS_WARNING("WMFVideoMFTManager::Output() unexpected error");
    return hr;
  }

  RefPtr<VideoData> frame;
  if (mUseHwAccel) {
    hr = CreateD3DVideoFrame(sample, aStreamOffset, getter_AddRefs(frame));
  } else {
    hr = CreateBasicVideoFrame(sample, aStreamOffset, getter_AddRefs(frame));
  }
  // Frame should be non null only when we succeeded.
  MOZ_ASSERT((frame != nullptr) == SUCCEEDED(hr));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  NS_ENSURE_TRUE(frame, E_FAIL);

  aOutData = frame;

  if (mNullOutputCount) {
    mGotValidOutputAfterNullOutput = true;
  }

  return S_OK;
}

void WMFVideoMFTManager::Shutdown() {
  if (mDXVA2Manager) {
    mDXVA2Manager->BeforeShutdownVideoMFTDecoder();
  }
  mDecoder = nullptr;
  mDXVA2Manager.reset();
}

bool WMFVideoMFTManager::IsHardwareAccelerated(
    nsACString& aFailureReason) const {
  aFailureReason = mDXVAFailureReason;
  return mDecoder && mUseHwAccel;
}

nsCString WMFVideoMFTManager::GetDescriptionName() const {
  nsCString failureReason;
  bool hw = IsHardwareAccelerated(failureReason);

  const char* formatName = [&]() {
    if (!mDecoder) {
      return "not initialized";
    }
    GUID format = mDecoder->GetOutputMediaSubType();
    if (format == MFVideoFormat_NV12) {
      if (!gfx::DeviceManagerDx::Get()->CanUseNV12()) {
        return "nv12->argb32";
      }
      return "nv12";
    }
    if (format == MFVideoFormat_P010) {
      if (!gfx::DeviceManagerDx::Get()->CanUseP010()) {
        return "p010->argb32";
      }
      return "p010";
    }
    if (format == MFVideoFormat_P016) {
      if (!gfx::DeviceManagerDx::Get()->CanUseP016()) {
        return "p016->argb32";
      }
      return "p016";
    }
    if (format == MFVideoFormat_YV12) {
      return "yv12";
    }
    return "unknown";
  }();

  const char* dxvaName = [&]() {
    if (!mDXVA2Manager) {
      return "no DXVA";
    }
    if (mDXVA2Manager->IsD3D11()) {
      return "D3D11";
    }
    return "D3D9";
  }();

  return nsPrintfCString("wmf %s codec %s video decoder - %s, %s",
                         StreamTypeToString(mStreamType),
                         hw ? "hardware" : "software", dxvaName, formatName);
}

}  // namespace mozilla
