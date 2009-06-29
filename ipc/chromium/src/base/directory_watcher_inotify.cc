// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/directory_watcher.h"

#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <sys/select.h>
#include <unistd.h>

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/task.h"
#include "base/thread.h"

namespace {

// Singleton to manage all inotify watches.
class InotifyReader {
 public:
  typedef int Watch;  // Watch descriptor used by AddWatch and RemoveWatch.
  static const Watch kInvalidWatch = -1;

  // Watch |path| for changes. |delegate| will be notified on each change. Does
  // not check for duplicates. If you call it n times with same |path|
  // and |delegate|, it will receive n notifications for each change
  // in |path|. It makes implementation of DirectoryWatcher simple.
  // Returns kInvalidWatch on failure.
  Watch AddWatch(const FilePath& path, DirectoryWatcher::Delegate* delegate);

  // Remove |watch| for |delegate|. If you had n watches for same |delegate|
  // and path, after calling this function you will have n - 1.
  // Returns true on success.
  bool RemoveWatch(Watch watch, DirectoryWatcher::Delegate* delegate);

  // Callback for InotifyReaderTask.
  void OnInotifyEvent(inotify_event* event);

 private:
  friend struct DefaultSingletonTraits<InotifyReader>;

  typedef std::pair<DirectoryWatcher::Delegate*, MessageLoop*> DelegateInfo;
  typedef std::multiset<DelegateInfo> DelegateSet;

  InotifyReader();
  ~InotifyReader();

  // We keep track of which delegates want to be notified on which watches.
  // Multiset is used because there may be many DirectoryWatchers for same path
  // and delegate.
  base::hash_map<Watch, DelegateSet> delegates_;

  // For each watch we also want to know the path it's watching.
  base::hash_map<Watch, FilePath> paths_;

  // Lock to protect delegates_ and paths_.
  Lock lock_;

  // Separate thread on which we run blocking read for inotify events.
  base::Thread thread_;

  // File descriptor returned by inotify_init.
  const int inotify_fd_;

  // Use self-pipe trick to unblock select during shutdown.
  int shutdown_pipe_[2];

  // Flag set to true when startup was successful.
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(InotifyReader);
};

class InotifyReaderTask : public Task {
 public:
  InotifyReaderTask(InotifyReader* reader, int inotify_fd, int shutdown_fd)
      : reader_(reader),
        inotify_fd_(inotify_fd),
        shutdown_fd_(shutdown_fd) {
  }

  virtual void Run() {
    while (true) {
      fd_set rfds;
      FD_ZERO(&rfds);
      FD_SET(inotify_fd_, &rfds);
      FD_SET(shutdown_fd_, &rfds);

      // Wait until some inotify events are available.
      int select_result =
        HANDLE_EINTR(select(std::max(inotify_fd_, shutdown_fd_) + 1,
                            &rfds, NULL, NULL, NULL));
      if (select_result < 0) {
        DLOG(WARNING) << "select failed: " << strerror(errno);
        return;
      }

      if (FD_ISSET(shutdown_fd_, &rfds))
        return;

      // Adjust buffer size to current event queue size.
      int buffer_size;
      int ioctl_result = HANDLE_EINTR(ioctl(inotify_fd_, FIONREAD,
                                            &buffer_size));

      if (ioctl_result != 0) {
        DLOG(WARNING) << "ioctl failed: " << strerror(errno);
        return;
      }

      std::vector<char> buffer(buffer_size);

      ssize_t bytes_read = HANDLE_EINTR(read(inotify_fd_, &buffer[0],
                                             buffer_size));

      if (bytes_read < 0) {
        DLOG(WARNING) << "read from inotify fd failed: " << strerror(errno);
        return;
      }

      ssize_t i = 0;
      while (i < bytes_read) {
        inotify_event* event = reinterpret_cast<inotify_event*>(&buffer[i]);
        size_t event_size = sizeof(inotify_event) + event->len;
        DCHECK(i + event_size <= static_cast<size_t>(bytes_read));
        reader_->OnInotifyEvent(event);
        i += event_size;
      }
    }
  }

 private:
  InotifyReader* reader_;
  int inotify_fd_;
  int shutdown_fd_;

  DISALLOW_COPY_AND_ASSIGN(InotifyReaderTask);
};

class InotifyReaderNotifyTask : public Task {
 public:
  InotifyReaderNotifyTask(DirectoryWatcher::Delegate* delegate,
                          const FilePath& path)
      : delegate_(delegate),
        path_(path) {
  }

  virtual void Run() {
    delegate_->OnDirectoryChanged(path_);
  }

