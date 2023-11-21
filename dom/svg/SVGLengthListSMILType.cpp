/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGLengthListSMILType.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/SMILValue.h"
#include "nsMathUtils.h"
#include "SVGLengthList.h"
#include <math.h>
#include <algorithm>

namespace mozilla {

/*static*/
SVGLengthListSMILType SVGLengthListSMILType::sSingleton;

//----------------------------------------------------------------------
// nsISMILType implementation

void SVGLengthListSMILType::Init(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mPtr = new SVGLengthListAndInfo();
  aValue.mType = this;
}

void SVGLengthListSMILType::Destroy(SMILValue& aValue) const {
  MOZ_ASSERT(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<SVGLengthListAndInfo*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nullptr;
  aValue.mType = SMILNullType::Singleton();
}

nsresult SVGLengthListSMILType::Assign(SMILValue& aDest,
                                       const SMILValue& aSrc) const {
  MOZ_ASSERT(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL value");

  const SVGLengthListAndInfo* src =
      static_cast<const SVGLengthListAndInfo*>(aSrc.mU.mPtr);
  SVGLengthListAndInfo* dest =
      static_cast<SVGLengthListAndInfo*>(aDest.mU.mPtr);

  return dest->CopyFrom(*src);
}

bool SVGLengthListSMILType::IsEqual(const SMILValue& aLeft,
                                    const SMILValue& aRight) const {
  MOZ_ASSERT(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  MOZ_ASSERT(aLeft.mType == this, "Unexpected type for SMIL value");

  return *static_cast<const SVGLengthListAndInfo*>(aLeft.mU.mPtr) ==
         *static_cast<const SVGLengthListAndInfo*>(aRight.mU.mPtr);
}

nsresult SVGLengthListSMILType::Add(SMILValue& aDest,
                                    const SMILValue& aValueToAdd,
                                    uint32_t aCount) const {
  MOZ_ASSERT(aDest.mType == this, "Unexpected SMIL type");
  MOZ_ASSERT(aValueToAdd.mType == this, "Incompatible SMIL type");

  SVGLengthListAndInfo& dest =
      *static_cast<SVGLengthListAndInfo*>(aDest.mU.mPtr);
  const SVGLengthListAndInfo& valueToAdd =
      *static_cast<const SVGLengthListAndInfo*>(aValueToAdd.mU.mPtr);

  // To understand this code, see the comments documenting our Init() method,
  // and documenting SVGLengthListAndInfo::CanZeroPadList().

  // Note that *this* method actually may safely zero pad a shorter list
  // regardless of the value returned by CanZeroPadList() for that list,
  // just so long as the shorter list is being added *to* the longer list
  // and *not* vice versa! It's okay in the case of adding a shorter list to a
  // longer list because during the add operation we'll end up adding the
  // zeros to actual specified values. It's *not* okay in the case of adding a
  // longer list to a shorter list because then we end up adding to implicit
  // zeros when we'd actually need to add to whatever the underlying values
  // should be, not zeros, and those values are not explicit or otherwise
  // available.

  if (valueToAdd.IsIdentity()) {  // Adding identity value - no-op
    return NS_OK;
  }

  if (dest.IsIdentity()) {  // Adding *to* an identity value
    if (!dest.SetLength(valueToAdd.Length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < dest.Length(); ++i) {
      dest[i].SetValueAndUnit(valueToAdd[i].GetValueInCurrentUnits() * aCount,
                              valueToAdd[i].GetUnit());
    }
    dest.SetInfo(
        valueToAdd.Element(), valueToAdd.Axis(),
        valueToAdd.CanZeroPadList());  // propagate target element info!
    return NS_OK;
  }
  MOZ_ASSERT(dest.Element() == valueToAdd.Element(),
             "adding values from different elements...?");

  // Zero-pad our |dest| list, if necessary.
  if (dest.Length() < valueToAdd.Length()) {
    if (!dest.CanZeroPadList()) {
      // SVGContentUtils::ReportToConsole
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(valueToAdd.CanZeroPadList(),
               "values disagree about attribute's zero-paddibility");

    uint32_t i = dest.Length();
    if (!dest.SetLength(valueToAdd.Length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (; i < valueToAdd.Length(); ++i) {
      dest[i].SetValueAndUnit(0.0f, valueToAdd[i].GetUnit());
    }
  }

  for (uint32_t i = 0; i < valueToAdd.Length(); ++i) {
    float valToAdd;
    if (dest[i].GetUnit() == valueToAdd[i].GetUnit()) {
      valToAdd = valueToAdd[i].GetValueInCurrentUnits();
    } else {
      // If units differ, we use the unit of the item in 'dest'.
      // We leave it to the frame code to check that values are finite.
      valToAdd = valueToAdd[i].GetValueInSpecifiedUnit(
          dest[i].GetUnit(), dest.Element(), dest.Axis());
    }
    dest[i].SetValueAndUnit(
        dest[i].GetValueInCurrentUnits() + valToAdd * aCount,
        dest[i].GetUnit());
  }

  // propagate target element info!
  dest.SetInfo(valueToAdd.Element(), valueToAdd.Axis(),
               dest.CanZeroPadList() && valueToAdd.CanZeroPadList());

  return NS_OK;
}

nsresult SVGLengthListSMILType::ComputeDistance(const SMILValue& aFrom,
                                                const SMILValue& aTo,
                                                double& aDistance) const {
  MOZ_ASSERT(aFrom.mType == this, "Unexpected SMIL type");
  MOZ_ASSERT(aTo.mType == this, "Incompatible SMIL type");

  const SVGLengthListAndInfo& from =
      *static_cast<const SVGLengthListAndInfo*>(aFrom.mU.mPtr);
  const SVGLengthListAndInfo& to =
      *static_cast<const SVGLengthListAndInfo*>(aTo.mU.mPtr);

  // To understand this code, see the comments documenting our Init() method,
  // and documenting SVGLengthListAndInfo::CanZeroPadList().

  NS_ASSERTION((from.CanZeroPadList() == to.CanZeroPadList()) ||
                   (from.CanZeroPadList() && from.IsEmpty()) ||
                   (to.CanZeroPadList() && to.IsEmpty()),
               "Only \"zero\" SMILValues from the SMIL engine should "
               "return true for CanZeroPadList() when the attribute "
               "being animated can't be zero padded");

  if ((from.Length() < to.Length() && !from.CanZeroPadList()) ||
      (to.Length() < from.Length() && !to.CanZeroPadList())) {
    // SVGContentUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }

  // We return the root of the sum of the squares of the deltas between the
  // user unit values of the lengths at each correspanding index. In the
  // general case, paced animation is probably not useful, but this strategy at
  // least does the right thing for paced animation in the face of simple
  // 'values' lists such as:
  //
  //   values="100 200 300; 101 201 301; 110 210 310"
  //
  // I.e. half way through the simple duration we'll get "105 205 305".

  double total = 0.0;

  uint32_t i = 0;
  for (; i < from.Length() && i < to.Length(); ++i) {
    double f = from[i].GetValueInPixels(from.Element(), from.Axis());
    double t = to[i].GetValueInPixels(to.Element(), to.Axis());
    double delta = t - f;
    total += delta * delta;
  }

  // In the case that from.Length() != to.Length(), one of the following loops
  // will run. (OK since CanZeroPadList()==true for the other list.)

  for (; i < from.Length(); ++i) {
    double f = from[i].GetValueInPixels(from.Element(), from.Axis());
    total += f * f;
  }
  for (; i < to.Length(); ++i) {
    double t = to[i].GetValueInPixels(to.Element(), to.Axis());
    total += t * t;
  }

  float distance = sqrt(total);
  if (!std::isfinite(distance)) {
    return NS_ERROR_FAILURE;
  }
  aDistance = distance;
  return NS_OK;
}

nsresult SVGLengthListSMILType::Interpolate(const SMILValue& aStartVal,
                                            const SMILValue& aEndVal,
                                            double aUnitDistance,
                                            SMILValue& aResult) const {
  MOZ_ASSERT(aStartVal.mType == aEndVal.mType,
             "Trying to interpolate different types");
  MOZ_ASSERT(aStartVal.mType == this, "Unexpected types for interpolation");
  MOZ_ASSERT(aResult.mType == this, "Unexpected result type");

  const SVGLengthListAndInfo& start =
      *static_cast<const SVGLengthListAndInfo*>(aStartVal.mU.mPtr);
  const SVGLengthListAndInfo& end =
      *static_cast<const SVGLengthListAndInfo*>(aEndVal.mU.mPtr);
  SVGLengthListAndInfo& result =
      *static_cast<SVGLengthListAndInfo*>(aResult.mU.mPtr);

  // To understand this code, see the comments documenting our Init() method,
  // and documenting SVGLengthListAndInfo::CanZeroPadList().

  NS_ASSERTION((start.CanZeroPadList() == end.CanZeroPadList()) ||
                   (start.CanZeroPadList() && start.IsEmpty()) ||
                   (end.CanZeroPadList() && end.IsEmpty()),
               "Only \"zero\" SMILValues from the SMIL engine should "
               "return true for CanZeroPadList() when the attribute "
               "being animated can't be zero padded");

  if ((start.Length() < end.Length() && !start.CanZeroPadList()) ||
      (end.Length() < start.Length() && !end.CanZeroPadList())) {
    // SVGContentUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }

  if (!result.SetLength(std::max(start.Length(), end.Length()))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // If units differ, we use the unit of the nearest item.
  // We leave it to the frame code to check that values are finite.
  bool useEndUnits = (aUnitDistance > 0.5);

  uint32_t i = 0;
  for (; i < start.Length() && i < end.Length(); ++i) {
    float s, e;
    uint8_t unit;
    if (start[i].GetUnit() == end[i].GetUnit()) {
      unit = start[i].GetUnit();
      s = start[i].GetValueInCurrentUnits();
      e = end[i].GetValueInCurrentUnits();
    } else if (useEndUnits) {
      unit = end[i].GetUnit();
      s = start[i].GetValueInSpecifiedUnit(unit, end.Element(), end.Axis());
      e = end[i].GetValueInCurrentUnits();
    } else {
      unit = start[i].GetUnit();
      s = start[i].GetValueInCurrentUnits();
      e = end[i].GetValueInSpecifiedUnit(unit, start.Element(), start.Axis());
    }
    result[i].SetValueAndUnit(s + (e - s) * aUnitDistance, unit);
  }

  // In the case that start.Length() != end.Length(), one of the following
  // loops will run. (Okay, since CanZeroPadList()==true for the other list.)

  for (; i < start.Length(); ++i) {
    result[i].SetValueAndUnit(
        start[i].GetValueInCurrentUnits() -
            start[i].GetValueInCurrentUnits() * aUnitDistance,
        start[i].GetUnit());
  }
  for (; i < end.Length(); ++i) {
    result[i].SetValueAndUnit(end[i].GetValueInCurrentUnits() * aUnitDistance,
                              end[i].GetUnit());
  }

  // propagate target element info!
  result.SetInfo(end.Element(), end.Axis(),
                 start.CanZeroPadList() && end.CanZeroPadList());

  return NS_OK;
}

}  // namespace mozilla
