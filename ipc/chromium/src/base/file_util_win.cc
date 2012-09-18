// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <time.h>
#include <string>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/win_util.h"

namespace file_util {

std::wstring GetDirectoryFromPath(const std::wstring& path) {
  wchar_t path_buffer[MAX_PATH];
  wchar_t* file_ptr = NULL;
  if (GetFullPathName(path.c_str(), MAX_PATH, path_buffer, &file_ptr) == 0)
    return L"";

  std::wstring::size_type length =
      file_ptr ? file_ptr - path_buffer : path.length();
  std::wstring directory(path, 0, length);
  TrimTrailingSeparator(&directory);
  return directory;
}

bool AbsolutePath(FilePath* path) {
  wchar_t file_path_buf[MAX_PATH];
  if (!_wfullpath(file_path_buf, path->value().c_str(), MAX_PATH))
    return false;
  *path = FilePath(file_path_buf);
  return true;
}

int CountFilesCreatedAfter(const FilePath& path,
                           const base::Time& comparison_time) {
  int file_count = 0;
  FILETIME comparison_filetime(comparison_time.ToFileTime());

  WIN32_FIND_DATA find_file_data;
  // All files in given dir
  std::wstring filename_spec = path.Append(L"*").value();
  HANDLE find_handle = FindFirstFile(filename_spec.c_str(), &find_file_data);
  if (find_handle != INVALID_HANDLE_VALUE) {
    do {
      // Don't count current or parent directories.
      if ((wcscmp(find_file_data.cFileName, L"..") == 0) ||
          (wcscmp(find_file_data.cFileName, L".") == 0))
        continue;

      long result = CompareFileTime(&find_file_data.ftCreationTime,
                                    &comparison_filetime);
      // File was created after or on comparison time
      if ((result == 1) || (result == 0))
        ++file_count;
    } while (FindNextFile(find_handle,  &find_file_data));
    FindClose(find_handle);
  }

  return file_count;
}

bool Delete(const FilePath& path, bool recursive) {
  if (path.value().length() >= MAX_PATH)
    return false;

  // If we're not recursing use DeleteFile; it should be faster. DeleteFile
  // fails if passed a directory though, which is why we fall through on
  // failure to the SHFileOperation.
  if (!recursive && DeleteFile(path.value().c_str()) != 0)
    return true;

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path, path.value().c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_DELETE;
  file_operation.pFrom = double_terminated_path;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION;
  if (!recursive)
    file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;
  int err = SHFileOperation(&file_operation);
  // Some versions of Windows return ERROR_FILE_NOT_FOUND when
  // deleting an empty directory.
  return (err == 0 || err == ERROR_FILE_NOT_FOUND);
}

bool Move(const FilePath& from_path, const FilePath& to_path) {
  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }
  if (MoveFileEx(from_path.value().c_str(), to_path.value().c_str(),
                 MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING) != 0)
    return true;
  if (DirectoryExists(from_path)) {
    // MoveFileEx fails if moving directory across volumes. We will simulate
    // the move by using Copy and Delete. Ideally we could check whether
    // from_path and to_path are indeed in different volumes.
    return CopyAndDeleteDirectory(from_path, to_path);
  }
  return false;
}

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }
  return (::CopyFile(from_path.value().c_str(), to_path.value().c_str(),
                     false) != 0);
}

bool ShellCopy(const FilePath& from_path, const FilePath& to_path,
               bool recursive) {
  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }

  // SHFILEOPSTRUCT wants the path to be terminated with two NULLs,
  // so we have to use wcscpy because wcscpy_s writes non-NULLs
  // into the rest of the buffer.
  wchar_t double_terminated_path_from[MAX_PATH + 1] = {0};
  wchar_t double_terminated_path_to[MAX_PATH + 1] = {0};
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_from, from_path.value().c_str());
#pragma warning(suppress:4996)  // don't complain about wcscpy deprecation
  wcscpy(double_terminated_path_to, to_path.value().c_str());

  SHFILEOPSTRUCT file_operation = {0};
  file_operation.wFunc = FO_COPY;
  file_operation.pFrom = double_terminated_path_from;
  file_operation.pTo = double_terminated_path_to;
  file_operation.fFlags = FOF_NOERRORUI | FOF_SILENT | FOF_NOCONFIRMATION |
                          FOF_NOCONFIRMMKDIR;
  if (!recursive)
    file_operation.fFlags |= FOF_NORECURSION | FOF_FILESONLY;

  return (SHFileOperation(&file_operation) == 0);
}

