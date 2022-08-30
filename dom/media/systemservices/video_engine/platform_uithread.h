/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_PLATFORM_UITHREAD_H_
#define RTC_BASE_PLATFORM_UITHREAD_H_

#if defined(WEBRTC_WIN)
#  include "Assertions.h"
#  include "rtc_base/deprecated/recursive_critical_section.h"
#  include "rtc_base/platform_thread.h"
#  include "api/sequence_checker.h"
#  include "ThreadSafety.h"

namespace rtc {
/*
 * Windows UI thread for screen capture
 * Launches a thread which enters a message wait loop after calling the
 * provided ThreadRunFunction once. A repeating timer event might be registered
 * with a callback through the Win32 API. If so, that timer will cause WM_TIMER
 * messages to appear in the threads message queue. This will wake the thread
 * which will then first look to see if it received the WM_QUIT message, then
 * it will pass any non WM_QUIT messages on to the registered message handlers
 * (synchronously on the current thread). In the case oF WM_TIMER the
 * registered handler calls the NativeEventCallback which is simply the
 * ThreadRunFunction which was passed to the constructor.
 *
 * Shutdown of the message wait loop is triggered by sending a WM_CLOSE which
 * will start tearing down the "window" which hosts the UI thread. This will
 * cause a WM_DESTROY message to be received. Upon reception a WM_QUIT message
 * is enqueued. When the message wait loop receives a WM_QUIT message it stops,
 * thus allowing the thread to be joined.
 *
 * Note: that the only source of a WM_CLOSE should be PlatformUIThread::Stop.
 * Note: because PlatformUIThread::Stop is called from a different thread than
 * PlatformUIThread::Run, it is possible that Stop can race Run.
 *
 * After being stopped PlatformUIThread can not be started again.
 *
 */

class PlatformUIThread {
 public:
  PlatformUIThread(std::function<void()> func, const char* name,
                   ThreadAttributes attributes)
      : name_(name),
        native_event_callback_(std::move(func)),
        monitor_thread_(PlatformThread::SpawnJoinable([this]() { Run(); }, name,
                                                      attributes)) {}

  virtual ~PlatformUIThread();

  void Stop();

  /**
   * Request a recurring callback.
   */
  bool RequestCallbackTimer(unsigned int milliseconds);

 protected:
  void Run();

 private:
  static LRESULT CALLBACK EventWindowProc(HWND, UINT, WPARAM, LPARAM);
  void NativeEventCallback();
  // Initialize the UI thread that is servicing the timer events
  bool InternalInit();

  // Needs to be initialized before monitor_thread_ as it takes a string view to
  // name_
  std::string name_;
  RecursiveCriticalSection cs_;
  std::function<void()> native_event_callback_;
  webrtc::SequenceChecker thread_checker_;
  PlatformThread monitor_thread_;
  HWND hwnd_ MOZ_GUARDED_BY(cs_) = nullptr;
  UINT_PTR timerid_ MOZ_GUARDED_BY(cs_) = 0;
  unsigned int timeout_ MOZ_GUARDED_BY(cs_) = 0;
  enum class State {
    UNSTARTED,
    STARTED,
    STOPPED,
  };
  State state_ MOZ_GUARDED_BY(cs_) = State::UNSTARTED;
};

}  // namespace rtc

#endif
#endif  // RTC_BASE_PLATFORM_UITHREAD_H_
