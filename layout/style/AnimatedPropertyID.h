/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimatedPropertyID_h
#define mozilla_AnimatedPropertyID_h

#include "nsCSSPropertyID.h"
#include "nsCSSProps.h"
#include "nsString.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/ServoBindings.h"

namespace mozilla {

struct AnimatedPropertyID {
  explicit AnimatedPropertyID(nsCSSPropertyID aProperty) : mID(aProperty) {
    MOZ_ASSERT(aProperty != eCSSPropertyExtra_variable,
               "Cannot create an AnimatedPropertyID from only a "
               "eCSSPropertyExtra_variable.");
  }

  explicit AnimatedPropertyID(RefPtr<nsAtom> aCustomName)
      : mID(eCSSPropertyExtra_variable), mCustomName(std::move(aCustomName)) {
    MOZ_ASSERT(mCustomName, "Null custom property name");
  }

  nsCSSPropertyID mID = eCSSProperty_UNKNOWN;
  RefPtr<nsAtom> mCustomName;

  bool IsCustom() const { return mID == eCSSPropertyExtra_variable; }
  bool operator==(const AnimatedPropertyID& aOther) const {
    return mID == aOther.mID && mCustomName == aOther.mCustomName;
  }
  bool operator!=(const AnimatedPropertyID& aOther) const {
    return !(*this == aOther);
  }

  bool IsValid() const {
    if (mID == eCSSProperty_UNKNOWN) {
      return false;
    }
    return IsCustom() == !!mCustomName;
  }

  void ToString(nsAString& aString) const {
    if (IsCustom()) {
      MOZ_ASSERT(mCustomName, "Custom property should have a name");
      // mCustomName does not include the "--" prefix
      aString.AssignLiteral("--");
      aString.Append(nsDependentAtomString(mCustomName));
    } else {
      aString.Assign(NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(mID)));
    }
  }

  HashNumber Hash() const {
    HashNumber hash = mCustomName ? mCustomName->hash() : 0;
    return AddToHash(hash, mID);
  }

  AnimatedPropertyID ToPhysical(const ComputedStyle& aStyle) const {
    if (IsCustom()) {
      return *this;
    }
    return AnimatedPropertyID{nsCSSProps::Physicalize(mID, aStyle)};
  }
};

// MOZ_DBG support for AnimatedPropertyId
inline std::ostream& operator<<(std::ostream& aOut,
                                const AnimatedPropertyID& aProperty) {
  if (aProperty.IsCustom()) {
    return aOut << nsAtomCString(aProperty.mCustomName);
  }
  return aOut << nsCSSProps::GetStringValue(aProperty.mID);
}

}  // namespace mozilla

#endif  // mozilla_AnimatedPropertyID_h
