/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BUFFER_UNROTATE_H
#define MOZILLA_GFX_BUFFER_UNROTATE_H

#include "mozilla/Types.h"

namespace mozilla {
namespace gfx {

void BufferUnrotate(uint8_t* aBuffer, int aByteWidth, int aHeight,
                    int aByteStride, int aXByteBoundary, int aYBoundary);

}  // namespace gfx
}  // namespace mozilla

#endif  // MOZILLA_GFX_BUFFER_UNROTATE_H
