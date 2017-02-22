/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Inlined methods for GenericSpecifiedValues. Will just redirect to
 * nsRuleData methods when compiled without stylo, but will do
 * virtual dispatch (by checking which kind of container it is)
 * in stylo mode.
 */

#ifndef mozilla_GenericSpecifiedValuesInlines_h
#define mozilla_GenericSpecifiedValuesInlines_h

#include "nsRuleData.h"
#include "mozilla/GenericSpecifiedValues.h"
#include "mozilla/ServoSpecifiedValues.h"

namespace mozilla {

MOZ_DEFINE_STYLO_METHODS(GenericSpecifiedValues, nsRuleData, ServoSpecifiedValues)

bool
GenericSpecifiedValues::PropertyIsSet(nsCSSPropertyID aId)
{
  MOZ_STYLO_FORWARD(PropertyIsSet, (aId))
}

void
GenericSpecifiedValues::SetIdentStringValue(nsCSSPropertyID aId, const nsString& aValue)
{
  MOZ_STYLO_FORWARD(SetIdentStringValue, (aId, aValue))
}

void
GenericSpecifiedValues::SetIdentStringValueIfUnset(nsCSSPropertyID aId, const nsString& aValue)
{
  MOZ_STYLO_FORWARD(SetIdentStringValueIfUnset, (aId, aValue))
}

void
GenericSpecifiedValues::SetKeywordValue(nsCSSPropertyID aId, int32_t aValue)
{
  // there are some static asserts in MOZ_STYLO_FORWARD which
  // won't work with the overloaded SetKeywordValue function,
  // so we copy its expansion and use SetIntValue for decltype
  // instead
  static_assert(!mozilla::IsSame<decltype(&MOZ_STYLO_THIS_TYPE::SetIntValue),
                 decltype(&MOZ_STYLO_GECKO_TYPE::SetKeywordValue)>
        ::value, "Gecko subclass should define its own SetKeywordValue");
  static_assert(!mozilla::IsSame<decltype(&MOZ_STYLO_THIS_TYPE::SetIntValue),
                 decltype(&MOZ_STYLO_SERVO_TYPE::SetKeywordValue)>
        ::value, "Servo subclass should define its own SetKeywordValue");

  if (IsServo()) {
    return AsServo()->SetKeywordValue(aId, aValue);
  }
  return AsGecko()->SetKeywordValue(aId, aValue);
}

void
GenericSpecifiedValues::SetKeywordValueIfUnset(nsCSSPropertyID aId, int32_t aValue)
{
  // there are some static asserts in MOZ_STYLO_FORWARD which
  // won't work with the overloaded SetKeywordValue function,
  // so we copy its expansion and use SetIntValue for decltype
  // instead
  static_assert(!mozilla::IsSame<decltype(&MOZ_STYLO_THIS_TYPE::SetIntValue),
                 decltype(&MOZ_STYLO_GECKO_TYPE::SetKeywordValueIfUnset)>
        ::value, "Gecko subclass should define its own SetKeywordValueIfUnset");
  static_assert(!mozilla::IsSame<decltype(&MOZ_STYLO_THIS_TYPE::SetIntValue),
                 decltype(&MOZ_STYLO_SERVO_TYPE::SetKeywordValueIfUnset)>
        ::value, "Servo subclass should define its own SetKeywordValueIfUnset");

  if (IsServo()) {
    return AsServo()->SetKeywordValueIfUnset(aId, aValue);
  }
  return AsGecko()->SetKeywordValueIfUnset(aId, aValue);
}

void
GenericSpecifiedValues::SetIntValue(nsCSSPropertyID aId, int32_t aValue)
{
  MOZ_STYLO_FORWARD(SetIntValue, (aId, aValue))
}

void
GenericSpecifiedValues::SetPixelValue(nsCSSPropertyID aId, float aValue)
{
  MOZ_STYLO_FORWARD(SetPixelValue, (aId, aValue))
}

void
GenericSpecifiedValues::SetPixelValueIfUnset(nsCSSPropertyID aId, float aValue)
{
  MOZ_STYLO_FORWARD(SetPixelValueIfUnset, (aId, aValue))
}

void
GenericSpecifiedValues::SetPercentValue(nsCSSPropertyID aId, float aValue)
{
  MOZ_STYLO_FORWARD(SetPercentValue, (aId, aValue))
}

void
GenericSpecifiedValues::SetPercentValueIfUnset(nsCSSPropertyID aId, float aValue)
{
  MOZ_STYLO_FORWARD(SetPercentValueIfUnset, (aId, aValue))
}

void
GenericSpecifiedValues::SetAutoValue(nsCSSPropertyID aId)
{
  MOZ_STYLO_FORWARD(SetAutoValue, (aId))
}

void
GenericSpecifiedValues::SetAutoValueIfUnset(nsCSSPropertyID aId)
{
  MOZ_STYLO_FORWARD(SetAutoValueIfUnset, (aId))
}

void
GenericSpecifiedValues::SetCurrentColor(nsCSSPropertyID aId)
{
  MOZ_STYLO_FORWARD(SetCurrentColor, (aId))
}

void
GenericSpecifiedValues::SetCurrentColorIfUnset(nsCSSPropertyID aId)
{
  MOZ_STYLO_FORWARD(SetCurrentColorIfUnset, (aId))
}

void
GenericSpecifiedValues::SetColorValue(nsCSSPropertyID aId, nscolor aValue)
{
  MOZ_STYLO_FORWARD(SetColorValue, (aId, aValue))
}

void
GenericSpecifiedValues::SetColorValueIfUnset(nsCSSPropertyID aId, nscolor aValue)
{
  MOZ_STYLO_FORWARD(SetColorValueIfUnset, (aId, aValue))
}

void
GenericSpecifiedValues::SetFontFamily(const nsString& aValue)
{
  MOZ_STYLO_FORWARD(SetFontFamily, (aValue))
}

void
GenericSpecifiedValues::SetTextDecorationColorOverride()
{
  MOZ_STYLO_FORWARD(SetTextDecorationColorOverride, ())
}

} // namespace mozilla

#endif // mozilla_GenericSpecifiedValuesInlines_h
