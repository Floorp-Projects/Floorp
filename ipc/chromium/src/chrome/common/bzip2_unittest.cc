// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/bzip2/bzlib.h"

namespace {
  class Bzip2Test : public testing::Test {
  };
};

// This test does a simple round trip to test that the bzip2 library is
// present and working.
TEST(Bzip2Test, Roundtrip) {
  char input[] = "Test Data, More Test Data, Even More Data of Test";
  char output[256];

  memset(output, 0, arraysize(output));

  bz_stream stream;
  stream.bzalloc = NULL;
  stream.bzfree = NULL;
  stream.opaque = NULL;
  int result = BZ2_bzCompressInit(&stream,
                                  9,   // 900k block size
                                  0,   // quiet
                                  0);  // default work factor
  ASSERT_EQ(BZ_OK, result);

  stream.next_in = input;
  stream.avail_in = arraysize(input) - 1;
  stream.next_out = output;
  stream.avail_out = arraysize(output);
  do {
    result = BZ2_bzCompress(&stream, BZ_FINISH);
  } while (result == BZ_FINISH_OK);
  ASSERT_EQ(BZ_STREAM_END, result);
  result = BZ2_bzCompressEnd(&stream);
  ASSERT_EQ(BZ_OK, result);
  int written = stream.total_out_lo32;

  // Make sure we wrote something; otherwise not sure what to expect
  ASSERT_GT(written, 0);

  // Now decompress and check that we got the same thing.
  result = BZ2_bzDecompressInit(&stream, 0, 0);
  ASSERT_EQ(BZ_OK, result);
  char output2[256];
  memset(output2, 0, arraysize(output2));

  stream.next_in = output;
  stream.avail_in = written;
  stream.next_out = output2;
  stream.avail_out = arraysize(output2);

  do {
    result = BZ2_bzDecompress(&stream);
  } while (result == BZ_OK);
  ASSERT_EQ(result, BZ_STREAM_END);
  result = BZ2_bzDecompressEnd(&stream);
  ASSERT_EQ(result, BZ_OK);

  EXPECT_EQ(arraysize(input) - 1, stream.total_out_lo32);
  EXPECT_STREQ(input, output2);
}
