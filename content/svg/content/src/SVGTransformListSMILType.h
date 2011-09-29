/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef SVGTRANSFORMLISTSMILTYPE_H_
#define SVGTRANSFORMLISTSMILTYPE_H_

#include "nsISMILType.h"
#include "nsTArray.h"

class nsSMILValue;

namespace mozilla {

class SVGTransform;
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
class SVGTransformListSMILType : public nsISMILType
{
public:
  // Singleton for nsSMILValue objects to hold onto.
  static SVGTransformListSMILType sSingleton;

protected:
  // nsISMILType Methods
  // -------------------
  virtual void     Init(nsSMILValue& aValue) const;
  virtual void     Destroy(nsSMILValue& aValue) const;
  virtual nsresult Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const;
  virtual bool     IsEqual(const nsSMILValue& aLeft,
                           const nsSMILValue& aRight) const;
  virtual nsresult Add(nsSMILValue& aDest,
                       const nsSMILValue& aValueToAdd,
                       PRUint32 aCount) const;
  virtual nsresult SandwichAdd(nsSMILValue& aDest,
                               const nsSMILValue& aValueToAdd) const;
  virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const;
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const;

public:
  // Transform array accessors
  // -------------------------
  static nsresult AppendTransform(const SVGTransformSMILData& aTransform,
                                  nsSMILValue& aValue);
  static bool AppendTransforms(const SVGTransformList& aList,
                                 nsSMILValue& aValue);
  static bool GetTransforms(const nsSMILValue& aValue,
                              nsTArray<SVGTransform>& aTransforms);


private:
  // Private constructor & destructor: prevent instances beyond my singleton,
  // and prevent others from deleting my singleton.
  SVGTransformListSMILType() {}
  ~SVGTransformListSMILType() {}
};

} // end namespace mozilla

#endif // SVGLISTTRANSFORMSMILTYPE_H_
