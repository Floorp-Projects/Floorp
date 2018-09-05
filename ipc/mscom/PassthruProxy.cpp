/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/PassthruProxy.h"
#include "mozilla/mscom/ProxyStream.h"
#include "VTableBuilder.h"

// {96EF5801-CE6D-416E-A50A-0C2959AEAE1C}
static const GUID CLSID_PassthruProxy =
{ 0x96ef5801, 0xce6d, 0x416e, { 0xa5, 0xa, 0xc, 0x29, 0x59, 0xae, 0xae, 0x1c } };

namespace mozilla {
namespace mscom {

PassthruProxy::PassthruProxy()
  : mRefCnt(0)
  , mWrappedIid()
  , mVTableSize(0)
  , mVTable(nullptr)
  , mForgetPreservedStream(false)
{
}

PassthruProxy::PassthruProxy(ProxyStream::Environment* aEnv, REFIID aIidToWrap,
                             uint32_t aVTableSize, NotNull<IUnknown*> aObjToWrap)
  : mRefCnt(0)
  , mWrappedIid(aIidToWrap)
  , mVTableSize(aVTableSize)
  , mVTable(nullptr)
  , mForgetPreservedStream(false)
{
  ProxyStream proxyStream(aIidToWrap, aObjToWrap, aEnv,
                          ProxyStreamFlags::ePreservable);
  mPreservedStream = std::move(proxyStream.GetPreservedStream());
  MOZ_ASSERT(mPreservedStream);
}

PassthruProxy::~PassthruProxy()
{
  if (mForgetPreservedStream) {
    // We want to release the ref without clearing marshal data
    IStream* stream = mPreservedStream.release();
    stream->Release();
  }

  if (mVTable) {
    DeleteNullVTable(mVTable);
  }
}

HRESULT
PassthruProxy::QueryProxyInterface(void** aOutInterface)
{
  // Even though we don't really provide the methods for the interface that
  // we are proxying, we need to support it in QueryInterface. Instead we
  // return an interface that, other than IUnknown, contains nullptr for all of
  // its vtable entires. Obviously this interface is not intended to actually
  // be called, it just has to be there.

  if (!mVTable) {
    MOZ_ASSERT(mVTableSize);
    mVTable = BuildNullVTable(static_cast<IMarshal*>(this), mVTableSize);
    MOZ_ASSERT(mVTable);
  }

  *aOutInterface = mVTable;
  mVTable->AddRef();
  return S_OK;
}

HRESULT
PassthruProxy::QueryInterface(REFIID aIid, void** aOutInterface)
{
  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  *aOutInterface = nullptr;

  if (aIid == IID_IUnknown || aIid == IID_IMarshal) {
    RefPtr<IMarshal> ptr(this);
    ptr.forget(aOutInterface);
    return S_OK;
  }

  if (!IsInitialMarshal()) {
    // We implement IClientSecurity so that IsProxy() recognizes us as such
    if (aIid == IID_IClientSecurity) {
      RefPtr<IClientSecurity> ptr(this);
      ptr.forget(aOutInterface);
      return S_OK;
    }

    if (aIid == mWrappedIid) {
      return QueryProxyInterface(aOutInterface);
    }
  }

  return E_NOINTERFACE;
}

ULONG
PassthruProxy::AddRef()
{
  return ++mRefCnt;
}

ULONG
PassthruProxy::Release()
{
  ULONG result = --mRefCnt;
  if (!result) {
    delete this;
  }

  return result;
}

HRESULT
PassthruProxy::GetUnmarshalClass(REFIID riid, void* pv, DWORD dwDestContext,
                                 void* pvDestContext, DWORD mshlflags,
                                 CLSID* pCid)
{
  if (!pCid) {
    return E_INVALIDARG;
  }

  if (IsInitialMarshal()) {
    // To properly use this class we need to be using TABLESTRONG marshaling
    MOZ_ASSERT(mshlflags & MSHLFLAGS_TABLESTRONG);

    // When we're marshaling for the first time, we identify ourselves as the
    // class to use for unmarshaling.
    *pCid = CLSID_PassthruProxy;
  } else {
    // Subsequent marshals use the standard marshaler.
    *pCid = CLSID_StdMarshal;
  }

  return S_OK;
}

HRESULT
PassthruProxy::GetMarshalSizeMax(REFIID riid, void* pv, DWORD dwDestContext,
                                 void* pvDestContext, DWORD mshlflags,
                                 DWORD* pSize)
{
  STATSTG statstg;
  HRESULT hr;

  if (!IsInitialMarshal()) {
    // If we are not the initial marshal then we are just copying mStream out
    // to the marshal stream, so we just use mStream's size.
    hr = mStream->Stat(&statstg, STATFLAG_NONAME);
    if (FAILED(hr)) {
      return hr;
    }

    *pSize = statstg.cbSize.LowPart;

    return hr;
  }

  // To properly use this class we need to be using TABLESTRONG marshaling
  MOZ_ASSERT(mshlflags & MSHLFLAGS_TABLESTRONG);

  if (!mPreservedStream) {
    return E_POINTER;
  }

  hr = mPreservedStream->Stat(&statstg, STATFLAG_NONAME);
  if (FAILED(hr)) {
    return hr;
  }

  *pSize = statstg.cbSize.LowPart + sizeof(mVTableSize) + sizeof(mWrappedIid);
  return hr;
}

HRESULT
PassthruProxy::MarshalInterface(IStream* pStm, REFIID riid, void* pv,
                                DWORD dwDestContext, void* pvDestContext,
                                DWORD mshlflags)
{
  MOZ_ASSERT(riid == mWrappedIid);
  if (riid != mWrappedIid) {
    return E_NOINTERFACE;
  }

  MOZ_ASSERT(pv == mVTable);
  if (pv != mVTable) {
    return E_INVALIDARG;
  }

  HRESULT hr;
  RefPtr<IStream> cloned;

  if (IsInitialMarshal()) {
    // To properly use this class we need to be using TABLESTRONG marshaling
    MOZ_ASSERT(mshlflags & MSHLFLAGS_TABLESTRONG);

    if (!mPreservedStream) {
      return E_POINTER;
    }

    // We write out the vtable size and the IID so that the wrapped proxy knows
    // how to build its vtable on the content side.
    ULONG bytesWritten;
    hr = pStm->Write(&mVTableSize, sizeof(mVTableSize), &bytesWritten);
    if (FAILED(hr)) {
      return hr;
    }
    if (bytesWritten != sizeof(mVTableSize)) {
      return E_UNEXPECTED;
    }

    hr = pStm->Write(&mWrappedIid, sizeof(mWrappedIid), &bytesWritten);
    if (FAILED(hr)) {
      return hr;
    }
    if (bytesWritten != sizeof(mWrappedIid)) {
      return E_UNEXPECTED;
    }

    hr = mPreservedStream->Clone(getter_AddRefs(cloned));
  } else {
    hr = mStream->Clone(getter_AddRefs(cloned));
  }

  if (FAILED(hr)) {
    return hr;
  }

  STATSTG statstg;
  hr = cloned->Stat(&statstg, STATFLAG_NONAME);
  if (FAILED(hr)) {
    return hr;
  }

  // Copy the proxy data
  hr = cloned->CopyTo(pStm, statstg.cbSize, nullptr, nullptr);

  if (SUCCEEDED(hr) && IsInitialMarshal() && mPreservedStream &&
      (mshlflags & MSHLFLAGS_TABLESTRONG)) {
    // If we have successfully copied mPreservedStream at least once for a
    // MSHLFLAGS_TABLESTRONG marshal, then we want to forget our reference to it.
    // This is because the COM runtime will manage it from here on out.
    mForgetPreservedStream = true;
  }

  return hr;
}

HRESULT
PassthruProxy::UnmarshalInterface(IStream* pStm, REFIID riid, void** ppv)
{
  // Read out the interface info that we copied during marshaling
  ULONG bytesRead;
  HRESULT hr = pStm->Read(&mVTableSize, sizeof(mVTableSize), &bytesRead);
  if (FAILED(hr)) {
    return hr;
  }
  if (bytesRead != sizeof(mVTableSize)) {
    return E_UNEXPECTED;
  }

  hr = pStm->Read(&mWrappedIid, sizeof(mWrappedIid), &bytesRead);
  if (FAILED(hr)) {
    return hr;
  }
  if (bytesRead != sizeof(mWrappedIid)) {
    return E_UNEXPECTED;
  }

  // Now we copy the proxy inside pStm into mStream
  hr = CopySerializedProxy(pStm, getter_AddRefs(mStream));
  if (FAILED(hr)) {
    return hr;
  }

  return QueryInterface(riid, ppv);
}

HRESULT
PassthruProxy::ReleaseMarshalData(IStream* pStm)
{
  if (!IsInitialMarshal()) {
    return S_OK;
  }

  if (!pStm) {
    return E_INVALIDARG;
  }

  if (mPreservedStream) {
    // If we still have mPreservedStream, then simply clearing it will release
    // its marshal data automagically.
    mPreservedStream = nullptr;
    return S_OK;
  }

  // Skip past the metadata that we wrote during initial marshaling.
  LARGE_INTEGER seekTo;
  seekTo.QuadPart = sizeof(mVTableSize) + sizeof(mWrappedIid);
  HRESULT hr = pStm->Seek(seekTo, STREAM_SEEK_CUR, nullptr);
  if (FAILED(hr)) {
    return hr;
  }

  // Now release the "inner" marshal data
  return ::CoReleaseMarshalData(pStm);
}

HRESULT
PassthruProxy::DisconnectObject(DWORD dwReserved)
{
  return S_OK;
}

// The remainder of this code is just boilerplate COM stuff that provides the
// association between CLSID_PassthruProxy and the PassthruProxy class itself.

class PassthruProxyClassObject final : public IClassFactory
{
public:
  PassthruProxyClassObject();

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID aIid, void** aOutInterface) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IClassFactory
  STDMETHODIMP CreateInstance(IUnknown* aOuter, REFIID aIid, void** aOutObject) override;
  STDMETHODIMP LockServer(BOOL aLock) override;

private:
  ~PassthruProxyClassObject() = default;

