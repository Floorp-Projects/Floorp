// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREAD_H_
#define BASE_THREAD_H_

#include <string>

#include "base/message_loop.h"
#include "base/platform_thread.h"

namespace base {

// A simple thread abstraction that establishes a MessageLoop on a new thread.
// The consumer uses the MessageLoop of the thread to cause code to execute on
// the thread.  When this object is destroyed the thread is terminated.  All
// pending tasks queued on the thread's message loop will run to completion
// before the thread is terminated.
class Thread : PlatformThread::Delegate {
 public:
  struct Options {
    // Specifies the type of message loop that will be allocated on the thread.
    MessageLoop::Type message_loop_type;

    // Specifies the maximum stack size that the thread is allowed to use.
    // This does not necessarily correspond to the thread's initial stack size.
    // A value of 0 indicates that the default maximum should be used.
    size_t stack_size;

    Options() : message_loop_type(MessageLoop::TYPE_DEFAULT), stack_size(0) {}
    Options(MessageLoop::Type type, size_t size)
        : message_loop_type(type), stack_size(size) {}
  };

  // Constructor.
  // name is a display string to identify the thread.
  explicit Thread(const char *name);

  // Destroys the thread, stopping it if necessary.
  //
  // NOTE: If you are subclassing from Thread, and you wish for your CleanUp
  // method to be called, then you need to call Stop() from your destructor.
  //
  virtual ~Thread();

  // Starts the thread.  Returns true if the thread was successfully started;
  // otherwise, returns false.  Upon successful return, the message_loop()
  // getter will return non-null.
  //
  // Note: This function can't be called on Windows with the loader lock held;
  // i.e. during a DllMain, global object construction or destruction, atexit()
  // callback.
  bool Start();

  // Starts the thread. Behaves exactly like Start in addition to allow to
  // override the default options.
  //
  // Note: This function can't be called on Windows with the loader lock held;
  // i.e. during a DllMain, global object construction or destruction, atexit()
  // callback.
  bool StartWithOptions(const Options& options);

  // Signals the thread to exit and returns once the thread has exited.  After
  // this method returns, the Thread object is completely reset and may be used
  // as if it were newly constructed (i.e., Start may be called again).
  //
  // Stop may be called multiple times and is simply ignored if the thread is
  // already stopped.
  //
  // NOTE: This method is optional.  It is not strictly necessary to call this
  // method as the Thread's destructor will take care of stopping the thread if
  // necessary.
  //
  void Stop();

  // Signals the thread to exit in the near future.
  //
  // WARNING: This function is not meant to be commonly used. Use at your own
  // risk. Calling this function will cause message_loop() to become invalid in
  // the near future. This function was created to workaround a specific
  // deadlock on Windows with printer worker thread. In any other case, Stop()
  // should be used.
  //
  // StopSoon should not be called multiple times as it is risky to do so. It
  // could cause a timing issue in message_loop() access. Call Stop() to reset
  // the thread object once it is known that the thread has quit.
  void StopSoon();

  // Returns the message loop for this thread.  Use the MessageLoop's
  // PostTask methods to execute code on the thread.  This only returns
  // non-null after a successful call to Start.  After Stop has been called,
  // this will return NULL.
  //
  // NOTE: You must not call this MessageLoop's Quit method directly.  Use
  // the Thread's Stop method instead.
  //
  MessageLoop* message_loop() const { return message_loop_; }

  // Set the name of this thread (for display in debugger too).
  const std::string &thread_name() { return name_; }

  // The native thread handle.
  PlatformThreadHandle thread_handle() { return thread_; }

  // The thread ID.
  PlatformThreadId thread_id() const { return thread_id_; }

  // Reset thread ID as current thread.
  PlatformThreadId reset_thread_id() {
      thread_id_ = PlatformThread::CurrentId();
      return thread_id_;
  }

  // Returns true if the thread has been started, and not yet stopped.
  // When a thread is running, the thread_id_ is non-zero.
  bool IsRunning() const { return thread_id_ != 0; }

 protected:
  // Called just prior to starting the message loop
  virtual void Init() {}

  // Called just after the message loop ends
  virtual void CleanUp() {}

  static void SetThreadWasQuitProperly(bool flag);
  static bool GetThreadWasQuitProperly();

 private:
  // PlatformThread::Delegate methods:
  virtual void ThreadMain();

  // We piggy-back on the startup_data_ member to know if we successfully
  // started the thread.  This way we know that we need to call Join.
  bool thread_was_started() const { return startup_data_ != NULL; }

  // Used to pass data to ThreadMain.
  struct StartupData;
  StartupData* startup_data_;

  // The thread's handle.
  PlatformThreadHandle thread_;

  // The thread's message loop.  Valid only while the thread is alive.  Set
  // by the created thread.
  MessageLoop* message_loop_;

  // Our thread's ID.
  PlatformThreadId thread_id_;

  // The name of the thread.  Used for debugging purposes.
  std::string name_;

  friend class ThreadQuitTask;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

}  // namespace base

#endif  // BASE_THREAD_H_
