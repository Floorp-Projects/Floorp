/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/AgileReference.h"

#include <utility>

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/mscom/Utils.h"

#if defined(__MINGW32__)

// Declarations from Windows SDK specific to Windows 8.1

enum AgileReferenceOptions {
  AGILEREFERENCE_DEFAULT = 0,
  AGILEREFERENCE_DELAYEDMARSHAL = 1,
};

HRESULT WINAPI RoGetAgileReference(AgileReferenceOptions options, REFIID riid,
                                   IUnknown* pUnk,
                                   IAgileReference** ppAgileReference);

#endif  // defined(__MINGW32__)

namespace mozilla::mscom {

AgileReference::AgileReference() : mIid(), mHResult(E_NOINTERFACE) {}

AgileReference::AgileReference(REFIID aIid, IUnknown* aObject)
    : mIid(aIid), mHResult(E_UNEXPECTED) {
  AssignInternal(aObject);
}

AgileReference::AgileReference(AgileReference&& aOther) noexcept
    : mIid(aOther.mIid),
      mAgileRef(std::move(aOther.mAgileRef)),
      mHResult(aOther.mHResult) {
  aOther.mHResult = CO_E_RELEASED;
}

void AgileReference::Assign(REFIID aIid, IUnknown* aObject) {
  Clear();
  mIid = aIid;
  AssignInternal(aObject);
}

void AgileReference::AssignInternal(IUnknown* aObject) {
  // We expect mIid to already be set
  DebugOnly<IID> zeroIid = {};
  MOZ_ASSERT(mIid != zeroIid);

  MOZ_ASSERT(aObject);

  mHResult = RoGetAgileReference(AGILEREFERENCE_DEFAULT, mIid, aObject,
                                 getter_AddRefs(mAgileRef));
}

AgileReference::~AgileReference() { Clear(); }

void AgileReference::Clear() {
  mIid = {};
  mAgileRef = nullptr;
  mHResult = E_NOINTERFACE;
}

AgileReference& AgileReference::operator=(const AgileReference& aOther) {
  Clear();
  mIid = aOther.mIid;
  mAgileRef = aOther.mAgileRef;
  mHResult = aOther.mHResult;
  return *this;
}

AgileReference& AgileReference::operator=(AgileReference&& aOther) noexcept {
  Clear();
  mIid = aOther.mIid;
  mAgileRef = std::move(aOther.mAgileRef);
  mHResult = aOther.mHResult;
  aOther.mHResult = CO_E_RELEASED;
  return *this;
}

HRESULT
AgileReference::ResolveRaw(REFIID aIid, void** aOutInterface) const {
  MOZ_ASSERT(aOutInterface);
  MOZ_ASSERT(mAgileRef);
  MOZ_ASSERT(IsCOMInitializedOnCurrentThread());

  if (!aOutInterface) {
    return E_INVALIDARG;
  }

  *aOutInterface = nullptr;

  if (mAgileRef) {
    return mAgileRef->Resolve(aIid, aOutInterface);
  }

  return E_NOINTERFACE;
}

}  // namespace mozilla::mscom
