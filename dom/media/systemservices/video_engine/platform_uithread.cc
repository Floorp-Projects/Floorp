/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_WIN)

#  include "platform_uithread.h"

namespace rtc {

// timer id used in delayed callbacks
static const UINT_PTR kTimerId = 1;
static const wchar_t kThisProperty[] = L"ThreadWindowsUIPtr";
static const wchar_t kThreadWindow[] = L"WebrtcWindowsUIThread";

PlatformUIThread::~PlatformUIThread() {
  CritScope scoped_lock(&cs_);
  switch (state_) {
    case State::STARTED: {
      MOZ_DIAGNOSTIC_ASSERT(
          false, "PlatformUIThread must be stopped before destruction");
      break;
    }
    case State::STOPPED:
      break;
    case State::UNSTARTED:
      break;
  }
}

bool PlatformUIThread::InternalInit() {
  // Create an event window for use in generating callbacks to capture
  // objects.
  CritScope scoped_lock(&cs_);
  switch (state_) {
    // We have already started there is nothing todo. Should this be assert?
    case State::STARTED:
      break;
    // Stop() has already been called so there is likewise nothing to do.
    case State::STOPPED:
      break;
    // Stop() has not been called yet, setup the UI thread, and set our
    // state to STARTED.
    case State::UNSTARTED: {
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
      // Added in review of bug 1760843, follow up to remove 1767861
      MOZ_RELEASE_ASSERT(hwnd_);
      // Expected to always work but if it doesn't we should still fulfill the
      // contract of always running the process loop at least a single
      // iteration.
      // This could be rexamined in the future.
      if (hwnd_) {
        SetPropW(hwnd_, kThisProperty, this);
        // state_ needs to be STARTED before we request the initial timer
        state_ = State::STARTED;
        if (timeout_) {
          // if someone set the timer before we started
          RequestCallbackTimer(timeout_);
        }
      }
      break;
    }
  };
  return state_ == State::STARTED;
}

bool PlatformUIThread::RequestCallbackTimer(unsigned int milliseconds) {
  CritScope scoped_lock(&cs_);

  switch (state_) {
    // InternalInit() has yet to run so we do not have a UI thread to use as a
    // target of the timer. We should just remember what timer interval was
    // requested and let InternalInit() call this function again when it is
    // ready.
    case State::UNSTARTED: {
      timeout_ = milliseconds;
      return false;
    }
    // We have already stopped, do not schedule a new timer.
    case State::STOPPED:
      return false;
    case State::STARTED: {
      if (timerid_) {
        KillTimer(hwnd_, timerid_);
      }
      timeout_ = milliseconds;
      timerid_ = SetTimer(hwnd_, kTimerId, milliseconds, NULL);
      return !!timerid_;
    }
  }
  // UNREACHABLE
}

void PlatformUIThread::Stop() {
  {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    CritScope scoped_lock(&cs_);
    // Shut down the dispatch loop and let the background thread exit.
    if (timerid_) {
      MOZ_ASSERT(hwnd_);
      KillTimer(hwnd_, timerid_);
      timerid_ = 0;
    }
    switch (state_) {
      // If we haven't started yet there is nothing to do, we will go into
      // the STOPPED state at the end of the function and InternalInit()
      // will not move us to STARTED.
      case State::UNSTARTED:
        break;
      // If we have started, that means that InternalInit() has run and the
      // message wait loop has or will run. We need to signal it to stop. wich
      // will allow PlatformThread::Stop to join that thread.
      case State::STARTED: {
        MOZ_ASSERT(hwnd_);
        PostMessage(hwnd_, WM_CLOSE, 0, 0);
        break;
      }
      // We have already stopped. There is nothing to do.
      case State::STOPPED:
        break;
    }
    // Always set our state to STOPPED
    state_ = State::STOPPED;
  }
  monitor_thread_.Finalize();
}

void PlatformUIThread::Run() {
  // InternalInit() will return false when the thread is already in shutdown.
  // otherwise we must run until we get a Windows WM_QUIT msg.
  const bool runUntilQuitMsg = InternalInit();
  // The interface contract of Start/Stop is that for a successful call to
  // Start, there should be at least one call to the run function.
  NativeEventCallback();
  while (runUntilQuitMsg) {
    // Alertable sleep to receive WM_QUIT (following a WM_CLOSE triggering a
    // WM_DESTROY)
    if (MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT,
                                    MWMO_ALERTABLE | MWMO_INPUTAVAILABLE) ==
        WAIT_OBJECT_0) {
      MSG msg;
      if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
          // THE ONLY WAY to exit the thread loop
          break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }
}

void PlatformUIThread::NativeEventCallback() { native_event_callback_(); }

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

  if (uMsg == WM_TIMER && wParam == kTimerId) {
    twui->NativeEventCallback();
    return 0;
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

}  // namespace rtc

#endif
