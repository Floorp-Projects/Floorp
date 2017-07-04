/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxUtils.h"
#include "mozilla/Range.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/InlineTranslator.h"
#include "mozilla/gfx/RecordedEvent.h"
#include "WebRenderTypes.h"

#include <iostream>

#ifdef MOZ_ENABLE_FREETYPE
#include "mozilla/ThreadLocal.h"
#endif

namespace mozilla {
namespace wr {

#ifdef MOZ_ENABLE_FREETYPE
static MOZ_THREAD_LOCAL(FT_Library) sFTLibrary;
#endif

static bool Moz2DRenderCallback(const Range<const uint8_t> aBlob,
                                gfx::IntSize aSize,
                                gfx::SurfaceFormat aFormat,
                                Range<uint8_t> aOutput)
{
  MOZ_ASSERT(aSize.width > 0 && aSize.height > 0);
  if (aSize.width <= 0 || aSize.height <= 0) {
    return false;
  }

  auto stride = aSize.width * gfx::BytesPerPixel(aFormat);

  if (aOutput.length() < static_cast<size_t>(aSize.height * stride)) {
    return false;
  }

  void* fontContext = nullptr;
#ifdef MOZ_ENABLE_FREETYPE
  if (!sFTLibrary.init()) {
    return false;
  }
  if (!sFTLibrary.get()) {
    FT_Library library = gfx::Factory::NewFTLibrary();
    if (!library) {
      return false;
    }
    sFTLibrary.set(library);
  }
  fontContext = sFTLibrary.get();
#endif

  // In bindings.rs we allocate a buffer filled with opaque white.
  bool uninitialized = false;

  RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateDrawTargetForData(
    gfx::BackendType::SKIA,
    aOutput.begin().get(),
    aSize,
    stride,
    aFormat,
    uninitialized
  );

  if (!dt) {
    return false;
  }

  gfx::InlineTranslator translator(dt, fontContext);

  auto ret = translator.TranslateRecording((char*)aBlob.begin().get(), aBlob.length());

#if 0
  static int i = 0;
  char filename[40];
  sprintf(filename, "out%d.png", i++);
  gfxUtils::WriteAsPNG(dt, filename);
#endif

  return ret;
}

} // namespace
} // namespace

extern "C" {

bool wr_moz2d_render_cb(const WrByteSlice blob,
                        uint32_t width, uint32_t height,
                        mozilla::wr::ImageFormat aFormat,
                        MutByteSlice output)
{
  return mozilla::wr::Moz2DRenderCallback(mozilla::wr::ByteSliceToRange(blob),
                                          mozilla::gfx::IntSize(width, height),
                                          mozilla::wr::WrImageFormatToSurfaceFormat(aFormat),
                                          mozilla::wr::MutByteSliceToRange(output));
}

} // extern


