/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccessibleWrap.h"
#include "GeckoCustom.h"

using namespace mozilla;
using namespace mozilla::a11y;

IMPL_IUNKNOWN_QUERY_HEAD(GeckoCustom)
IMPL_IUNKNOWN_QUERY_IFACE(IGeckoCustom)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mMsaa)

HRESULT
GeckoCustom::get_anchorCount(long* aCount) {
  *aCount = mMsaa->LocalAcc()->AnchorCount();
  return S_OK;
}

HRESULT
GeckoCustom::get_boundsInCSSPixels(int32_t* aX, int32_t* aY, int32_t* aWidth,
                                   int32_t* aHeight) {
  nsIntRect bounds = mMsaa->LocalAcc()->BoundsInCSSPixels();
  if (!bounds.IsEmpty()) {
    *aWidth = bounds.Width();
    *aHeight = bounds.Height();
  }
  // We should always report positional bounds info, even if
  // our rect is empty.
  *aX = bounds.X();
  *aY = bounds.Y();

  return S_OK;
}

HRESULT
GeckoCustom::get_DOMNodeID(BSTR* aID) {
  nsIContent* content = mMsaa->LocalAcc()->GetContent();
  if (!content) {
    return S_OK;
  }

  nsAtom* id = content->GetID();
  if (id) {
    nsAutoString idStr;
    id->ToString(idStr);
    *aID = ::SysAllocStringLen(idStr.get(), idStr.Length());
  }
  return S_OK;
}

STDMETHODIMP
GeckoCustom::get_ID(uint64_t* aID) {
  AccessibleWrap* acc = mMsaa->LocalAcc();
  *aID = acc->IsDoc() ? 0 : reinterpret_cast<uintptr_t>(acc);
  return S_OK;
}

STDMETHODIMP
GeckoCustom::get_minimumIncrement(double* aIncrement) {
  *aIncrement = mMsaa->LocalAcc()->Step();
  return S_OK;
}

STDMETHODIMP
GeckoCustom::get_mozState(uint64_t* aState) {
  *aState = mMsaa->LocalAcc()->State();
  return S_OK;
}
