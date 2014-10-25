/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGPOINTLISTSMILTYPE_H_
#define MOZILLA_SVGPOINTLISTSMILTYPE_H_

#include "mozilla/Attributes.h"
#include "nsISMILType.h"

class nsSMILValue;

namespace mozilla {

////////////////////////////////////////////////////////////////////////
// SVGPointListSMILType
//
// Operations for animating an SVGPointList.
//
class SVGPointListSMILType : public nsISMILType
{
public:
  // Singleton for nsSMILValue objects to hold onto.
  static SVGPointListSMILType sSingleton;

protected:
  // nsISMILType Methods
  // -------------------

  virtual void     Init(nsSMILValue& aValue) const MOZ_OVERRIDE;

  virtual void     Destroy(nsSMILValue& aValue) const MOZ_OVERRIDE;
  virtual nsresult Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const MOZ_OVERRIDE;
  virtual bool     IsEqual(const nsSMILValue& aLeft,
                           const nsSMILValue& aRight) const MOZ_OVERRIDE;
  virtual nsresult Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                       uint32_t aCount) const MOZ_OVERRIDE;
  virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const MOZ_OVERRIDE;
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const MOZ_OVERRIDE;

private:
  // Private constructor: prevent instances beyond my singleton.
  MOZ_CONSTEXPR SVGPointListSMILType() {}
};

} // namespace mozilla

#endif // MOZILLA_SVGPOINTLISTSMILTYPE_H_
