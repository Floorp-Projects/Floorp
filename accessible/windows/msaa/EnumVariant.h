/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_EnumVariant_h__
#define mozilla_a11y_EnumVariant_h__

#include "AccessibleWrap.h"
#include "IUnknownImpl.h"

namespace mozilla {
namespace a11y {

/**
 * Used to fetch accessible children.
 */
class ChildrenEnumVariant final : public IEnumVARIANT
{
public:
  ChildrenEnumVariant(AccessibleWrap* aAnchor) : mAnchorAcc(aAnchor),
    mCurAcc(mAnchorAcc->GetChildAt(0)), mCurIndex(0) { }

  // IUnknown
  DECL_IUNKNOWN

  // IEnumVariant
  virtual /* [local] */ HRESULT STDMETHODCALLTYPE Next(
    /* [in] */ ULONG aCount,
    /* [length_is][size_is][out] */ VARIANT* aItems,
    /* [out] */ ULONG* aCountFetched);

  virtual HRESULT STDMETHODCALLTYPE Skip(
    /* [in] */ ULONG aCount);

  virtual HRESULT STDMETHODCALLTYPE Reset();

  virtual HRESULT STDMETHODCALLTYPE Clone(
    /* [out] */ IEnumVARIANT** aEnumVaraint);

private:
  ChildrenEnumVariant() = delete;
  ChildrenEnumVariant& operator =(const ChildrenEnumVariant&) = delete;

  ChildrenEnumVariant(const ChildrenEnumVariant& aEnumVariant) :
    mAnchorAcc(aEnumVariant.mAnchorAcc), mCurAcc(aEnumVariant.mCurAcc),
    mCurIndex(aEnumVariant.mCurIndex) { }
  virtual ~ChildrenEnumVariant() { }

protected:
  RefPtr<AccessibleWrap> mAnchorAcc;
  Accessible* mCurAcc;
  uint32_t mCurIndex;
};

} // a11y namespace
} // mozilla namespace

#endif
