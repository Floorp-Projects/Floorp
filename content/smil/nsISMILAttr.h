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
 * The Initial Developer of the Original Code is Brian Birtles.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Birtles <birtles@gmail.com>
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

#ifndef NS_ISMILATTR_H_
#define NS_ISMILATTR_H_

#include "nsStringFwd.h"

class nsSMILValue;
class nsISMILType;
class nsISMILAnimationElement;
class nsIContent;

////////////////////////////////////////////////////////////////////////
// nsISMILAttr: A variable targeted by SMIL for animation and can therefore have
// an underlying (base) value and an animated value For example, an attribute of
// a particular SVG element.
//
// These objects only exist during the compositing phase of SMIL animation
// calculations. They have a single owner who is responsible for deleting the
// object.

class nsISMILAttr
{
public:
  /**
   * Creates a new nsSMILValue for this attribute from a string. The string is
   * parsed in the context of this attribute so that context-dependent values
   * such as em-based units can be resolved into a canonical form suitable for
   * animation (including interpolation etc.).
   *
   * @param aStr        A string defining the new value to be created.
   * @param aSrcElement The source animation element. This may be needed to
   *                    provided additional context data such as for
   *                    animateTransform where the 'type' attribute is needed to
   *                    parse the value.
   * @param[out] aValue Outparam for storing the parsed value.
   * @param[out] aPreventCachingOfSandwich
   *                    Outparam to indicate whether the attribute contains
   *                    dependencies on its context that should prevent the
   *                    result of the animation sandwich from being cached and
   *                    reused in future samples.
   * @return NS_OK on success or an error code if creation failed.
   */
  virtual nsresult ValueFromString(const nsAString& aStr,
                                   const nsISMILAnimationElement* aSrcElement,
                                   nsSMILValue& aValue,
                                   bool& aPreventCachingOfSandwich) const = 0;

  /**
   * Gets the underlying value of this attribute.
   *
   * @return an nsSMILValue object. returned_object.IsNull() will be true if an
   * error occurred.
   */
  virtual nsSMILValue GetBaseValue() const = 0;

  /**
   * Clears the animated value of this attribute.
   *
   * NOTE: The animation target is not guaranteed to be in a document when this
   * method is called. (See bug 523188)
   */
  virtual void ClearAnimValue() = 0;

  /**
   * Sets the presentation value of this attribute.
   *
   * @param aValue  The value to set.
   * @return NS_OK on success or an error code if setting failed.
   */
  virtual nsresult SetAnimValue(const nsSMILValue& aValue) = 0;

  /**
   * Returns the targeted content node, for any nsISMILAttr implementations
   * that want to expose that to the animation logic.  Otherwise, returns
   * null.
   *
   * @return the targeted content node, if this nsISMILAttr implementation
   * wishes to make it avaiable.  Otherwise, nsnull.
   */
  virtual const nsIContent* GetTargetNode() const { return nsnull; }

  /**
   * Virtual destructor, to make sure subclasses can clean themselves up.
   */
  virtual ~nsISMILAttr() {}
};

#endif // NS_ISMILATTR_H_
