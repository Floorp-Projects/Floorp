/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_SMILValue_h
#define mozilla_SMILValue_h

#include "mozilla/SMILNullType.h"
#include "mozilla/SMILType.h"

namespace mozilla {

/**
 * Although objects of this type are generally only created on the stack and
 * only exist during the taking of a new time sample, that's not always the
 * case. The SMILValue objects obtained from attributes' base values are
 * cached so that the SMIL engine can make certain optimizations during a
 * sample if the base value has not changed since the last sample (potentially
 * avoiding recomposing). These SMILValue objects typically live much longer
 * than a single sample.
 */
class SMILValue {
 public:
  SMILValue() : mU(), mType(SMILNullType::Singleton()) {}
  explicit SMILValue(const SMILType* aType);
  SMILValue(const SMILValue& aVal);

  ~SMILValue() { mType->Destroy(*this); }

  const SMILValue& operator=(const SMILValue& aVal);

  // Move constructor / reassignment operator:
  SMILValue(SMILValue&& aVal) noexcept;
  SMILValue& operator=(SMILValue&& aVal) noexcept;

  // Equality operators. These are allowed to be conservative (return false
  // more than you'd expect) - see comment above SMILType::IsEqual.
  bool operator==(const SMILValue& aVal) const;
  bool operator!=(const SMILValue& aVal) const { return !(*this == aVal); }

  bool IsNull() const { return (mType == SMILNullType::Singleton()); }

  nsresult Add(const SMILValue& aValueToAdd, uint32_t aCount = 1);
  nsresult SandwichAdd(const SMILValue& aValueToAdd);
  nsresult ComputeDistance(const SMILValue& aTo, double& aDistance) const;
  nsresult Interpolate(const SMILValue& aEndVal, double aUnitDistance,
                       SMILValue& aResult) const;

  union {
    bool mBool;
    uint64_t mUint;
    int64_t mInt;
    double mDouble;
    struct {
      float mAngle;
      uint16_t mUnit;
      uint16_t mOrientType;
    } mOrient;
    int32_t mIntPair[2];
    float mNumberPair[2];
    void* mPtr;
  } mU;
  const SMILType* mType;

 protected:
  void InitAndCheckPostcondition(const SMILType* aNewType);
  void DestroyAndCheckPostcondition();
  void DestroyAndReinit(const SMILType* aNewType);
};

}  // namespace mozilla

#endif  // mozilla_SMILValue_h
