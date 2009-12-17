// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides a way to monitor a directory for changes.

#ifndef BASE_DIRECTORY_WATCHER_H_
#define BASE_DIRECTORY_WATCHER_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"

class FilePath;

// This class lets you register interest in changes on a directory.
// The delegate will get called whenever a file is added or changed in the
// directory.
class DirectoryWatcher {
 public:
  class Delegate {
   public:
    virtual void OnDirectoryChanged(const FilePath& path) = 0;
  };

  DirectoryWatcher();
  ~DirectoryWatcher() {}

  // Register interest in any changes in the directory |path|.
  // OnDirectoryChanged will be called back for each change within the dir.
  // If |recursive| is true, the delegate will be notified for each change
  // within the directory tree starting at |path|. Returns false on error.
  //
  // Note: on Windows you may got more notifications for non-recursive watch
  // than you expect, especially on versions earlier than Vista. The behavior
  // is consistent on any particular version of Windows, but not across
  // different versions.
  bool Watch(const FilePath& path, Delegate* delegate, bool recursive) {
    return impl_->Watch(path, delegate, recursive);
  }

  // Used internally to encapsulate different members on different platforms.
  class PlatformDelegate : public base::RefCounted<PlatformDelegate> {
   public:
    virtual ~PlatformDelegate() {}
    virtual bool Watch(const FilePath& path, Delegate* delegate,
                       bool recursive) = 0;
  };

 private:
  scoped_refptr<PlatformDelegate> impl_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryWatcher);
};

#endif  // BASE_DIRECTORY_WATCHER_H_
