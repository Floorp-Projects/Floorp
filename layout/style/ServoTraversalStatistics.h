/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ServoTraversalStatistics_h
#define mozilla_ServoTraversalStatistics_h

#include <inttypes.h>

namespace mozilla {

// Traversal statistics for Servo traversal.
//
// See style::context::PerThreadTraversalStatistics for
// meaning of these fields.
struct ServoTraversalStatistics {
  uint32_t mElementsTraversed = 0;
  uint32_t mElementsStyled = 0;
  uint32_t mElementsMatched = 0;
  uint32_t mStylesShared = 0;
  uint32_t mStylesReused = 0;

  static bool sActive;
  static ServoTraversalStatistics sSingleton;
};

}  // namespace mozilla

#endif  // mozilla_ServoTraversalStatistics_h
