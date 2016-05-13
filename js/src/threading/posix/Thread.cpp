/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include <new>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <dlfcn.h>
#endif

#if defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <pthread_np.h>
#endif

#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include "threading/Thread.h"

class js::Thread::Id::PlatformData
{
  friend class js::Thread;
  friend js::Thread::Id js::ThisThread::GetId();

  pthread_t ptThread;

  // pthread_t does not have a default initializer, so we have to carry a bool
  // to tell whether it is safe to compare or not.
  bool hasThread;
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
  platformData()->hasThread = false;
}

bool
js::Thread::Id::operator==(const Id& aOther)
{
  const PlatformData& self = *platformData();
  const PlatformData& other = *aOther.platformData();
  return (!self.hasThread && !other.hasThread) ||
         (self.hasThread == other.hasThread &&
          pthread_equal(self.ptThread, other.ptThread));
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
js::Thread::create(void* (*aMain)(void*), void* aArg)
{
  int r = pthread_create(&id_.platformData()->ptThread, nullptr, aMain, aArg);
  MOZ_RELEASE_ASSERT(!r);
  id_.platformData()->hasThread = true;
}

void
js::Thread::join()
{
  MOZ_RELEASE_ASSERT(joinable());
  int r = pthread_join(id_.platformData()->ptThread, nullptr);
  MOZ_RELEASE_ASSERT(!r);
  id_ = Id();
}

void
js::Thread::detach()
{
  MOZ_RELEASE_ASSERT(joinable());
  int r = pthread_detach(id_.platformData()->ptThread);
  MOZ_RELEASE_ASSERT(!r);
  id_ = Id();
}

js::Thread::Id
js::ThisThread::GetId()
{
  js::Thread::Id id;
  id.platformData()->ptThread = pthread_self();
  id.platformData()->hasThread = true;
  return id;
}

void
js::ThisThread::SetName(const char* name)
{
  MOZ_RELEASE_ASSERT(name);

#if (defined(__APPLE__) && defined(__MACH__)) || defined(__linux__)
  // On linux and OS X the name may not be longer than 16 bytes, including
  // the null terminator. Truncate the name to 15 characters.
  char nameBuf[16];

  strncpy(nameBuf, name, sizeof nameBuf - 1);
  nameBuf[sizeof nameBuf - 1] = '\0';
  name = nameBuf;
#endif

  int rv;
#ifdef XP_DARWIN
  rv = pthread_setname_np(name);
#elif defined(__DragonFly__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  pthread_set_name_np(pthread_self(), name);
  rv = 0;
#elif defined(__NetBSD__)
  rv = pthread_setname_np(pthread_self(), "%s", (void*)name);
#else
  rv = pthread_setname_np(pthread_self(), name);
#endif
  MOZ_RELEASE_ASSERT(!rv);
}
