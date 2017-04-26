/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* representation of a value for a SMIL-animated CSS property */

#ifndef NS_SMILCSSVALUETYPE_H_
#define NS_SMILCSSVALUETYPE_H_

#include "nsISMILType.h"
#include "nsCSSPropertyID.h"
#include "mozilla/Attributes.h"

class nsAString;

namespace mozilla {
struct AnimationValue;
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
  typedef mozilla::AnimationValue AnimationValue;

  // Singleton for nsSMILValue objects to hold onto.
  static nsSMILCSSValueType sSingleton;

protected:
  // nsISMILType Methods
  // -------------------
  virtual void     Init(nsSMILValue& aValue) const override;
  virtual void     Destroy(nsSMILValue&) const override;
  virtual nsresult Assign(nsSMILValue& aDest,
                          const nsSMILValue& aSrc) const override;
  virtual bool     IsEqual(const nsSMILValue& aLeft,
                           const nsSMILValue& aRight) const override;
  virtual nsresult Add(nsSMILValue& aDest,
                       const nsSMILValue& aValueToAdd,
                       uint32_t aCount) const override;
  virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const override;
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const override;

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
   * @param [out] aIsContextSensitive Set to true if |aString| may produce
   *                                  a different |aValue| depending on other
   *                                  CSS properties on |aTargetElement|
   *                                  or its ancestors (e.g. 'inherit).
   *                                  false otherwise. May be nullptr.
   *                                  Not set if the method fails.
   * @pre  aValue.IsNull()
   * @post aValue.IsNull() || aValue.mType == nsSMILCSSValueType::sSingleton
   */
  static void ValueFromString(nsCSSPropertyID aPropID,
                              Element* aTargetElement,
                              const nsAString& aString,
                              nsSMILValue& aValue,
                              bool* aIsContextSensitive);

  /**
   * Creates an nsSMILValue to wrap the given animation value.
   *
   * @param aPropID         The property that |aValue| corresponds to.
   * @param aTargetElement  The target element to which the animation value
   *                        applies.
   * @param aValue          The animation value to use.
   * @return                A new nsSMILValue. On failure, returns an
   *                        nsSMILValue with the null type (i.e. rv.IsNull()
   *                        returns true).
   */
  static nsSMILValue ValueFromAnimationValue(nsCSSPropertyID aPropID,
                                             Element* aTargetElement,
                                             const AnimationValue& aValue);

  /**
   * Creates a string representation of the given nsSMILValue.
   *
   * Note: aValue is expected to be of this type (that is, it's expected to
   * have been initialized by nsSMILCSSValueType::sSingleton).  If aValue is a
   * freshly-initialized value the resulting string will be empty.
   *
   * @param       aValue   The nsSMILValue to be converted into a string.
   * @param [out] aString  The string to be populated with the given value.
   */
  static void ValueToString(const nsSMILValue& aValue, nsAString& aString);

  /**
   * Return the CSS property animated by the specified value.
   *
   * @param   aValue   The nsSMILValue to examine.
   * @return           The nsCSSPropertyID enum value of the property animated
   *                   by |aValue|, or eCSSProperty_UNKNOWN if the type of
   *                   |aValue| is not nsSMILCSSValueType.
   */
  static nsCSSPropertyID PropertyFromValue(const nsSMILValue& aValue);

private:
  // Private constructor: prevent instances beyond my singleton.
  constexpr nsSMILCSSValueType() {}
};

#endif // NS_SMILCSSVALUETYPE_H_
