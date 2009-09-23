// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/directory_watcher.h"

#include <CoreServices/CoreServices.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_cftyperef.h"

namespace {

const CFAbsoluteTime kEventLatencySeconds = 0.3;

class DirectoryWatcherImpl : public DirectoryWatcher::PlatformDelegate {
 public:
  DirectoryWatcherImpl() {}
  ~DirectoryWatcherImpl() {
    if (!path_.value().empty()) {
      FSEventStreamStop(fsevent_stream_);
      FSEventStreamInvalidate(fsevent_stream_);
      FSEventStreamRelease(fsevent_stream_);
    }
  }

  virtual bool Watch(const FilePath& path, DirectoryWatcher::Delegate* delegate,
                     bool recursive);

  void OnFSEventsCallback(const FilePath& event_path) {
    DCHECK(!path_.value().empty());
    if (!recursive_) {
      FilePath absolute_event_path = event_path;
      if (!file_util::AbsolutePath(&absolute_event_path))
        return;
      if (absolute_event_path != path_)
        return;
    }
    delegate_->OnDirectoryChanged(path_);
  }

 private:
  // Delegate to notify upon changes.
  DirectoryWatcher::Delegate* delegate_;

  // Path we're watching (passed to delegate).
  FilePath path_;

  // Indicates recursive watch.
  bool recursive_;

  // Backend stream we receive event callbacks from (strong reference).
  FSEventStreamRef fsevent_stream_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryWatcherImpl);
};

void FSEventsCallback(ConstFSEventStreamRef stream,
                      void* event_watcher, size_t num_events,
                      void* event_paths, const FSEventStreamEventFlags flags[],
                      const FSEventStreamEventId event_ids[]) {
  char** paths = reinterpret_cast<char**>(event_paths);
  DirectoryWatcherImpl* watcher =
      reinterpret_cast<DirectoryWatcherImpl*> (event_watcher);
  for (size_t i = 0; i < num_events; i++) {
    watcher->OnFSEventsCallback(FilePath(paths[i]));
  }
}

bool DirectoryWatcherImpl::Watch(const FilePath& path,
                                 DirectoryWatcher::Delegate* delegate,
                                 bool recursive) {
  DCHECK(path_.value().empty());  // Can only watch one path.

  DCHECK(MessageLoop::current()->type() == MessageLoop::TYPE_UI);

  if (!file_util::PathExists(path))
    return false;

  path_ = path;
  if (!file_util::AbsolutePath(&path_)) {
    path_ = FilePath();  // Make sure we're marked as not-in-use.
    return false;
  }
  delegate_ = delegate;
  recursive_ = recursive;

  scoped_cftyperef<CFStringRef> cf_path(CFStringCreateWithCString(
      NULL, path.value().c_str(), kCFStringEncodingMacHFS));
  CFStringRef path_for_array = cf_path.get();
  scoped_cftyperef<CFArrayRef> watched_paths(CFArrayCreate(
      NULL, reinterpret_cast<const void**>(&path_for_array), 1,
      &kCFTypeArrayCallBacks));

  FSEventStreamContext context;
  context.version = 0;
  context.info = this;
  context.retain = NULL;
  context.release = NULL;
  context.copyDescription = NULL;

  fsevent_stream_ = FSEventStreamCreate(NULL, &FSEventsCallback, &context,
                                        watched_paths,
                                        kFSEventStreamEventIdSinceNow,
                                        kEventLatencySeconds,
                                        kFSEventStreamCreateFlagNone);
  FSEventStreamScheduleWithRunLoop(fsevent_stream_, CFRunLoopGetCurrent(),
                                   kCFRunLoopDefaultMode);
  FSEventStreamStart(fsevent_stream_);

  return true;
}

}  // namespace

DirectoryWatcher::DirectoryWatcher() {
  impl_ = new DirectoryWatcherImpl();
}
