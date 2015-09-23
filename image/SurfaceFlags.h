/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_SurfaceFlags_h
#define mozilla_image_SurfaceFlags_h

#include "imgIContainer.h"
#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace image {

/**
 * Flags that change the output a decoder generates. Because different
 * combinations of these flags result in logically different surfaces, these
 * flags must be taken into account in SurfaceCache lookups.
 */
enum class SurfaceFlags : uint8_t
{
  NO_PREMULTIPLY_ALPHA     = 1 << 0,
  NO_COLORSPACE_CONVERSION = 1 << 1
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(SurfaceFlags)

/**
 * @return the default set of surface flags.
 */
inline SurfaceFlags
DefaultSurfaceFlags()
{
  return SurfaceFlags();
}

/**
 * Given a set of imgIContainer FLAG_* flags, returns a set of SurfaceFlags with
 * the corresponding flags set.
 */
inline SurfaceFlags
ToSurfaceFlags(uint32_t aFlags)
{
  SurfaceFlags flags = DefaultSurfaceFlags();
  if (aFlags & imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA) {
    flags |= SurfaceFlags::NO_PREMULTIPLY_ALPHA;
  }
  if (aFlags & imgIContainer::FLAG_DECODE_NO_COLORSPACE_CONVERSION) {
    flags |= SurfaceFlags::NO_COLORSPACE_CONVERSION;
  }
  return flags;
}

/**
 * Given a set of SurfaceFlags, returns a set of imgIContainer FLAG_* flags with
 * the corresponding flags set.
 */
inline uint32_t
FromSurfaceFlags(SurfaceFlags aFlags)
{
  uint32_t flags = imgIContainer::DECODE_FLAGS_DEFAULT;
  if (aFlags & SurfaceFlags::NO_PREMULTIPLY_ALPHA) {
    flags |= imgIContainer::FLAG_DECODE_NO_PREMULTIPLY_ALPHA;
  }
  if (aFlags & SurfaceFlags::NO_COLORSPACE_CONVERSION) {
    flags |= imgIContainer::FLAG_DECODE_NO_COLORSPACE_CONVERSION;
  }
  return flags;
}

} // namespace image
} // namespace mozilla

#endif // mozilla_image_SurfaceFlags_h
