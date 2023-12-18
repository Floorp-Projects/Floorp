/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Keyframe_h
#define mozilla_dom_Keyframe_h

#include "nsCSSValue.h"
#include "nsTArray.h"
#include "mozilla/dom/BaseKeyframeTypesBinding.h"  // CompositeOperationOrAuto
#include "mozilla/AnimatedPropertyID.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"

namespace mozilla {
struct StyleLockedDeclarationBlock;

/**
 * A property-value pair specified on a keyframe.
 */
struct PropertyValuePair {
  explicit PropertyValuePair(const AnimatedPropertyID& aProperty)
      : mProperty(aProperty) {}

  PropertyValuePair(const AnimatedPropertyID& aProperty,
                    RefPtr<StyleLockedDeclarationBlock>&& aValue)
      : mProperty(aProperty), mServoDeclarationBlock(std::move(aValue)) {
    MOZ_ASSERT(mServoDeclarationBlock, "Should be valid property value");
  }

  AnimatedPropertyID mProperty;

  // The specified value when using the Servo backend.
  RefPtr<StyleLockedDeclarationBlock> mServoDeclarationBlock;

#ifdef DEBUG
  // Flag to indicate that when we call StyleAnimationValue::ComputeValues on
  // this value we should behave as if that function had failed.
  bool mSimulateComputeValuesFailure = false;
#endif

  bool operator==(const PropertyValuePair&) const;
};

/**
 * A single keyframe.
 *
 * This is the canonical form in which keyframe effects are stored and
 * corresponds closely to the type of objects returned via the getKeyframes()
 * API.
 *
 * Before computing an output animation value, however, we flatten these frames
 * down to a series of per-property value arrays where we also resolve any
 * overlapping shorthands/longhands, convert specified CSS values to computed
 * values, etc.
 *
 * When the target element or computed style changes, however, we rebuild these
 * per-property arrays from the original list of keyframes objects. As a result,
 * these objects represent the master definition of the effect's values.
 */
struct Keyframe {
  Keyframe() = default;
  Keyframe(const Keyframe& aOther) = default;
  Keyframe(Keyframe&& aOther) = default;

  Keyframe& operator=(const Keyframe& aOther) = default;
  Keyframe& operator=(Keyframe&& aOther) = default;

  Maybe<double> mOffset;
  static constexpr double kComputedOffsetNotSet = -1.0;
  double mComputedOffset = kComputedOffsetNotSet;
  Maybe<StyleComputedTimingFunction> mTimingFunction;  // Nothing() here means
                                                       // "linear"
  dom::CompositeOperationOrAuto mComposite =
      dom::CompositeOperationOrAuto::Auto;
  CopyableTArray<PropertyValuePair> mPropertyValues;
};

}  // namespace mozilla

#endif  // mozilla_dom_Keyframe_h
