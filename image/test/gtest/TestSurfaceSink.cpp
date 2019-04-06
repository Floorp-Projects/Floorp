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
#include "SourceBuffer.h"
#include "SurfacePipe.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

enum class Orient { NORMAL, FLIP_VERTICALLY };

static void InitializeRowBuffer(uint32_t* aBuffer, size_t aSize,
                                size_t aStartPixel, size_t aEndPixel,
                                uint32_t aSetPixel) {
  uint32_t transparentPixel = BGRAColor::Transparent().AsPixel();
  for (size_t i = 0; i < aStartPixel && i < aSize; ++i) {
    aBuffer[i] = transparentPixel;
  }
  for (size_t i = aStartPixel; i < aEndPixel && i < aSize; ++i) {
    aBuffer[i] = aSetPixel;
  }
  for (size_t i = aEndPixel; i < aSize; ++i) {
    aBuffer[i] = transparentPixel;
  }
}

template <Orient Orientation, typename Func>
void WithSurfaceSink(Func aFunc) {
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  const bool flipVertically = Orientation == Orient::FLIP_VERTICALLY;

  WithFilterPipeline(decoder, std::forward<Func>(aFunc),
                     SurfaceConfig{decoder, IntSize(100, 100),
                                   SurfaceFormat::B8G8R8A8, flipVertically});
}

void ResetForNextPass(SurfaceFilter* aSink) {
  aSink->ResetToFirstRow();
  EXPECT_FALSE(aSink->IsSurfaceFinished());
  Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
  EXPECT_TRUE(invalidRect.isNothing());
}

template <typename WriteFunc, typename CheckFunc>
void DoCheckIterativeWrite(SurfaceFilter* aSink, WriteFunc aWriteFunc,
                           CheckFunc aCheckFunc) {
  // Write the buffer to successive rows until every row of the surface
  // has been written.
  uint32_t row = 0;
  WriteState result = WriteState::NEED_MORE_DATA;
  while (result == WriteState::NEED_MORE_DATA) {
    result = aWriteFunc(row);
    ++row;
  }
  EXPECT_EQ(WriteState::FINISHED, result);
  EXPECT_EQ(100u, row);

  AssertCorrectPipelineFinalState(aSink, IntRect(0, 0, 100, 100),
                                  IntRect(0, 0, 100, 100));

  // Check that the generated image is correct.
  aCheckFunc();
}

template <typename WriteFunc>
void CheckIterativeWrite(Decoder* aDecoder, SurfaceSink* aSink,
                         const IntRect& aOutputRect, WriteFunc aWriteFunc) {
  // Ignore the row passed to WriteFunc, since no callers use it.
  auto writeFunc = [&](uint32_t) { return aWriteFunc(); };

  DoCheckIterativeWrite(aSink, writeFunc,
                        [&] { CheckGeneratedImage(aDecoder, aOutputRect); });
}

