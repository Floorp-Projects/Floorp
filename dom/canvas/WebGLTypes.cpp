/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTypes.h"

// -

namespace mozilla {

std::string ToString(const WebGLTexelFormat format) {
  switch (format) {
#define _(X) case WebGLTexelFormat::X: return #X;

    _(None)
    _(FormatNotSupportingAnyConversion)
    _(Auto)
    // 1-channel formats
    _(A8)
    _(A16F)
    _(A32F)
    _(R8)
    _(R16F)
    _(R32F)
    // 2-channel formats
    _(RA8)
    _(RA16F)
    _(RA32F)
    _(RG8)
    _(RG16F)
    _(RG32F)
    // 3-channel formats
    _(RGB8)
    _(RGB565)
    _(RGB11F11F10F)
    _(RGB16F)
    _(RGB32F)
    // 4-channel formats
    _(RGBA8)
    _(RGBA5551)
    _(RGBA4444)
    _(RGBA16F)
    _(RGBA32F)
    // DOM element source only formats.
    _(RGBX8)
    _(BGRX8)
    _(BGRA8)
#undef _
  }
  MOZ_ASSERT_UNREACHABLE();
}

}  // namespace mozilla
