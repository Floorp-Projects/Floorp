/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFUtils.h"
#include "mozilla/StandardInteger.h"
#include "mozilla/RefPtr.h"
#include "prlog.h"
#include "nsThreadUtils.h"
#include "WinUtils.h"
#include "nsWindowsHelpers.h"

using namespace mozilla::widget;

namespace mozilla {

struct GuidToName {
  GUID guid;
  const char* name;
};

#define GUID_TO_NAME_ENTRY(g) { g, #g }
#define INTERFACE_TO_NAME_ENTRY(i) {IID_##i, #i }

GuidToName GuidToNameTable[] = {
  GUID_TO_NAME_ENTRY(MF_MT_MAJOR_TYPE),
  GUID_TO_NAME_ENTRY(MF_MT_MAJOR_TYPE),
  GUID_TO_NAME_ENTRY(MF_MT_SUBTYPE),
  GUID_TO_NAME_ENTRY(MF_MT_ALL_SAMPLES_INDEPENDENT),
  GUID_TO_NAME_ENTRY(MF_MT_FIXED_SIZE_SAMPLES),
  GUID_TO_NAME_ENTRY(MF_MT_COMPRESSED),
  GUID_TO_NAME_ENTRY(MF_MT_SAMPLE_SIZE),
  GUID_TO_NAME_ENTRY(MF_MT_WRAPPED_TYPE),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_NUM_CHANNELS),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_SAMPLES_PER_SECOND),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_AVG_BYTES_PER_SECOND),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_BLOCK_ALIGNMENT),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_BITS_PER_SAMPLE),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_SAMPLES_PER_BLOCK),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_CHANNEL_MASK),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_FOLDDOWN_MATRIX),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_WMADRC_PEAKREF),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_WMADRC_PEAKTARGET),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_WMADRC_AVGREF),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_WMADRC_AVGTARGET),
  GUID_TO_NAME_ENTRY(MF_MT_AUDIO_PREFER_WAVEFORMATEX),
  GUID_TO_NAME_ENTRY(MF_MT_AAC_PAYLOAD_TYPE),
  GUID_TO_NAME_ENTRY(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION),
  GUID_TO_NAME_ENTRY(MF_MT_FRAME_SIZE),
  GUID_TO_NAME_ENTRY(MF_MT_FRAME_RATE),
  GUID_TO_NAME_ENTRY(MF_MT_FRAME_RATE_RANGE_MAX),
  GUID_TO_NAME_ENTRY(MF_MT_FRAME_RATE_RANGE_MIN),
  GUID_TO_NAME_ENTRY(MF_MT_PIXEL_ASPECT_RATIO),
  GUID_TO_NAME_ENTRY(MF_MT_DRM_FLAGS),
  GUID_TO_NAME_ENTRY(MF_MT_PAD_CONTROL_FLAGS),
  GUID_TO_NAME_ENTRY(MF_MT_SOURCE_CONTENT_HINT),
  GUID_TO_NAME_ENTRY(MF_MT_VIDEO_CHROMA_SITING),
  GUID_TO_NAME_ENTRY(MF_MT_INTERLACE_MODE),
  GUID_TO_NAME_ENTRY(MF_MT_TRANSFER_FUNCTION),
  GUID_TO_NAME_ENTRY(MF_MT_VIDEO_PRIMARIES),
  GUID_TO_NAME_ENTRY(MF_MT_CUSTOM_VIDEO_PRIMARIES),
  GUID_TO_NAME_ENTRY(MF_MT_YUV_MATRIX),
  GUID_TO_NAME_ENTRY(MF_MT_VIDEO_LIGHTING),
  GUID_TO_NAME_ENTRY(MF_MT_VIDEO_NOMINAL_RANGE),
  GUID_TO_NAME_ENTRY(MF_MT_GEOMETRIC_APERTURE),
  GUID_TO_NAME_ENTRY(MF_MT_MINIMUM_DISPLAY_APERTURE),
  GUID_TO_NAME_ENTRY(MF_MT_PAN_SCAN_APERTURE),
  GUID_TO_NAME_ENTRY(MF_MT_PAN_SCAN_ENABLED),
  GUID_TO_NAME_ENTRY(MF_MT_AVG_BITRATE),
  GUID_TO_NAME_ENTRY(MF_MT_AVG_BIT_ERROR_RATE),
  GUID_TO_NAME_ENTRY(MF_MT_MAX_KEYFRAME_SPACING),
  GUID_TO_NAME_ENTRY(MF_MT_DEFAULT_STRIDE),
  GUID_TO_NAME_ENTRY(MF_MT_PALETTE),
  GUID_TO_NAME_ENTRY(MF_MT_USER_DATA),
  GUID_TO_NAME_ENTRY(MF_MT_AM_FORMAT_TYPE),
  GUID_TO_NAME_ENTRY(MF_MT_MPEG_START_TIME_CODE),
  GUID_TO_NAME_ENTRY(MF_MT_MPEG2_PROFILE),
  GUID_TO_NAME_ENTRY(MF_MT_MPEG2_LEVEL),
  GUID_TO_NAME_ENTRY(MF_MT_MPEG2_FLAGS),
  GUID_TO_NAME_ENTRY(MF_MT_MPEG_SEQUENCE_HEADER),
  GUID_TO_NAME_ENTRY(MF_MT_DV_AAUX_SRC_PACK_0),
  GUID_TO_NAME_ENTRY(MF_MT_DV_AAUX_CTRL_PACK_0),
  GUID_TO_NAME_ENTRY(MF_MT_DV_AAUX_SRC_PACK_1),
  GUID_TO_NAME_ENTRY(MF_MT_DV_AAUX_CTRL_PACK_1),
  GUID_TO_NAME_ENTRY(MF_MT_DV_VAUX_SRC_PACK),
  GUID_TO_NAME_ENTRY(MF_MT_DV_VAUX_CTRL_PACK),
  GUID_TO_NAME_ENTRY(MF_MT_ARBITRARY_HEADER),
  GUID_TO_NAME_ENTRY(MF_MT_ARBITRARY_FORMAT),
  GUID_TO_NAME_ENTRY(MF_MT_IMAGE_LOSS_TOLERANT),
  GUID_TO_NAME_ENTRY(MF_MT_MPEG4_SAMPLE_DESCRIPTION),
  GUID_TO_NAME_ENTRY(MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY),
  GUID_TO_NAME_ENTRY(MF_MT_ORIGINAL_4CC),
  GUID_TO_NAME_ENTRY(MF_MT_ORIGINAL_WAVE_FORMAT_TAG),

  GUID_TO_NAME_ENTRY(MFMediaType_Audio),
  GUID_TO_NAME_ENTRY(MFMediaType_Video),
  GUID_TO_NAME_ENTRY(MFMediaType_Protected),
  GUID_TO_NAME_ENTRY(MFMediaType_SAMI),
  GUID_TO_NAME_ENTRY(MFMediaType_Script),
  GUID_TO_NAME_ENTRY(MFMediaType_Image),
  GUID_TO_NAME_ENTRY(MFMediaType_HTML),
  GUID_TO_NAME_ENTRY(MFMediaType_Binary),
  GUID_TO_NAME_ENTRY(MFMediaType_FileTransfer),

  GUID_TO_NAME_ENTRY(MFVideoFormat_AI44),
  GUID_TO_NAME_ENTRY(MFVideoFormat_ARGB32),
  GUID_TO_NAME_ENTRY(MFVideoFormat_AYUV),
  GUID_TO_NAME_ENTRY(MFVideoFormat_DV25),
  GUID_TO_NAME_ENTRY(MFVideoFormat_DV50),
  GUID_TO_NAME_ENTRY(MFVideoFormat_DVH1),
  GUID_TO_NAME_ENTRY(MFVideoFormat_DVSD),
  GUID_TO_NAME_ENTRY(MFVideoFormat_DVSL),
  GUID_TO_NAME_ENTRY(MFVideoFormat_H264),
  GUID_TO_NAME_ENTRY(MFVideoFormat_I420),
  GUID_TO_NAME_ENTRY(MFVideoFormat_IYUV),
  GUID_TO_NAME_ENTRY(MFVideoFormat_M4S2),
  GUID_TO_NAME_ENTRY(MFVideoFormat_MJPG),
  GUID_TO_NAME_ENTRY(MFVideoFormat_MP43),
  GUID_TO_NAME_ENTRY(MFVideoFormat_MP4S),
  GUID_TO_NAME_ENTRY(MFVideoFormat_MP4V),
  GUID_TO_NAME_ENTRY(MFVideoFormat_MPG1),
  GUID_TO_NAME_ENTRY(MFVideoFormat_MSS1),
  GUID_TO_NAME_ENTRY(MFVideoFormat_MSS2),
  GUID_TO_NAME_ENTRY(MFVideoFormat_NV11),
  GUID_TO_NAME_ENTRY(MFVideoFormat_NV12),
  GUID_TO_NAME_ENTRY(MFVideoFormat_P010),
  GUID_TO_NAME_ENTRY(MFVideoFormat_P016),
  GUID_TO_NAME_ENTRY(MFVideoFormat_P210),
  GUID_TO_NAME_ENTRY(MFVideoFormat_P216),
  GUID_TO_NAME_ENTRY(MFVideoFormat_RGB24),
  GUID_TO_NAME_ENTRY(MFVideoFormat_RGB32),
  GUID_TO_NAME_ENTRY(MFVideoFormat_RGB555),
  GUID_TO_NAME_ENTRY(MFVideoFormat_RGB565),
  GUID_TO_NAME_ENTRY(MFVideoFormat_RGB8),
  GUID_TO_NAME_ENTRY(MFVideoFormat_UYVY),
  GUID_TO_NAME_ENTRY(MFVideoFormat_v210),
  GUID_TO_NAME_ENTRY(MFVideoFormat_v410),
  GUID_TO_NAME_ENTRY(MFVideoFormat_WMV1),
  GUID_TO_NAME_ENTRY(MFVideoFormat_WMV2),
  GUID_TO_NAME_ENTRY(MFVideoFormat_WMV3),
  GUID_TO_NAME_ENTRY(MFVideoFormat_WVC1),
  GUID_TO_NAME_ENTRY(MFVideoFormat_Y210),
  GUID_TO_NAME_ENTRY(MFVideoFormat_Y216),
  GUID_TO_NAME_ENTRY(MFVideoFormat_Y410),
  GUID_TO_NAME_ENTRY(MFVideoFormat_Y416),
  GUID_TO_NAME_ENTRY(MFVideoFormat_Y41P),
  GUID_TO_NAME_ENTRY(MFVideoFormat_Y41T),
  GUID_TO_NAME_ENTRY(MFVideoFormat_YUY2),
  GUID_TO_NAME_ENTRY(MFVideoFormat_YV12),
  GUID_TO_NAME_ENTRY(MFVideoFormat_YVYU),

  GUID_TO_NAME_ENTRY(MFAudioFormat_PCM),
  GUID_TO_NAME_ENTRY(MFAudioFormat_Float),
  GUID_TO_NAME_ENTRY(MFAudioFormat_DTS),
  GUID_TO_NAME_ENTRY(MFAudioFormat_Dolby_AC3_SPDIF),
  GUID_TO_NAME_ENTRY(MFAudioFormat_DRM),
  GUID_TO_NAME_ENTRY(MFAudioFormat_WMAudioV8),
  GUID_TO_NAME_ENTRY(MFAudioFormat_WMAudioV9),
  GUID_TO_NAME_ENTRY(MFAudioFormat_WMAudio_Lossless),
  GUID_TO_NAME_ENTRY(MFAudioFormat_WMASPDIF),
  GUID_TO_NAME_ENTRY(MFAudioFormat_MSP1),
  GUID_TO_NAME_ENTRY(MFAudioFormat_MP3),
  GUID_TO_NAME_ENTRY(MFAudioFormat_MPEG),
  GUID_TO_NAME_ENTRY(MFAudioFormat_AAC),
  GUID_TO_NAME_ENTRY(MFAudioFormat_ADTS),

  // Interfaces which may be implemented by WMFByteStream.
  INTERFACE_TO_NAME_ENTRY(IUnknown),
  INTERFACE_TO_NAME_ENTRY(IMFByteStream),
  INTERFACE_TO_NAME_ENTRY(IMFMediaSource),
  INTERFACE_TO_NAME_ENTRY(IMFAttributes),
  INTERFACE_TO_NAME_ENTRY(IMFByteStreamBuffering),
};

