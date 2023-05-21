/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Accessible2.h"
#include "RemoteAccessible.h"
#include "ia2AccessibleRelation.h"
#include "ia2AccessibleValue.h"
#include "IGeckoCustom.h"
#include "mozilla/a11y/DocAccessibleParent.h"
#include "DocAccessible.h"
#include "mozilla/a11y/DocManager.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/Unused.h"
#include "mozilla/a11y/Platform.h"
#include "Relation.h"
#include "RelationType.h"
#include "mozilla/a11y/Role.h"
#include "nsAccessibilityService.h"

#include <comutil.h>

namespace mozilla {
namespace a11y {

bool RemoteAccessible::GetCOMInterface(void** aOutAccessible) const {
  if (!aOutAccessible) {
    return false;
  }

  // This should never be called if the cache is enabled. We can't get a COM
  // proxy from the content process in that case. Instead, the code below would
  // return an MsaaAccessible from our process which would end up calling
  // methods here in RemoteAccessible, causing infinite recursion.
  MOZ_ASSERT(!a11y::IsCacheActive());
  if (!mCOMProxy && mSafeToRecurse) {
    WeakPtr<RemoteAccessible> thisPtr = const_cast<RemoteAccessible*>(this);
    // See if we can lazily obtain a COM proxy
    MsaaAccessible* msaa = MsaaAccessible::GetFrom(thisPtr);
    bool isDefunct = false;
    // NB: Don't pass CHILDID_SELF here, use the absolute MSAA ID. Otherwise
    // GetIAccessibleFor will recurse into this function and we will just
    // overflow the stack.
    VARIANT realId = {{{VT_I4}}};
    realId.ulVal = msaa->GetExistingID();
    MOZ_DIAGNOSTIC_ASSERT(realId.ulVal != CHILDID_SELF);
    RefPtr<IAccessible> proxy = msaa->GetIAccessibleFor(realId, &isDefunct);
    if (!thisPtr) {
      *aOutAccessible = nullptr;
      return false;
    }
    thisPtr->mCOMProxy = proxy;
  }

  RefPtr<IAccessible> addRefed = mCOMProxy;
  addRefed.forget(aOutAccessible);
  return !!mCOMProxy;
}

/**
 * Specializations of this template map an IAccessible type to its IID
 */
template <typename Interface>
struct InterfaceIID {};

template <>
struct InterfaceIID<IAccessibleValue> {
  static REFIID Value() { return IID_IAccessibleValue; }
};

template <>
struct InterfaceIID<IAccessibleText> {
  static REFIID Value() { return IID_IAccessibleText; }
};

template <>
struct InterfaceIID<IAccessibleHyperlink> {
  static REFIID Value() { return IID_IAccessibleHyperlink; }
};

template <>
struct InterfaceIID<IGeckoCustom> {
  static REFIID Value() { return IID_IGeckoCustom; }
};

template <>
struct InterfaceIID<IAccessible2_2> {
  static REFIID Value() { return IID_IAccessible2_2; }
};

/**
 * Get the COM proxy for this proxy accessible and QueryInterface it with the
 * correct IID
 */
template <typename Interface>
static already_AddRefed<Interface> QueryInterface(
    const RemoteAccessible* aProxy) {
  RefPtr<IAccessible> acc;
  if (!aProxy->GetCOMInterface((void**)getter_AddRefs(acc))) {
    return nullptr;
  }

  RefPtr<Interface> acc2;
  if (FAILED(acc->QueryInterface(InterfaceIID<Interface>::Value(),
                                 (void**)getter_AddRefs(acc2)))) {
    return nullptr;
  }

  return acc2.forget();
}

/**
 * aCoordinateType is one of the nsIAccessibleCoordinateType constants.
 */
void RemoteAccessible::ScrollSubstringToPoint(int32_t aStartOffset,
                                              int32_t aEndOffset,
                                              uint32_t aCoordinateType,
                                              int32_t aX, int32_t aY) {
  if (a11y::IsCacheActive()) {
    // Not yet supported by the cache.
    return;
  }
  RefPtr<IAccessibleText> acc = QueryInterface<IAccessibleText>(this);
  if (!acc) {
    return;
  }

  IA2CoordinateType coordType;
  if (aCoordinateType ==
      nsIAccessibleCoordinateType::COORDTYPE_SCREEN_RELATIVE) {
    coordType = IA2_COORDTYPE_SCREEN_RELATIVE;
  } else if (aCoordinateType ==
             nsIAccessibleCoordinateType::COORDTYPE_PARENT_RELATIVE) {
    coordType = IA2_COORDTYPE_PARENT_RELATIVE;
  } else {
    MOZ_CRASH("unsupported coord type");
  }

  acc->scrollSubstringToPoint(static_cast<long>(aStartOffset),
                              static_cast<long>(aEndOffset), coordType,
                              static_cast<long>(aX), static_cast<long>(aY));
}

}  // namespace a11y
}  // namespace mozilla
