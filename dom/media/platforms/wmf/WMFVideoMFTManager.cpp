/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFVideoMFTManager.h"

#include "DXVA2Manager.h"
#include "GMPUtils.h" // For SplitAt. TODO: Move SplitAt to a central place.
#include "IMFYCbCrImage.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "MP4Decoder.h"
#include "MediaInfo.h"
#include "MediaTelemetryConstants.h"
#include "VPXDecoder.h"
#include "VideoUtils.h"
#include "WMFUtils.h"
#include "gfx2DGlue.h"
#include "gfxPrefs.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/Telemetry.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "nsWindowsHelpers.h"
#include "WMFDecoderModule.h"
#include <algorithm>
#include <psapi.h>
#include <winsdkver.h>

#define LOG(...) MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

using mozilla::layers::Image;
using mozilla::layers::IMFYCbCrImage;
using mozilla::layers::LayerManager;
using mozilla::layers::LayersBackend;
using mozilla::media::TimeUnit;

// AMD
// Path is appended on to the %ProgramW6432% base path.
const wchar_t kAMDVPXDecoderDLLPath[] =
  L"\\Common Files\\ATI Technologies\\Multimedia\\";

const wchar_t kAMDVP9DecoderDLLName[] =
#if defined(ARCH_CPU_X86)
  L"amf-mft-decvp9-decoder32.dll";
#elif defined(ARCH_CPU_X86_64)
  L"amf-mft-decvp9-decoder64.dll";
#else
#error Unsupported Windows CPU Architecture
#endif

extern const GUID CLSID_AMDWebmMfVp9Dec =
{
  0x2d2d728a,
  0x67d6,
  0x48ab,
  { 0x89, 0xfb, 0xa6, 0xec, 0x65, 0x55, 0x49, 0x70 }
};

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

// Note: CLSID_WebmMfVpxDec needs to be extern for the CanCreateWMFDecoder
// template in WMFDecoderModule.cpp to work.
extern const GUID CLSID_WebmMfVpxDec =
{
  0xe3aaf548,
  0xc9a4,
  0x4c6e,
  { 0x23, 0x4d, 0x5a, 0xda, 0x37, 0x4b, 0x00, 0x00 }
};

namespace mozilla {

template<class T>
class DeleteObjectTask: public Runnable {
public:
  explicit DeleteObjectTask(nsAutoPtr<T>& aObject)
    : Runnable("VideoUtils::DeleteObjectTask")
    , mObject(aObject)
  {
  }
  NS_IMETHOD Run() override {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    mObject = nullptr;
    return NS_OK;
  }
private:
  nsAutoPtr<T> mObject;
};

template<class T>
void DeleteOnMainThread(nsAutoPtr<T>& aObject) {
  nsCOMPtr<nsIRunnable> r = new DeleteObjectTask<T>(aObject);
  SystemGroup::Dispatch(TaskCategory::Other, r.forget());
}

LayersBackend
GetCompositorBackendType(layers::KnowsCompositor* aKnowsCompositor)
{
  if (aKnowsCompositor) {
    return aKnowsCompositor->GetCompositorBackendType();
  }
  return LayersBackend::LAYERS_NONE;
}

WMFVideoMFTManager::WMFVideoMFTManager(
                            const VideoInfo& aConfig,
                            layers::KnowsCompositor* aKnowsCompositor,
                            layers::ImageContainer* aImageContainer,
                            bool aDXVAEnabled)
  : mVideoInfo(aConfig)
  , mImageSize(aConfig.mImage)
  , mVideoStride(0)
  , mImageContainer(aImageContainer)
  , mDXVAEnabled(aDXVAEnabled)
  , mKnowsCompositor(aKnowsCompositor)
  , mAMDVP9InUse(false)
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
  uint32_t telemetry =
    (mNullOutputCount == 0)
    ? 0
    : (mGotValidOutputAfterNullOutput && mGotExcessiveNullOutput)
      ? 1
      : mGotExcessiveNullOutput
        ? 2
        : mGotValidOutputAfterNullOutput ? 3 : 4;

  nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction("WMFVideoMFTManager::~WMFVideoMFTManager",
                                                      [=]() -> void {
    LOG(nsPrintfCString("Reporting telemetry VIDEO_MFT_OUTPUT_NULL_SAMPLES=%d",
                        telemetry)
        .get());
    Telemetry::Accumulate(Telemetry::HistogramID::VIDEO_MFT_OUTPUT_NULL_SAMPLES,
                          telemetry);
  });
  SystemGroup::Dispatch(TaskCategory::Other, task.forget());
}

