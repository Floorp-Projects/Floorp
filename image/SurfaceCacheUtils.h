/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_SurfaceCacheUtils_h
#define mozilla_image_SurfaceCacheUtils_h

/**
 * SurfaceCacheUtils provides an ImageLib-external API to interact with
 * ImageLib's SurfaceCache.
 */

namespace mozilla {
namespace image {

class SurfaceCacheUtils
{
public:
  /**
   * Evicts all evictable entries from the surface cache.
   *
   * See the documentation for SurfaceCache::DiscardAll() for the details.
   */
  static void DiscardAll();

private:
  virtual ~SurfaceCacheUtils() = 0;  // Forbid instantiation.
};

} // namespace image
} // namespace mozilla

#endif // mozilla_image_SurfaceCacheUtils_h
