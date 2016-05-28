#/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_2D_SSSE3_SCALER_H_
#define MOZILLA_GFX_2D_SSSE3_SCALER_H_

#ifdef __cplusplus
extern "C" {
#endif
void ssse3_scale_data(uint32_t *src, int src_width, int src_height,
                int src_stride,
                uint32_t *dest, int dest_width, int dest_height,
                int dest_rowstride,
                int x, int y,
                int width, int height);
#ifdef __cplusplus
}
#endif

#endif // MOZILLA_GFX_2D_SSS3_SCALER_H_
