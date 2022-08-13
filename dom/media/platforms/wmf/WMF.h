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

#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/StaticMutex.h"
#include "nsThreadUtils.h"

// The Windows headers helpfully declare min and max macros, which don't
// compile in the presence of std::min and std::max and unified builds.
// So undef them here.
#ifdef min
#  undef min
#endif
#ifdef max
#  undef max
#endif

// https://stackoverflow.com/questions/25759700/ms-format-tag-for-opus-codec
#ifndef MFAudioFormat_Opus
DEFINE_GUID(MFAudioFormat_Opus, WAVE_FORMAT_OPUS, 0x000, 0x0010, 0x80, 0x00,
            0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
#endif

namespace mozilla::wmf {

// A helper class for automatically starting and shuting down the Media
// Foundation. Prior to using Media Foundation in a process, users should call
// MediaFoundationInitializer::HasInitialized() to ensure Media Foundation is
// initialized. Users should also check the result of this call, in case the
// internal call to MFStartup fails. The first check to HasInitialized will
// cause the helper to start up Media Foundation and set up a runnable to handle
// Media Foundation shutdown at XPCOM shutdown. Calls after the first will not
// cause any extra startups or shutdowns, so it's safe to check multiple times
// in the same process. Users do not need to do any manual shutdown, the helper
// will handle this internally.
class MediaFoundationInitializer final {
 public:
  ~MediaFoundationInitializer() {
    if (mHasInitialized) {
      if (FAILED(MFShutdown())) {
        NS_WARNING("MFShutdown failed");
      }
    }
  }
  static bool HasInitialized() {
    if (sIsShutdown) {
      return false;
    }
    return Get()->mHasInitialized;
  }

 private:
  static MediaFoundationInitializer* Get() {
    {
      StaticMutexAutoLock lock(sCreateMutex);
      if (!sInitializer) {
        sInitializer.reset(new MediaFoundationInitializer());
        GetMainThreadSerialEventTarget()->Dispatch(
            NS_NewRunnableFunction("MediaFoundationInitializer::Get", [&] {
              // Need to run this before MTA thread gets destroyed.
              RunOnShutdown(
                  [&] {
                    sInitializer.reset();
                    sIsShutdown = true;
                  },
                  ShutdownPhase::XPCOMShutdown);
            }));
      }
    }
    return sInitializer.get();
  }

  MediaFoundationInitializer() : mHasInitialized(SUCCEEDED(MFStartup())) {
    if (!mHasInitialized) {
      NS_WARNING("MFStartup failed");
    }
  }

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

  static inline UniquePtr<MediaFoundationInitializer> sInitializer;
  static inline StaticMutex sCreateMutex;
  static inline Atomic<bool> sIsShutdown{false};
  const bool mHasInitialized;
};

// All functions below are wrappers around the corresponding WMF function,
// and automatically locate and call the corresponding function in the WMF DLLs.

HRESULT MFCreateMediaType(IMFMediaType** aOutMFType);

HRESULT MFGetStrideForBitmapInfoHeader(DWORD aFormat, DWORD aWidth,
                                       LONG* aOutStride);

HRESULT MFGetService(IUnknown* punkObject, REFGUID guidService, REFIID riid,
                     LPVOID* ppvObject);

HRESULT DXVA2CreateDirect3DDeviceManager9(
    UINT* pResetToken, IDirect3DDeviceManager9** ppDXVAManager);

HRESULT MFCreateDXGIDeviceManager(UINT* pResetToken,
                                  IMFDXGIDeviceManager** ppDXVAManager);

HRESULT MFCreateSample(IMFSample** ppIMFSample);

HRESULT MFCreateAlignedMemoryBuffer(DWORD cbMaxLength, DWORD fAlignmentFlags,
                                    IMFMediaBuffer** ppBuffer);

HRESULT MFCreateDXGISurfaceBuffer(REFIID riid, IUnknown* punkSurface,
                                  UINT uSubresourceIndex,
                                  BOOL fButtomUpWhenLinear,
                                  IMFMediaBuffer** ppBuffer);

HRESULT MFTEnumEx(GUID guidCategory, UINT32 Flags,
                  const MFT_REGISTER_TYPE_INFO* pInputType,
                  const MFT_REGISTER_TYPE_INFO* pOutputType,
                  IMFActivate*** pppMFTActivate, UINT32* pnumMFTActivate);

HRESULT MFTGetInfo(CLSID clsidMFT, LPWSTR* pszName,
                   MFT_REGISTER_TYPE_INFO** ppInputTypes, UINT32* pcInputTypes,
                   MFT_REGISTER_TYPE_INFO** ppOutputTypes,
                   UINT32* pcOutputTypes, IMFAttributes** ppAttributes);

HRESULT MFCreateAttributes(IMFAttributes** ppMFAttributes, UINT32 cInitialSize);

HRESULT MFCreateEventQueue(IMFMediaEventQueue** ppMediaEventQueue);

HRESULT MFCreateStreamDescriptor(DWORD dwStreamIdentifier, DWORD cMediaTypes,
                                 IMFMediaType** apMediaTypes,
                                 IMFStreamDescriptor** ppDescriptor);

HRESULT MFCreateAsyncResult(IUnknown* punkObject, IMFAsyncCallback* pCallback,
                            IUnknown* punkState,
                            IMFAsyncResult** ppAsyncResult);

HRESULT MFCreatePresentationDescriptor(
    DWORD cStreamDescriptors, IMFStreamDescriptor** apStreamDescriptors,
    IMFPresentationDescriptor** ppPresentationDescriptor);

HRESULT MFCreateMemoryBuffer(DWORD cbMaxLength, IMFMediaBuffer** ppBuffer);

}  // namespace mozilla::wmf

#endif
