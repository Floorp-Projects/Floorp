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

#define DEFINE_REFPTR_TRAITS(name_, type_)  \
  template<>                                \
  struct RefPtrTraits<type_>                \
  {                                         \
    static void AddRef(type_* aPtr)         \
    {                                       \
      Servo_##name_##_AddRef(aPtr);         \
    }                                       \
    static void Release(type_* aPtr)        \
    {                                       \
      Servo_##name_##_Release(aPtr);        \
    }                                       \
  }

DEFINE_REFPTR_TRAITS(StyleSheet, RawServoStyleSheet);
DEFINE_REFPTR_TRAITS(ComputedValues, ServoComputedValues);
DEFINE_REFPTR_TRAITS(DeclarationBlock, ServoDeclarationBlock);

#undef DEFINE_REFPTR_TRAITS

template<>
class DefaultDelete<RawServoStyleSet>
{
public:
  void operator()(RawServoStyleSet* aPtr) const
  {
    Servo_StyleSet_Drop(aPtr);
  }
};

} // namespace mozilla

#endif // mozilla_ServoBindingHelpers_h
