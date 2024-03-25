/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UiaRoot.h"

#include "MsaaRootAccessible.h"
#include "RootAccessible.h"

using namespace mozilla;
using namespace mozilla::a11y;

// UiaRoot

Accessible* UiaRoot::Acc() {
  auto* mr = static_cast<MsaaRootAccessible*>(this);
  return static_cast<MsaaAccessible*>(mr)->Acc();
}

// IRawElementProviderFragmentRoot

STDMETHODIMP
UiaRoot::ElementProviderFromPoint(
    double aX, double aY,
    __RPC__deref_out_opt IRawElementProviderFragment** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  if (Accessible* target = acc->ChildAtPoint(
          aX, aY, Accessible::EWhichChildAtPoint::DeepestChild)) {
    RefPtr<IRawElementProviderFragment> fragment =
        MsaaAccessible::GetFrom(target);
    fragment.forget(aRetVal);
  }
  return S_OK;
}

STDMETHODIMP
UiaRoot::GetFocus(__RPC__deref_out_opt IRawElementProviderFragment** aRetVal) {
  if (!aRetVal) {
    return E_INVALIDARG;
  }
  *aRetVal = nullptr;
  Accessible* acc = Acc();
  if (!acc) {
    return CO_E_OBJNOTCONNECTED;
  }
  if (Accessible* focus = acc->FocusedChild()) {
    RefPtr<IRawElementProviderFragment> fragment =
        MsaaAccessible::GetFrom(focus);
    fragment.forget(aRetVal);
  }
  return S_OK;
}
