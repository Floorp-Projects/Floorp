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

#endif // mozilla_ServoUtils_h
