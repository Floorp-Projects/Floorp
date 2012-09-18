// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#include <fstream>
#include <iostream>
#include <set>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/string_util.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)

namespace {

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest
class FileUtilTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();
    // Name a subdirectory of the temp directory.
    ASSERT_TRUE(PathService::Get(base::DIR_TEMP, &test_dir_));
    test_dir_ = test_dir_.Append(FILE_PATH_LITERAL("FileUtilTest"));

    // Create a fresh, empty copy of this directory.
    file_util::Delete(test_dir_, true);
    file_util::CreateDirectory(test_dir_);
  }
  virtual void TearDown() {
    PlatformTest::TearDown();
    // Clean up test directory
    ASSERT_TRUE(file_util::Delete(test_dir_, true));
    ASSERT_FALSE(file_util::PathExists(test_dir_));
  }

  // the path to temporary directory used to contain the test operations
  FilePath test_dir_;
};

// Collects all the results from the given file enumerator, and provides an
// interface to query whether a given file is present.
class FindResultCollector {
 public:
  FindResultCollector(file_util::FileEnumerator& enumerator) {
    FilePath cur_file;
    while (!(cur_file = enumerator.Next()).value().empty()) {
      FilePath::StringType path = cur_file.value();
      // The file should not be returned twice.
      EXPECT_TRUE(files_.end() == files_.find(path))
          << "Same file returned twice";

      // Save for later.
      files_.insert(path);
    }
  }

  // Returns true if the enumerator found the file.
  bool HasFile(const FilePath& file) const {
    return files_.find(file.value()) != files_.end();
  }

  int size() {
    return static_cast<int>(files_.size());
  }

 private:
  std::set<FilePath::StringType> files_;
};

// Simple function to dump some text into a new file.
void CreateTextFile(const FilePath& filename,
                    const std::wstring& contents) {
  std::ofstream file;
  file.open(WideToUTF8(filename.ToWStringHack()).c_str());
  ASSERT_TRUE(file.is_open());
  file << contents;
  file.close();
}

// Simple function to take out some text from a file.
std::wstring ReadTextFile(const FilePath& filename) {
  wchar_t contents[64];
  std::wifstream file;
  file.open(WideToUTF8(filename.ToWStringHack()).c_str());
  EXPECT_TRUE(file.is_open());
  file.getline(contents, 64);
  file.close();
  return std::wstring(contents);
}

#if defined(OS_WIN)
uint64_t FileTimeAsUint64(const FILETIME& ft) {
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return u.QuadPart;
}
#endif

const struct append_case {
  const wchar_t* path;
  const wchar_t* ending;
  const wchar_t* result;
} append_cases[] = {
#if defined(OS_WIN)
  {L"c:\\colon\\backslash", L"path", L"c:\\colon\\backslash\\path"},
  {L"c:\\colon\\backslash\\", L"path", L"c:\\colon\\backslash\\path"},
  {L"c:\\colon\\backslash\\\\", L"path", L"c:\\colon\\backslash\\\\path"},
  {L"c:\\colon\\backslash\\", L"", L"c:\\colon\\backslash\\"},
  {L"c:\\colon\\backslash", L"", L"c:\\colon\\backslash\\"},
  {L"", L"path", L"\\path"},
  {L"", L"", L"\\"},
#elif defined(OS_POSIX)
  {L"/foo/bar", L"path", L"/foo/bar/path"},
  {L"/foo/bar/", L"path", L"/foo/bar/path"},
  {L"/foo/bar//", L"path", L"/foo/bar//path"},
  {L"/foo/bar/", L"", L"/foo/bar/"},
  {L"/foo/bar", L"", L"/foo/bar/"},
  {L"", L"path", L"/path"},
  {L"", L"", L"/"},
#endif
};

TEST_F(FileUtilTest, AppendToPath) {
  for (unsigned int i = 0; i < arraysize(append_cases); ++i) {
    const append_case& value = append_cases[i];
    std::wstring result = value.path;
    file_util::AppendToPath(&result, value.ending);
    EXPECT_EQ(value.result, result);
  }

#ifdef NDEBUG
  file_util::AppendToPath(NULL, L"path");  // asserts in debug mode
#endif
}

