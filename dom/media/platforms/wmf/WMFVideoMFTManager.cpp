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
#include "MP4Decoder.h"
#include "MediaInfo.h"
#include "MediaTelemetryConstants.h"
#include "VPXDecoder.h"
#include "VideoUtils.h"
#include "WMFDecoderModule.h"
#include "WMFUtils.h"
#include "gfx2DGlue.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/SchedulerGroup.h"
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

// Note: CLSID_WebmMfVpxDec needs to be extern for the CanCreateWMFDecoder
// template in WMFDecoderModule.cpp to work.
extern const GUID CLSID_WebmMfVpxDec = {
    0xe3aaf548,
    0xc9a4,
    0x4c6e,
    {0x23, 0x4d, 0x5a, 0xda, 0x37, 0x4b, 0x00, 0x00}};

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
      mDecodedImageSize(aConfig.mImage),
      mVideoStride(0),
      mColorSpace(aConfig.mColorSpace),
      mColorRange(aConfig.mColorRange),
      mImageContainer(aImageContainer),
      mKnowsCompositor(aKnowsCompositor),
      mDXVAEnabled(aDXVAEnabled &&
                   !aOptions.contains(
                       CreateDecoderParams::Option::HardwareDecoderNotAllowed)),
      mFramerate(aFramerate),
      mLowLatency(aOptions.contains(CreateDecoderParams::Option::LowLatency))
