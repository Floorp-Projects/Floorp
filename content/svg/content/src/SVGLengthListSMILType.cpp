/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SVGLengthListSMILType.h"
#include "nsSMILValue.h"
#include "SVGLengthList.h"
#include "nsMathUtils.h"
#include <math.h>
#include <algorithm>

namespace mozilla {

/*static*/ SVGLengthListSMILType SVGLengthListSMILType::sSingleton;

//----------------------------------------------------------------------
// nsISMILType implementation

void
SVGLengthListSMILType::Init(nsSMILValue &aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected value type");

  SVGLengthListAndInfo* lengthList = new SVGLengthListAndInfo();

  // See the comment documenting Init() in our header file:
  lengthList->SetCanZeroPadList(true);

  aValue.mU.mPtr = lengthList;
  aValue.mType = this;
}

void
SVGLengthListSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<SVGLengthListAndInfo*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nullptr;
  aValue.mType = nsSMILNullType::Singleton();
}

nsresult
SVGLengthListSMILType::Assign(nsSMILValue& aDest,
                              const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value");

  const SVGLengthListAndInfo* src =
    static_cast<const SVGLengthListAndInfo*>(aSrc.mU.mPtr);
  SVGLengthListAndInfo* dest =
    static_cast<SVGLengthListAndInfo*>(aDest.mU.mPtr);

  return dest->CopyFrom(*src);
}

bool
SVGLengthListSMILType::IsEqual(const nsSMILValue& aLeft,
                               const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return *static_cast<const SVGLengthListAndInfo*>(aLeft.mU.mPtr) ==
         *static_cast<const SVGLengthListAndInfo*>(aRight.mU.mPtr);
}

nsresult
SVGLengthListSMILType::Add(nsSMILValue& aDest,
                           const nsSMILValue& aValueToAdd,
                           uint32_t aCount) const
{
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL type");
  NS_PRECONDITION(aValueToAdd.mType == this, "Incompatible SMIL type");

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

  if (dest.IsEmpty() && valueToAdd.IsEmpty()) {
    // Adding two identity values, no-op.  This occurs when performing a
    // discrete by-animation on an attribute with no specified base value.
    return NS_OK;
  }

  if (!valueToAdd.Element()) { // Adding identity value - no-op
    NS_ABORT_IF_FALSE(valueToAdd.IsEmpty(),
                      "Identity values should be empty");
    return NS_OK;
  }

  if (!dest.Element()) { // Adding *to* an identity value
    NS_ABORT_IF_FALSE(dest.IsEmpty(),
                      "Identity values should be empty");
    if (!dest.SetLength(valueToAdd.Length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (uint32_t i = 0; i < dest.Length(); ++i) {
      dest[i].SetValueAndUnit(valueToAdd[i].GetValueInCurrentUnits() * aCount,
                              valueToAdd[i].GetUnit());
    }
    dest.SetInfo(valueToAdd.Element(), valueToAdd.Axis(),
                 valueToAdd.CanZeroPadList()); // propagate target element info!
    return NS_OK;
  }
  NS_ABORT_IF_FALSE(dest.Element() == valueToAdd.Element(),
                    "adding values from different elements...?");

  // Zero-pad our |dest| list, if necessary.
  if (dest.Length() < valueToAdd.Length()) {
    if (!dest.CanZeroPadList()) {
      // SVGContentUtils::ReportToConsole
      return NS_ERROR_FAILURE;
    }

    NS_ABORT_IF_FALSE(valueToAdd.CanZeroPadList(),
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
      valToAdd = valueToAdd[i].GetValueInSpecifiedUnit(dest[i].GetUnit(),
                                                       dest.Element(),
                                                       dest.Axis());
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

nsresult
SVGLengthListSMILType::ComputeDistance(const nsSMILValue& aFrom,
                                       const nsSMILValue& aTo,
                                       double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == this, "Unexpected SMIL type");
  NS_PRECONDITION(aTo.mType == this, "Incompatible SMIL type");

  const SVGLengthListAndInfo& from =
    *static_cast<const SVGLengthListAndInfo*>(aFrom.mU.mPtr);
  const SVGLengthListAndInfo& to =
    *static_cast<const SVGLengthListAndInfo*>(aTo.mU.mPtr);

  // To understand this code, see the comments documenting our Init() method,
  // and documenting SVGLengthListAndInfo::CanZeroPadList().

  NS_ASSERTION((from.CanZeroPadList() == to.CanZeroPadList()) ||
               (from.CanZeroPadList() && from.IsEmpty()) ||
               (to.CanZeroPadList() && to.IsEmpty()),
               "Only \"zero\" nsSMILValues from the SMIL engine should "
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
    double f = from[i].GetValueInUserUnits(from.Element(), from.Axis());
    double t = to[i].GetValueInUserUnits(to.Element(), to.Axis());
    double delta = t - f;
    total += delta * delta;
  }

  // In the case that from.Length() != to.Length(), one of the following loops
  // will run. (OK since CanZeroPadList()==true for the other list.)

  for (; i < from.Length(); ++i) {
    double f = from[i].GetValueInUserUnits(from.Element(), from.Axis());
    total += f * f;
  }
  for (; i < to.Length(); ++i) {
    double t = to[i].GetValueInUserUnits(to.Element(), to.Axis());
    total += t * t;
  }

  float distance = sqrt(total);
  if (!NS_finite(distance)) {
    return NS_ERROR_FAILURE;
  }
  aDistance = distance;
  return NS_OK;
}

nsresult
SVGLengthListSMILType::Interpolate(const nsSMILValue& aStartVal,
                                   const nsSMILValue& aEndVal,
                                   double aUnitDistance,
                                   nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation");
  NS_PRECONDITION(aResult.mType == this, "Unexpected result type");

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
               "Only \"zero\" nsSMILValues from the SMIL engine should "
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

  uint32_t i = 0;
  for (; i < start.Length() && i < end.Length(); ++i) {
    float s;
    if (start[i].GetUnit() == end[i].GetUnit()) {
      s = start[i].GetValueInCurrentUnits();
    } else {
      // If units differ, we use the unit of the item in 'end'.
      // We leave it to the frame code to check that values are finite.
      s = start[i].GetValueInSpecifiedUnit(end[i].GetUnit(), end.Element(), end.Axis());
    }
    float e = end[i].GetValueInCurrentUnits();
    result[i].SetValueAndUnit(s + (e - s) * aUnitDistance, end[i].GetUnit());
  }

  // In the case that start.Length() != end.Length(), one of the following
  // loops will run. (Okay, since CanZeroPadList()==true for the other list.)

  for (; i < start.Length(); ++i) {
    result[i].SetValueAndUnit(start[i].GetValueInCurrentUnits() -
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

} // namespace mozilla