static const struct InsertBeforeExtensionCase {
  const FilePath::CharType* path;
  const FilePath::CharType* suffix;
  const FilePath::CharType* result;
} kInsertBeforeExtension[] = {
  {FPL(""), FPL(""), FPL("")},
  {FPL(""), FPL("txt"), FPL("txt")},
  {FPL("."), FPL("txt"), FPL("txt.")},
  {FPL("."), FPL(""), FPL(".")},
  {FPL("foo.dll"), FPL("txt"), FPL("footxt.dll")},
  {FPL("foo.dll"), FPL(".txt"), FPL("foo.txt.dll")},
  {FPL("foo"), FPL("txt"), FPL("footxt")},
  {FPL("foo"), FPL(".txt"), FPL("foo.txt")},
  {FPL("foo.baz.dll"), FPL("txt"), FPL("foo.baztxt.dll")},
  {FPL("foo.baz.dll"), FPL(".txt"), FPL("foo.baz.txt.dll")},
  {FPL("foo.dll"), FPL(""), FPL("foo.dll")},
  {FPL("foo.dll"), FPL("."), FPL("foo..dll")},
  {FPL("foo"), FPL(""), FPL("foo")},
  {FPL("foo"), FPL("."), FPL("foo.")},
  {FPL("foo.baz.dll"), FPL(""), FPL("foo.baz.dll")},
  {FPL("foo.baz.dll"), FPL("."), FPL("foo.baz..dll")},
#if defined(OS_WIN)
  {FPL("\\"), FPL(""), FPL("\\")},
  {FPL("\\"), FPL("txt"), FPL("\\txt")},
  {FPL("\\."), FPL("txt"), FPL("\\txt.")},
  {FPL("\\."), FPL(""), FPL("\\.")},
  {FPL("C:\\bar\\foo.dll"), FPL("txt"), FPL("C:\\bar\\footxt.dll")},
  {FPL("C:\\bar.baz\\foodll"), FPL("txt"), FPL("C:\\bar.baz\\foodlltxt")},
  {FPL("C:\\bar.baz\\foo.dll"), FPL("txt"), FPL("C:\\bar.baz\\footxt.dll")},
  {FPL("C:\\bar.baz\\foo.dll.exe"), FPL("txt"),
   FPL("C:\\bar.baz\\foo.dlltxt.exe")},
  {FPL("C:\\bar.baz\\foo"), FPL(""), FPL("C:\\bar.baz\\foo")},
  {FPL("C:\\bar.baz\\foo.exe"), FPL(""), FPL("C:\\bar.baz\\foo.exe")},
  {FPL("C:\\bar.baz\\foo.dll.exe"), FPL(""), FPL("C:\\bar.baz\\foo.dll.exe")},
  {FPL("C:\\bar\\baz\\foo.exe"), FPL(" (1)"), FPL("C:\\bar\\baz\\foo (1).exe")},
#elif defined(OS_POSIX)
  {FPL("/"), FPL(""), FPL("/")},
  {FPL("/"), FPL("txt"), FPL("/txt")},
  {FPL("/."), FPL("txt"), FPL("/txt.")},
  {FPL("/."), FPL(""), FPL("/.")},
  {FPL("/bar/foo.dll"), FPL("txt"), FPL("/bar/footxt.dll")},
  {FPL("/bar.baz/foodll"), FPL("txt"), FPL("/bar.baz/foodlltxt")},
  {FPL("/bar.baz/foo.dll"), FPL("txt"), FPL("/bar.baz/footxt.dll")},
  {FPL("/bar.baz/foo.dll.exe"), FPL("txt"), FPL("/bar.baz/foo.dlltxt.exe")},
  {FPL("/bar.baz/foo"), FPL(""), FPL("/bar.baz/foo")},
  {FPL("/bar.baz/foo.exe"), FPL(""), FPL("/bar.baz/foo.exe")},
  {FPL("/bar.baz/foo.dll.exe"), FPL(""), FPL("/bar.baz/foo.dll.exe")},
  {FPL("/bar/baz/foo.exe"), FPL(" (1)"), FPL("/bar/baz/foo (1).exe")},
#endif
};

TEST_F(FileUtilTest, InsertBeforeExtensionTest) {
  for (unsigned int i = 0; i < arraysize(kInsertBeforeExtension); ++i) {
    FilePath path(kInsertBeforeExtension[i].path);
    file_util::InsertBeforeExtension(&path, kInsertBeforeExtension[i].suffix);
    EXPECT_EQ(kInsertBeforeExtension[i].result, path.value());
  }
}

static const struct filename_case {
  const wchar_t* path;
  const wchar_t* filename;
} filename_cases[] = {
#if defined(OS_WIN)
  {L"c:\\colon\\backslash", L"backslash"},
  {L"c:\\colon\\backslash\\", L""},
  {L"\\\\filename.exe", L"filename.exe"},
  {L"filename.exe", L"filename.exe"},
  {L"", L""},
  {L"\\\\\\", L""},
  {L"c:/colon/backslash", L"backslash"},
  {L"c:/colon/backslash/", L""},
  {L"//////", L""},
  {L"///filename.exe", L"filename.exe"},
#elif defined(OS_POSIX)
  {L"/foo/bar", L"bar"},
  {L"/foo/bar/", L""},
  {L"/filename.exe", L"filename.exe"},
  {L"filename.exe", L"filename.exe"},
  {L"", L""},
  {L"/", L""},
#endif
};

TEST_F(FileUtilTest, GetFilenameFromPath) {
  for (unsigned int i = 0; i < arraysize(filename_cases); ++i) {
    const filename_case& value = filename_cases[i];
    std::wstring result = file_util::GetFilenameFromPath(value.path);
    EXPECT_EQ(value.filename, result);
  }
}

// Test finding the file type from a path name
static const struct extension_case {
  const wchar_t* path;
  const wchar_t* extension;
} extension_cases[] = {
#if defined(OS_WIN)
  {L"C:\\colon\\backslash\\filename.extension", L"extension"},
  {L"C:\\colon\\backslash\\filename.", L""},
  {L"C:\\colon\\backslash\\filename", L""},
  {L"C:\\colon\\backslash\\", L""},
  {L"C:\\colon\\backslash.\\", L""},
  {L"C:\\colon\\backslash\filename.extension.extension2", L"extension2"},
#elif defined(OS_POSIX)
  {L"/foo/bar/filename.extension", L"extension"},
  {L"/foo/bar/filename.", L""},
  {L"/foo/bar/filename", L""},
  {L"/foo/bar/", L""},
  {L"/foo/bar./", L""},
  {L"/foo/bar/filename.extension.extension2", L"extension2"},
  {L".", L""},
  {L"..", L""},
  {L"./foo", L""},
  {L"./foo.extension", L"extension"},
  {L"/foo.extension1/bar.extension2", L"extension2"},
#endif
};

TEST_F(FileUtilTest, GetFileExtensionFromPath) {
  for (unsigned int i = 0; i < arraysize(extension_cases); ++i) {
    const extension_case& ext = extension_cases[i];
    const std::wstring fext = file_util::GetFileExtensionFromPath(ext.path);
    EXPECT_EQ(ext.extension, fext);
  }
}

