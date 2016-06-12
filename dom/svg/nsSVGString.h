/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGSTRING_H__
#define __NS_SVGSTRING_H__

#include "nsAutoPtr.h"
#include "nsError.h"
#include "nsSVGElement.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/SVGAnimatedString.h"

class nsSVGString
{

public:
  void Init(uint8_t aAttrEnum) {
    mAnimVal = nullptr;
    mAttrEnum = aAttrEnum;
    mIsBaseSet = false;
  }

  void SetBaseValue(const nsAString& aValue,
                    nsSVGElement *aSVGElement,
                    bool aDoSetAttr);
  void GetBaseValue(nsAString& aValue, const nsSVGElement *aSVGElement) const
    { aSVGElement->GetStringBaseValue(mAttrEnum, aValue); }

  void SetAnimValue(const nsAString& aValue, nsSVGElement *aSVGElement);
  void GetAnimValue(nsAString& aValue, const nsSVGElement *aSVGElement) const;

  // Returns true if the animated value of this string has been explicitly
  // set (either by animation, or by taking on the base value which has been
  // explicitly set by markup or a DOM call), false otherwise.
  // If this returns false, the animated value is still valid, that is,
  // useable, and represents the default base value of the attribute.
  bool IsExplicitlySet() const
    { return !!mAnimVal || mIsBaseSet; }

  already_AddRefed<mozilla::dom::SVGAnimatedString>
  ToDOMAnimatedString(nsSVGElement* aSVGElement);

  // Returns a new nsISMILAttr object that the caller must delete
  nsISMILAttr* ToSMILAttr(nsSVGElement *aSVGElement);

private:

  nsAutoPtr<nsString> mAnimVal;
  uint8_t mAttrEnum; // element specified tracking for attribute
  bool mIsBaseSet;

public:
  struct DOMAnimatedString final : public mozilla::dom::SVGAnimatedString
  {
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(DOMAnimatedString)

    DOMAnimatedString(nsSVGString* aVal, nsSVGElement* aSVGElement)
      : mozilla::dom::SVGAnimatedString(aSVGElement)
      , mVal(aVal)
    {}

    nsSVGString* mVal; // kept alive because it belongs to content

    void GetBaseVal(nsAString & aResult) override
    {
      mVal->GetBaseValue(aResult, mSVGElement);
    }

    void SetBaseVal(const nsAString & aValue) override
    {
      mVal->SetBaseValue(aValue, mSVGElement, true);
    }

    void GetAnimVal(nsAString & aResult) override
    {
      mSVGElement->FlushAnimations();
      mVal->GetAnimValue(aResult, mSVGElement);
    }

  private:
    virtual ~DOMAnimatedString();
  };
  struct SMILString : public nsISMILAttr
  {
  public:
    SMILString(nsSVGString *aVal, nsSVGElement *aSVGElement)
      : mVal(aVal), mSVGElement(aSVGElement) {}

    // These will stay alive because a nsISMILAttr only lives as long
    // as the Compositing step, and DOM elements don't get a chance to
    // die during that.
    nsSVGString* mVal;
    nsSVGElement* mSVGElement;

    // nsISMILAttr methods
    virtual nsresult ValueFromString(const nsAString& aStr,
                                     const mozilla::dom::SVGAnimationElement *aSrcElement,
                                     nsSMILValue& aValue,
                                     bool& aPreventCachingOfSandwich) const override;
    virtual nsSMILValue GetBaseValue() const override;
    virtual void ClearAnimValue() override;
    virtual nsresult SetAnimValue(const nsSMILValue& aValue) override;
  };
};
#endif //__NS_SVGSTRING_H__
