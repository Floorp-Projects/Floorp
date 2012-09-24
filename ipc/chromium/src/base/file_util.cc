// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#if defined(OS_WIN)
#include <io.h>
#endif
#include <stdio.h>
#if defined(ANDROID) || defined(OS_POSIX)
#include <unistd.h>
#endif

#include <fstream>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/string_util.h"

#include "base/string_piece.h"
#include "base/sys_string_conversions.h"

namespace {

const FilePath::CharType kExtensionSeparator = FILE_PATH_LITERAL('.');

}  // namespace

namespace file_util {

void PathComponents(const FilePath& path,
                    std::vector<FilePath::StringType>* components) {
  DCHECK(components);
  if (!components)
    return;

  FilePath::StringType path_str = path.value();
  FilePath::StringType::size_type start = 0;
  FilePath::StringType::size_type end =
      path_str.find_first_of(FilePath::kSeparators);

  // If the path starts with a separator, add it to components.
  if (end == start) {
    components->push_back(FilePath::StringType(path_str, 0, 1));
    start = end + 1;
    end = path_str.find_first_of(FilePath::kSeparators, start);
  }
  while (end != FilePath::StringType::npos) {
    FilePath::StringType component =
        FilePath::StringType(path_str, start, end - start);
    components->push_back(component);
    start = end + 1;
    end = path_str.find_first_of(FilePath::kSeparators, start);
  }

  components->push_back(FilePath::StringType(path_str, start));
}

bool EndsWithSeparator(const FilePath& path) {
  FilePath::StringType value = path.value();
  if (value.empty())
    return false;

  return FilePath::IsSeparator(value[value.size() - 1]);
}

bool EnsureEndsWithSeparator(FilePath* path) {
  if (!DirectoryExists(*path))
    return false;

  if (EndsWithSeparator(*path))
    return true;

  FilePath::StringType& path_str =
      const_cast<FilePath::StringType&>(path->value());
  path_str.append(&FilePath::kSeparators[0], 1);

  return true;
}

void TrimTrailingSeparator(std::wstring* dir) {
  while (dir->length() > 1 && EndsWithSeparator(dir))
    dir->resize(dir->length() - 1);
}

FilePath::StringType GetFileExtensionFromPath(const FilePath& path) {
  FilePath::StringType file_name = path.BaseName().value();
  const FilePath::StringType::size_type last_dot =
      file_name.rfind(kExtensionSeparator);
  return FilePath::StringType(last_dot == FilePath::StringType::npos ?
                              FILE_PATH_LITERAL("") :
                              file_name, last_dot+1);
}

void InsertBeforeExtension(FilePath* path, const FilePath::StringType& suffix) {
  FilePath::StringType& value =
      const_cast<FilePath::StringType&>(path->value());

  const FilePath::StringType::size_type last_dot =
      value.rfind(kExtensionSeparator);
  const FilePath::StringType::size_type last_separator =
      value.find_last_of(FilePath::StringType(FilePath::kSeparators));

  if (last_dot == FilePath::StringType::npos ||
      (last_separator != std::wstring::npos && last_dot < last_separator)) {
    // The path looks something like "C:\pics.old\jojo" or "C:\pics\jojo".
    // We should just append the suffix to the entire path.
    value.append(suffix);
    return;
  }

  value.insert(last_dot, suffix);
}

void ReplaceExtension(FilePath* path, const FilePath::StringType& extension) {
  FilePath::StringType clean_extension;
  // If the new extension is "" or ".", then we will just remove the current
  // extension.
  if (!extension.empty() &&
      extension != FilePath::StringType(&kExtensionSeparator, 1)) {
    if (extension[0] != kExtensionSeparator)
      clean_extension.append(&kExtensionSeparator, 1);
    clean_extension.append(extension);
  }

  FilePath::StringType& value =
      const_cast<FilePath::StringType&>(path->value());
  const FilePath::StringType::size_type last_dot =
      value.rfind(kExtensionSeparator);
  const FilePath::StringType::size_type last_separator =
      value.find_last_of(FilePath::StringType(FilePath::kSeparators));

  // Erase the current extension, if any.
  if ((last_dot > last_separator ||
      last_separator == FilePath::StringType::npos) &&
      last_dot != FilePath::StringType::npos)
    value.erase(last_dot);

  value.append(clean_extension);
}

bool ContentsEqual(const FilePath& filename1, const FilePath& filename2) {
  // We open the file in binary format even if they are text files because
  // we are just comparing that bytes are exactly same in both files and not
  // doing anything smart with text formatting.
  std::ifstream file1, file2;

  filename1.OpenInputStream(file1);
  filename2.OpenInputStream(file2);

  // Even if both files aren't openable (and thus, in some sense, "equal"),
  // any unusable file yields a result of "false".
  if (!file1.is_open() || !file2.is_open())
    return false;

  const int BUFFER_SIZE = 2056;
  char buffer1[BUFFER_SIZE], buffer2[BUFFER_SIZE];
  do {
    file1.read(buffer1, BUFFER_SIZE);
    file2.read(buffer2, BUFFER_SIZE);

    if ((file1.eof() && !file2.eof()) ||
        (!file1.eof() && file2.eof()) ||
        (file1.gcount() != file2.gcount()) ||
        (memcmp(buffer1, buffer2, file1.gcount()))) {
      file1.close();
      file2.close();
      return false;
    }
  } while (!file1.eof() && !file2.eof());

  file1.close();
  file2.close();
  return true;
}

bool ReadFileToString(const FilePath& path, std::string* contents) {
  FILE* file = OpenFile(path, "rb");
  if (!file) {
    return false;
  }

  char buf[1 << 16];
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), file)) > 0) {
    contents->append(buf, len);
  }
  CloseFile(file);

  return true;
}

