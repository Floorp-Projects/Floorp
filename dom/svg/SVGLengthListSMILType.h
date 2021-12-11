/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGLENGTHLISTSMILTYPE_H_
#define DOM_SVG_SVGLENGTHLISTSMILTYPE_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILType.h"

namespace mozilla {

class SMILValue;

////////////////////////////////////////////////////////////////////////
// SVGLengthListSMILType
//
// Operations for animating an SVGLengthList.
//
class SVGLengthListSMILType : public SMILType {
 public:
  // Singleton for SMILValue objects to hold onto.
  static SVGLengthListSMILType sSingleton;

 protected:
  // SMILType Methods
  // -------------------

  /**
   * When this method initializes the SVGLengthListAndInfo for its SMILValue
   * argument, it has to blindly set its mCanZeroPadList to true despite
   * the fact that some attributes can't be zero-padded. (See the explaination
   * that follows.) SVGAnimatedLengthList::SMILAnimatedLengthList's
   * GetBaseValue() and ValueFromString() methods then override this for the
   * SMILValue objects that they create to set this flag to the appropriate
   * value for the attribute in question.
   *
   * The reason that we default to setting the mCanZeroPadList to true is
   * because the SMIL engine creates "zero" valued SMILValue objects for
   * intermediary calculations, and may pass such a SMILValue (along with a
   * SMILValue from an animation element - that is a SMILValue created by
   * SVGAnimatedLengthList::SMILAnimatedLengthList's GetBaseValue() or
   * ValueFromString() methods) into the Add(), ComputeDistance() or
   * Interpolate() methods below. Even in the case of animation of list
   * attributes that may *not* be padded with zeros (such as 'x' and 'y' on the
   * <text> element), we need to allow zero-padding of these "zero" valued
   * SMILValue's lists. One reason for this is illustrated by the following
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
   * the SMIL engine interpolates between a "zero" SMILValue that it creates
   * (since there is no explicit "from") and the "2 2", before the result of
   * that interpolation is added to the "2 4" from the base layer. Clearly for
   * the interpolation between the "zero" SMILValue and "2 2" to work, the
   * "zero" SMILValue's SVGLengthListAndInfo must be zero paddable - hence
   * why this method always sets mCanZeroPadList to true.
   *
   * (Since the Add(), ComputeDistance() and Interpolate() methods may be
   * passed two input SMILValue objects for which CanZeroPadList() returns
   * opposite values, these methods must be careful what they set the flag to
   * on the SMILValue that they output. If *either* of the input SMILValues
   * has an SVGLengthListAndInfo for which CanZeroPadList() returns false,
   * then they must set the flag to false on the output SMILValue too. If
   * the methods failed to do that, then when the result SMILValue objects
   * from each sandwich layer are composited together, we could end up allowing
   * animation between lists of different length when we should not!)
   */
  virtual void Init(SMILValue& aValue) const override;

  virtual void Destroy(SMILValue& aValue) const override;
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
  constexpr SVGLengthListSMILType() = default;
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGLENGTHLISTSMILTYPE_H_
