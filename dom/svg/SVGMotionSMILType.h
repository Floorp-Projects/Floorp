/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of nsISMILType for use by <animateMotion> element */

#ifndef MOZILLA_SVGMOTIONSMILTYPE_H_
#define MOZILLA_SVGMOTIONSMILTYPE_H_

#include "mozilla/gfx/2D.h"
#include "mozilla/Attributes.h"
#include "nsISMILType.h"

class nsSMILValue;

namespace mozilla {

/**
 * MotionRotateType: Enum to indicate the type of our "rotate" attribute.
 */
enum RotateType {
  eRotateType_Explicit,     // for e.g. rotate="45"/"45deg"/"0.785rad"
  eRotateType_Auto,         // for rotate="auto"
  eRotateType_AutoReverse   // for rotate="auto-reverse"
};

/**
 * SVGMotionSMILType: Implements the nsISMILType interface for SMIL animations
 * from <animateMotion>.
 *
 * NOTE: Even though there's technically no "motion" attribute, we behave in
 * many ways as if there were, for simplicity.
 */
class SVGMotionSMILType : public nsISMILType
{
  typedef mozilla::gfx::Path Path;

public:
  // Singleton for nsSMILValue objects to hold onto.
  static SVGMotionSMILType sSingleton;

protected:
  // nsISMILType Methods
  // -------------------
  virtual void     Init(nsSMILValue& aValue) const override;
  virtual void     Destroy(nsSMILValue& aValue) const override;
  virtual nsresult Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const override;
  virtual bool     IsEqual(const nsSMILValue& aLeft,
                           const nsSMILValue& aRight) const override;
  virtual nsresult Add(nsSMILValue& aDest,
                       const nsSMILValue& aValueToAdd,
                       uint32_t aCount) const override;
  virtual nsresult SandwichAdd(nsSMILValue& aDest,
                               const nsSMILValue& aValueToAdd) const override;
  virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const override;
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const override;
public:
  // Used to generate a transform matrix from an <animateMotion> nsSMILValue.
  static gfx::Matrix CreateMatrix(const nsSMILValue& aSMILVal);

  // Used to generate a nsSMILValue for the point at the given distance along
  // the given path.
  static nsSMILValue ConstructSMILValue(Path* aPath,
                                        float aDist,
                                        RotateType aRotateType,
                                        float aRotateAngle);

private:
  // Private constructor: prevent instances beyond my singleton.
  constexpr SVGMotionSMILType() {}
};

} // namespace mozilla

#endif // MOZILLA_SVGMOTIONSMILTYPE_H_