// Test finding the directory component of a path
static const struct dir_case {
  const wchar_t* full_path;
  const wchar_t* directory;
} dir_cases[] = {
#if defined(OS_WIN)
  {L"C:\\WINDOWS\\system32\\gdi32.dll", L"C:\\WINDOWS\\system32"},
  {L"C:\\WINDOWS\\system32\\not_exist_thx_1138", L"C:\\WINDOWS\\system32"},
  {L"C:\\WINDOWS\\system32\\", L"C:\\WINDOWS\\system32"},
  {L"C:\\WINDOWS\\system32\\\\", L"C:\\WINDOWS\\system32"},
  {L"C:\\WINDOWS\\system32", L"C:\\WINDOWS"},
  {L"C:\\WINDOWS\\system32.\\", L"C:\\WINDOWS\\system32."},
  {L"C:\\", L"C:"},
#elif defined(OS_POSIX)
  {L"/foo/bar/gdi32.dll", L"/foo/bar"},
  {L"/foo/bar/not_exist_thx_1138", L"/foo/bar"},
  {L"/foo/bar/", L"/foo/bar"},
  {L"/foo/bar//", L"/foo/bar"},
  {L"/foo/bar", L"/foo"},
  {L"/foo/bar./", L"/foo/bar."},
  {L"/", L"/"},
  {L".", L"."},
  {L"..", L"."}, // yes, ".." technically lives in "."
#endif
};

TEST_F(FileUtilTest, GetDirectoryFromPath) {
  for (unsigned int i = 0; i < arraysize(dir_cases); ++i) {
    const dir_case& dir = dir_cases[i];
    const std::wstring parent =
        file_util::GetDirectoryFromPath(dir.full_path);
    EXPECT_EQ(dir.directory, parent);
  }
}

TEST_F(FileUtilTest, CountFilesCreatedAfter) {
  // Create old file (that we don't want to count)
  FilePath old_file_name = test_dir_.Append(FILE_PATH_LITERAL("Old File.txt"));
  CreateTextFile(old_file_name, L"Just call me Mr. Creakybits");

  // Age to perfection
#if defined(OS_WIN)
  PlatformThread::Sleep(100);
#elif defined(OS_POSIX)
  // We need to wait at least one second here because the precision of
  // file creation time is one second.
  PlatformThread::Sleep(1500);
#endif

  // Establish our cutoff time
  base::Time now(base::Time::NowFromSystemTime());
  EXPECT_EQ(0, file_util::CountFilesCreatedAfter(test_dir_, now));

  // Create a new file (that we do want to count)
  FilePath new_file_name = test_dir_.Append(FILE_PATH_LITERAL("New File.txt"));
  CreateTextFile(new_file_name, L"Waaaaaaaaaaaaaah.");

  // We should see only the new file.
  EXPECT_EQ(1, file_util::CountFilesCreatedAfter(test_dir_, now));

  // Delete new file, we should see no files after cutoff now
  EXPECT_TRUE(file_util::Delete(new_file_name, false));
  EXPECT_EQ(0, file_util::CountFilesCreatedAfter(test_dir_, now));
}

// Tests that the Delete function works as expected, especially
// the recursion flag.  Also coincidentally tests PathExists.
TEST_F(FileUtilTest, Delete) {
  // Create a file
  FilePath file_name = test_dir_.Append(FILE_PATH_LITERAL("Test File.txt"));
  CreateTextFile(file_name, L"I'm cannon fodder.");

  ASSERT_TRUE(file_util::PathExists(file_name));

  FilePath subdir_path = test_dir_.Append(FILE_PATH_LITERAL("Subdirectory"));
  file_util::CreateDirectory(subdir_path);

  ASSERT_TRUE(file_util::PathExists(subdir_path));

  FilePath directory_contents = test_dir_;
#if defined(OS_WIN)
  // TODO(erikkay): see if anyone's actually using this feature of the API
  directory_contents = directory_contents.Append(FILE_PATH_LITERAL("*"));
  // Delete non-recursively and check that only the file is deleted
  ASSERT_TRUE(file_util::Delete(directory_contents, false));
  EXPECT_FALSE(file_util::PathExists(file_name));
  EXPECT_TRUE(file_util::PathExists(subdir_path));
#endif

  // Delete recursively and make sure all contents are deleted
  ASSERT_TRUE(file_util::Delete(directory_contents, true));
  EXPECT_FALSE(file_util::PathExists(file_name));
  EXPECT_FALSE(file_util::PathExists(subdir_path));
}

