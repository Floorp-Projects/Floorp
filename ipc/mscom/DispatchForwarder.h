/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_DispatchForwarder_h
#define mozilla_mscom_DispatchForwarder_h

#include <oaidl.h>

#include "mozilla/mscom/Interceptor.h"
#include "mozilla/mscom/Ptr.h"

namespace mozilla {
namespace mscom {

class DispatchForwarder final : public IDispatch
{
public:
  static HRESULT Create(IInterceptor* aInterceptor,
                        STAUniquePtr<IDispatch>& aTarget, IUnknown** aOutput);

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IDispatch
  STDMETHODIMP GetTypeInfoCount(
      /* [out] */ __RPC__out UINT *pctinfo) override;

  STDMETHODIMP GetTypeInfo(
      /* [in] */ UINT iTInfo,
      /* [in] */ LCID lcid,
      /* [out] */ __RPC__deref_out_opt ITypeInfo **ppTInfo) override;

  STDMETHODIMP GetIDsOfNames(
      /* [in] */ __RPC__in REFIID riid,
      /* [size_is][in] */ __RPC__in_ecount_full(cNames) LPOLESTR *rgszNames,
      /* [range][in] */ __RPC__in_range(0,16384) UINT cNames,
      /* [in] */ LCID lcid,
      /* [size_is][out] */ __RPC__out_ecount_full(cNames) DISPID *rgDispId)
    override;

  STDMETHODIMP Invoke(
      /* [annotation][in] */
      _In_  DISPID dispIdMember,
      /* [annotation][in] */
      _In_  REFIID riid,
      /* [annotation][in] */
      _In_  LCID lcid,
      /* [annotation][in] */
      _In_  WORD wFlags,
      /* [annotation][out][in] */
      _In_  DISPPARAMS *pDispParams,
      /* [annotation][out] */
      _Out_opt_  VARIANT *pVarResult,
      /* [annotation][out] */
      _Out_opt_  EXCEPINFO *pExcepInfo,
      /* [annotation][out] */
      _Out_opt_  UINT *puArgErr) override;

private:
  DispatchForwarder(IInterceptor* aInterceptor,
                    STAUniquePtr<IDispatch>& aTarget);
  ~DispatchForwarder();

private:
  ULONG mRefCnt;
  RefPtr<IInterceptor> mInterceptor;
  STAUniquePtr<IDispatch> mTarget;
  RefPtr<ITypeInfo> mTypeInfo;
  RefPtr<IUnknown> mInterface;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_DispatchForwarder_h

