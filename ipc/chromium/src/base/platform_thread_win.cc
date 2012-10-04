// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_thread.h"

#include "base/logging.h"
#include "base/win_util.h"

namespace {

// The information on how to set the thread name comes from
// a MSDN article: http://msdn2.microsoft.com/en-us/library/xcb2z8hs.aspx
const DWORD kVCThreadNameException = 0x406D1388;

typedef struct tagTHREADNAME_INFO {
  DWORD dwType;  // Must be 0x1000.
  LPCSTR szName;  // Pointer to name (in user addr space).
  DWORD dwThreadID;  // Thread ID (-1=caller thread).
  DWORD dwFlags;  // Reserved for future use, must be zero.
} THREADNAME_INFO;

DWORD __stdcall ThreadFunc(void* closure) {
  PlatformThread::Delegate* delegate =
      static_cast<PlatformThread::Delegate*>(closure);
  delegate->ThreadMain();
  return 0;
}

}  // namespace

// static
PlatformThreadId PlatformThread::CurrentId() {
  return GetCurrentThreadId();
}

// static
void PlatformThread::YieldCurrentThread() {
  ::Sleep(0);
}

// static
void PlatformThread::Sleep(int duration_ms) {
  ::Sleep(duration_ms);
}

// static
void PlatformThread::SetName(const char* name) {
  // The debugger needs to be around to catch the name in the exception.  If
  // there isn't a debugger, we are just needlessly throwing an exception.
  if (!::IsDebuggerPresent())
    return;

  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = name;
  info.dwThreadID = CurrentId();
  info.dwFlags = 0;

  MOZ_SEH_TRY {
    RaiseException(kVCThreadNameException, 0, sizeof(info)/sizeof(DWORD),
                   reinterpret_cast<DWORD_PTR*>(&info));
  } MOZ_SEH_EXCEPT(EXCEPTION_CONTINUE_EXECUTION) {
  }
}

// static
bool PlatformThread::Create(size_t stack_size, Delegate* delegate,
                            PlatformThreadHandle* thread_handle) {
  unsigned int flags = 0;
  if (stack_size > 0 && win_util::GetWinVersion() >= win_util::WINVERSION_XP) {
    flags = STACK_SIZE_PARAM_IS_A_RESERVATION;
  } else {
    stack_size = 0;
  }

  // Using CreateThread here vs _beginthreadex makes thread creation a bit
  // faster and doesn't require the loader lock to be available.  Our code will
  // have to work running on CreateThread() threads anyway, since we run code
  // on the Windows thread pool, etc.  For some background on the difference:
  //   http://www.microsoft.com/msj/1099/win32/win321099.aspx
  *thread_handle = CreateThread(
      NULL, stack_size, ThreadFunc, delegate, flags, NULL);
  return *thread_handle != NULL;
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  PlatformThreadHandle thread_handle;
  bool result = Create(stack_size, delegate, &thread_handle);
  CloseHandle(thread_handle);
  return result;
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  DCHECK(thread_handle);

  // Wait for the thread to exit.  It should already have terminated but make
  // sure this assumption is valid.
  DWORD result = WaitForSingleObject(thread_handle, INFINITE);
  DCHECK_EQ(WAIT_OBJECT_0, result);

  CloseHandle(thread_handle);
}