TEST_F(FileUtilTest, Move) {
  // Create a directory
  FilePath dir_name_from =
      test_dir_.Append(FILE_PATH_LITERAL("Move_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Move the directory
  FilePath dir_name_to = test_dir_.Append(FILE_PATH_LITERAL("Move_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Move_Test_File.txt"));

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::Move(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(file_util::PathExists(dir_name_from));
  EXPECT_FALSE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}

TEST_F(FileUtilTest, CopyDirectoryRecursively) {
  // Create a directory.
  FilePath dir_name_from =
      test_dir_.Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  file_util::CreateDirectory(subdir_name_from);
  ASSERT_TRUE(file_util::PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name2_from));

  // Copy the directory recursively.
  FilePath dir_name_to =
      test_dir_.Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));
  FilePath file_name2_to =
      subdir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::CopyDirectory(dir_name_from, dir_name_to, true));

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(dir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(subdir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name2_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_TRUE(file_util::PathExists(subdir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name2_to));
}

TEST_F(FileUtilTest, CopyDirectory) {
  // Create a directory.
  FilePath dir_name_from =
      test_dir_.Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory.
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Create a subdirectory.
  FilePath subdir_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Subdir"));
  file_util::CreateDirectory(subdir_name_from);
  ASSERT_TRUE(file_util::PathExists(subdir_name_from));

  // Create a file under the subdirectory.
  FilePath file_name2_from =
      subdir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  CreateTextFile(file_name2_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name2_from));

  // Copy the directory not recursively.
  FilePath dir_name_to =
      test_dir_.Append(FILE_PATH_LITERAL("Copy_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  FilePath subdir_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("Subdir"));

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::CopyDirectory(dir_name_from, dir_name_to, false));

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(dir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(subdir_name_from));
  EXPECT_TRUE(file_util::PathExists(file_name2_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
  EXPECT_FALSE(file_util::PathExists(subdir_name_to));
}

TEST_F(FileUtilTest, CopyFile) {
  // Create a directory
  FilePath dir_name_from =
      test_dir_.Append(FILE_PATH_LITERAL("Copy_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("Copy_Test_File.txt"));
  const std::wstring file_contents(L"Gooooooooooooooooooooogle");
  CreateTextFile(file_name_from, file_contents);
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Copy the file.
  FilePath dest_file = dir_name_from.Append(FILE_PATH_LITERAL("DestFile.txt"));
  ASSERT_TRUE(file_util::CopyFile(file_name_from, dest_file));

  // Copy the file to another location using '..' in the path.
  std::wstring dest_file2(dir_name_from.ToWStringHack());
  file_util::AppendToPath(&dest_file2, L"..");
  file_util::AppendToPath(&dest_file2, L"DestFile.txt");
  ASSERT_TRUE(file_util::CopyFile(file_name_from,
                                  FilePath::FromWStringHack(dest_file2)));
  std::wstring dest_file2_test(dir_name_from.ToWStringHack());
  file_util::UpOneDirectory(&dest_file2_test);
  file_util::AppendToPath(&dest_file2_test, L"DestFile.txt");

  // Check everything has been copied.
  EXPECT_TRUE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dest_file));
  const std::wstring read_contents = ReadTextFile(dest_file);
  EXPECT_EQ(file_contents, read_contents);
  EXPECT_TRUE(file_util::PathExists(
      FilePath::FromWStringHack(dest_file2_test)));
  EXPECT_TRUE(file_util::PathExists(FilePath::FromWStringHack(dest_file2)));
}

// TODO(erikkay): implement
#if defined(OS_WIN)
TEST_F(FileUtilTest, GetFileCreationLocalTime) {
  FilePath file_name = test_dir_.Append(L"Test File.txt");

  SYSTEMTIME start_time;
  GetLocalTime(&start_time);
  Sleep(100);
  CreateTextFile(file_name, L"New file!");
  Sleep(100);
  SYSTEMTIME end_time;
  GetLocalTime(&end_time);

  SYSTEMTIME file_creation_time;
  file_util::GetFileCreationLocalTime(file_name.value(), &file_creation_time);

  FILETIME start_filetime;
  SystemTimeToFileTime(&start_time, &start_filetime);
  FILETIME end_filetime;
  SystemTimeToFileTime(&end_time, &end_filetime);
  FILETIME file_creation_filetime;
  SystemTimeToFileTime(&file_creation_time, &file_creation_filetime);

  EXPECT_EQ(-1, CompareFileTime(&start_filetime, &file_creation_filetime)) <<
    "start time: " << FileTimeAsUint64(start_filetime) << ", " <<
    "creation time: " << FileTimeAsUint64(file_creation_filetime);

  EXPECT_EQ(-1, CompareFileTime(&file_creation_filetime, &end_filetime)) <<
    "creation time: " << FileTimeAsUint64(file_creation_filetime) << ", " <<
    "end time: " << FileTimeAsUint64(end_filetime);

  ASSERT_TRUE(DeleteFile(file_name.value().c_str()));
}
#endif

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest.
typedef PlatformTest ReadOnlyFileUtilTest;

TEST_F(ReadOnlyFileUtilTest, ContentsEqual) {
  FilePath data_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("base"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(FILE_PATH_LITERAL("file_util_unittest"));
  ASSERT_TRUE(file_util::PathExists(data_dir));

  FilePath original_file =
      data_dir.Append(FILE_PATH_LITERAL("original.txt"));
  FilePath same_file =
      data_dir.Append(FILE_PATH_LITERAL("same.txt"));
  FilePath same_length_file =
      data_dir.Append(FILE_PATH_LITERAL("same_length.txt"));
  FilePath different_file =
      data_dir.Append(FILE_PATH_LITERAL("different.txt"));
  FilePath different_first_file =
      data_dir.Append(FILE_PATH_LITERAL("different_first.txt"));
  FilePath different_last_file =
      data_dir.Append(FILE_PATH_LITERAL("different_last.txt"));
  FilePath empty1_file =
      data_dir.Append(FILE_PATH_LITERAL("empty1.txt"));
  FilePath empty2_file =
      data_dir.Append(FILE_PATH_LITERAL("empty2.txt"));
  FilePath shortened_file =
      data_dir.Append(FILE_PATH_LITERAL("shortened.txt"));
  FilePath binary_file =
      data_dir.Append(FILE_PATH_LITERAL("binary_file.bin"));
  FilePath binary_file_same =
      data_dir.Append(FILE_PATH_LITERAL("binary_file_same.bin"));
  FilePath binary_file_diff =
      data_dir.Append(FILE_PATH_LITERAL("binary_file_diff.bin"));

  EXPECT_TRUE(file_util::ContentsEqual(original_file, original_file));
  EXPECT_TRUE(file_util::ContentsEqual(original_file, same_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, same_length_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, different_file));
  EXPECT_FALSE(file_util::ContentsEqual(L"bogusname", L"bogusname"));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, different_first_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, different_last_file));
  EXPECT_TRUE(file_util::ContentsEqual(empty1_file, empty2_file));
  EXPECT_FALSE(file_util::ContentsEqual(original_file, shortened_file));
  EXPECT_FALSE(file_util::ContentsEqual(shortened_file, original_file));
  EXPECT_TRUE(file_util::ContentsEqual(binary_file, binary_file_same));
  EXPECT_FALSE(file_util::ContentsEqual(binary_file, binary_file_diff));
}

// We don't need equivalent functionality outside of Windows.
#if defined(OS_WIN)
TEST_F(FileUtilTest, ResolveShortcutTest) {
  FilePath target_file = test_dir_.Append(L"Target.txt");
  CreateTextFile(target_file, L"This is the target.");

  FilePath link_file = test_dir_.Append(L"Link.lnk");

  HRESULT result;
  IShellLink *shell = NULL;
  IPersistFile *persist = NULL;

  CoInitialize(NULL);
  // Temporarily create a shortcut for test
  result = CoCreateInstance(CLSID_ShellLink, NULL,
                          CLSCTX_INPROC_SERVER, IID_IShellLink,
                          reinterpret_cast<LPVOID*>(&shell));
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->QueryInterface(IID_IPersistFile,
                             reinterpret_cast<LPVOID*>(&persist));
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->SetPath(target_file.value().c_str());
  EXPECT_TRUE(SUCCEEDED(result));
  result = shell->SetDescription(L"ResolveShortcutTest");
  EXPECT_TRUE(SUCCEEDED(result));
  result = persist->Save(link_file.value().c_str(), TRUE);
  EXPECT_TRUE(SUCCEEDED(result));
  if (persist)
    persist->Release();
  if (shell)
    shell->Release();

  bool is_solved;
  std::wstring link_file_str = link_file.value();
  is_solved = file_util::ResolveShortcut(&link_file_str);
  EXPECT_TRUE(is_solved);
  std::wstring contents;
  contents = ReadTextFile(FilePath(link_file_str));
  EXPECT_EQ(L"This is the target.", contents);

  // Cleaning
  DeleteFile(target_file.value().c_str());
  DeleteFile(link_file_str.c_str());
  CoUninitialize();
}

TEST_F(FileUtilTest, CreateShortcutTest) {
  const wchar_t file_contents[] = L"This is another target.";
  FilePath target_file = test_dir_.Append(L"Target1.txt");
  CreateTextFile(target_file, file_contents);

  FilePath link_file = test_dir_.Append(L"Link1.lnk");

  CoInitialize(NULL);
  EXPECT_TRUE(file_util::CreateShortcutLink(target_file.value().c_str(),
                                            link_file.value().c_str(),
                                            NULL, NULL, NULL, NULL, 0));
  std::wstring resolved_name = link_file.value();
  EXPECT_TRUE(file_util::ResolveShortcut(&resolved_name));
  std::wstring read_contents = ReadTextFile(FilePath(resolved_name));
  EXPECT_EQ(file_contents, read_contents);

  DeleteFile(target_file.value().c_str());
  DeleteFile(link_file.value().c_str());
  CoUninitialize();
}

TEST_F(FileUtilTest, CopyAndDeleteDirectoryTest) {
  // Create a directory
  FilePath dir_name_from =
      test_dir_.Append(FILE_PATH_LITERAL("CopyAndDelete_From_Subdir"));
  file_util::CreateDirectory(dir_name_from);
  ASSERT_TRUE(file_util::PathExists(dir_name_from));

  // Create a file under the directory
  FilePath file_name_from =
      dir_name_from.Append(FILE_PATH_LITERAL("CopyAndDelete_Test_File.txt"));
  CreateTextFile(file_name_from, L"Gooooooooooooooooooooogle");
  ASSERT_TRUE(file_util::PathExists(file_name_from));

  // Move the directory by using CopyAndDeleteDirectory
  FilePath dir_name_to = test_dir_.Append(
      FILE_PATH_LITERAL("CopyAndDelete_To_Subdir"));
  FilePath file_name_to =
      dir_name_to.Append(FILE_PATH_LITERAL("CopyAndDelete_Test_File.txt"));

  ASSERT_FALSE(file_util::PathExists(dir_name_to));

  EXPECT_TRUE(file_util::CopyAndDeleteDirectory(dir_name_from, dir_name_to));

  // Check everything has been moved.
  EXPECT_FALSE(file_util::PathExists(dir_name_from));
  EXPECT_FALSE(file_util::PathExists(file_name_from));
  EXPECT_TRUE(file_util::PathExists(dir_name_to));
  EXPECT_TRUE(file_util::PathExists(file_name_to));
}
#endif

TEST_F(FileUtilTest, CreateTemporaryFileNameTest) {
  std::wstring temp_files[3];
  for (int i = 0; i < 3; i++) {
    ASSERT_TRUE(file_util::CreateTemporaryFileName(&(temp_files[i])));
    EXPECT_TRUE(file_util::PathExists(temp_files[i]));
    EXPECT_FALSE(file_util::DirectoryExists(temp_files[i]));
  }
  for (int i = 0; i < 3; i++)
    EXPECT_FALSE(temp_files[i] == temp_files[(i+1)%3]);
  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(file_util::Delete(temp_files[i], false));
}

TEST_F(FileUtilTest, CreateAndOpenTemporaryFileNameTest) {
  FilePath names[3];
  FILE *fps[3];
  int i;

  // Create; make sure they are open and exist.
  for (i = 0; i < 3; ++i) {
    fps[i] = file_util::CreateAndOpenTemporaryFile(&(names[i]));
    ASSERT_TRUE(fps[i]);
    EXPECT_TRUE(file_util::PathExists(names[i]));
  }

  // Make sure all names are unique.
  for (i = 0; i < 3; ++i) {
    EXPECT_FALSE(names[i] == names[(i+1)%3]);
  }

  // Close and delete.
  for (i = 0; i < 3; ++i) {
    EXPECT_TRUE(file_util::CloseFile(fps[i]));
    EXPECT_TRUE(file_util::Delete(names[i], false));
  }
}

TEST_F(FileUtilTest, CreateNewTempDirectoryTest) {
  std::wstring temp_dir;
  ASSERT_TRUE(file_util::CreateNewTempDirectory(std::wstring(), &temp_dir));
  EXPECT_TRUE(file_util::PathExists(temp_dir));
  EXPECT_TRUE(file_util::Delete(temp_dir, false));
}

TEST_F(FileUtilTest, GetShmemTempDirTest) {
  FilePath dir;
  EXPECT_TRUE(file_util::GetShmemTempDir(&dir));
  EXPECT_TRUE(file_util::DirectoryExists(dir));
}

TEST_F(FileUtilTest, CreateDirectoryTest) {
  FilePath test_root =
      test_dir_.Append(FILE_PATH_LITERAL("create_directory_test"));
#if defined(OS_WIN)
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("dir\\tree\\likely\\doesnt\\exist\\"));
#elif defined(OS_POSIX)
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("dir/tree/likely/doesnt/exist/"));
#endif

  EXPECT_FALSE(file_util::PathExists(test_path));
  EXPECT_TRUE(file_util::CreateDirectory(test_path));
  EXPECT_TRUE(file_util::PathExists(test_path));
  // CreateDirectory returns true if the DirectoryExists returns true.
  EXPECT_TRUE(file_util::CreateDirectory(test_path));

  // Doesn't work to create it on top of a non-dir
  test_path = test_path.Append(FILE_PATH_LITERAL("foobar.txt"));
  EXPECT_FALSE(file_util::PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(file_util::PathExists(test_path));
  EXPECT_FALSE(file_util::CreateDirectory(test_path));

  EXPECT_TRUE(file_util::Delete(test_root, true));
  EXPECT_FALSE(file_util::PathExists(test_root));
  EXPECT_FALSE(file_util::PathExists(test_path));
}

TEST_F(FileUtilTest, DetectDirectoryTest) {
  // Check a directory
  FilePath test_root =
      test_dir_.Append(FILE_PATH_LITERAL("detect_directory_test"));
  EXPECT_FALSE(file_util::PathExists(test_root));
  EXPECT_TRUE(file_util::CreateDirectory(test_root));
  EXPECT_TRUE(file_util::PathExists(test_root));
  EXPECT_TRUE(file_util::DirectoryExists(test_root));

  // Check a file
  FilePath test_path =
      test_root.Append(FILE_PATH_LITERAL("foobar.txt"));
  EXPECT_FALSE(file_util::PathExists(test_path));
  CreateTextFile(test_path, L"test file");
  EXPECT_TRUE(file_util::PathExists(test_path));
  EXPECT_FALSE(file_util::DirectoryExists(test_path));
  EXPECT_TRUE(file_util::Delete(test_path, false));

  EXPECT_TRUE(file_util::Delete(test_root, true));
}

static const struct goodbad_pair {
  std::wstring bad_name;
  std::wstring good_name;
} kIllegalCharacterCases[] = {
  {L"bad*file:name?.jpg", L"bad-file-name-.jpg"},
  {L"**********::::.txt", L"--------------.txt"},
  // We can't use UCNs (universal character names) for C0/C1 characters and
  // U+007F, but \x escape is interpreted by MSVC and gcc as we intend.
  {L"bad\x0003\x0091 file\u200E\u200Fname.png", L"bad-- file--name.png"},
#if defined(OS_WIN)
  {L"bad*file\\name.jpg", L"bad-file-name.jpg"},
  {L"\t  bad*file\\name/.jpg ", L"bad-file-name-.jpg"},
  {L"bad\uFFFFfile\U0010FFFEname.jpg ", L"bad-file-name.jpg"},
#elif defined(OS_POSIX)
  {L"bad*file?name.jpg", L"bad-file-name.jpg"},
  {L"\t  bad*file?name/.jpg ", L"bad-file-name-.jpg"},
  {L"bad\uFFFFfile-name.jpg ", L"bad-file-name.jpg"},
#endif
  {L"this_file_name is okay!.mp3", L"this_file_name is okay!.mp3"},
  {L"\u4E00\uAC00.mp3", L"\u4E00\uAC00.mp3"},
  {L"\u0635\u200C\u0644.mp3", L"\u0635\u200C\u0644.mp3"},
  {L"\U00010330\U00010331.mp3", L"\U00010330\U00010331.mp3"},
  // Unassigned codepoints are ok.
  {L"\u0378\U00040001.mp3", L"\u0378\U00040001.mp3"},
};

TEST_F(FileUtilTest, ReplaceIllegalCharactersTest) {
  for (unsigned int i = 0; i < arraysize(kIllegalCharacterCases); ++i) {
    std::wstring bad_name(kIllegalCharacterCases[i].bad_name);
    file_util::ReplaceIllegalCharacters(&bad_name, L'-');
    EXPECT_EQ(kIllegalCharacterCases[i].good_name, bad_name);
  }
}

static const struct ReplaceExtensionCase {
  std::wstring file_name;
  FilePath::StringType extension;
  std::wstring result;
} kReplaceExtension[] = {
  {L"", FILE_PATH_LITERAL(""), L""},
  {L"", FILE_PATH_LITERAL("txt"), L".txt"},
  {L".", FILE_PATH_LITERAL("txt"), L".txt"},
  {L".", FILE_PATH_LITERAL(""), L""},
  {L"foo.dll", FILE_PATH_LITERAL("txt"), L"foo.txt"},
  {L"foo.dll", FILE_PATH_LITERAL(".txt"), L"foo.txt"},
  {L"foo", FILE_PATH_LITERAL("txt"), L"foo.txt"},
  {L"foo", FILE_PATH_LITERAL(".txt"), L"foo.txt"},
  {L"foo.baz.dll", FILE_PATH_LITERAL("txt"), L"foo.baz.txt"},
  {L"foo.baz.dll", FILE_PATH_LITERAL(".txt"), L"foo.baz.txt"},
  {L"foo.dll", FILE_PATH_LITERAL(""), L"foo"},
  {L"foo.dll", FILE_PATH_LITERAL("."), L"foo"},
  {L"foo", FILE_PATH_LITERAL(""), L"foo"},
  {L"foo", FILE_PATH_LITERAL("."), L"foo"},
  {L"foo.baz.dll", FILE_PATH_LITERAL(""), L"foo.baz"},
  {L"foo.baz.dll", FILE_PATH_LITERAL("."), L"foo.baz"},
};

TEST_F(FileUtilTest, ReplaceExtensionTest) {
  for (unsigned int i = 0; i < arraysize(kReplaceExtension); ++i) {
    FilePath path = FilePath::FromWStringHack(kReplaceExtension[i].file_name);
    file_util::ReplaceExtension(&path, kReplaceExtension[i].extension);
    EXPECT_EQ(kReplaceExtension[i].result, path.ToWStringHack());
  }
}

// Make sure ReplaceExtension doesn't replace an extension that occurs as one of
// the directory names of the path.
TEST_F(FileUtilTest, ReplaceExtensionTestWithPathSeparators) {
  FilePath path;
  path = path.Append(FILE_PATH_LITERAL("foo.bar"));
  path = path.Append(FILE_PATH_LITERAL("foo"));
  // '/foo.bar/foo' with extension '.baz'
  FilePath result_path = path;
  file_util::ReplaceExtension(&result_path, FILE_PATH_LITERAL(".baz"));
  EXPECT_EQ(path.value() + FILE_PATH_LITERAL(".baz"),
            result_path.value());
}

TEST_F(FileUtilTest, FileEnumeratorTest) {
  // Test an empty directory.
  file_util::FileEnumerator f0(test_dir_, true,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES);
  EXPECT_EQ(f0.Next().value(), FILE_PATH_LITERAL(""));
  EXPECT_EQ(f0.Next().value(), FILE_PATH_LITERAL(""));

  // create the directories
  FilePath dir1 = test_dir_.Append(FILE_PATH_LITERAL("dir1"));
  EXPECT_TRUE(file_util::CreateDirectory(dir1));
  FilePath dir2 = test_dir_.Append(FILE_PATH_LITERAL("dir2"));
  EXPECT_TRUE(file_util::CreateDirectory(dir2));
  FilePath dir2inner = dir2.Append(FILE_PATH_LITERAL("inner"));
  EXPECT_TRUE(file_util::CreateDirectory(dir2inner));

  // create the files
  FilePath dir2file = dir2.Append(FILE_PATH_LITERAL("dir2file.txt"));
  CreateTextFile(dir2file, L"");
  FilePath dir2innerfile = dir2inner.Append(FILE_PATH_LITERAL("innerfile.txt"));
  CreateTextFile(dir2innerfile, L"");
  FilePath file1 = test_dir_.Append(FILE_PATH_LITERAL("file1.txt"));
  CreateTextFile(file1, L"");
  FilePath file2_rel =
      dir2.Append(FilePath::kParentDirectory)
          .Append(FILE_PATH_LITERAL("file2.txt"));
  CreateTextFile(file2_rel, L"");
  FilePath file2_abs = test_dir_.Append(FILE_PATH_LITERAL("file2.txt"));

  // Only enumerate files.
  file_util::FileEnumerator f1(test_dir_, true,
                               file_util::FileEnumerator::FILES);
  FindResultCollector c1(f1);
  EXPECT_TRUE(c1.HasFile(file1));
  EXPECT_TRUE(c1.HasFile(file2_abs));
  EXPECT_TRUE(c1.HasFile(dir2file));
  EXPECT_TRUE(c1.HasFile(dir2innerfile));
  EXPECT_EQ(c1.size(), 4);

  // Only enumerate directories.
  file_util::FileEnumerator f2(test_dir_, true,
                               file_util::FileEnumerator::DIRECTORIES);
  FindResultCollector c2(f2);
  EXPECT_TRUE(c2.HasFile(dir1));
  EXPECT_TRUE(c2.HasFile(dir2));
  EXPECT_TRUE(c2.HasFile(dir2inner));
  EXPECT_EQ(c2.size(), 3);

  // Only enumerate directories non-recursively.
  file_util::FileEnumerator f2_non_recursive(
      test_dir_, false, file_util::FileEnumerator::DIRECTORIES);
  FindResultCollector c2_non_recursive(f2_non_recursive);
  EXPECT_TRUE(c2_non_recursive.HasFile(dir1));
  EXPECT_TRUE(c2_non_recursive.HasFile(dir2));
  EXPECT_EQ(c2_non_recursive.size(), 2);

  // Enumerate files and directories.
  file_util::FileEnumerator f3(test_dir_, true,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES);
  FindResultCollector c3(f3);
  EXPECT_TRUE(c3.HasFile(dir1));
  EXPECT_TRUE(c3.HasFile(dir2));
  EXPECT_TRUE(c3.HasFile(file1));
  EXPECT_TRUE(c3.HasFile(file2_abs));
  EXPECT_TRUE(c3.HasFile(dir2file));
  EXPECT_TRUE(c3.HasFile(dir2inner));
  EXPECT_TRUE(c3.HasFile(dir2innerfile));
  EXPECT_EQ(c3.size(), 7);

  // Non-recursive operation.
  file_util::FileEnumerator f4(test_dir_, false,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES);
  FindResultCollector c4(f4);
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(dir2));
  EXPECT_TRUE(c4.HasFile(file1));
  EXPECT_TRUE(c4.HasFile(file2_abs));
  EXPECT_EQ(c4.size(), 4);

  // Enumerate with a pattern.
  file_util::FileEnumerator f5(test_dir_, true,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES,
      FILE_PATH_LITERAL("dir*"));
  FindResultCollector c5(f5);
  EXPECT_TRUE(c5.HasFile(dir1));
  EXPECT_TRUE(c5.HasFile(dir2));
  EXPECT_TRUE(c5.HasFile(dir2file));
  EXPECT_TRUE(c5.HasFile(dir2inner));
  EXPECT_TRUE(c5.HasFile(dir2innerfile));
  EXPECT_EQ(c5.size(), 5);

  // Make sure the destructor closes the find handle while in the middle of a
  // query to allow TearDown to delete the directory.
  file_util::FileEnumerator f6(test_dir_, true,
      file_util::FileEnumerator::FILES_AND_DIRECTORIES);
  EXPECT_FALSE(f6.Next().value().empty());  // Should have found something
                                            // (we don't care what).
}


