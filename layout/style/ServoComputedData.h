/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoComputedData_h
#define mozilla_ServoComputedData_h

#include "mozilla/ServoTypes.h"

/*
 * ServoComputedData and its related types.
 */

#define STYLE_STRUCT(name_) struct nsStyle##name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

namespace mozilla {

struct ServoWritingMode {
  uint8_t mBits;
};

struct ServoCustomPropertiesMap {
  uintptr_t mPtr;
};

struct ServoRuleNode {
  uintptr_t mPtr;
};

class ComputedStyle;

struct ServoVisitedStyle {
  // This is actually a strong reference
  // but ServoComputedData's destructor is
  // managed by the Rust code so we just use a
  // regular pointer
  ComputedStyle* mPtr;
};

struct ServoComputedValueFlags {
  uint16_t mFlags;
};

#define STYLE_STRUCT(name_) struct Gecko##name_;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

} // namespace mozilla

class ServoComputedData;

struct ServoComputedDataForgotten
{
  // Make sure you manually mem::forget the backing ServoComputedData
  // after calling this
  explicit ServoComputedDataForgotten(const ServoComputedData* aValue) : mPtr(aValue) {}
  const ServoComputedData* mPtr;
};

/**
 * We want C++ to be able to read the style struct fields of ComputedValues
 * so we define this type on the C++ side and use the bindgenned version
 * on the Rust side.
 */
class ServoComputedData
{
  friend class mozilla::ComputedStyle;

public:
  // Constructs via memcpy.  Will not move out of aValue.
  explicit ServoComputedData(const ServoComputedDataForgotten aValue);

#define STYLE_STRUCT(name_)                                \
  mozilla::ServoRawOffsetArc<mozilla::Gecko##name_> name_; \
  inline const nsStyle##name_* GetStyle##name_() const;
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  void AddSizeOfExcludingThis(nsWindowSizes& aSizes) const;

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

#endif // mozilla_ServoComputedData_h
