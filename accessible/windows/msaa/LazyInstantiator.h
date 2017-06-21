/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_LazyInstantiator_h
#define mozilla_a11y_LazyInstantiator_h

#include "IUnknownImpl.h"
#include "mozilla/RefPtr.h"
#include "nsString.h"
#if defined(MOZ_TELEMETRY_REPORTING)
#include "nsThreadUtils.h"
#endif // defined(MOZ_TELEMETRY_REPORTING)

#include <oleacc.h>

class nsIFile;

namespace mozilla {
namespace a11y {

class RootAccessibleWrap;

/**
 * LazyInstantiator is an IAccessible that initially acts as a placeholder.
 * The a11y service is not actually started until two conditions are met:
 *
 * (1) A method is called on the LazyInstantiator that would require a11y
 *     services in order to fulfill; and
 * (2) LazyInstantiator::ShouldInstantiate returns true.
 */
class LazyInstantiator final : public IAccessible
                             , public IServiceProvider
{
public:
  static already_AddRefed<IAccessible> GetRootAccessible(HWND aHwnd);
  static void EnableBlindAggregation(HWND aHwnd);

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID aIid, void** aOutInterface) override;
  STDMETHODIMP_(ULONG) AddRef() override;
  STDMETHODIMP_(ULONG) Release() override;

  // IDispatch
  STDMETHODIMP GetTypeInfoCount(UINT* pctinfo) override;
  STDMETHODIMP GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo) override;
  STDMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR *rgszNames, UINT cNames,
                             LCID lcid, DISPID* rgDispId) override;
  STDMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
                      DISPPARAMS* pDispParams, VARIANT* pVarResult,
                      EXCEPINFO* pExcepInfo, UINT* puArgErr) override;

  // IAccessible
  STDMETHODIMP get_accParent(IDispatch **ppdispParent) override;
  STDMETHODIMP get_accChildCount(long *pcountChildren) override;
  STDMETHODIMP get_accChild(VARIANT varChild, IDispatch **ppdispChild) override;
  STDMETHODIMP get_accName(VARIANT varChild, BSTR *pszName) override;
  STDMETHODIMP get_accValue(VARIANT varChild, BSTR *pszValue) override;
  STDMETHODIMP get_accDescription(VARIANT varChild, BSTR *pszDescription) override;
  STDMETHODIMP get_accRole(VARIANT varChild, VARIANT *pvarRole) override;
  STDMETHODIMP get_accState(VARIANT varChild, VARIANT *pvarState) override;
  STDMETHODIMP get_accHelp(VARIANT varChild, BSTR *pszHelp) override;
  STDMETHODIMP get_accHelpTopic(BSTR *pszHelpFile, VARIANT varChild, long *pidTopic) override;
  STDMETHODIMP get_accKeyboardShortcut(VARIANT varChild, BSTR *pszKeyboardShortcut) override;
  STDMETHODIMP get_accFocus(VARIANT *pvarChild) override;
  STDMETHODIMP get_accSelection(VARIANT *pvarChildren) override;
  STDMETHODIMP get_accDefaultAction(VARIANT varChild, BSTR *pszDefaultAction) override;
  STDMETHODIMP accSelect(long flagsSelect, VARIANT varChild) override;
  STDMETHODIMP accLocation(long *pxLeft, long *pyTop, long *pcxWidth, long *pcyHeight, VARIANT varChild) override;
  STDMETHODIMP accNavigate(long navDir, VARIANT varStart, VARIANT *pvarEndUpAt) override;
  STDMETHODIMP accHitTest(long xLeft, long yTop, VARIANT *pvarChild) override;
  STDMETHODIMP accDoDefaultAction(VARIANT varChild) override;
  STDMETHODIMP put_accName(VARIANT varChild, BSTR szName) override;
  STDMETHODIMP put_accValue(VARIANT varChild, BSTR szValue) override;

  // IServiceProvider
  STDMETHODIMP QueryService(REFGUID aServiceId, REFIID aServiceIid, void** aOutInterface) override;

private:
  explicit LazyInstantiator(HWND aHwnd);
  ~LazyInstantiator();

  bool ShouldInstantiate(const DWORD aClientTid);

  bool GetClientExecutableName(const DWORD aClientTid, nsIFile** aOutClientExe);
#if defined(MOZ_TELEMETRY_REPORTING)
  void AppendVersionInfo(nsIFile* aClientExe, nsAString& aStrToAppend);
  void GatherTelemetry(nsIFile* aClientExe);
  void AccumulateTelemetry(const nsString& aValue);
#endif // defined(MOZ_TELEMETRY_REPORTING)

  /**
   * @return S_OK if we have a valid mRealRoot to invoke methods on
   */
  HRESULT MaybeResolveRoot();

  /**
   * @return S_OK if we have a valid mWeakDispatch to invoke methods on
   */
  HRESULT ResolveDispatch();

  RootAccessibleWrap* ResolveRootAccWrap();
  void TransplantRefCnt();
  void ClearProp();

private:
  mozilla::a11y::AutoRefCnt mRefCnt;
  HWND                mHwnd;
  bool                mAllowBlindAggregation;
  RefPtr<IUnknown>    mRealRootUnk;
  RefPtr<IUnknown>    mStdDispatch;
  /**
   * mWeakRootAccWrap, mWeakAccessible and mWeakDispatch are weak because they
   * are interfaces that come from objects that we aggregate. Aggregated object
   * interfaces share refcount methods with ours, so if we were to hold strong
   * references to them, we would be holding strong references to ourselves,
   * creating a cycle.
   */
  RootAccessibleWrap* mWeakRootAccWrap;
  IAccessible*        mWeakAccessible;
  IDispatch*          mWeakDispatch;
#if defined(MOZ_TELEMETRY_REPORTING)
  nsCOMPtr<nsIThread> mTelemetryThread;
#endif // defined(MOZ_TELEMETRY_REPORTING)
};

} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_LazyInstantiator_h