FILE* CreateAndOpenTemporaryFile(FilePath* path) {
  FilePath directory;
  if (!GetTempDir(&directory))
    return NULL;

  return CreateAndOpenTemporaryFileInDir(directory, path);
}

bool GetFileSize(const FilePath& file_path, int64_t* file_size) {
  FileInfo info;
  if (!GetFileInfo(file_path, &info))
    return false;
  *file_size = info.size;
  return true;
}

bool CloseFile(FILE* file) {
  if (file == NULL)
    return true;
  return fclose(file) == 0;
}

bool TruncateFile(FILE* file) {
  if (file == NULL)
    return false;
  long current_offset = ftell(file);
  if (current_offset == -1)
    return false;
#if defined(OS_WIN)
  int fd = _fileno(file);
  if (_chsize(fd, current_offset) != 0)
    return false;
#else
  int fd = fileno(file);
  if (ftruncate(fd, current_offset) != 0)
    return false;
#endif
  return true;
}

bool ContainsPath(const FilePath &parent, const FilePath& child) {
  FilePath abs_parent = FilePath(parent);
  FilePath abs_child = FilePath(child);

  if (!file_util::AbsolutePath(&abs_parent) ||
      !file_util::AbsolutePath(&abs_child))
    return false;

#if defined(OS_WIN)
  // file_util::AbsolutePath() does not flatten case on Windows, so we must do
  // a case-insensitive compare.
  if (!StartsWith(abs_child.value(), abs_parent.value(), false))
#else
  if (!StartsWithASCII(abs_child.value(), abs_parent.value(), true))
#endif
    return false;

  // file_util::AbsolutePath() normalizes '/' to '\' on Windows, so we only need
  // to check kSeparators[0].
  if (abs_child.value().length() <= abs_parent.value().length() ||
      abs_child.value()[abs_parent.value().length()] !=
          FilePath::kSeparators[0])
    return false;

  return true;
}

///////////////////////////////////////////////
// MemoryMappedFile

MemoryMappedFile::~MemoryMappedFile() {
  CloseHandles();
}

bool MemoryMappedFile::Initialize(const FilePath& file_name) {
  if (IsValid())
    return false;

  if (!MapFileToMemory(file_name)) {
    CloseHandles();
    return false;
  }

  return true;
}

bool MemoryMappedFile::IsValid() {
  return data_ != NULL;
}

// Deprecated functions ----------------------------------------------------

bool ReadFileToString(const std::wstring& path, std::string* contents) {
  return ReadFileToString(FilePath::FromWStringHack(path), contents);
}

