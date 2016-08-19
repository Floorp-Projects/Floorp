/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_PlaybackType_h
#define mozilla_image_PlaybackType_h

#include "imgIContainer.h"

namespace mozilla {
namespace image {

/**
 * PlaybackType identifies a surface cache entry as either a static surface or
 * an animation. Note that a specific cache entry is one or the other, but
 * images may be associated with both types of cache entries, since in some
 * circumstances we may want to treat an animated image as if it were static.
 */
enum class PlaybackType : uint8_t
{
  eStatic,   // Calls to DrawableRef() will always return the same surface.
  eAnimated  // An animation; calls to DrawableRef() may return different
             // surfaces at different times.
};

/**
 * Given an imgIContainer FRAME_* value, returns the corresponding PlaybackType
 * for use in surface cache lookups.
 */
inline PlaybackType
ToPlaybackType(uint32_t aWhichFrame)
{
  MOZ_ASSERT(aWhichFrame == imgIContainer::FRAME_FIRST ||
             aWhichFrame == imgIContainer::FRAME_CURRENT);
  return aWhichFrame == imgIContainer::FRAME_CURRENT
       ? PlaybackType::eAnimated
       : PlaybackType::eStatic;
}

} // namespace image
} // namespace mozilla

#endif // mozilla_image_PlaybackType_h
