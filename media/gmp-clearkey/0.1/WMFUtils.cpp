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

#define INITGUID
#include <guiddef.h>
#include <stdio.h>
#include <versionhelpers.h>

#include <algorithm>

#include "ClearKeyUtils.h"

#ifndef __MINGW32__
#  pragma comment(lib, "mfuuid.lib")
#  pragma comment(lib, "wmcodecdspuuid")
#endif

void LOG(const char* format, ...) {
#ifdef WMF_DECODER_LOG
  va_list args;
  va_start(args, format);
  vprintf(format, args);
#endif
}

DEFINE_GUID(CLSID_CMSH264DecMFT, 0x62CE7E72, 0x4C71, 0x4d20, 0xB1, 0x5D, 0x45,
            0x28, 0x31, 0xA8, 0x7D, 0x9D);

namespace wmf {

#define MFPLAT_FUNC(_func, _dllname) decltype(::_func)* _func;
#include "WMFSymbols.h"
#undef MFPLAT_FUNC

static bool LinkMfplat() {
  static bool sInitDone = false;
  static bool sInitOk = false;
  if (!sInitDone) {
    sInitDone = true;
    HMODULE handle;

#define MFPLAT_FUNC(_func, _dllname)                                  \
  handle = GetModuleHandleA(_dllname);                                \
  if (!(_func = (decltype(_func))(GetProcAddress(handle, #_func)))) { \
    return false;                                                     \
  }

#include "WMFSymbols.h"
#undef MFPLAT_FUNC
    sInitOk = true;
  }
  return sInitOk;
}

bool EnsureLibs() {
  static bool sInitDone = false;
  static bool sInitOk = false;
  if (!sInitDone) {
    sInitOk = LinkMfplat() && !!GetModuleHandleA(WMFDecoderDllName());
    sInitDone = true;
  }
  return sInitOk;
}

int32_t MFOffsetToInt32(const MFOffset& aOffset) {
  return int32_t(aOffset.value + (aOffset.fract / 65536.0f));
}

// Gets the sub-region of the video frame that should be displayed.
// See:
// http://msdn.microsoft.com/en-us/library/windows/desktop/bb530115(v=vs.85).aspx
HRESULT
GetPictureRegion(IMFMediaType* aMediaType, IntRect& aOutPictureRegion) {
  // Determine if "pan and scan" is enabled for this media. If it is, we
  // only display a region of the video frame, not the entire frame.
  BOOL panScan =
      MFGetAttributeUINT32(aMediaType, MF_MT_PAN_SCAN_ENABLED, FALSE);

  // If pan and scan mode is enabled. Try to get the display region.
  HRESULT hr = E_FAIL;
  MFVideoArea videoArea;
  memset(&videoArea, 0, sizeof(MFVideoArea));
  if (panScan) {
    hr = aMediaType->GetBlob(MF_MT_PAN_SCAN_APERTURE, (UINT8*)&videoArea,
                             sizeof(MFVideoArea), NULL);
  }

  // If we're not in pan-and-scan mode, or the pan-and-scan region is not set,
  // check for a minimimum display aperture.
  if (!panScan || hr == MF_E_ATTRIBUTENOTFOUND) {
    hr = aMediaType->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&videoArea,
                             sizeof(MFVideoArea), NULL);
  }

  if (hr == MF_E_ATTRIBUTENOTFOUND) {
    // Minimum display aperture is not set, for "backward compatibility with
    // some components", check for a geometric aperture.
    hr = aMediaType->GetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&videoArea,
                             sizeof(MFVideoArea), NULL);
  }

  if (SUCCEEDED(hr)) {
    // The media specified a picture region, return it.
    IntRect picture = IntRect(MFOffsetToInt32(videoArea.OffsetX),
                              MFOffsetToInt32(videoArea.OffsetY),
                              videoArea.Area.cx, videoArea.Area.cy);
    ENSURE(picture.width <= mozilla::MAX_VIDEO_WIDTH, E_FAIL);
    ENSURE(picture.height <= mozilla::MAX_VIDEO_HEIGHT, E_FAIL);
    aOutPictureRegion = picture;
    return S_OK;
  }

  // No picture region defined, fall back to using the entire video area.
  UINT32 width = 0, height = 0;
  hr = MFGetAttributeSize(aMediaType, MF_MT_FRAME_SIZE, &width, &height);
  ENSURE(SUCCEEDED(hr), hr);
  ENSURE(width <= mozilla::MAX_VIDEO_WIDTH, E_FAIL);
  ENSURE(height <= mozilla::MAX_VIDEO_HEIGHT, E_FAIL);
  aOutPictureRegion = IntRect(0, 0, width, height);
  return S_OK;
}

HRESULT
GetDefaultStride(IMFMediaType* aType, uint32_t* aOutStride) {
  // Try to get the default stride from the media type.
  UINT32 stride = 0;
  HRESULT hr = aType->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride);
  if (SUCCEEDED(hr)) {
    ENSURE(stride <= mozilla::MAX_VIDEO_WIDTH, E_FAIL);
    *aOutStride = stride;
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
  ENSURE(width <= mozilla::MAX_VIDEO_WIDTH, E_FAIL);
  ENSURE(height <= mozilla::MAX_VIDEO_HEIGHT, E_FAIL);

  LONG lstride = 0;
  hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, &lstride);
  ENSURE(SUCCEEDED(hr), hr);
  ENSURE(lstride <= mozilla::MAX_VIDEO_WIDTH, E_FAIL);
  ENSURE(lstride >= 0, E_FAIL);
  *aOutStride = lstride;

  return hr;
}

void dump(const uint8_t* data, uint32_t len, const char* filename) {
  FILE* f = 0;
  fopen_s(&f, filename, "wb");
  fwrite(data, len, 1, f);
  fclose(f);
}

HRESULT
CreateMFT(const CLSID& clsid, const char* aDllName,
          CComPtr<IMFTransform>& aOutMFT) {
  HMODULE module = ::GetModuleHandleA(aDllName);
  if (!module) {
    LOG("Failed to get %S\n", aDllName);
    return E_FAIL;
  }

  typedef HRESULT(WINAPI * DllGetClassObjectFnPtr)(
      const CLSID& clsid, const IID& iid, void** object);

  DllGetClassObjectFnPtr GetClassObjPtr =
      reinterpret_cast<DllGetClassObjectFnPtr>(
          GetProcAddress(module, "DllGetClassObject"));
  if (!GetClassObjPtr) {
    LOG("Failed to get DllGetClassObject\n");
    return E_FAIL;
  }

  CComPtr<IClassFactory> classFactory;
  HRESULT hr = GetClassObjPtr(
      clsid, __uuidof(IClassFactory),
      reinterpret_cast<void**>(static_cast<IClassFactory**>(&classFactory)));
  if (FAILED(hr)) {
    LOG("Failed to get H264 IClassFactory\n");
    return E_FAIL;
  }

  hr = classFactory->CreateInstance(
      NULL, __uuidof(IMFTransform),
      reinterpret_cast<void**>(static_cast<IMFTransform**>(&aOutMFT)));
  if (FAILED(hr)) {
    LOG("Failed to get create MFT\n");
    return E_FAIL;
  }

  return S_OK;
}

int32_t GetNumThreads(int32_t aCoreCount) {
  return aCoreCount > 4 ? -1 : (std::max)(aCoreCount - 1, 1);
}

}  // namespace wmf
