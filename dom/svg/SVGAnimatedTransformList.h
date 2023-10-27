/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGANIMATEDTRANSFORMLIST_H_
#define DOM_SVG_SVGANIMATEDTRANSFORMLIST_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/SVGTransformList.h"

class nsAtom;

namespace mozilla {

class SMILValue;

namespace dom {
class SVGAnimationElement;
class SVGElement;
class DOMSVGTransform;
}  // namespace dom

/**
 * Class SVGAnimatedTransformList
 *
 * This class is very different to the SVG DOM interface of the same name found
 * in the SVG specification. This is a lightweight internal class - see
 * DOMSVGAnimatedTransformList for the heavier DOM class that wraps instances of
 * this class and implements the SVG specification's SVGAnimatedTransformList
 * DOM interface.
 *
 * Except where noted otherwise, this class' methods take care of keeping the
 * appropriate DOM wrappers in sync (see the comment in
 * DOMSVGAnimatedTransformList::InternalBaseValListWillChangeTo) so that their
 * consumers don't need to concern themselves with that.
 */
class SVGAnimatedTransformList {
  // friends so that they can get write access to mBaseVal
  friend class dom::DOMSVGTransform;
  friend class dom::DOMSVGTransformList;

 public:
  SVGAnimatedTransformList()
      : mIsBaseSet(false), mCreatedOrRemovedOnLastChange(true) {}

  SVGAnimatedTransformList& operator=(const SVGAnimatedTransformList& aOther) {
    mBaseVal = aOther.mBaseVal;
    if (aOther.mAnimVal) {
      mAnimVal = MakeUnique<SVGTransformList>(*aOther.mAnimVal);
    }
    mIsBaseSet = aOther.mIsBaseSet;
    mCreatedOrRemovedOnLastChange = aOther.mCreatedOrRemovedOnLastChange;
    return *this;
  }

  /**
   * Because it's so important that mBaseVal and its DOMSVGTransformList wrapper
   * (if any) be kept in sync (see the comment in
   * DOMSVGAnimatedTransformList::InternalBaseValListWillChangeTo), this method
   * returns a const reference. Only our friend classes may get mutable
   * references to mBaseVal.
   */
  const SVGTransformList& GetBaseValue() const { return mBaseVal; }

  nsresult SetBaseValue(const SVGTransformList& aValue,
                        dom::SVGElement* aSVGElement);

  nsresult SetBaseValueString(const nsAString& aValue,
                              dom::SVGElement* aSVGElement);

  void ClearBaseValue();

  const SVGTransformList& GetAnimValue() const {
    return mAnimVal ? *mAnimVal : mBaseVal;
  }

  nsresult SetAnimValue(const SVGTransformList& aValue,
                        dom::SVGElement* aElement);

  void ClearAnimValue(dom::SVGElement* aElement);

  /**
   * Returns true if the corresponding transform attribute is set (or animated)
   * to a valid value. Unlike HasTransform it will return true for an empty
   * transform.
   */
  bool IsExplicitlySet() const;

  /**
   * Returns true if the corresponding transform attribute is set (or animated)
   * to a valid value, such that we have at least one transform in our list.
   * Returns false otherwise (e.g. if the transform attribute is missing or
   * empty or invalid).
   */
  bool HasTransform() const {
    return (mAnimVal && !mAnimVal->IsEmpty()) || !mBaseVal.IsEmpty();
  }

  bool IsAnimating() const { return !!mAnimVal; }

  /**
   * Returns true if the last change of this transform went from having to not
   * having a transform or vice versa.
   *
   * (This is used as part of an optimization in
   * SVGTransformableElement::GetAttributeChangeHint. That function reports an
   * inexpensive nsChangeHint when a transform has just modified -- but this
   * accessor lets it detect cases where the "modification" is actually creating
   * a transform where we previously had none. These cases require a more
   * thorough nsChangeHint.)
   */
  bool CreatedOrRemovedOnLastChange() const {
    return mCreatedOrRemovedOnLastChange;
  }

  UniquePtr<SMILAttr> ToSMILAttr(dom::SVGElement* aSVGElement);

 private:
  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGTransformList mBaseVal;
  UniquePtr<SVGTransformList> mAnimVal;
  bool mIsBaseSet;
  // See documentation for accessor.
  bool mCreatedOrRemovedOnLastChange;

  struct SMILAnimatedTransformList : public SMILAttr {
   public:
    SMILAnimatedTransformList(SVGAnimatedTransformList* aVal,
                              dom::SVGElement* aSVGElement)
        : mVal(aVal), mElement(aSVGElement) {}

    // SMILAttr methods
    nsresult ValueFromString(const nsAString& aStr,
                             const dom::SVGAnimationElement* aSrcElement,
                             SMILValue& aValue,
                             bool& aPreventCachingOfSandwich) const override;
    SMILValue GetBaseValue() const override;
    void ClearAnimValue() override;
    nsresult SetAnimValue(const SMILValue& aNewAnimValue) override;

   protected:
    static void ParseValue(const nsAString& aSpec, const nsAtom* aTransformType,
                           SMILValue& aResult);
    static int32_t ParseParameterList(const nsAString& aSpec, float* aVars,
                                      int32_t aNVars);

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedTransformList* mVal;
    dom::SVGElement* mElement;
  };
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGANIMATEDTRANSFORMLIST_H_
