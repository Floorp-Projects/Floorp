/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SVGTRANSFORMLISTSMILTYPE_H_
#define SVGTRANSFORMLISTSMILTYPE_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILType.h"
#include "nsTArray.h"

namespace mozilla {

class SMILValue;
class SVGTransform;
class SVGTransformList;
class SVGTransformSMILData;

////////////////////////////////////////////////////////////////////////
// SVGTransformListSMILType
//
// Operations for animating an SVGTransformList.
//
// This class is confused somewhat by the fact that:
// (i)  An <animateTransform> element animates an SVGTransformList
// (ii) BUT <animateTransform> only allows the user to specify animation values
//      for an SVGTransform
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
class SVGTransformListSMILType : public SMILType {
 public:
  // Singleton for SMILValue objects to hold onto.
  static SVGTransformListSMILType* Singleton() {
    static SVGTransformListSMILType sSingleton;
    return &sSingleton;
  }

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
  // Transform array accessors
  // -------------------------
  static nsresult AppendTransform(const SVGTransformSMILData& aTransform,
                                  SMILValue& aValue);
  static bool AppendTransforms(const SVGTransformList& aList,
                               SMILValue& aValue);
  static bool GetTransforms(const SMILValue& aValue,
                            FallibleTArray<SVGTransform>& aTransforms);

 private:
  // Private constructor: prevent instances beyond my singleton.
  constexpr SVGTransformListSMILType() = default;
};

}  // end namespace mozilla

#endif  // SVGLISTTRANSFORMSMILTYPE_H_