nsCString GetGUIDName(const GUID& guid)
{
  unsigned numTypes = NS_ARRAY_LENGTH(GuidToNameTable);
  for (unsigned i = 0; i < numTypes; i++) {
    if (guid == GuidToNameTable[i].guid) {
      return nsDependentCString(GuidToNameTable[i].name);
    }
  }

  WCHAR* name = nullptr;
  HRESULT hr = StringFromCLSID(guid , &name);
  if (FAILED(hr)) {
    return nsDependentCString("GuidUnknown");
  }
  nsCString name_u8(NS_ConvertUTF16toUTF8(nsDependentString((PRUnichar*)(name))));
  CoTaskMemFree(name);
  return name_u8;
}

bool
SourceReaderHasStream(IMFSourceReader* aReader, const DWORD aIndex)
{
  RefPtr<IMFMediaType> nativeType;
  HRESULT hr = aReader->GetNativeMediaType(aIndex, 0, byRef(nativeType));
  return FAILED(hr) ? false : true;
}

HRESULT
DoGetInterface(IUnknown* aUnknown, void** aInterface)
{
  if (!aInterface)
    return E_POINTER;
  *aInterface = aUnknown;
  aUnknown->AddRef();
  return S_OK;
}

namespace wmf {

// Some SDK versions don't define the AAC decoder CLSID.
#ifndef CLSID_CMSAACDecMFT
// {32D186A7-218F-4C75-8876-DD77273A8999}
DEFINE_GUID(CLSID_CMSAACDecMFT, 0x32D186A7, 0x218F, 0x4C75, 0x88, 0x76, 0xDD, 0x77, 0x27, 0x3A, 0x89, 0x99);
#endif

static bool
IsSupportedDecoder(const GUID& aDecoderGUID)
{
  return aDecoderGUID == CLSID_CMSH264DecoderMFT ||
         aDecoderGUID == CLSID_CMSAACDecMFT ||
         aDecoderGUID == CLSID_CMP3DecMediaObject;
}

static HRESULT
DisableBlockedDecoders(IMFPluginControl* aPluginControl,
                       const GUID& aCategory)
{
  HRESULT hr = S_OK;

  UINT32 numMFTs = 0;
  IMFActivate **ppActivate = NULL;
  hr = wmf::MFTEnumEx(aCategory,
                      MFT_ENUM_FLAG_ALL,
                      nullptr, // Input type, nullptr -> match all.
                      nullptr, // Output type, nullptr -> match all.
                      &ppActivate,
                      &numMFTs);

  if (SUCCEEDED(hr) && numMFTs == 0) {
    hr = MF_E_TOPO_CODEC_NOT_FOUND;
  }

  for (UINT32 i = 0; i < numMFTs; i++) {
    // Note: We must release all IMFActivate objects in the list, hence
    // we're putting individual IMFActivate objects in into a smart ptr.
    RefPtr<IMFActivate> activate = ppActivate[i];
    GUID guid = GUID_NULL;
    hr = activate->GetGUID(MFT_TRANSFORM_CLSID_Attribute, &guid);
    if (FAILED(hr)) {
      NS_WARNING("FAILED to get IMFActivate clsid");
      continue;
    }
    if (!IsSupportedDecoder(guid)) {
      hr = aPluginControl->SetDisabled(MF_Plugin_Type_MFT, guid, TRUE);
      NS_ASSERTION(SUCCEEDED(hr), "Failed to disable plugin!");
    }
  }
  CoTaskMemFree(ppActivate);

  return hr;
}

static HRESULT
DisableBlockedDecoders()
{
  RefPtr<IMFPluginControl> pluginControl;
  HRESULT hr = wmf::MFGetPluginControl(byRef(pluginControl));
  if (SUCCEEDED(hr) && pluginControl) {
    hr = DisableBlockedDecoders(pluginControl,
                                MFT_CATEGORY_VIDEO_DECODER);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);

    hr = DisableBlockedDecoders(pluginControl,
                                MFT_CATEGORY_AUDIO_DECODER);
    NS_ENSURE_TRUE(SUCCEEDED(hr), hr);
  }

