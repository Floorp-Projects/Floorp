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

struct ServoNodeData;
namespace mozilla {

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

// Indicates whether the Servo style system should perform normal processing or
// whether it should only process unstyled children of the root and their
// descendants.
enum class TraversalRootBehavior {
  Normal,
  UnstyledChildrenOnly,
};

// Indicates whether the Servo style system should perform normal processing,
// animation-only processing (so we can flush any throttled animation styles),
// or whether it should traverse in a mode that doesn't generate any change
// hints, which is what's required when handling frame reconstruction.
// The change hints in this case are unneeded, since the old frames have
// already been destroyed.
// Indicates how the Servo style system should perform.
enum class TraversalRestyleBehavior {
  // Normal processing.
  Normal,
  // Normal processing, but tolerant to calls to restyle elements in unstyled
  // or display:none subtrees (which can occur when styling elements with
  // newly applied XBL bindings).
  ForNewlyBoundElement,
  // Traverses in a mode that doesn't generate any change hints, which is what's
  // required when handling frame reconstruction.  The change hints in this case
  // are unneeded, since the old frames have already been destroyed.
  ForReconstruct,
  // Processes just the traversal for animation-only restyles and skips the
  // normal traversal for other restyles unrelated to animations.
  // This is used to bring throttled animations up-to-date such as when we need
  // to get correct position for transform animations that are throttled because
  // they are running on the compositor.
  ForThrottledAnimationFlush,
  // Traverses as normal mode but tries to update all CSS animations.
  ForCSSRuleChanges,
};

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
  CSSAnimations    = 1 << 0,
  CSSTransitions   = 1 << 1,
  EffectProperties = 1 << 2,
  CascadeResults   = 1 << 3,
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

} // namespace mozilla

#endif // mozilla_ServoTypes_h
