/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_BUFFERUNROTATE_H
#define GFX_BUFFERUNROTATE_H

#include "mozilla/Types.h"

void BufferUnrotate(uint8_t* aBuffer, int aByteWidth, int aHeight,
                    int aByteStride, int aXByteBoundary, int aYBoundary);

#endif // GFX_BUFFERUNROTATE_H
