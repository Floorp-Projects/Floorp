/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_UiaRoot_h__
#define mozilla_a11y_UiaRoot_h__

#include "objbase.h"
#include "uiautomation.h"

namespace mozilla {
namespace a11y {
class Accessible;

/**
 * IRawElementProviderFragmentRoot implementation.
 */
class UiaRoot : public IRawElementProviderFragmentRoot {
 public:
  // IRawElementProviderFragmentRoot
  virtual HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(
      /* [in] */ double aX,
      /* [in] */ double aY,
      /* [retval][out] */
      __RPC__deref_out_opt IRawElementProviderFragment** aRetVal);

  virtual HRESULT STDMETHODCALLTYPE GetFocus(
      /* [retval][out] */ __RPC__deref_out_opt IRawElementProviderFragment**
          aRetVal);

 private:
  Accessible* Acc();
};

}  // namespace a11y
}  // namespace mozilla

#endif
