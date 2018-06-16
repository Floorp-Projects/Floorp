/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "BitReader.h"
#include "BitWriter.h"

using namespace mozilla;

TEST(BitWriter, BitWriter)
{
  RefPtr<MediaByteBuffer> test = new MediaByteBuffer();
  BitWriter b(test);
  b.WriteBit(false);
  b.WriteBits(~1ULL, 1); // ensure that extra bits don't modify byte buffer.
  b.WriteBits(3, 1);
  b.WriteUE(1280 / 16 - 1);
  b.WriteUE(720 / 16 - 1);
  b.WriteUE(1280);
  b.WriteUE(720);
  b.WriteBit(true);
  b.WriteBit(false);
  b.WriteBit(true);
  b.WriteU8(7);
  b.WriteU32(16356);
  b.WriteU64(116356);
  b.WriteBits(~(0ULL) & ~1ULL, 16);
  const uint32_t length = b.BitCount();
  b.CloseWithRbspTrailing();

  BitReader c(test);

  EXPECT_EQ(c.ReadBit(), false);
  EXPECT_EQ(c.ReadBit(), false);
  EXPECT_EQ(c.ReadBit(), true);
  EXPECT_EQ(c.ReadUE(), 1280u / 16 - 1);
  EXPECT_EQ(c.ReadUE(), 720u / 16 - 1);
  EXPECT_EQ(c.ReadUE(), 1280u);
  EXPECT_EQ(c.ReadUE(), 720u);
  EXPECT_EQ(c.ReadBit(), true);
  EXPECT_EQ(c.ReadBit(), false);
  EXPECT_EQ(c.ReadBit(), true);
  EXPECT_EQ(c.ReadBits(8), 7u);
  EXPECT_EQ(c.ReadU32(), 16356u);
  EXPECT_EQ(c.ReadU64(), 116356u);
  EXPECT_EQ(c.ReadBits(16), 0xfffeu);
  EXPECT_EQ(length, BitReader::GetBitLength(test));
}
