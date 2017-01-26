/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoAnimationRule_h
#define mozilla_ServoAnimationRule_h

#include "nsCSSPropertyID.h"
#include "nsHashKeys.h" // For nsUint32HashKey
#include "nsRefPtrHashtable.h"
#include "ServoBindings.h"

namespace mozilla {

/**
 * A rule for Stylo Animation Rule.
 */
class ServoAnimationRule
{
public:
  ServoAnimationRule() = default;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ServoAnimationRule)

  void AddValue(nsCSSPropertyID aProperty,
                RawServoAnimationValue* aValue);
  MOZ_MUST_USE RawServoDeclarationBlockStrong GetValues() const;

private:
  ~ServoAnimationRule() = default;

  nsRefPtrHashtable<nsUint32HashKey, RawServoAnimationValue> mAnimationValues;
};

} // namespace mozilla

#endif // mozilla_ServoAnimationRule_h
