/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_SVGANIMATEDPATHSEGLIST_H__
#define MOZILLA_SVGANIMATEDPATHSEGLIST_H__

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include "nsAutoPtr.h"
#include "nsISMILAttr.h"
#include "SVGPathData.h"

class nsSMILValue;
class nsSVGElement;

namespace mozilla {

namespace dom {
class SVGAnimationElement;
} // namespace dom

/**
 * Class SVGAnimatedPathSegList
 *
 * Despite the fact that no SVGAnimatedPathSegList interface or objects exist
 * in the SVG specification (unlike e.g. SVGAnimated*Length*List), we
 * nevertheless have this internal class. (Note that there is an
 * SVGAnimatedPathData interface, but that's quite different to
 * SVGAnimatedLengthList since it is inherited by elements, as opposed to
 * elements having members of that type.) The reason that we have this class is
 * to provide a single locked down point of entry to the SVGPathData objects,
 * which helps ensure that the DOM wrappers for SVGPathData objects' are always
 * kept in sync. This is vitally important (see the comment in
 * DOMSVGPathSegList::InternalListWillChangeTo) and frees consumers from having
 * to know or worry about wrappers (or forget about them!) for the most part.
 */
class SVGAnimatedPathSegList final
{
  // friends so that they can get write access to mBaseVal and mAnimVal
  friend class DOMSVGPathSeg;
  friend class DOMSVGPathSegList;

public:
  SVGAnimatedPathSegList() {}

  /**
   * Because it's so important that mBaseVal and its DOMSVGPathSegList wrapper
   * (if any) be kept in sync (see the comment in
   * DOMSVGPathSegList::InternalListWillChangeTo), this method returns a const
   * reference. Only our friend classes may get mutable references to mBaseVal.
   */
  const SVGPathData& GetBaseValue() const {
    return mBaseVal;
  }

  nsresult SetBaseValueString(const nsAString& aValue);

  void ClearBaseValue();

  /**
   * const! See comment for GetBaseValue!
   */
  const SVGPathData& GetAnimValue() const {
    return mAnimVal ? *mAnimVal : mBaseVal;
  }

  nsresult SetAnimValue(const SVGPathData& aValue,
                        nsSVGElement *aElement);

  void ClearAnimValue(nsSVGElement *aElement);

  /**
   * Needed for correct DOM wrapper construction since GetAnimValue may
   * actually return the baseVal!
   */
  void *GetBaseValKey() const {
    return (void*)&mBaseVal;
  }
  void *GetAnimValKey() const {
    return (void*)&mAnimVal;
  }

  bool IsAnimating() const {
    return !!mAnimVal;
  }

  UniquePtr<nsISMILAttr> ToSMILAttr(nsSVGElement* aElement);

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const;

private:

  // mAnimVal is a pointer to allow us to determine if we're being animated or
  // not. Making it a non-pointer member and using mAnimVal.IsEmpty() to check
  // if we're animating is not an option, since that would break animation *to*
  // the empty string (<set to="">).

  SVGPathData mBaseVal;
  nsAutoPtr<SVGPathData> mAnimVal;

  struct SMILAnimatedPathSegList : public nsISMILAttr
  {
  public:
    SMILAnimatedPathSegList(SVGAnimatedPathSegList* aVal,
                            nsSVGElement* aElement)
      : mVal(aVal)
      , mElement(aElement)
    {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    SVGAnimatedPathSegList *mVal;
    nsSVGElement *mElement;

    // nsISMILAttr methods
    virtual nsresult ValueFromString(const nsAString& aStr,
                                     const dom::SVGAnimationElement* aSrcElement,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const override;
    virtual nsSMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const nsSMILValue& aValue) override;
  };
};

} // namespace mozilla

#endif // MOZILLA_SVGANIMATEDPATHSEGLIST_H__
