/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Range.h"
#include "mozilla/gfx/2D.h"
#include "WebRenderTypes.h"

namespace mozilla {
namespace wr {

static bool Moz2DRenderCallback(const Range<uint8_t> aBlob,
                                gfx::IntSize aSize,
                                gfx::SurfaceFormat aFormat,
                                Range<uint8_t> output)
{
  return false; // TODO(nical)
}

} // namespace
} // namespace

extern "C" {

bool wr_moz2d_render_cb(const WrByteSlice blob,
                        uint32_t width, uint32_t height,
                        mozilla::wr::ImageFormat aFormat,
                        WrByteSlice output)
{
  return mozilla::wr::Moz2DRenderCallback(mozilla::wr::ByteSliceToRange(blob),
                                          mozilla::gfx::IntSize(width, height),
                                          mozilla::wr::WrImageFormatToSurfaceFormat(aFormat),
                                          mozilla::wr::ByteSliceToRange(output));
}

} // extern


