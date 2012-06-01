/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_EnumVariant_h__
#define mozilla_a11y_EnumVariant_h__

#include "AccessibleWrap.h"

namespace mozilla {
namespace a11y {

/**
 * Used to fetch accessible children.
 */
class ChildrenEnumVariant MOZ_FINAL : public IEnumVARIANT
{
public:
  ChildrenEnumVariant(AccessibleWrap* aAnchor) : mAnchorAcc(aAnchor),
    mCurAcc(mAnchorAcc->GetChildAt(0)), mCurIndex(0), mRefCnt(0) { }

  // IUnknown
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(
    /* [in] */ REFIID aRefIID,
    /* [annotation][iid_is][out] */ void** aObject);

  virtual ULONG STDMETHODCALLTYPE AddRef();
  virtual ULONG STDMETHODCALLTYPE Release();

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
  ChildrenEnumVariant() MOZ_DELETE;
  ChildrenEnumVariant& operator =(const ChildrenEnumVariant&) MOZ_DELETE;

  ChildrenEnumVariant(const ChildrenEnumVariant& aEnumVariant) :
    mAnchorAcc(aEnumVariant.mAnchorAcc), mCurAcc(aEnumVariant.mCurAcc),
    mCurIndex(aEnumVariant.mCurIndex), mRefCnt(0) { }
  virtual ~ChildrenEnumVariant() { }

protected:
  nsRefPtr<AccessibleWrap> mAnchorAcc;
  Accessible* mCurAcc;
  PRUint32 mCurIndex;

private:
  ULONG mRefCnt;
};

} // a11y namespace
} // mozilla namespace

#endif
