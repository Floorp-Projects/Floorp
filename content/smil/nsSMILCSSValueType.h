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

/* representation of a value for a SMIL-animated CSS property */

#ifndef NS_SMILCSSVALUETYPE_H_
#define NS_SMILCSSVALUETYPE_H_

#include "nsISMILType.h"
#include "nsCSSProperty.h"
#include "nscore.h" // For NS_OVERRIDE

class nsAString;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

/*
 * nsSMILCSSValueType: Represents a SMIL-animated CSS value.
 */
class nsSMILCSSValueType : public nsISMILType
{
public:
  typedef mozilla::dom::Element Element;

  // Singleton for nsSMILValue objects to hold onto.
  static nsSMILCSSValueType sSingleton;

protected:
  // nsISMILType Methods
  // -------------------
  NS_OVERRIDE virtual void     Init(nsSMILValue& aValue) const;
  NS_OVERRIDE virtual void     Destroy(nsSMILValue&) const;
  NS_OVERRIDE virtual nsresult Assign(nsSMILValue& aDest,
                                      const nsSMILValue& aSrc) const;
  NS_OVERRIDE virtual bool     IsEqual(const nsSMILValue& aLeft,
                                       const nsSMILValue& aRight) const;
  NS_OVERRIDE virtual nsresult Add(nsSMILValue& aDest,
                                   const nsSMILValue& aValueToAdd,
                                   PRUint32 aCount) const;
  NS_OVERRIDE virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                               const nsSMILValue& aTo,
                                               double& aDistance) const;
  NS_OVERRIDE virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                                           const nsSMILValue& aEndVal,
                                           double aUnitDistance,
                                           nsSMILValue& aResult) const;

public:
  // Helper Methods
  // --------------
  /**
   * Sets up the given nsSMILValue to represent the given string value.  The
   * string is interpreted as a value for the given property on the given
   * element.
   *
   * On failure, this method leaves aValue.mType == nsSMILNullType::sSingleton.
   * Otherwise, this method leaves aValue.mType == this class's singleton.
   *
   * @param       aPropID         The property for which we're parsing a value.
   * @param       aTargetElement  The target element to whom the property/value
   *                              setting applies.
   * @param       aString         The string to be parsed as a CSS value.
   * @param [out] aValue          The nsSMILValue to be populated. Should
   *                              initially be null-typed.
   * @param [out] aIsContextSensitive Set to PR_TRUE if |aString| may produce
   *                                  a different |aValue| depending on other
   *                                  CSS properties on |aTargetElement|
   *                                  or its ancestors (e.g. 'inherit).
   *                                  PR_FALSE otherwise. May be nsnull.
   *                                  Not set if the method fails.
   * @pre  aValue.IsNull()
   * @post aValue.IsNull() || aValue.mType == nsSMILCSSValueType::sSingleton
   */
  static void ValueFromString(nsCSSProperty aPropID,
                              Element* aTargetElement,
                              const nsAString& aString,
                              nsSMILValue& aValue,
                              bool* aIsContextSensitive);

  /**
   * Creates a string representation of the given nsSMILValue.
   *
   * Note: aValue is expected to be of this type (that is, it's expected to
   * have been initialized by nsSMILCSSValueType::sSingleton).  If aValue is a
   * freshly-initialized value, this method will succeed, though the resulting
   * string will be empty.
   *
   * @param       aValue   The nsSMILValue to be converted into a string.
   * @param [out] aString  The string to be populated with the given value.
   * @return               PR_TRUE on success, PR_FALSE on failure.
   */
  static bool ValueToString(const nsSMILValue& aValue, nsAString& aString);

private:
  // Private constructor & destructor: prevent instances beyond my singleton,
  // and prevent others from deleting my singleton.
  nsSMILCSSValueType()  {}
  ~nsSMILCSSValueType() {}
};

#endif // NS_SMILCSSVALUETYPE_H_