TEST(ImageSurfaceSink, SurfaceSinkInitialization)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Check initial state.
    EXPECT_FALSE(aSink->IsSurfaceFinished());
    Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
    EXPECT_TRUE(invalidRect.isNothing());

    // Check that the surface is zero-initialized. We verify this by calling
    // CheckGeneratedImage() and telling it that we didn't write to the surface
    // anyway (i.e., we wrote to the empty rect); it will then expect the entire
    // surface to be transparent, which is what it should be if it was
    // zero-initialied.
    CheckGeneratedImage(aDecoder, IntRect(0, 0, 0, 0));
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWritePixels)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    CheckWritePixels(aDecoder, aSink);
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWritePixelsFinish)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Write nothing into the surface; just finish immediately.
    uint32_t count = 0;
    auto result = aSink->WritePixels<uint32_t>([&]() {
      count++;
      return AsVariant(WriteState::FINISHED);
    });
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(1u, count);

    AssertCorrectPipelineFinalState(aSink, IntRect(0, 0, 100, 100),
                                    IntRect(0, 0, 100, 100));

    // Attempt to write more and make sure that nothing gets written.
    count = 0;
    result = aSink->WritePixels<uint32_t>([&]() {
      count++;
      return AsVariant(BGRAColor::Red().AsPixel());
    });
    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(0u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Check that the generated image is correct.
    RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
    RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
    EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Transparent()));
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWritePixelsEarlyExit)
{
  auto checkEarlyExit = [](Decoder* aDecoder, SurfaceSink* aSink,
                           WriteState aState) {
    // Write half a row of green pixels and then exit early with |aState|. If
    // the lambda keeps getting called, we'll write red pixels, which will cause
    // the test to fail.
    uint32_t count = 0;
    auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
      if (count == 50) {
        return AsVariant(aState);
      }
      return count++ < 50 ? AsVariant(BGRAColor::Green().AsPixel())
                          : AsVariant(BGRAColor::Red().AsPixel());
    });

    EXPECT_EQ(aState, result);
    EXPECT_EQ(50u, count);
    CheckGeneratedImage(aDecoder, IntRect(0, 0, 50, 1));

    if (aState != WriteState::FINISHED) {
      // We should still be able to write more at this point.
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Verify that we can resume writing. We'll finish up the same row.
      count = 0;
      result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 50) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        ++count;
        return AsVariant(BGRAColor::Green().AsPixel());
      });

      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(50u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());
      CheckGeneratedImage(aDecoder, IntRect(0, 0, 100, 1));

      return;
    }

    // We should've finished the surface at this point.
    AssertCorrectPipelineFinalState(aSink, IntRect(0, 0, 100, 100),
                                    IntRect(0, 0, 100, 100));

    // Attempt to write more and make sure that nothing gets written.
    count = 0;
    result = aSink->WritePixels<uint32_t>([&] {
      count++;
      return AsVariant(BGRAColor::Red().AsPixel());
    });

    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(0u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Check that the generated image is still correct.
    CheckGeneratedImage(aDecoder, IntRect(0, 0, 50, 1));
  };

  WithSurfaceSink<Orient::NORMAL>([&](Decoder* aDecoder, SurfaceSink* aSink) {
    checkEarlyExit(aDecoder, aSink, WriteState::NEED_MORE_DATA);
  });

  WithSurfaceSink<Orient::NORMAL>([&](Decoder* aDecoder, SurfaceSink* aSink) {
    checkEarlyExit(aDecoder, aSink, WriteState::FAILURE);
  });

  WithSurfaceSink<Orient::NORMAL>([&](Decoder* aDecoder, SurfaceSink* aSink) {
    checkEarlyExit(aDecoder, aSink, WriteState::FINISHED);
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWritePixelsToRow)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Write the first 99 rows of our 100x100 surface and verify that even
    // though our lambda will yield pixels forever, only one row is written per
    // call to WritePixelsToRow().
    for (int row = 0; row < 99; ++row) {
      uint32_t count = 0;
      WriteState result = aSink->WritePixelsToRow<uint32_t>([&] {
        ++count;
        return AsVariant(BGRAColor::Green().AsPixel());
      });

      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(100u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, row, 100, 1), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, row, 100, 1), invalidRect->mOutputSpaceRect);

      CheckGeneratedImage(aDecoder, IntRect(0, 0, 100, row + 1));
    }

    // Write the final line, which should finish the surface.
    uint32_t count = 0;
    WriteState result = aSink->WritePixelsToRow<uint32_t>([&] {
      ++count;
      return AsVariant(BGRAColor::Green().AsPixel());
    });

    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u, count);

    // Note that the final invalid rect we expect here is only the last row;
    // that's because we called TakeInvalidRect() repeatedly in the loop above.
    AssertCorrectPipelineFinalState(aSink, IntRect(0, 99, 100, 1),
                                    IntRect(0, 99, 100, 1));

    // Check that the generated image is correct.
    CheckGeneratedImage(aDecoder, IntRect(0, 0, 100, 100));

    // Attempt to write more and make sure that nothing gets written.
    count = 0;
    result = aSink->WritePixelsToRow<uint32_t>([&] {
      count++;
      return AsVariant(BGRAColor::Red().AsPixel());
    });

    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(0u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Check that the generated image is still correct.
    CheckGeneratedImage(aDecoder, IntRect(0, 0, 100, 100));
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWritePixelsToRowEarlyExit)
{
  auto checkEarlyExit = [](Decoder* aDecoder, SurfaceSink* aSink,
                           WriteState aState) {
    // Write half a row of green pixels and then exit early with |aState|. If
    // the lambda keeps getting called, we'll write red pixels, which will cause
    // the test to fail.
    uint32_t count = 0;
    auto result =
        aSink->WritePixelsToRow<uint32_t>([&]() -> NextPixel<uint32_t> {
          if (count == 50) {
            return AsVariant(aState);
          }
          return count++ < 50 ? AsVariant(BGRAColor::Green().AsPixel())
                              : AsVariant(BGRAColor::Red().AsPixel());
        });

    EXPECT_EQ(aState, result);
    EXPECT_EQ(50u, count);
    CheckGeneratedImage(aDecoder, IntRect(0, 0, 50, 1));

    if (aState != WriteState::FINISHED) {
      // We should still be able to write more at this point.
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Verify that we can resume the same row and still stop at the end.
      count = 0;
      WriteState result = aSink->WritePixelsToRow<uint32_t>([&] {
        ++count;
        return AsVariant(BGRAColor::Green().AsPixel());
      });

      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(50u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());
      CheckGeneratedImage(aDecoder, IntRect(0, 0, 100, 1));

      return;
    }

    // We should've finished the surface at this point.
    AssertCorrectPipelineFinalState(aSink, IntRect(0, 0, 100, 100),
                                    IntRect(0, 0, 100, 100));

    // Attempt to write more and make sure that nothing gets written.
    count = 0;
    result = aSink->WritePixelsToRow<uint32_t>([&] {
      count++;
      return AsVariant(BGRAColor::Red().AsPixel());
    });

    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(0u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Check that the generated image is still correct.
    CheckGeneratedImage(aDecoder, IntRect(0, 0, 50, 1));
  };

  WithSurfaceSink<Orient::NORMAL>([&](Decoder* aDecoder, SurfaceSink* aSink) {
    checkEarlyExit(aDecoder, aSink, WriteState::NEED_MORE_DATA);
  });

  WithSurfaceSink<Orient::NORMAL>([&](Decoder* aDecoder, SurfaceSink* aSink) {
    checkEarlyExit(aDecoder, aSink, WriteState::FAILURE);
  });

  WithSurfaceSink<Orient::NORMAL>([&](Decoder* aDecoder, SurfaceSink* aSink) {
    checkEarlyExit(aDecoder, aSink, WriteState::FINISHED);
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWriteBuffer)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Create a green buffer the same size as one row of the surface (which is
    // 100x100), containing 60 pixels of green in the middle and 20 transparent
    // pixels on either side.
    uint32_t buffer[100];
    InitializeRowBuffer(buffer, 100, 20, 80, BGRAColor::Green().AsPixel());

    // Write the buffer to every row of the surface and check that the generated
    // image is correct.
    CheckIterativeWrite(aDecoder, aSink, IntRect(20, 0, 60, 100),
                        [&] { return aSink->WriteBuffer(buffer); });
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWriteBufferPartialRow)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Create a buffer the same size as one row of the surface, containing all
    // green pixels.
    uint32_t buffer[100];
    for (int i = 0; i < 100; ++i) {
      buffer[i] = BGRAColor::Green().AsPixel();
    }

    // Write the buffer to the middle 60 pixels of every row of the surface and
    // check that the generated image is correct.
    CheckIterativeWrite(aDecoder, aSink, IntRect(20, 0, 60, 100),
                        [&] { return aSink->WriteBuffer(buffer, 20, 60); });
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWriteBufferPartialRowStartColOverflow)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Create a buffer the same size as one row of the surface, containing all
    // green pixels.
    uint32_t buffer[100];
    for (int i = 0; i < 100; ++i) {
      buffer[i] = BGRAColor::Green().AsPixel();
    }

    {
      // Write the buffer to successive rows until every row of the surface
      // has been written. We place the start column beyond the end of the row,
      // which will prevent us from writing anything, so we check that the
      // generated image is entirely transparent.
      CheckIterativeWrite(aDecoder, aSink, IntRect(0, 0, 0, 0),
                          [&] { return aSink->WriteBuffer(buffer, 100, 100); });
    }

    ResetForNextPass(aSink);

    {
      // Write the buffer to successive rows until every row of the surface
      // has been written. We use column 50 as the start column, but we still
      // write the buffer, which means we overflow the right edge of the surface
      // by 50 pixels. We check that the left half of the generated image is
      // transparent and the right half is green.
      CheckIterativeWrite(aDecoder, aSink, IntRect(50, 0, 50, 100),
                          [&] { return aSink->WriteBuffer(buffer, 50, 100); });
    }
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWriteBufferPartialRowBufferOverflow)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Create a buffer twice as large as a row of the surface. The first half
    // (which is as large as a row of the image) will contain green pixels,
    // while the second half will contain red pixels.
    uint32_t buffer[200];
    for (int i = 0; i < 200; ++i) {
      buffer[i] =
          i < 100 ? BGRAColor::Green().AsPixel() : BGRAColor::Red().AsPixel();
    }

    {
      // Write the buffer to successive rows until every row of the surface has
      // been written. The buffer extends 100 pixels to the right of a row of
      // the surface, but bounds checking will prevent us from overflowing the
      // buffer. We check that the generated image is entirely green since the
      // pixels on the right side of the buffer shouldn't have been written to
      // the surface.
      CheckIterativeWrite(aDecoder, aSink, IntRect(0, 0, 100, 100),
                          [&] { return aSink->WriteBuffer(buffer, 0, 200); });
    }

    ResetForNextPass(aSink);

    {
      // Write from the buffer to the middle of each row of the surface. That
      // means that the left side of each row should be transparent, since we
      // didn't write anything there. A buffer overflow would cause us to write
      // buffer contents into the left side of each row. We check that the
      // generated image is transparent on the left side and green on the right.
      CheckIterativeWrite(aDecoder, aSink, IntRect(50, 0, 50, 100),
                          [&] { return aSink->WriteBuffer(buffer, 50, 200); });
    }
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWriteBufferFromNullSource)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Calling WriteBuffer() with a null pointer should fail without making any
    // changes to the surface.
    uint32_t* nullBuffer = nullptr;
    WriteState result = aSink->WriteBuffer(nullBuffer);

    EXPECT_EQ(WriteState::FAILURE, result);
    EXPECT_FALSE(aSink->IsSurfaceFinished());
    Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
    EXPECT_TRUE(invalidRect.isNothing());

    // Check that nothing got written to the surface.
    CheckGeneratedImage(aDecoder, IntRect(0, 0, 0, 0));
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWriteEmptyRow)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    {
      // Write an empty row to each row of the surface. We check that the
      // generated image is entirely transparent.
      CheckIterativeWrite(aDecoder, aSink, IntRect(0, 0, 0, 0),
                          [&] { return aSink->WriteEmptyRow(); });
    }

    ResetForNextPass(aSink);

    {
      // Write a partial row before we begin calling WriteEmptyRow(). We check
      // that the generated image is entirely transparent, which is to be
      // expected since WriteEmptyRow() overwrites the current row even if some
      // data has already been written to it.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 50) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        ++count;
        return AsVariant(BGRAColor::Green().AsPixel());
      });

      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(50u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      CheckIterativeWrite(aDecoder, aSink, IntRect(0, 0, 0, 0),
                          [&] { return aSink->WriteEmptyRow(); });
    }

    ResetForNextPass(aSink);

    {
      // Create a buffer the same size as one row of the surface, containing all
      // green pixels.
      uint32_t buffer[100];
      for (int i = 0; i < 100; ++i) {
        buffer[i] = BGRAColor::Green().AsPixel();
      }

      // Write an empty row to the middle 60 rows of the surface. The first 20
      // and last 20 rows will be green. (We need to use DoCheckIterativeWrite()
      // here because we need a custom function to check the output, since it
      // can't be described by a simple rect.)
      auto writeFunc = [&](uint32_t aRow) {
        if (aRow < 20 || aRow >= 80) {
          return aSink->WriteBuffer(buffer);
        } else {
          return aSink->WriteEmptyRow();
        }
      };

      auto checkFunc = [&] {
        RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
        RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();

        EXPECT_TRUE(RowsAreSolidColor(surface, 0, 20, BGRAColor::Green()));
        EXPECT_TRUE(
            RowsAreSolidColor(surface, 20, 60, BGRAColor::Transparent()));
        EXPECT_TRUE(RowsAreSolidColor(surface, 80, 20, BGRAColor::Green()));
      };

      DoCheckIterativeWrite(aSink, writeFunc, checkFunc);
    }
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWriteUnsafeComputedRow)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Create a green buffer the same size as one row of the surface.
    uint32_t buffer[100];
    for (int i = 0; i < 100; ++i) {
      buffer[i] = BGRAColor::Green().AsPixel();
    }

    // Write the buffer to successive rows until every row of the surface
    // has been written. We only write to the right half of each row, so we
    // check that the left side of the generated image is transparent and the
    // right side is green.
    CheckIterativeWrite(aDecoder, aSink, IntRect(50, 0, 50, 100), [&] {
      return aSink->WriteUnsafeComputedRow<uint32_t>(
          [&](uint32_t* aRow, uint32_t aLength) {
            EXPECT_EQ(100u, aLength);
            memcpy(aRow + 50, buffer, 50 * sizeof(uint32_t));
          });
    });
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWritePixelBlocks)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Create a green buffer the same size as one row of the surface (which is
    // 100x100), containing 60 pixels of green in the middle and 20 transparent
    // pixels on either side.
    uint32_t buffer[100];
    InitializeRowBuffer(buffer, 100, 20, 80, BGRAColor::Green().AsPixel());

    uint32_t count = 0;
    WriteState result = aSink->WritePixelBlocks<uint32_t>(
        [&](uint32_t* aBlockStart, int32_t aLength) {
          ++count;
          EXPECT_EQ(int32_t(100), aLength);
          memcpy(aBlockStart, buffer, 100 * sizeof(uint32_t));
          return MakeTuple(int32_t(100), Maybe<WriteState>());
        });

    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(100u, count);

    AssertCorrectPipelineFinalState(aSink, IntRect(0, 0, 100, 100),
                                    IntRect(0, 0, 100, 100));

    // Check that the generated image is correct.
    CheckGeneratedImage(aDecoder, IntRect(20, 0, 60, 100));

    // Attempt to write more and make sure that nothing gets written.
    count = 0;
    result = aSink->WritePixelBlocks<uint32_t>(
        [&](uint32_t* aBlockStart, int32_t aLength) {
          count++;
          for (int32_t i = 0; i < aLength; ++i) {
            aBlockStart[i] = BGRAColor::Red().AsPixel();
          }
          return MakeTuple(aLength, Maybe<WriteState>());
        });

    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(0u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Check that the generated image is still correct.
    CheckGeneratedImage(aDecoder, IntRect(20, 0, 60, 100));
  });
}

