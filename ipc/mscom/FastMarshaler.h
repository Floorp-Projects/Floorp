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
 * When we are marshaling to the parent process main thread, we want to turn
 * off COM's ping functionality. That functionality is designed to free
 * strong references held by defunct client processes. Since our content
 * processes cannot outlive our parent process, we turn off pings when we know
 * that the COM client is going to be our parent process. This provides a
 * significant performance boost in a11y code due to large numbers of remote
 * objects being created and destroyed within a short period of time.
 */
class FastMarshaler final : public IMarshal
{
public:
  static HRESULT Create(IUnknown* aOuter,
                        IUnknown** aOutMarshalerUnk);

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

  Atomic<ULONG>     mRefCnt;
  IUnknown*         mOuter;
  RefPtr<IUnknown>  mStdMarshalUnk;
  IMarshal*         mStdMarshalWeak;
  DECLARE_AGGREGATABLE(FastMarshaler);
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_FastMarshaler_h
