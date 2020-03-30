/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"

#include "Common.h"
#include "Decoder.h"
#include "DecoderFactory.h"
#include "IDecodingTask.h"
#include "mozilla/RefPtr.h"
#include "ProgressTracker.h"
#include "SourceBuffer.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

namespace {

static void CheckDecoderState(const ImageTestCase& aTestCase,
                              image::Decoder* aDecoder,
                              const IntSize& aOutputSize) {
  // image::Decoder should match what we asked for in the MIME type.
  EXPECT_NE(aDecoder->GetType(), DecoderType::UNKNOWN);
  EXPECT_EQ(aDecoder->GetType(),
            DecoderFactory::GetDecoderType(aTestCase.mMimeType));

  EXPECT_TRUE(aDecoder->GetDecodeDone());
  EXPECT_FALSE(aDecoder->HasError());

  // Verify that the decoder made the expected progress.
  Progress progress = aDecoder->TakeProgress();
  EXPECT_FALSE(bool(progress & FLAG_HAS_ERROR));
  EXPECT_FALSE(bool(aTestCase.mFlags & TEST_CASE_HAS_ERROR));

  EXPECT_TRUE(bool(progress & FLAG_SIZE_AVAILABLE));
  EXPECT_TRUE(bool(progress & FLAG_DECODE_COMPLETE));
  EXPECT_TRUE(bool(progress & FLAG_FRAME_COMPLETE));
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_IS_TRANSPARENT),
            bool(progress & FLAG_HAS_TRANSPARENCY));
  EXPECT_EQ(bool(aTestCase.mFlags & TEST_CASE_IS_ANIMATED),
            bool(progress & FLAG_IS_ANIMATED));

  // The decoder should get the correct size.
  IntSize size = aDecoder->Size();
  EXPECT_EQ(aTestCase.mSize.width, size.width);
  EXPECT_EQ(aTestCase.mSize.height, size.height);

  // Get the current frame, which is always the first frame of the image
  // because CreateAnonymousDecoder() forces a first-frame-only decode.
  RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
  RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

  // Verify that the resulting surfaces matches our expectations.
  EXPECT_TRUE(surface->IsDataSourceSurface());
  EXPECT_TRUE(surface->GetFormat() == SurfaceFormat::OS_RGBX ||
              surface->GetFormat() == SurfaceFormat::OS_RGBA);
  EXPECT_EQ(aOutputSize, surface->GetSize());
}

template <typename Func>
static void WithSingleChunkDecode(const ImageTestCase& aTestCase,
                                  SourceBuffer* aSourceBuffer,
                                  const Maybe<IntSize>& aOutputSize,
                                  Func aResultChecker) {
  auto sourceBuffer = WrapNotNull(RefPtr<SourceBuffer>(aSourceBuffer));

  // Create a decoder.
  DecoderType decoderType = DecoderFactory::GetDecoderType(aTestCase.mMimeType);
  RefPtr<image::Decoder> decoder = DecoderFactory::CreateAnonymousDecoder(
      decoderType, sourceBuffer, aOutputSize, DecoderFlags::FIRST_FRAME_ONLY,
      aTestCase.mSurfaceFlags);
  ASSERT_TRUE(decoder != nullptr);
  RefPtr<IDecodingTask> task =
      new AnonymousDecodingTask(WrapNotNull(decoder), /* aResumable */ false);

  // Run the full decoder synchronously.
  task->Run();

  // Call the lambda to verify the expected results.
  aResultChecker(decoder);
}

static void CheckDecode(const ImageTestCase& aTestCase,
                        SourceBuffer* aSourceBuffer) {
  WithSingleChunkDecode(
      aTestCase, aSourceBuffer, Nothing(), [&](image::Decoder* aDecoder) {
        CheckDecoderState(aTestCase, aDecoder, aTestCase.mSize);
      });
}