const GUID&
WMFVideoMFTManager::GetMFTGUID()
{
  MOZ_ASSERT(mStreamType != Unknown);
  switch (mStreamType) {
    case H264: return CLSID_CMSH264DecoderMFT;
    case VP8: return CLSID_WebmMfVpxDec;
    case VP9: return CLSID_WebmMfVpxDec;
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
FindDXVABlacklistedDLL(
  StaticAutoPtr<D3DDLLBlacklistingCache>& aDLLBlacklistingCache,
  const nsCString& aBlacklist,
  const char* aDLLBlacklistPrefName)
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");

  if (!aDLLBlacklistingCache) {
    // First time here, create persistent data that will be reused in all
    // D3D11-blacklisting checks.
    aDLLBlacklistingCache = new D3DDLLBlacklistingCache();
    ClearOnShutdown(&aDLLBlacklistingCache);
  }

  if (aBlacklist.IsEmpty()) {
    // Empty blacklist -> No blacklisting.
    aDLLBlacklistingCache->mBlacklistPref.SetLength(0);
    aDLLBlacklistingCache->mBlacklistedDLL.SetLength(0);
    return aDLLBlacklistingCache->mBlacklistedDLL;
  }

  // Detect changes in pref.
  if (aDLLBlacklistingCache->mBlacklistPref.Equals(aBlacklist)) {
    // Same blacklist -> Return same result (i.e., don't check DLLs again).
    return aDLLBlacklistingCache->mBlacklistedDLL;
  }
  // Adopt new pref now, so we don't work on it again.
  aDLLBlacklistingCache->mBlacklistPref = aBlacklist;

  HANDLE hProcess = GetCurrentProcess();
  mozilla::UniquePtr<HMODULE[]> hMods;
  unsigned int modulesNum = 0;
  if (hProcess != NULL) {
    DWORD modulesSize;
    if (EnumProcessModules(hProcess, nullptr, 0, &modulesSize)) {
      modulesNum = modulesSize / sizeof(HMODULE);
      hMods = mozilla::MakeUnique<HMODULE[]>(modulesNum);
      if (EnumProcessModules(hProcess, hMods.get(),
            modulesNum * sizeof(HMODULE), &modulesSize)) {
        // The list may have shrunk
        if (modulesSize / sizeof(HMODULE) < modulesNum) {
          modulesNum = modulesSize / sizeof(HMODULE);
        }
      } else {
        modulesNum = 0;
      }
    }
  }

  // media.wmf.disable-d3d*-for-dlls format: (whitespace is trimmed)
  // "dll1.dll: 1.2.3.4[, more versions...][; more dlls...]"
  nsTArray<nsCString> dlls;
  SplitAt(";", aBlacklist, dlls);
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

    for (unsigned int i = 0; i <= modulesNum; i++) {
      WCHAR dllPath[MAX_PATH + 1];

      if (i < modulesNum) {
        if (!GetModuleFileNameEx(
              hProcess, hMods[i], dllPath, sizeof(dllPath) / sizeof(WCHAR))) {
          continue;
        }

        nsCOMPtr<nsIFile> file;
        if (NS_WARN_IF(NS_FAILED(NS_NewLocalFile(
              nsDependentString(dllPath), false, getter_AddRefs(file))))) {
          continue;
        }

        nsAutoString leafName;
        if (NS_WARN_IF(NS_FAILED(file->GetLeafName(leafName)))) {
          continue;
        }

        if (_wcsicmp(leafName.get(), name.get())) {
          continue;
        }
      } else {
        if (!ConstructSystem32Path(name.get(), dllPath, MAX_PATH + 1)) {
          // Cannot build path -> Assume it's not the blacklisted DLL.
          continue;
        }
      }

      DWORD zero;
      DWORD infoSize = GetFileVersionInfoSizeW(dllPath, &zero);
      if (infoSize == 0) {
        // Can't get file info -> Assume we don't have the blacklisted DLL.
        continue;
      }
      // vInfo is a pointer into infoData, that's why we keep it outside of the loop.
      auto infoData = MakeUnique<unsigned char[]>(infoSize);
      VS_FIXEDFILEINFO *vInfo;
      UINT vInfoLen;
      if (!GetFileVersionInfoW(dllPath, 0, infoSize, infoData.get())
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
          NS_WARNING(
            nsPrintfCString("Skipping incorrect '%s' a.b.c.d version format",
                            aDLLBlacklistPrefName)
            .get());
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
          NS_WARNING(
            nsPrintfCString("Skipping incorrect '%s' a.b.c.d version format",
                            aDLLBlacklistPrefName)
            .get());
          continue;
        }

        if (vInfo->dwFileVersionMS == ((numbers[0] << 16) | numbers[1])
            && vInfo->dwFileVersionLS == ((numbers[2] << 16) | numbers[3])) {
          // Blacklisted! Record bad DLL.
          aDLLBlacklistingCache->mBlacklistedDLL.SetLength(0);
          aDLLBlacklistingCache->mBlacklistedDLL.AppendPrintf(
            "%s (%lu.%lu.%lu.%lu)",
            nameAndVersions[0].get(),
            numbers[0],
            numbers[1],
            numbers[2],
            numbers[3]);
          return aDLLBlacklistingCache->mBlacklistedDLL;
        }
      }
    }
  }

  // No blacklisted DLL.
  aDLLBlacklistingCache->mBlacklistedDLL.SetLength(0);
  return aDLLBlacklistingCache->mBlacklistedDLL;
}

