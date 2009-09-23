// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/file_version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FileVersionInfoTest : public testing::Test {
};

std::wstring GetTestDataPath() {
  std::wstring path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  file_util::AppendToPath(&path, L"base");
  file_util::AppendToPath(&path, L"data");
  file_util::AppendToPath(&path, L"file_version_info_unittest");
  return path;
}

}

#ifdef OS_WIN
TEST(FileVersionInfoTest, HardCodedProperties) {
  const wchar_t* kDLLNames[] = {
    L"FileVersionInfoTest1.dll"
  };

  const wchar_t* kExpectedValues[1][15] = {
      // FileVersionInfoTest.dll
      L"Goooooogle",                      // company_name
      L"Google",                          // company_short_name
      L"This is the product name",        // product_name
      L"This is the product short name",  // product_short_name
      L"The Internal Name",               // internal_name
      L"4.3.2.1",                         // product_version
      L"Private build property",          // private_build
      L"Special build property",          // special_build
      L"This is a particularly interesting comment",  // comments
      L"This is the original filename",   // original_filename
      L"This is my file description",     // file_description
      L"1.2.3.4",                         // file_version
      L"This is the legal copyright",     // legal_copyright
      L"This is the legal trademarks",    // legal_trademarks
      L"This is the last change",         // last_change

  };

  for (int i = 0; i < arraysize(kDLLNames); ++i) {
    std::wstring dll_path = GetTestDataPath();
    file_util::AppendToPath(&dll_path, kDLLNames[i]);

    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(dll_path));

    int j = 0;
    EXPECT_EQ(kExpectedValues[i][j++], version_info->company_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->company_short_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->product_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->product_short_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->internal_name());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->product_version());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->private_build());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->special_build());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->comments());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->original_filename());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->file_description());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->file_version());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->legal_copyright());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->legal_trademarks());
    EXPECT_EQ(kExpectedValues[i][j++], version_info->last_change());
  }
}
#endif

#ifdef OS_WIN
TEST(FileVersionInfoTest, IsOfficialBuild) {
  const wchar_t* kDLLNames[] = {
    L"FileVersionInfoTest1.dll",
    L"FileVersionInfoTest2.dll"
  };

  const bool kExpected[] = {
    true,
    false,
  };

  // Test consistency check.
  ASSERT_EQ(arraysize(kDLLNames), arraysize(kExpected));

  for (int i = 0; i < arraysize(kDLLNames); ++i) {
    std::wstring dll_path = GetTestDataPath();
    file_util::AppendToPath(&dll_path, kDLLNames[i]);

    scoped_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(dll_path));

    EXPECT_EQ(kExpected[i], version_info->is_official_build());
  }
}
#endif

TEST(FileVersionInfoTest, CustomProperties) {
  std::wstring dll_path = GetTestDataPath();
  file_util::AppendToPath(&dll_path, L"FileVersionInfoTest1.dll");

  scoped_ptr<FileVersionInfo> version_info(
      FileVersionInfo::CreateFileVersionInfo(dll_path));

  // Test few existing properties.
  std::wstring str;
#ifdef OS_WIN
  EXPECT_TRUE(version_info->GetValue(L"Custom prop 1",  &str));
  EXPECT_EQ(L"Un", str);
  EXPECT_EQ(L"Un", version_info->GetStringValue(L"Custom prop 1"));

  EXPECT_TRUE(version_info->GetValue(L"Custom prop 2",  &str));
  EXPECT_EQ(L"Deux", str);
  EXPECT_EQ(L"Deux", version_info->GetStringValue(L"Custom prop 2"));

  EXPECT_TRUE(version_info->GetValue(L"Custom prop 3",  &str));
  EXPECT_EQ(L"1600 Amphitheatre Parkway Mountain View, CA 94043", str);
  EXPECT_EQ(L"1600 Amphitheatre Parkway Mountain View, CA 94043",
            version_info->GetStringValue(L"Custom prop 3"));
#endif

  // Test an non-existing property.
  EXPECT_FALSE(version_info->GetValue(L"Unknown property",  &str));
  EXPECT_EQ(L"", version_info->GetStringValue(L"Unknown property"));
}
