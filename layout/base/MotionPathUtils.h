/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_MotionPathUtils_h
#define mozilla_MotionPathUtils_h

#include "mozilla/gfx/Point.h"
#include "mozilla/Maybe.h"

class nsIFrame;

namespace mozilla {

struct MotionPathData {
  gfx::Point mTranslate;
  float mRotate;
  // The delta value between transform-origin and offset-anchor.
  gfx::Point mShift;
};

// MotionPathUtils is a namespace class containing utility functions related to
// processing motion path in the [motion-1].
// https://drafts.fxtf.org/motion-1/
//
class MotionPathUtils final {
 public:
  /**
   * Generate the motion path transform result.
   **/
  static Maybe<MotionPathData> ResolveMotionPath(const nsIFrame* aFrame);
};

}  // namespace mozilla

#endif  // mozilla_MotionPathUtils_h
