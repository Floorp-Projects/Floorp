/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "SVGLengthListSMILType.h"
#include "nsSMILValue.h"
#include "SVGLengthList.h"
#include <math.h>

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
  lengthList->SetCanZeroPadList(PR_TRUE);

  aValue.mU.mPtr = lengthList;
  aValue.mType = this;
}

void
SVGLengthListSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value type");
  delete static_cast<SVGLengthListAndInfo*>(aValue.mU.mPtr);
  aValue.mU.mPtr = nsnull;
  aValue.mType = &nsSMILNullType::sSingleton;
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

PRBool
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
                           PRUint32 aCount) const
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

  if (dest.Length() < valueToAdd.Length()) {
    if (!dest.CanZeroPadList()) {
      // nsSVGUtils::ReportToConsole
      return NS_ERROR_FAILURE;
    }

    NS_ASSERTION(valueToAdd.CanZeroPadList() || dest.Length() == 0,
                 "Only \"zero\" nsSMILValues from the SMIL engine should "
                 "return PR_TRUE for CanZeroPadList() when the attribute "
                 "being animated can't be zero padded");

    PRUint32 i = dest.Length();
    if (!dest.SetLength(valueToAdd.Length())) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (; i < valueToAdd.Length(); ++i) {
      dest[i].SetValueAndUnit(0.0f, valueToAdd[i].GetUnit());
    }
  }

  for (PRUint32 i = 0; i < valueToAdd.Length(); ++i) {
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
    dest[i].SetValueAndUnit(dest[i].GetValueInCurrentUnits() + valToAdd,
                            dest[i].GetUnit());
  }

  // propagate flag:
  dest.SetCanZeroPadList(dest.CanZeroPadList() &&
                         valueToAdd.CanZeroPadList());

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
               (from.CanZeroPadList() && from.Length() == 0) ||
               (to.CanZeroPadList() && to.Length() == 0),
               "Only \"zero\" nsSMILValues from the SMIL engine should "
               "return PR_TRUE for CanZeroPadList() when the attribute "
               "being animated can't be zero padded");

  if ((from.Length() < to.Length() && !from.CanZeroPadList()) ||
      (to.Length() < from.Length() && !to.CanZeroPadList())) {
    // nsSVGUtils::ReportToConsole
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

  PRUint32 i = 0;
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
  if (!NS_FloatIsFinite(distance)) {
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
               (start.CanZeroPadList() && start.Length() == 0) ||
               (end.CanZeroPadList() && end.Length() == 0),
               "Only \"zero\" nsSMILValues from the SMIL engine should "
               "return PR_TRUE for CanZeroPadList() when the attribute "
               "being animated can't be zero padded");

  if ((start.Length() < end.Length() && !start.CanZeroPadList()) ||
      (end.Length() < start.Length() && !end.CanZeroPadList())) {
    // nsSVGUtils::ReportToConsole
    return NS_ERROR_FAILURE;
  }

  if (!result.SetLength(NS_MAX(start.Length(), end.Length()))) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  PRUint32 i = 0;
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

  // propagate flag:
  result.SetCanZeroPadList(start.CanZeroPadList() &&
                           end.CanZeroPadList());

  return NS_OK;
}

} // namespace mozilla
