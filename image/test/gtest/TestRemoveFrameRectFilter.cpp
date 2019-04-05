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
#include "SurfaceFilters.h"
#include "SurfacePipe.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

template <typename Func>
void WithRemoveFrameRectFilter(const IntSize& aSize, const IntRect& aFrameRect,
                               Func aFunc) {
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  WithFilterPipeline(
      decoder, std::forward<Func>(aFunc), RemoveFrameRectConfig{aFrameRect},
      SurfaceConfig{decoder, aSize, SurfaceFormat::B8G8R8A8, false});
}

void AssertConfiguringRemoveFrameRectFilterFails(const IntSize& aSize,
                                                 const IntRect& aFrameRect) {
  RefPtr<Decoder> decoder = CreateTrivialDecoder();
  ASSERT_TRUE(decoder != nullptr);

  AssertConfiguringPipelineFails(
      decoder, RemoveFrameRectConfig{aFrameRect},
      SurfaceConfig{decoder, aSize, SurfaceFormat::B8G8R8A8, false});
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_0_0_100_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(0, 0, 100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 100, 100)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_0_0_0_0)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(0, 0, 0, 0),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_Minus50_50_0_0)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(-50, 50, 0, 0),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_50_Minus50_0_0)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(50, -50, 0, 0),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_150_50_0_0)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(150, 50, 0, 0),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_50_150_0_0)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(50, 150, 0, 0),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_200_200_100_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(200, 200, 100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Note that aInputRect is zero-size because RemoveFrameRectFilter
        // ignores trailing rows that don't show up in the output. (Leading rows
        // unfortunately can't be ignored.)
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_Minus200_25_100_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(-200, 25, 100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Note that aInputRect is zero-size because RemoveFrameRectFilter
        // ignores trailing rows that don't show up in the output. (Leading rows
        // unfortunately can't be ignored.)
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_25_Minus200_100_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(25, -200, 100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Note that aInputRect is zero-size because RemoveFrameRectFilter
        // ignores trailing rows that don't show up in the output. (Leading rows
        // unfortunately can't be ignored.)
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_200_25_100_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(200, 25, 100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Note that aInputRect is zero-size because RemoveFrameRectFilter
        // ignores trailing rows that don't show up in the output. (Leading rows
        // unfortunately can't be ignored.)
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_25_200_100_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(25, 200, 100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Note that aInputRect is zero-size because RemoveFrameRectFilter
        // ignores trailing rows that don't show up in the output. (Leading rows
        // unfortunately can't be ignored.)
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter,
     WritePixels100_100_to_Minus200_Minus200_100_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(-200, -200, 100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 0, 0)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 0, 0)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_Minus50_Minus50_100_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(-50, -50, 100, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 0, 50, 50)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_Minus50_25_100_50)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(-50, 25, 100, 50),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 100, 50)),
                         /* aOutputWriteRect = */ Some(IntRect(0, 25, 50, 50)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_25_Minus50_50_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(25, -50, 50, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(aDecoder, aFilter,
                         /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
                         /* aInputWriteRect = */ Some(IntRect(0, 0, 50, 100)),
                         /* aOutputWriteRect = */ Some(IntRect(25, 0, 50, 50)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_50_25_100_50)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(50, 25, 100, 50),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        CheckWritePixels(
            aDecoder, aFilter,
            /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
            /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
            /* aInputWriteRect = */ Some(IntRect(0, 0, 100, 50)),
            /* aOutputWriteRect = */ Some(IntRect(50, 25, 50, 50)));
      });
}

TEST(ImageRemoveFrameRectFilter, WritePixels100_100_to_25_50_50_100)
{
  WithRemoveFrameRectFilter(
      IntSize(100, 100), IntRect(25, 50, 50, 100),
      [](Decoder* aDecoder, SurfaceFilter* aFilter) {
        // Note that aInputRect is 50x50 because RemoveFrameRectFilter ignores
        // trailing rows that don't show up in the output. (Leading rows
        // unfortunately can't be ignored.)
        CheckWritePixels(
            aDecoder, aFilter,
            /* aOutputRect = */ Some(IntRect(0, 0, 100, 100)),
            /* aInputRect = */ Some(IntRect(0, 0, 100, 100)),
            /* aInputWriteRect = */ Some(IntRect(0, 0, 50, 50)),
            /* aOutputWriteRect = */ Some(IntRect(25, 50, 50, 100)));
      });
}

TEST(ImageRemoveFrameRectFilter, RemoveFrameRectFailsFor0_0_to_0_0_100_100)
{
  // A zero-size image is disallowed.
  AssertConfiguringRemoveFrameRectFilterFails(IntSize(0, 0),
                                              IntRect(0, 0, 100, 100));
}

TEST(ImageRemoveFrameRectFilter,
     RemoveFrameRectFailsForMinus1_Minus1_to_0_0_100_100)
{
  // A negative-size image is disallowed.
  AssertConfiguringRemoveFrameRectFilterFails(IntSize(-1, -1),
                                              IntRect(0, 0, 100, 100));
}

TEST(ImageRemoveFrameRectFilter, RemoveFrameRectFailsFor100_100_to_0_0_0_0)
{
  // A zero size frame rect is disallowed.
  AssertConfiguringRemoveFrameRectFilterFails(IntSize(100, 100),
                                              IntRect(0, 0, -1, -1));
}

TEST(ImageRemoveFrameRectFilter,
     RemoveFrameRectFailsFor100_100_to_0_0_Minus1_Minus1)
{
  // A negative size frame rect is disallowed.
  AssertConfiguringRemoveFrameRectFilterFails(IntSize(100, 100),
                                              IntRect(0, 0, -1, -1));
}