static void CheckDownscaleDuringDecode(const ImageTestCase& aTestCase,
                                       SourceBuffer* aSourceBuffer) {
  IntSize outputSize(20, 20);
  WithSingleChunkDecode(aTestCase, aSourceBuffer, Some(outputSize),
                        [&](image::Decoder* aDecoder) {
                          CheckDecoderState(aTestCase, aDecoder, outputSize);
                        });
}

#define IMAGE_GTEST_BENCH_FIXTURE(test_fixture, test_case) \
  class test_fixture : public ImageBenchmarkBase {         \
   protected:                                              \
    test_fixture() : ImageBenchmarkBase(test_case()) {}    \
  };

#define IMAGE_GTEST_NATIVE_BENCH_F(test_fixture) \
  MOZ_GTEST_BENCH_F(test_fixture, Native,        \
                    [this] { CheckDecode(mTestCase, mSourceBuffer); });

#define IMAGE_GTEST_DOWNSCALE_BENCH_F(test_fixture)       \
  MOZ_GTEST_BENCH_F(test_fixture, Downscale, [this] {     \
    CheckDownscaleDuringDecode(mTestCase, mSourceBuffer); \
  });

#define IMAGE_GTEST_NO_COLOR_MANAGEMENT_BENCH_F(test_fixture)         \
  MOZ_GTEST_BENCH_F(test_fixture, NoColorManagement, [this] {         \
    ImageTestCase testCase = mTestCase;                               \
    testCase.mSurfaceFlags |= SurfaceFlags::NO_COLORSPACE_CONVERSION; \
    CheckDecode(testCase, mSourceBuffer);                             \
  });

#define IMAGE_GTEST_NO_PREMULTIPLY_BENCH_F(test_fixture)          \
  MOZ_GTEST_BENCH_F(test_fixture, NoPremultiplyAlpha, [this] {    \
    ImageTestCase testCase = mTestCase;                           \
    testCase.mSurfaceFlags |= SurfaceFlags::NO_PREMULTIPLY_ALPHA; \
    CheckDecode(testCase, mSourceBuffer);                         \
  });

#define IMAGE_GTEST_BENCH_F(type, test)                            \
  IMAGE_GTEST_BENCH_FIXTURE(ImageDecodersPerf_##type##_##test,     \
                            Perf##test##type##TestCase)            \
  IMAGE_GTEST_NATIVE_BENCH_F(ImageDecodersPerf_##type##_##test)    \
  IMAGE_GTEST_DOWNSCALE_BENCH_F(ImageDecodersPerf_##type##_##test) \
  IMAGE_GTEST_NO_COLOR_MANAGEMENT_BENCH_F(ImageDecodersPerf_##type##_##test)

#define IMAGE_GTEST_BENCH_ALPHA_F(type, test) \
  IMAGE_GTEST_BENCH_F(type, test)             \
  IMAGE_GTEST_NO_PREMULTIPLY_BENCH_F(ImageDecodersPerf_##type##_##test)

IMAGE_GTEST_BENCH_F(JPG, YCbCr)
IMAGE_GTEST_BENCH_F(JPG, Cmyk)
IMAGE_GTEST_BENCH_F(JPG, Gray)

IMAGE_GTEST_BENCH_F(PNG, Rgb)
IMAGE_GTEST_BENCH_F(PNG, Gray)
IMAGE_GTEST_BENCH_ALPHA_F(PNG, RgbAlpha)
IMAGE_GTEST_BENCH_ALPHA_F(PNG, GrayAlpha)

IMAGE_GTEST_BENCH_F(WebP, RgbLossless)
IMAGE_GTEST_BENCH_F(WebP, RgbLossy)
IMAGE_GTEST_BENCH_ALPHA_F(WebP, RgbAlphaLossless)
IMAGE_GTEST_BENCH_ALPHA_F(WebP, RgbAlphaLossy)

IMAGE_GTEST_BENCH_F(GIF, Rgb)

}  // namespace
