/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Move.h"
#include "mozilla/mscom/DispatchForwarder.h"
#include "mozilla/mscom/MainThreadInvoker.h"

#include <oleauto.h>

namespace mozilla {
namespace mscom {

/* static */ HRESULT
DispatchForwarder::Create(IInterceptor* aInterceptor,
                          STAUniquePtr<IDispatch>& aTarget, IUnknown** aOutput)
{
  MOZ_ASSERT(aInterceptor && aOutput);
  if (!aOutput) {
    return E_INVALIDARG;
  }
  *aOutput = nullptr;
  if (!aInterceptor) {
    return E_INVALIDARG;
  }
  DispatchForwarder* forwarder = new DispatchForwarder(aInterceptor, aTarget);
  HRESULT hr = forwarder->QueryInterface(IID_IDispatch, (void**) aOutput);
  forwarder->Release();
  return hr;
}

DispatchForwarder::DispatchForwarder(IInterceptor* aInterceptor,
                                     STAUniquePtr<IDispatch>& aTarget)
  : mRefCnt(1)
  , mInterceptor(aInterceptor)
  , mTarget(Move(aTarget))
{
}

DispatchForwarder::~DispatchForwarder()
{
}

HRESULT
DispatchForwarder::QueryInterface(REFIID riid, void** ppv)
{
  if (!ppv) {
    return E_INVALIDARG;
  }

  // Since this class implements a tearoff, any interfaces that are not
  // IDispatch must be routed to the original object's QueryInterface.
  // This is especially important for IUnknown since COM uses that interface
  // to determine object identity.
  if (riid != IID_IDispatch) {
    return mInterceptor->QueryInterface(riid, ppv);
  }

  IUnknown* punk = static_cast<IDispatch*>(this);
  *ppv = punk;
  if (!punk) {
    return E_NOINTERFACE;
  }

  punk->AddRef();
  return S_OK;
}

ULONG
DispatchForwarder::AddRef()
{
  return (ULONG) InterlockedIncrement((LONG*)&mRefCnt);
}

ULONG
DispatchForwarder::Release()
{
  ULONG newRefCnt = (ULONG) InterlockedDecrement((LONG*)&mRefCnt);
  if (newRefCnt == 0) {
    delete this;
  }
  return newRefCnt;
}

HRESULT
DispatchForwarder::GetTypeInfoCount(UINT *pctinfo)
{
  if (!pctinfo) {
    return E_INVALIDARG;
  }
  *pctinfo = 1;
  return S_OK;
}

HRESULT
DispatchForwarder::GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
{
  // ITypeInfo as implemented by COM is apartment-neutral, so we don't need
  // to wrap it (yay!)
  if (mTypeInfo) {
    RefPtr<ITypeInfo> copy(mTypeInfo);
    copy.forget(ppTInfo);
    return S_OK;
  }
  HRESULT hr = E_UNEXPECTED;
  auto fn = [&]() -> void {
    hr = mTarget->GetTypeInfo(iTInfo, lcid, ppTInfo);
  };
  MainThreadInvoker invoker;
  if (!invoker.Invoke(NS_NewRunnableFunction(fn))) {
    return E_UNEXPECTED;
  }
  if (FAILED(hr)) {
    return hr;
  }
  mTypeInfo = *ppTInfo;
  return hr;
}

HRESULT
DispatchForwarder::GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames, UINT cNames,
                                 LCID lcid, DISPID *rgDispId)
{
  HRESULT hr = E_UNEXPECTED;
  auto fn = [&]() -> void {
    hr = mTarget->GetIDsOfNames(riid, rgszNames, cNames, lcid, rgDispId);
  };
  MainThreadInvoker invoker;
  if (!invoker.Invoke(NS_NewRunnableFunction(fn))) {
    return E_UNEXPECTED;
  }
  return hr;
}

HRESULT
DispatchForwarder::Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,
                          WORD wFlags, DISPPARAMS *pDispParams,
                          VARIANT *pVarResult, EXCEPINFO *pExcepInfo,
                          UINT *puArgErr)
{
  HRESULT hr;
  if (!mInterface) {
    if (!mTypeInfo) {
      return E_UNEXPECTED;
    }
    TYPEATTR* typeAttr = nullptr;
    hr = mTypeInfo->GetTypeAttr(&typeAttr);
    if (FAILED(hr)) {
      return hr;
    }
    hr = mInterceptor->QueryInterface(typeAttr->guid,
                                      (void**)getter_AddRefs(mInterface));
    mTypeInfo->ReleaseTypeAttr(typeAttr);
    if (FAILED(hr)) {
      return hr;
    }
  }
  // We don't invoke IDispatch on the target, but rather on the interceptor!
  hr = ::DispInvoke(mInterface.get(), mTypeInfo, dispIdMember, wFlags,
                    pDispParams, pVarResult, pExcepInfo, puArgErr);
  return hr;
}

} // namespace mscom
} // namespace mozilla
