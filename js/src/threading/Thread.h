/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef threading_Thread_h
#define threading_Thread_h

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/IndexSequence.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Tuple.h"

#include <stdint.h>

#ifdef XP_WIN
# define THREAD_RETURN_TYPE unsigned int
# define THREAD_CALL_API __stdcall
#else
# define THREAD_RETURN_TYPE void*
# define THREAD_CALL_API
#endif

namespace js {
namespace detail {
template <typename F, typename... Args>
class ThreadTrampoline;
} // namespace detail

// Execute the given functor concurrent with the currently executing instruction
// stream and within the current address space. Use with care.
class Thread
{
public:
  class Id
  {
    class PlatformData;
    void* platformData_[2];

  public:
    Id();

    Id(const Id&) = default;
    Id(Id&&) = default;
    Id& operator=(const Id&) = default;
    Id& operator=(Id&&) = default;

    bool operator==(const Id& aOther);
    bool operator!=(const Id& aOther) { return !operator==(aOther); }

    inline PlatformData* platformData();
    inline const PlatformData* platformData() const;
  };

  // Create a Thread in an initially unjoinable state. A thread of execution can
  // be created for this Thread by calling |init|.
  Thread() : id_(Id()) {}

  // Start a thread of execution at functor |f| with parameters |args|.
  template <typename F, typename... Args>
  explicit Thread(F&& f, Args&&... args) {
    MOZ_RELEASE_ASSERT(init(mozilla::Forward<F>(f),
                            mozilla::Forward<Args>(args)...));
  }

  // Start a thread of execution at functor |f| with parameters |args|. This
  // method will return false if thread creation fails. This Thread must not
  // already have been created.
  template <typename F, typename... Args>
  MOZ_MUST_USE bool init(F&& f, Args&&... args) {
    MOZ_RELEASE_ASSERT(!joinable());
    using Trampoline = detail::ThreadTrampoline<F, Args...>;
    auto trampoline = new Trampoline(mozilla::Forward<F>(f),
                                     mozilla::Forward<Args>(args)...);
    MOZ_RELEASE_ASSERT(trampoline);
    return create(Trampoline::Start, trampoline);
  }

  // The thread must be joined or detached before destruction.
  ~Thread() {
    MOZ_RELEASE_ASSERT(!joinable());
  }

  // Move the thread into the detached state without blocking. In the detatched
  // state, the thread continues to run until it exits, but cannot be joined.
  // After this method returns, this Thread no longer represents a thread of
  // execution. When the thread exits, its resources will be cleaned up by the
  // system. At process exit, if the thread is still running, the thread's TLS
  // storage will be destructed, but the thread stack will *not* be unrolled.
  void detach();

  // Block the current thread until this Thread returns from the functor it was
  // created with. The thread's resources will be cleaned up before this
  // function returns. After this method returns, this Thread no longer
  // represents a thread of execution.
  void join();

  // Return true if this thread has not yet been joined or detached. If this
  // method returns false, this Thread does not have an associated thread of
  // execution, for example, if it has been previously moved or joined.
  bool joinable() const {
    return get_id() != Id();
  }

  // Returns the id of this thread if this represents a thread of execution or
  // the default constructed Id() if not. The thread ID is guaranteed to
  // uniquely identify a thread and can be compared with the == operator.
  Id get_id() const { return id_; }

  // Allow threads to be moved so that they can be stored in containers.
  Thread(Thread&& aOther);
  Thread& operator=(Thread&& aOther);

private:
  // Disallow copy as that's not sensible for unique resources.
  Thread(const Thread&) = delete;
  void operator=(const Thread&) = delete;

  // Provide a process global ID to each thread.
  Id id_;

  // Dispatch to per-platform implementation of thread creation.
  MOZ_MUST_USE bool create(THREAD_RETURN_TYPE (THREAD_CALL_API *aMain)(void*), void* aArg);
};

namespace ThisThread {

// Return the thread id of the calling thread.
Thread::Id GetId();

// Set the current thread name. Returns true if successful. Note that setting
// the thread name may not be available on all platforms; on these platforms
// setName() will simply do nothing.
void SetName(const char* name);

} // namespace ThisThread

namespace detail {

// Platform thread APIs allow passing a single void* argument to the target
// thread. This class is responsible for safely ferrying the arg pack and
// functor across that void* membrane and running it in the other thread.
template <typename F, typename... Args>
class ThreadTrampoline
{
  F f;
  mozilla::Tuple<Args...> args;

public:
  explicit ThreadTrampoline(F&& aF, Args&&... aArgs)
    : f(mozilla::Forward<F>(aF)),
      args(mozilla::Forward<Args>(aArgs)...)
  {
  }

  static THREAD_RETURN_TYPE THREAD_CALL_API Start(void* aPack) {
    auto* pack = static_cast<ThreadTrampoline<F, Args...>*>(aPack);
    pack->callMain(typename mozilla::IndexSequenceFor<Args...>::Type());
    delete pack;
    return 0;
  }

  template<size_t ...Indices>
  void callMain(mozilla::IndexSequence<Indices...>) {
    f(mozilla::Get<Indices>(args)...);
  }
};

} // namespace detail
} // namespace js

#undef THREAD_RETURN_TYPE

#endif // threading_Thread_h
