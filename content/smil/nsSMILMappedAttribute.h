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
 * The Original Code is the Mozilla SMIL module.
 *
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Holbert <dholbert@mozilla.com>
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

/* representation of a SMIL-animatable mapped attribute on an element */

#ifndef NS_SMILMAPPEDATTRIBUTE_H_
#define NS_SMILMAPPEDATTRIBUTE_H_

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
                                   const nsISMILAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const;
  virtual nsSMILValue GetBaseValue() const;
  virtual nsresult    SetAnimValue(const nsSMILValue& aValue);
  virtual void        ClearAnimValue();

protected:
  // Helper Methods
  void FlushChangesToTargetAttr() const;
  already_AddRefed<nsIAtom> GetAttrNameAtom() const;
};
#endif // NS_SMILMAPPEDATTRIBUTE_H_