TEST(ImageSurfaceSink, SurfaceSinkWritePixelBlocksPartialRow)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    // Create a green buffer the same size as one row of the surface (which is
    // 100x100), containing 60 pixels of green in the middle and 20 transparent
    // pixels on either side.
    uint32_t buffer[100];
    InitializeRowBuffer(buffer, 100, 20, 80, BGRAColor::Green().AsPixel());

    // Write the first 99 rows of our 100x100 surface and verify that even
    // though our lambda will yield pixels forever, only one row is written per
    // call to WritePixelsToRow().
    for (int row = 0; row < 99; ++row) {
      for (int32_t written = 0; written < 100;) {
        WriteState result = aSink->WritePixelBlocks<uint32_t>(
            [&](uint32_t* aBlockStart, int32_t aLength) {
              // When we write the final block of pixels, it will request we
              // start another row. We should abort at that point.
              if (aLength == int32_t(100) && written == int32_t(100)) {
                return MakeTuple(int32_t(0), Some(WriteState::NEED_MORE_DATA));
              }

              // It should always request enough data to fill the row. So it
              // should request 100, 75, 50, and finally 25 pixels.
              EXPECT_EQ(int32_t(100) - written, aLength);

              // Only write one quarter of the pixels for the row.
              memcpy(aBlockStart, &buffer[written], 25 * sizeof(uint32_t));
              written += 25;

              // We've written the last pixels remaining for the row.
              if (written == int32_t(100)) {
                return MakeTuple(int32_t(25), Maybe<WriteState>());
              }

              // We've written another quarter of the row but not yet all of it.
              return MakeTuple(int32_t(25), Some(WriteState::NEED_MORE_DATA));
            });

        EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      }

      EXPECT_FALSE(aSink->IsSurfaceFinished());

      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, row, 100, 1), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, row, 100, 1), invalidRect->mOutputSpaceRect);

      CheckGeneratedImage(aDecoder, IntRect(20, 0, 60, row + 1));
    }

    // Write the final line, which should finish the surface.
    uint32_t count = 0;
    WriteState result = aSink->WritePixelBlocks<uint32_t>(
        [&](uint32_t* aBlockStart, int32_t aLength) {
          ++count;
          EXPECT_EQ(int32_t(100), aLength);
          memcpy(aBlockStart, buffer, 100 * sizeof(uint32_t));
          return MakeTuple(int32_t(100), Maybe<WriteState>());
        });

    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(1u, count);

    // Note that the final invalid rect we expect here is only the last row;
    // that's because we called TakeInvalidRect() repeatedly in the loop above.
    AssertCorrectPipelineFinalState(aSink, IntRect(0, 99, 100, 1),
                                    IntRect(0, 99, 100, 1));

    // Check that the generated image is correct.
    CheckGeneratedImage(aDecoder, IntRect(20, 0, 60, 100));

    // Attempt to write more and make sure that nothing gets written.
    count = 0;
    result = aSink->WritePixelBlocks<uint32_t>(
        [&](uint32_t* aBlockStart, int32_t aLength) {
          count++;
          for (int32_t i = 0; i < aLength; ++i) {
            aBlockStart[i] = BGRAColor::Red().AsPixel();
          }
          return MakeTuple(aLength, Maybe<WriteState>());
        });

    EXPECT_EQ(WriteState::FINISHED, result);
    EXPECT_EQ(0u, count);
    EXPECT_TRUE(aSink->IsSurfaceFinished());

    // Check that the generated image is still correct.
    CheckGeneratedImage(aDecoder, IntRect(20, 0, 60, 100));
  });
}

