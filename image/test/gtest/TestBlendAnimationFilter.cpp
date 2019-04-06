/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/gfx/2D.h"
#include "skia/include/core/SkColorPriv.h"  // for SkPMSrcOver
#include "Common.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "SourceBuffer.h"
#include "SurfaceFilters.h"
#include "SurfacePipe.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

static already_AddRefed<Decoder> CreateTrivialBlendingDecoder() {
  gfxPrefs::GetSingleton();
  DecoderType decoderType = DecoderFactory::GetDecoderType("image/gif");
  DecoderFlags decoderFlags = DefaultDecoderFlags();
  SurfaceFlags surfaceFlags = DefaultSurfaceFlags();
  auto sourceBuffer = MakeNotNull<RefPtr<SourceBuffer>>();
  return DecoderFactory::CreateAnonymousDecoder(
      decoderType, sourceBuffer, Nothing(), decoderFlags, surfaceFlags);
}

template <typename Func>
RawAccessFrameRef WithBlendAnimationFilter(Decoder* aDecoder,
                                           const AnimationParams& aAnimParams,
                                           const IntSize& aOutputSize,
                                           Func aFunc) {
  DecoderTestHelper decoderHelper(aDecoder);

  if (!aDecoder->HasAnimation()) {
    decoderHelper.PostIsAnimated(aAnimParams.mTimeout);
  }

  BlendAnimationConfig blendAnim{aDecoder};
  SurfaceConfig surfaceSink{aDecoder, aOutputSize, SurfaceFormat::B8G8R8A8,
                            false, Some(aAnimParams)};

  auto func = [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
    aFunc(aDecoder, aFilter);
  };

  WithFilterPipeline(aDecoder, func, false, blendAnim, surfaceSink);

  RawAccessFrameRef current = aDecoder->GetCurrentFrameRef();
  if (current) {
    decoderHelper.PostFrameStop(Opacity::SOME_TRANSPARENCY);
  }

  return current;
}