  return S_OK;
}

static bool sDLLsLoaded = false;
static bool sFailedToLoadDlls = false;

struct WMFModule {
  const wchar_t* name;
  HMODULE handle;
};

static WMFModule sDLLs[] = {
  { L"mfplat.dll", NULL },
  { L"mfreadwrite.dll", NULL },
  { L"propsys.dll", NULL },
  { L"mf.dll", NULL },
  { L"dxva2.dll", NULL }
};

HRESULT
LoadDLLs()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (sDLLsLoaded) {
    return S_OK;
  }
  if (sFailedToLoadDlls) {
    return E_FAIL;
  }

  // Try to load all the required DLLs.
  uint32_t dllLength = NS_ARRAY_LENGTH(sDLLs);
  for (uint32_t i = 0; i < dllLength; i++) {
    sDLLs[i].handle = LoadLibrarySystem32(sDLLs[i].name);
    if (!sDLLs[i].handle) {
      sFailedToLoadDlls = true;
      NS_WARNING("Failed to load WMF DLLs");
      UnloadDLLs();
      return E_FAIL;
    }
  }

  // Enumerate all the decoders on the system, and disable the ones except
  // those which we expect to use, the MP3, AAC and H.264 decoders.
  if (FAILED(DisableBlockedDecoders())) {
    sFailedToLoadDlls = true;
    NS_WARNING("Failed to disable non whitelisted WMF decoders");
    UnloadDLLs();
    return E_FAIL;
  }

  sDLLsLoaded = true;

  return S_OK;
}

