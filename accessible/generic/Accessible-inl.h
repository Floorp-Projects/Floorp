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
  if (!mRoleMapEntry || mRoleMapEntry->roleRule != kUseMapRole)
    return ARIATransformRole(NativeRole());

  return ARIATransformRole(mRoleMapEntry->role);
}

inline bool
Accessible::IsARIARole(nsIAtom* aARIARole) const
{
  return mRoleMapEntry && mRoleMapEntry->Is(aARIARole);
}

inline bool
Accessible::HasStrongARIARole() const
{
  return mRoleMapEntry && mRoleMapEntry->roleRule == kUseMapRole;
}

inline mozilla::a11y::role
Accessible::ARIARole()
{
  if (!mRoleMapEntry || mRoleMapEntry->roleRule != kUseMapRole)
    return mozilla::a11y::roles::NOTHING;

  return ARIATransformRole(mRoleMapEntry->role);
}

inline bool
Accessible::IsSearchbox() const
{
  return (mRoleMapEntry && mRoleMapEntry->Is(nsGkAtoms::searchbox)) ||
    (mContent->IsHTMLElement(nsGkAtoms::input) &&
     mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                           nsGkAtoms::textInputType, eCaseMatters));
}

inline bool
Accessible::HasGenericType(AccGenericType aType) const
{
  return (mGenericTypes & aType) ||
    (mRoleMapEntry && mRoleMapEntry->IsOfType(aType));
}

inline bool
Accessible::HasNumericValue() const
{
  if (mStateFlags & eHasNumericValue)
    return true;

  return mRoleMapEntry && mRoleMapEntry->valueRule != eNoValue;
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
