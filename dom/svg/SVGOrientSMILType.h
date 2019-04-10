/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGORIENTSMILTYPE_H_
#define MOZILLA_SVGORIENTSMILTYPE_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILType.h"

/**
 * This SMILType class is a special case for the 'orient' attribute on SVG's
 * 'marker' element.
 *
 *   orient = "auto | auto-start-reverse | <angle>"
 *
 * Unusually, this attribute doesn't have just a single corresponding DOM
 * property, but rather is split into two properties: 'orientType' (of type
 * DOMSVGAnimatedEnumeration) and 'orientAngle' (of type DOMSVGAnimatedAngle).
 * If 'orientType.animVal' is SVG_MARKER_ORIENT_ANGLE, then
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

class SMILValue;

class SVGOrientSMILType : public SMILType {
 public:
  // Singleton for SMILValue objects to hold onto.
  static SVGOrientSMILType sSingleton;

 protected:
  // SMILType Methods
  // -------------------
  virtual void Init(SMILValue& aValue) const override;
  virtual void Destroy(SMILValue&) const override;
  virtual nsresult Assign(SMILValue& aDest,
                          const SMILValue& aSrc) const override;
  virtual bool IsEqual(const SMILValue& aLeft,
                       const SMILValue& aRight) const override;
  virtual nsresult Add(SMILValue& aDest, const SMILValue& aValueToAdd,
                       uint32_t aCount) const override;
  virtual nsresult ComputeDistance(const SMILValue& aFrom, const SMILValue& aTo,
                                   double& aDistance) const override;
  virtual nsresult Interpolate(const SMILValue& aStartVal,
                               const SMILValue& aEndVal, double aUnitDistance,
                               SMILValue& aResult) const override;

 private:
  // Private constructor: prevent instances beyond my singleton.
  constexpr SVGOrientSMILType() = default;
};

}  // namespace mozilla

#endif  // MOZILLA_SVGORIENTSMILTYPE_H_