static const nsCString&
FindD3D11BlacklistedDLL()
{
  return FindDXVABlacklistedDLL(sD3D11BlacklistingCache,
                                gfx::gfxVars::PDMWMFDisableD3D11Dlls(),
                                "media.wmf.disable-d3d11-for-dlls");
}

static const nsCString&
FindD3D9BlacklistedDLL()
{
  return FindDXVABlacklistedDLL(sD3D9BlacklistingCache,
                                gfx::gfxVars::PDMWMFDisableD3D9Dlls(),
                                "media.wmf.disable-d3d9-for-dlls");
}

const nsCString
GetFoundD3D11BlacklistedDLL()
{
  if (sD3D11BlacklistingCache) {
    return sD3D11BlacklistingCache->mBlacklistedDLL;
  }

  return nsCString();
}

const nsCString
GetFoundD3D9BlacklistedDLL()
{
  if (sD3D9BlacklistingCache) {
    return sD3D9BlacklistingCache->mBlacklistedDLL;
  }

  return nsCString();
}

class CreateDXVAManagerEvent : public Runnable
{
public:
  CreateDXVAManagerEvent(layers::KnowsCompositor* aKnowsCompositor,
                         nsCString& aFailureReason)
    : Runnable("CreateDXVAManagerEvent")
    , mBackend(LayersBackend::LAYERS_D3D11)
    , mKnowsCompositor(aKnowsCompositor)
    , mFailureReason(aFailureReason)
  {
  }

