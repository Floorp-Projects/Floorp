/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "nsTArray.h"
#include "VPXDecoder.h"

#include <stdio.h>

using namespace mozilla;

static void ReadVPXFile(const char* aPath, nsTArray<uint8_t>& aBuffer) {
  FILE* f = fopen(aPath, "rb");
  ASSERT_NE(f, (FILE*)nullptr);

  int r = fseek(f, 0, SEEK_END);
  ASSERT_EQ(r, 0);

  long size = ftell(f);
  ASSERT_NE(size, -1);
  aBuffer.SetLength(size);

  r = fseek(f, 0, SEEK_SET);
  ASSERT_EQ(r, 0);

  size_t got = fread(aBuffer.Elements(), 1, size, f);
  ASSERT_EQ(got, size_t(size));

  r = fclose(f);
  ASSERT_EQ(r, 0);
}

static vpx_codec_iface_t* ParseIVFConfig(nsTArray<uint8_t>& data,
                                         vpx_codec_dec_cfg_t& config) {
  if (data.Length() < 32 + 12) {
    // Not enough data for file & first frame headers.
    return nullptr;
  }
  if (data[0] != 'D' || data[1] != 'K' || data[2] != 'I' || data[3] != 'F') {
    // Expect 'DKIP'
    return nullptr;
  }
  if (data[4] != 0 || data[5] != 0) {
    // Expect version==0.
    return nullptr;
  }
  if (data[8] != 'V' || data[9] != 'P' ||
      (data[10] != '8' && data[10] != '9') || data[11] != '0') {
    // Expect 'VP80' or 'VP90'.
    return nullptr;
  }
  config.w = uint32_t(data[12]) || (uint32_t(data[13]) << 8);
  config.h = uint32_t(data[14]) || (uint32_t(data[15]) << 8);
  vpx_codec_iface_t* codec =
      (data[10] == '8') ? vpx_codec_vp8_dx() : vpx_codec_vp9_dx();
  // Remove headers, to just leave raw VPx data to be decoded.
  data.RemoveElementsAt(0, 32 + 12);
  return codec;
}

struct TestFileData {
  const char* mFilename;
  vpx_codec_err_t mDecodeResult;
};
static const TestFileData testFiles[] = {
    {"test_case_1224361.vp8.ivf", VPX_CODEC_OK},
    {"test_case_1224363.vp8.ivf", VPX_CODEC_CORRUPT_FRAME},
    {"test_case_1224369.vp8.ivf", VPX_CODEC_CORRUPT_FRAME}};

TEST(libvpx, test_cases)
{
  for (size_t test = 0; test < ArrayLength(testFiles); ++test) {
    nsTArray<uint8_t> data;
    ReadVPXFile(testFiles[test].mFilename, data);
    ASSERT_GT(data.Length(), 0u);

    vpx_codec_dec_cfg_t config;
    vpx_codec_iface_t* dx = ParseIVFConfig(data, config);
    ASSERT_TRUE(dx);
    config.threads = 2;

    vpx_codec_ctx_t ctx;
    PodZero(&ctx);
    vpx_codec_err_t r = vpx_codec_dec_init(&ctx, dx, &config, 0);
    ASSERT_EQ(VPX_CODEC_OK, r);

    r = vpx_codec_decode(&ctx, data.Elements(), data.Length(), nullptr, 0);
    // This test case is known to be corrupt.
    EXPECT_EQ(testFiles[test].mDecodeResult, r);

    r = vpx_codec_destroy(&ctx);
    EXPECT_EQ(VPX_CODEC_OK, r);
  }
}