TEST(ImageSurfaceSink, SurfaceSinkProgressivePasses)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    {
      // Fill the image with a first pass of red.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        ++count;
        return AsVariant(BGRAColor::Red().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(100u * 100u, count);

      AssertCorrectPipelineFinalState(aSink, IntRect(0, 0, 100, 100),
                                      IntRect(0, 0, 100, 100));

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Red()));
    }

    {
      ResetForNextPass(aSink);

      // Check that the generated image is still the first pass image.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Red()));
    }

    {
      // Fill the image with a second pass of green.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        ++count;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(100u * 100u, count);

      AssertCorrectPipelineFinalState(aSink, IntRect(0, 0, 100, 100),
                                      IntRect(0, 0, 100, 100));

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Green()));
    }
  });
}

TEST(ImageSurfaceSink, SurfaceSinkInvalidRect)
{
  WithSurfaceSink<Orient::NORMAL>([](Decoder* aDecoder, SurfaceSink* aSink) {
    {
      // Write one row.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 100) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(100u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 0, 100, 1), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 0, 100, 1), invalidRect->mOutputSpaceRect);
    }

    {
      // Write eight rows.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 100 * 8) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(100u * 8u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 1, 100, 8), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 1, 100, 8), invalidRect->mOutputSpaceRect);
    }

    {
      // Write the left half of one row.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 50) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(50u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we don't have an invalid rect, since the invalid rect only
      // gets updated when a row gets completed.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isNothing());
    }

    {
      // Write the right half of the same row.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 50) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(50u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect, which will include both the
      // left and right halves of this row now that we've completed it.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 9, 100, 1), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 9, 100, 1), invalidRect->mOutputSpaceRect);
    }

    {
      // Write no rows.
      auto result = aSink->WritePixels<uint32_t>(
          [&]() { return AsVariant(WriteState::NEED_MORE_DATA); });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we don't have an invalid rect.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isNothing());
    }

    {
      // Fill the rest of the image.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(100u * 90u, count);
      EXPECT_TRUE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 10, 100, 90), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 10, 100, 90), invalidRect->mOutputSpaceRect);

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Green()));
    }
  });
}

