// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/directory_watcher.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/object_watcher.h"
#include "base/ref_counted.h"

namespace {

class DirectoryWatcherImpl : public DirectoryWatcher::PlatformDelegate,
                             public base::ObjectWatcher::Delegate {
 public:
  DirectoryWatcherImpl() : handle_(INVALID_HANDLE_VALUE) {}
  virtual ~DirectoryWatcherImpl();

  virtual bool Watch(const FilePath& path, DirectoryWatcher::Delegate* delegate,
                     bool recursive);

  // Callback from MessageLoopForIO.
  virtual void OnObjectSignaled(HANDLE object);

 private:
  // Delegate to notify upon changes.
  DirectoryWatcher::Delegate* delegate_;
  // Path we're watching (passed to delegate).
  FilePath path_;
  // Handle for FindFirstChangeNotification.
  HANDLE handle_;
  // ObjectWatcher to watch handle_ for events.
  base::ObjectWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryWatcherImpl);
};

DirectoryWatcherImpl::~DirectoryWatcherImpl() {
  if (handle_ != INVALID_HANDLE_VALUE) {
    watcher_.StopWatching();
    FindCloseChangeNotification(handle_);
  }
}

bool DirectoryWatcherImpl::Watch(const FilePath& path,
    DirectoryWatcher::Delegate* delegate, bool recursive) {
  DCHECK(path_.value().empty());  // Can only watch one path.

  handle_ = FindFirstChangeNotification(
      path.value().c_str(),
      recursive,
      FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_SIZE |
      FILE_NOTIFY_CHANGE_LAST_WRITE);
  if (handle_ == INVALID_HANDLE_VALUE)
    return false;

  delegate_ = delegate;
  path_ = path;
  watcher_.StartWatching(handle_, this);

  return true;
}

void DirectoryWatcherImpl::OnObjectSignaled(HANDLE object) {
  DCHECK(object == handle_);
  // Make sure we stay alive through the body of this function.
  scoped_refptr<DirectoryWatcherImpl> keep_alive(this);

  delegate_->OnDirectoryChanged(path_);

  // Register for more notifications on file change.
  BOOL ok = FindNextChangeNotification(object);
  DCHECK(ok);
  watcher_.StartWatching(object, this);
}

}  // namespace

DirectoryWatcher::DirectoryWatcher() {
  impl_ = new DirectoryWatcherImpl();
}