bool CopyDirectory(const FilePath& from_path, const FilePath& to_path,
                   bool recursive) {
  if (recursive)
    return ShellCopy(from_path, to_path, true);

  // Instead of creating a new directory, we copy the old one to include the
  // security information of the folder as part of the copy.
  if (!PathExists(to_path)) {
    // Except that Vista fails to do that, and instead do a recursive copy if
    // the target directory doesn't exist.
    if (win_util::GetWinVersion() >= win_util::WINVERSION_VISTA)
      CreateDirectory(to_path);
    else
      ShellCopy(from_path, to_path, false);
  }

  FilePath directory = from_path.Append(L"*.*");
  return ShellCopy(directory, to_path, false);
}

bool CopyAndDeleteDirectory(const FilePath& from_path,
                            const FilePath& to_path) {
  if (CopyDirectory(from_path, to_path, true)) {
    if (Delete(from_path, true)) {
      return true;
    }
    // Like Move, this function is not transactional, so we just
    // leave the copied bits behind if deleting from_path fails.
    // If to_path exists previously then we have already overwritten
    // it by now, we don't get better off by deleting the new bits.
  }
  return false;
}


bool PathExists(const FilePath& path) {
  return (GetFileAttributes(path.value().c_str()) != INVALID_FILE_ATTRIBUTES);
}

bool PathIsWritable(const FilePath& path) {
  HANDLE dir =
      CreateFile(path.value().c_str(), FILE_ADD_FILE,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                 NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

  if (dir == INVALID_HANDLE_VALUE)
    return false;

  CloseHandle(dir);
  return true;
}

bool DirectoryExists(const FilePath& path) {
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr != INVALID_FILE_ATTRIBUTES)
    return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return false;
}

bool GetFileCreationLocalTimeFromHandle(HANDLE file_handle,
                                        LPSYSTEMTIME creation_time) {
  if (!file_handle)
    return false;

  FILETIME utc_filetime;
  if (!GetFileTime(file_handle, &utc_filetime, NULL, NULL))
    return false;

  FILETIME local_filetime;
  if (!FileTimeToLocalFileTime(&utc_filetime, &local_filetime))
    return false;

  return !!FileTimeToSystemTime(&local_filetime, creation_time);
}

