// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for dealing with the local
// filesystem.

#ifndef BASE_FILE_UTIL_H_
#define BASE_FILE_UTIL_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(ANDROID)
#include <sys/stat.h>
#elif defined(OS_POSIX) 
#include <sys/types.h>
#include <fts.h>
#include <sys/stat.h>
#endif

#include <stdio.h>

#include <stack>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/file_path.h"

namespace base {
class Time;
}

namespace file_util {

//-----------------------------------------------------------------------------
// Functions that operate purely on a path string w/o touching the filesystem:

// Returns a vector of all of the components of the provided path.
void PathComponents(const FilePath& path,
                    std::vector<FilePath::StringType>* components);
#if defined(OS_WIN)
// Deprecated temporary compatibility function.
void PathComponents(const std::wstring& path,
                    std::vector<std::wstring>* components);
#endif

// Returns true if the given path ends with a path separator character.
bool EndsWithSeparator(const FilePath& path);
// These two versions are both deprecated. TODO(estade): remove them.
bool EndsWithSeparator(std::wstring* path);
bool EndsWithSeparator(const std::wstring& path);

// Makes sure that |path| ends with a separator IFF path is a directory that
// exists. Returns true if |path| is an existing directory, false otherwise.
bool EnsureEndsWithSeparator(FilePath* path);

// Modifies a string by trimming all trailing separators from the end.
// Deprecated. FilePath does this automatically, and if it's constructed from a
// path with a trailing separator, StripTrailingSeparators() may be used.
void TrimTrailingSeparator(std::wstring* dir);

// Strips the topmost directory from the end of 'dir'.  Assumes 'dir' does not
// refer to a file.
// If 'dir' is a root directory, return without change.
// Deprecated. Use FilePath::DirName instead.
void UpOneDirectory(std::wstring* dir);
// Strips the topmost directory from the end of 'dir'.  Assumes 'dir' does not
// refer to a file.
// If 'dir' is a root directory, the result becomes empty string.
// Deprecated. Use FilePath::DirName instead.
void UpOneDirectoryOrEmpty(std::wstring* dir);

// Returns the filename portion of 'path', without any leading \'s or /'s.
// Deprecated. Use FilePath::BaseName instead.
std::wstring GetFilenameFromPath(const std::wstring& path);

// Deprecated compatibility function.  Use FilePath::Extension.
FilePath::StringType GetFileExtensionFromPath(const FilePath& path);
// Deprecated temporary compatibility function.
std::wstring GetFileExtensionFromPath(const std::wstring& path);

// Returns the directory component of a path, without the trailing
// path separator, or an empty string on error. The function does not
// check for the existence of the path, so if it is passed a directory
// without the trailing \, it will interpret the last component of the
// path as a file and chomp it. This does not support relative paths.
// Examples:
// path == "C:\pics\jojo.jpg",     returns "C:\pics"
// path == "C:\Windows\system32\", returns "C:\Windows\system32"
// path == "C:\Windows\system32",  returns "C:\Windows"
std::wstring GetDirectoryFromPath(const std::wstring& path);

// Appends new_ending to path, adding a separator between the two if necessary.
void AppendToPath(std::wstring* path, const std::wstring& new_ending);

// Convert provided relative path into an absolute path.  Returns false on
// error. On POSIX, this function fails if the path does not exist.
bool AbsolutePath(FilePath* path);
// Deprecated temporary compatibility function.
bool AbsolutePath(std::wstring* path);

// Returns true if |parent| contains |child|. Both paths are converted to
// absolute paths before doing the comparison.
bool ContainsPath(const FilePath& parent, const FilePath& child);

// Deprecated compatibility function.  Use FilePath::InsertBeforeExtension.
void InsertBeforeExtension(FilePath* path, const FilePath::StringType& suffix);

// Deprecated compatibility function.  Use FilePath::ReplaceExtension.
void ReplaceExtension(FilePath* file_name,
                      const FilePath::StringType& extension);

#if defined(OS_WIN)
// Deprecated temporary compatibility functions.
void InsertBeforeExtension(std::wstring* path, const std::wstring& suffix);
void ReplaceExtension(std::wstring* file_name, const std::wstring& extension);
#endif

// Replaces characters in 'file_name' that are illegal for file names with
// 'replace_char'. 'file_name' must not be a full or relative path, but just the
// file name component. Any leading or trailing whitespace in 'file_name' is
// removed.
// Example:
//   file_name == "bad:file*name?.txt", changed to: "bad-file-name-.txt" when
//   'replace_char' is '-'.
void ReplaceIllegalCharacters(std::wstring* file_name, int replace_char);

//-----------------------------------------------------------------------------
// Functions that involve filesystem access or modification:

// created on or after the given |file_time|.  Doesn't count ".." or ".".
//
// Note for POSIX environments: a file created before |file_time|
// can be mis-detected as a newer file due to low precision of
// timestmap of file creation time. If you need to avoid such
// mis-detection perfectly, you should wait one second before
// obtaining |file_time|.
int CountFilesCreatedAfter(const FilePath& path,
                           const base::Time& file_time);

// Deletes the given path, whether it's a file or a directory.
// If it's a directory, it's perfectly happy to delete all of the
// directory's contents.  Passing true to recursive deletes
// subdirectories and their contents as well.
// Returns true if successful, false otherwise.
//
// WARNING: USING THIS WITH recursive==true IS EQUIVALENT
//          TO "rm -rf", SO USE WITH CAUTION.
bool Delete(const FilePath& path, bool recursive);
// Deprecated temporary compatibility function.
bool Delete(const std::wstring& path, bool recursive);

// Moves the given path, whether it's a file or a directory.
// If a simple rename is not possible, such as in the case where the paths are
// on different volumes, this will attempt to copy and delete. Returns
// true for success.
bool Move(const FilePath& from_path, const FilePath& to_path);
// Deprecated temporary compatibility function.
bool Move(const std::wstring& from_path, const std::wstring& to_path);

// Copies a single file. Use CopyDirectory to copy directories.
bool CopyFile(const FilePath& from_path, const FilePath& to_path);
// Deprecated temporary compatibility function.
bool CopyFile(const std::wstring& from_path, const std::wstring& to_path);

// Copies the given path, and optionally all subdirectories and their contents
// as well.
// If there are files existing under to_path, always overwrite.
// Returns true if successful, false otherwise.
// Dont't use wildcards on the names, it may stop working without notice.
//
// If you only need to copy a file use CopyFile, it's faster.
bool CopyDirectory(const FilePath& from_path, const FilePath& to_path,
                   bool recursive);
// Deprecated temporary compatibility function.
bool CopyDirectory(const std::wstring& from_path, const std::wstring& to_path,
                   bool recursive);

// Returns true if the given path exists on the local filesystem,
// false otherwise.
bool PathExists(const FilePath& path);
// Deprecated temporary compatibility function.
bool PathExists(const std::wstring& path);

// Returns true if the given path is writable by the user, false otherwise.
bool PathIsWritable(const FilePath& path);
// Deprecated temporary compatibility function.
bool PathIsWritable(const std::wstring& path);

// Returns true if the given path exists and is a directory, false otherwise.
bool DirectoryExists(const FilePath& path);
// Deprecated temporary compatibility function.
bool DirectoryExists(const std::wstring& path);

#if defined(OS_WIN)
// Gets the creation time of the given file (expressed in the local timezone),
// and returns it via the creation_time parameter.  Returns true if successful,
// false otherwise.
bool GetFileCreationLocalTime(const std::wstring& filename,
                              LPSYSTEMTIME creation_time);

// Same as above, but takes a previously-opened file handle instead of a name.
bool GetFileCreationLocalTimeFromHandle(HANDLE file_handle,
                                        LPSYSTEMTIME creation_time);
#endif  // defined(OS_WIN)

// Returns true if the contents of the two files given are equal, false
// otherwise.  If either file can't be read, returns false.
bool ContentsEqual(const FilePath& filename1,
                   const FilePath& filename2);
// Deprecated temporary compatibility function.
bool ContentsEqual(const std::wstring& filename1,
                   const std::wstring& filename2);

// Read the file at |path| into |contents|, returning true on success.
// Useful for unit tests.
bool ReadFileToString(const FilePath& path, std::string* contents);
// Deprecated version.
bool ReadFileToString(const std::wstring& path, std::string* contents);

#if defined(OS_POSIX)
// Read exactly |bytes| bytes from file descriptor |fd|, storing the result
// in |buffer|. This function is protected against EINTR and partial reads.
// Returns true iff |bytes| bytes have been successfuly read from |fd|.
bool ReadFromFD(int fd, char* buffer, size_t bytes);
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
// Resolve Windows shortcut (.LNK file)
// Argument path specifies a valid LNK file. On success, return true and put
// the URL into path. If path is a invalid .LNK file, return false.
bool ResolveShortcut(FilePath* path);
// Deprecated temporary compatibility function.
bool ResolveShortcut(std::wstring* path);

// Create a Windows shortcut (.LNK file)
// This method creates a shortcut link using the information given. Ensure
// you have initialized COM before calling into this function. 'source'
// and 'destination' parameters are required, everything else can be NULL.
// 'source' is the existing file, 'destination' is the new link file to be
// created; for best results pass the filename with the .lnk extension.
// The 'icon' can specify a dll or exe in which case the icon index is the
// resource id.
// Note that if the shortcut exists it will overwrite it.
bool CreateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index);

