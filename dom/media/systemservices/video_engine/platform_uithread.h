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

#include "rtc_base/platform_thread.h"

namespace rtc {

#if defined(WEBRTC_WIN)
class PlatformUIThread : public PlatformThread {
 public:
  PlatformUIThread(ThreadRunFunctionDeprecated func, void* obj,
                   const char* thread_name)
      : PlatformThread(func, obj, thread_name),
        hwnd_(nullptr),
        timerid_(0),
        timeout_(0) {}
  virtual ~PlatformUIThread() {}

  void Stop() override;

  /**
   * Request a recurring callback.
   */
  bool RequestCallbackTimer(unsigned int milliseconds);

 protected:
  void Run() override;

 private:
  static LRESULT CALLBACK EventWindowProc(HWND, UINT, WPARAM, LPARAM);
  void NativeEventCallback();
  bool InternalInit();

  HWND hwnd_;
  UINT_PTR timerid_;
  unsigned int timeout_;
};
#endif

}  // namespace rtc

#endif  // RTC_BASE_PLATFORM_UITHREAD_H_
