/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGANIMATEDLENGTHLIST_H_
#define DOM_SVG_SVGANIMATEDLENGTHLIST_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/UniquePtr.h"
#include "SVGLengthList.h"

namespace mozilla {

class SMILValue;

namespace dom {
class SVGAnimationElement;
class SVGElement;
}  // namespace dom

/**
 * Class SVGAnimatedLengthList
 *
 * This class is very different to the SVG DOM interface of the same name found
 * in the SVG specification. This is a lightweight internal class - see
 * DOMSVGAnimatedLengthList for the heavier DOM class that wraps instances of
 * this class and implements the SVG specification's SVGAnimatedLengthList DOM
 * interface.
 *
 * Except where noted otherwise, this class' methods take care of keeping the
 * appropriate DOM wrappers in sync (see the comment in
 * DOMSVGAnimatedLengthList::InternalBaseValListWillChangeTo) so that their
 * consumers don't need to concern themselves with that.
 */
class SVGAnimatedLengthList {
  // friends so that they can get write access to mBaseVal
  friend class dom::DOMSVGLength;
  friend class dom::DOMSVGLengthList;

 public:
  SVGAnimatedLengthList() = default;

  SVGAnimatedLengthList& operator=(const SVGAnimatedLengthList& aOther) {
    mBaseVal = aOther.mBaseVal;
    if (aOther.mAnimVal) {
      mAnimVal = MakeUnique<SVGLengthList>(*aOther.mAnimVal);
    }
    return *this;
  }

  /**
   * Because it's so important that mBaseVal and its DOMSVGLengthList wrapper
   * (if any) be kept in sync (see the comment in
   * DOMSVGAnimatedLengthList::InternalBaseValListWillChangeTo), this method
   * returns a const reference. Only our friend classes may get mutable
   * references to mBaseVal.
   */
  const SVGLengthList& GetBaseValue() const { return mBaseVal; }

  nsresult SetBaseValueString(const nsAString& aValue);

  void ClearBaseValue(uint32_t aAttrEnum);

  const SVGLengthList& GetAnimValue() const {
    return mAnimVal ? *mAnimVal : mBaseVal;
  }

  nsresult SetAnimValue(const SVGLengthList& aNewAnimValue,
                        dom::SVGElement* aElement, uint32_t aAttrEnum);

  void ClearAnimValue(dom::SVGElement* aElement, uint32_t aAttrEnum);

  bool IsAnimating() const { return !!mAnimVal; }

  UniquePtr<SMILAttr> ToSMILAttr(dom::SVGElement* aSVGElement,
                                 uint8_t aAttrEnum, uint8_t aAxis,
                                 bool aCanZeroPadList);

 private:
  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGLengthList mBaseVal;
  UniquePtr<SVGLengthList> mAnimVal;

  struct SMILAnimatedLengthList : public SMILAttr {
   public:
    SMILAnimatedLengthList(SVGAnimatedLengthList* aVal,
                           dom::SVGElement* aSVGElement, uint8_t aAttrEnum,
                           uint8_t aAxis, bool aCanZeroPadList)
        : mVal(aVal),
          mElement(aSVGElement),
          mAttrEnum(aAttrEnum),
          mAxis(aAxis),
          mCanZeroPadList(aCanZeroPadList) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedLengthList* mVal;
    dom::SVGElement* mElement;
    uint8_t mAttrEnum;
    uint8_t mAxis;
    bool mCanZeroPadList;  // See SVGLengthListAndInfo::CanZeroPadList

    // SMILAttr methods
    virtual nsresult ValueFromString(
        const nsAString& aStr, const dom::SVGAnimationElement* aSrcElement,
        SMILValue& aValue, bool& aPreventCachingOfSandwich) const override;
    virtual SMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const SMILValue& aValue) override;
  };
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGANIMATEDLENGTHLIST_H_
