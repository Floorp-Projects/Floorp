/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoTypes_h
#define mozilla_ServoTypes_h

#include "mozilla/TypedEnumBits.h"

/*
 * Type definitions used to interact with Servo. This gets included by nsINode,
 * so don't add significant include dependencies to this file.
 */

class nsWindowSizes;
struct ServoNodeData;
namespace mozilla {

class SizeOfState;

/*
 * Replaced types. These get mapped to associated Servo types in bindgen.
 */

template<typename T>
struct ServoUnsafeCell {
  T value;

  // Ensure that primitive types (i.e. pointers) get zero-initialized.
  ServoUnsafeCell() : value() {};
};

template<typename T>
struct ServoCell {
  ServoUnsafeCell<T> value;
  T Get() const { return value.value; }
  void Set(T arg) { value.value = arg; }
  ServoCell() : value() {};
};

// Indicates whether the Servo style system should expect the style on an element
// to have already been resolved (i.e. via a parallel traversal), or whether it
// may be lazily computed.
enum class LazyComputeBehavior {
  Allow,
  Assert,
};

// Various flags for the servo traversal.
enum class ServoTraversalFlags : uint32_t {
  Empty = 0,
  // Perform animation processing but not regular styling.
  AnimationOnly = 1 << 0,
  // Traverses as normal mode but tries to update all CSS animations.
  ForCSSRuleChanges = 1 << 1,
  // A forgetful traversal ignores the previous state of the frame tree, and
  // thus does not compute damage or maintain other state describing the styles
  // pre-traversal. A forgetful traversal is usually the right thing if you
  // aren't going to do a post-traversal.
  Forgetful = 1 << 3,
  // Clears all the dirty bits (dirty descendants, animation-only dirty-descendants,
  // needs frame, descendants need frames) on the elements traversed.
  // in the subtree.
  ClearDirtyBits = 1 << 5,
  // Clears only the animation-only dirty descendants bit in the subtree.
  ClearAnimationOnlyDirtyDescendants = 1 << 6,
  // Allows the traversal to run in parallel if there are sufficient cores on
  // the machine.
  ParallelTraversal = 1 << 7,
  // Flush throttled animations. By default, we only update throttled animations
  // when we have other non-throttled work to do. With this flag, we
  // unconditionally tick and process them.
  FlushThrottledAnimations = 1 << 8,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ServoTraversalFlags)

// Indicates which rules should be included when performing selecting matching
// on an element.  DefaultOnly is used to exclude all rules except for those
// that come from UA style sheets, and is used to implemented
// getDefaultComputedStyle.
enum class StyleRuleInclusion {
  All,
  DefaultOnly,
};

// Represents which tasks are performed in a SequentialTask of UpdateAnimations.
enum class UpdateAnimationsTasks : uint8_t {
  CSSAnimations          = 1 << 0,
  CSSTransitions         = 1 << 1,
  EffectProperties       = 1 << 2,
  CascadeResults         = 1 << 3,
  DisplayChangedFromNone = 1 << 4,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(UpdateAnimationsTasks)

// The mode to use when parsing values.
enum class ParsingMode : uint8_t {
  // In CSS, lengths must have units, except for zero values, where the unit can
  // be omitted.
  // https://www.w3.org/TR/css3-values/#lengths
  Default = 0,
  // In SVG, a coordinate or length value without a unit identifier (e.g., "25")
  // is assumed to be in user units (px).
  // https://www.w3.org/TR/SVG/coords.html#Units
  AllowUnitlessLength = 1 << 0,
  // In SVG, out-of-range values are not treated as an error in parsing.
  // https://www.w3.org/TR/SVG/implnote.html#RangeClamping
  AllowAllNumericValues = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ParsingMode)

// The kind of style we're generating when requesting Servo to give us an
// inherited style.
enum class InheritTarget {
  // We're requesting a text style.
  Text,
  // We're requesting a first-letter continuation frame style.
  FirstLetterContinuation,
  // We're requesting a style for a placeholder frame.
  PlaceholderFrame,
};

// These measurements are obtained for both the UA cache and the Stylist, but
// not all the fields are used in both cases.
class ServoStyleSetSizes
{
public:
  size_t mRuleTree;                // Stylist-only
  size_t mPrecomputedPseudos;      // UA cache-only
  size_t mElementAndPseudosMaps;   // Used for both
  size_t mInvalidationMap;         // Used for both
  size_t mRevalidationSelectors;   // Used for both
  size_t mOther;                   // Used for both

  ServoStyleSetSizes()
    : mRuleTree(0)
    , mPrecomputedPseudos(0)
    , mElementAndPseudosMaps(0)
    , mInvalidationMap(0)
    , mRevalidationSelectors(0)
    , mOther(0)
  {}
};

template <typename T>
struct ServoRawOffsetArc {
  // Again, a strong reference, but
  // managed by the Rust code
  T* mPtr;
};

} // namespace mozilla

#endif // mozilla_ServoTypes_h
