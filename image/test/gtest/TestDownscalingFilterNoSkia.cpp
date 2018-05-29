/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/gfx/2D.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "SourceBuffer.h"
#include "SurfacePipe.h"

// We want to ensure that we're testing the non-Skia fallback version of
// DownscalingFilter, but there are two issues:
//  (1) We don't know whether Skia is currently enabled.
//  (2) If we force disable it, the disabled version will get linked into the
//      binary and will cause the tests in TestDownscalingFilter to fail.
// To avoid these problems, we ensure that MOZ_ENABLE_SKIA is defined when
// including DownscalingFilter.h, and we use the preprocessor to redefine the
// DownscalingFilter class to DownscalingFilterNoSkia.

#define DownscalingFilter DownscalingFilterNoSkia

#ifdef MOZ_ENABLE_SKIA

#undef MOZ_ENABLE_SKIA
#include "Common.h"
#include "DownscalingFilter.h"
#define MOZ_ENABLE_SKIA

#else

#include "Common.h"
#include "DownscalingFilter.h"

#endif

#undef DownscalingFilter

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

TEST(ImageDownscalingFilter, NoSkia)
{
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(bool(decoder));

  // Configuring a DownscalingFilter should fail without Skia.
  AssertConfiguringPipelineFails(decoder,
                                 DownscalingConfig { IntSize(100, 100),
                                                     SurfaceFormat::B8G8R8A8 },
                                 SurfaceConfig { decoder, IntSize(50, 50),
                                                 SurfaceFormat::B8G8R8A8, false });
}
