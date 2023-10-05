/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>
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
  b.WriteBits(~1ULL, 1);  // ensure that extra bits don't modify byte buffer.
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
  b.WriteULEB128(16ULL);
  b.WriteULEB128(31895793ULL);
  b.WriteULEB128(426894039235654ULL);
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
  EXPECT_EQ(c.ReadULEB128(), 16ull);
  EXPECT_EQ(c.ReadULEB128(), 31895793ull);
  EXPECT_EQ(c.ReadULEB128(), 426894039235654ull);
  EXPECT_EQ(length, BitReader::GetBitLength(test));
}

TEST(BitWriter, AdvanceBytes)
{
  RefPtr<MediaByteBuffer> test = new MediaByteBuffer();
  BitWriter b(test);
  b.WriteBits(0xff, 8);
  EXPECT_EQ(test->Length(), 1u);

  uint8_t data[] = {0xfe, 0xfd};
  test->AppendElements(data, sizeof(data));
  EXPECT_EQ(test->Length(), 3u);
  b.AdvanceBytes(2);

  b.WriteBits(0xfc, 8);
  EXPECT_EQ(test->Length(), 4u);

  BitReader c(test);
  EXPECT_EQ(c.ReadU32(), 0xfffefdfc);
}

TEST(BitWriter, SPS)
{
  uint8_t sps_pps[] = {0x01, 0x4d, 0x40, 0x0c, 0xff, 0xe1, 0x00, 0x1b, 0x67,
                       0x4d, 0x40, 0x0c, 0xe8, 0x80, 0x80, 0x9d, 0x80, 0xb5,
                       0x01, 0x01, 0x01, 0x40, 0x00, 0x00, 0x03, 0x00, 0x40,
                       0x00, 0x00, 0x0f, 0x03, 0xc5, 0x0a, 0x44, 0x80, 0x01,
                       0x00, 0x04, 0x68, 0xeb, 0xef, 0x20};

  RefPtr<MediaByteBuffer> extraData = new MediaByteBuffer();
  extraData->AppendElements(sps_pps, sizeof(sps_pps));
  SPSData spsdata1;
  bool success = H264::DecodeSPSFromExtraData(extraData, spsdata1);
  EXPECT_EQ(success, true);

  auto testOutput = [&](uint8_t aProfile, uint8_t aConstraints, uint8_t aLevel,
                        gfx::IntSize aSize, char const* aDesc) {
    RefPtr<MediaByteBuffer> extraData =
        H264::CreateExtraData(aProfile, aConstraints, aLevel, aSize);
    SPSData spsData;
    success = H264::DecodeSPSFromExtraData(extraData, spsData);
    EXPECT_EQ(success, true) << aDesc;
    EXPECT_EQ(spsData.profile_idc, aProfile) << aDesc;
    EXPECT_EQ(spsData.constraint_set0_flag, (aConstraints >> 7) & 1) << aDesc;
    EXPECT_EQ(spsData.constraint_set1_flag, (aConstraints >> 6) & 1) << aDesc;
    EXPECT_EQ(spsData.constraint_set2_flag, (aConstraints >> 5) & 1) << aDesc;
    EXPECT_EQ(spsData.constraint_set3_flag, (aConstraints >> 4) & 1) << aDesc;
    EXPECT_EQ(spsData.constraint_set4_flag, (aConstraints >> 3) & 1) << aDesc;
    EXPECT_EQ(spsData.constraint_set5_flag, (aConstraints >> 2) & 1) << aDesc;

    EXPECT_EQ(spsData.level_idc, aLevel) << aDesc;
    EXPECT_TRUE(!aSize.IsEmpty());
    EXPECT_EQ(spsData.pic_width, static_cast<uint32_t>(aSize.width)) << aDesc;
    EXPECT_EQ(spsData.pic_height, static_cast<uint32_t>(aSize.height)) << aDesc;
  };

  testOutput(0x42, 0x40, 0x1E, {1920, 1080}, "Constrained Baseline Profile");
  testOutput(0x4D, 0x00, 0x0B, {300, 300}, "Main Profile");
  testOutput(0x64, 0x0C, 0x33, {1280, 720}, "Constrained High Profile");
}
