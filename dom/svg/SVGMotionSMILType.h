/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* implementation of SMILType for use by <animateMotion> element */

#ifndef MOZILLA_SVGMOTIONSMILTYPE_H_
#define MOZILLA_SVGMOTIONSMILTYPE_H_

#include "mozilla/gfx/2D.h"
#include "mozilla/Attributes.h"
#include "mozilla/SMILType.h"

namespace mozilla {

class SMILValue;

/**
 * MotionRotateType: Enum to indicate the type of our "rotate" attribute.
 */
enum RotateType {
  eRotateType_Explicit,    // for e.g. rotate="45"/"45deg"/"0.785rad"
  eRotateType_Auto,        // for rotate="auto"
  eRotateType_AutoReverse  // for rotate="auto-reverse"
};

/**
 * SVGMotionSMILType: Implements the SMILType interface for SMIL animations
 * from <animateMotion>.
 *
 * NOTE: Even though there's technically no "motion" attribute, we behave in
 * many ways as if there were, for simplicity.
 */
class SVGMotionSMILType : public SMILType {
  typedef mozilla::gfx::Path Path;

 public:
  // Singleton for SMILValue objects to hold onto.
  static SVGMotionSMILType sSingleton;

 protected:
  // SMILType Methods
  // -------------------
  virtual void Init(SMILValue& aValue) const override;
  virtual void Destroy(SMILValue& aValue) const override;
  virtual nsresult Assign(SMILValue& aDest,
                          const SMILValue& aSrc) const override;
  virtual bool IsEqual(const SMILValue& aLeft,
                       const SMILValue& aRight) const override;
  virtual nsresult Add(SMILValue& aDest, const SMILValue& aValueToAdd,
                       uint32_t aCount) const override;
  virtual nsresult SandwichAdd(SMILValue& aDest,
                               const SMILValue& aValueToAdd) const override;
  virtual nsresult ComputeDistance(const SMILValue& aFrom, const SMILValue& aTo,
                                   double& aDistance) const override;
  virtual nsresult Interpolate(const SMILValue& aStartVal,
                               const SMILValue& aEndVal, double aUnitDistance,
                               SMILValue& aResult) const override;

 public:
  // Used to generate a transform matrix from an <animateMotion> SMILValue.
  static gfx::Matrix CreateMatrix(const SMILValue& aSMILVal);

  // Used to generate a SMILValue for the point at the given distance along
  // the given path.
  static SMILValue ConstructSMILValue(Path* aPath, float aDist,
                                      RotateType aRotateType,
                                      float aRotateAngle);

 private:
  // Private constructor: prevent instances beyond my singleton.
  constexpr SVGMotionSMILType() = default;
};

}  // namespace mozilla

#endif  // MOZILLA_SVGMOTIONSMILTYPE_H_
