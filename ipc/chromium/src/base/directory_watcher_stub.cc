// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file exists for Linux systems which don't have the inotify headers, and
// thus cannot build directory_watcher_inotify.cc

#include "base/directory_watcher.h"

class DirectoryWatcherImpl : public DirectoryWatcher::PlatformDelegate {
 public:
  virtual bool Watch(const FilePath& path, DirectoryWatcher::Delegate* delegate,
                     bool recursive) {
    return false;
  }
};

DirectoryWatcher::DirectoryWatcher() {
  impl_ = new DirectoryWatcherImpl();
}