void PathComponents(const std::wstring& path,
                    std::vector<std::wstring>* components) {
  DCHECK(components != NULL);
  if (components == NULL)
    return;
  std::wstring::size_type start = 0;
  std::wstring::size_type end = path.find('/', start);

  // Special case the "/" or "\" directory.  On Windows with a drive letter,
  // this code path won't hit, but the right thing should still happen.
  // "E:\foo" will turn into "E:","foo".
  if (end == start) {
    components->push_back(std::wstring(path, 0, 1));
    start = end + 1;
    end = path.find('/', start);
  }
  while (end != std::wstring::npos) {
    std::wstring component = std::wstring(path, start, end - start);
    components->push_back(component);
    start = end + 1;
    end = path.find('/', start);
  }
  std::wstring component = std::wstring(path, start);
  components->push_back(component);
}

static const struct PathComponentsCase {
  const FilePath::CharType* path;
  const FilePath::CharType* result;
} kPathComponents[] = {
  {FILE_PATH_LITERAL("/foo/bar/baz/"), FILE_PATH_LITERAL("/|foo|bar|baz|")},
  {FILE_PATH_LITERAL("/foo/bar/baz"), FILE_PATH_LITERAL("/|foo|bar|baz")},
  {FILE_PATH_LITERAL("e:/foo"), FILE_PATH_LITERAL("e:|foo")},
};

