// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_LINUX_UTIL_H__
#define BASE_LINUX_UTIL_H__

#include <stdint.h>

namespace base {

// Makes a copy of |pixels| with the ordering changed from BGRA to RGBA.
// The caller is responsible for free()ing the data. If |stride| is 0,
// it's assumed to be 4 * |width|.
uint8_t* BGRAToRGBA(const uint8_t* pixels, int width, int height, int stride);

}  // namespace base

#endif  // BASE_LINUX_UTIL_H__
