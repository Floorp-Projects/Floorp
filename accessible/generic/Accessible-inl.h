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

#ifdef A11Y_LOG
#include "Logging.h"
#endif

namespace mozilla {
namespace a11y {

inline mozilla::a11y::role
Accessible::Role()
{
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry || roleMapEntry->roleRule != kUseMapRole)
    return ARIATransformRole(NativeRole());

  return ARIATransformRole(roleMapEntry->role);
}

inline bool
Accessible::HasARIARole() const
{
  return mRoleMapEntryIndex != aria::NO_ROLE_MAP_ENTRY_INDEX;
}

inline bool
Accessible::IsARIARole(nsIAtom* aARIARole) const
{
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return roleMapEntry && roleMapEntry->Is(aARIARole);
}

inline bool
Accessible::HasStrongARIARole() const
{
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return roleMapEntry && roleMapEntry->roleRule == kUseMapRole;
}

inline const nsRoleMapEntry*
Accessible::ARIARoleMap() const
{
  return aria::GetRoleMapFromIndex(mRoleMapEntryIndex);
}

inline mozilla::a11y::role
Accessible::ARIARole()
{
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry || roleMapEntry->roleRule != kUseMapRole)
    return mozilla::a11y::roles::NOTHING;

  return ARIATransformRole(roleMapEntry->role);
}

inline void
Accessible::SetRoleMapEntry(const nsRoleMapEntry* aRoleMapEntry)
{
  mRoleMapEntryIndex = aria::GetIndexFromRoleMap(aRoleMapEntry);
}

inline bool
Accessible::IsSearchbox() const
{
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return (roleMapEntry && roleMapEntry->Is(nsGkAtoms::searchbox)) ||
    (mContent->IsHTMLElement(nsGkAtoms::input) &&
     mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                           nsGkAtoms::textInputType, eCaseMatters));
}

inline bool
Accessible::HasGenericType(AccGenericType aType) const
{
  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  return (mGenericTypes & aType) ||
    (roleMapEntry && roleMapEntry->IsOfType(aType));
}

inline bool
Accessible::HasNumericValue() const
{
  if (mStateFlags & eHasNumericValue)
    return true;

  const nsRoleMapEntry* roleMapEntry = ARIARoleMap();
  if (!roleMapEntry || roleMapEntry->valueRule == eNoValue)
    return false;

  if (roleMapEntry->valueRule == eHasValueMinMaxIfFocusable)
    return InteractiveState() & states::FOCUSABLE;

  return true;
}

inline void
Accessible::ScrollTo(uint32_t aHow) const
{
  if (mContent)
    nsCoreUtils::ScrollTo(mDoc->PresShell(), mContent, aHow);
}

inline bool
Accessible::InsertAfter(Accessible* aNewChild, Accessible* aRefChild)
{
  MOZ_ASSERT(aNewChild, "No new child to insert");

  if (aRefChild && aRefChild->Parent() != this) {
#ifdef A11Y_LOG
    logging::TreeInfo("broken accessible tree", 0,
                      "parent", this, "prev sibling parent",
                      aRefChild->Parent(), "child", aNewChild, nullptr);
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

} // namespace a11y
} // namespace mozilla

#endif
