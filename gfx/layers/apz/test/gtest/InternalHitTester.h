/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_InternalHitTester_h
#define mozilla_layers_InternalHitTester_h

#include "apz/src/IAPZHitTester.h"

namespace mozilla {
namespace layers {

// IAPZHitTester implementation for the internal APZ hit test.
class InternalHitTester : public IAPZHitTester {
 public:
  virtual HitTestResult GetAPZCAtPoint(
      const ScreenPoint& aHitTestPoint,
      const RecursiveMutexAutoLock& aProofOfTreeLock) override;
};

}  // namespace layers
}  // namespace mozilla

#endif  // define mozilla_layers_InternalHitTester_h
