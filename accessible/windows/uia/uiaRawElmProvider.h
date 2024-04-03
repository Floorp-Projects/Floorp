/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_uiaRawElmProvider_h__
#define mozilla_a11y_uiaRawElmProvider_h__

#include <objbase.h>
#include <stdint.h>
#include <uiautomation.h>

namespace mozilla {
namespace a11y {

class Accessible;

/**
 * IRawElementProviderSimple implementation (maintains IAccessibleEx approach).
 */
class uiaRawElmProvider : public IAccessibleEx,
                          public IRawElementProviderSimple,
                          public IRawElementProviderFragment,
                          public IInvokeProvider,
                          public IToggleProvider,
                          public IExpandCollapseProvider {
 public:
  static void RaiseUiaEventForGeckoEvent(Accessible* aAcc,
                                         uint32_t aGeckoEvent);
  static void RaiseUiaEventForStateChange(Accessible* aAcc, uint64_t aState,
                                          bool aEnabled);

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID aIid, void** aInterface);

  // IAccessibleEx
  virtual HRESULT STDMETHODCALLTYPE GetObjectForChild(
      /* [in] */ long aIdChild,
      /* [retval][out] */ __RPC__deref_out_opt IAccessibleEx** aAccEx);

  virtual HRESULT STDMETHODCALLTYPE GetIAccessiblePair(
      /* [out] */ __RPC__deref_out_opt IAccessible** aAcc,
      /* [out] */ __RPC__out long* aIdChild);

  virtual HRESULT STDMETHODCALLTYPE GetRuntimeId(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRuntimeIds);

  virtual HRESULT STDMETHODCALLTYPE ConvertReturnedElement(
      /* [in] */ __RPC__in_opt IRawElementProviderSimple* aRawElmProvider,
      /* [out] */ __RPC__deref_out_opt IAccessibleEx** aAccEx);

  // IRawElementProviderSimple
  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ProviderOptions(
      /* [retval][out] */ __RPC__out enum ProviderOptions* aProviderOptions);

  virtual HRESULT STDMETHODCALLTYPE GetPatternProvider(
      /* [in] */ PATTERNID aPatternId,
      /* [retval][out] */ __RPC__deref_out_opt IUnknown** aPatternProvider);

  virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(
      /* [in] */ PROPERTYID aPropertyId,
      /* [retval][out] */ __RPC__out VARIANT* aPropertyValue);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
      /* [retval][out] */ __RPC__deref_out_opt IRawElementProviderSimple**
          aRawElmProvider);

  // IRawElementProviderFragment
  virtual HRESULT STDMETHODCALLTYPE Navigate(
      /* [in] */ enum NavigateDirection aDirection,
      /* [retval][out] */ __RPC__deref_out_opt IRawElementProviderFragment**
          aRetVal);

  // GetRuntimeId is shared with IAccessibleEx.

  virtual HRESULT STDMETHODCALLTYPE get_BoundingRectangle(
      /* [retval][out] */ __RPC__out struct UiaRect* aRetVal);

  virtual HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(
      /* [retval][out] */ __RPC__deref_out_opt SAFEARRAY** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE SetFocus(void);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_FragmentRoot(
      /* [retval][out] */ __RPC__deref_out_opt IRawElementProviderFragmentRoot**
          aRetVal);

  // IInvokeProvider
  virtual HRESULT STDMETHODCALLTYPE Invoke(void);

  // IToggleProvider
  virtual HRESULT STDMETHODCALLTYPE Toggle(void);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ToggleState(
      /* [retval][out] */ __RPC__out enum ToggleState* aRetVal);

  // IExpandCollapseProvider
  virtual HRESULT STDMETHODCALLTYPE Expand(void);

  virtual HRESULT STDMETHODCALLTYPE Collapse(void);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(
      /* [retval][out] */ __RPC__out enum ExpandCollapseState* aRetVal);

 private:
  Accessible* Acc() const;
  bool IsControl();
  long GetControlType() const;
  bool HasTogglePattern();
  bool HasExpandCollapsePattern();
};

}  // namespace a11y
}  // namespace mozilla

#endif
