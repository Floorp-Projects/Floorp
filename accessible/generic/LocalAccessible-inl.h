/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_Accessible_inl_h_
#define mozilla_a11y_Accessible_inl_h_

#include "DocAccessible.h"
#include "ARIAMap.h"
#include "nsCoreUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/PresShell.h"

#ifdef A11Y_LOG
#  include "Logging.h"
#endif

namespace mozilla {
namespace a11y {

inline mozilla::a11y::role LocalAccessible::Role() const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry || roleMapEntry->roleRule != kUseMapRole) {
    return ARIATransformRole(NativeRole());
  }

  return ARIATransformRole(roleMapEntry->role);
}

inline mozilla::a11y::role LocalAccessible::ARIARole() {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry || roleMapEntry->roleRule != kUseMapRole) {
    return mozilla::a11y::roles::NOTHING;
  }

  return ARIATransformRole(roleMapEntry->role);
}

inline void LocalAccessible::SetRoleMapEntry(
    const nsRoleMapEntry* aRoleMapEntry) {
  mRoleMapEntryIndex = aria::GetIndexFromRoleMap(aRoleMapEntry);
}

inline bool LocalAccessible::IsSearchbox() const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return (roleMapEntry && roleMapEntry->Is(nsGkAtoms::searchbox)) ||
         (mContent->IsHTMLElement(nsGkAtoms::input) &&
          mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                             nsGkAtoms::search, eCaseMatters));
}

inline bool LocalAccessible::NativeHasNumericValue() const {
  return mGenericTypes & eNumericValue;
}

inline bool LocalAccessible::ARIAHasNumericValue() const {
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry || roleMapEntry->valueRule == eNoValue) return false;

  if (roleMapEntry->valueRule == eHasValueMinMaxIfFocusable) {
    return InteractiveState() & states::FOCUSABLE;
  }

  return true;
}

inline bool LocalAccessible::HasNumericValue() const {
  return NativeHasNumericValue() || ARIAHasNumericValue();
}

inline bool LocalAccessible::IsDefunct() const {
  MOZ_ASSERT(mStateFlags & eIsDefunct || IsApplication() || IsDoc() ||
                 IsProxy() || mStateFlags & eSharedNode || mContent,
             "No content");
  return mStateFlags & eIsDefunct;
}

inline void LocalAccessible::ScrollTo(uint32_t aHow) const {
  if (mContent) {
    RefPtr<PresShell> presShell = mDoc->PresShellPtr();
    nsCOMPtr<nsIContent> content = mContent;
    nsCoreUtils::ScrollTo(presShell, content, aHow);
  }
}

inline bool LocalAccessible::InsertAfter(LocalAccessible* aNewChild,
                                         LocalAccessible* aRefChild) {
  MOZ_ASSERT(aNewChild, "No new child to insert");

  if (aRefChild && aRefChild->LocalParent() != this) {
#ifdef A11Y_LOG
    logging::TreeInfo("broken accessible tree", 0, "parent", this,
                      "prev sibling parent", aRefChild->LocalParent(), "child",
                      aNewChild, nullptr);
    if (logging::IsEnabled(logging::eVerbose)) {
      logging::Tree("TREE", "Document tree", mDoc);
      logging::DOMTree("TREE", "DOM document tree", mDoc);
    }
#endif
    MOZ_ASSERT_UNREACHABLE("Broken accessible tree");
    mDoc->UnbindFromDocument(aNewChild);
    return false;
  }

  return InsertChildAt(aRefChild ? aRefChild->IndexInParent() + 1 : 0,
                       aNewChild);
}

}  // namespace a11y
}  // namespace mozilla

#endif