// Update a Windows shortcut (.LNK file). This method assumes the shortcut
// link already exists (otherwise false is returned). Ensure you have
// initialized COM before calling into this function. Only 'destination'
// parameter is required, everything else can be NULL (but if everything else
// is NULL no changes are made to the shortcut). 'destination' is the link
// file to be updated. For best results pass the filename with the .lnk
// extension.
bool UpdateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index);

// Return true if the given directory is empty
bool IsDirectoryEmpty(const std::wstring& dir_path);

// Copy from_path to to_path recursively and then delete from_path recursively.
// Returns true if all operations succeed.
// This function simulates Move(), but unlike Move() it works across volumes.
// This fuction is not transactional.
bool CopyAndDeleteDirectory(const FilePath& from_path,
                            const FilePath& to_path);
#endif

// Get the temporary directory provided by the system.
bool GetTempDir(FilePath* path);
// Deprecated temporary compatibility function.
bool GetTempDir(std::wstring* path);
// Get a temporary directory for shared memory files.
// Only useful on POSIX; redirects to GetTempDir() on Windows.
bool GetShmemTempDir(FilePath* path);

// Creates a temporary file. The full path is placed in |path|, and the
// function returns true if was successful in creating the file. The file will
// be empty and all handles closed after this function returns.
// TODO(erikkay): rename this function and track down all of the callers.
// (Clarification of erik's comment: the intent is to rename the BlahFileName()
//  calls into BlahFile(), since they create temp files (not temp filenames).)
bool CreateTemporaryFileName(FilePath* path);
// Deprecated temporary compatibility function.
bool CreateTemporaryFileName(std::wstring* temp_file);

