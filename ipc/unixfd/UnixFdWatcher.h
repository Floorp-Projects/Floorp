/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_UnixFdWatcher_h
#define mozilla_ipc_UnixFdWatcher_h

#include "base/message_loop.h"
#include "mozilla/FileUtils.h"

namespace mozilla {
namespace ipc {

class UnixFdWatcher : public MessageLoopForIO::Watcher
{
public:
  enum {
    READ_WATCHER = 1<<0,
    WRITE_WATCHER = 1<<1
  };

  virtual ~UnixFdWatcher();

  MessageLoop* GetIOLoop() const
  {
    return mIOLoop;
  }

  int GetFd() const
  {
    return mFd;
  }

  bool IsOpen() const
  {
    return GetFd() >= 0;
  }

  virtual void Close();

  void AddWatchers(unsigned long aWatchers, bool aPersistent);
  void RemoveWatchers(unsigned long aWatchers);

  // Callback method that's run before closing the file descriptor
  virtual void OnClose() {};

  // Callback method that's run on POSIX errors
  virtual void OnError(const char* aFunction, int aErrno);

protected:
  UnixFdWatcher(MessageLoop* aIOLoop);
  UnixFdWatcher(MessageLoop* aIOLoop, int aFd);
  void SetFd(int aFd);

private:
  static bool FdIsNonBlocking(int aFd);

  MessageLoop* mIOLoop;
  ScopedClose mFd;
  MessageLoopForIO::FileDescriptorWatcher mReadWatcher;
  MessageLoopForIO::FileDescriptorWatcher mWriteWatcher;
};

}
}

#endif
