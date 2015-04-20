/*
 * Copyright 2013, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "WMFUtils.h"
#include "ClearKeyUtils.h"
#include <VersionHelpers.h>

#include <stdio.h>

#define INITGUID
#include <guiddef.h>

#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "wmcodecdspuuid")
#pragma comment(lib, "mfplat.lib")

void LOG(const char* format, ...)
{
#ifdef WMF_DECODER_LOG
  va_list args;
  va_start(args, format);
  vprintf(format, args);
#endif
}

#ifdef WMF_MUST_DEFINE_AAC_MFT_CLSID
// Some SDK versions don't define the AAC decoder CLSID.
// {32D186A7-218F-4C75-8876-DD77273A8999}
DEFINE_GUID(CLSID_CMSAACDecMFT, 0x32D186A7, 0x218F, 0x4C75, 0x88, 0x76, 0xDD, 0x77, 0x27, 0x3A, 0x89, 0x99);
#endif

DEFINE_GUID(CLSID_CMSH264DecMFT, 0x62CE7E72, 0x4C71, 0x4d20, 0xB1, 0x5D, 0x45, 0x28, 0x31, 0xA8, 0x7D, 0x9D);

namespace wmf {


#define MFPLAT_FUNC(_func, _dllname) \
  decltype(::_func)* _func;
#include "WMFSymbols.h"
#undef MFPLAT_FUNC

static bool
LinkMfplat()
{
  static bool sInitDone = false;
  static bool sInitOk = false;
  if (!sInitDone) {
    sInitDone = true;
    HMODULE handle;

#define MFPLAT_FUNC(_func, _dllname) \
      handle = GetModuleHandleA(_dllname); \
      if (!(_func = (decltype(_func))(GetProcAddress(handle, #_func)))) { \
        return false; \
      }

#include "WMFSymbols.h"
#undef MFPLAT_FUNC
    sInitOk = true;
  }
  return sInitOk;
}

const char*
WMFDecoderDllNameFor(CodecType aCodec)
{
  if (aCodec == H264) {
    // For H.264 decoding, we need msmpeg2vdec.dll on Win 7 & 8,
    // and mfh264dec.dll on Vista.
    if (IsWindows7OrGreater()) {
      return "msmpeg2vdec.dll";
    } else {
      return "mfh264dec.dll";
    }
  } else if (aCodec == AAC) {
    // For AAC decoding, we need to use msauddecmft.dll on Win8,
    // msmpeg2adec.dll on Win7, and msheaacdec.dll on Vista.
    if (IsWindows8OrGreater()) {
      return "msauddecmft.dll";
    } else if (IsWindows7OrGreater()) {
      return "msmpeg2adec.dll";
    } else {
      return "mfheaacdec.dll";
    }
  } else {
    return "";
  }
}


bool
EnsureLibs()
{
  static bool sInitDone = false;
  static bool sInitOk = false;
  if (!sInitDone) {
    sInitOk = LinkMfplat() &&
              !!GetModuleHandleA(WMFDecoderDllNameFor(AAC)) &&
              !!GetModuleHandleA(WMFDecoderDllNameFor(H264));
    sInitDone = true;
  }
  return sInitOk;
}

int32_t
MFOffsetToInt32(const MFOffset& aOffset)
{
  return int32_t(aOffset.value + (aOffset.fract / 65536.0f));
}

// Gets the sub-region of the video frame that should be displayed.
// See: http://msdn.microsoft.com/en-us/library/windows/desktop/bb530115(v=vs.85).aspx
HRESULT
GetPictureRegion(IMFMediaType* aMediaType, IntRect& aOutPictureRegion)
{
  // Determine if "pan and scan" is enabled for this media. If it is, we
  // only display a region of the video frame, not the entire frame.
  BOOL panScan = MFGetAttributeUINT32(aMediaType, MF_MT_PAN_SCAN_ENABLED, FALSE);

  // If pan and scan mode is enabled. Try to get the display region.
  HRESULT hr = E_FAIL;
  MFVideoArea videoArea;
  memset(&videoArea, 0, sizeof(MFVideoArea));
  if (panScan) {
    hr = aMediaType->GetBlob(MF_MT_PAN_SCAN_APERTURE,
                             (UINT8*)&videoArea,
                             sizeof(MFVideoArea),
                             NULL);
  }

  // If we're not in pan-and-scan mode, or the pan-and-scan region is not set,
  // check for a minimimum display aperture.
  if (!panScan || hr == MF_E_ATTRIBUTENOTFOUND) {
    hr = aMediaType->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE,
                             (UINT8*)&videoArea,
                             sizeof(MFVideoArea),
                             NULL);
  }

  if (hr == MF_E_ATTRIBUTENOTFOUND) {
    // Minimum display aperture is not set, for "backward compatibility with
    // some components", check for a geometric aperture.
    hr = aMediaType->GetBlob(MF_MT_GEOMETRIC_APERTURE,
                             (UINT8*)&videoArea,
                             sizeof(MFVideoArea),
                             NULL);
  }

  if (SUCCEEDED(hr)) {
    // The media specified a picture region, return it.
    aOutPictureRegion = IntRect(MFOffsetToInt32(videoArea.OffsetX),
                                  MFOffsetToInt32(videoArea.OffsetY),
                                  videoArea.Area.cx,
                                  videoArea.Area.cy);
    return S_OK;
  }

  // No picture region defined, fall back to using the entire video area.
  UINT32 width = 0, height = 0;
  hr = MFGetAttributeSize(aMediaType, MF_MT_FRAME_SIZE, &width, &height);
  ENSURE(SUCCEEDED(hr), hr);
  aOutPictureRegion = IntRect(0, 0, width, height);
  return S_OK;
}


HRESULT
GetDefaultStride(IMFMediaType *aType, uint32_t* aOutStride)
{
  // Try to get the default stride from the media type.
  HRESULT hr = aType->GetUINT32(MF_MT_DEFAULT_STRIDE, aOutStride);
  if (SUCCEEDED(hr)) {
    return S_OK;
  }

  // Stride attribute not set, calculate it.
  GUID subtype = GUID_NULL;
  uint32_t width = 0;
  uint32_t height = 0;

  hr = aType->GetGUID(MF_MT_SUBTYPE, &subtype);
  ENSURE(SUCCEEDED(hr), hr);

  hr = MFGetAttributeSize(aType, MF_MT_FRAME_SIZE, &width, &height);
  ENSURE(SUCCEEDED(hr), hr);

  hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, (LONG*)(aOutStride));
  ENSURE(SUCCEEDED(hr), hr);

  return hr;
}

void dump(const uint8_t* data, uint32_t len, const char* filename)
{
  FILE* f = 0;
  fopen_s(&f, filename, "wb");
  fwrite(data, len, 1, f);
  fclose(f);
}

HRESULT
CreateMFT(const CLSID& clsid,
          const char* aDllName,
          CComPtr<IMFTransform>& aOutMFT)
{
  HMODULE module = ::GetModuleHandleA(aDllName);
  if (!module) {
    LOG("Failed to get %S\n", aDllName);
    return E_FAIL;
  }

  typedef HRESULT (WINAPI* DllGetClassObjectFnPtr)(const CLSID& clsid,
                                                   const IID& iid,
                                                   void** object);

  DllGetClassObjectFnPtr GetClassObjPtr =
    reinterpret_cast<DllGetClassObjectFnPtr>(GetProcAddress(module, "DllGetClassObject"));
  if (!GetClassObjPtr) {
    LOG("Failed to get DllGetClassObject\n");
    return E_FAIL;
  }

  CComPtr<IClassFactory> classFactory;
  HRESULT hr = GetClassObjPtr(clsid,
                              __uuidof(IClassFactory),
                              reinterpret_cast<void**>(static_cast<IClassFactory**>(&classFactory)));
  if (FAILED(hr)) {
    LOG("Failed to get H264 IClassFactory\n");
    return E_FAIL;
  }

  hr = classFactory->CreateInstance(NULL,
                                    __uuidof(IMFTransform),
                                    reinterpret_cast<void**>(static_cast<IMFTransform**>(&aOutMFT)));
  if (FAILED(hr)) {
    LOG("Failed to get create MFT\n");
    return E_FAIL;
  }

  return S_OK;
}

} // namespace
