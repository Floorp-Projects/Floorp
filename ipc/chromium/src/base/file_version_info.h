// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILE_VERSION_INFO_H__
#define BASE_FILE_VERSION_INFO_H__

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"

#if defined(OS_WIN)
struct tagVS_FIXEDFILEINFO;
typedef tagVS_FIXEDFILEINFO VS_FIXEDFILEINFO;
#elif defined(OS_MACOSX)
#ifdef __OBJC__
@class NSBundle;
#else
class NSBundle;
#endif
#endif

// Provides a way to access the version information for a file.
// This is the information you access when you select a file in the Windows
// explorer, right-click select Properties, then click the Version tab.

class FileVersionInfo {
 public:
  // Creates a FileVersionInfo for the specified path. Returns NULL if something
  // goes wrong (typically the file does not exit or cannot be opened). The
  // returned object should be deleted when you are done with it.
  static FileVersionInfo* CreateFileVersionInfo(const FilePath& file_path);
  // This version, taking a wstring, is deprecated and only kept around
  // until we can fix all callers.
  static FileVersionInfo* CreateFileVersionInfo(const std::wstring& file_path);

  // Creates a FileVersionInfo for the current module. Returns NULL in case
  // of error. The returned object should be deleted when you are done with it.
  static FileVersionInfo* CreateFileVersionInfoForCurrentModule();

  ~FileVersionInfo();

  // Accessors to the different version properties.
  // Returns an empty string if the property is not found.
  std::wstring company_name();
  std::wstring company_short_name();
  std::wstring product_name();
  std::wstring product_short_name();
  std::wstring internal_name();
  std::wstring product_version();
  std::wstring private_build();
  std::wstring special_build();
  std::wstring comments();
  std::wstring original_filename();
  std::wstring file_description();
  std::wstring file_version();
  std::wstring legal_copyright();
  std::wstring legal_trademarks();
  std::wstring last_change();
  bool is_official_build();

  // Lets you access other properties not covered above.
  bool GetValue(const wchar_t* name, std::wstring* value);

  // Similar to GetValue but returns a wstring (empty string if the property
  // does not exist).
  std::wstring GetStringValue(const wchar_t* name);

#ifdef OS_WIN
  // Get the fixed file info if it exists. Otherwise NULL
  VS_FIXEDFILEINFO* fixed_file_info() { return fixed_file_info_; }
#endif

 private:
#if defined(OS_WIN)
  FileVersionInfo(void* data, int language, int code_page);

  scoped_ptr_malloc<char> data_;
  int language_;
  int code_page_;
  // This is a pointer into the data_ if it exists. Otherwise NULL.
  VS_FIXEDFILEINFO* fixed_file_info_;
#elif defined(OS_MACOSX)
  explicit FileVersionInfo(NSBundle *bundle);

  NSBundle *bundle_;
#elif defined(OS_LINUX)
  FileVersionInfo();
#endif

  DISALLOW_EVIL_CONSTRUCTORS(FileVersionInfo);
};

#endif  // BASE_FILE_VERSION_INFO_H__
