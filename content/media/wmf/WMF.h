/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WMF_H_
#define WMF_H_

#if WINVER < _WIN32_WINNT_WIN7
#error \
You must include WMF.h before including mozilla headers, \
otherwise mozconfig.h will be included \
and that sets WINVER to WinXP, \
which makes Windows Media Foundation unavailable.
#endif

#pragma push_macro("WINVER")
#undef WINVER
#define WINVER _WIN32_WINNT_WIN7

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <stdio.h>
#include <mferror.h>
#include <propvarutil.h>
#include <wmcodecdsp.h>
#include <initguid.h>
#include <d3d9.h>
#include <dxva2api.h>

namespace mozilla {
namespace wmf {

// Loads/Unloads all the DLLs in which the WMF functions are located.
// The DLLs must be loaded before any of the WMF functions below will work.
// All the function definitions below are wrappers which locate the
// corresponding WMF function in the appropriate DLL (hence why LoadDLL()
// must be called first...).
HRESULT LoadDLLs();
HRESULT UnloadDLLs();

// All functions below are wrappers around the corresponding WMF function,
// and automatically locate and call the corresponding function in the WMF DLLs.

HRESULT MFStartup();

HRESULT MFShutdown();

HRESULT MFCreateAsyncResult(IUnknown *aUunkObject,
                            IMFAsyncCallback *aCallback,
                            IUnknown *aUnkState,
                            IMFAsyncResult **aOutAsyncResult);

HRESULT MFInvokeCallback(IMFAsyncResult *aAsyncResult);

HRESULT MFCreateMediaType(IMFMediaType **aOutMFType);

HRESULT MFCreateSourceReaderFromByteStream(IMFByteStream *aByteStream,
                                           IMFAttributes *aAttributes,
                                           IMFSourceReader **aOutSourceReader);

HRESULT PropVariantToUInt32(REFPROPVARIANT aPropvar, ULONG *aOutUL);

HRESULT PropVariantToInt64(REFPROPVARIANT aPropVar, LONGLONG *aOutLL);

HRESULT MFTGetInfo(CLSID aClsidMFT,
                   LPWSTR *aOutName,
                   MFT_REGISTER_TYPE_INFO **aOutInputTypes,
                   UINT32 *aOutNumInputTypes,
                   MFT_REGISTER_TYPE_INFO **aOutOutputTypes,
                   UINT32 *aOutNumOutputTypes,
                   IMFAttributes **aOutAttributes);

HRESULT MFGetStrideForBitmapInfoHeader(DWORD aFormat,
                                       DWORD aWidth,
                                       LONG *aOutStride);

// Note: We shouldn't use this in production code; it's really only
// here so we can compare behaviour of the SourceReader using WMF's
// byte stream and ours when debugging.
HRESULT MFCreateSourceReaderFromURL(LPCWSTR aURL,
                                    IMFAttributes *aAttributes,
                                    IMFSourceReader **aSourceReader);

HRESULT MFCreateAttributes(IMFAttributes **ppMFAttributes, UINT32 cInitialSize);

HRESULT MFGetPluginControl(IMFPluginControl **aOutPluginControl);

HRESULT MFTEnumEx(GUID guidCategory,
                  UINT32 Flags,
                  const MFT_REGISTER_TYPE_INFO *pInputType,
                  const MFT_REGISTER_TYPE_INFO *pOutputType,
                  IMFActivate ***pppMFTActivate,
                  UINT32 *pcMFTActivate);

HRESULT MFGetService(IUnknown *punkObject,
                     REFGUID guidService,
                     REFIID riid,
                     LPVOID *ppvObject);

HRESULT DXVA2CreateDirect3DDeviceManager9(UINT *pResetToken,
                                          IDirect3DDeviceManager9 **ppDXVAManager);
} // end namespace wmf
} // end namespace mozilla



#pragma pop_macro("WINVER")

#endif
