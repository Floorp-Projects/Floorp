/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtest/gtest.h>
#include <stdint.h>

#include "MediaSpan.h"

#include "mozilla/ArrayUtils.h"

using namespace mozilla;

already_AddRefed<MediaByteBuffer> makeBuffer(uint8_t aStart, uint8_t aEnd) {
  RefPtr<MediaByteBuffer> buffer(new MediaByteBuffer);
  for (uint8_t i = aStart; i <= aEnd; i++) {
    buffer->AppendElement(i);
  }
  return buffer.forget();
}

bool IsRangeAt(const MediaSpan& aSpan, uint8_t aStart, uint8_t aEnd,
               size_t aAt) {
  size_t length = size_t(aEnd) - size_t(aStart) + 1;
  if (aAt + length > aSpan.Length()) {
    return false;
  }
  for (size_t i = 0; i < length; i++) {
    if (aSpan[aAt + i] != uint8_t(aStart + i)) {
      return false;
    }
  }
  return true;
}

bool IsRange(const MediaSpan& aSpan, uint8_t aStart, uint8_t aEnd) {
  return IsRangeAt(aSpan, aStart, aEnd, 0);
}

TEST(MediaSpan, AppendToFromSpan)
{
  RefPtr<MediaByteBuffer> buffer1 = makeBuffer(0, 9);
  MediaSpan span1 = MediaSpan(buffer1);
  EXPECT_EQ(span1.Length(), size_t(10));
  EXPECT_TRUE(IsRange(span1, 0, 9));

  MediaSpan span2 = span1.From(5);

  EXPECT_EQ(span2.Length(), size_t(5));
  EXPECT_TRUE(IsRange(span2, 5, 9));
  RefPtr<MediaByteBuffer> buffer2 = makeBuffer(10, 19);
  EXPECT_EQ(buffer2->Length(), size_t(10));
  span2.Append(buffer2);

  // Span2 should be: [5...19]
  EXPECT_EQ(span2.Length(), size_t(15));
  EXPECT_TRUE(IsRange(span2, 5, 19));

  // Span1 should not be modified by the append to span2.
  EXPECT_EQ(span1.Length(), size_t(10));
  EXPECT_TRUE(IsRange(span1, 0, 9));
}

TEST(MediaSpan, AppendToToSpan)
{
  RefPtr<MediaByteBuffer> buffer1 = makeBuffer(0, 9);
  MediaSpan span1 = MediaSpan(buffer1);
  EXPECT_EQ(span1.Length(), size_t(10));
  EXPECT_TRUE(IsRange(span1, 0, 9));

  MediaSpan span2 = span1.To(5);

  // Span2 should be [0...4]
  EXPECT_EQ(span2.Length(), size_t(5));
  EXPECT_TRUE(IsRange(span2, 0, 4));
  RefPtr<MediaByteBuffer> buffer2 = makeBuffer(10, 19);
  EXPECT_EQ(buffer2->Length(), size_t(10));
  span2.Append(buffer2);

  // Span2 should be: [0...4][10...19]
  EXPECT_EQ(span2.Length(), size_t(15));
  EXPECT_TRUE(IsRangeAt(span2, 0, 4, 0));
  EXPECT_TRUE(IsRangeAt(span2, 10, 19, 5));

  // Span1 should not be modified by the append to span2.
  EXPECT_EQ(span1.Length(), size_t(10));
  EXPECT_TRUE(IsRange(span1, 0, 9));
}

TEST(MediaSpan, RemoveFront)
{
  RefPtr<MediaByteBuffer> buffer1 = makeBuffer(0, 9);
  MediaSpan span1 = MediaSpan(buffer1);
  EXPECT_EQ(span1.Length(), size_t(10));
  EXPECT_TRUE(IsRange(span1, 0, 9));

  MediaSpan span2(span1);
  EXPECT_EQ(span2.Length(), size_t(10));

  span2.RemoveFront(5);

  // Span2 should now be [5...9]
  EXPECT_EQ(span2.Length(), size_t(5));
  EXPECT_TRUE(IsRange(span2, 5, 9));

  // Span1 should be unaffected.
  EXPECT_EQ(span1.Length(), size_t(10));
  EXPECT_TRUE(IsRange(span1, 0, 9));
}
