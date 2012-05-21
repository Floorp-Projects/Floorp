/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGORIENTSMILTYPE_H_
#define MOZILLA_SVGORIENTSMILTYPE_H_

#include "nsISMILType.h"

class nsSMILValue;

/**
 * This nsISMILType class is a special case for the 'orient' attribute on SVG's
 * 'marker' element.
 *
 *   orient = "auto | <angle>"
 *
 * Unusually, this attribute doesn't have just a single corresponding DOM
 * property, but rather is split into two properties: 'orientType' (of type
 * SVGAnimatedEnumeration) and 'orientAngle' (of type SVGAnimatedAngle). If
 * 'orientType.animVal' is not SVG_MARKER_ORIENT_AUTO, then
 * 'orientAngle.animVal' contains the angle that is being used. The lacuna
 * value is 0.
 */

namespace mozilla {

class SVGOrientSMILType : public nsISMILType
{
public:
  // Singleton for nsSMILValue objects to hold onto.
  static SVGOrientSMILType sSingleton;

protected:
  // nsISMILType Methods
  // -------------------
  virtual void     Init(nsSMILValue& aValue) const;
  virtual void     Destroy(nsSMILValue&) const;
  virtual nsresult Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const;
  virtual bool     IsEqual(const nsSMILValue& aLeft,
                           const nsSMILValue& aRight) const;
  virtual nsresult Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                       PRUint32 aCount) const;
  virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const;
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const;

private:
  // Private constructor & destructor: prevent instances beyond my singleton,
  // and prevent others from deleting my singleton.
  SVGOrientSMILType()  {}
  ~SVGOrientSMILType() {}
};

} // namespace mozilla

#endif // MOZILLA_SVGORIENTSMILTYPE_H_
