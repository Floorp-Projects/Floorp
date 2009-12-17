// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_OBJECT_WATCHER_H_
#define BASE_OBJECT_WATCHER_H_

#include <windows.h>

#include "base/message_loop.h"

namespace base {

// A class that provides a means to asynchronously wait for a Windows object to
// become signaled.  It is an abstraction around RegisterWaitForSingleObject
// that provides a notification callback, OnObjectSignaled, that runs back on
// the origin thread (i.e., the thread that called StartWatching).
//
// This class acts like a smart pointer such that when it goes out-of-scope,
// UnregisterWaitEx is automatically called, and any in-flight notification is
// suppressed.
//
// Typical usage:
//
//   class MyClass : public base::ObjectWatcher::Delegate {
//    public:
//     void DoStuffWhenSignaled(HANDLE object) {
//       watcher_.StartWatching(object, this);
//     }
//     virtual void OnObjectSignaled(HANDLE object) {
//       // OK, time to do stuff!
//     }
//    private:
//     base::ObjectWatcher watcher_;
//   };
//
// In the above example, MyClass wants to "do stuff" when object becomes
// signaled.  ObjectWatcher makes this task easy.  When MyClass goes out of
// scope, the watcher_ will be destroyed, and there is no need to worry about
// OnObjectSignaled being called on a deleted MyClass pointer.  Easy!
//
class ObjectWatcher : public MessageLoop::DestructionObserver {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Called from the MessageLoop when a signaled object is detected.  To
    // continue watching the object, AddWatch must be called again.
    virtual void OnObjectSignaled(HANDLE object) = 0;
  };

  ObjectWatcher();
  ~ObjectWatcher();

  // When the object is signaled, the given delegate is notified on the thread
  // where StartWatching is called.  The ObjectWatcher is not responsible for
  // deleting the delegate.
  //
  // Returns true if the watch was started.  Otherwise, false is returned.
  //
  bool StartWatching(HANDLE object, Delegate* delegate);

  // Stops watching.  Does nothing if the watch has already completed.  If the
  // watch is still active, then it is canceled, and the associated delegate is
  // not notified.
  //
  // Returns true if the watch was canceled.  Otherwise, false is returned.
  //
  bool StopWatching();

  // Returns the handle of the object being watched, or NULL if the object
  // watcher is stopped.
  HANDLE GetWatchedObject();

 private:
  // Called on a background thread when done waiting.
  static void CALLBACK DoneWaiting(void* param, BOOLEAN timed_out);

  // MessageLoop::DestructionObserver implementation:
  virtual void WillDestroyCurrentMessageLoop();

  // Internal state.
  struct Watch;
  Watch* watch_;

  DISALLOW_COPY_AND_ASSIGN(ObjectWatcher);
};

}  // namespace base

#endif  // BASE_OBJECT_WATCHER_H_
