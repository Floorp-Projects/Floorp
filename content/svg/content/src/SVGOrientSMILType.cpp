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
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

#include "SVGOrientSMILType.h"
#include "nsSMILValue.h"
#include "nsSVGViewBox.h"
#include "nsSVGAngle.h"
#include "nsIDOMSVGMarkerElement.h"
#include "nsDebug.h"
#include <math.h>

namespace mozilla {

/*static*/ SVGOrientSMILType SVGOrientSMILType::sSingleton;

void
SVGOrientSMILType::Init(nsSMILValue& aValue) const
{
  NS_ABORT_IF_FALSE(aValue.IsNull(), "Unexpected value type");

  aValue.mU.mOrient.mAngle = 0.0f;
  aValue.mU.mOrient.mUnit = nsIDOMSVGAngle::SVG_ANGLETYPE_UNSPECIFIED;
  aValue.mU.mOrient.mOrientType = nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE;
  aValue.mType = this;
}

void
SVGOrientSMILType::Destroy(nsSMILValue& aValue) const
{
  NS_PRECONDITION(aValue.mType == this, "Unexpected SMIL value.");
  aValue.mU.mPtr = nsnull;
  aValue.mType = &nsSMILNullType::sSingleton;
}

nsresult
SVGOrientSMILType::Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const
{
  NS_PRECONDITION(aDest.mType == aSrc.mType, "Incompatible SMIL types.");
  NS_PRECONDITION(aDest.mType == this, "Unexpected SMIL value.");

  aDest.mU.mOrient.mAngle = aSrc.mU.mOrient.mAngle;
  aDest.mU.mOrient.mUnit = aSrc.mU.mOrient.mUnit;
  aDest.mU.mOrient.mOrientType = aSrc.mU.mOrient.mOrientType;
  return NS_OK;
}

bool
SVGOrientSMILType::IsEqual(const nsSMILValue& aLeft,
                           const nsSMILValue& aRight) const
{
  NS_PRECONDITION(aLeft.mType == aRight.mType, "Incompatible SMIL types");
  NS_PRECONDITION(aLeft.mType == this, "Unexpected type for SMIL value");

  return
    aLeft.mU.mOrient.mAngle == aRight.mU.mOrient.mAngle &&
    aLeft.mU.mOrient.mUnit == aRight.mU.mOrient.mUnit &&
    aLeft.mU.mOrient.mOrientType == aRight.mU.mOrient.mOrientType;
}

nsresult
SVGOrientSMILType::Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                       PRUint32 aCount) const
{
  NS_PRECONDITION(aValueToAdd.mType == aDest.mType,
                  "Trying to add invalid types");
  NS_PRECONDITION(aValueToAdd.mType == this, "Unexpected source type");

  if (aDest.mU.mOrient.mOrientType != nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE ||
      aValueToAdd.mU.mOrient.mOrientType != nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE) {
    // TODO: it would be nice to be able to add to auto angles
    return NS_ERROR_FAILURE;
  }

  // We may be dealing with two different angle units, so we normalize to
  // degrees for the add:
  float currentAngle = aDest.mU.mOrient.mAngle *
                       nsSVGAngle::GetDegreesPerUnit(aDest.mU.mOrient.mUnit);
  float angleToAdd = aValueToAdd.mU.mOrient.mAngle *
                     nsSVGAngle::GetDegreesPerUnit(aValueToAdd.mU.mOrient.mUnit) *
                     aCount;

  // And then we give the resulting animated value the same units as the value
  // that we're animating to/by (i.e. the same as aValueToAdd):
  aDest.mU.mOrient.mAngle = (currentAngle + angleToAdd) /
                    nsSVGAngle::GetDegreesPerUnit(aValueToAdd.mU.mOrient.mUnit);
  aDest.mU.mOrient.mUnit = aValueToAdd.mU.mOrient.mUnit;

  return NS_OK;
}

nsresult
SVGOrientSMILType::ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const
{
  NS_PRECONDITION(aFrom.mType == aTo.mType,"Trying to compare different types");
  NS_PRECONDITION(aFrom.mType == this, "Unexpected source type");

  if (aFrom.mU.mOrient.mOrientType != nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE ||
      aTo.mU.mOrient.mOrientType != nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE) {
    // TODO: it would be nice to be able to compute distance with auto angles
    return NS_ERROR_FAILURE;
  }

  // Normalize both to degrees in case they're different angle units:
  double from = aFrom.mU.mOrient.mAngle *
                  nsSVGAngle::GetDegreesPerUnit(aFrom.mU.mOrient.mUnit);
  double to   = aTo.mU.mOrient.mAngle *
                  nsSVGAngle::GetDegreesPerUnit(aTo.mU.mOrient.mUnit);

  aDistance = fabs(to - from);

  return NS_OK;
}

nsresult
SVGOrientSMILType::Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const
{
  NS_PRECONDITION(aStartVal.mType == aEndVal.mType,
                  "Trying to interpolate different types");
  NS_PRECONDITION(aStartVal.mType == this,
                  "Unexpected types for interpolation.");
  NS_PRECONDITION(aResult.mType   == this, "Unexpected result type.");

  if (aStartVal.mU.mOrient.mOrientType != nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE ||
      aStartVal.mU.mOrient.mOrientType != nsIDOMSVGMarkerElement::SVG_MARKER_ORIENT_ANGLE) {
    // TODO: it would be nice to be able to handle auto angles too.
    return NS_ERROR_FAILURE;
  }

  float start  = aStartVal.mU.mOrient.mAngle *
                   nsSVGAngle::GetDegreesPerUnit(aStartVal.mU.mOrient.mUnit);
  float end    = aEndVal.mU.mOrient.mAngle *
                   nsSVGAngle::GetDegreesPerUnit(aEndVal.mU.mOrient.mUnit);
  float result = (start + (end - start) * aUnitDistance);

  // Again, we use the unit of the to/by value for the result:
  aResult.mU.mOrient.mAngle = result /
    nsSVGAngle::GetDegreesPerUnit(aEndVal.mU.mOrient.mUnit);
  aResult.mU.mOrient.mUnit = aEndVal.mU.mOrient.mUnit;

  return NS_OK;
}

} // namespace mozilla
