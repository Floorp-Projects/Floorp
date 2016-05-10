/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <new.h>
#include <process.h>

#include <windows.h>

#include "threading/Thread.h"

class js::Thread::Id::PlatformData
{
  friend class js::Thread;
  friend js::Thread::Id js::ThisThread::GetId();

  HANDLE handle;
  unsigned id;
};

inline js::Thread::Id::PlatformData*
js::Thread::Id::platformData()
{
  static_assert(sizeof platformData_ >= sizeof(PlatformData),
                "platformData_ is too small");
  return reinterpret_cast<PlatformData*>(platformData_);
}

inline const js::Thread::Id::PlatformData*
js::Thread::Id::platformData() const
{
  static_assert(sizeof platformData_ >= sizeof(PlatformData),
                "platformData_ is too small");
  return reinterpret_cast<const PlatformData*>(platformData_);
}

js::Thread::Id::Id()
{
  platformData()->handle = nullptr;
  platformData()->id = 0;
}

bool
js::Thread::Id::operator==(const Id& aOther)
{
  return platformData()->id == aOther.platformData()->id;
}

js::Thread::Thread(Thread&& aOther)
{
  id_ = aOther.id_;
  aOther.id_ = Id();
}

js::Thread&
js::Thread::operator=(Thread&& aOther)
{
  MOZ_RELEASE_ASSERT(!joinable());
  id_ = aOther.id_;
  aOther.id_ = Id();
  return *this;
}

void
js::Thread::create(unsigned int (__stdcall* aMain)(void*), void* aArg)
{
  // Use _beginthreadex and not CreateThread, because threads that are
  // created with the latter leak a small amount of memory when they use
  // certain msvcrt functions and then exit.
  uintptr_t handle = _beginthreadex(nullptr, 0, aMain, aArg, 0,
                                    &id_.platformData()->id);
  MOZ_RELEASE_ASSERT(handle != 0);
  id_.platformData()->handle = reinterpret_cast<HANDLE>(handle);
}

void
js::Thread::join()
{
  MOZ_RELEASE_ASSERT(joinable());
  DWORD r = WaitForSingleObject(id_.platformData()->handle, INFINITE);
  MOZ_RELEASE_ASSERT(r == WAIT_OBJECT_0);
  BOOL success = CloseHandle(id_.platformData()->handle);
  MOZ_RELEASE_ASSERT(success);
  id_ = Id();
}

void
js::Thread::detach()
{
  MOZ_RELEASE_ASSERT(joinable());
  BOOL success = CloseHandle(id_.platformData()->handle);
  MOZ_RELEASE_ASSERT(success);
  id_ = Id();
}

js::Thread::Id
js::ThisThread::GetId()
{
  js::Thread::Id id;
  id.platformData()->handle = GetCurrentThread();
  id.platformData()->id = GetCurrentThreadId();
  MOZ_RELEASE_ASSERT(id != js::Thread::Id());
  return id;
}

void
js::ThisThread::SetName(const char* name)
{
  MOZ_RELEASE_ASSERT(name);

#ifdef _MSC_VER
  // Setting the thread name requires compiler support for structured
  // exceptions, so this only works when compiled with MSVC.
  static const DWORD THREAD_NAME_EXCEPTION = 0x406D1388;
  static const DWORD THREAD_NAME_INFO_TYPE = 0x1000;

#pragma pack(push, 8)
  struct THREADNAME_INFO
  {
    DWORD dwType;
    LPCSTR szName;
    DWORD dwThreadID;
    DWORD dwFlags;
  };
#pragma pack(pop)

  THREADNAME_INFO info;
  info.dwType = THREAD_NAME_INFO_TYPE;
  info.szName = name;
  info.dwThreadID = GetCurrentThreadId();
  info.dwFlags = 0;

  __try {
    RaiseException(THREAD_NAME_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                   (ULONG_PTR*)&info);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    // Do nothing.
  }
#endif
}
