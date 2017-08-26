/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/FastMarshaler.h"

#include "mozilla/mscom/Utils.h"

#include <objbase.h>

namespace mozilla {
namespace mscom {

HRESULT
FastMarshaler::Create(IUnknown* aOuter,
                      IUnknown** aOutMarshalerUnk)
{
  MOZ_ASSERT(XRE_IsContentProcess());

  if (!aOuter || !aOutMarshalerUnk) {
    return E_INVALIDARG;
  }

  *aOutMarshalerUnk = nullptr;

  HRESULT hr;
  RefPtr<FastMarshaler> fm(new FastMarshaler(aOuter, &hr));
  if (FAILED(hr)) {
    return hr;
  }

  return fm->InternalQueryInterface(IID_IUnknown, (void**)aOutMarshalerUnk);
}

FastMarshaler::FastMarshaler(IUnknown* aOuter,
                             HRESULT* aResult)
  : mRefCnt(0)
  , mOuter(aOuter)
  , mStdMarshalWeak(nullptr)
{
  *aResult = ::CoGetStdMarshalEx(aOuter, SMEXF_SERVER,
                                 getter_AddRefs(mStdMarshalUnk));
  if (FAILED(*aResult)) {
    return;
  }

  *aResult = mStdMarshalUnk->QueryInterface(IID_IMarshal,
                                            (void**)&mStdMarshalWeak);
  if (FAILED(*aResult)) {
    return;
  }

  // mStdMarshalWeak is weak
  mStdMarshalWeak->Release();
}

HRESULT
FastMarshaler::InternalQueryInterface(REFIID riid, void** ppv)
{
  if (!ppv) {
    return E_INVALIDARG;
  }

  if (riid == IID_IUnknown) {
    RefPtr<IUnknown> punk(static_cast<IUnknown*>(&mInternalUnknown));
    punk.forget(ppv);
    return S_OK;
  }

  if (riid == IID_IMarshal) {
    RefPtr<IMarshal> ptr(this);
    ptr.forget(ppv);
    return S_OK;
  }

  return mStdMarshalUnk->QueryInterface(riid, ppv);
}

ULONG
FastMarshaler::InternalAddRef()
{
  return ++mRefCnt;
}

ULONG
FastMarshaler::InternalRelease()
{
  ULONG result = --mRefCnt;
  if (!result) {
    delete this;
  }

  return result;
}

DWORD
FastMarshaler::GetMarshalFlags(DWORD aDestContext, DWORD aMshlFlags)
{
  // Only worry about local contexts.
  if (aDestContext != MSHCTX_LOCAL) {
    return aMshlFlags;
  }

  if (!IsCallerExternalProcess()) {
    return aMshlFlags;
  }

  // The caller is our parent main thread. Disable ping functionality.
  return aMshlFlags | MSHLFLAGS_NOPING;
}

HRESULT
FastMarshaler::GetUnmarshalClass(REFIID riid, void* pv, DWORD dwDestContext,
                                 void* pvDestContext, DWORD mshlflags,
                                 CLSID* pCid)
{
  if (!mStdMarshalWeak) {
    return E_POINTER;
  }

  return mStdMarshalWeak->GetUnmarshalClass(riid, pv, dwDestContext,
                                            pvDestContext,
                                            GetMarshalFlags(dwDestContext,
                                                            mshlflags), pCid);
}

HRESULT
FastMarshaler::GetMarshalSizeMax(REFIID riid, void* pv, DWORD dwDestContext,
                                 void* pvDestContext, DWORD mshlflags,
                                 DWORD* pSize)
{
  if (!mStdMarshalWeak) {
    return E_POINTER;
  }

  return mStdMarshalWeak->GetMarshalSizeMax(riid, pv, dwDestContext,
                                            pvDestContext,
                                            GetMarshalFlags(dwDestContext,
                                                            mshlflags), pSize);
}

HRESULT
FastMarshaler::MarshalInterface(IStream* pStm, REFIID riid, void* pv,
                                DWORD dwDestContext, void* pvDestContext,
                                DWORD mshlflags)
{
  if (!mStdMarshalWeak) {
    return E_POINTER;
  }

  return mStdMarshalWeak->MarshalInterface(pStm, riid, pv, dwDestContext,
                                           pvDestContext,
                                           GetMarshalFlags(dwDestContext,
                                                           mshlflags));
}

HRESULT
FastMarshaler::UnmarshalInterface(IStream* pStm, REFIID riid, void** ppv)
{
  if (!mStdMarshalWeak) {
    return E_POINTER;
  }

  return mStdMarshalWeak->UnmarshalInterface(pStm, riid, ppv);
}

HRESULT
FastMarshaler::ReleaseMarshalData(IStream* pStm)
{
  if (!mStdMarshalWeak) {
    return E_POINTER;
  }

  return mStdMarshalWeak->ReleaseMarshalData(pStm);
}

HRESULT
FastMarshaler::DisconnectObject(DWORD dwReserved)
{
  if (!mStdMarshalWeak) {
    return E_POINTER;
  }

  return mStdMarshalWeak->DisconnectObject(dwReserved);
}

} // namespace mscom
} // namespace mozilla
