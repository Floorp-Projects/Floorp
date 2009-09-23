// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)

struct UnaryTestData {
  const FilePath::CharType* input;
  const FilePath::CharType* expected;
};

struct UnaryBooleanTestData {
  const FilePath::CharType* input;
  bool expected;
};

struct BinaryTestData {
  const FilePath::CharType* inputs[2];
  const FilePath::CharType* expected;
};

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest
class FilePathTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    PlatformTest::SetUp();
  }
  virtual void TearDown() {
    PlatformTest::TearDown();
  }
};

TEST_F(FilePathTest, DirName) {
  const struct UnaryTestData cases[] = {
    { FPL(""),              FPL(".") },
    { FPL("aa"),            FPL(".") },
    { FPL("/aa/bb"),        FPL("/aa") },
    { FPL("/aa/bb/"),       FPL("/aa") },
    { FPL("/aa/bb//"),      FPL("/aa") },
    { FPL("/aa/bb/ccc"),    FPL("/aa/bb") },
    { FPL("/aa"),           FPL("/") },
    { FPL("/aa/"),          FPL("/") },
    { FPL("/"),             FPL("/") },
    { FPL("//"),            FPL("//") },
    { FPL("///"),           FPL("/") },
    { FPL("aa/"),           FPL(".") },
    { FPL("aa/bb"),         FPL("aa") },
    { FPL("aa/bb/"),        FPL("aa") },
    { FPL("aa/bb//"),       FPL("aa") },
    { FPL("aa//bb//"),      FPL("aa") },
    { FPL("aa//bb/"),       FPL("aa") },
    { FPL("aa//bb"),        FPL("aa") },
    { FPL("//aa/bb"),       FPL("//aa") },
    { FPL("//aa/"),         FPL("//") },
    { FPL("//aa"),          FPL("//") },
    { FPL("0:"),            FPL(".") },
    { FPL("@:"),            FPL(".") },
    { FPL("[:"),            FPL(".") },
    { FPL("`:"),            FPL(".") },
    { FPL("{:"),            FPL(".") },
    { FPL("\xB3:"),         FPL(".") },
    { FPL("\xC5:"),         FPL(".") },
#if defined(OS_WIN)
    { FPL("\x0143:"),       FPL(".") },
#endif  // OS_WIN
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("c:"),            FPL("c:") },
    { FPL("C:"),            FPL("C:") },
    { FPL("A:"),            FPL("A:") },
    { FPL("Z:"),            FPL("Z:") },
    { FPL("a:"),            FPL("a:") },
    { FPL("z:"),            FPL("z:") },
    { FPL("c:aa"),          FPL("c:") },
    { FPL("c:/"),           FPL("c:/") },
    { FPL("c://"),          FPL("c://") },
    { FPL("c:///"),         FPL("c:/") },
    { FPL("c:/aa"),         FPL("c:/") },
    { FPL("c:/aa/"),        FPL("c:/") },
    { FPL("c:/aa/bb"),      FPL("c:/aa") },
    { FPL("c:aa/bb"),       FPL("c:aa") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { FPL("\\aa\\bb"),      FPL("\\aa") },
    { FPL("\\aa\\bb\\"),    FPL("\\aa") },
    { FPL("\\aa\\bb\\\\"),  FPL("\\aa") },
    { FPL("\\aa\\bb\\ccc"), FPL("\\aa\\bb") },
    { FPL("\\aa"),          FPL("\\") },
    { FPL("\\aa\\"),        FPL("\\") },
    { FPL("\\"),            FPL("\\") },
    { FPL("\\\\"),          FPL("\\\\") },
    { FPL("\\\\\\"),        FPL("\\") },
    { FPL("aa\\"),          FPL(".") },
    { FPL("aa\\bb"),        FPL("aa") },
    { FPL("aa\\bb\\"),      FPL("aa") },
    { FPL("aa\\bb\\\\"),    FPL("aa") },
    { FPL("aa\\\\bb\\\\"),  FPL("aa") },
    { FPL("aa\\\\bb\\"),    FPL("aa") },
    { FPL("aa\\\\bb"),      FPL("aa") },
    { FPL("\\\\aa\\bb"),    FPL("\\\\aa") },
    { FPL("\\\\aa\\"),      FPL("\\\\") },
    { FPL("\\\\aa"),        FPL("\\\\") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("c:\\"),          FPL("c:\\") },
    { FPL("c:\\\\"),        FPL("c:\\\\") },
    { FPL("c:\\\\\\"),      FPL("c:\\") },
    { FPL("c:\\aa"),        FPL("c:\\") },
    { FPL("c:\\aa\\"),      FPL("c:\\") },
    { FPL("c:\\aa\\bb"),    FPL("c:\\aa") },
    { FPL("c:aa\\bb"),      FPL("c:aa") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    FilePath observed = input.DirName();
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed.value()) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, BaseName) {
  const struct UnaryTestData cases[] = {
    { FPL(""),              FPL("") },
    { FPL("aa"),            FPL("aa") },
    { FPL("/aa/bb"),        FPL("bb") },
    { FPL("/aa/bb/"),       FPL("bb") },
    { FPL("/aa/bb//"),      FPL("bb") },
    { FPL("/aa/bb/ccc"),    FPL("ccc") },
    { FPL("/aa"),           FPL("aa") },
    { FPL("/"),             FPL("/") },
    { FPL("//"),            FPL("//") },
    { FPL("///"),           FPL("/") },
    { FPL("aa/"),           FPL("aa") },
    { FPL("aa/bb"),         FPL("bb") },
    { FPL("aa/bb/"),        FPL("bb") },
    { FPL("aa/bb//"),       FPL("bb") },
    { FPL("aa//bb//"),      FPL("bb") },
    { FPL("aa//bb/"),       FPL("bb") },
    { FPL("aa//bb"),        FPL("bb") },
    { FPL("//aa/bb"),       FPL("bb") },
    { FPL("//aa/"),         FPL("aa") },
    { FPL("//aa"),          FPL("aa") },
    { FPL("0:"),            FPL("0:") },
    { FPL("@:"),            FPL("@:") },
    { FPL("[:"),            FPL("[:") },
    { FPL("`:"),            FPL("`:") },
    { FPL("{:"),            FPL("{:") },
    { FPL("\xB3:"),         FPL("\xB3:") },
    { FPL("\xC5:"),         FPL("\xC5:") },
#if defined(OS_WIN)
    { FPL("\x0143:"),       FPL("\x0143:") },
#endif  // OS_WIN
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("c:"),            FPL("") },
    { FPL("C:"),            FPL("") },
    { FPL("A:"),            FPL("") },
    { FPL("Z:"),            FPL("") },
    { FPL("a:"),            FPL("") },
    { FPL("z:"),            FPL("") },
    { FPL("c:aa"),          FPL("aa") },
    { FPL("c:/"),           FPL("/") },
    { FPL("c://"),          FPL("//") },
    { FPL("c:///"),         FPL("/") },
    { FPL("c:/aa"),         FPL("aa") },
    { FPL("c:/aa/"),        FPL("aa") },
    { FPL("c:/aa/bb"),      FPL("bb") },
    { FPL("c:aa/bb"),       FPL("bb") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { FPL("\\aa\\bb"),      FPL("bb") },
    { FPL("\\aa\\bb\\"),    FPL("bb") },
    { FPL("\\aa\\bb\\\\"),  FPL("bb") },
    { FPL("\\aa\\bb\\ccc"), FPL("ccc") },
    { FPL("\\aa"),          FPL("aa") },
    { FPL("\\"),            FPL("\\") },
    { FPL("\\\\"),          FPL("\\\\") },
    { FPL("\\\\\\"),        FPL("\\") },
    { FPL("aa\\"),          FPL("aa") },
    { FPL("aa\\bb"),        FPL("bb") },
    { FPL("aa\\bb\\"),      FPL("bb") },
    { FPL("aa\\bb\\\\"),    FPL("bb") },
    { FPL("aa\\\\bb\\\\"),  FPL("bb") },
    { FPL("aa\\\\bb\\"),    FPL("bb") },
    { FPL("aa\\\\bb"),      FPL("bb") },
    { FPL("\\\\aa\\bb"),    FPL("bb") },
    { FPL("\\\\aa\\"),      FPL("aa") },
    { FPL("\\\\aa"),        FPL("aa") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("c:\\"),          FPL("\\") },
    { FPL("c:\\\\"),        FPL("\\\\") },
    { FPL("c:\\\\\\"),      FPL("\\") },
    { FPL("c:\\aa"),        FPL("aa") },
    { FPL("c:\\aa\\"),      FPL("aa") },
    { FPL("c:\\aa\\bb"),    FPL("bb") },
    { FPL("c:aa\\bb"),      FPL("bb") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    FilePath observed = input.BaseName();
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed.value()) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, Append) {
  const struct BinaryTestData cases[] = {
    { { FPL(""),           FPL("cc") }, FPL("cc") },
    { { FPL("."),          FPL("ff") }, FPL("ff") },
    { { FPL("/"),          FPL("cc") }, FPL("/cc") },
    { { FPL("/aa"),        FPL("") },   FPL("/aa") },
    { { FPL("/aa/"),       FPL("") },   FPL("/aa") },
    { { FPL("//aa"),       FPL("") },   FPL("//aa") },
    { { FPL("//aa/"),      FPL("") },   FPL("//aa") },
    { { FPL("//"),         FPL("aa") }, FPL("//aa") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { { FPL("c:"),         FPL("a") },  FPL("c:a") },
    { { FPL("c:"),         FPL("") },   FPL("c:") },
    { { FPL("c:/"),        FPL("a") },  FPL("c:/a") },
    { { FPL("c://"),       FPL("a") },  FPL("c://a") },
    { { FPL("c:///"),      FPL("a") },  FPL("c:/a") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    // Append introduces the default separator character, so these test cases
    // need to be defined with different expected results on platforms that use
    // different default separator characters.
    { { FPL("\\"),         FPL("cc") }, FPL("\\cc") },
    { { FPL("\\aa"),       FPL("") },   FPL("\\aa") },
    { { FPL("\\aa\\"),     FPL("") },   FPL("\\aa") },
    { { FPL("\\\\aa"),     FPL("") },   FPL("\\\\aa") },
    { { FPL("\\\\aa\\"),   FPL("") },   FPL("\\\\aa") },
    { { FPL("\\\\"),       FPL("aa") }, FPL("\\\\aa") },
    { { FPL("/aa/bb"),     FPL("cc") }, FPL("/aa/bb\\cc") },
    { { FPL("/aa/bb/"),    FPL("cc") }, FPL("/aa/bb\\cc") },
    { { FPL("aa/bb/"),     FPL("cc") }, FPL("aa/bb\\cc") },
    { { FPL("aa/bb"),      FPL("cc") }, FPL("aa/bb\\cc") },
    { { FPL("a/b"),        FPL("c") },  FPL("a/b\\c") },
    { { FPL("a/b/"),       FPL("c") },  FPL("a/b\\c") },
    { { FPL("//aa"),       FPL("bb") }, FPL("//aa\\bb") },
    { { FPL("//aa/"),      FPL("bb") }, FPL("//aa\\bb") },
    { { FPL("\\aa\\bb"),   FPL("cc") }, FPL("\\aa\\bb\\cc") },
    { { FPL("\\aa\\bb\\"), FPL("cc") }, FPL("\\aa\\bb\\cc") },
    { { FPL("aa\\bb\\"),   FPL("cc") }, FPL("aa\\bb\\cc") },
    { { FPL("aa\\bb"),     FPL("cc") }, FPL("aa\\bb\\cc") },
    { { FPL("a\\b"),       FPL("c") },  FPL("a\\b\\c") },
    { { FPL("a\\b\\"),     FPL("c") },  FPL("a\\b\\c") },
    { { FPL("\\\\aa"),     FPL("bb") }, FPL("\\\\aa\\bb") },
    { { FPL("\\\\aa\\"),   FPL("bb") }, FPL("\\\\aa\\bb") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { { FPL("c:\\"),       FPL("a") },  FPL("c:\\a") },
    { { FPL("c:\\\\"),     FPL("a") },  FPL("c:\\\\a") },
    { { FPL("c:\\\\\\"),   FPL("a") },  FPL("c:\\a") },
    { { FPL("c:\\"),       FPL("") },   FPL("c:\\") },
    { { FPL("c:\\a"),      FPL("b") },  FPL("c:\\a\\b") },
    { { FPL("c:\\a\\"),    FPL("b") },  FPL("c:\\a\\b") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#else  // FILE_PATH_USES_WIN_SEPARATORS
    { { FPL("/aa/bb"),     FPL("cc") }, FPL("/aa/bb/cc") },
    { { FPL("/aa/bb/"),    FPL("cc") }, FPL("/aa/bb/cc") },
    { { FPL("aa/bb/"),     FPL("cc") }, FPL("aa/bb/cc") },
    { { FPL("aa/bb"),      FPL("cc") }, FPL("aa/bb/cc") },
    { { FPL("a/b"),        FPL("c") },  FPL("a/b/c") },
    { { FPL("a/b/"),       FPL("c") },  FPL("a/b/c") },
    { { FPL("//aa"),       FPL("bb") }, FPL("//aa/bb") },
    { { FPL("//aa/"),      FPL("bb") }, FPL("//aa/bb") },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { { FPL("c:/"),        FPL("a") },  FPL("c:/a") },
    { { FPL("c:/"),        FPL("") },   FPL("c:/") },
    { { FPL("c:/a"),       FPL("b") },  FPL("c:/a/b") },
    { { FPL("c:/a/"),      FPL("b") },  FPL("c:/a/b") },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath root(cases[i].inputs[0]);
    FilePath::StringType leaf(cases[i].inputs[1]);
    FilePath observed_str = root.Append(leaf);
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed_str.value()) <<
              "i: " << i << ", root: " << root.value() << ", leaf: " << leaf;
    FilePath observed_path = root.Append(FilePath(leaf));
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed_path.value()) <<
              "i: " << i << ", root: " << root.value() << ", leaf: " << leaf;

    // TODO(erikkay): It would be nice to have a unicode test append value to
    // handle the case when AppendASCII is passed UTF8
#if defined(OS_WIN)
    std::string ascii = WideToASCII(leaf);
#elif defined(OS_POSIX)
    std::string ascii = leaf;
#endif
    observed_str = root.AppendASCII(ascii);
    EXPECT_EQ(FilePath::StringType(cases[i].expected), observed_str.value()) <<
              "i: " << i << ", root: " << root.value() << ", leaf: " << leaf;
  }
}

TEST_F(FilePathTest, IsAbsolute) {
  const struct UnaryBooleanTestData cases[] = {
    { FPL(""),       false },
    { FPL("a"),      false },
    { FPL("c:"),     false },
    { FPL("c:a"),    false },
    { FPL("a/b"),    false },
    { FPL("//"),     true },
    { FPL("//a"),    true },
    { FPL("c:a/b"),  false },
    { FPL("?:/a"),   false },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("/"),      false },
    { FPL("/a"),     false },
    { FPL("/."),     false },
    { FPL("/.."),    false },
    { FPL("c:/"),    true },
    { FPL("c:/a"),   true },
    { FPL("c:/."),   true },
    { FPL("c:/.."),  true },
    { FPL("C:/a"),   true },
    { FPL("d:/a"),   true },
#else  // FILE_PATH_USES_DRIVE_LETTERS
    { FPL("/"),      true },
    { FPL("/a"),     true },
    { FPL("/."),     true },
    { FPL("/.."),    true },
    { FPL("c:/"),    false },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { FPL("a\\b"),   false },
    { FPL("\\\\"),   true },
    { FPL("\\\\a"),  true },
    { FPL("a\\b"),   false },
    { FPL("\\\\"),   true },
    { FPL("//a"),    true },
    { FPL("c:a\\b"), false },
    { FPL("?:\\a"),  false },
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
    { FPL("\\"),     false },
    { FPL("\\a"),    false },
    { FPL("\\."),    false },
    { FPL("\\.."),   false },
    { FPL("c:\\"),   true },
    { FPL("c:\\"),   true },
    { FPL("c:\\a"),  true },
    { FPL("c:\\."),  true },
    { FPL("c:\\.."), true },
    { FPL("C:\\a"),  true },
    { FPL("d:\\a"),  true },
#else  // FILE_PATH_USES_DRIVE_LETTERS
    { FPL("\\"),     true },
    { FPL("\\a"),    true },
    { FPL("\\."),    true },
    { FPL("\\.."),   true },
    { FPL("c:\\"),   false },
#endif  // FILE_PATH_USES_DRIVE_LETTERS
#endif  // FILE_PATH_USES_WIN_SEPARATORS
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    FilePath input(cases[i].input);
    bool observed = input.IsAbsolute();
    EXPECT_EQ(cases[i].expected, observed) <<
              "i: " << i << ", input: " << input.value();
  }
}

TEST_F(FilePathTest, Extension) {
  FilePath base_dir(FILE_PATH_LITERAL("base_dir"));

  FilePath jpg = base_dir.Append(FILE_PATH_LITERAL("foo.jpg"));
  EXPECT_EQ(jpg.Extension(), FILE_PATH_LITERAL(".jpg"));

  FilePath base = jpg.BaseName().RemoveExtension();
  EXPECT_EQ(base.value(), FILE_PATH_LITERAL("foo"));

  FilePath path_no_ext = base_dir.Append(base);
  EXPECT_EQ(jpg.RemoveExtension().value(), path_no_ext.value());

  EXPECT_EQ(path_no_ext.value(), path_no_ext.RemoveExtension().value());
  EXPECT_EQ(path_no_ext.Extension(), FILE_PATH_LITERAL(""));
}

TEST_F(FilePathTest, Extension2) {
  const struct UnaryTestData cases[] = {
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { FPL("C:\\a\\b\\c.ext"),        FPL(".ext") },
    { FPL("C:\\a\\b\\c."),           FPL(".") },
    { FPL("C:\\a\\b\\c"),            FPL("") },
    { FPL("C:\\a\\b\\"),             FPL("") },
    { FPL("C:\\a\\b.\\"),            FPL(".") },
    { FPL("C:\\a\\b\\c.ext1.ext2"),  FPL(".ext2") },
    { FPL("C:\\foo.bar\\\\\\"),      FPL(".bar") },
    { FPL("C:\\foo.bar\\.."),        FPL("") },
    { FPL("C:\\foo.bar\\..\\\\"),    FPL("") },
#endif
    { FPL("/foo/bar/baz.ext"),       FPL(".ext") },
    { FPL("/foo/bar/baz."),          FPL(".") },
    { FPL("/foo/bar/baz.."),         FPL(".") },
    { FPL("/foo/bar/baz"),           FPL("") },
    { FPL("/foo/bar/"),              FPL("") },
    { FPL("/foo/bar./"),             FPL(".") },
    { FPL("/foo/bar/baz.ext1.ext2"), FPL(".ext2") },
    { FPL("."),                      FPL("") },
    { FPL(".."),                     FPL("") },
    { FPL("./foo"),                  FPL("") },
    { FPL("./foo.ext"),              FPL(".ext") },
    { FPL("/foo.ext1/bar.ext2"),     FPL(".ext2") },
    { FPL("/foo.bar////"),           FPL(".bar") },
    { FPL("/foo.bar/.."),            FPL("") },
    { FPL("/foo.bar/..////"),        FPL("") },
  };
  for (unsigned int i = 0; i < arraysize(cases); ++i) {
    FilePath path(cases[i].input);
    FilePath::StringType extension = path.Extension();
    EXPECT_STREQ(cases[i].expected, extension.c_str()) << "i: " << i <<
        ", path: " << path.value();
  }
}

TEST_F(FilePathTest, InsertBeforeExtension) {
  const struct BinaryTestData cases[] = {
    { { FPL(""),                FPL("") },        FPL("") },
    { { FPL(""),                FPL("txt") },     FPL("") },
    { { FPL("."),               FPL("txt") },     FPL("") },
    { { FPL(".."),              FPL("txt") },     FPL("") },
    { { FPL("foo.dll"),         FPL("txt") },     FPL("footxt.dll") },
    { { FPL("."),               FPL("") },        FPL(".") },
    { { FPL("foo.dll"),         FPL(".txt") },    FPL("foo.txt.dll") },
    { { FPL("foo"),             FPL("txt") },     FPL("footxt") },
    { { FPL("foo"),             FPL(".txt") },    FPL("foo.txt") },
    { { FPL("foo.baz.dll"),     FPL("txt") },     FPL("foo.baztxt.dll") },
    { { FPL("foo.baz.dll"),     FPL(".txt") },    FPL("foo.baz.txt.dll") },
    { { FPL("foo.dll"),         FPL("") },        FPL("foo.dll") },
    { { FPL("foo.dll"),         FPL(".") },       FPL("foo..dll") },
    { { FPL("foo"),             FPL("") },        FPL("foo") },
    { { FPL("foo"),             FPL(".") },       FPL("foo.") },
    { { FPL("foo.baz.dll"),     FPL("") },        FPL("foo.baz.dll") },
    { { FPL("foo.baz.dll"),     FPL(".") },       FPL("foo.baz..dll") },
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { { FPL("\\"),              FPL("") },        FPL("\\") },
    { { FPL("\\"),              FPL("txt") },     FPL("\\txt") },
    { { FPL("\\."),             FPL("txt") },     FPL("") },
    { { FPL("\\.."),            FPL("txt") },     FPL("") },
    { { FPL("\\."),             FPL("") },        FPL("\\.") },
    { { FPL("C:\\bar\\foo.dll"), FPL("txt") },
        FPL("C:\\bar\\footxt.dll") },
    { { FPL("C:\\bar.baz\\foodll"), FPL("txt") },
        FPL("C:\\bar.baz\\foodlltxt") },
    { { FPL("C:\\bar.baz\\foo.dll"), FPL("txt") },
        FPL("C:\\bar.baz\\footxt.dll") },
    { { FPL("C:\\bar.baz\\foo.dll.exe"), FPL("txt") },
        FPL("C:\\bar.baz\\foo.dlltxt.exe") },
    { { FPL("C:\\bar.baz\\foo"), FPL("") },
        FPL("C:\\bar.baz\\foo") },
    { { FPL("C:\\bar.baz\\foo.exe"), FPL("") },
        FPL("C:\\bar.baz\\foo.exe") },
    { { FPL("C:\\bar.baz\\foo.dll.exe"), FPL("") },
        FPL("C:\\bar.baz\\foo.dll.exe") },
    { { FPL("C:\\bar\\baz\\foo.exe"), FPL(" (1)") },
        FPL("C:\\bar\\baz\\foo (1).exe") },
    { { FPL("C:\\foo.baz\\\\"), FPL(" (1)") },    FPL("C:\\foo (1).baz") },
    { { FPL("C:\\foo.baz\\..\\"), FPL(" (1)") },  FPL("") },
#endif
    { { FPL("/"),               FPL("") },        FPL("/") },
    { { FPL("/"),               FPL("txt") },     FPL("/txt") },
    { { FPL("/."),              FPL("txt") },     FPL("") },
    { { FPL("/.."),             FPL("txt") },     FPL("") },
    { { FPL("/."),              FPL("") },        FPL("/.") },
    { { FPL("/bar/foo.dll"),    FPL("txt") },     FPL("/bar/footxt.dll") },
    { { FPL("/bar.baz/foodll"), FPL("txt") },     FPL("/bar.baz/foodlltxt") },
    { { FPL("/bar.baz/foo.dll"), FPL("txt") },    FPL("/bar.baz/footxt.dll") },
    { { FPL("/bar.baz/foo.dll.exe"), FPL("txt") },
        FPL("/bar.baz/foo.dlltxt.exe") },
    { { FPL("/bar.baz/foo"),    FPL("") },        FPL("/bar.baz/foo") },
    { { FPL("/bar.baz/foo.exe"), FPL("") },       FPL("/bar.baz/foo.exe") },
    { { FPL("/bar.baz/foo.dll.exe"), FPL("") },   FPL("/bar.baz/foo.dll.exe") },
    { { FPL("/bar/baz/foo.exe"), FPL(" (1)") },   FPL("/bar/baz/foo (1).exe") },
    { { FPL("/bar/baz/..////"), FPL(" (1)") },    FPL("") },
  };
  for (unsigned int i = 0; i < arraysize(cases); ++i) {
    FilePath path(cases[i].inputs[0]);
    FilePath result = path.InsertBeforeExtension(cases[i].inputs[1]);
    EXPECT_EQ(cases[i].expected, result.value()) << "i: " << i <<
        ", path: " << path.value() << ", insert: " << cases[i].inputs[1];
  }
}

TEST_F(FilePathTest, ReplaceExtension) {
  const struct BinaryTestData cases[] = {
    { { FPL(""),              FPL("") },      FPL("") },
    { { FPL(""),              FPL("txt") },   FPL("") },
    { { FPL("."),             FPL("txt") },   FPL("") },
    { { FPL(".."),            FPL("txt") },   FPL("") },
    { { FPL("."),             FPL("") },      FPL("") },
    { { FPL("foo.dll"),       FPL("txt") },   FPL("foo.txt") },
    { { FPL("foo..dll"),      FPL("txt") },   FPL("foo..txt") },
    { { FPL("foo.dll"),       FPL(".txt") },  FPL("foo.txt") },
    { { FPL("foo"),           FPL("txt") },   FPL("foo.txt") },
    { { FPL("foo."),          FPL("txt") },   FPL("foo.txt") },
    { { FPL("foo.."),         FPL("txt") },   FPL("foo..txt") },
    { { FPL("foo"),           FPL(".txt") },  FPL("foo.txt") },
    { { FPL("foo.baz.dll"),   FPL("txt") },   FPL("foo.baz.txt") },
    { { FPL("foo.baz.dll"),   FPL(".txt") },  FPL("foo.baz.txt") },
    { { FPL("foo.dll"),       FPL("") },      FPL("foo") },
    { { FPL("foo.dll"),       FPL(".") },     FPL("foo") },
    { { FPL("foo"),           FPL("") },      FPL("foo") },
    { { FPL("foo"),           FPL(".") },     FPL("foo") },
    { { FPL("foo.baz.dll"),   FPL("") },      FPL("foo.baz") },
    { { FPL("foo.baz.dll"),   FPL(".") },     FPL("foo.baz") },
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { { FPL("C:\\foo.bar\\foo"),    FPL("baz") }, FPL("C:\\foo.bar\\foo.baz") },
    { { FPL("C:\\foo.bar\\..\\\\"), FPL("baz") }, FPL("") },
#endif
    { { FPL("/foo.bar/foo"),        FPL("baz") }, FPL("/foo.bar/foo.baz") },
    { { FPL("/foo.bar/..////"),     FPL("baz") }, FPL("") },
  };
  for (unsigned int i = 0; i < arraysize(cases); ++i) {
    FilePath path(cases[i].inputs[0]);
    FilePath replaced = path.ReplaceExtension(cases[i].inputs[1]);
    EXPECT_EQ(cases[i].expected, replaced.value()) << "i: " << i <<
        ", path: " << path.value() << ", replace: " << cases[i].inputs[1];
  }
}