// Create and open a temporary file.  File is opened for read/write.
// The full path is placed in |path|, and the function returns true if
// was successful in creating and opening the file.
FILE* CreateAndOpenTemporaryFile(FilePath* path);
// Like above but for shmem files.  Only useful for POSIX.
FILE* CreateAndOpenTemporaryShmemFile(FilePath* path);

// Similar to CreateAndOpenTemporaryFile, but the file is created in |dir|.
FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path);

// Same as CreateTemporaryFileName but the file is created in |dir|.
bool CreateTemporaryFileNameInDir(const std::wstring& dir,
                                  std::wstring* temp_file);

// Create a new directory under TempPath. If prefix is provided, the new
// directory name is in the format of prefixyyyy.
// NOTE: prefix is ignored in the POSIX implementation.
// TODO(erikkay): is this OK?
// If success, return true and output the full path of the directory created.
bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path);
// Deprecated temporary compatibility function.
bool CreateNewTempDirectory(const std::wstring& prefix,
                            std::wstring* new_temp_path);

// Creates a directory, as well as creating any parent directories, if they
// don't exist. Returns 'true' on successful creation, or if the directory
// already exists.
bool CreateDirectory(const FilePath& full_path);
// Deprecated temporary compatibility function.
bool CreateDirectory(const std::wstring& full_path);

// Returns the file size. Returns true on success.
bool GetFileSize(const FilePath& file_path, int64_t* file_size);
// Deprecated temporary compatibility function.
bool GetFileSize(const std::wstring& file_path, int64_t* file_size);

// Used to hold information about a given file path.  See GetFileInfo below.
struct FileInfo {
  // The size of the file in bytes.  Undefined when is_directory is true.
  int64_t size;

  // True if the file corresponds to a directory.
  bool is_directory;

  // Add additional fields here as needed.
};

// Returns information about the given file path.
bool GetFileInfo(const FilePath& file_path, FileInfo* info);
// Deprecated temporary compatibility function.
bool GetFileInfo(const std::wstring& file_path, FileInfo* info);

// Wrapper for fopen-like calls. Returns non-NULL FILE* on success.
FILE* OpenFile(const FilePath& filename, const char* mode);
// Deprecated temporary compatibility functions.
FILE* OpenFile(const std::string& filename, const char* mode);
FILE* OpenFile(const std::wstring& filename, const char* mode);

// Closes file opened by OpenFile. Returns true on success.
bool CloseFile(FILE* file);

// Truncates an open file to end at the location of the current file pointer.
// This is a cross-platform analog to Windows' SetEndOfFile() function.
bool TruncateFile(FILE* file);

// Reads the given number of bytes from the file into the buffer.  Returns
// the number of read bytes, or -1 on error.
int ReadFile(const FilePath& filename, char* data, int size);
// Deprecated temporary compatibility function.
int ReadFile(const std::wstring& filename, char* data, int size);