void AssertConfiguringBlendAnimationFilterFails(const IntRect& aFrameRect,
                                                const IntSize& aOutputSize) {
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams animParams{aFrameRect, FrameTimeout::FromRawMilliseconds(0),
                             0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  BlendAnimationConfig blendAnim{decoder};
  SurfaceConfig surfaceSink{decoder, aOutputSize, SurfaceFormat::B8G8R8A8,
                            false, Some(animParams)};
  AssertConfiguringPipelineFails(decoder, blendAnim, surfaceSink);
}

TEST(ImageBlendAnimationFilter, BlendFailsForNegativeFrameRect)
{
  // A negative frame rect size is disallowed.
  AssertConfiguringBlendAnimationFilterFails(
      IntRect(IntPoint(0, 0), IntSize(-1, -1)), IntSize(100, 100));
}

TEST(ImageBlendAnimationFilter, WriteFullFirstFrame)
{
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams params{
      IntRect(0, 0, 100, 100), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  RawAccessFrameRef frame0 = WithBlendAnimationFilter(
      decoder, params, IntSize(100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter, Some(IntRect(0, 0, 100, 100)));
      });
  EXPECT_EQ(IntRect(0, 0, 100, 100), frame0->GetDirtyRect());
}

TEST(ImageBlendAnimationFilter, WritePartialFirstFrame)
{
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams params{
      IntRect(25, 50, 50, 25), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  RawAccessFrameRef frame0 = WithBlendAnimationFilter(
      decoder, params, IntSize(100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter, Some(IntRect(0, 0, 100, 100)),
                         Nothing(), Some(IntRect(25, 50, 50, 25)),
                         Some(IntRect(25, 50, 50, 25)));
      });
  EXPECT_EQ(IntRect(0, 0, 100, 100), frame0->GetDirtyRect());
}

static void TestWithBlendAnimationFilterClear(BlendMethod aBlendMethod) {
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams params0{
      IntRect(0, 0, 100, 100), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  RawAccessFrameRef frame0 = WithBlendAnimationFilter(
      decoder, params0, IntSize(100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(BGRAColor::Green().AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 100), frame0->GetDirtyRect());

  AnimationParams params1{
      IntRect(0, 40, 100, 20), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 1, BlendMethod::SOURCE, DisposalMethod::CLEAR};
  RawAccessFrameRef frame1 = WithBlendAnimationFilter(
      decoder, params1, IntSize(100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(BGRAColor::Red().AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 40, 100, 20), frame1->GetDirtyRect());

  ASSERT_TRUE(frame1.get() != nullptr);

  RefPtr<SourceSurface> surface = frame1->GetSourceSurface();
  EXPECT_TRUE(RowsAreSolidColor(surface, 0, 40, BGRAColor::Green()));
  EXPECT_TRUE(RowsAreSolidColor(surface, 40, 20, BGRAColor::Red()));
  EXPECT_TRUE(RowsAreSolidColor(surface, 60, 40, BGRAColor::Green()));

  AnimationParams params2{
      IntRect(0, 50, 100, 20), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 2, aBlendMethod, DisposalMethod::KEEP};
  RawAccessFrameRef frame2 = WithBlendAnimationFilter(
      decoder, params2, IntSize(100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(BGRAColor::Blue().AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });

  ASSERT_TRUE(frame2.get() != nullptr);

  surface = frame2->GetSourceSurface();
  EXPECT_TRUE(RowsAreSolidColor(surface, 0, 40, BGRAColor::Green()));
  EXPECT_TRUE(RowsAreSolidColor(surface, 40, 10, BGRAColor::Transparent()));
  EXPECT_TRUE(RowsAreSolidColor(surface, 50, 20, BGRAColor::Blue()));
  EXPECT_TRUE(RowsAreSolidColor(surface, 70, 30, BGRAColor::Green()));
}

TEST(ImageBlendAnimationFilter, ClearWithOver)
{ TestWithBlendAnimationFilterClear(BlendMethod::OVER); }

TEST(ImageBlendAnimationFilter, ClearWithSource)
{ TestWithBlendAnimationFilterClear(BlendMethod::SOURCE); }

TEST(ImageBlendAnimationFilter, KeepWithSource)
{
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams params0{
      IntRect(0, 0, 100, 100), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  RawAccessFrameRef frame0 = WithBlendAnimationFilter(
      decoder, params0, IntSize(100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(BGRAColor::Green().AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 100), frame0->GetDirtyRect());

  AnimationParams params1{
      IntRect(0, 40, 100, 20), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 1, BlendMethod::SOURCE, DisposalMethod::KEEP};
  RawAccessFrameRef frame1 = WithBlendAnimationFilter(
      decoder, params1, IntSize(100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(BGRAColor::Red().AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 40, 100, 20), frame1->GetDirtyRect());

  ASSERT_TRUE(frame1.get() != nullptr);

  RefPtr<SourceSurface> surface = frame1->GetSourceSurface();
  EXPECT_TRUE(RowsAreSolidColor(surface, 0, 40, BGRAColor::Green()));
  EXPECT_TRUE(RowsAreSolidColor(surface, 40, 20, BGRAColor::Red()));
  EXPECT_TRUE(RowsAreSolidColor(surface, 60, 40, BGRAColor::Green()));
}

TEST(ImageBlendAnimationFilter, KeepWithOver)
{
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams params0{
      IntRect(0, 0, 100, 100), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  BGRAColor frameColor0(0, 0xFF, 0, 0x40);
  RawAccessFrameRef frame0 = WithBlendAnimationFilter(
      decoder, params0, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor0.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 100), frame0->GetDirtyRect());

  AnimationParams params1{
      IntRect(0, 40, 100, 20), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 1, BlendMethod::OVER, DisposalMethod::KEEP};
  BGRAColor frameColor1(0, 0, 0xFF, 0x80);
  RawAccessFrameRef frame1 = WithBlendAnimationFilter(
      decoder, params1, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor1.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 40, 100, 20), frame1->GetDirtyRect());

  ASSERT_TRUE(frame1.get() != nullptr);

  BGRAColor blendedColor(0, 0x20, 0x80, 0xA0, true);  // already premultiplied
  EXPECT_EQ(SkPMSrcOver(frameColor1.AsPixel(), frameColor0.AsPixel()),
            blendedColor.AsPixel());

  RefPtr<SourceSurface> surface = frame1->GetSourceSurface();
  EXPECT_TRUE(RowsAreSolidColor(surface, 0, 40, frameColor0));
  EXPECT_TRUE(RowsAreSolidColor(surface, 40, 20, blendedColor));
  EXPECT_TRUE(RowsAreSolidColor(surface, 60, 40, frameColor0));
}

TEST(ImageBlendAnimationFilter, RestorePreviousWithOver)
{
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams params0{
      IntRect(0, 0, 100, 100), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  BGRAColor frameColor0(0, 0xFF, 0, 0x40);
  RawAccessFrameRef frame0 = WithBlendAnimationFilter(
      decoder, params0, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor0.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 100), frame0->GetDirtyRect());

  AnimationParams params1{
      IntRect(0, 10, 100, 80), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 1, BlendMethod::SOURCE, DisposalMethod::RESTORE_PREVIOUS};
  BGRAColor frameColor1 = BGRAColor::Green();
  RawAccessFrameRef frame1 = WithBlendAnimationFilter(
      decoder, params1, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor1.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 10, 100, 80), frame1->GetDirtyRect());

  AnimationParams params2{
      IntRect(0, 40, 100, 20), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 2, BlendMethod::OVER, DisposalMethod::KEEP};
  BGRAColor frameColor2(0, 0, 0xFF, 0x80);
  RawAccessFrameRef frame2 = WithBlendAnimationFilter(
      decoder, params2, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor2.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 10, 100, 80), frame2->GetDirtyRect());

  ASSERT_TRUE(frame2.get() != nullptr);

  BGRAColor blendedColor(0, 0x20, 0x80, 0xA0, true);  // already premultiplied
  EXPECT_EQ(SkPMSrcOver(frameColor2.AsPixel(), frameColor0.AsPixel()),
            blendedColor.AsPixel());

  RefPtr<SourceSurface> surface = frame2->GetSourceSurface();
  EXPECT_TRUE(RowsAreSolidColor(surface, 0, 40, frameColor0));
  EXPECT_TRUE(RowsAreSolidColor(surface, 40, 20, blendedColor));
  EXPECT_TRUE(RowsAreSolidColor(surface, 60, 40, frameColor0));
}

TEST(ImageBlendAnimationFilter, RestorePreviousWithSource)
{
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams params0{
      IntRect(0, 0, 100, 100), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  BGRAColor frameColor0(0, 0xFF, 0, 0x40);
  RawAccessFrameRef frame0 = WithBlendAnimationFilter(
      decoder, params0, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor0.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 100), frame0->GetDirtyRect());

  AnimationParams params1{
      IntRect(0, 10, 100, 80), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 1, BlendMethod::SOURCE, DisposalMethod::RESTORE_PREVIOUS};
  BGRAColor frameColor1 = BGRAColor::Green();
  RawAccessFrameRef frame1 = WithBlendAnimationFilter(
      decoder, params1, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor1.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 10, 100, 80), frame1->GetDirtyRect());

  AnimationParams params2{
      IntRect(0, 40, 100, 20), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 2, BlendMethod::SOURCE, DisposalMethod::KEEP};
  BGRAColor frameColor2(0, 0, 0xFF, 0x80);
  RawAccessFrameRef frame2 = WithBlendAnimationFilter(
      decoder, params2, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor2.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 10, 100, 80), frame2->GetDirtyRect());

  ASSERT_TRUE(frame2.get() != nullptr);

  RefPtr<SourceSurface> surface = frame2->GetSourceSurface();
  EXPECT_TRUE(RowsAreSolidColor(surface, 0, 40, frameColor0));
  EXPECT_TRUE(RowsAreSolidColor(surface, 40, 20, frameColor2));
  EXPECT_TRUE(RowsAreSolidColor(surface, 60, 40, frameColor0));
}

TEST(ImageBlendAnimationFilter, RestorePreviousClearWithSource)
{
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams params0{
      IntRect(0, 0, 100, 100), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  BGRAColor frameColor0 = BGRAColor::Red();
  RawAccessFrameRef frame0 = WithBlendAnimationFilter(
      decoder, params0, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor0.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 100), frame0->GetDirtyRect());

  AnimationParams params1{
      IntRect(0, 0, 100, 20), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 1, BlendMethod::SOURCE, DisposalMethod::CLEAR};
  BGRAColor frameColor1 = BGRAColor::Blue();
  RawAccessFrameRef frame1 = WithBlendAnimationFilter(
      decoder, params1, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor1.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 20), frame1->GetDirtyRect());

  AnimationParams params2{
      IntRect(0, 10, 100, 80), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 2, BlendMethod::SOURCE, DisposalMethod::RESTORE_PREVIOUS};
  BGRAColor frameColor2 = BGRAColor::Green();
  RawAccessFrameRef frame2 = WithBlendAnimationFilter(
      decoder, params2, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor2.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 90), frame2->GetDirtyRect());

  AnimationParams params3{
      IntRect(0, 40, 100, 20), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 3, BlendMethod::SOURCE, DisposalMethod::KEEP};
  BGRAColor frameColor3 = BGRAColor::Blue();
  RawAccessFrameRef frame3 = WithBlendAnimationFilter(
      decoder, params3, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor3.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 90), frame3->GetDirtyRect());

  ASSERT_TRUE(frame3.get() != nullptr);

  RefPtr<SourceSurface> surface = frame3->GetSourceSurface();
  EXPECT_TRUE(RowsAreSolidColor(surface, 0, 20, BGRAColor::Transparent()));
  EXPECT_TRUE(RowsAreSolidColor(surface, 20, 20, frameColor0));
  EXPECT_TRUE(RowsAreSolidColor(surface, 40, 20, frameColor3));
  EXPECT_TRUE(RowsAreSolidColor(surface, 60, 40, frameColor0));
}

TEST(ImageBlendAnimationFilter, PartialOverlapFrameRect)
{
  RefPtr<Decoder> decoder = CreateTrivialBlendingDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AnimationParams params0{
      IntRect(-10, -20, 110, 100), FrameTimeout::FromRawMilliseconds(0),
      /* aFrameNum */ 0, BlendMethod::SOURCE, DisposalMethod::KEEP};
  BGRAColor frameColor0 = BGRAColor::Red();
  RawAccessFrameRef frame0 = WithBlendAnimationFilter(
      decoder, params0, IntSize(100, 100),
      [&](Decoder* aDecoder, SurfaceFilter* aFilter) {
        auto result = aFilter->WritePixels<uint32_t>(
            [&] { return AsVariant(frameColor0.AsPixel()); });
        EXPECT_EQ(WriteState::FINISHED, result);
      });
  EXPECT_EQ(IntRect(0, 0, 100, 100), frame0->GetDirtyRect());

  RefPtr<SourceSurface> surface = frame0->GetSourceSurface();
  EXPECT_TRUE(RowsAreSolidColor(surface, 0, 80, frameColor0));
  EXPECT_TRUE(RowsAreSolidColor(surface, 80, 20, BGRAColor::Transparent()));
}
