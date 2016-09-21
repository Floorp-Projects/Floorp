/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <winsdkver.h>
#include "WMFVideoMFTManager.h"
#include "MediaDecoderReader.h"
#include "MediaPrefs.h"
#include "WMFUtils.h"
#include "ImageContainer.h"
#include "VideoUtils.h"
#include "DXVA2Manager.h"
#include "nsThreadUtils.h"
#include "Layers.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/layers/LayersTypes.h"
#include "MediaInfo.h"
#include "MediaPrefs.h"
#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"
#include "nsWindowsHelpers.h"
#include "gfx2DGlue.h"
#include "gfxWindowsPlatform.h"
#include "IMFYCbCrImage.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/Telemetry.h"
#include "nsPrintfCString.h"
#include "MediaTelemetryConstants.h"
#include "GMPUtils.h" // For SplitAt. TODO: Move SplitAt to a central place.
#include "MP4Decoder.h"
#include "VPXDecoder.h"

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

using mozilla::layers::Image;
using mozilla::layers::IMFYCbCrImage;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;

#if WINVER_MAXVER < 0x0A00
// Windows 10+ SDK has VP80 and VP90 defines
const GUID MFVideoFormat_VP80 =
{
  0x30385056,
  0x0000,
  0x0010,
  {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

const GUID MFVideoFormat_VP90 =
{
  0x30395056,
  0x0000,
  0x0010,
  {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};
#endif

const CLSID CLSID_WebmMfVp8Dec =
{
  0x451e3cb7,
  0x2622,
  0x4ba5,
  {0x8e, 0x1d, 0x44, 0xb3, 0xc4, 0x1d, 0x09, 0x24}
};

const CLSID CLSID_WebmMfVp9Dec =
{
  0x7ab4bd2,
  0x1979,
  0x4fcd,
  {0xa6, 0x97, 0xdf, 0x9a, 0xd1, 0x5b, 0x34, 0xfe}
};

namespace mozilla {

WMFVideoMFTManager::WMFVideoMFTManager(
                            const VideoInfo& aConfig,
                            mozilla::layers::LayersBackend aLayersBackend,
                            mozilla::layers::ImageContainer* aImageContainer,
                            bool aDXVAEnabled)
  : mVideoInfo(aConfig)
  , mVideoStride(0)
  , mImageSize(aConfig.mImage)
  , mImageContainer(aImageContainer)
  , mDXVAEnabled(aDXVAEnabled)
  , mLayersBackend(aLayersBackend)
  , mNullOutputCount(0)
  , mGotValidOutputAfterNullOutput(false)
  , mGotExcessiveNullOutput(false)
  , mIsValid(true)
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
}

WMFVideoMFTManager::~WMFVideoMFTManager()
{
  MOZ_COUNT_DTOR(WMFVideoMFTManager);
  // Ensure DXVA/D3D9 related objects are released on the main thread.
  if (mDXVA2Manager) {
    DeleteOnMainThread(mDXVA2Manager);
  }

  // Record whether the video decoder successfully decoded, or output null
  // samples but did/didn't recover.
  uint32_t telemetry = (mNullOutputCount == 0) ? 0 :
                       (mGotValidOutputAfterNullOutput && mGotExcessiveNullOutput) ? 1 :
                       mGotExcessiveNullOutput ? 2 :
                       mGotValidOutputAfterNullOutput ? 3 :
                       4;

  nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction([=]() -> void {
    LOG(nsPrintfCString("Reporting telemetry VIDEO_MFT_OUTPUT_NULL_SAMPLES=%d", telemetry).get());
    Telemetry::Accumulate(Telemetry::ID::VIDEO_MFT_OUTPUT_NULL_SAMPLES, telemetry);
  });
  AbstractThread::MainThread()->Dispatch(task.forget());
}

const GUID&
WMFVideoMFTManager::GetMFTGUID()
{
  MOZ_ASSERT(mStreamType != Unknown);
  switch (mStreamType) {
    case H264: return CLSID_CMSH264DecoderMFT;
    case VP8: return CLSID_WebmMfVp8Dec;
    case VP9: return CLSID_WebmMfVp9Dec;
    default: return GUID_NULL;
  };
}

const GUID&
WMFVideoMFTManager::GetMediaSubtypeGUID()
{
  MOZ_ASSERT(mStreamType != Unknown);
  switch (mStreamType) {
    case H264: return MFVideoFormat_H264;
    case VP8: return MFVideoFormat_VP80;
    case VP9: return MFVideoFormat_VP90;
    default: return GUID_NULL;
  };
}

struct D3DDLLBlacklistingCache
{
  // Blacklist pref value last seen.
  nsCString mBlacklistPref;
  // Non-empty if a blacklisted DLL was found.
  nsCString mBlacklistedDLL;
};
StaticAutoPtr<D3DDLLBlacklistingCache> sD3D11BlacklistingCache;
StaticAutoPtr<D3DDLLBlacklistingCache> sD3D9BlacklistingCache;

// If a blacklisted DLL is found, return its information, otherwise "".
static const nsCString&
FindDXVABlacklistedDLL(StaticAutoPtr<D3DDLLBlacklistingCache>& aDLLBlacklistingCache,
                       const char* aDLLBlacklistPrefName)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");

  if (!aDLLBlacklistingCache) {
    // First time here, create persistent data that will be reused in all
    // D3D11-blacklisting checks.
    aDLLBlacklistingCache = new D3DDLLBlacklistingCache();
    ClearOnShutdown(&aDLLBlacklistingCache);
  }

  if (XRE_GetProcessType() == GeckoProcessType_GPU) {
    // The blacklist code doesn't support running in
    // the GPU process yet.
    aDLLBlacklistingCache->mBlacklistPref.SetLength(0);
    aDLLBlacklistingCache->mBlacklistedDLL.SetLength(0);
    return aDLLBlacklistingCache->mBlacklistedDLL;
  }

  nsAdoptingCString blacklist = Preferences::GetCString(aDLLBlacklistPrefName);
  if (blacklist.IsEmpty()) {
    // Empty blacklist -> No blacklisting.
    aDLLBlacklistingCache->mBlacklistPref.SetLength(0);
    aDLLBlacklistingCache->mBlacklistedDLL.SetLength(0);
    return aDLLBlacklistingCache->mBlacklistedDLL;
  }

  // Detect changes in pref.
  if (aDLLBlacklistingCache->mBlacklistPref.Equals(blacklist)) {
    // Same blacklist -> Return same result (i.e., don't check DLLs again).
    return aDLLBlacklistingCache->mBlacklistedDLL;
  }
  // Adopt new pref now, so we don't work on it again.
  aDLLBlacklistingCache->mBlacklistPref = blacklist;

  // media.wmf.disable-d3d*-for-dlls format: (whitespace is trimmed)
  // "dll1.dll: 1.2.3.4[, more versions...][; more dlls...]"
  nsTArray<nsCString> dlls;
  SplitAt(";", blacklist, dlls);
  for (const auto& dll : dlls) {
    nsTArray<nsCString> nameAndVersions;
    SplitAt(":", dll, nameAndVersions);
    if (nameAndVersions.Length() != 2) {
      NS_WARNING(nsPrintfCString("Skipping incorrect '%s' dll:versions format",
                                 aDLLBlacklistPrefName).get());
      continue;
    }

    nameAndVersions[0].CompressWhitespace();
    NS_ConvertUTF8toUTF16 name(nameAndVersions[0]);
    WCHAR systemPath[MAX_PATH + 1];
    if (!ConstructSystem32Path(name.get(), systemPath, MAX_PATH + 1)) {
      // Cannot build path -> Assume it's not the blacklisted DLL.
      continue;
    }

    DWORD zero;
    DWORD infoSize = GetFileVersionInfoSizeW(systemPath, &zero);
    if (infoSize == 0) {
      // Can't get file info -> Assume we don't have the blacklisted DLL.
      continue;
    }
    // vInfo is a pointer into infoData, that's why we keep it outside of the loop.
    auto infoData = MakeUnique<unsigned char[]>(infoSize);
    VS_FIXEDFILEINFO *vInfo;
    UINT vInfoLen;
    if (!GetFileVersionInfoW(systemPath, 0, infoSize, infoData.get())
        || !VerQueryValueW(infoData.get(), L"\\", (LPVOID*)&vInfo, &vInfoLen)
        || !vInfo) {
      // Can't find version -> Assume it's not blacklisted.
      continue;
    }

    nsTArray<nsCString> versions;
    SplitAt(",", nameAndVersions[1], versions);
    for (const auto& version : versions) {
      nsTArray<nsCString> numberStrings;
      SplitAt(".", version, numberStrings);
      if (numberStrings.Length() != 4) {
        NS_WARNING(nsPrintfCString("Skipping incorrect '%s' a.b.c.d version format",
                                   aDLLBlacklistPrefName).get());
        continue;
      }
      DWORD numbers[4];
      nsresult errorCode = NS_OK;
      for (int i = 0; i < 4; ++i) {
        numberStrings[i].CompressWhitespace();
        numbers[i] = DWORD(numberStrings[i].ToInteger(&errorCode));
        if (NS_FAILED(errorCode)) {
          break;
        }
        if (numbers[i] > UINT16_MAX) {
          errorCode = NS_ERROR_FAILURE;
          break;
        }
      }

      if (NS_FAILED(errorCode)) {
        NS_WARNING(nsPrintfCString("Skipping incorrect '%s' a.b.c.d version format",
                                   aDLLBlacklistPrefName).get());
        continue;
      }

      if (vInfo->dwFileVersionMS == ((numbers[0] << 16) | numbers[1])
          && vInfo->dwFileVersionLS == ((numbers[2] << 16) | numbers[3])) {
        // Blacklisted! Record bad DLL.
        aDLLBlacklistingCache->mBlacklistedDLL.SetLength(0);
        aDLLBlacklistingCache->mBlacklistedDLL.AppendPrintf(
          "%s (%lu.%lu.%lu.%lu)",
          nameAndVersions[0].get(), numbers[0], numbers[1], numbers[2], numbers[3]);
        return aDLLBlacklistingCache->mBlacklistedDLL;
      }
    }
  }

  // No blacklisted DLL.
  aDLLBlacklistingCache->mBlacklistedDLL.SetLength(0);
  return aDLLBlacklistingCache->mBlacklistedDLL;
}

static const nsCString&
FindD3D11BlacklistedDLL() {
  return FindDXVABlacklistedDLL(sD3D11BlacklistingCache,
                                "media.wmf.disable-d3d11-for-dlls");
}

static const nsCString&
FindD3D9BlacklistedDLL() {
  return FindDXVABlacklistedDLL(sD3D9BlacklistingCache,
                                "media.wmf.disable-d3d9-for-dlls");
}

class CreateDXVAManagerEvent : public Runnable {
public:
  CreateDXVAManagerEvent(LayersBackend aBackend, nsCString& aFailureReason)
    : mBackend(aBackend)
    , mFailureReason(aFailureReason)
  {}

  NS_IMETHOD Run() override {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    nsACString* failureReason = &mFailureReason;
    nsCString secondFailureReason;
    bool allowD3D11 = (XRE_GetProcessType() == GeckoProcessType_GPU) ||
                      MediaPrefs::PDMWMFAllowD3D11();
    if (mBackend == LayersBackend::LAYERS_D3D11 &&
        allowD3D11 && IsWin8OrLater()) {
      const nsCString& blacklistedDLL = FindD3D11BlacklistedDLL();
      if (!blacklistedDLL.IsEmpty()) {
        failureReason->AppendPrintf("D3D11 blacklisted with DLL %s",
                                    blacklistedDLL.get());
      } else {
        mDXVA2Manager = DXVA2Manager::CreateD3D11DXVA(*failureReason);
        if (mDXVA2Manager) {
          return NS_OK;
        }
      }
      // Try again with d3d9, but record the failure reason
      // into a new var to avoid overwriting the d3d11 failure.
      failureReason = &secondFailureReason;
      mFailureReason.Append(NS_LITERAL_CSTRING("; "));
    }

    const nsCString& blacklistedDLL = FindD3D9BlacklistedDLL();
    if (!blacklistedDLL.IsEmpty()) {
      mFailureReason.AppendPrintf("D3D9 blacklisted with DLL %s",
                                  blacklistedDLL.get());
    } else {
      mDXVA2Manager = DXVA2Manager::CreateD3D9DXVA(*failureReason);
      // Make sure we include the messages from both attempts (if applicable).
      mFailureReason.Append(secondFailureReason);
    }
    return NS_OK;
  }
  nsAutoPtr<DXVA2Manager> mDXVA2Manager;
  LayersBackend mBackend;
  nsACString& mFailureReason;
};

bool
WMFVideoMFTManager::InitializeDXVA(bool aForceD3D9)
{
  // If we use DXVA but aren't running with a D3D layer manager then the
  // readback of decoded video frames from GPU to CPU memory grinds painting
  // to a halt, and makes playback performance *worse*.
  if (!mDXVAEnabled) {
    mDXVAFailureReason.AssignLiteral("Hardware video decoding disabled or blacklisted");
    return false;
  }
  MOZ_ASSERT(!mDXVA2Manager);
  if (mLayersBackend != LayersBackend::LAYERS_D3D9 &&
      mLayersBackend != LayersBackend::LAYERS_D3D11) {
    mDXVAFailureReason.AssignLiteral("Unsupported layers backend");
    return false;
  }

  // The DXVA manager must be created on the main thread.
  RefPtr<CreateDXVAManagerEvent> event =
    new CreateDXVAManagerEvent(aForceD3D9 ? LayersBackend::LAYERS_D3D9
                                          : mLayersBackend,
                               mDXVAFailureReason);

  if (NS_IsMainThread()) {
    event->Run();
  } else {
    NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
  }
  mDXVA2Manager = event->mDXVA2Manager;

  return mDXVA2Manager != nullptr;
}

bool
WMFVideoMFTManager::ValidateVideoInfo()
{
  // The WMF H.264 decoder is documented to have a minimum resolution
  // 48x48 pixels. We've observed the decoder working for output smaller than
  // that, but on some output it hangs in IMFTransform::ProcessOutput(), so
  // we just reject streams which are less than the documented minimum.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd797815(v=vs.85).aspx
  static const int32_t MIN_H264_FRAME_DIMENSION = 48;
  if (mStreamType == H264 &&
      (mVideoInfo.mImage.width < MIN_H264_FRAME_DIMENSION ||
       mVideoInfo.mImage.height < MIN_H264_FRAME_DIMENSION)) {
    LogToBrowserConsole(NS_LITERAL_STRING(
      "Can't decode H.264 stream with width or height less than 48 pixels."));
    mIsValid = false;
  }

  return mIsValid;
}

bool
WMFVideoMFTManager::Init()
{
  if (!ValidateVideoInfo()) {
    return false;
  }

  bool success = InitInternal(/* aForceD3D9 = */ false);

  if (success && mDXVA2Manager) {
    // If we had some failures but eventually made it work,
    // make sure we preserve the messages.
    if (mDXVA2Manager->IsD3D11()) {
      mDXVAFailureReason.Append(NS_LITERAL_CSTRING("Using D3D11 API"));
    } else {
      mDXVAFailureReason.Append(NS_LITERAL_CSTRING("Using D3D9 API"));
    }
  }

  return success;
}

bool
WMFVideoMFTManager::InitInternal(bool aForceD3D9)
{
  mUseHwAccel = false; // default value; changed if D3D setup succeeds.
  bool useDxva = InitializeDXVA(aForceD3D9 ||
                                mStreamType == VP8 || mStreamType == VP9);

  RefPtr<MFTDecoder> decoder(new MFTDecoder());

  HRESULT hr = decoder->Create(GetMFTGUID());
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  RefPtr<IMFAttributes> attr(decoder->GetAttributes());
  UINT32 aware = 0;
  if (attr) {
    attr->GetUINT32(MF_SA_D3D_AWARE, &aware);
    attr->SetUINT32(CODECAPI_AVDecNumWorkerThreads,
      WMFDecoderModule::GetNumDecoderThreads());
    if ((XRE_GetProcessType() != GeckoProcessType_GPU) &&
        MediaPrefs::PDMWMFLowLatencyEnabled()) {
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
      //hr = attr->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, TRUE);
      //NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
      MOZ_ASSERT(mDXVA2Manager);
      ULONG_PTR manager = ULONG_PTR(mDXVA2Manager->GetDXVADeviceManager());
      hr = decoder->SendMFTMessage(MFT_MESSAGE_SET_D3D_MANAGER, manager);
      if (SUCCEEDED(hr)) {
        mUseHwAccel = true;
      } else {
        DeleteOnMainThread(mDXVA2Manager);
        mDXVAFailureReason = nsPrintfCString("MFT_MESSAGE_SET_D3D_MANAGER failed with code %X", hr);
      }
    }
    else {
      mDXVAFailureReason.AssignLiteral("Decoder returned false for MF_SA_D3D_AWARE");
    }
  }

  if (!mUseHwAccel) {
    Telemetry::Accumulate(Telemetry::MEDIA_DECODER_BACKEND_USED,
                          uint32_t(media::MediaDecoderBackend::WMFSoftware));
  }

  mDecoder = decoder;
  hr = SetDecoderMediaTypes();
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  LOG("Video Decoder initialized, Using DXVA: %s", (mUseHwAccel ? "Yes" : "No"));

  return true;
}

HRESULT
WMFVideoMFTManager::SetDecoderMediaTypes()
{
  // Setup the input/output media types.
  RefPtr<IMFMediaType> inputType;
  HRESULT hr = wmf::MFCreateMediaType(getter_AddRefs(inputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetGUID(MF_MT_SUBTYPE, GetMediaSubtypeGUID());
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaType> outputType;
  hr = wmf::MFCreateMediaType(getter_AddRefs(outputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  GUID outputSubType = mUseHwAccel ? MFVideoFormat_NV12 : MFVideoFormat_YV12;
  hr = outputType->SetGUID(MF_MT_SUBTYPE, outputSubType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  return mDecoder->SetMediaTypes(inputType, outputType);
}

HRESULT
WMFVideoMFTManager::Input(MediaRawData* aSample)
{
  if (!mIsValid) {
    return E_FAIL;
  }

  if (!mDecoder) {
    // This can happen during shutdown.
    return E_FAIL;
  }

  HRESULT hr = mDecoder->CreateInputSample(aSample->Data(),
                                           uint32_t(aSample->Size()),
                                           aSample->mTime,
                                           &mLastInput);
  NS_ENSURE_TRUE(SUCCEEDED(hr) && mLastInput != nullptr, hr);

  mLastDuration = aSample->mDuration;
  mLastTime = aSample->mTime;
  mSamplesCount++;

  // Forward sample data to the decoder.
  return mDecoder->Input(mLastInput);
}

class SupportsConfigEvent : public Runnable {
public:
  SupportsConfigEvent(DXVA2Manager* aDXVA2Manager, IMFMediaType* aMediaType, float aFramerate)
    : mDXVA2Manager(aDXVA2Manager)
    , mMediaType(aMediaType)
    , mFramerate(aFramerate)
    , mSupportsConfig(false)
  {}

  NS_IMETHOD Run() {
    MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
    mSupportsConfig = mDXVA2Manager->SupportsConfig(mMediaType, mFramerate);
    return NS_OK;
  }
  DXVA2Manager* mDXVA2Manager;
  IMFMediaType* mMediaType;
  float mFramerate;
  bool mSupportsConfig;
};

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
bool
WMFVideoMFTManager::CanUseDXVA(IMFMediaType* aType)
{
  MOZ_ASSERT(mDXVA2Manager);
  // SupportsConfig only checks for valid h264 decoders currently.
  if (mStreamType != H264) {
    return true;
  }

  // Assume the current samples duration is representative for the
  // entire video.
  float framerate = 1000000.0 / mLastDuration;

  // The supports config check must be done on the main thread since we have
  // a crash guard protecting it.
  RefPtr<SupportsConfigEvent> event =
    new SupportsConfigEvent(mDXVA2Manager, aType, framerate);

  if (NS_IsMainThread()) {
    event->Run();
  } else {
    NS_DispatchToMainThread(event, NS_DISPATCH_SYNC);
  }

  return event->mSupportsConfig;
}

HRESULT
WMFVideoMFTManager::ConfigureVideoFrameGeometry()
{
  RefPtr<IMFMediaType> mediaType;
  HRESULT hr = mDecoder->GetOutputMediaType(mediaType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  // If we enabled/disabled DXVA in response to a resolution
  // change then we need to renegotiate our media types,
  // and resubmit our previous frame (since the MFT appears
  // to lose it otherwise).
  if (mUseHwAccel && !CanUseDXVA(mediaType)) {
    mDXVAEnabled = false;
    if (!Init()) {
      return E_FAIL;
    }

    mDecoder->Input(mLastInput);
    return S_OK;
  }

  // Verify that the video subtype is what we expect it to be.
  // When using hardware acceleration/DXVA2 the video format should
  // be NV12, which is DXVA2's preferred format. For software decoding
  // we use YV12, as that's easier for us to stick into our rendering
  // pipeline than NV12. NV12 has interleaved UV samples, whereas YV12
  // is a planar format.
  GUID videoFormat;
  hr = mediaType->GetGUID(MF_MT_SUBTYPE, &videoFormat);
  NS_ENSURE_TRUE(videoFormat == MFVideoFormat_NV12 || !mUseHwAccel, E_FAIL);
  NS_ENSURE_TRUE(videoFormat == MFVideoFormat_YV12 || mUseHwAccel, E_FAIL);

  nsIntRect pictureRegion;
  hr = GetPictureRegion(mediaType, pictureRegion);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  UINT32 width = pictureRegion.width;
  UINT32 height = pictureRegion.height;
  mImageSize = nsIntSize(width, height);
  // Calculate and validate the picture region and frame dimensions after
  // scaling by the pixel aspect ratio.
  pictureRegion = mVideoInfo.ScaledImageRect(width, height);
  if (!IsValidVideoRegion(mImageSize, pictureRegion, mVideoInfo.mDisplay)) {
    // Video track's frame sizes will overflow. Ignore the video track.
    return E_FAIL;
  }

  if (mDXVA2Manager) {
    hr = mDXVA2Manager->ConfigureForSize(width, height);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  }

  // Success! Save state.
  GetDefaultStride(mediaType, width, &mVideoStride);

  LOG("WMFVideoMFTManager frame geometry frame=(%u,%u) stride=%u picture=(%d, %d, %d, %d) display=(%d,%d)",
      width, height,
      mVideoStride,
      pictureRegion.x, pictureRegion.y, pictureRegion.width, pictureRegion.height,
      mVideoInfo.mDisplay.width, mVideoInfo.mDisplay.height);

  return S_OK;
}

HRESULT
WMFVideoMFTManager::CreateBasicVideoFrame(IMFSample* aSample,
                                          int64_t aStreamOffset,
                                          VideoData** aOutVideoData)
{
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
  hr = buffer->QueryInterface(static_cast<IMF2DBuffer**>(getter_AddRefs(twoDBuffer)));
  if (SUCCEEDED(hr)) {
    hr = twoDBuffer->Lock2D(&data, &stride);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  } else {
    hr = buffer->Lock(&data, nullptr, nullptr);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
    stride = mVideoStride;
  }

  // YV12, planar format: [YYYY....][VVVV....][UUUU....]
  // i.e., Y, then V, then U.
  VideoData::YCbCrBuffer b;

  uint32_t videoWidth = mImageSize.width;
  uint32_t videoHeight = mImageSize.height;

  // Y (Y') plane
  b.mPlanes[0].mData = data;
  b.mPlanes[0].mStride = stride;
  b.mPlanes[0].mHeight = videoHeight;
  b.mPlanes[0].mWidth = videoWidth;
  b.mPlanes[0].mOffset = 0;
  b.mPlanes[0].mSkip = 0;

  // The V and U planes are stored 16-row-aligned, so we need to add padding
  // to the row heights to ensure the Y'CbCr planes are referenced properly.
  uint32_t padding = 0;
  if (videoHeight % 16 != 0) {
    padding = 16 - (videoHeight % 16);
  }
  uint32_t y_size = stride * (videoHeight + padding);
  uint32_t v_size = stride * (videoHeight + padding) / 4;
  uint32_t halfStride = (stride + 1) / 2;
  uint32_t halfHeight = (videoHeight + 1) / 2;
  uint32_t halfWidth = (videoWidth + 1) / 2;

  // U plane (Cb)
  b.mPlanes[1].mData = data + y_size + v_size;
  b.mPlanes[1].mStride = halfStride;
  b.mPlanes[1].mHeight = halfHeight;
  b.mPlanes[1].mWidth = halfWidth;
  b.mPlanes[1].mOffset = 0;
  b.mPlanes[1].mSkip = 0;

  // V plane (Cr)
  b.mPlanes[2].mData = data + y_size;
  b.mPlanes[2].mStride = halfStride;
  b.mPlanes[2].mHeight = halfHeight;
  b.mPlanes[2].mWidth = halfWidth;
  b.mPlanes[2].mOffset = 0;
  b.mPlanes[2].mSkip = 0;

  media::TimeUnit pts = GetSampleTime(aSample);
  NS_ENSURE_TRUE(pts.IsValid(), E_FAIL);
  media::TimeUnit duration = GetSampleDuration(aSample);
  NS_ENSURE_TRUE(duration.IsValid(), E_FAIL);
  nsIntRect pictureRegion = mVideoInfo.ScaledImageRect(videoWidth, videoHeight);

  if (mLayersBackend != LayersBackend::LAYERS_D3D9 &&
      mLayersBackend != LayersBackend::LAYERS_D3D11) {
    RefPtr<VideoData> v =
      VideoData::CreateAndCopyData(mVideoInfo,
                                   mImageContainer,
                                   aStreamOffset,
                                   pts.ToMicroseconds(),
                                   duration.ToMicroseconds(),
                                   b,
                                   false,
                                   -1,
                                   pictureRegion);
    if (twoDBuffer) {
      twoDBuffer->Unlock2D();
    } else {
      buffer->Unlock();
    }
    v.forget(aOutVideoData);
    return S_OK;
  }

  RefPtr<layers::PlanarYCbCrImage> image =
    new IMFYCbCrImage(buffer, twoDBuffer);

  VideoData::SetVideoDataToImage(image,
                                 mVideoInfo,
                                 b,
                                 pictureRegion,
                                 false);

  RefPtr<VideoData> v =
    VideoData::CreateFromImage(mVideoInfo,
                               aStreamOffset,
                               pts.ToMicroseconds(),
                               duration.ToMicroseconds(),
                               image.forget(),
                               false,
                               -1,
                               pictureRegion);

  v.forget(aOutVideoData);
  return S_OK;
}

HRESULT
WMFVideoMFTManager::CreateD3DVideoFrame(IMFSample* aSample,
                                        int64_t aStreamOffset,
                                        VideoData** aOutVideoData)
{
  NS_ENSURE_TRUE(aSample, E_POINTER);
  NS_ENSURE_TRUE(aOutVideoData, E_POINTER);
  NS_ENSURE_TRUE(mDXVA2Manager, E_ABORT);
  NS_ENSURE_TRUE(mUseHwAccel, E_ABORT);

  *aOutVideoData = nullptr;
  HRESULT hr;

  nsIntRect pictureRegion =
    mVideoInfo.ScaledImageRect(mImageSize.width, mImageSize.height);
  RefPtr<Image> image;
  hr = mDXVA2Manager->CopyToImage(aSample,
                                  pictureRegion,
                                  getter_AddRefs(image));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  NS_ENSURE_TRUE(image, E_FAIL);

  media::TimeUnit pts = GetSampleTime(aSample);
  NS_ENSURE_TRUE(pts.IsValid(), E_FAIL);
  media::TimeUnit duration = GetSampleDuration(aSample);
  NS_ENSURE_TRUE(duration.IsValid(), E_FAIL);
  RefPtr<VideoData> v = VideoData::CreateFromImage(mVideoInfo,
                                                   aStreamOffset,
                                                   pts.ToMicroseconds(),
                                                   duration.ToMicroseconds(),
                                                   image.forget(),
                                                   false,
                                                   -1,
                                                   pictureRegion);

  NS_ENSURE_TRUE(v, E_FAIL);
  v.forget(aOutVideoData);

  return S_OK;
}

// Blocks until decoded sample is produced by the deoder.
HRESULT
WMFVideoMFTManager::Output(int64_t aStreamOffset,
                           RefPtr<MediaData>& aOutData)
{
  RefPtr<IMFSample> sample;
  HRESULT hr;
  aOutData = nullptr;
  int typeChangeCount = 0;
  bool wasDraining = mDraining;
  int64_t sampleCount = mSamplesCount;
  if (wasDraining) {
    mSamplesCount = 0;
    mDraining = false;
  }

  media::TimeUnit pts;
  media::TimeUnit duration;

  // Loop until we decode a sample, or an unexpected error that we can't
  // handle occurs.
  while (true) {
    hr = mDecoder->Output(&sample);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      // Video stream output type change. Probably a geometric apperature
      // change. Reconfigure the video geometry, so that we output the
      // correct size frames.
      MOZ_ASSERT(!sample);
      hr = ConfigureVideoFrameGeometry();
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
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
          LOG("Excessive Video MFTDecoder returning success but no output; giving up");
          mGotExcessiveNullOutput = true;
          return E_FAIL;
        }
        continue;
      }
      pts = GetSampleTime(sample);
      duration = GetSampleDuration(sample);
      if (!pts.IsValid() || !duration.IsValid()) {
        return E_FAIL;
      }
      if (wasDraining && sampleCount == 1 && pts == media::TimeUnit()) {
        // WMF is unable to calculate a duration if only a single sample
        // was parsed. Additionally, the pts always comes out at 0 under those
        // circumstances.
        // Seeing that we've only fed the decoder a single frame, the pts
        // and duration are known, it's of the last sample.
        pts = media::TimeUnit::FromMicroseconds(mLastTime);
        duration = media::TimeUnit::FromMicroseconds(mLastDuration);
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
    // Else unexpected error, assert, and bail.
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
  // Set the potentially corrected pts and duration.
  aOutData->mTime = pts.ToMicroseconds();
  aOutData->mDuration = duration.ToMicroseconds();

  if (mNullOutputCount) {
    mGotValidOutputAfterNullOutput = true;
  }

  return S_OK;
}

void
WMFVideoMFTManager::Shutdown()
{
  mDecoder = nullptr;
  DeleteOnMainThread(mDXVA2Manager);
}

bool
WMFVideoMFTManager::IsHardwareAccelerated(nsACString& aFailureReason) const
{
  aFailureReason = mDXVAFailureReason;
  return mDecoder && mUseHwAccel;
}

void
WMFVideoMFTManager::ConfigurationChanged(const TrackInfo& aConfig)
{
  MOZ_ASSERT(aConfig.GetAsVideoInfo());
  mVideoInfo = *aConfig.GetAsVideoInfo();
  mImageSize = mVideoInfo.mImage;
  ValidateVideoInfo();
}

} // namespace mozilla