// Writes the given buffer into the file, overwriting any data that was
// previously there.  Returns the number of bytes written, or -1 on error.
int WriteFile(const FilePath& filename, const char* data, int size);
// Deprecated temporary compatibility function.
int WriteFile(const std::wstring& filename, const char* data, int size);

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* path);
// Deprecated temporary compatibility function.
bool GetCurrentDirectory(std::wstring* path);

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& path);
// Deprecated temporary compatibility function.
bool SetCurrentDirectory(const std::wstring& current_directory);

// A class to handle auto-closing of FILE*'s.
class ScopedFILEClose {
 public:
  inline void operator()(FILE* x) const {
    if (x) {
      fclose(x);
    }
  }
};

typedef scoped_ptr_malloc<FILE, ScopedFILEClose> ScopedFILE;

// A class for enumerating the files in a provided path. The order of the
// results is not guaranteed.
//
// DO NOT USE FROM THE MAIN THREAD of your application unless it is a test
// program where latency does not matter. This class is blocking.
class FileEnumerator {
 public:
#if defined(OS_WIN)
  typedef WIN32_FIND_DATA FindInfo;
#elif defined(OS_POSIX)
  typedef struct {
    struct stat stat;
    std::string filename;
  } FindInfo;
#endif

  enum FILE_TYPE {
    FILES                 = 0x1,
    DIRECTORIES           = 0x2,
    FILES_AND_DIRECTORIES = 0x3
  };

  // |root_path| is the starting directory to search for. It may or may not end
  // in a slash.
  //
  // If |recursive| is true, this will enumerate all matches in any
  // subdirectories matched as well. It does a breadth-first search, so all
  // files in one directory will be returned before any files in a
  // subdirectory.
  //
  // |file_type| specifies whether the enumerator should match files,
  // directories, or both.
  //
  // |pattern| is an optional pattern for which files to match. This
  // works like shell globbing. For example, "*.txt" or "Foo???.doc".
  // However, be careful in specifying patterns that aren't cross platform
  // since the underlying code uses OS-specific matching routines.  In general,
  // Windows matching is less featureful than others, so test there first.
  // If unspecified, this will match all files.
  // NOTE: the pattern only matches the contents of root_path, not files in
  // recursive subdirectories.
  // TODO(erikkay): Fix the pattern matching to work at all levels.
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 FileEnumerator::FILE_TYPE file_type);
  FileEnumerator(const FilePath& root_path,
                 bool recursive,
                 FileEnumerator::FILE_TYPE file_type,
                 const FilePath::StringType& pattern);
  ~FileEnumerator();

  // Returns an empty string if there are no more results.
  FilePath Next();

  // Write the file info into |info|.
  void GetFindInfo(FindInfo* info);

 private:
  FilePath root_path_;
  bool recursive_;
  FILE_TYPE file_type_;
  FilePath pattern_;  // Empty when we want to find everything.

  // Set to true when there is a find operation open. This way, we can lazily
  // start the operations when the caller calls Next().
  bool is_in_find_op_;

  // A stack that keeps track of which subdirectories we still need to
  // enumerate in the breadth-first search.
  std::stack<FilePath> pending_paths_;

#if defined(OS_WIN)
  WIN32_FIND_DATA find_data_;
  HANDLE find_handle_;
#elif defined(ANDROID)
  void *fts_;
#elif defined(OS_POSIX)
  FTS* fts_;
  FTSENT* fts_ent_;
#endif

  DISALLOW_EVIL_CONSTRUCTORS(FileEnumerator);
};

class MemoryMappedFile {
 public:
  // The default constructor sets all members to invalid/null values.
  MemoryMappedFile();
  ~MemoryMappedFile();

  // Opens an existing file and maps it into memory. Access is restricted to
  // read only. If this object already points to a valid memory mapped file
  // then this method will fail and return false. If it cannot open the file,
  // the file does not exist, or the memory mapping fails, it will return false.
  // Later we may want to allow the user to specify access.
  bool Initialize(const FilePath& file_name);

  const uint8_t* data() const { return data_; }
  size_t length() const { return length_; }

  // Is file_ a valid file handle that points to an open, memory mapped file?
  bool IsValid();

 private:
  // Map the file to memory, set data_ to that memory address. Return true on
  // success, false on any kind of failure. This is a helper for Initialize().
  bool MapFileToMemory(const FilePath& file_name);

  // Closes all open handles. Later we may want to make this public.
  void CloseHandles();

#if defined(OS_WIN)
  HANDLE file_;
  HANDLE file_mapping_;
#elif defined(OS_POSIX)
  // The file descriptor.
  int file_;
#endif
  uint8_t* data_;
  size_t length_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMappedFile);
};

// Renames a file using the SHFileOperation API to ensure that the target file
// gets the correct default security descriptor in the new path.
bool RenameFileAndResetSecurityDescriptor(
    const FilePath& source_file_path,
    const FilePath& target_file_path);

}  // namespace file_util

#endif  // BASE_FILE_UTIL_H_
