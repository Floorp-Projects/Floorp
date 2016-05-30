/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_IOThreadChild_h
#define dom_plugins_IOThreadChild_h 1

#include "chrome/common/child_thread.h"

namespace mozilla {
namespace ipc {
//-----------------------------------------------------------------------------

// The IOThreadChild class represents a background thread where the
// IPC IO MessageLoop lives.
class IOThreadChild : public ChildThread {
public:
  IOThreadChild()
    : ChildThread(base::Thread::Options(MessageLoop::TYPE_IO,
                                        0)) // stack size
  { }

  ~IOThreadChild()
  { }

  static MessageLoop* message_loop() {
    return IOThreadChild::current()->Thread::message_loop();
  }

  // IOThreadChild owns the returned IPC::Channel.
  static IPC::Channel* channel() {
    return IOThreadChild::current()->ChildThread::channel();
  }

protected:
  static IOThreadChild* current() {
    return static_cast<IOThreadChild*>(ChildThread::current());
  }

private:
  DISALLOW_EVIL_CONSTRUCTORS(IOThreadChild);
};

} // namespace ipc
} // namespace mozilla

#endif  // ifndef dom_plugins_IOThreadChild_h
