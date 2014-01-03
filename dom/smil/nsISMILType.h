/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_ISMILTYPE_H_
#define NS_ISMILTYPE_H_

#include "mozilla/Attributes.h"
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
// subclasses of nsSMILValue. This is so that sizeof(nsSMILValue) is the same
// for all value types, allowing us to have a type-agnostic nsTArray of
// nsSMILValue objects (actual objects, not pointers). It also allows most
// nsSMILValues (except those that need to allocate extra memory for their
// data) to be allocated on the stack and directly assigned to one another
// provided performance benefits for the animation code.
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
// | -- SandwichAdd?     |     -         |    -?       |    X             |
// | -- ComputeDistance? |     -         |    -        |    X?            |
// | -- Interpolate?     |     -         |    X        |    X             |
// +---------------------+---------------+-------------+------------------+
//

class nsISMILType
{
  /**
   * Only give the nsSMILValue class access to this interface.
   */
  friend class nsSMILValue;

protected:
  /**
   * Initialises aValue and sets it to some identity value such that adding
   * aValue to another value of the same type has no effect.
   *
   * @pre  aValue.IsNull()
   * @post aValue.mType == this
   */
  virtual void Init(nsSMILValue& aValue) const = 0;

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
   * Test two nsSMILValue objects (of this nsISMILType) for equality.
   *
   * A return value of true represents a guarantee that aLeft and aRight are
   * equal. (That is, they would behave identically if passed to the methods
   * Add, SandwichAdd, ComputeDistance, and Interpolate).
   *
   * A return value of false simply indicates that we make no guarantee
   * about equality.
   *
   * NOTE: It's perfectly legal for implementations of this method to return
   * false in all cases.  However, smarter implementations will make this
   * method more useful for optimization.
   *
   * @param   aLeft       The left-hand side of the equality check.
   * @param   aRight      The right-hand side of the equality check.
   * @return  true if we're sure the values are equal, false otherwise.
   *
   * @pre aDest.mType == aSrc.mType == this
   */
  virtual bool IsEqual(const nsSMILValue& aLeft,
                         const nsSMILValue& aRight) const = 0;

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
                       uint32_t aCount) const = 0;

  /**
   * Adds aValueToAdd to the underlying value in the animation sandwich, aDest.
   *
   * For most types this operation is identical to a regular Add() but for some
   * types (notably <animateTransform>) the operation differs. For
   * <animateTransform> Add() corresponds to simply adding together the
   * transform parameters and is used when calculating cumulative values or
   * by-animation values. On the other hand SandwichAdd() is used when adding to
   * the underlying value and requires matrix post-multiplication. (This
   * distinction is most clearly indicated by the SVGT1.2 test suite. It is not
   * obvious within the SMIL specifications.)
   *
   * @param   aDest       The value to add to.
   * @param   aValueToAdd The value to add.
   * @return  NS_OK on success, an error code on failure.
   *
   * @pre aValueToAdd.mType == aDest.mType == this
   */
  virtual nsresult SandwichAdd(nsSMILValue& aDest,
                               const nsSMILValue& aValueToAdd) const
  {
    return Add(aDest, aValueToAdd, 1);
  }

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
   * @return  NS_OK on success, NS_ERROR_FAILURE if this data type cannot be
   *          interpolated or NS_ERROR_OUT_OF_MEMORY if insufficient memory was
   *          available for storing the result.
   *
   * @pre aStartVal.mType == aEndVal.mType == aResult.mType == this
   */
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const = 0;
};

#endif // NS_ISMILTYPE_H_
