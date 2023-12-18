/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimatedPropertyIDSet_h
#define mozilla_AnimatedPropertyIDSet_h

#include "mozilla/ServoBindings.h"

#include "AnimatedPropertyID.h"
#include "nsTHashSet.h"

namespace mozilla {

class AnimatedPropertyIDSet {
 public:
  void AddProperty(const AnimatedPropertyID& aProperty) {
    if (aProperty.IsCustom()) {
      mCustomNames.Insert(aProperty.mCustomName);
    } else {
      mIDs.AddProperty(aProperty.mID);
    }
  }

  void RemoveProperty(const AnimatedPropertyID& aProperty) {
    if (aProperty.IsCustom()) {
      mCustomNames.Remove(aProperty.mCustomName);
    } else {
      mIDs.RemoveProperty(aProperty.mID);
    }
  }

  bool HasProperty(const AnimatedPropertyID& aProperty) const {
    if (aProperty.IsCustom()) {
      return mCustomNames.Contains(aProperty.mCustomName);
    }
    return mIDs.HasProperty(aProperty.mID);
  }

  bool Intersects(const nsCSSPropertyIDSet& aIDs) const {
    return mIDs.Intersects(aIDs);
  }

  bool IsSubsetOf(const AnimatedPropertyIDSet& aOther) const {
    if (!mIDs.IsSubsetOf(aOther.mIDs) ||
        mCustomNames.Count() > aOther.mCustomNames.Count()) {
      return false;
    }
    for (const auto& name : mCustomNames) {
      if (!aOther.mCustomNames.Contains(name)) {
        return false;
      }
    }
    return true;
  }

  void AddProperties(const AnimatedPropertyIDSet& aOther) {
    mIDs |= aOther.mIDs;
    for (const auto& name : aOther.mCustomNames) {
      mCustomNames.Insert(name);
    }
  }

 private:
  using CustomNameSet = nsTHashSet<RefPtr<nsAtom>>;

 public:
  // Iterator for use in range-based for loops
  class Iterator {
   public:
    Iterator(Iterator&& aOther)
        : mPropertySet(aOther.mPropertySet),
          mIDIterator(std::move(aOther.mIDIterator)),
          mCustomNameIterator(std::move(aOther.mCustomNameIterator)),
          mPropertyID(eCSSProperty_UNKNOWN) {}
    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(const Iterator&&) = delete;

    static Iterator BeginIterator(const AnimatedPropertyIDSet& aPropertySet) {
      return Iterator(aPropertySet, aPropertySet.mIDs.begin(),
                      aPropertySet.mCustomNames.begin());
    }

    static Iterator EndIterator(const AnimatedPropertyIDSet& aPropertySet) {
      return Iterator(aPropertySet, aPropertySet.mIDs.end(),
                      aPropertySet.mCustomNames.end());
    }

    bool operator!=(const Iterator& aOther) const {
      return mIDIterator != aOther.mIDIterator ||
             mCustomNameIterator != aOther.mCustomNameIterator;
    }

    Iterator& operator++() {
      MOZ_ASSERT(mIDIterator != mPropertySet.mIDs.end() ||
                     mCustomNameIterator != mPropertySet.mCustomNames.end(),
                 "Should not iterate beyond end");
      if (mIDIterator != mPropertySet.mIDs.end()) {
        ++mIDIterator;
      } else {
        ++mCustomNameIterator;
      }
      return *this;
    }

    AnimatedPropertyID operator*() {
      if (mIDIterator != mPropertySet.mIDs.end()) {
        mPropertyID.mID = *mIDIterator;
        mPropertyID.mCustomName = nullptr;
      } else if (mCustomNameIterator != mPropertySet.mCustomNames.end()) {
        mPropertyID.mID = eCSSPropertyExtra_variable;
        mPropertyID.mCustomName = *mCustomNameIterator;
      } else {
        MOZ_ASSERT_UNREACHABLE("Should not dereference beyond end");
        mPropertyID.mID = eCSSProperty_UNKNOWN;
        mPropertyID.mCustomName = nullptr;
      }
      return mPropertyID;
    }

   private:
    explicit Iterator(const AnimatedPropertyIDSet& aPropertySet,
                      nsCSSPropertyIDSet::Iterator aIDIterator,
                      CustomNameSet::const_iterator aCustomNameIterator)
        : mPropertySet(aPropertySet),
          mIDIterator(std::move(aIDIterator)),
          mCustomNameIterator(std::move(aCustomNameIterator)),
          mPropertyID(eCSSProperty_UNKNOWN) {}

    const AnimatedPropertyIDSet& mPropertySet;
    nsCSSPropertyIDSet::Iterator mIDIterator;
    CustomNameSet::const_iterator mCustomNameIterator;
    AnimatedPropertyID mPropertyID;
  };

  Iterator begin() const { return Iterator::BeginIterator(*this); }
  Iterator end() const { return Iterator::EndIterator(*this); }

 private:
  nsCSSPropertyIDSet mIDs;
  CustomNameSet mCustomNames;
};

}  // namespace mozilla

#endif  // mozilla_AnimatedPropertyIDSet_h
