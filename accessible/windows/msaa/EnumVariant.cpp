/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EnumVariant.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// ChildrenEnumVariant
////////////////////////////////////////////////////////////////////////////////

IMPL_IUNKNOWN_QUERY_HEAD(ChildrenEnumVariant)
IMPL_IUNKNOWN_QUERY_IFACE(IEnumVARIANT);
IMPL_IUNKNOWN_QUERY_IFACE(IUnknown);
IMPL_IUNKNOWN_QUERY_AGGR_COND(mAnchorAcc, !mAnchorAcc->IsDefunct());
IMPL_IUNKNOWN_QUERY_TAIL

STDMETHODIMP
ChildrenEnumVariant::Next(ULONG aCount, VARIANT FAR* aItems,
                          ULONG FAR* aCountFetched)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aItems || !aCountFetched)
    return E_INVALIDARG;

  *aCountFetched = 0;

  if (mAnchorAcc->IsDefunct() || mAnchorAcc->GetChildAt(mCurIndex) != mCurAcc)
    return CO_E_OBJNOTCONNECTED;

  ULONG countFetched = 0;
  for (; mCurAcc && countFetched < aCount; countFetched++) {
    VariantInit(aItems + countFetched);
    aItems[countFetched].pdispVal = AccessibleWrap::NativeAccessible(mCurAcc);
    aItems[countFetched].vt = VT_DISPATCH;

    mCurIndex++;
    mCurAcc = mAnchorAcc->GetChildAt(mCurIndex);
  }

  (*aCountFetched) = countFetched;

  return countFetched < aCount ? S_FALSE : S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ChildrenEnumVariant::Skip(ULONG aCount)
{
  A11Y_TRYBLOCK_BEGIN

  if (mAnchorAcc->IsDefunct() || mAnchorAcc->GetChildAt(mCurIndex) != mCurAcc)
    return CO_E_OBJNOTCONNECTED;

  mCurIndex += aCount;
  mCurAcc = mAnchorAcc->GetChildAt(mCurIndex);

  return mCurAcc ? S_OK : S_FALSE;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ChildrenEnumVariant::Reset()
{
  A11Y_TRYBLOCK_BEGIN

  if (mAnchorAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  mCurIndex = 0;
  mCurAcc = mAnchorAcc->GetChildAt(0);

  return S_OK;

  A11Y_TRYBLOCK_END
}

STDMETHODIMP
ChildrenEnumVariant::Clone(IEnumVARIANT** aEnumVariant)
{
  A11Y_TRYBLOCK_BEGIN

  if (!aEnumVariant)
    return E_INVALIDARG;

  *aEnumVariant = new ChildrenEnumVariant(*this);
  (*aEnumVariant)->AddRef();

  return S_OK;

  A11Y_TRYBLOCK_END
}
