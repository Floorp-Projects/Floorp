/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Handler.h"
#include "Module.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/mscom/Objref.h"
#include "nsWindowsHelpers.h"

#include <objbase.h>
#include <shlwapi.h>
#include <string.h>

/* WARNING! The code in this file may be loaded into the address spaces of other
   processes! It MUST NOT link against xul.dll or other Gecko binaries! Only
   inline code may be included! */

namespace mozilla {
namespace mscom {

Handler::Handler(IUnknown* aOuter, HRESULT* aResult)
    : mRefCnt(0), mOuter(aOuter), mUnmarshal(nullptr), mHasPayload(false) {
  MOZ_ASSERT(aResult);

  if (!aOuter) {
    *aResult = E_INVALIDARG;
    return;
  }

  StabilizedRefCount<ULONG> stabilizer(mRefCnt);

  *aResult =
      ::CoGetStdMarshalEx(aOuter, SMEXF_HANDLER, getter_AddRefs(mInnerUnk));
  if (FAILED(*aResult)) {
    return;
  }

  *aResult = mInnerUnk->QueryInterface(IID_IMarshal, (void**)&mUnmarshal);
  if (FAILED(*aResult)) {
    return;
  }

  // mUnmarshal is a weak ref
  mUnmarshal->Release();
}

HRESULT
Handler::InternalQueryInterface(REFIID riid, void** ppv) {
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

  // Try the handler implementation
  HRESULT hr = QueryHandlerInterface(mInnerUnk, riid, ppv);
  if (hr == S_FALSE) {
    // The handler knows this interface is not available, so don't bother
    // asking the proxy.
    return E_NOINTERFACE;
  }
  if (hr != E_NOINTERFACE) {
    return hr;
  }

  // Now forward to the marshaler's inner
  return mInnerUnk->QueryInterface(riid, ppv);
}

ULONG
Handler::InternalAddRef() {
  if (!mRefCnt) {
    Module::Lock();
  }
  return ++mRefCnt;
}

ULONG
Handler::InternalRelease() {
  ULONG newRefCnt = --mRefCnt;
  if (newRefCnt == 0) {
    delete this;
    Module::Unlock();
  }
  return newRefCnt;
}

HRESULT
Handler::GetUnmarshalClass(REFIID riid, void* pv, DWORD dwDestContext,
                           void* pvDestContext, DWORD mshlflags, CLSID* pCid) {
  return mUnmarshal->GetUnmarshalClass(MarshalAs(riid), pv, dwDestContext,
                                       pvDestContext, mshlflags, pCid);
}

HRESULT
Handler::GetMarshalSizeMax(REFIID riid, void* pv, DWORD dwDestContext,
                           void* pvDestContext, DWORD mshlflags, DWORD* pSize) {
  if (!pSize) {
    return E_INVALIDARG;
  }

  *pSize = 0;

  RefPtr<IUnknown> unkToMarshal;
  HRESULT hr;

  REFIID marshalAs = MarshalAs(riid);
  if (marshalAs == riid) {
    unkToMarshal = static_cast<IUnknown*>(pv);
  } else {
    hr = mInnerUnk->QueryInterface(marshalAs, getter_AddRefs(unkToMarshal));
    if (FAILED(hr)) {
      return hr;
    }
  }

  // We do not necessarily want to use the pv that COM is giving us; we may want
  // to marshal a different proxy that is more appropriate to what we're
  // wrapping...
  hr = mUnmarshal->GetMarshalSizeMax(marshalAs, unkToMarshal.get(),
                                     dwDestContext, pvDestContext, mshlflags,
                                     pSize);

#if defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
  return hr;
#else
  if (FAILED(hr)) {
    return hr;
  }

  if (!HasPayload()) {
    return S_OK;
  }

  DWORD payloadSize = 0;
  hr = GetHandlerPayloadSize(marshalAs, &payloadSize);
  if (FAILED(hr)) {
    return hr;
  }

  *pSize += payloadSize;
  return S_OK;
#endif  // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
}

HRESULT
Handler::GetMarshalInterface(REFIID aMarshalAsIid, NotNull<IUnknown*> aProxy,
                             NotNull<IID*> aOutIid,
                             NotNull<IUnknown**> aOutUnk) {
  *aOutIid = aMarshalAsIid;
  return aProxy->QueryInterface(
      aMarshalAsIid,
      reinterpret_cast<void**>(static_cast<IUnknown**>(aOutUnk)));
}

HRESULT
Handler::MarshalInterface(IStream* pStm, REFIID riid, void* pv,
                          DWORD dwDestContext, void* pvDestContext,
                          DWORD mshlflags) {
  // We do not necessarily want to use the pv that COM is giving us; we may want
  // to marshal a different proxy that is more appropriate to what we're
  // wrapping...
  RefPtr<IUnknown> unkToMarshal;
  HRESULT hr;

#if defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
  LARGE_INTEGER seekTo;
  seekTo.QuadPart = 0;

  ULARGE_INTEGER objrefPos;

  // Save the current position as it points to the location where the OBJREF
  // will be written.
  hr = pStm->Seek(seekTo, STREAM_SEEK_CUR, &objrefPos);
  if (FAILED(hr)) {
    return hr;
  }
#endif  // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)

  REFIID marshalAs = MarshalAs(riid);
  IID marshalOutAs;

  hr = GetMarshalInterface(
      marshalAs, WrapNotNull<IUnknown*>(mInnerUnk), WrapNotNull(&marshalOutAs),
      WrapNotNull<IUnknown**>(getter_AddRefs(unkToMarshal)));
  if (FAILED(hr)) {
    return hr;
  }

  hr = mUnmarshal->MarshalInterface(pStm, marshalAs, unkToMarshal.get(),
                                    dwDestContext, pvDestContext, mshlflags);
  if (FAILED(hr)) {
    return hr;
  }

#if defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
  // Obtain the current stream position which is the end of the OBJREF
  ULARGE_INTEGER endPos;
  hr = pStm->Seek(seekTo, STREAM_SEEK_CUR, &endPos);
  if (FAILED(hr)) {
    return hr;
  }

  // Now strip out the handler.
  if (!StripHandlerFromOBJREF(WrapNotNull(pStm), objrefPos.QuadPart,
                              endPos.QuadPart)) {
    return E_FAIL;
  }

  // Fix the IID
  if (!SetIID(WrapNotNull(pStm), objrefPos.QuadPart, marshalOutAs)) {
    return E_FAIL;
  }

  return S_OK;
#else
  if (!HasPayload()) {
    return S_OK;
  }

  // Unfortunately when COM re-marshals a proxy that prevouisly had a payload,
  // we must re-serialize it.
  return WriteHandlerPayload(pStm, marshalAs);
#endif  // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
}

HRESULT
Handler::UnmarshalInterface(IStream* pStm, REFIID riid, void** ppv) {
  REFIID unmarshalAs = MarshalAs(riid);
  HRESULT hr = mUnmarshal->UnmarshalInterface(pStm, unmarshalAs, ppv);
  if (FAILED(hr)) {
    return hr;
  }

  // This method may be called on the same object multiple times (as new
  // interfaces are queried off the proxy). Not all interfaces will necessarily
  // refresh the payload, so we set mHasPayload using OR to reflect that fact.
  // (Otherwise mHasPayload could be cleared and the handler would think that
  // it doesn't have a payload even though it actually does).
  mHasPayload |= (ReadHandlerPayload(pStm, unmarshalAs) == S_OK);

  return hr;
}

HRESULT
Handler::ReleaseMarshalData(IStream* pStm) {
  return mUnmarshal->ReleaseMarshalData(pStm);
}

HRESULT
Handler::DisconnectObject(DWORD dwReserved) {
  return mUnmarshal->DisconnectObject(dwReserved);
}

HRESULT
Handler::Unregister(REFCLSID aClsid) { return Module::Deregister(aClsid); }

HRESULT
Handler::Register(REFCLSID aClsid, const bool aMsixContainer) {
  return Module::Register(aClsid, Module::ThreadingModel::DedicatedUiThreadOnly,
                          Module::ClassType::InprocHandler, nullptr,
                          aMsixContainer);
}

}  // namespace mscom
}  // namespace mozilla
