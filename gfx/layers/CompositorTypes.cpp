/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorTypes.h"

#include <ostream>

namespace mozilla {
namespace layers {

std::ostream& operator<<(std::ostream& aStream, const TextureFlags& aFlags) {
  if (aFlags == TextureFlags::NO_FLAGS) {
    aStream << "NoFlags";
  } else {
#define AppendFlag(test)       \
  {                            \
    if (!!(aFlags & (test))) { \
      if (previous) {          \
        aStream << "|";        \
      }                        \
      aStream << #test;        \
      previous = true;         \
    }                          \
  }
    bool previous = false;
    AppendFlag(TextureFlags::USE_NEAREST_FILTER);
    AppendFlag(TextureFlags::ORIGIN_BOTTOM_LEFT);
    AppendFlag(TextureFlags::DISALLOW_BIGIMAGE);
    AppendFlag(TextureFlags::RB_SWAPPED);
    AppendFlag(TextureFlags::NON_PREMULTIPLIED);
    AppendFlag(TextureFlags::RECYCLE);
    AppendFlag(TextureFlags::DEALLOCATE_CLIENT);
    AppendFlag(TextureFlags::DEALLOCATE_SYNC);
    AppendFlag(TextureFlags::IMMUTABLE);
    AppendFlag(TextureFlags::IMMEDIATE_UPLOAD);
    AppendFlag(TextureFlags::COMPONENT_ALPHA);
    AppendFlag(TextureFlags::INVALID_COMPOSITOR);
    AppendFlag(TextureFlags::RGB_FROM_YCBCR);
    AppendFlag(TextureFlags::SNAPSHOT);
    AppendFlag(TextureFlags::NON_BLOCKING_READ_LOCK);
    AppendFlag(TextureFlags::BLOCKING_READ_LOCK);
    AppendFlag(TextureFlags::WAIT_HOST_USAGE_END);
    AppendFlag(TextureFlags::IS_OPAQUE);
#undef AppendFlag
  }
  return aStream;
}

}  // namespace layers
}  // namespace mozilla
