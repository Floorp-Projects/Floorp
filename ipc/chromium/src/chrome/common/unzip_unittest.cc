// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/unzip.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Make the test a PlatformTest to setup autorelease pools properly on Mac.
class UnzipTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();

    ASSERT_TRUE(file_util::CreateNewTempDirectory(
        FILE_PATH_LITERAL("unzip_unittest_"), &test_dir_));

    FilePath zip_path(test_dir_.AppendASCII("test"));
    zip_contents_.insert(zip_path);
    zip_contents_.insert(zip_path.AppendASCII("foo.txt"));
    zip_path = zip_path.AppendASCII("foo");
    zip_contents_.insert(zip_path);
    zip_contents_.insert(zip_path.AppendASCII("bar.txt"));
    zip_path = zip_path.AppendASCII("bar");
    zip_contents_.insert(zip_path);
    zip_contents_.insert(zip_path.AppendASCII("baz.txt"));
    zip_contents_.insert(zip_path.AppendASCII("quux.txt"));
    zip_path = zip_path.AppendASCII("baz");
    zip_contents_.insert(zip_path);
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  void TestZipFile(const FilePath::StringType& filename) {
    FilePath test_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    test_dir = test_dir.AppendASCII("unzip");
    FilePath path = test_dir.Append(filename);

    ASSERT_TRUE(file_util::PathExists(path)) << "no file " << path.value();
    std::vector<FilePath> out_files;
    ASSERT_TRUE(Unzip(path, test_dir_, &out_files));

    file_util::FileEnumerator files(test_dir_, true,
        file_util::FileEnumerator::FILES_AND_DIRECTORIES);
    FilePath next_path = files.Next();
    size_t count = 0;
    while (!next_path.value().empty()) {
      EXPECT_EQ(zip_contents_.count(next_path), 1U) <<
          "Couldn't find " << next_path.value();
      count++;
      next_path = files.Next();
    }
    EXPECT_EQ(count, zip_contents_.size());
    EXPECT_EQ(count, out_files.size());
    std::vector<FilePath>::iterator iter;
    for (iter = out_files.begin(); iter != out_files.end(); ++iter) {
      EXPECT_EQ(zip_contents_.count(*iter), 1U) <<
          "Couldn't find " << (*iter).value();
    }
  }

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;

  // hard-coded contents of a known zip file
  std::set<FilePath> zip_contents_;
};


TEST_F(UnzipTest, Unzip) {
  TestZipFile(FILE_PATH_LITERAL("test.zip"));
}

TEST_F(UnzipTest, UnzipUncompressed) {
  TestZipFile(FILE_PATH_LITERAL("test_nocompress.zip"));
}

}  // namespace
