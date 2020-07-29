/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_FastMarshaler_h
#define mozilla_mscom_FastMarshaler_h

#include "mozilla/Atomics.h"
#include "mozilla/mscom/Aggregation.h"
#include "mozilla/RefPtr.h"

#include <objidl.h>

namespace mozilla {
namespace mscom {

/**
 * COM ping functionality is enabled by default and is designed to free strong
 * references held by defunct client processes. However, this incurs a
 * significant performance penalty in a11y code due to large numbers of remote
 * objects being created and destroyed within a short period of time. Thus, we
 * turn off pings to improve performance.
 * ACHTUNG! When COM pings are disabled, Release calls from remote clients are
 * never sent to the server! If you use this marshaler, you *must* explicitly
 * disconnect clients using CoDisconnectObject when the object is no longer
 * relevant. Otherwise, references to the object will never be released, causing
 * a leak.
 */
class FastMarshaler final : public IMarshal {
 public:
  static HRESULT Create(IUnknown* aOuter, IUnknown** aOutMarshalerUnk);

  // IMarshal
  STDMETHODIMP GetUnmarshalClass(REFIID riid, void* pv, DWORD dwDestContext,
                                 void* pvDestContext, DWORD mshlflags,
                                 CLSID* pCid) override;
  STDMETHODIMP GetMarshalSizeMax(REFIID riid, void* pv, DWORD dwDestContext,
                                 void* pvDestContext, DWORD mshlflags,
                                 DWORD* pSize) override;
  STDMETHODIMP MarshalInterface(IStream* pStm, REFIID riid, void* pv,
                                DWORD dwDestContext, void* pvDestContext,
                                DWORD mshlflags) override;
  STDMETHODIMP UnmarshalInterface(IStream* pStm, REFIID riid,
                                  void** ppv) override;
  STDMETHODIMP ReleaseMarshalData(IStream* pStm) override;
  STDMETHODIMP DisconnectObject(DWORD dwReserved) override;

 private:
  FastMarshaler(IUnknown* aOuter, HRESULT* aResult);
  ~FastMarshaler() = default;

  static DWORD GetMarshalFlags(DWORD aDestContext, DWORD aMshlFlags);

  Atomic<ULONG> mRefCnt;
  IUnknown* mOuter;
  RefPtr<IUnknown> mStdMarshalUnk;
  IMarshal* mStdMarshalWeak;
  DECLARE_AGGREGATABLE(FastMarshaler);
};

}  // namespace mscom
}  // namespace mozilla

#endif  // mozilla_mscom_FastMarshaler_h
