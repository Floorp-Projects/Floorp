/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleContextSource_h
#define mozilla_StyleContextSource_h

#include "mozilla/ServoBindingTypes.h"
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
  explicit NonOwningStyleContextSource(const ServoComputedValues* aComputedValues)
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

  const ServoComputedValues* AsServoComputedValues() const {
    MOZ_ASSERT(IsServoComputedValues());
    return reinterpret_cast<ServoComputedValues*>(mBits & ~1);
  }

private:
  uintptr_t mBits;
};

} // namespace mozilla

#endif // mozilla_StyleContextSource_h
