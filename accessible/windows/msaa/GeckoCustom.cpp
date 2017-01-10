/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoCustom.h"

using namespace mozilla;
using namespace mozilla::a11y;

IMPL_IUNKNOWN_QUERY_HEAD(GeckoCustom)
IMPL_IUNKNOWN_QUERY_IFACE(IGeckoCustom)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mAcc)

HRESULT
GeckoCustom::get_anchorCount(long* aCount)
{
  *aCount = mAcc->AnchorCount();
  return S_OK;
}

HRESULT
GeckoCustom::get_DOMNodeID(BSTR* aID)
{
  nsIContent* content = mAcc->GetContent();
  if (!content) {
    return S_OK;
  }

  nsIAtom* id = content->GetID();
  if (id) {
    nsAutoString idStr;
    id->ToString(idStr);
    *aID = ::SysAllocStringLen(idStr.get(), idStr.Length());
  }
  return S_OK;
}

STDMETHODIMP
GeckoCustom::get_ID(uint64_t* aID)
{
  *aID = mAcc->IsDoc() ? 0 : reinterpret_cast<uintptr_t>(mAcc.get());
  return S_OK;
}
