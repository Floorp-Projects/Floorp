/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGORIENTSMILTYPE_H_
#define DOM_SVG_SVGORIENTSMILTYPE_H_

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
  void Init(SMILValue& aValue) const override;
  void Destroy(SMILValue&) const override;
  nsresult Assign(SMILValue& aDest, const SMILValue& aSrc) const override;
  bool IsEqual(const SMILValue& aLeft, const SMILValue& aRight) const override;
  nsresult Add(SMILValue& aDest, const SMILValue& aValueToAdd,
               uint32_t aCount) const override;
  nsresult ComputeDistance(const SMILValue& aFrom, const SMILValue& aTo,
                           double& aDistance) const override;
  nsresult Interpolate(const SMILValue& aStartVal, const SMILValue& aEndVal,
                       double aUnitDistance, SMILValue& aResult) const override;

 private:
  // Private constructor: prevent instances beyond my singleton.
  constexpr SVGOrientSMILType() = default;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGORIENTSMILTYPE_H_
