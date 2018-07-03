/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "BitReader.h"
#include "BitWriter.h"
#include "H264.h"

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

TEST(BitWriter, SPS)
{
  uint8_t sps_pps[] = { 0x01, 0x4d, 0x40, 0x0c, 0xff, 0xe1, 0x00, 0x1b, 0x67,
                        0x4d, 0x40, 0x0c, 0xe8, 0x80, 0x80, 0x9d, 0x80, 0xb5,
                        0x01, 0x01, 0x01, 0x40, 0x00, 0x00, 0x03, 0x00, 0x40,
                        0x00, 0x00, 0x0f, 0x03, 0xc5, 0x0a, 0x44, 0x80, 0x01,
                        0x00, 0x04, 0x68, 0xeb, 0xef, 0x20 };

  RefPtr<MediaByteBuffer> extraData = new MediaByteBuffer();
  extraData->AppendElements(sps_pps, sizeof(sps_pps));
  SPSData spsdata1;
  bool success = H264::DecodeSPSFromExtraData(extraData, spsdata1);
  EXPECT_EQ(success, true);

  RefPtr<MediaByteBuffer> extraData2 =
    H264::CreateExtraData(0x42, 0xc0, 0x1e, { 1280, 720 });
  SPSData spsdata2;
  success = H264::DecodeSPSFromExtraData(extraData2, spsdata2);
  EXPECT_EQ(success, true);
}
