// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/data_pack.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

class DataPackTest : public testing::Test {
 public:
  DataPackTest() {
    PathService::Get(base::DIR_SOURCE_ROOT, &data_path_);
    data_path_ = data_path_.Append(
        FILE_PATH_LITERAL("base/data/data_pack_unittest/sample.pak"));
  }

  FilePath data_path_;
};

TEST_F(DataPackTest, Load) {
  base::DataPack pack;
  ASSERT_TRUE(pack.Load(data_path_));

  StringPiece data;
  ASSERT_TRUE(pack.Get(4, &data));
  EXPECT_EQ("this is id 4", data);
  ASSERT_TRUE(pack.Get(6, &data));
  EXPECT_EQ("this is id 6", data);

  // Try reading zero-length data blobs, just in case.
  ASSERT_TRUE(pack.Get(1, &data));
  EXPECT_EQ(0U, data.length());
  ASSERT_TRUE(pack.Get(10, &data));
  EXPECT_EQ(0U, data.length());

  // Try looking up an invalid key.
  ASSERT_FALSE(pack.Get(140, &data));
}
