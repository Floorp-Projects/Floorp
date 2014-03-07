/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "2D.h"

namespace mozilla {
namespace gfx {

void
ConvertBGRXToBGRA(uint8_t* aData, const IntSize &aSize, int32_t aStride);

/**
 * Convert aSurface to a packed buffer in BGRA format. The pixel data is
 * returned in a buffer allocated with new uint8_t[].
 */
uint8_t*
SurfaceToPackedBGRA(DataSourceSurface *aSurface);

}
}
