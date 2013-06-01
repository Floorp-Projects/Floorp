/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SVGTRANSFORMLISTSMILTYPE_H_
#define SVGTRANSFORMLISTSMILTYPE_H_

#include "mozilla/Attributes.h"
#include "nsISMILType.h"
#include "nsTArray.h"

class nsSMILValue;

namespace mozilla {

class nsSVGTransform;
class SVGTransformList;
class SVGTransformSMILData;

////////////////////////////////////////////////////////////////////////
// SVGTransformListSMILType
//
// Operations for animating an nsSVGTransformList.
//
// This class is confused somewhat by the fact that:
// (i)  An <animateTransform> element animates an SVGTransformList
// (ii) BUT <animateTransform> only allows the user to specify animation values
//      for an nsSVGTransform
//
// This may be rectified in a future edition of SVG but for now it means that
// the underlying value of an animation may be something of the form:
//
//   rotate(90) scale(2) skewX(50)
//
// BUT the animation values can only ever be SINGLE transform operations such as
//
//   rotate(90)
//
//   (actually the syntax here is:
//      <animateTransform type="rotate" from="0" to="90"...
//      OR maybe
//      <animateTransform type="rotate" values="0; 90; 30; 50"...
//      OR even (with a rotation centre)
//      <animateTransform type="rotate" values="0 50 20; 30 50 20; 70 0 0"...)
//
// This has many implications for the number of elements we expect in the
// transform array supplied for each operation.
//
// For example, Add() only ever operates on the values specified on an
// <animateTransform> element and so these values can only ever contain 0 or
// 1 TRANSFORM elements as the syntax doesn't allow more. (A "value" here is
// a single element in the values array such as "0 50 20" above.)
//
// Likewise ComputeDistance() only ever operates within the values specified on
// an <animateTransform> element so similar conditions hold.
//
// However, SandwichAdd() combines with a base value which may contain 0..n
// transforms either because the base value of the attribute specifies a series
// of transforms, e.g.
//
//   <circle transform="translate(30) rotate(50)"... >
//     <animateTransform.../>
//   </circle>
//
// or because several animations target the same attribute and are additive and
// so are simply appended on to the transformation array, e.g.
//
//   <circle transform="translate(30)"... >
//     <animateTransform type="rotate" additive="sum".../>
//     <animateTransform type="scale" additive="sum".../>
//     <animateTransform type="skewX" additive="sum".../>
//   </circle>
//
// Similar conditions hold for Interpolate() which in cases such as to-animation
// may have use a start-value the base value of the target attribute (which as
// we have seen above can contain 0..n elements) whilst the end-value comes from
// the <animateTransform> and so can only hold 1 transform.
//
class SVGTransformListSMILType : public nsISMILType
{
public:
  // Singleton for nsSMILValue objects to hold onto.
  static SVGTransformListSMILType sSingleton;

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
  // Transform array accessors
  // -------------------------
  static nsresult AppendTransform(const SVGTransformSMILData& aTransform,
                                  nsSMILValue& aValue);
  static bool AppendTransforms(const SVGTransformList& aList,
                                 nsSMILValue& aValue);
  static bool GetTransforms(const nsSMILValue& aValue,
                              FallibleTArray<nsSVGTransform>& aTransforms);


private:
  // Private constructor & destructor: prevent instances beyond my singleton,
  // and prevent others from deleting my singleton.
  SVGTransformListSMILType() {}
  ~SVGTransformListSMILType() {}
};

} // end namespace mozilla

#endif // SVGLISTTRANSFORMSMILTYPE_H_