  Atomic<ULONG> mRefCnt;
};

PassthruProxyClassObject::PassthruProxyClassObject()
  : mRefCnt(0)
{
}

HRESULT
PassthruProxyClassObject::QueryInterface(REFIID aIid, void** aOutInterface)
{
  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  *aOutInterface = nullptr;

  if (aIid == IID_IUnknown || aIid == IID_IClassFactory) {
    RefPtr<IClassFactory> ptr(this);
    ptr.forget(aOutInterface);
    return S_OK;
  }

  return E_NOINTERFACE;
}

ULONG
PassthruProxyClassObject::AddRef()
{
  return ++mRefCnt;
}

ULONG
PassthruProxyClassObject::Release()
{
  ULONG result = --mRefCnt;
  if (!result) {
    delete this;
  }

  return result;
}

HRESULT
PassthruProxyClassObject::CreateInstance(IUnknown* aOuter, REFIID aIid,
                                         void** aOutObject)
{
  // We don't expect to aggregate
  MOZ_ASSERT(!aOuter);
  if (aOuter) {
    return E_INVALIDARG;
  }

  RefPtr<PassthruProxy> ptr(new PassthruProxy());
  return ptr->QueryInterface(aIid, aOutObject);
}

HRESULT
PassthruProxyClassObject::LockServer(BOOL aLock)
{
  // No-op since xul.dll is always in memory
  return S_OK;
}

/* static */ HRESULT
PassthruProxy::Register()
{
  DWORD cookie;
  RefPtr<IClassFactory> classObj(new PassthruProxyClassObject());
  return ::CoRegisterClassObject(CLSID_PassthruProxy, classObj,
                                 CLSCTX_INPROC_SERVER, REGCLS_MULTIPLEUSE,
                                 &cookie);
}

} // namespace mscom
} // namespace mozilla

HRESULT
RegisterPassthruProxy()
{
  return mozilla::mscom::PassthruProxy::Register();
}