  NS_IMETHOD Run() override {
    NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");
    const bool deblacklistingForTelemetry =
      XRE_IsGPUProcess() && gfxPrefs::PDMWMFDeblacklistingForTelemetryInGPUProcess();
    nsACString* failureReason = &mFailureReason;
    nsCString secondFailureReason;
    if (mBackend == LayersBackend::LAYERS_D3D11 &&
      gfxPrefs::PDMWMFAllowD3D11() && IsWin8OrLater()) {
      const nsCString& blacklistedDLL = FindD3D11BlacklistedDLL();
      if (!deblacklistingForTelemetry && !blacklistedDLL.IsEmpty()) {
        failureReason->AppendPrintf("D3D11 blacklisted with DLL %s",
                                    blacklistedDLL.get());
      } else {
        mDXVA2Manager =
          DXVA2Manager::CreateD3D11DXVA(mKnowsCompositor, *failureReason);
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
    if (!deblacklistingForTelemetry && !blacklistedDLL.IsEmpty()) {
      mFailureReason.AppendPrintf("D3D9 blacklisted with DLL %s",
                                  blacklistedDLL.get());
    } else {
      mDXVA2Manager =
        DXVA2Manager::CreateD3D9DXVA(mKnowsCompositor, *failureReason);
      // Make sure we include the messages from both attempts (if applicable).
      mFailureReason.Append(secondFailureReason);
    }
    return NS_OK;
  }
  nsAutoPtr<DXVA2Manager> mDXVA2Manager;
  layers::LayersBackend mBackend;
  layers::KnowsCompositor* mKnowsCompositor;
  nsACString& mFailureReason;
};

bool
WMFVideoMFTManager::InitializeDXVA()
{
  // If we use DXVA but aren't running with a D3D layer manager then the
  // readback of decoded video frames from GPU to CPU memory grinds painting
  // to a halt, and makes playback performance *worse*.
  if (!mDXVAEnabled) {
    mDXVAFailureReason.AssignLiteral(
      "Hardware video decoding disabled or blacklisted");
    return false;
  }
  MOZ_ASSERT(!mDXVA2Manager);
  LayersBackend backend = GetCompositorBackendType(mKnowsCompositor);
  bool useANGLE =
      mKnowsCompositor ? mKnowsCompositor->GetCompositorUseANGLE() : false;
  bool wrWithANGLE = (backend == LayersBackend::LAYERS_WR) && useANGLE;
  if (backend != LayersBackend::LAYERS_D3D11 && !wrWithANGLE) {
    mDXVAFailureReason.AssignLiteral("Unsupported layers backend");
    return false;
  }

  // The DXVA manager must be created on the main thread.
  RefPtr<CreateDXVAManagerEvent> event =
    new CreateDXVAManagerEvent(mKnowsCompositor,
                               mDXVAFailureReason);

  if (NS_IsMainThread()) {
    event->Run();
  } else {
    // This logic needs to run on the main thread
    mozilla::SyncRunnable::DispatchToThread(
      SystemGroup::EventTargetFor(mozilla::TaskCategory::Other), event);
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
  if (mStreamType == H264
      && (mVideoInfo.mImage.width < MIN_H264_FRAME_DIMENSION
          || mVideoInfo.mImage.height < MIN_H264_FRAME_DIMENSION)) {
    LogToBrowserConsole(NS_LITERAL_STRING(
      "Can't decode H.264 stream with width or height less than 48 pixels."));
    mIsValid = false;
  }

  return mIsValid;
}

already_AddRefed<MFTDecoder>
WMFVideoMFTManager::LoadAMDVP9Decoder()
{
  MOZ_ASSERT(mStreamType == VP9);

  RefPtr<MFTDecoder> decoder = new MFTDecoder();

  HRESULT hr = decoder->Create(CLSID_AMDWebmMfVp9Dec);
  if (SUCCEEDED(hr)) {
    return decoder.forget();
  }

  // Check if we can load the AMD VP9 decoder using the path name.
  nsString path = GetProgramW6432Path();
  path.Append(kAMDVPXDecoderDLLPath);
  path.Append(kAMDVP9DecoderDLLName);
  HMODULE decoderDLL = ::LoadLibraryEx(path.get(), NULL,
                                       LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!decoderDLL) {
    return nullptr;
  }
  hr = decoder->Create(decoderDLL, CLSID_AMDWebmMfVp9Dec);
  NS_ENSURE_TRUE(SUCCEEDED(hr), nullptr);
  return decoder.forget();
}

bool
WMFVideoMFTManager::Init()
{
  if (!ValidateVideoInfo()) {
    return false;
  }

  bool success = InitInternal();
  if (!success && mAMDVP9InUse) {
    // Something failed with the AMD VP9 decoder; attempt again defaulting back
    // to Microsoft MFT.
    mCheckForAMDDecoder = false;
    if (mDXVA2Manager) {
      DeleteOnMainThread(mDXVA2Manager);
    }
    success = InitInternal();
  }

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
WMFVideoMFTManager::InitInternal()
{
  mUseHwAccel = false; // default value; changed if D3D setup succeeds.
  bool useDxva = InitializeDXVA();

  RefPtr<MFTDecoder> decoder;

  HRESULT hr;
  if (mStreamType == VP9 && useDxva && mCheckForAMDDecoder &&
      MediaPrefs::PDMWMFAMDVP9DecoderEnabled()) {
    if ((decoder = LoadAMDVP9Decoder())) {
      mAMDVP9InUse = true;
    }
  }
  if (!decoder) {
    mCheckForAMDDecoder = false;
    mAMDVP9InUse = false;
    decoder = new MFTDecoder();
    hr = decoder->Create(GetMFTGUID());
    NS_ENSURE_TRUE(SUCCEEDED(hr), false);
  }

  RefPtr<IMFAttributes> attr(decoder->GetAttributes());
  UINT32 aware = 0;
  if (attr) {
    attr->GetUINT32(MF_SA_D3D_AWARE, &aware);
    attr->SetUINT32(CODECAPI_AVDecNumWorkerThreads,
      WMFDecoderModule::GetNumDecoderThreads());
    if (gfxPrefs::PDMWMFLowLatencyEnabled()) {
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
        mDXVAFailureReason = nsPrintfCString(
          "MFT_MESSAGE_SET_D3D_MANAGER failed with code %X", hr);
      }
    }
    else {
      mDXVAFailureReason.AssignLiteral(
        "Decoder returned false for MF_SA_D3D_AWARE");
    }
  }

  if (!mUseHwAccel) {
    // Use VP8/9 MFT only if HW acceleration is available
    if (mStreamType == VP9 || mStreamType == VP8) {
      return false;
    }
    Telemetry::Accumulate(Telemetry::MEDIA_DECODER_BACKEND_USED,
                          uint32_t(media::MediaDecoderBackend::WMFSoftware));
  }

  mDecoder = decoder;
  hr = SetDecoderMediaTypes();
  NS_ENSURE_TRUE(SUCCEEDED(hr), false);

  RefPtr<IMFMediaType> outputType;
  hr = mDecoder->GetOutputMediaType(outputType);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  if (mUseHwAccel && !CanUseDXVA(outputType)) {
    mDXVAEnabled = false;
    // DXVA initialization with current decoder actually failed,
    // re-do initialization.
    return InitInternal();
  }

  LOG("Video Decoder initialized, Using DXVA: %s",
      (mUseHwAccel ? "Yes" : "No"));

  if (mDXVA2Manager) {
    hr = mDXVA2Manager->ConfigureForSize(mVideoInfo.ImageRect().width,
                                         mVideoInfo.ImageRect().height);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  } else {
    GetDefaultStride(outputType, mVideoInfo.ImageRect().width, &mVideoStride);
  }
  LOG("WMFVideoMFTManager frame geometry stride=%u picture=(%d, %d, %d, %d) "
      "display=(%d,%d)",
      mVideoStride,
      mVideoInfo.ImageRect().x,
      mVideoInfo.ImageRect().y,
      mVideoInfo.ImageRect().width,
      mVideoInfo.ImageRect().height,
      mVideoInfo.mDisplay.width,
      mVideoInfo.mDisplay.height);

  if (!mUseHwAccel) {
    RefPtr<ID3D11Device> device =
      gfx::DeviceManagerDx::Get()->GetCompositorDevice();
    if (!device) {
      device = gfx::DeviceManagerDx::Get()->GetContentDevice();
    }
    if (device) {
      RefPtr<ID3D10Multithread> multi;
      HRESULT hr =
        device->QueryInterface((ID3D10Multithread**)getter_AddRefs(multi));
      if (SUCCEEDED(hr) && multi) {
        multi->SetMultithreadProtected(TRUE);
        mIMFUsable = true;
      }
    }
  }
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

  hr = inputType->SetUINT32(MF_MT_INTERLACE_MODE,
                            MFVideoInterlace_MixedInterlaceOrProgressive);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = inputType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = MFSetAttributeSize(inputType,
                          MF_MT_FRAME_SIZE,
                          mVideoInfo.ImageRect().width,
                          mVideoInfo.ImageRect().height);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  RefPtr<IMFMediaType> outputType;
  hr = wmf::MFCreateMediaType(getter_AddRefs(outputType));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = outputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

  hr = MFSetAttributeSize(outputType,
                          MF_MT_FRAME_SIZE,
                          mVideoInfo.ImageRect().width,
                          mVideoInfo.ImageRect().height);
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

  RefPtr<IMFSample> inputSample;
  HRESULT hr = mDecoder->CreateInputSample(aSample->Data(),
                                           uint32_t(aSample->Size()),
                                           aSample->mTime.ToMicroseconds(),
                                           &inputSample);
  NS_ENSURE_TRUE(SUCCEEDED(hr) && inputSample != nullptr, hr);

  mLastDuration = aSample->mDuration;
  mLastTime = aSample->mTime;
  mSamplesCount++;

  // Forward sample data to the decoder.
  return mDecoder->Input(inputSample);
}

class SupportsConfigEvent : public Runnable {
public:
  SupportsConfigEvent(DXVA2Manager* aDXVA2Manager,
                      IMFMediaType* aMediaType,
                      float aFramerate)
    : Runnable("SupportsConfigEvent")
    , mDXVA2Manager(aDXVA2Manager)
    , mMediaType(aMediaType)
    , mFramerate(aFramerate)
    , mSupportsConfig(false)
  {
  }

  NS_IMETHOD Run()
  {
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
  float framerate = 1000000.0 / mLastDuration.ToMicroseconds();

  // The supports config check must be done on the main thread since we have
  // a crash guard protecting it.
  RefPtr<SupportsConfigEvent> event =
    new SupportsConfigEvent(mDXVA2Manager, aType, framerate);

  if (NS_IsMainThread()) {
    event->Run();
  } else {
    // This logic needs to run on the main thread
    mozilla::SyncRunnable::DispatchToThread(
      SystemGroup::EventTargetFor(mozilla::TaskCategory::Other), event);
  }

  return event->mSupportsConfig;
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

  TimeUnit pts = GetSampleTime(aSample);
  NS_ENSURE_TRUE(pts.IsValid(), E_FAIL);
  TimeUnit duration = GetSampleDuration(aSample);
  NS_ENSURE_TRUE(duration.IsValid(), E_FAIL);
  gfx::IntRect pictureRegion =
    mVideoInfo.ScaledImageRect(videoWidth, videoHeight);

  LayersBackend backend = GetCompositorBackendType(mKnowsCompositor);
  if (backend != LayersBackend::LAYERS_D3D11 || !mIMFUsable) {
    RefPtr<VideoData> v =
      VideoData::CreateAndCopyData(mVideoInfo,
                                   mImageContainer,
                                   aStreamOffset,
                                   pts,
                                   duration,
                                   b,
                                   false,
                                   TimeUnit::FromMicroseconds(-1),
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
    VideoData::CreateFromImage(mVideoInfo.mDisplay,
                               aStreamOffset,
                               pts,
                               duration,
                               image.forget(),
                               false,
                               TimeUnit::FromMicroseconds(-1));

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

  gfx::IntRect pictureRegion =
    mVideoInfo.ScaledImageRect(mImageSize.width, mImageSize.height);
  RefPtr<Image> image;
  hr = mDXVA2Manager->CopyToImage(aSample,
                                  pictureRegion,
                                  getter_AddRefs(image));
  NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  NS_ENSURE_TRUE(image, E_FAIL);

  TimeUnit pts = GetSampleTime(aSample);
  NS_ENSURE_TRUE(pts.IsValid(), E_FAIL);
  TimeUnit duration = GetSampleDuration(aSample);
  NS_ENSURE_TRUE(duration.IsValid(), E_FAIL);
  RefPtr<VideoData> v = VideoData::CreateFromImage(mVideoInfo.mDisplay,
                                                   aStreamOffset,
                                                   pts,
                                                   duration,
                                                   image.forget(),
                                                   false,
                                                   TimeUnit::FromMicroseconds(-1));

  NS_ENSURE_TRUE(v, E_FAIL);
  v.forget(aOutVideoData);

  return S_OK;
}

// Blocks until decoded sample is produced by the decoder.
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

  TimeUnit pts;
  TimeUnit duration;

  // Loop until we decode a sample, or an unexpected error that we can't
  // handle occurs.
  while (true) {
    hr = mDecoder->Output(&sample);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
      return MF_E_TRANSFORM_NEED_MORE_INPUT;
    }

    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
      MOZ_ASSERT(!sample);
      // Video stream output type change, probably geometric aperture change.
      // We must reconfigure the decoder output type.
      hr = mDecoder->SetDecoderOutputType(false /* check all attribute */,
                                          nullptr,
                                          nullptr);
      NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

      if (!mUseHwAccel) {
        // The stride may have changed, recheck for it.
        RefPtr<IMFMediaType> outputType;
        hr = mDecoder->GetOutputMediaType(outputType);
        NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
        hr = GetDefaultStride(outputType, mVideoInfo.ImageRect().width,
                              &mVideoStride);
        NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
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
      pts = GetSampleTime(sample);
      duration = GetSampleDuration(sample);
      if (!pts.IsValid() || !duration.IsValid()) {
        return E_FAIL;
      }
      if (wasDraining && sampleCount == 1 && pts == TimeUnit::Zero()) {
        // WMF is unable to calculate a duration if only a single sample
        // was parsed. Additionally, the pts always comes out at 0 under those
        // circumstances.
        // Seeing that we've only fed the decoder a single frame, the pts
        // and duration are known, it's of the last sample.
        pts = mLastTime;
        duration = mLastDuration;
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
  aOutData->mTime = pts;
  // The VP9 decoder doesn't provide a valid duration. AS VP9 doesn't have a
  // concept of pts vs dts and have no latency. We can as such use the last
  // known input duration.
  aOutData->mDuration = (mStreamType == VP9 && duration == TimeUnit::Zero())
                        ? mLastDuration
                        : duration;

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

} // namespace mozilla