// mVideoStride, mVideoWidth, mVideoHeight, mUseHwAccel are initialized in
// Init().
{
  MOZ_COUNT_CTOR(WMFVideoMFTManager);

  // Need additional checks/params to check vp8/vp9
  if (MP4Decoder::IsH264(aConfig.mMimeType)) {
    mStreamType = H264;
  } else if (VPXDecoder::IsVP8(aConfig.mMimeType)) {
    mStreamType = VP8;
  } else if (VPXDecoder::IsVP9(aConfig.mMimeType)) {
    mStreamType = VP9;
  } else {
    mStreamType = Unknown;
  }

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

const GUID& WMFVideoMFTManager::GetMFTGUID() {
  MOZ_ASSERT(mStreamType != Unknown);
  switch (mStreamType) {
    case H264:
      return CLSID_CMSH264DecoderMFT;
    case VP8:
      return CLSID_WebmMfVpxDec;
    case VP9:
      return CLSID_WebmMfVpxDec;
    default:
      return GUID_NULL;
  };
}

const GUID& WMFVideoMFTManager::GetMediaSubtypeGUID() {
  MOZ_ASSERT(mStreamType != Unknown);
  switch (mStreamType) {
    case H264:
      return MFVideoFormat_H264;
    case VP8:
      return MFVideoFormat_VP80;
    case VP9:
      return MFVideoFormat_VP90;
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

  nsACString* failureReason = &mDXVAFailureReason;
  nsCString secondFailureReason;
  if (StaticPrefs::media_wmf_dxva_d3d11_enabled() && IsWin8OrLater()) {
    mDXVA2Manager.reset(
        DXVA2Manager::CreateD3D11DXVA(mKnowsCompositor, *failureReason));
    if (mDXVA2Manager) {
      return true;
    }
    // Try again with d3d9, but record the failure reason
    // into a new var to avoid overwriting the d3d11 failure.
    failureReason = &secondFailureReason;
    mDXVAFailureReason.AppendLiteral("; ");
  }

  mDXVA2Manager.reset(
      DXVA2Manager::CreateD3D9DXVA(mKnowsCompositor, *failureReason));
  // Make sure we include the messages from both attempts (if applicable).
  mDXVAFailureReason.Append(secondFailureReason);

  return mDXVA2Manager != nullptr;
}

MediaResult WMFVideoMFTManager::ValidateVideoInfo() {
  if (mStreamType != H264 ||
      StaticPrefs::media_wmf_allow_unsupported_resolutions()) {
    return NS_OK;
  }

  // The WMF H.264 decoder is documented to have a minimum resolution 48x48
  // pixels for resolution, but we won't enable hw decoding for the resolution <
  // 132 pixels. It's assumed the software decoder doesn't have this limitation,
  // but it still might have maximum resolution limitation.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd797815(v=vs.85).aspx
  const bool Is4KCapable = IsWin8OrLater() || IsWin7H264Decoder4KCapable();
  static const int32_t MAX_H264_PIXEL_COUNT =
      Is4KCapable ? 4096 * 2304 : 1920 * 1088;
  const CheckedInt32 pixelCount =
      CheckedInt32(mVideoInfo.mImage.width) * mVideoInfo.mImage.height;

  if (!pixelCount.isValid() || pixelCount.value() > MAX_H264_PIXEL_COUNT) {
    mIsValid = false;
    return MediaResult(
        NS_ERROR_DOM_MEDIA_FATAL_ERR,
        RESULT_DETAIL("Can't decode H.264 stream because its "
                      "resolution is out of the maximum limitation"));
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
  bool useDxva = (mStreamType != H264 ||
                  (mVideoInfo.ImageRect().width > MIN_H264_HW_WIDTH &&
                   mVideoInfo.ImageRect().height > MIN_H264_HW_HEIGHT)) &&
                 InitializeDXVA();

  RefPtr<MFTDecoder> decoder = new MFTDecoder();
  HRESULT hr = decoder->Create(GetMFTGUID());
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
            "MFT_MESSAGE_SET_D3D_MANAGER failed with code %X", hr);
      }
    } else {
      mDXVAFailureReason.AssignLiteral(
          "Decoder returned false for MF_SA_D3D_AWARE");
    }
  }

  if (!mUseHwAccel) {
    if (mDXVA2Manager) {
      // Either mDXVAEnabled was set to false prior the second call to
      // InitInternal() due to CanUseDXVA() returning false, or
      // MFT_MESSAGE_SET_D3D_MANAGER failed
      mDXVA2Manager.reset();
    }
    if (mStreamType == VP9 || mStreamType == VP8) {
      return MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                         RESULT_DETAIL("Use VP8/9 MFT only if HW acceleration "
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

  RefPtr<IMFMediaType> outputType;
  hr = mDecoder->GetOutputMediaType(outputType);
  NS_ENSURE_TRUE(
      SUCCEEDED(hr),
      MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                  RESULT_DETAIL("Fail to get the output media type.")));

  if (mUseHwAccel && !CanUseDXVA(outputType, mFramerate)) {
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

  RefPtr<IMFMediaType> outputType;
  hr = wmf::MFCreateMediaType(getter_AddRefs(outputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = MFSetAttributeSize(outputType, MF_MT_FRAME_SIZE,
                          mVideoInfo.ImageRect().width,
                          mVideoInfo.ImageRect().height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  GUID outputSubType = mUseHwAccel ? MFVideoFormat_NV12 : MFVideoFormat_YV12;
  hr = outputType->SetGUID(MF_MT_SUBTYPE, outputSubType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

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

  if (mStreamType == VP9 && aSample->mKeyframe) {
    // Check the VP9 profile. the VP9 MFT can only handle correctly profile 0
    // and 2 (yuv420 8/10/12 bits)
    int profile =
        VPXDecoder::GetVP9Profile(Span(aSample->Data(), aSample->Size()));
    if (profile != 0 && profile != 2) {
      return E_FAIL;
    }
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

// The MFTransform we use for decoding h264 video will silently fall
// back to software decoding (even if we've negotiated DXVA) if the GPU
// doesn't support decoding the given resolution. It will then upload
// the software decoded frames into d3d textures to preserve behaviour.
//
// Unfortunately this seems to cause corruption (see bug 1193547) and is
// slow because the upload is done into a non-shareable texture and requires
// us to copy it.
//
// This code tests if the given resolution can be supported directly on the GPU,
// and makes sure we only ask the MFT for DXVA if it can be supported properly.
//
// Ideally we'd know the framerate during initialization and would also ensure
// that new decoders are created if the resolution changes. Then we could move
// this check into Init and consolidate the main thread blocking code.
bool WMFVideoMFTManager::CanUseDXVA(IMFMediaType* aType, float aFramerate) {
  MOZ_ASSERT(mDXVA2Manager);
  // SupportsConfig only checks for valid h264 decoders currently.
  if (mStreamType != H264) {
    return true;
  }

  return mDXVA2Manager->SupportsConfig(aType, aFramerate);
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
    LOG("Got negative sample duration: %d seconds. Using mLastDuration "
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
  hr =
      mDXVA2Manager->CopyToImage(aSample, pictureRegion, getter_AddRefs(image));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
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
      if (!pts.IsValid() || !duration.IsValid()) {
        return E_FAIL;
      }
      if (mSeekTargetThreshold.isSome()) {
        if ((pts + duration) < mSeekTargetThreshold.ref()) {
          LOG("Dropping video frame which pts is smaller than seek target.");
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
  return nsPrintfCString("wmf %s codec %s video decoder - %s",
                         StreamTypeString(), hw ? "hardware" : "software",
                         hw ? StaticPrefs::media_wmf_use_nv12_format() &&
                                      gfx::DeviceManagerDx::Get()->CanUseNV12()
                                  ? "nv12"
                                  : "rgba32"
                            : "yuv420");
}

}  // namespace mozilla