bool AbsolutePath(std::wstring* path_str) {
  FilePath path(FilePath::FromWStringHack(*path_str));
  if (!AbsolutePath(&path))
    return false;
  *path_str = path.ToWStringHack();
  return true;
}
void AppendToPath(std::wstring* path, const std::wstring& new_ending) {
  if (!path) {
    NOTREACHED();
    return;  // Don't crash in this function in release builds.
  }

  if (!EndsWithSeparator(path))
    path->push_back(FilePath::kSeparators[0]);
  path->append(new_ending);
}
bool CopyDirectory(const std::wstring& from_path, const std::wstring& to_path,
                   bool recursive) {
  return CopyDirectory(FilePath::FromWStringHack(from_path),
                       FilePath::FromWStringHack(to_path),
                       recursive);
}
bool ContentsEqual(const std::wstring& filename1,
                   const std::wstring& filename2) {
  return ContentsEqual(FilePath::FromWStringHack(filename1),
                       FilePath::FromWStringHack(filename2));
}
bool CopyFile(const std::wstring& from_path, const std::wstring& to_path) {
  return CopyFile(FilePath::FromWStringHack(from_path),
                  FilePath::FromWStringHack(to_path));
}
bool CreateDirectory(const std::wstring& full_path) {
  return CreateDirectory(FilePath::FromWStringHack(full_path));
}
bool CreateNewTempDirectory(const std::wstring& prefix,
                            std::wstring* new_temp_path) {
#if defined(OS_WIN)
  FilePath::StringType dir_prefix(prefix);
#elif defined(OS_POSIX)
  FilePath::StringType dir_prefix = WideToUTF8(prefix);
#endif
  FilePath temp_path;
  if (!CreateNewTempDirectory(dir_prefix, &temp_path))
    return false;
  *new_temp_path = temp_path.ToWStringHack();
  return true;
}
bool CreateTemporaryFileName(std::wstring* temp_file) {
  FilePath temp_file_path;
  if (!CreateTemporaryFileName(&temp_file_path))
    return false;
  *temp_file = temp_file_path.ToWStringHack();
  return true;
}
bool Delete(const std::wstring& path, bool recursive) {
  return Delete(FilePath::FromWStringHack(path), recursive);
}
bool DirectoryExists(const std::wstring& path) {
  return DirectoryExists(FilePath::FromWStringHack(path));
}
bool EndsWithSeparator(std::wstring* path) {
  return EndsWithSeparator(FilePath::FromWStringHack(*path));
}
bool EndsWithSeparator(const std::wstring& path) {
  return EndsWithSeparator(FilePath::FromWStringHack(path));
}
bool GetCurrentDirectory(std::wstring* path_str) {
  FilePath path;
  if (!GetCurrentDirectory(&path))
    return false;
  *path_str = path.ToWStringHack();
  return true;
}
std::wstring GetFileExtensionFromPath(const std::wstring& path) {
  FilePath::StringType extension =
      GetFileExtensionFromPath(FilePath::FromWStringHack(path));
#if defined(OS_WIN)
  return extension;
#elif defined(OS_POSIX)
  return UTF8ToWide(extension);
#endif
}
bool GetFileInfo(const std::wstring& file_path, FileInfo* results) {
  return GetFileInfo(FilePath::FromWStringHack(file_path), results);
}
std::wstring GetFilenameFromPath(const std::wstring& path) {
  if (path.empty() || EndsWithSeparator(path))
    return std::wstring();

  return FilePath::FromWStringHack(path).BaseName().ToWStringHack();
}
bool GetFileSize(const std::wstring& file_path, int64_t* file_size) {
  return GetFileSize(FilePath::FromWStringHack(file_path), file_size);
}
bool GetTempDir(std::wstring* path_str) {
  FilePath path;
  if (!GetTempDir(&path))
    return false;
  *path_str = path.ToWStringHack();
  return true;
}
bool Move(const std::wstring& from_path, const std::wstring& to_path) {
  return Move(FilePath::FromWStringHack(from_path),
              FilePath::FromWStringHack(to_path));
}
FILE* OpenFile(const std::wstring& filename, const char* mode) {
  return OpenFile(FilePath::FromWStringHack(filename), mode);
}
bool PathExists(const std::wstring& path) {
  return PathExists(FilePath::FromWStringHack(path));
}
bool PathIsWritable(const std::wstring& path) {
  return PathIsWritable(FilePath::FromWStringHack(path));
}
int ReadFile(const std::wstring& filename, char* data, int size) {
  return ReadFile(FilePath::FromWStringHack(filename), data, size);
}
bool SetCurrentDirectory(const std::wstring& directory) {
  return SetCurrentDirectory(FilePath::FromWStringHack(directory));
}
void UpOneDirectory(std::wstring* dir) {
  FilePath path = FilePath::FromWStringHack(*dir);
  FilePath directory = path.DirName();
  // If there is no separator, we will get back kCurrentDirectory.
  // In this case don't change |dir|.
  if (directory.value() != FilePath::kCurrentDirectory)
    *dir = directory.ToWStringHack();
}
void UpOneDirectoryOrEmpty(std::wstring* dir) {
  FilePath path = FilePath::FromWStringHack(*dir);
  FilePath directory = path.DirName();
  // If there is no separator, we will get back kCurrentDirectory.
  // In this case, clear dir.
  if (directory == path || directory.value() == FilePath::kCurrentDirectory)
    dir->clear();
  else
    *dir = directory.ToWStringHack();
}
int WriteFile(const std::wstring& filename, const char* data, int size) {
  return WriteFile(FilePath::FromWStringHack(filename), data, size);
}
}  // namespace
