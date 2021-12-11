/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZILLA_INTERNAL_API)
#  error This code is NOT for internal Gecko use!
#endif  // defined(MOZILLA_INTERNAL_API)

#include "HandlerRelation.h"
#include "mozilla/Assertions.h"

#include "AccessibleRelation_i.c"

namespace mozilla {
namespace a11y {

HandlerRelation::HandlerRelation(AccessibleHandler* aHandler,
                                 IARelationData& aData)
    : mHandler(aHandler), mData(aData), mTargets(nullptr) {
  // This instance now owns any pointers, so ensure no one else can
  // manipulate them.
  aData.mType = nullptr;
}

HandlerRelation::~HandlerRelation() {
  if (mData.mType) {
    ::SysFreeString(mData.mType);
  }

  if (mTargets) {
    for (long index = 0; index < mData.mNTargets; ++index) {
      mTargets[index]->Release();
    }
    ::CoTaskMemFree(mTargets);
    mTargets = nullptr;
  }
}

HRESULT
HandlerRelation::GetTargets() {
  if (mTargets) {
    // Already cached.
    return S_OK;
  }

  // Marshaling all of the IAccessibleRelation objects across processes is
  // slow, and the client probably only wants targets for a few of them.
  // Therefore, we just use IAccessible2_2::relationTargetsOfType, passing
  // the type we have cached. This is a bit inefficient because Gecko has
  // to look up the relation twice, but it's significantly faster than
  // marshaling the relation objects regardless.
  return mHandler->get_relationTargetsOfType(mData.mType, 0, &mTargets,
                                             &mData.mNTargets);
}

IMPL_IUNKNOWN1(HandlerRelation, IAccessibleRelation)

/*** IAccessibleRelation ***/

HRESULT
HandlerRelation::get_relationType(BSTR* aType) {
  if (!aType) {
    return E_INVALIDARG;
  }
  if (!mData.mType) {
    return E_FAIL;
  }
  *aType = CopyBSTR(mData.mType);
  return S_OK;
}

HRESULT
HandlerRelation::get_localizedRelationType(BSTR* aLocalizedType) {
  // This is not implemented as per ia2AccessibleRelation.
  return E_NOTIMPL;
}

HRESULT
HandlerRelation::get_nTargets(long* aNTargets) {
  if (!aNTargets) {
    return E_INVALIDARG;
  }
  if (mData.mNTargets == -1) {
    return E_FAIL;
  }
  *aNTargets = mData.mNTargets;
  return S_OK;
}

HRESULT
HandlerRelation::get_target(long aIndex, IUnknown** aTarget) {
  if (!aTarget) {
    return E_INVALIDARG;
  }

  HRESULT hr = GetTargets();
  if (FAILED(hr)) {
    return hr;
  }
  if (aIndex >= mData.mNTargets) {
    return E_INVALIDARG;
  }

  *aTarget = mTargets[aIndex];
  (*aTarget)->AddRef();
  return S_OK;
}

HRESULT
HandlerRelation::get_targets(long aMaxTargets, IUnknown** aTargets,
                             long* aNTargets) {
  if (aMaxTargets == 0 || !aTargets || !aNTargets) {
    return E_INVALIDARG;
  }

  HRESULT hr = GetTargets();
  if (FAILED(hr)) {
    return hr;
  }

  if (mData.mNTargets > aMaxTargets) {
    // Don't give back more targets than were requested.
    *aNTargets = aMaxTargets;
  } else {
    *aNTargets = mData.mNTargets;
  }

  for (long index = 0; index < *aNTargets; ++index) {
    aTargets[index] = mTargets[index];
    aTargets[index]->AddRef();
  }
  return S_OK;
}

}  // namespace a11y
}  // namespace mozilla