bool GetFileCreationLocalTime(const std::wstring& filename,
                              LPSYSTEMTIME creation_time) {
  ScopedHandle file_handle(
      CreateFile(filename.c_str(), GENERIC_READ,
                 FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                 OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
  return GetFileCreationLocalTimeFromHandle(file_handle.Get(), creation_time);
}

bool ResolveShortcut(std::wstring* path) {
  FilePath file_path(*path);
  bool result = ResolveShortcut(&file_path);
  *path = file_path.value();
  return result;
}

bool ResolveShortcut(FilePath* path) {
  HRESULT result;
  IShellLink *shell = NULL;
  bool is_resolved = false;

  // Get pointer to the IShellLink interface
  result = CoCreateInstance(CLSID_ShellLink, NULL,
                            CLSCTX_INPROC_SERVER, IID_IShellLink,
                            reinterpret_cast<LPVOID*>(&shell));
  if (SUCCEEDED(result)) {
    IPersistFile *persist = NULL;
    // Query IShellLink for the IPersistFile interface
    result = shell->QueryInterface(IID_IPersistFile,
                                   reinterpret_cast<LPVOID*>(&persist));
    if (SUCCEEDED(result)) {
      WCHAR temp_path[MAX_PATH];
      // Load the shell link
      result = persist->Load(path->value().c_str(), STGM_READ);
      if (SUCCEEDED(result)) {
        // Try to find the target of a shortcut
        result = shell->Resolve(0, SLR_NO_UI);
        if (SUCCEEDED(result)) {
          result = shell->GetPath(temp_path, MAX_PATH,
                                  NULL, SLGP_UNCPRIORITY);
          *path = FilePath(temp_path);
          is_resolved = true;
        }
      }
    }
    if (persist)
      persist->Release();
  }
  if (shell)
    shell->Release();

  return is_resolved;
}

bool CreateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index) {
  IShellLink *i_shell_link = NULL;
  IPersistFile *i_persist_file = NULL;

  // Get pointer to the IShellLink interface
  HRESULT result = CoCreateInstance(CLSID_ShellLink, NULL,
                                    CLSCTX_INPROC_SERVER, IID_IShellLink,
                                    reinterpret_cast<LPVOID*>(&i_shell_link));
  if (FAILED(result))
    return false;

  // Query IShellLink for the IPersistFile interface
  result = i_shell_link->QueryInterface(IID_IPersistFile,
      reinterpret_cast<LPVOID*>(&i_persist_file));
  if (FAILED(result)) {
    i_shell_link->Release();
    return false;
  }

  if (FAILED(i_shell_link->SetPath(source))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (working_dir && FAILED(i_shell_link->SetWorkingDirectory(working_dir))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (arguments && FAILED(i_shell_link->SetArguments(arguments))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (description && FAILED(i_shell_link->SetDescription(description))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (icon && FAILED(i_shell_link->SetIconLocation(icon, icon_index))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  result = i_persist_file->Save(destination, TRUE);
  i_persist_file->Release();
  i_shell_link->Release();
  return SUCCEEDED(result);
}


bool UpdateShortcutLink(const wchar_t *source, const wchar_t *destination,
                        const wchar_t *working_dir, const wchar_t *arguments,
                        const wchar_t *description, const wchar_t *icon,
                        int icon_index) {
  // Get pointer to the IPersistFile interface and load existing link
  IShellLink *i_shell_link = NULL;
  if (FAILED(CoCreateInstance(CLSID_ShellLink, NULL,
                              CLSCTX_INPROC_SERVER, IID_IShellLink,
                              reinterpret_cast<LPVOID*>(&i_shell_link))))
    return false;

  IPersistFile *i_persist_file = NULL;
  if (FAILED(i_shell_link->QueryInterface(
      IID_IPersistFile, reinterpret_cast<LPVOID*>(&i_persist_file)))) {
    i_shell_link->Release();
    return false;
  }

  if (FAILED(i_persist_file->Load(destination, 0))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (source && FAILED(i_shell_link->SetPath(source))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (working_dir && FAILED(i_shell_link->SetWorkingDirectory(working_dir))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (arguments && FAILED(i_shell_link->SetArguments(arguments))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (description && FAILED(i_shell_link->SetDescription(description))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  if (icon && FAILED(i_shell_link->SetIconLocation(icon, icon_index))) {
    i_persist_file->Release();
    i_shell_link->Release();
    return false;
  }

  HRESULT result = i_persist_file->Save(destination, TRUE);
  i_persist_file->Release();
  i_shell_link->Release();
  return SUCCEEDED(result);
}

bool IsDirectoryEmpty(const std::wstring& dir_path) {
  FileEnumerator files(FilePath(dir_path),
                       false, FileEnumerator::FILES_AND_DIRECTORIES);
  if (files.Next().value().empty())
    return true;
  return false;
}

bool GetTempDir(FilePath* path) {
  wchar_t temp_path[MAX_PATH + 1];
  DWORD path_len = ::GetTempPath(MAX_PATH, temp_path);
  if (path_len >= MAX_PATH || path_len <= 0)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  std::wstring path_str(temp_path);
  TrimTrailingSeparator(&path_str);
  *path = FilePath(path_str);
  return true;
}

bool GetShmemTempDir(FilePath* path) {
  return GetTempDir(path);
}

bool CreateTemporaryFileName(FilePath* path) {
  std::wstring temp_path, temp_file;

  if (!GetTempDir(&temp_path))
    return false;

  if (CreateTemporaryFileNameInDir(temp_path, &temp_file)) {
    *path = FilePath(temp_file);
    return true;
  }

  return false;
}

FILE* CreateAndOpenTemporaryShmemFile(FilePath* path) {
  return CreateAndOpenTemporaryFile(path);
}

// On POSIX we have semantics to create and open a temporary file
// atomically.
// TODO(jrg): is there equivalent call to use on Windows instead of
// going 2-step?
FILE* CreateAndOpenTemporaryFileInDir(const FilePath& dir, FilePath* path) {
  std::wstring wstring_path;
  if (!CreateTemporaryFileNameInDir(dir.value(), &wstring_path)) {
    return NULL;
  }
  *path = FilePath(wstring_path);
  // Open file in binary mode, to avoid problems with fwrite. On Windows
  // it replaces \n's with \r\n's, which may surprise you.
  // Reference: http://msdn.microsoft.com/en-us/library/h9t88zwz(VS.71).aspx
  return OpenFile(*path, "wb+");
}

bool CreateTemporaryFileNameInDir(const std::wstring& dir,
                                  std::wstring* temp_file) {
  wchar_t temp_name[MAX_PATH + 1];

  if (!GetTempFileName(dir.c_str(), L"", 0, temp_name))
    return false;  // fail!

  DWORD path_len = GetLongPathName(temp_name, temp_name, MAX_PATH);
  if (path_len > MAX_PATH + 1 || path_len == 0)
    return false;  // fail!

  temp_file->assign(temp_name, path_len);
  return true;
}

bool CreateNewTempDirectory(const FilePath::StringType& prefix,
                            FilePath* new_temp_path) {
  FilePath system_temp_dir;
  if (!GetTempDir(&system_temp_dir))
    return false;

  FilePath path_to_create;
  srand(static_cast<uint32_t>(time(NULL)));

  int count = 0;
  while (count < 50) {
    // Try create a new temporary directory with random generated name. If
    // the one exists, keep trying another path name until we reach some limit.
    path_to_create = system_temp_dir;
    std::wstring new_dir_name;
    new_dir_name.assign(prefix);
    new_dir_name.append(IntToWString(rand() % kint16max));
    path_to_create = path_to_create.Append(new_dir_name);

    if (::CreateDirectory(path_to_create.value().c_str(), NULL))
      break;
    count++;
  }

  if (count == 50) {
    return false;
  }

  *new_temp_path = path_to_create;
  return true;
}

bool CreateDirectory(const FilePath& full_path) {
  if (DirectoryExists(full_path))
    return true;
  int err = SHCreateDirectoryEx(NULL, full_path.value().c_str(), NULL);
  return err == ERROR_SUCCESS;
}

bool GetFileInfo(const FilePath& file_path, FileInfo* results) {
  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(file_path.ToWStringHack().c_str(),
                           GetFileExInfoStandard, &attr)) {
    return false;
  }

  ULARGE_INTEGER size;
  size.HighPart = attr.nFileSizeHigh;
  size.LowPart = attr.nFileSizeLow;
  results->size = size.QuadPart;

  results->is_directory =
      (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return true;
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  std::wstring w_mode = ASCIIToWide(std::string(mode));
  FILE* file;
  if (_wfopen_s(&file, filename.value().c_str(), w_mode.c_str()) != 0) {
    return NULL;
  }
  return file;
}

FILE* OpenFile(const std::string& filename, const char* mode) {
  FILE* file;
  if (fopen_s(&file, filename.c_str(), mode) != 0) {
    return NULL;
  }
  return file;
}

int ReadFile(const FilePath& filename, char* data, int size) {
  ScopedHandle file(CreateFile(filename.value().c_str(),
                               GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,
                               OPEN_EXISTING,
                               FILE_FLAG_SEQUENTIAL_SCAN,
                               NULL));
  if (file == INVALID_HANDLE_VALUE)
    return -1;

  int ret_value;
  DWORD read;
  if (::ReadFile(file, data, size, &read, NULL) && read == size) {
    ret_value = static_cast<int>(read);
  } else {
    ret_value = -1;
  }

  return ret_value;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  ScopedHandle file(CreateFile(filename.value().c_str(),
                               GENERIC_WRITE,
                               0,
                               NULL,
                               CREATE_ALWAYS,
                               0,
                               NULL));
  if (file == INVALID_HANDLE_VALUE) {
    LOG(WARNING) << "CreateFile failed for path " << filename.value() <<
        " error code=" << GetLastError() <<
        " error text=" << win_util::FormatLastWin32Error();
    return -1;
  }

  DWORD written;
  BOOL result = ::WriteFile(file, data, size, &written, NULL);
  if (result && written == size)
    return static_cast<int>(written);

  if (!result) {
    // WriteFile failed.
    LOG(WARNING) << "writing file " << filename.value() <<
        " failed, error code=" << GetLastError() <<
        " description=" << win_util::FormatLastWin32Error();
  } else {
    // Didn't write all the bytes.
    LOG(WARNING) << "wrote" << written << " bytes to " <<
        filename.value() << " expected " << size;
  }
  return -1;
}

bool RenameFileAndResetSecurityDescriptor(const FilePath& source_file_path,
                                          const FilePath& target_file_path) {
  // The parameters to SHFileOperation must be terminated with 2 NULL chars.
  std::wstring source = source_file_path.value();
  std::wstring target = target_file_path.value();

  source.append(1, L'\0');
  target.append(1, L'\0');

  SHFILEOPSTRUCT move_info = {0};
  move_info.wFunc = FO_MOVE;
  move_info.pFrom = source.c_str();
  move_info.pTo = target.c_str();
  move_info.fFlags = FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI |
                     FOF_NOCONFIRMMKDIR | FOF_NOCOPYSECURITYATTRIBS;

  if (0 != SHFileOperation(&move_info))
    return false;

  return true;
}

// Gets the current working directory for the process.
bool GetCurrentDirectory(FilePath* dir) {
  wchar_t system_buffer[MAX_PATH];
  system_buffer[0] = 0;
  DWORD len = ::GetCurrentDirectory(MAX_PATH, system_buffer);
  if (len == 0 || len > MAX_PATH)
    return false;
  // TODO(evanm): the old behavior of this function was to always strip the
  // trailing slash.  We duplicate this here, but it shouldn't be necessary
  // when everyone is using the appropriate FilePath APIs.
  std::wstring dir_str(system_buffer);
  file_util::TrimTrailingSeparator(&dir_str);
  *dir = FilePath(dir_str);
  return true;
}

// Sets the current working directory for the process.
bool SetCurrentDirectory(const FilePath& directory) {
  BOOL ret = ::SetCurrentDirectory(directory.value().c_str());
  return ret != 0;
}

///////////////////////////////////////////////
// FileEnumerator

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type)
    : recursive_(recursive),
      file_type_(file_type),
      is_in_find_op_(false),
      find_handle_(INVALID_HANDLE_VALUE) {
  pending_paths_.push(root_path);
}

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               FileEnumerator::FILE_TYPE file_type,
                               const FilePath::StringType& pattern)
    : recursive_(recursive),
      file_type_(file_type),
      is_in_find_op_(false),
      pattern_(pattern),
      find_handle_(INVALID_HANDLE_VALUE) {
  pending_paths_.push(root_path);
}

FileEnumerator::~FileEnumerator() {
  if (find_handle_ != INVALID_HANDLE_VALUE)
    FindClose(find_handle_);
}

void FileEnumerator::GetFindInfo(FindInfo* info) {
  DCHECK(info);

  if (!is_in_find_op_)
    return;

  memcpy(info, &find_data_, sizeof(*info));
}

FilePath FileEnumerator::Next() {
  if (!is_in_find_op_) {
    if (pending_paths_.empty())
      return FilePath();

    // The last find FindFirstFile operation is done, prepare a new one.
    root_path_ = pending_paths_.top();
    pending_paths_.pop();

    // Start a new find operation.
    FilePath src = root_path_;

    if (pattern_.value().empty())
      src = src.Append(L"*");  // No pattern = match everything.
    else
      src = src.Append(pattern_);

    find_handle_ = FindFirstFile(src.value().c_str(), &find_data_);
    is_in_find_op_ = true;

  } else {
    // Search for the next file/directory.
    if (!FindNextFile(find_handle_, &find_data_)) {
      FindClose(find_handle_);
      find_handle_ = INVALID_HANDLE_VALUE;
    }
  }

  if (INVALID_HANDLE_VALUE == find_handle_) {
    is_in_find_op_ = false;

    // This is reached when we have finished a directory and are advancing to
    // the next one in the queue. We applied the pattern (if any) to the files
    // in the root search directory, but for those directories which were
    // matched, we want to enumerate all files inside them. This will happen
    // when the handle is empty.
    pattern_ = FilePath();

    return Next();
  }

  FilePath cur_file(find_data_.cFileName);
  // Skip over . and ..
  if (L"." == cur_file.value() || L".." == cur_file.value())
    return Next();

  // Construct the absolute filename.
  cur_file = root_path_.Append(cur_file);

  if (find_data_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    if (recursive_) {
      // If |cur_file| is a directory, and we are doing recursive searching, add
      // it to pending_paths_ so we scan it after we finish scanning this
      // directory.
      pending_paths_.push(cur_file);
    }
    return (file_type_ & FileEnumerator::DIRECTORIES) ? cur_file : Next();
  }
  return (file_type_ & FileEnumerator::FILES) ? cur_file : Next();
}

///////////////////////////////////////////////
// MemoryMappedFile

MemoryMappedFile::MemoryMappedFile()
    : file_(INVALID_HANDLE_VALUE),
      file_mapping_(INVALID_HANDLE_VALUE),
      data_(NULL),
      length_(INVALID_FILE_SIZE) {
}

bool MemoryMappedFile::MapFileToMemory(const FilePath& file_name) {
  file_ = ::CreateFile(file_name.value().c_str(), GENERIC_READ,
                       FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
  if (file_ == INVALID_HANDLE_VALUE)
    return false;

  length_ = ::GetFileSize(file_, NULL);
  if (length_ == INVALID_FILE_SIZE)
    return false;

  file_mapping_ = ::CreateFileMapping(file_, NULL, PAGE_READONLY,
                                      0, length_, NULL);
  if (file_mapping_ == INVALID_HANDLE_VALUE)
    return false;

  data_ = static_cast<uint8_t*>(
      ::MapViewOfFile(file_mapping_, FILE_MAP_READ, 0, 0, length_));
  return data_ != NULL;
}

void MemoryMappedFile::CloseHandles() {
  if (data_)
    ::UnmapViewOfFile(data_);
  if (file_mapping_ != INVALID_HANDLE_VALUE)
    ::CloseHandle(file_mapping_);
  if (file_ != INVALID_HANDLE_VALUE)
    ::CloseHandle(file_);

  data_ = NULL;
  file_mapping_ = file_ = INVALID_HANDLE_VALUE;
  length_ = INVALID_FILE_SIZE;
}

// Deprecated functions ----------------------------------------------------

void InsertBeforeExtension(std::wstring* path_str,
                           const std::wstring& suffix) {
  FilePath path(*path_str);
  InsertBeforeExtension(&path, suffix);
  path_str->assign(path.value());
}
void PathComponents(const std::wstring& path,
                    std::vector<std::wstring>* components) {
  PathComponents(FilePath(path), components);
}
void ReplaceExtension(std::wstring* file_name, const std::wstring& extension) {
  FilePath path(*file_name);
  ReplaceExtension(&path, extension);
  file_name->assign(path.value());
}
}  // namespace file_util