TEST(ImageSurfaceSink, SurfaceSinkFlipVertically)
{
  WithSurfaceSink<Orient::FLIP_VERTICALLY>([](Decoder* aDecoder,
                                              SurfaceSink* aSink) {
    {
      // Fill the image with a first pass of red.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        ++count;
        return AsVariant(BGRAColor::Red().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(100u * 100u, count);

      AssertCorrectPipelineFinalState(aSink, IntRect(0, 0, 100, 100),
                                      IntRect(0, 0, 100, 100));

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Red()));
    }

    {
      ResetForNextPass(aSink);

      // Check that the generated image is still the first pass image.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Red()));
    }

    {
      // Fill 25 rows of the image with green and make sure everything is OK.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() -> NextPixel<uint32_t> {
        if (count == 25 * 100) {
          return AsVariant(WriteState::NEED_MORE_DATA);
        }
        count++;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::NEED_MORE_DATA, result);
      EXPECT_EQ(25u * 100u, count);
      EXPECT_FALSE(aSink->IsSurfaceFinished());

      // Assert that we have the right invalid rect, which should include the
      // *bottom* (since we're flipping vertically) 25 rows of the image.
      Maybe<SurfaceInvalidRect> invalidRect = aSink->TakeInvalidRect();
      EXPECT_TRUE(invalidRect.isSome());
      EXPECT_EQ(IntRect(0, 75, 100, 25), invalidRect->mInputSpaceRect);
      EXPECT_EQ(IntRect(0, 75, 100, 25), invalidRect->mOutputSpaceRect);

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
      EXPECT_TRUE(RowsAreSolidColor(surface, 0, 75, BGRAColor::Red()));
      EXPECT_TRUE(RowsAreSolidColor(surface, 75, 25, BGRAColor::Green()));
    }

    {
      // Fill the rest of the image with a second pass of green.
      uint32_t count = 0;
      auto result = aSink->WritePixels<uint32_t>([&]() {
        ++count;
        return AsVariant(BGRAColor::Green().AsPixel());
      });
      EXPECT_EQ(WriteState::FINISHED, result);
      EXPECT_EQ(75u * 100u, count);

      AssertCorrectPipelineFinalState(aSink, IntRect(0, 0, 100, 75),
                                      IntRect(0, 0, 100, 75));

      // Check that the generated image is correct.
      RawAccessFrameRef currentFrame = aDecoder->GetCurrentFrameRef();
      RefPtr<SourceSurface> surface = currentFrame->GetSourceSurface();
      EXPECT_TRUE(IsSolidColor(surface, BGRAColor::Green()));
    }
  });
}
