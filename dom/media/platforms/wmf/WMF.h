/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WMF_H_
#define WMF_H_

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfobjects.h>
#include <ks.h>
#include <stdio.h>
#include <mferror.h>
#include <propvarutil.h>
#include <wmcodecdsp.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <wmcodecdsp.h>
#include <codecapi.h>

// The Windows headers helpfully declare min and max macros, which don't
// compile in the presence of std::min and std::max and unified builds.
// So undef them here.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// Some SDK versions don't define the AAC decoder CLSID.
#ifndef CLSID_CMSAACDecMFT
extern "C" const CLSID CLSID_CMSAACDecMFT;
#define WMF_MUST_DEFINE_AAC_MFT_CLSID
#endif

namespace mozilla {
namespace wmf {

// If successful, loads all required WMF DLLs and calls the WMF MFStartup()
// function. This delegates the WMF MFStartup() call to the MTA thread if
// the current thread is not MTA. This is to ensure we always interact with
// WMF from threads with the same COM compartment model.
HRESULT MFStartup();

// Calls the WMF MFShutdown() function. Call this once for every time
// wmf::MFStartup() succeeds. Note: does not unload the WMF DLLs loaded by
// MFStartup(); leaves them in memory to save I/O at next MFStartup() call.
// This delegates the WMF MFShutdown() call to the MTA thread if the current
// thread is not MTA. This is to ensure we always interact with
// WMF from threads with the same COM compartment model.
HRESULT MFShutdown();

// All functions below are wrappers around the corresponding WMF function,
// and automatically locate and call the corresponding function in the WMF DLLs.

HRESULT MFCreateMediaType(IMFMediaType **aOutMFType);

HRESULT MFGetStrideForBitmapInfoHeader(DWORD aFormat,
                                       DWORD aWidth,
                                       LONG *aOutStride);

HRESULT MFGetService(IUnknown *punkObject,
                     REFGUID guidService,
                     REFIID riid,
                     LPVOID *ppvObject);

HRESULT DXVA2CreateDirect3DDeviceManager9(UINT *pResetToken,
                                          IDirect3DDeviceManager9 **ppDXVAManager);


HRESULT MFCreateDXGIDeviceManager(UINT *pResetToken, IMFDXGIDeviceManager **ppDXVAManager);

HRESULT MFCreateSample(IMFSample **ppIMFSample);

HRESULT MFCreateAlignedMemoryBuffer(DWORD cbMaxLength,
                                    DWORD fAlignmentFlags,
                                    IMFMediaBuffer **ppBuffer);

HRESULT MFCreateDXGISurfaceBuffer(REFIID riid,
                                  IUnknown *punkSurface,
                                  UINT uSubresourceIndex,
                                  BOOL fButtomUpWhenLinear,
                                  IMFMediaBuffer **ppBuffer);

} // end namespace wmf
} // end namespace mozilla

#endif