HRESULT
UnloadDLLs()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  uint32_t length = NS_ARRAY_LENGTH(sDLLs);
  for (uint32_t i = 0; i < length; i++) {
    if (sDLLs[i].handle) {
      FreeLibrary(sDLLs[i].handle);
      sDLLs[i].handle = NULL;
    }
    sDLLsLoaded = false;
  }
  return S_OK;
}

#define ENSURE_FUNCTION_PTR(FunctionName, DLL) \
  static FunctionName##Ptr_t FunctionName##Ptr = nullptr; \
  if (!FunctionName##Ptr) { \
    FunctionName##Ptr = (FunctionName##Ptr_t)GetProcAddress(GetModuleHandle( #DLL ), #FunctionName ); \
    if (!FunctionName##Ptr) { \
      NS_WARNING("Failed to get GetProcAddress of " #FunctionName " from " #DLL ); \
      return E_FAIL; \
    } \
  }

#define DECL_FUNCTION_PTR(FunctionName, ...) \
  typedef HRESULT (STDMETHODCALLTYPE * FunctionName##Ptr_t)(__VA_ARGS__)

HRESULT
MFStartup()
{
  const int MF_VISTA_VERSION = (0x0001 << 16 | MF_API_VERSION);
  const int MF_WIN7_VERSION = (0x0002 << 16 | MF_API_VERSION);

  DECL_FUNCTION_PTR(MFStartup, ULONG, DWORD);
  ENSURE_FUNCTION_PTR(MFStartup, Mfplat.dll)
  if (WinUtils::GetWindowsVersion() == WinUtils::VISTA_VERSION)
    return MFStartupPtr(MF_VISTA_VERSION, MFSTARTUP_FULL);
  else
    return MFStartupPtr(MF_WIN7_VERSION, MFSTARTUP_FULL);
}

HRESULT
MFShutdown()
{
  DECL_FUNCTION_PTR(MFShutdown);
  ENSURE_FUNCTION_PTR(MFShutdown, Mfplat.dll)
  return (MFShutdownPtr)();
}

HRESULT
MFCreateAsyncResult(IUnknown *aUnkObject,
                    IMFAsyncCallback *aCallback,
                    IUnknown *aUnkState,
                    IMFAsyncResult **aOutAsyncResult)
{
  DECL_FUNCTION_PTR(MFCreateAsyncResult, IUnknown*, IMFAsyncCallback*, IUnknown*, IMFAsyncResult**);
  ENSURE_FUNCTION_PTR(MFCreateAsyncResult, Mfplat.dll)
  return (MFCreateAsyncResultPtr)(aUnkObject, aCallback, aUnkState, aOutAsyncResult);
}

HRESULT
MFInvokeCallback(IMFAsyncResult *aAsyncResult)
{
  DECL_FUNCTION_PTR(MFInvokeCallback, IMFAsyncResult*);
  ENSURE_FUNCTION_PTR(MFInvokeCallback, Mfplat.dll);
  return (MFInvokeCallbackPtr)(aAsyncResult);
}

HRESULT
MFCreateMediaType(IMFMediaType **aOutMFType)
{
  DECL_FUNCTION_PTR(MFCreateMediaType, IMFMediaType**);
  ENSURE_FUNCTION_PTR(MFCreateMediaType, Mfplat.dll)
  return (MFCreateMediaTypePtr)(aOutMFType);
}

HRESULT
MFCreateSourceReaderFromByteStream(IMFByteStream *aByteStream,
                                   IMFAttributes *aAttributes,
                                   IMFSourceReader **aOutSourceReader)
{
  DECL_FUNCTION_PTR(MFCreateSourceReaderFromByteStream, IMFByteStream*, IMFAttributes*, IMFSourceReader**);
  ENSURE_FUNCTION_PTR(MFCreateSourceReaderFromByteStream, Mfreadwrite.dll)
  return (MFCreateSourceReaderFromByteStreamPtr)(aByteStream,
                                                 aAttributes,
                                                 aOutSourceReader);
}

HRESULT
PropVariantToUInt32(REFPROPVARIANT aPropvar, ULONG *aOutUL)
{
  DECL_FUNCTION_PTR(PropVariantToUInt32, REFPROPVARIANT, ULONG *);
  ENSURE_FUNCTION_PTR(PropVariantToUInt32, Propsys.dll)
  return (PropVariantToUInt32Ptr)(aPropvar, aOutUL);
}

HRESULT PropVariantToInt64(REFPROPVARIANT aPropVar, LONGLONG *aOutLL)
{
  DECL_FUNCTION_PTR(PropVariantToInt64, REFPROPVARIANT, LONGLONG *);
  ENSURE_FUNCTION_PTR(PropVariantToInt64, Propsys.dll)
  return (PropVariantToInt64Ptr)(aPropVar, aOutLL);
}

HRESULT
MFTGetInfo(CLSID aClsidMFT,
           LPWSTR *aOutName,
           MFT_REGISTER_TYPE_INFO **aOutInputTypes,
           UINT32 *aOutNumInputTypes,
           MFT_REGISTER_TYPE_INFO **aOutOutputTypes,
           UINT32 *aOutNumOutputTypes,
           IMFAttributes **aOutAttributes)
{
  DECL_FUNCTION_PTR(MFTGetInfo, CLSID, LPWSTR*, MFT_REGISTER_TYPE_INFO**, UINT32*, MFT_REGISTER_TYPE_INFO**, UINT32*, IMFAttributes**);
  ENSURE_FUNCTION_PTR(MFTGetInfo, Mfplat.dll)
  return (MFTGetInfoPtr)(aClsidMFT,
                         aOutName,
                         aOutInputTypes,
                         aOutNumInputTypes,
                         aOutOutputTypes,
                         aOutNumOutputTypes,
                         aOutAttributes);
}

HRESULT
MFGetStrideForBitmapInfoHeader(DWORD aFormat,
                               DWORD aWidth,
                               LONG *aOutStride)
{
  DECL_FUNCTION_PTR(MFGetStrideForBitmapInfoHeader, DWORD, DWORD, LONG*);
  ENSURE_FUNCTION_PTR(MFGetStrideForBitmapInfoHeader, Mfplat.dll)
  return (MFGetStrideForBitmapInfoHeaderPtr)(aFormat, aWidth, aOutStride);
}

HRESULT
MFCreateSourceReaderFromURL(LPCWSTR aURL,
                            IMFAttributes *aAttributes,
                            IMFSourceReader **aSourceReader)
{
  DECL_FUNCTION_PTR(MFCreateSourceReaderFromURL, LPCWSTR, IMFAttributes*, IMFSourceReader**);
  ENSURE_FUNCTION_PTR(MFCreateSourceReaderFromURL, Mfreadwrite.dll)
  return (MFCreateSourceReaderFromURLPtr)(aURL, aAttributes, aSourceReader);
}

HRESULT
MFCreateAttributes(IMFAttributes **ppMFAttributes, UINT32 cInitialSize)
{
  DECL_FUNCTION_PTR(MFCreateAttributes, IMFAttributes**, UINT32);
  ENSURE_FUNCTION_PTR(MFCreateAttributes, mfplat.dll)
  return (MFCreateAttributesPtr)(ppMFAttributes, cInitialSize);
}

HRESULT
MFGetPluginControl(IMFPluginControl **aOutPluginControl)
{
  DECL_FUNCTION_PTR(MFGetPluginControl, IMFPluginControl **);
  ENSURE_FUNCTION_PTR(MFGetPluginControl, mfplat.dll)
  return (MFGetPluginControlPtr)(aOutPluginControl);
}

HRESULT
MFTEnumEx(GUID guidCategory,
          UINT32 Flags,
          const MFT_REGISTER_TYPE_INFO *pInputType,
          const MFT_REGISTER_TYPE_INFO *pOutputType,
          IMFActivate ***pppMFTActivate,
          UINT32 *pcMFTActivate)
{
  DECL_FUNCTION_PTR(MFTEnumEx, GUID, UINT32, const MFT_REGISTER_TYPE_INFO *, const MFT_REGISTER_TYPE_INFO *, IMFActivate ***, UINT32 *);
  ENSURE_FUNCTION_PTR(MFTEnumEx, mfplat.dll)
  return (MFTEnumExPtr)(guidCategory, Flags, pInputType, pOutputType, pppMFTActivate, pcMFTActivate);
}

HRESULT MFGetService(IUnknown *punkObject,
                     REFGUID guidService,
                     REFIID riid,
                     LPVOID *ppvObject)
{
  DECL_FUNCTION_PTR(MFGetService, IUnknown*, REFGUID, REFIID, LPVOID *);
  ENSURE_FUNCTION_PTR(MFGetService, mf.dll)
  return (MFGetServicePtr)(punkObject, guidService, riid, ppvObject);
}

HRESULT
DXVA2CreateDirect3DDeviceManager9(UINT *pResetToken,
                                  IDirect3DDeviceManager9 **ppDXVAManager)
{
  DECL_FUNCTION_PTR(DXVA2CreateDirect3DDeviceManager9, UINT*, IDirect3DDeviceManager9 **);
  ENSURE_FUNCTION_PTR(DXVA2CreateDirect3DDeviceManager9, dxva2.dll)
  return (DXVA2CreateDirect3DDeviceManager9Ptr)(pResetToken, ppDXVAManager);
}

} // end namespace wmf
} // end namespace mozilla
