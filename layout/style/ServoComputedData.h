/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoComputedData_h
#define mozilla_ServoComputedData_h

class nsWindowSizes;

#include "mozilla/ServoStyleConsts.h"

/*
 * ServoComputedData and its related types.
 */

namespace mozilla {

struct ServoWritingMode {
  uint8_t mBits;
};

struct ServoComputedCustomProperties {
  uintptr_t mInherited;
  uintptr_t mNonInherited;
};

struct ServoRuleNode {
  uintptr_t mPtr;
};

class ComputedStyle;

}  // namespace mozilla

#define STYLE_STRUCT(name_) struct nsStyle##name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

class ServoComputedData;

struct ServoComputedDataForgotten {
  // Make sure you manually mem::forget the backing ServoComputedData
  // after calling this
  explicit ServoComputedDataForgotten(const ServoComputedData* aValue)
      : mPtr(aValue) {}
  const ServoComputedData* mPtr;
};

/**
 * We want C++ to be able to read the style struct fields of ComputedValues
 * so we define this type on the C++ side and use the bindgenned version
 * on the Rust side.
 */
class ServoComputedData {
  friend class mozilla::ComputedStyle;

 public:
  // Constructs via memcpy.  Will not move out of aValue.
  explicit ServoComputedData(const ServoComputedDataForgotten aValue);

#define STYLE_STRUCT(name_)                                       \
  const nsStyle##name_* name_;                                    \
  const nsStyle##name_* Style##name_() const MOZ_NONNULL_RETURN { \
    return name_;                                                 \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  void AddSizeOfExcludingThis(nsWindowSizes& aSizes) const;

  mozilla::ServoWritingMode WritingMode() const { return writing_mode; }

 private:
  mozilla::ServoComputedCustomProperties custom_properties;
  mozilla::ServoWritingMode writing_mode;
  /// The effective zoom (as in, the CSS zoom property) of this style.
  ///
  /// zoom is a non-inherited property, yet changes to it propagate through in
  /// an inherited fashion, and all length resolution code need to access it.
  /// This could, in theory, be stored in any other inherited struct, but it's
  /// weird to have an inherited struct field depend on a non inherited
  /// property.
  ///
  /// So the style object itself is probably a reasonable place to store it.
  mozilla::StyleZoom effective_zoom;
  mozilla::StyleComputedValueFlags flags;
  /// The rule node representing the ordered list of rules matched for this
  /// node.  Can be None for default values and text nodes.  This is
  /// essentially an optimization to avoid referencing the root rule node.
  mozilla::ServoRuleNode rules;
  /// The element's computed values if visited, only computed if there's a
  /// relevant link for this element. A element's "relevant link" is the
  /// element being matched if it is a link or the nearest ancestor link.
  const mozilla::ComputedStyle* visited_style;

  // C++ just sees this struct as a bucket of bits, and will
  // do the wrong thing if we let it use the default copy ctor/assignment
  // operator. Remove them so that there is no footgun.
  //
  // We remove the move ctor/assignment operator as well, because
  // moves in C++ don't prevent destructors from being called,
  // which will lead to double frees.
  ServoComputedData& operator=(const ServoComputedData&) = delete;
  ServoComputedData(const ServoComputedData&) = delete;
  ServoComputedData&& operator=(const ServoComputedData&&) = delete;
  ServoComputedData(const ServoComputedData&&) = delete;
};

#endif  // mozilla_ServoComputedData_h
