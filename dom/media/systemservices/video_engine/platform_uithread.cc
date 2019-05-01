/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "platform_uithread.h"

namespace rtc {

#if defined(WEBRTC_WIN)
// For use in ThreadWindowsUI callbacks
static UINT static_reg_windows_msg =
    RegisterWindowMessageW(L"WebrtcWindowsUIThreadEvent");
// timer id used in delayed callbacks
static const UINT_PTR kTimerId = 1;
static const wchar_t kThisProperty[] = L"ThreadWindowsUIPtr";
static const wchar_t kThreadWindow[] = L"WebrtcWindowsUIThread";

bool PlatformUIThread::InternalInit() {
  // Create an event window for use in generating callbacks to capture
  // objects.
  CritScope scoped_lock(&cs_);
  if (hwnd_ == NULL) {
    WNDCLASSW wc;
    HMODULE hModule = GetModuleHandle(NULL);
    if (!GetClassInfoW(hModule, kThreadWindow, &wc)) {
      ZeroMemory(&wc, sizeof(WNDCLASSW));
      wc.hInstance = hModule;
      wc.lpfnWndProc = EventWindowProc;
      wc.lpszClassName = kThreadWindow;
      RegisterClassW(&wc);
    }
    hwnd_ = CreateWindowW(kThreadWindow, L"", 0, 0, 0, 0, 0, NULL, NULL,
                          hModule, NULL);
    RTC_DCHECK(hwnd_);
    SetPropW(hwnd_, kThisProperty, this);

    if (timeout_) {
      // if someone set the timer before we started
      RequestCallbackTimer(timeout_);
    }
  }
  return !!hwnd_;
}

void PlatformUIThread::RequestCallback() {
  RTC_DCHECK(hwnd_);
  RTC_DCHECK(static_reg_windows_msg);
  PostMessage(hwnd_, static_reg_windows_msg, 0, 0);
}

bool PlatformUIThread::RequestCallbackTimer(unsigned int milliseconds) {
  CritScope scoped_lock(&cs_);
  if (!hwnd_) {
    // There is a condition that thread_ (PlatformUIThread) has been
    // created but PlatformUIThread::Run() hasn't been run yet (hwnd_ is
    // null while thread_ is not). If we do RTC_DCHECK(!thread_) here,
    // it would lead to crash in this condition.

    // set timer once thread starts
  } else {
    if (timerid_) {
      KillTimer(hwnd_, timerid_);
    }
    timerid_ = SetTimer(hwnd_, kTimerId, milliseconds, NULL);
  }
  timeout_ = milliseconds;
  return !!timerid_;
}

void PlatformUIThread::Stop() {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  // Shut down the dispatch loop and let the background thread exit.
  if (timerid_) {
    KillTimer(hwnd_, timerid_);
    timerid_ = 0;
  }

  PostMessage(hwnd_, WM_CLOSE, 0, 0);

  hwnd_ = NULL;

  PlatformThread::Stop();
}

void PlatformUIThread::Run() {
  RTC_CHECK(InternalInit());  // always evaluates
  do {
    // The interface contract of Start/Stop is that for a successful call to
    // Start, there should be at least one call to the run function.  So we
    // call the function before checking |stop_|.
    run_function_deprecated_(obj_);

    // Alertable sleep to permit RaiseFlag to run and update |stop_|.
    if (MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT,
                                    MWMO_ALERTABLE | MWMO_INPUTAVAILABLE) ==
        WAIT_OBJECT_0) {
      MSG msg;
      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          stop_ = true;
          break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }

  } while (!stop_);
}

void PlatformUIThread::NativeEventCallback() {
  if (!run_function_deprecated_) {
    stop_ = true;
    return;
  }
  run_function_deprecated_(obj_);
}

/* static */
LRESULT CALLBACK PlatformUIThread::EventWindowProc(HWND hwnd, UINT uMsg,
                                                   WPARAM wParam,
                                                   LPARAM lParam) {
  if (uMsg == WM_DESTROY) {
    RemovePropW(hwnd, kThisProperty);
    PostQuitMessage(0);
    return 0;
  }

  PlatformUIThread* twui =
      static_cast<PlatformUIThread*>(GetPropW(hwnd, kThisProperty));
  if (!twui) {
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
  }

  if ((uMsg == static_reg_windows_msg && uMsg != WM_NULL) ||
      (uMsg == WM_TIMER && wParam == kTimerId)) {
    twui->NativeEventCallback();
    return 0;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
#endif

}  // namespace rtc
