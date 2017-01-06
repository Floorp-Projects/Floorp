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
IMPL_IUNKNOWN_QUERY_IFACE(IEnumVARIANT)
IMPL_IUNKNOWN_QUERY_TAIL_AGGREGATED(mAnchorAcc)

STDMETHODIMP
ChildrenEnumVariant::Next(ULONG aCount, VARIANT FAR* aItems,
                          ULONG FAR* aCountFetched)
{
  if (!aItems || !aCountFetched)
    return E_INVALIDARG;

  *aCountFetched = 0;

  if (mAnchorAcc->IsDefunct() || mAnchorAcc->GetChildAt(mCurIndex) != mCurAcc)
    return CO_E_OBJNOTCONNECTED;

  ULONG countFetched = 0;
  while (mCurAcc && countFetched < aCount) {
    VariantInit(aItems + countFetched);

    IDispatch* accNative = AccessibleWrap::NativeAccessible(mCurAcc);

    ++mCurIndex;
    mCurAcc = mAnchorAcc->GetChildAt(mCurIndex);

    // Don't output the accessible and count it as having been fetched unless
    // it is non-null
    MOZ_ASSERT(accNative);
    if (!accNative) {
      continue;
    }

    aItems[countFetched].pdispVal = accNative;
    aItems[countFetched].vt = VT_DISPATCH;
    ++countFetched;
  }

  (*aCountFetched) = countFetched;

  return countFetched < aCount ? S_FALSE : S_OK;
}

STDMETHODIMP
ChildrenEnumVariant::Skip(ULONG aCount)
{
  if (mAnchorAcc->IsDefunct() || mAnchorAcc->GetChildAt(mCurIndex) != mCurAcc)
    return CO_E_OBJNOTCONNECTED;

  mCurIndex += aCount;
  mCurAcc = mAnchorAcc->GetChildAt(mCurIndex);

  return mCurAcc ? S_OK : S_FALSE;
}

STDMETHODIMP
ChildrenEnumVariant::Reset()
{
  if (mAnchorAcc->IsDefunct())
    return CO_E_OBJNOTCONNECTED;

  mCurIndex = 0;
  mCurAcc = mAnchorAcc->GetChildAt(0);

  return S_OK;
}

STDMETHODIMP
ChildrenEnumVariant::Clone(IEnumVARIANT** aEnumVariant)
{
  if (!aEnumVariant)
    return E_INVALIDARG;

  *aEnumVariant = new ChildrenEnumVariant(*this);
  (*aEnumVariant)->AddRef();

  return S_OK;
}
