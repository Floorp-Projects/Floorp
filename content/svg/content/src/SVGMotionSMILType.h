/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of nsISMILType for use by <animateMotion> element */

#ifndef MOZILLA_SVGMOTIONSMILTYPE_H_
#define MOZILLA_SVGMOTIONSMILTYPE_H_

#include "mozilla/Attributes.h"
#include "gfxMatrix.h"
#include "nsISMILType.h"

class gfxFlattenedPath;
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
public:
  // Singleton for nsSMILValue objects to hold onto.
  static SVGMotionSMILType sSingleton;

protected:
  // nsISMILType Methods
  // -------------------
  virtual void     Init(nsSMILValue& aValue) const MOZ_OVERRIDE;
  virtual void     Destroy(nsSMILValue& aValue) const MOZ_OVERRIDE;
  virtual nsresult Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const MOZ_OVERRIDE;
  virtual bool     IsEqual(const nsSMILValue& aLeft,
                           const nsSMILValue& aRight) const MOZ_OVERRIDE;
  virtual nsresult Add(nsSMILValue& aDest,
                       const nsSMILValue& aValueToAdd,
                       uint32_t aCount) const MOZ_OVERRIDE;
  virtual nsresult SandwichAdd(nsSMILValue& aDest,
                               const nsSMILValue& aValueToAdd) const MOZ_OVERRIDE;
  virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const MOZ_OVERRIDE;
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const MOZ_OVERRIDE;
public:
  // Used to generate a transform matrix from an <animateMotion> nsSMILValue.
  static gfxMatrix CreateMatrix(const nsSMILValue& aSMILVal);

  // Used to generate a nsSMILValue for the point at the given distance along
  // the given path.
  static nsSMILValue ConstructSMILValue(gfxFlattenedPath* aPath,
                                        float aDist,
                                        RotateType aRotateType,
                                        float aRotateAngle);

private:
  // Private constructor & destructor: prevent instances beyond my singleton,
  // and prevent others from deleting my singleton.
  SVGMotionSMILType()  {}
  ~SVGMotionSMILType() {}
};

} // namespace mozilla

#endif // MOZILLA_SVGMOTIONSMILTYPE_H_
