/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGLENGTHLISTSMILTYPE_H_
#define MOZILLA_SVGLENGTHLISTSMILTYPE_H_

#include "nsISMILType.h"

class nsSMILValue;

namespace mozilla {

////////////////////////////////////////////////////////////////////////
// SVGLengthListSMILType
//
// Operations for animating an SVGLengthList.
//
class SVGLengthListSMILType : public nsISMILType
{
public:
  // Singleton for nsSMILValue objects to hold onto.
  static SVGLengthListSMILType sSingleton;

protected:
  // nsISMILType Methods
  // -------------------

  /**
   * When this method initializes the SVGLengthListAndInfo for its nsSMILValue
   * argument, it has to blindly set its mCanZeroPadList to true despite
   * the fact that some attributes can't be zero-padded. (See the explaination
   * that follows.) SVGAnimatedLengthList::SMILAnimatedLengthList's
   * GetBaseValue() and ValueFromString() methods then override this for the
   * nsSMILValue objects that they create to set this flag to the appropriate
   * value for the attribute in question.
   *
   * The reason that we default to setting the mCanZeroPadList to true is
   * because the SMIL engine creates "zero" valued nsSMILValue objects for
   * intermediary calculations, and may pass such an nsSMILValue (along with an
   * nsSMILValue from an animation element - that is an nsSMILValue created by
   * SVGAnimatedLengthList::SMILAnimatedLengthList's GetBaseValue() or
   * ValueFromString() methods) into the Add(), ComputeDistance() or
   * Interpolate() methods below. Even in the case of animation of list
   * attributes that may *not* be padded with zeros (such as 'x' and 'y' on the
   * <text> element), we need to allow zero-padding of these "zero" valued
   * nsSMILValue's lists. One reason for this is illustrated by the following
   * example:
   *
   *   <text x="2 4">foo
   *      <animate by="2 2" .../>
   *   </text>
   *
   * In this example there are two SMIL animation layers to be sandwiched: the
   * base layer, and the layer created for the <animate> element. The SMIL
   * engine calculates the result of each layer *independently*, before
   * compositing the results together. Thus for the <animate> sandwich layer
   * the SMIL engine interpolates between a "zero" nsSMILValue that it creates
   * (since there is no explicit "from") and the "2 2", before the result of
   * that interpolation is added to the "2 4" from the base layer. Clearly for
   * the interpolation between the "zero" nsSMILValue and "2 2" to work, the
   * "zero" nsSMILValue's SVGLengthListAndInfo must be zero paddable - hence
   * why this method always sets mCanZeroPadList to true.
   *
   * (Since the Add(), ComputeDistance() and Interpolate() methods may be
   * passed two input nsSMILValue objects for which CanZeroPadList() returns
   * opposite values, these methods must be careful what they set the flag to
   * on the nsSMILValue that they output. If *either* of the input nsSMILValues
   * has an SVGLengthListAndInfo for which CanZeroPadList() returns false,
   * then they must set the flag to false on the output nsSMILValue too. If
   * the methods failed to do that, then when the result nsSMILValue objects
   * from each sandwich layer are composited together, we could end up allowing
   * animation between lists of different length when we should not!)
   */
  virtual void     Init(nsSMILValue& aValue) const;

  virtual void     Destroy(nsSMILValue& aValue) const;
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
  SVGLengthListSMILType() {}
  ~SVGLengthListSMILType() {}
};

} // namespace mozilla

#endif // MOZILLA_SVGLENGTHLISTSMILTYPE_H_