 private:
  DirectoryWatcher::Delegate* delegate_;
  FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(InotifyReaderNotifyTask);
};

InotifyReader::InotifyReader()
    : thread_("inotify_reader"),
      inotify_fd_(inotify_init()),
      valid_(false) {
  shutdown_pipe_[0] = -1;
  shutdown_pipe_[1] = -1;
  if (inotify_fd_ >= 0 && pipe(shutdown_pipe_) == 0 && thread_.Start()) {
    thread_.message_loop()->PostTask(
        FROM_HERE, new InotifyReaderTask(this, inotify_fd_, shutdown_pipe_[0]));
    valid_ = true;
  }
}

InotifyReader::~InotifyReader() {
  if (valid_) {
    // Write to the self-pipe so that the select call in InotifyReaderTask
    // returns.
    HANDLE_EINTR(write(shutdown_pipe_[1], "", 1));
    thread_.Stop();
  }
  if (inotify_fd_ >= 0)
    close(inotify_fd_);
  if (shutdown_pipe_[0] >= 0)
    close(shutdown_pipe_[0]);
  if (shutdown_pipe_[1] >= 0)
    close(shutdown_pipe_[1]);
}

InotifyReader::Watch InotifyReader::AddWatch(
    const FilePath& path, DirectoryWatcher::Delegate* delegate) {
  if (!valid_)
    return kInvalidWatch;

  AutoLock auto_lock(lock_);

  Watch watch = inotify_add_watch(inotify_fd_, path.value().c_str(),
                                  IN_CREATE | IN_DELETE |
                                  IN_CLOSE_WRITE | IN_MOVE);
  if (watch == kInvalidWatch)
    return kInvalidWatch;

  if (paths_[watch].empty())
    paths_[watch] = path;  // We don't yet watch this path.

  delegates_[watch].insert(std::make_pair(delegate, MessageLoop::current()));

  return watch;
}

bool InotifyReader::RemoveWatch(Watch watch,
                                DirectoryWatcher::Delegate* delegate) {
  if (!valid_)
    return false;

  AutoLock auto_lock(lock_);

  if (paths_[watch].empty())
    return false;  // We don't recognize this watch.

  // Only erase one occurrence of delegate (there may be more).
  delegates_[watch].erase(
      delegates_[watch].find(std::make_pair(delegate, MessageLoop::current())));

  if (delegates_[watch].empty()) {
    paths_.erase(watch);
    delegates_.erase(watch);

    return (inotify_rm_watch(inotify_fd_, watch) == 0);
  }

  return true;
}

void InotifyReader::OnInotifyEvent(inotify_event* event) {
  if (event->mask & IN_IGNORED)
    return;

  DelegateSet delegates_to_notify;
  FilePath changed_path;

  {
    AutoLock auto_lock(lock_);
    changed_path = paths_[event->wd];
    delegates_to_notify.insert(delegates_[event->wd].begin(),
                               delegates_[event->wd].end());
  }

  DelegateSet::iterator i;
  for (i = delegates_to_notify.begin(); i != delegates_to_notify.end(); ++i) {
    DirectoryWatcher::Delegate* delegate = i->first;
    MessageLoop* loop = i->second;
    loop->PostTask(FROM_HERE,
                   new InotifyReaderNotifyTask(delegate, changed_path));
  }
}

class DirectoryWatcherImpl : public DirectoryWatcher::PlatformDelegate {
 public:
  DirectoryWatcherImpl() : watch_(InotifyReader::kInvalidWatch) {}
  ~DirectoryWatcherImpl();

  virtual bool Watch(const FilePath& path, DirectoryWatcher::Delegate* delegate,
                     bool recursive);

 private:
  // Delegate to notify upon changes.
  DirectoryWatcher::Delegate* delegate_;

  // Path we're watching (passed to delegate).
  FilePath path_;

  // Watch returned by InotifyReader.
  InotifyReader::Watch watch_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryWatcherImpl);
};

DirectoryWatcherImpl::~DirectoryWatcherImpl() {
  if (watch_ != InotifyReader::kInvalidWatch)
    Singleton<InotifyReader>::get()->RemoveWatch(watch_, delegate_);
}

bool DirectoryWatcherImpl::Watch(const FilePath& path,
    DirectoryWatcher::Delegate* delegate, bool recursive) {
  DCHECK(watch_ == InotifyReader::kInvalidWatch);  // Can only watch one path.

  if (recursive) {
    // TODO(phajdan.jr): Support recursive watches.
    // Unfortunately inotify has no "native" support for them, but it can be
    // emulated by walking the directory tree and setting up watches for each
    // directory. Of course this is ineffective for large directory trees.
    // For the algorithm, see the link below:
    // http://osdir.com/ml/gnome.dashboard.devel/2004-10/msg00022.html
    NOTIMPLEMENTED();
    return false;
  }

  delegate_ = delegate;
  path_ = path;
  watch_ = Singleton<InotifyReader>::get()->AddWatch(path, delegate_);

  return watch_ != InotifyReader::kInvalidWatch;
}

}  // namespace

DirectoryWatcher::DirectoryWatcher() {
  impl_ = new DirectoryWatcherImpl();
}
