/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_SMILFLOATTYPE_H_
#define NS_SMILFLOATTYPE_H_

#include "mozilla/Attributes.h"
#include "nsISMILType.h"

class nsSMILFloatType : public nsISMILType
{
public:
  // Singleton for nsSMILValue objects to hold onto.
  static nsSMILFloatType*
  Singleton()
  {
    static nsSMILFloatType sSingleton;
    return &sSingleton;
  }

protected:
  // nsISMILType Methods
  // -------------------
  virtual void Init(nsSMILValue& aValue) const override;
  virtual void Destroy(nsSMILValue& aValue) const override;
  virtual nsresult Assign(nsSMILValue& aDest, const nsSMILValue& aSrc) const override;
  virtual bool IsEqual(const nsSMILValue& aLeft,
                       const nsSMILValue& aRight) const override;
  virtual nsresult Add(nsSMILValue& aDest, const nsSMILValue& aValueToAdd,
                       uint32_t aCount) const override;
  virtual nsresult ComputeDistance(const nsSMILValue& aFrom,
                                   const nsSMILValue& aTo,
                                   double& aDistance) const override;
  virtual nsresult Interpolate(const nsSMILValue& aStartVal,
                               const nsSMILValue& aEndVal,
                               double aUnitDistance,
                               nsSMILValue& aResult) const override;

private:
  // Private constructor: prevent instances beyond my singleton.
  MOZ_CONSTEXPR nsSMILFloatType() {}
};

#endif // NS_SMILFLOATTYPE_H_
