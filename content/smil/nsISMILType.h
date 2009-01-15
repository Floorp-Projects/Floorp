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
 *   Robert O'Callahan <roc+moz@cs.cmu.edu>
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

#ifndef NS_ISMILTYPE_H_
#define NS_ISMILTYPE_H_

#include "nscore.h"

class nsSMILValue;

//////////////////////////////////////////////////////////////////////////////
// nsISMILType: Interface for defining the basic operations needed for animating
// a particular kind of data (e.g. lengths, colors, transformation matrices).
//
// This interface is never used directly but always through an nsSMILValue that
// bundles together a pointer to a concrete implementation of this interface and
// the data upon which it should operate.
//
// We keep the data and type separate rather than just providing different
// subclasses of nsSMILValue as this allows nsSMILValues to be allocated on the
// stack and directly assigned to one another provided performance benefits for
// the animation code.
//
// Note that different types have different capabilities. Roughly speaking there
// are probably three main types:
//
// +---------------------+---------------+-------------+------------------+
// | CATEGORY:           | DISCRETE      | LINEAR      | ADDITIVE         |
// +---------------------+---------------+-------------+------------------+
// | Example:            | strings,      | path data?  | lengths,         |
// |                     | color k/words?|             | RGB color values |
// |                     |               |             |                  |
// | -- Assign?          |     X         |    X        |    X             |
// | -- Add?             |     -         |    X?       |    X             |
// | -- ComputeDistance? |     -         |    -        |    X?            |
// | -- Interpolate?     |     -         |    X        |    X             |
// +---------------------+---------------+-------------+------------------+
//

class nsISMILType
{
public:
  /**
   * Initialises aValue and sets it to some identity value such that adding
   * aValue to another value of the same type has no effect.
   *
   * @pre (aValue.mType == this && aValue.mU is valid)
   *      || aValue.mType == null-type
   * @post aValue.mType == this || NS_FAILED(rv)
   */
  virtual nsresult Init(nsSMILValue& aValue) const = 0;

  /**
   * Destroys any data associated with a value of this type.
   *
   * @pre  aValue.mType == this
   * @post aValue.IsNull()
   */
  virtual void Destroy(nsSMILValue& aValue) const = 0;

  /**
   * Assign this object the value of another. Think of this as the assignment
   * operator.
   *
   * @param   aDest       The left-hand side of the assignment.
   * @param   aSrc        The right-hand side of the assignment.
   * @return  NS_OK on success, an error code on failure such as when the
   *          underlying type of the specified object differs.
   *
   * @pre aDest.mType == aSrc.mType == this
   */
  virtual nsresult Assign(nsSMILValue& aDest,
                          const nsSMILValue& aSrc) const = 0;

  /**
   * Adds two values.
   *
   * The count parameter facilitates repetition.
   *
   * By equation,
   *
   *   aDest += aValueToAdd * aCount
   *
   * Therefore, if aCount == 0, aDest will be unaltered.
   *
   * This method will fail if this data type is not additive or the value was
   * not specified using an additive syntax.
   *
   * See SVG 1.1, section 19.2.5. In particular,
   *
   *   "If a given attribute or property can take values of keywords (which are
   *   not additive) or numeric values (which are additive), then additive
   *   animations are possible if the subsequent animation uses a numeric value
   *   even if the base animation uses a keyword value; however, if the
   *   subsequent animation uses a keyword value, additive animation is not
   *   possible."
   *
   * If this method fails (e.g. because the data type is not additive), aDest
   * will be unaltered.
   *
   * @param   aDest       The value to add to.
   * @param   aValueToAdd The value to add.
   * @param   aCount      The number of times to add aValueToAdd.
   * @return  NS_OK on success, an error code on failure.
   *
   * @pre aValueToAdd.mType == aDest.mType == this
   */
  virtual nsresult Add(nsSMILValue& aDest,
                       const nsSMILValue& aValueToAdd,
                       PRUint32 aCount) const = 0;

  /**
   * Calculates the 'distance' between two values. This is the distance used in
   * paced interpolation.
   *
   * @param   aFrom     The start of the interval for which the distance should
   *                    be calculated.
   * @param   aTo       The end of the interval for which the distance should be
   *                    calculated.
   * @param   aDistance The result of the calculation.
   * @return  NS_OK on success, or an appropriate error code if there is no
   *          notion of distance for the underlying data type or the distance
   *          could not be calculated.
   *
   * @pre aFrom.mType == aTo.mType == this
   */
  virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const = 0;

  /**
   * Calculates an interpolated value between two values using the specified
   * proportion.
   *
   * @param   aStartVal     The value defining the start of the interval of
   *                        interpolation.
   * @param   aEndVal       The value defining the end of the interval of
   *                        interpolation.
   * @param   aUnitDistance A number between 0.0 and 1.0 (inclusive) defining
   *                        the distance of the interpolated value in the
   *                        interval.
   * @param   aResult       The interpolated value.
   * @result  NS_OK on success, NS_ERROR_FAILURE if this data type cannot be
   *          interpolated or NS_ERROR_OUT_OF_MEMORY if insufficient memory was
   *          available for storing the result.
   *
   * @pre aStartVal.mType == aEndVal.mType == aResult.mType == this
   */
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const = 0;

  /*
   * Virtual destructor: Nothing to do here, but subclasses
   * may need it.
   */
  virtual ~nsISMILType() {};
};

#endif // NS_ISMILTYPE_H_
