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

#include "stdafx.h"

void LOG(wchar_t* format, ...)
{
  va_list args;
  va_start(args, format);

  WCHAR msg[MAX_PATH];
  if (FAILED(StringCbVPrintf(msg, sizeof(msg), format, args))) {
    return;
  }

  OutputDebugString(msg);
}


#ifdef TEST_DECODING

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

#endif

void dump(const uint8_t* data, uint32_t len, const char* filename)
{
  FILE* f = 0;
  fopen_s(&f, filename, "wb");
  fwrite(data, len, 1, f);
  fclose(f);
}

HRESULT
CreateMFT(const CLSID& clsid,
          const wchar_t* aDllName,
          CComPtr<IMFTransform>& aOutMFT)
{
  HMODULE module = ::GetModuleHandle(aDllName);
  if (!module) {
    LOG(L"Failed to get %S\n", aDllName);
    return E_FAIL;
  }

  typedef HRESULT (WINAPI* DllGetClassObjectFnPtr)(const CLSID& clsid,
                                                   const IID& iid,
                                                   void** object);

  DllGetClassObjectFnPtr GetClassObjPtr =
    reinterpret_cast<DllGetClassObjectFnPtr>(GetProcAddress(module, "DllGetClassObject"));
  if (!GetClassObjPtr) {
    LOG(L"Failed to get DllGetClassObject\n");
    return E_FAIL;
  }

  CComPtr<IClassFactory> classFactory;
  HRESULT hr = GetClassObjPtr(clsid,
                              __uuidof(IClassFactory),
                              reinterpret_cast<void**>(static_cast<IClassFactory**>(&classFactory)));
  if (FAILED(hr)) {
    LOG(L"Failed to get H264 IClassFactory\n");
    return E_FAIL;
  }

  hr = classFactory->CreateInstance(NULL,
                                    __uuidof(IMFTransform),
                                    reinterpret_cast<void**>(static_cast<IMFTransform**>(&aOutMFT)));
  if (FAILED(hr)) {
    LOG(L"Failed to get create MFT\n");
    return E_FAIL;
  }

  return S_OK;
}