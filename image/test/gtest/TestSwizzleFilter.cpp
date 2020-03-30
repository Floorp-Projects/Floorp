/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/gfx/2D.h"
#include "Common.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "SurfaceFilters.h"
#include "SurfacePipe.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

template <typename Func>
void WithSwizzleFilter(const IntSize& aSize, SurfaceFormat aInputFormat,
                       SurfaceFormat aOutputFormat, bool aPremultiplyAlpha,
                       Func aFunc) {
  RefPtr<image::Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  WithFilterPipeline(
      decoder, std::forward<Func>(aFunc),
      SwizzleConfig{aInputFormat, aOutputFormat, aPremultiplyAlpha},
      SurfaceConfig{decoder, aSize, aOutputFormat, false});
}

TEST(ImageSwizzleFilter, WritePixels_RGBA_to_BGRA)
{
  WithSwizzleFilter(
      IntSize(100, 100), SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8,
      false, [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckTransformedWritePixels(aDecoder, aFilter, BGRAColor::Blue(),
                                    BGRAColor::Red());
      });
}

TEST(ImageSwizzleFilter, WritePixels_RGBA_to_Premultiplied_BGRA)
{
  WithSwizzleFilter(
      IntSize(100, 100), SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8A8, true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckTransformedWritePixels(
            aDecoder, aFilter, BGRAColor(0x26, 0x00, 0x00, 0x7F, true),
            BGRAColor(0x00, 0x00, 0x26, 0x7F), Nothing(), Nothing(), Nothing(),
            Nothing(), /* aFuzz */ 1);
      });
}

TEST(ImageSwizzleFilter, WritePixels_RGBA_to_BGRX)
{
  WithSwizzleFilter(
      IntSize(100, 100), SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8,
      false, [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckTransformedWritePixels(aDecoder, aFilter,
                                    BGRAColor(0x26, 0x00, 0x00, 0x7F, true),
                                    BGRAColor(0x00, 0x00, 0x26, 0xFF));
      });
}

TEST(ImageSwizzleFilter, WritePixels_RGBA_to_Premultiplied_BGRX)
{
  WithSwizzleFilter(
      IntSize(100, 100), SurfaceFormat::R8G8B8A8, SurfaceFormat::B8G8R8X8, true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckTransformedWritePixels(aDecoder, aFilter,
                                    BGRAColor(0x26, 0x00, 0x00, 0x7F, true),
                                    BGRAColor(0x00, 0x00, 0x13, 0xFF));
      });
}

TEST(ImageSwizzleFilter, WritePixels_RGBA_to_RGBX)
{
  WithSwizzleFilter(
      IntSize(100, 100), SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8X8,
      false, [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckTransformedWritePixels(aDecoder, aFilter,
                                    BGRAColor(0x00, 0x00, 0x26, 0x7F, true),
                                    BGRAColor(0x00, 0x00, 0x26, 0xFF));
      });
}

TEST(ImageSwizzleFilter, WritePixels_RGBA_to_Premultiplied_RGRX)
{
  WithSwizzleFilter(
      IntSize(100, 100), SurfaceFormat::R8G8B8A8, SurfaceFormat::R8G8B8X8, true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckTransformedWritePixels(aDecoder, aFilter,
                                    BGRAColor(0x00, 0x00, 0x26, 0x7F, true),
                                    BGRAColor(0x00, 0x00, 0x13, 0xFF));
      });
}

TEST(ImageSwizzleFilter, WritePixels_BGRA_to_BGRX)
{
  WithSwizzleFilter(
      IntSize(100, 100), SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8X8,
      false, [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckTransformedWritePixels(aDecoder, aFilter,
                                    BGRAColor(0x10, 0x26, 0x00, 0x7F, true),
                                    BGRAColor(0x10, 0x26, 0x00, 0xFF));
      });
}

TEST(ImageSwizzleFilter, WritePixels_BGRA_to_Premultiplied_BGRA)
{
  WithSwizzleFilter(
      IntSize(100, 100), SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8, true,
      [](image::Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckTransformedWritePixels(
            aDecoder, aFilter, BGRAColor(0x10, 0x26, 0x00, 0x7F, true),
            BGRAColor(0x10, 0x26, 0x00, 0x7F), Nothing(), Nothing(), Nothing(),
            Nothing(), /* aFuzz */ 1);
      });
}
