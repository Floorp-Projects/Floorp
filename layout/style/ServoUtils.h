/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* some utilities for stylo */

#ifndef mozilla_ServoUtils_h
#define mozilla_ServoUtils_h

#include "mozilla/Assertions.h"
#include "mozilla/TypeTraits.h"
#include "MainThreadUtils.h"

namespace mozilla {

// Defined in ServoBindings.cpp.
void AssertIsMainThreadOrServoFontMetricsLocked();

class ServoStyleSet;
extern ServoStyleSet* sInServoTraversal;
inline bool IsInServoTraversal()
{
  // The callers of this function are generally main-thread-only _except_
  // for potentially running during the Servo traversal, in which case they may
  // take special paths that avoid writing to caches and the like. In order
  // to allow those callers to branch efficiently without checking TLS, we
  // maintain this static boolean. However, the danger is that those callers
  // are generally unprepared to deal with non-Servo-but-also-non-main-thread
  // callers, and are likely to take the main-thread codepath if this function
  // returns false. So we assert against other non-main-thread callers here.
  MOZ_ASSERT(sInServoTraversal || NS_IsMainThread());
  return sInServoTraversal;
}
} // namespace mozilla

#define MOZ_DECL_STYLO_CONVERT_METHODS_SERVO(servotype_) \
  inline servotype_* AsServo();                         \
  inline const servotype_* AsServo() const;             \
  inline servotype_* GetAsServo();                      \
  inline const servotype_* GetAsServo() const;

#define MOZ_DECL_STYLO_CONVERT_METHODS(geckotype_, servotype_) \
  MOZ_DECL_STYLO_CONVERT_METHODS_SERVO(servotype_)

/**
 * Macro used in a base class of |geckotype_| and |servotype_|.
 * The class should define |StyleBackendType mType;| itself.
 */
#define MOZ_DECL_STYLO_METHODS(geckotype_, servotype_)  \
  MOZ_DECL_STYLO_CONVERT_METHODS(geckotype_, servotype_)

#define MOZ_DEFINE_STYLO_METHODS_SERVO(type_, servotype_) \
  servotype_* type_::AsServo() {                                \
    return static_cast<servotype_*>(this);                      \
  }                                                             \
  const servotype_* type_::AsServo() const {                    \
    return static_cast<const servotype_*>(this);                \
  }                                                             \
  servotype_* type_::GetAsServo() {                             \
    return AsServo();                                           \
  }                                                             \
  const servotype_* type_::GetAsServo() const {                 \
    return AsServo();                                           \
  }


/**
 * Macro used in inline header of class |type_| with its Gecko and Servo
 * subclasses named |geckotype_| and |servotype_| correspondingly for
 * implementing the inline methods defined by MOZ_DECL_STYLO_METHODS.
 */
#define MOZ_DEFINE_STYLO_METHODS(type_, geckotype_, servotype_) \
  MOZ_DEFINE_STYLO_METHODS_SERVO(type_, servotype_)

#define MOZ_STYLO_THIS_TYPE  mozilla::RemovePointer<decltype(this)>::Type
#define MOZ_STYLO_SERVO_TYPE mozilla::RemovePointer<decltype(AsServo())>::Type

/**
 * Macro used to forward a method call to the concrete method defined by
 * the Servo or Gecko implementation. The class of the method using it
 * should use MOZ_DECL_STYLO_METHODS to define basic stylo methods.
 */
#define MOZ_STYLO_FORWARD_CONCRETE(method_, geckoargs_, servoargs_)         \
  return AsServo()->method_ servoargs_;

#define MOZ_STYLO_FORWARD(method_, args_) \
  MOZ_STYLO_FORWARD_CONCRETE(method_, args_, args_)

#endif // mozilla_ServoUtils_h
