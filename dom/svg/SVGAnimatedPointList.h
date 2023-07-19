/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_SVG_SVGANIMATEDPOINTLIST_H_
#define DOM_SVG_SVGANIMATEDPOINTLIST_H_

#include "mozilla/Attributes.h"
#include "mozilla/SMILAttr.h"
#include "mozilla/UniquePtr.h"
#include "SVGPointList.h"

namespace mozilla {

class SMILValue;

namespace dom {
class DOMSVGPoint;
class DOMSVGPointList;
class SVGAnimationElement;
class SVGElement;
}  // namespace dom

/**
 * Class SVGAnimatedPointList
 *
 * Despite the fact that no SVGAnimatedPointList interface or objects exist
 * in the SVG specification (unlike e.g. SVGAnimated*Length*List), we
 * nevertheless have this internal class. (Note that there is an
 * SVGAnimatedPoints interface, but that's quite different to
 * SVGAnimatedLengthList since it is inherited by elements, as opposed to
 * elements having members of that type.) The reason that we have this class is
 * to provide a single locked down point of entry to the SVGPointList objects,
 * which helps ensure that the DOM wrappers for SVGPointList objects' are
 * always kept in sync. This is vitally important (see the comment in
 * DOMSVGPointList::InternalListWillChangeTo) and frees consumers from having
 * to know or worry about wrappers (or forget about them!) for the most part.
 */
class SVGAnimatedPointList {
  // friends so that they can get write access to mBaseVal and mAnimVal
  friend class dom::DOMSVGPoint;
  friend class dom::DOMSVGPointList;

 public:
  SVGAnimatedPointList() = default;

  SVGAnimatedPointList& operator=(const SVGAnimatedPointList& aOther) {
    mBaseVal = aOther.mBaseVal;
    if (aOther.mAnimVal) {
      mAnimVal = MakeUnique<SVGPointList>(*aOther.mAnimVal);
    }
    return *this;
  }

  /**
   * Because it's so important that mBaseVal and its DOMSVGPointList wrapper
   * (if any) be kept in sync (see the comment in
   * DOMSVGPointList::InternalListWillChangeTo), this method returns a const
   * reference. Only our friend classes may get mutable references to mBaseVal.
   */
  const SVGPointList& GetBaseValue() const { return mBaseVal; }

  nsresult SetBaseValueString(const nsAString& aValue);

  void ClearBaseValue();

  /**
   * const! See comment for GetBaseValue!
   */
  const SVGPointList& GetAnimValue() const {
    return mAnimVal ? *mAnimVal : mBaseVal;
  }

  nsresult SetAnimValue(const SVGPointList& aNewAnimValue,
                        dom::SVGElement* aElement);

  void ClearAnimValue(dom::SVGElement* aElement);

  /**
   * Needed for correct DOM wrapper construction since GetAnimValue may
   * actually return the baseVal!
   */
  void* GetBaseValKey() const { return (void*)&mBaseVal; }
  void* GetAnimValKey() const { return (void*)&mAnimVal; }

  bool IsAnimating() const { return !!mAnimVal; }

  UniquePtr<SMILAttr> ToSMILAttr(dom::SVGElement* aElement);

 private:
  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGPointList mBaseVal;
  UniquePtr<SVGPointList> mAnimVal;

  struct SMILAnimatedPointList : public SMILAttr {
   public:
    SMILAnimatedPointList(SVGAnimatedPointList* aVal, dom::SVGElement* aElement)
        : mVal(aVal), mElement(aElement) {}

    // These will stay alive because a SMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedPointList* mVal;
    dom::SVGElement* mElement;

    // SMILAttr methods
    nsresult ValueFromString(const nsAString& aStr,
                             const dom::SVGAnimationElement* aSrcElement,
                             SMILValue& aValue,
                             bool& aPreventCachingOfSandwich) const override;
    SMILValue GetBaseValue() const override;
    void ClearAnimValue() override;
    nsresult SetAnimValue(const SMILValue& aValue) override;
  };
};

}  // namespace mozilla

#endif  // DOM_SVG_SVGANIMATEDPOINTLIST_H_
