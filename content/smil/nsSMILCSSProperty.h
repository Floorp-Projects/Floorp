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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

/* representation of a SMIL-animatable CSS property on an element */

#ifndef NS_SMILCSSPROPERTY_H_
#define NS_SMILCSSPROPERTY_H_

#include "nsISMILAttr.h"
#include "nsIAtom.h"
#include "nsCSSProperty.h"
#include "nsCSSValue.h"

class nsIContent;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

/**
 * nsSMILCSSProperty: Implements the nsISMILAttr interface for SMIL animations
 * that target CSS properties.  Represents a particular animation-targeted CSS
 * property on a particular element.
 */
class nsSMILCSSProperty : public nsISMILAttr
{
public:
  /**
   * Constructs a new nsSMILCSSProperty.
   * @param  aPropID   The CSS property we're interested in animating.
   * @param  aElement  The element whose CSS property is being animated.
   */
  nsSMILCSSProperty(nsCSSProperty aPropID, mozilla::dom::Element* aElement);

  // nsISMILAttr methods
  virtual nsresult ValueFromString(const nsAString& aStr,
                                   const nsISMILAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const;
  virtual nsSMILValue GetBaseValue() const;
  virtual nsresult    SetAnimValue(const nsSMILValue& aValue);
  virtual void        ClearAnimValue();

  /**
   * Utility method - returns PR_TRUE if the given property is supported for
   * SMIL animation.
   *
   * @param   aProperty  The property to check for animation support.
   * @return  PR_TRUE if the given property is supported for SMIL animation, or
   *          PR_FALSE otherwise
   */
  static bool IsPropertyAnimatable(nsCSSProperty aPropID);

protected:
  nsCSSProperty mPropID;
  // Using non-refcounted pointer for mElement -- we know mElement will stay
  // alive for my lifetime because a nsISMILAttr (like me) only lives as long
  // as the Compositing step, and DOM elements don't get a chance to die during
  // that time.
  mozilla::dom::Element*   mElement;
};

#endif // NS_SMILCSSPROPERTY_H_
