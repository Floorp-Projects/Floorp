/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a SMIL-animatable mapped attribute on an element */

#ifndef NS_SMILMAPPEDATTRIBUTE_H_
#define NS_SMILMAPPEDATTRIBUTE_H_

#include "mozilla/Attributes.h"
#include "nsSMILCSSProperty.h"

/* We'll use the empty-string atom |nsGkAtoms::_empty| as the key for storing
 * an element's animated content style rule in its Property Table, under the
 * property-category SMIL_MAPPED_ATTR_ANIMVAL.  Everything else stored in that
 * category is keyed off of the XML attribute name, so the empty string is a
 * good "reserved" key to use for storing the style rule (since XML attributes
 * all have nonempty names).
 */
#define SMIL_MAPPED_ATTR_STYLERULE_ATOM nsGkAtoms::_empty

/**
 * nsSMILMappedAttribute: Implements the nsISMILAttr interface for SMIL
 * animations whose targets are attributes that map to CSS properties.  An
 * instance of this class represents a particular animation-targeted mapped
 * attribute on a particular element.
 */
class nsSMILMappedAttribute : public nsSMILCSSProperty {
public:
  /**
   * Constructs a new nsSMILMappedAttribute.
   *
   * @param  aPropID   The CSS property for the mapped attribute we're
   *                   interested in animating.
   * @param  aElement  The element whose attribute is being animated.
   */
  nsSMILMappedAttribute(nsCSSProperty aPropID, mozilla::dom::Element* aElement) :
    nsSMILCSSProperty(aPropID, aElement) {}

  // nsISMILAttr methods
  virtual nsresult ValueFromString(const nsAString& aStr,
                                   const mozilla::dom::SVGAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const override;
  virtual nsSMILValue GetBaseValue() const override;
  virtual nsresult    SetAnimValue(const nsSMILValue& aValue) override;
  virtual void        ClearAnimValue() override;

protected:
  // Helper Methods
  void FlushChangesToTargetAttr() const;
  already_AddRefed<nsIAtom> GetAttrNameAtom() const;
};
#endif // NS_SMILMAPPEDATTRIBUTE_H_
