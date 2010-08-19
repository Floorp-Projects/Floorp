// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PATH_SERVICE_H__
#define BASE_PATH_SERVICE_H__

#include "build/build_config.h"
#ifdef OS_WIN
// TODO(erikkay): this should be removable, but because SetCurrentDirectory
// is the name of a Windows function, it gets macro-ized to SetCurrentDirectoryW
// by windows.h, which leads to a different name in the header vs. the impl.
// Even if we could fix that, it would still hose all callers of the function.
// The right thing is likely to rename.
#include <windows.h>
#endif

#include <string>

#include "base/base_paths.h"

class FilePath;

// The path service is a global table mapping keys to file system paths.  It is
// OK to use this service from multiple threads.
//
class PathService {
 public:
  // Retrieves a path to a special directory or file and places it into the
  // string pointed to by 'path'. If you ask for a directory it is guaranteed
  // to NOT have a path separator at the end. For example, "c:\windows\temp"
  // Directories are also guaranteed to exist when this function succeeds.
  //
  // Returns true if the directory or file was successfully retrieved. On
  // failure, 'path' will not be changed.
  static bool Get(int key, FilePath* path);
  // This version, producing a wstring, is deprecated and only kept around
  // until we can fix all callers.
  static bool Get(int key, std::wstring* path);

  // Overrides the path to a special directory or file.  This cannot be used to
  // change the value of DIR_CURRENT, but that should be obvious.  Also, if the
  // path specifies a directory that does not exist, the directory will be
  // created by this method.  This method returns true if successful.
  //
  // If the given path is relative, then it will be resolved against
  // DIR_CURRENT.
  //
  // WARNING: Consumers of PathService::Get may expect paths to be constant
  // over the lifetime of the app, so this method should be used with caution.
  static bool Override(int key, const std::wstring& path);

  // Return whether a path was overridden.
  static bool IsOverridden(int key);

  // Sets the current directory.
  static bool SetCurrentDirectory(const std::wstring& current_directory);

  // To extend the set of supported keys, you can register a path provider,
  // which is just a function mirroring PathService::Get.  The ProviderFunc
  // returns false if it cannot provide a non-empty path for the given key.
  // Otherwise, true is returned.
  //
  // WARNING: This function could be called on any thread from which the
  // PathService is used, so the ProviderFunc MUST BE THREADSAFE.
  //
  typedef bool (*ProviderFunc)(int, FilePath*);

  // Call to register a path provider.  You must specify the range "[key_start,
  // key_end)" of supported path keys.
  static void RegisterProvider(ProviderFunc provider,
                               int key_start,
                               int key_end);
 private:
  static bool GetFromCache(int key, FilePath* path);
  static void AddToCache(int key, const FilePath& path);

};

#endif // BASE_PATH_SERVICE_H__
