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

/* interface for accessing CSS values stored in nsSMILValue objects */

#ifndef NS_ISMILCSSVALUETYPE_H_
#define NS_ISMILCSSVALUETYPE_H_

#include "nsISMILType.h"
#include "nsCSSProperty.h"
#include "nscore.h" // For NS_OVERRIDE

class nsPresContext;
class nsIContent;
class nsAString;

//////////////////////////////////////////////////////////////////////////////
// nsISMILCSSValueType: Customized version of the nsISMILType interface, with
// some additional methods for parsing & extracting CSS values, so that the
// details of the value-storage representation can be abstracted away.

class nsISMILCSSValueType : public nsISMILType
{
public:
  // Methods inherited from nsISMILType
  // ----------------------------------
  NS_OVERRIDE virtual nsresult Init(nsSMILValue& aValue) const = 0;
  NS_OVERRIDE virtual void     Destroy(nsSMILValue& aValue) const = 0;
  NS_OVERRIDE virtual nsresult Assign(nsSMILValue& aDest,
                                      const nsSMILValue& aSrc) const = 0;
  NS_OVERRIDE virtual nsresult Add(nsSMILValue& aDest,
                                   const nsSMILValue& aValueToAdd,
                                   PRUint32 aCount) const = 0;
  NS_OVERRIDE virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                               const nsSMILValue& aTo,
                                               double& aDistance) const = 0;
  NS_OVERRIDE virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                                           const nsSMILValue& aEndVal,
                                           double aUnitDistance,
                                           nsSMILValue& aResult) const = 0;

  /*
   * Virtual destructor: nothing to do here, but subclasses
   * may need it.
   */
  virtual ~nsISMILCSSValueType() {};

  // Methods introduced in this interface
  // ------------------------------------
  // These are helper methods used by nsSMILCSSProperty - these let us keep
  // our data representation private.

  /**
   * Sets up the given nsSMILValue to represent the given string value.  The
   * string is interpreted as a value for the given property on the given
   * element.
   *
   * Note: aValue is expected to be freshly initialized (i.e. it should have
   * been passed into the "Init()" method of some nsISMILCSSValueType subclass)
   *
   * @param       aPropID         The property for which we're parsing a value.
   * @param       aTargetElement  The target element to whom the property/value
   *                              setting applies.
   * @param       aString         The string to be parsed as a CSS value.
   * @param [out] aValue          The nsSMILValue to be populated.
   * @return                      PR_TRUE on success, PR_FALSE on failure.
   */
  virtual PRBool ValueFromString(nsCSSProperty aPropID,
                                 nsIContent* aTargetElement,
                                 const nsAString& aString,
                                 nsSMILValue& aValue) const = 0;

  /**
   * Creates a string representation of the given nsSMILValue.
   *
   * @param       aValue   The nsSMILValue to be converted into a string.
   * @param [out] aString  The string to be populated with the given value.
   * @return               PR_TRUE on success, PR_FALSE on failure.
   */
  virtual PRBool ValueToString(const nsSMILValue& aValue,
                               nsAString& aString) const = 0;
};

#endif //  NS_ISMILCSSVALUETYPE_H_
