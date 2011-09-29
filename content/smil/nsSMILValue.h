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
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef NS_SMILVALUE_H_
#define NS_SMILVALUE_H_

#include "nsISMILType.h"
#include "nsSMILNullType.h"

/**
 * Although objects of this type are generally only created on the stack and
 * only exist during the taking of a new time sample, that's not always the
 * case. The nsSMILValue objects obtained from attributes' base values are
 * cached so that the SMIL engine can make certain optimizations during a
 * sample if the base value has not changed since the last sample (potentially
 * avoiding recomposing). These nsSMILValue objects typically live much longer
 * than a single sample.
 */
class nsSMILValue
{
public:
  nsSMILValue() : mU(), mType(&nsSMILNullType::sSingleton) { }
  explicit nsSMILValue(const nsISMILType* aType);
  nsSMILValue(const nsSMILValue& aVal);

  ~nsSMILValue()
  {
    mType->Destroy(*this);
  }

  const nsSMILValue& operator=(const nsSMILValue& aVal);

  // Equality operators. These are allowed to be conservative (return PR_FALSE
  // more than you'd expect) - see comment above nsISMILType::IsEqual.
  bool operator==(const nsSMILValue& aVal) const;
  bool operator!=(const nsSMILValue& aVal) const {
    return !(*this == aVal);
  }

  bool IsNull() const
  {
    return (mType == &nsSMILNullType::sSingleton);
  }

  // Swaps the member data (mU & mPtr) of |this| with |aOther|
  void     Swap(nsSMILValue& aOther);

  nsresult Add(const nsSMILValue& aValueToAdd, PRUint32 aCount = 1);
  nsresult SandwichAdd(const nsSMILValue& aValueToAdd);
  nsresult ComputeDistance(const nsSMILValue& aTo, double& aDistance) const;
  nsresult Interpolate(const nsSMILValue& aEndVal,
                       double aUnitDistance,
                       nsSMILValue& aResult) const;

  union {
    bool mBool;
    PRUint64 mUint;
    PRInt64 mInt;
    double mDouble;
    struct {
      float mAngle;
      PRUint16 mUnit;
      PRUint16 mOrientType;
    } mOrient;
    PRInt32 mIntPair[2];
    float mNumberPair[2];
    void* mPtr;
  } mU;
  const nsISMILType* mType;

protected:
  void InitAndCheckPostcondition(const nsISMILType* aNewType);
  void DestroyAndCheckPostcondition();
  void DestroyAndReinit(const nsISMILType* aNewType);
};

#endif  // NS_SMILVALUE_H_
