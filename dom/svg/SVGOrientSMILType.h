/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGORIENTSMILTYPE_H_
#define MOZILLA_SVGORIENTSMILTYPE_H_

#include "mozilla/Attributes.h"
#include "nsISMILType.h"

class nsSMILValue;

/**
 * This nsISMILType class is a special case for the 'orient' attribute on SVG's
 * 'marker' element.
 *
 *   orient = "auto | auto-start-reverse | <angle>"
 *
 * Unusually, this attribute doesn't have just a single corresponding DOM
 * property, but rather is split into two properties: 'orientType' (of type
 * SVGAnimatedEnumeration) and 'orientAngle' (of type SVGAnimatedAngle). If
 * 'orientType.animVal' is SVG_MARKER_ORIENT_ANGLE, then
 * 'orientAngle.animVal' contains the angle that is being used. The lacuna
 * value is 0.
 *
 * The SVG 2 specification does not define a
 * SVG_MARKER_ORIENT_AUTO_START_REVERSE constant value for orientType to use;
 * instead, if the attribute is set to "auto-start-reverse",
 * SVG_MARKER_ORIENT_UNKNOWN is used.  Internally, however, we do use a
 * constant with this name.
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
  virtual void     Init(nsSMILValue& aValue) const override;
  virtual void     Destroy(nsSMILValue&) const override;
  virtual nsresult Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const override;
  virtual bool     IsEqual(const nsSMILValue& aLeft,
                           const nsSMILValue& aRight) const override;
  virtual nsresult Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                       uint32_t aCount) const override;
  virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const override;
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const override;

private:
  // Private constructor: prevent instances beyond my singleton.
  constexpr SVGOrientSMILType() {}
};

} // namespace mozilla

#endif // MOZILLA_SVGORIENTSMILTYPE_H_
