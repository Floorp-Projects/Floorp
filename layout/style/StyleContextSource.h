/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleContextSource_h
#define mozilla_StyleContextSource_h

#include "mozilla/ServoBindingHelpers.h"
#include "nsRuleNode.h"

namespace mozilla {

// Tagged union between Gecko Rule Nodes and Servo Computed Values.
//
// The rule node is the node in the lexicographic tree of rule nodes
// (the "rule tree") that indicates which style rules are used to
// compute the style data, and in what cascading order.  The least
// specific rule matched is the one whose rule node is a child of the
// root of the rule tree, and the most specific rule matched is the
// |mRule| member of the rule node.
//
// In the Servo case, we hold an atomically-refcounted handle to a
// Servo ComputedValues struct, which is more or less the Servo equivalent
// of an nsStyleContext.

// Underlying pointer without any strong ownership semantics.
struct NonOwningStyleContextSource
{
  MOZ_IMPLICIT NonOwningStyleContextSource(nsRuleNode* aRuleNode)
    : mBits(reinterpret_cast<uintptr_t>(aRuleNode)) {}
  explicit NonOwningStyleContextSource(ServoComputedValues* aComputedValues)
    : mBits(reinterpret_cast<uintptr_t>(aComputedValues) | 1) {}

  bool operator==(const NonOwningStyleContextSource& aOther) const {
    MOZ_ASSERT(IsServoComputedValues() == aOther.IsServoComputedValues(),
               "Comparing Servo to Gecko - probably a bug");
    return mBits == aOther.mBits;
  }
  bool operator!=(const NonOwningStyleContextSource& aOther) const {
    return !(*this == aOther);
  }

  // We intentionally avoid exposing IsGeckoRuleNode() here, because that would
  // encourage callers to do:
  //
  // if (source.IsGeckoRuleNode()) {
  //   // Code that we would run unconditionally if it weren't for Servo.
  // }
  //
  // We want these branches to compile away when MOZ_STYLO is disabled, but that
  // won't happen if there's an implicit null-check.
  bool IsNull() const { return !mBits; }
  bool IsGeckoRuleNodeOrNull() const { return !IsServoComputedValues(); }
  bool IsServoComputedValues() const {
#ifdef MOZ_STYLO
    return mBits & 1;
#else
    return false;
#endif
  }

  nsRuleNode* AsGeckoRuleNode() const {
    MOZ_ASSERT(IsGeckoRuleNodeOrNull() && !IsNull());
    return reinterpret_cast<nsRuleNode*>(mBits);
  }

  ServoComputedValues* AsServoComputedValues() const {
    MOZ_ASSERT(IsServoComputedValues());
    return reinterpret_cast<ServoComputedValues*>(mBits & ~1);
  }

  bool MatchesNoRules() const {
    if (IsGeckoRuleNodeOrNull()) {
      return AsGeckoRuleNode()->IsRoot();
    } else {
      MOZ_CRASH("stylo: not implemented");
    }
  }

private:
  uintptr_t mBits;
};

// Higher-level struct that owns a strong reference to the source. The source
// is never null.
struct OwningStyleContextSource
{
  explicit OwningStyleContextSource(already_AddRefed<nsRuleNode> aRuleNode)
    : mRaw(aRuleNode.take()) { MOZ_ASSERT(!mRaw.IsNull()); };
  explicit OwningStyleContextSource(already_AddRefed<ServoComputedValues> aComputedValues)
    : mRaw(aComputedValues.take()) { MOZ_ASSERT(!mRaw.IsNull()); }
  OwningStyleContextSource(OwningStyleContextSource&& aOther)
    : mRaw(aOther.mRaw) { aOther.mRaw = nullptr; }

  OwningStyleContextSource& operator=(OwningStyleContextSource&) = delete;
  OwningStyleContextSource(OwningStyleContextSource&) = delete;

  ~OwningStyleContextSource() {
    if (mRaw.IsNull()) {
      // We must have invoked the move constructor.
    } else if (IsGeckoRuleNode()) {
      RefPtr<nsRuleNode> releaseme = dont_AddRef(AsGeckoRuleNode());
    } else {
      MOZ_ASSERT(IsServoComputedValues());
      RefPtr<ServoComputedValues> releaseme =
        dont_AddRef(AsServoComputedValues());
    }
  }

  bool operator==(const OwningStyleContextSource& aOther) const {
    return mRaw == aOther.mRaw;
  }
  bool operator!=(const OwningStyleContextSource& aOther) const {
    return !(*this == aOther);
  }
  bool IsNull() const { return mRaw.IsNull(); }
  bool IsGeckoRuleNode() const {
    MOZ_ASSERT(!mRaw.IsNull());
    return mRaw.IsGeckoRuleNodeOrNull();
  }
  bool IsServoComputedValues() const { return mRaw.IsServoComputedValues(); }

  NonOwningStyleContextSource AsRaw() const { return mRaw; }
  nsRuleNode* AsGeckoRuleNode() const { return mRaw.AsGeckoRuleNode(); }
  ServoComputedValues* AsServoComputedValues() const {
    return mRaw.AsServoComputedValues();
  }

  bool MatchesNoRules() const { return mRaw.MatchesNoRules(); }

private:
  NonOwningStyleContextSource mRaw;
};

} // namespace mozilla

#endif // mozilla_StyleContextSource_h
