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
  : mRefCnt(0)
  , mOuter(aOuter)
  , mUnmarshal(nullptr)
  , mHasPayload(false)
{
  MOZ_ASSERT(aResult);

  if (!aOuter) {
    *aResult = E_INVALIDARG;
    return;
  }

  StabilizedRefCount<ULONG> stabilizer(mRefCnt);

  *aResult = ::CoGetStdMarshalEx(aOuter, SMEXF_HANDLER,
                                 getter_AddRefs(mInnerUnk));
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
Handler::InternalQueryInterface(REFIID riid, void** ppv)
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

  // Try the handler implementation
  HRESULT hr = QueryHandlerInterface(mInnerUnk, riid, ppv);
  if (hr != E_NOINTERFACE) {
    return hr;
  }

  // Now forward to the marshaler's inner
  return mInnerUnk->QueryInterface(riid, ppv);
}

ULONG
Handler::InternalAddRef()
{
  if (!mRefCnt) {
    Module::Lock();
  }
  return ++mRefCnt;
}

ULONG
Handler::InternalRelease()
{
  ULONG newRefCnt = --mRefCnt;
  if (newRefCnt == 0) {
    delete this;
    Module::Unlock();
  }
  return newRefCnt;
}

HRESULT
Handler::GetUnmarshalClass(REFIID riid, void* pv, DWORD dwDestContext,
                           void* pvDestContext, DWORD mshlflags,
                           CLSID* pCid)
{
  return mUnmarshal->GetUnmarshalClass(riid, pv, dwDestContext, pvDestContext,
                                       mshlflags, pCid);
}

HRESULT
Handler::GetMarshalSizeMax(REFIID riid, void* pv, DWORD dwDestContext,
                           void* pvDestContext, DWORD mshlflags,
                           DWORD* pSize)
{
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
                                     dwDestContext, pvDestContext,
                                     mshlflags, pSize);

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
#endif // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
}

HRESULT
Handler::MarshalInterface(IStream* pStm, REFIID riid, void* pv,
                          DWORD dwDestContext, void* pvDestContext,
                          DWORD mshlflags)
{
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

  // When marshaling without a handler, we just use the riid as passed in.
  REFIID marshalAs = riid;
#else
  REFIID marshalAs = MarshalAs(riid);
#endif // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)

  hr = mInnerUnk->QueryInterface(marshalAs, getter_AddRefs(unkToMarshal));
  if (FAILED(hr)) {
    return hr;
  }

  hr = mUnmarshal->MarshalInterface(pStm, marshalAs, unkToMarshal.get(),
                                    dwDestContext, pvDestContext, mshlflags);
  if (FAILED(hr)) {
    return hr;
  }

#if defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
  // Now the OBJREF has been written, so seek back to its beginning (the
  // position that we saved earlier).
  seekTo.QuadPart = objrefPos.QuadPart;
  hr = pStm->Seek(seekTo, STREAM_SEEK_SET, nullptr);
  if (FAILED(hr)) {
    return hr;
  }

  // Now strip out the handler.
  if (!StripHandlerFromOBJREF(WrapNotNull(pStm))) {
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
#endif // defined(MOZ_MSCOM_REMARSHAL_NO_HANDLER)
}

HRESULT
Handler::UnmarshalInterface(IStream* pStm, REFIID riid, void** ppv)
{
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
Handler::ReleaseMarshalData(IStream* pStm)
{
  return mUnmarshal->ReleaseMarshalData(pStm);
}

HRESULT
Handler::DisconnectObject(DWORD dwReserved)
{
  return mUnmarshal->DisconnectObject(dwReserved);
}

template <size_t N>
static HRESULT
BuildClsidPath(wchar_t (&aPath)[N], REFCLSID aClsid)
{
  const wchar_t kClsid[] = {L'C', L'L', L'S', L'I', L'D', L'\\'};
  const size_t kReqdGuidLen = 39;
  static_assert(N >= kReqdGuidLen + mozilla::ArrayLength(kClsid),
                "aPath array is too short");
  if (wcsncpy_s(aPath, kClsid, mozilla::ArrayLength(kClsid))) {
    return E_INVALIDARG;
  }

  int guidConversionResult =
    StringFromGUID2(aClsid, &aPath[mozilla::ArrayLength(kClsid)],
                    N - mozilla::ArrayLength(kClsid));
  if (!guidConversionResult) {
    return E_INVALIDARG;
  }

  return S_OK;
}

HRESULT
Handler::Unregister(REFCLSID aClsid)
{
  wchar_t path[256] = {};
  HRESULT hr = BuildClsidPath(path, aClsid);
  if (FAILED(hr)) {
    return hr;
  }

  hr = HRESULT_FROM_WIN32(SHDeleteKey(HKEY_CLASSES_ROOT, path));
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT
Handler::Register(REFCLSID aClsid)
{
  wchar_t path[256] = {};
  HRESULT hr = BuildClsidPath(path, aClsid);
  if (FAILED(hr)) {
    return hr;
  }

  HKEY rawClsidKey;
  DWORD disposition;
  LONG result = RegCreateKeyEx(HKEY_CLASSES_ROOT, path, 0, nullptr,
                               REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
                               nullptr, &rawClsidKey, &disposition);
  if (result != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(result);
  }
  nsAutoRegKey clsidKey(rawClsidKey);

  if (wcscat_s(path, L"\\InprocHandler32")) {
    return E_UNEXPECTED;
  }

  HKEY rawInprocHandlerKey;
  result = RegCreateKeyEx(HKEY_CLASSES_ROOT, path, 0, nullptr,
                          REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
                          nullptr, &rawInprocHandlerKey, &disposition);
  if (result != ERROR_SUCCESS) {
    Unregister(aClsid);
    return HRESULT_FROM_WIN32(result);
  }
  nsAutoRegKey inprocHandlerKey(rawInprocHandlerKey);

  wchar_t absLibPath[MAX_PATH + 1] = {};
  HMODULE thisModule;
  if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                         GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         reinterpret_cast<LPCTSTR>(&Handler::Register),
                         &thisModule)) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  DWORD size = GetModuleFileName(thisModule, absLibPath,
                                 mozilla::ArrayLength(absLibPath));
  if (!size || (size == mozilla::ArrayLength(absLibPath) &&
      GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
    DWORD lastError = GetLastError();
    Unregister(aClsid);
    return HRESULT_FROM_WIN32(lastError);
  }

  result = RegSetValueEx(inprocHandlerKey, L"", 0, REG_EXPAND_SZ,
                         reinterpret_cast<const BYTE*>(absLibPath),
                         sizeof(absLibPath));
  if (result != ERROR_SUCCESS) {
    Unregister(aClsid);
    return HRESULT_FROM_WIN32(result);
  }

  const wchar_t kApartment[] = L"Apartment";
  result = RegSetValueEx(inprocHandlerKey, L"ThreadingModel", 0, REG_SZ,
                         reinterpret_cast<const BYTE*>(kApartment),
                         sizeof(kApartment));
  if (result != ERROR_SUCCESS) {
    Unregister(aClsid);
    return HRESULT_FROM_WIN32(result);
  }

  return S_OK;
}

} // namespace mscom
} // namespace mozilla

