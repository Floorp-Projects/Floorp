/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_thread.h"

#include <errno.h>
#include <sched.h>

#if defined(OS_MACOSX)
#include <mach/mach.h>
#elif defined(OS_NETBSD)
#include <lwp.h>
#elif defined(OS_LINUX)
#include <sys/syscall.h>
#include <sys/prctl.h>
#endif

#if !defined(OS_MACOSX)
#include <unistd.h>
#endif

#if defined(OS_BSD) && !defined(OS_NETBSD) && !defined(__GLIBC__)
#include <pthread_np.h>
#endif

#if defined(OS_MACOSX)
namespace base {
void InitThreading();
}  // namespace
#endif

static void* ThreadFunc(void* closure) {
  PlatformThread::Delegate* delegate =
      static_cast<PlatformThread::Delegate*>(closure);
  delegate->ThreadMain();
  return NULL;
}

// static
PlatformThreadId PlatformThread::CurrentId() {
  // Pthreads doesn't have the concept of a thread ID, so we have to reach down
  // into the kernel.
#if defined(OS_MACOSX)
  mach_port_t port = mach_thread_self();
  mach_port_deallocate(mach_task_self(), port);
  return port;
#elif defined(OS_LINUX)
#ifdef MOZ_WIDGET_GONK
  return (intptr_t) (pthread_self());
#else
  return syscall(__NR_gettid);
#endif
#elif defined(OS_OPENBSD) || defined(__GLIBC__)
  return (intptr_t) (pthread_self());
#elif defined(OS_NETBSD)
  return _lwp_self();
#elif defined(OS_DRAGONFLY)
  return lwp_gettid();
#elif defined(OS_FREEBSD)
  return pthread_getthreadid_np();
#endif
}

// static
void PlatformThread::YieldCurrentThread() {
  sched_yield();
}

// static
void PlatformThread::Sleep(int duration_ms) {
  struct timespec sleep_time, remaining;

  // Contains the portion of duration_ms >= 1 sec.
  sleep_time.tv_sec = duration_ms / 1000;
  duration_ms -= sleep_time.tv_sec * 1000;

  // Contains the portion of duration_ms < 1 sec.
  sleep_time.tv_nsec = duration_ms * 1000 * 1000;  // nanoseconds.

  while (nanosleep(&sleep_time, &remaining) == -1 && errno == EINTR)
    sleep_time = remaining;
}

#ifndef OS_MACOSX
// Mac is implemented in platform_thread_mac.mm.

// static
void PlatformThread::SetName(const char* name) {
  // On linux we can get the thread names to show up in the debugger by setting
  // the process name for the LWP.  We don't want to do this for the main
  // thread because that would rename the process, causing tools like killall
  // to stop working.
  if (PlatformThread::CurrentId() == getpid())
    return;

  // http://0pointer.de/blog/projects/name-your-threads.html
  // Set the name for the LWP (which gets truncated to 15 characters).
  // Note that glibc also has a 'pthread_setname_np' api, but it may not be
  // available everywhere and it's only benefit over using prctl directly is
  // that it can set the name of threads other than the current thread.
#if defined(OS_LINUX)
  prctl(PR_SET_NAME, reinterpret_cast<uintptr_t>(name), 0, 0, 0); 
#elif defined(OS_NETBSD)
  pthread_setname_np(pthread_self(), "%s", (void *)name);
#elif defined(OS_BSD) && !defined(__GLIBC__)
  pthread_set_name_np(pthread_self(), name);
#else
#endif
}
#endif // !OS_MACOSX

namespace {

bool CreateThread(size_t stack_size, bool joinable,
                  PlatformThread::Delegate* delegate,
                  PlatformThreadHandle* thread_handle) {
#if defined(OS_MACOSX)
  base::InitThreading();
#endif  // OS_MACOSX

  bool success = false;
  pthread_attr_t attributes;
  pthread_attr_init(&attributes);

  // Pthreads are joinable by default, so only specify the detached attribute if
  // the thread should be non-joinable.
  if (!joinable) {
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
  }

  if (stack_size > 0)
    pthread_attr_setstacksize(&attributes, stack_size);

  success = !pthread_create(thread_handle, &attributes, ThreadFunc, delegate);

  pthread_attr_destroy(&attributes);
  return success;
}

}  // anonymous namespace

// static
bool PlatformThread::Create(size_t stack_size, Delegate* delegate,
                            PlatformThreadHandle* thread_handle) {
  return CreateThread(stack_size, true /* joinable thread */,
                      delegate, thread_handle);
}

// static
bool PlatformThread::CreateNonJoinable(size_t stack_size, Delegate* delegate) {
  PlatformThreadHandle unused;

  bool result = CreateThread(stack_size, false /* non-joinable thread */,
                             delegate, &unused);
  return result;
}

// static
void PlatformThread::Join(PlatformThreadHandle thread_handle) {
  pthread_join(thread_handle, NULL);
}
