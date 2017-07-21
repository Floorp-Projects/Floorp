/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoTypes_h
#define mozilla_ServoTypes_h

#include "mozilla/TypedEnumBits.h"

#define STYLE_STRUCT(name_, checkdata_cb_) struct nsStyle##name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

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

struct ServoWritingMode {
  uint8_t mBits;
};

// Don't attempt to read from this
// (see comment on ServoFontComputationData
enum ServoKeywordSize {
  Empty, // when the Option is None
  XXSmall,
  XSmall,
  Small,
  Medium,
  Large,
  XLarge,
  XXLarge,
  XXXLarge,
};

// Don't attempt to read from this. We can't
// always guarantee that the interior representation
// of this is correct (the mKeyword field may have a different padding),
// but the entire struct should
// have the same size and alignment as the Rust version.
// Ensure layout tests get run if touching either side.
struct ServoFontComputationData {
  ServoKeywordSize mKeyword;
  float/*32_t*/ mRatio;

  static_assert(sizeof(float) == 4, "float should be 32 bit");
};

struct ServoCustomPropertiesMap {
  uintptr_t mPtr;
};

struct ServoRuleNode {
  uintptr_t mPtr;
};


class ServoStyleContext;

struct ServoVisitedStyle {
  // This is actually a strong reference
  // but ServoComputedValues' destructor is
  // managed by the Rust code so we just use a
  // regular pointer
  ServoStyleContext* mPtr;
};

template <typename T>
struct ServoRawOffsetArc {
  // Again, a strong reference, but
  // managed by the Rust code
  T* mPtr;
};

struct ServoComputedValueFlags {
  uint8_t mFlags;
};

#define STYLE_STRUCT(name_, checkdata_cb_) struct Gecko##name_;
#define STYLE_STRUCT_LIST_IGNORE_VARIABLES
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
#undef STYLE_STRUCT_LIST_IGNORE_VARIABLES

} // namespace mozilla

class ServoComputedValues;

struct ServoComputedValuesForgotten
{
  // Make sure you manually mem::forget the backing ServoComputedValues
  // after calling this
  explicit ServoComputedValuesForgotten(const ServoComputedValues* aValue) : mPtr(aValue) {}
  const ServoComputedValues* mPtr;
};

/**
 * We want C++ to be able to read the style struct fields of ComputedValues
 * so we define this type on the C++ side and use the bindgenned version
 * on the Rust side.
 */
class ServoComputedValues
{
  friend class mozilla::ServoStyleContext;

public:
  // Constructs via memcpy.  Will not move out of aValue.
  explicit ServoComputedValues(const ServoComputedValuesForgotten aValue);

#define STYLE_STRUCT(name_, checkdata_cb_)                 \
  mozilla::ServoRawOffsetArc<mozilla::Gecko##name_> name_; \
  inline const nsStyle##name_* GetStyle##name_() const;
  #define STYLE_STRUCT_LIST_IGNORE_VARIABLES
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
#undef STYLE_STRUCT_LIST_IGNORE_VARIABLES
  const nsStyleVariables* GetStyleVariables() const;

private:
  mozilla::ServoCustomPropertiesMap custom_properties;
  mozilla::ServoWritingMode writing_mode;
  mozilla::ServoComputedValueFlags flags;
  /// The rule node representing the ordered list of rules matched for this
  /// node.  Can be None for default values and text nodes.  This is
  /// essentially an optimization to avoid referencing the root rule node.
  mozilla::ServoRuleNode rules;
  /// The element's computed values if visited, only computed if there's a
  /// relevant link for this element. A element's "relevant link" is the
  /// element being matched if it is a link or the nearest ancestor link.
  mozilla::ServoVisitedStyle visited_style;

  // this is the last member because most of the other members
  // are pointer sized. This makes it easier to deal with the
  // alignment of the fields when replacing things via bindgen
  //
  // This is opaque, please don't read from it from C++
  // (see comment on ServoFontComputationData)
  mozilla::ServoFontComputationData font_computation_data;

  // C++ just sees this struct as a bucket of bits, and will
  // do the wrong thing if we let it use the default copy ctor/assignment
  // operator. Remove them so that there is no footgun.
  //
  // We remove the move ctor/assignment operator as well, because
  // moves in C++ don't prevent destructors from being called,
  // which will lead to double frees.
  ServoComputedValues& operator=(const ServoComputedValues&) = delete;
  ServoComputedValues(const ServoComputedValues&) = delete;
  ServoComputedValues&& operator=(const ServoComputedValues&&) = delete;
  ServoComputedValues(const ServoComputedValues&&) = delete;
};

#endif // mozilla_ServoTypes_h