TEST_F(FileUtilTest, PathComponentsTest) {
  for (size_t i = 0; i < arraysize(kPathComponents); ++i) {
    FilePath path(kPathComponents[i].path);
    std::vector<FilePath::StringType> comps;
    file_util::PathComponents(path, &comps);

    FilePath::StringType result;
    for (size_t j = 0; j < comps.size(); ++j) {
      result.append(comps[j]);
      if (j < comps.size() - 1)
        result.append(FILE_PATH_LITERAL("|"), 1);
    }
    EXPECT_EQ(kPathComponents[i].result, result);
  }
}

TEST_F(FileUtilTest, Contains) {
  FilePath data_dir = test_dir_.Append(FILE_PATH_LITERAL("FilePathTest"));

  // Create a fresh, empty copy of this directory.
  if (file_util::PathExists(data_dir)) {
    ASSERT_TRUE(file_util::Delete(data_dir, true));
  }
  ASSERT_TRUE(file_util::CreateDirectory(data_dir));

  FilePath foo(data_dir.Append(FILE_PATH_LITERAL("foo")));
  FilePath bar(foo.Append(FILE_PATH_LITERAL("bar.txt")));
  FilePath baz(data_dir.Append(FILE_PATH_LITERAL("baz.txt")));
  FilePath foobar(data_dir.Append(FILE_PATH_LITERAL("foobar.txt")));

  // Annoyingly, the directories must actually exist in order for realpath(),
  // which Contains() relies on in posix, to work.
  ASSERT_TRUE(file_util::CreateDirectory(foo));
  std::string data("hello");
  ASSERT_TRUE(file_util::WriteFile(bar, data.c_str(), data.length()));
  ASSERT_TRUE(file_util::WriteFile(baz, data.c_str(), data.length()));
  ASSERT_TRUE(file_util::WriteFile(foobar, data.c_str(), data.length()));

  EXPECT_TRUE(file_util::ContainsPath(foo, bar));
  EXPECT_FALSE(file_util::ContainsPath(foo, baz));
  EXPECT_FALSE(file_util::ContainsPath(foo, foobar));
  EXPECT_FALSE(file_util::ContainsPath(foo, foo));

// Platform-specific concerns
  FilePath foo_caps(data_dir.Append(FILE_PATH_LITERAL("FOO")));
#if defined(OS_WIN)
  EXPECT_TRUE(file_util::ContainsPath(foo,
      foo_caps.Append(FILE_PATH_LITERAL("bar.txt"))));
  EXPECT_TRUE(file_util::ContainsPath(foo,
      FilePath(foo.value() + FILE_PATH_LITERAL("/bar.txt"))));
#elif defined(OS_LINUX)
  EXPECT_FALSE(file_util::ContainsPath(foo,
      foo_caps.Append(FILE_PATH_LITERAL("bar.txt"))));
#else
  // We can't really do this test on osx since the case-sensitivity of the
  // filesystem is configurable.
#endif
}

}  // namespace
