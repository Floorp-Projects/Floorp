/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_AnimationParams_h
#define mozilla_image_AnimationParams_h

#include <stdint.h>
#include "mozilla/gfx/Rect.h"
#include "FrameTimeout.h"

namespace mozilla {
namespace image {

enum class BlendMethod : int8_t {
  // All color components of the frame, including alpha, overwrite the current
  // contents of the frame's output buffer region.
  SOURCE,

  // The frame should be composited onto the output buffer based on its alpha,
  // using a simple OVER operation.
  OVER
};

enum class DisposalMethod : int8_t {
  CLEAR_ALL = -1,  // Clear the whole image, revealing what's underneath.
  NOT_SPECIFIED,   // Leave the frame and let the new frame draw on top.
  KEEP,            // Leave the frame and let the new frame draw on top.
  CLEAR,           // Clear the frame's area, revealing what's underneath.
  RESTORE_PREVIOUS // Restore the previous (composited) frame.
};

struct AnimationParams
{
  gfx::IntRect mBlendRect;
  FrameTimeout mTimeout;
  uint32_t mFrameNum;
  BlendMethod mBlendMethod;
  DisposalMethod mDisposalMethod;
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_AnimationParams_h
