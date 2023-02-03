/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "MediaCodecsSupport.h"

using namespace mozilla;
using namespace media;

// Test MCSInfo::GetDecodeSupportSet function.
// This function is used to retrieve SW/HW support information for a
// given codec from a MediaCodecsSupported EnumSet.
// We validate that SW, HW, SW+HW, or lack of support information is
// properly returned.
TEST(MediaCodecsSupport, GetDecodeSupportSet)
{
  // Mock VP8 SW support, VP9 HW support, H264 SW+HW support
  MediaCodecsSupported supported{MediaCodecsSupport::VP8SoftwareDecode,
                                 MediaCodecsSupport::VP9HardwareDecode,
                                 MediaCodecsSupport::H264SoftwareDecode,
                                 MediaCodecsSupport::H264HardwareDecode};

  MediaCodec codec;     // Codec used to generate + filter results
  DecodeSupportSet RV;  // Return value to check for validity

  // Check only SW support returned for VP8
  codec = MediaCodec::VP8;
  RV = MCSInfo::GetDecodeSupportSet(codec, supported);
  EXPECT_TRUE(RV.contains(DecodeSupport::SoftwareDecode));
  EXPECT_TRUE(RV.size() == 1);

  // Check only HW support returned for VP9
  codec = MediaCodec::VP9;
  RV = MCSInfo::GetDecodeSupportSet(codec, supported);
  EXPECT_TRUE(RV.contains(DecodeSupport::HardwareDecode));
  EXPECT_TRUE(RV.size() == 1);

  // Check for both SW/HW support returned for H264
  codec = MediaCodec::H264;
  RV = MCSInfo::GetDecodeSupportSet(codec, supported);
  EXPECT_TRUE(RV.contains(DecodeSupport::SoftwareDecode));
  EXPECT_TRUE(RV.contains(DecodeSupport::HardwareDecode));
  EXPECT_TRUE(RV.size() == 2);

  // Check empty return if codec not in list of codecs
  codec = MediaCodec::AV1;
  RV = MCSInfo::GetDecodeSupportSet(codec, supported);
  EXPECT_TRUE(RV.size() == 0);
}

// Test MCSInfo::GetDecodeMediaCodecsSupported function.
// This function is used to generate codec-specific SW/HW
// support information from a generic codec identifier enum and
// generic SW/HW support information.
// We validate that SW, HW, SW+HW, or lack of support information is
// properly returned.
TEST(MediaCodecsSupport, GetDecodeMediaCodecsSupported)
{
  MediaCodec codec;         // Codec used to generate / filter results
  MediaCodecsSupported RV;  // Return value to check for validity
  DecodeSupportSet dss;     // Non codec-specific SW / HW support information

  // Check SW support returned for VP8
  codec = MediaCodec::VP8;
  dss = DecodeSupportSet{DecodeSupport::SoftwareDecode};
  RV = MCSInfo::GetDecodeMediaCodecsSupported(codec, dss);
  EXPECT_TRUE(RV.contains(MediaCodecsSupport::VP8SoftwareDecode));
  EXPECT_TRUE(RV.size() == 1);

  // Check HW support returned for AV1
  codec = MediaCodec::AV1;
  dss = DecodeSupportSet{DecodeSupport::HardwareDecode};
  RV = MCSInfo::GetDecodeMediaCodecsSupported(codec, dss);
  EXPECT_TRUE(RV.contains(MediaCodecsSupport::AV1HardwareDecode));
  EXPECT_TRUE(RV.size() == 1);

  // Check SW + HW support returned for VP9
  codec = MediaCodec::VP9;
  dss = DecodeSupportSet{DecodeSupport::SoftwareDecode,
                         DecodeSupport::HardwareDecode};
  RV = MCSInfo::GetDecodeMediaCodecsSupported(codec, dss);
  EXPECT_TRUE(RV.contains(MediaCodecsSupport::VP9SoftwareDecode));
  EXPECT_TRUE(RV.contains(MediaCodecsSupport::VP9HardwareDecode));
  EXPECT_TRUE(RV.size() == 2);

  // Check empty return if codec not supported
  codec = MediaCodec::AV1;
  dss = DecodeSupportSet{};
  RV = MCSInfo::GetDecodeMediaCodecsSupported(codec, dss);
  EXPECT_TRUE(RV.size() == 0);
}

// Test MCSInfo::AddSupport function.
// This function is used to store codec support data.
// Incoming support data will be merged with any data that
// has already been stored.
TEST(MediaCodecsSupport, AddSupport)
{
  // Make sure we're not storing any existing support information.
  MCSInfo::ResetSupport();
  EXPECT_TRUE(MCSInfo::GetSupport().size() == 0);

  // Add codec support one at a time via individual calls
  MCSInfo::AddSupport(MediaCodecsSupport::AACSoftwareDecode);
  MCSInfo::AddSupport(MediaCodecsSupport::VP9SoftwareDecode);
  MCSInfo::AddSupport(MediaCodecsSupport::AV1HardwareDecode);

  // Add multiple codec support via MediaCodecsSupported EnumSet
  MCSInfo::AddSupport(
      MediaCodecsSupported{MediaCodecsSupport::H264SoftwareDecode,
                           MediaCodecsSupport::H264HardwareDecode});

  // Query MCSInfo for supported codecs
  MediaCodecsSupported supported = MCSInfo::GetSupport();
  DecodeSupportSet dss;

  // AAC should only report software decode support
  dss = MCSInfo::GetDecodeSupportSet(MediaCodec::AAC, supported);
  EXPECT_TRUE(dss.size() == 1);
  EXPECT_TRUE(dss.contains(DecodeSupport::SoftwareDecode));

  // AV1 should only report hardware decode support
  dss = MCSInfo::GetDecodeSupportSet(MediaCodec::AV1, supported);
  EXPECT_TRUE(dss.size() == 1);
  EXPECT_TRUE(dss.contains(DecodeSupport::HardwareDecode));

  // H264 should report both SW + HW decode support
  dss = MCSInfo::GetDecodeSupportSet(MediaCodec::H264, supported);
  EXPECT_TRUE(dss.size() == 2);
  EXPECT_TRUE(dss.contains(DecodeSupport::SoftwareDecode));
  EXPECT_TRUE(dss.contains(DecodeSupport::HardwareDecode));

  // Vorbis should report no decode support
  dss = MCSInfo::GetDecodeSupportSet(MediaCodec::Vorbis, supported);
  EXPECT_TRUE(dss.size() == 0);
}

// Test MCSInfo::GetMediaCodecsSupportedString function.
// This function returns a human-readable string containing codec
// names and SW/HW playback support information.
TEST(MediaCodecsSupport, GetMediaCodecsSupportedString)
{
  // Make sure we're not storing any existing support information.
  MCSInfo::ResetSupport();
  EXPECT_TRUE(MCSInfo::GetSupport().size() == 0);

  // Add H264 SW/HW support + VP8 Software decode support.
  MCSInfo::AddSupport({MediaCodecsSupport::H264SoftwareDecode,
                       MediaCodecsSupport::H264HardwareDecode,
                       MediaCodecsSupport::VP8SoftwareDecode});

  nsCString supportString;
  MCSInfo::GetMediaCodecsSupportedString(supportString, MCSInfo::GetSupport());
  EXPECT_TRUE(supportString.Equals("H264 SW\nH264 HW\nVP8 SW"_ns));
}
