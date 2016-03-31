/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoBindingHelpers_h
#define mozilla_ServoBindingHelpers_h

#include "mozilla/RefPtr.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

template<>
struct RefPtrTraits<RawServoStyleSheet>
{
  static void AddRef(RawServoStyleSheet* aPtr) {
    Servo_AddRefStyleSheet(aPtr);
  }
  static void Release(RawServoStyleSheet* aPtr) {
    Servo_ReleaseStyleSheet(aPtr);
  }
};

template<>
struct RefPtrTraits<ServoComputedValues>
{
  static void AddRef(ServoComputedValues* aPtr) {
    Servo_AddRefComputedValues(aPtr);
  }
  static void Release(ServoComputedValues* aPtr) {
    Servo_ReleaseComputedValues(aPtr);
  }
};

template<>
class DefaultDelete<RawServoStyleSet>
{
public:
  void operator()(RawServoStyleSet* aPtr) const
  {
    Servo_DropStyleSet(aPtr);
  }
};

} // namespace mozilla

#endif // mozilla_ServoBindingHelpers_h
