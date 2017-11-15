/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRThread.h"
#include "nsThreadUtils.h"

namespace mozilla {

namespace gfx {

static StaticRefPtr<VRListenerThreadHolder> sVRListenerThreadHolder;
static bool sFinishedVRListenerShutDown = false;
static const uint32_t kDefaultThreadLifeTime = 60; // in 60 seconds.
static const uint32_t kDelayPostTaskTime = 20000; // in 20000 ms.

VRListenerThreadHolder* GetVRListenerThreadHolder()
{
  return sVRListenerThreadHolder;
}

base::Thread*
VRListenerThread()
{
  return sVRListenerThreadHolder
         ? sVRListenerThreadHolder->GetThread()
         : nullptr;
}

/* static */ MessageLoop*
VRListenerThreadHolder::Loop()
{
  return VRListenerThread() ? VRListenerThread()->message_loop() : nullptr;
}

VRListenerThreadHolder*
VRListenerThreadHolder::GetSingleton()
{
  return sVRListenerThreadHolder;
}

VRListenerThreadHolder::VRListenerThreadHolder()
 : mThread(CreateThread())
{
  MOZ_ASSERT(NS_IsMainThread());
}

VRListenerThreadHolder::~VRListenerThreadHolder()
{
  MOZ_ASSERT(NS_IsMainThread());
  DestroyThread(mThread);
}

/* static */ void
VRListenerThreadHolder::DestroyThread(base::Thread* aThread)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRListenerThreadHolder,
             "We shouldn't be destroying the VR listener thread yet.");
  delete aThread;
  sFinishedVRListenerShutDown = true;
}

/* static */ base::Thread*
VRListenerThreadHolder::CreateThread()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!sVRListenerThreadHolder, "The VR listener thread has already been started!");

  base::Thread* vrThread = new base::Thread("VRListener");
  base::Thread::Options options;
  /* Timeout values are powers-of-two to enable us get better data.
     128ms is chosen for transient hangs because 8Hz should be the minimally
     acceptable goal for Compositor responsiveness (normal goal is 60Hz). */
  options.transient_hang_timeout = 128; // milliseconds
  /* 2048ms is chosen for permanent hangs because it's longer than most
   * Compositor hangs seen in the wild, but is short enough to not miss getting
   * native hang stacks. */
  options.permanent_hang_timeout = 2048; // milliseconds

  if (!vrThread->StartWithOptions(options)) {
    delete vrThread;
    return nullptr;
  }

  return vrThread;
}

void
VRListenerThreadHolder::Start()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread!");
  MOZ_ASSERT(!sVRListenerThreadHolder, "The VR listener thread has already been started!");

  sVRListenerThreadHolder = new VRListenerThreadHolder();
}

void
VRListenerThreadHolder::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should be on the main thread!");
  MOZ_ASSERT(sVRListenerThreadHolder, "The VR listener thread has already been shut down!");

  sVRListenerThreadHolder = nullptr;

  SpinEventLoopUntil([&]() { return sFinishedVRListenerShutDown; });
}

/* static */ bool
VRListenerThreadHolder::IsInVRListenerThread()
{
  return VRListenerThread() &&
		 VRListenerThread()->thread_id() == PlatformThread::CurrentId();
}

VRThread::VRThread(const nsCString& aName)
 : mThread(nullptr)
 , mLifeTime(kDefaultThreadLifeTime)
 , mStarted(false)
{
  mName = aName;
}

VRThread::~VRThread()
{
  Shutdown();
}

void
VRThread::Start()
{
  MOZ_ASSERT(VRListenerThreadHolder::IsInVRListenerThread());

  if (!mThread) {
    nsresult rv = NS_NewNamedThread(mName, getter_AddRefs(mThread));
    MOZ_ASSERT(mThread);

    if (NS_FAILED(rv)) {
      MOZ_ASSERT(false, "Failed to create a vr thread.");
    }
    RefPtr<Runnable> runnable =
      NewRunnableMethod<TimeStamp>(
        "gfx::VRThread::CheckLife", this, &VRThread::CheckLife, TimeStamp::Now());
    // Post it to the main thread for tracking the lifetime.
    nsCOMPtr<nsIThread> mainThread;
    rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_FAILED(rv)) {
      NS_WARNING("VRThread::Start() could not get Main thread");
      return;
    }
    mainThread->DelayedDispatch(runnable.forget(), kDelayPostTaskTime);
  }
  mStarted = true;
  mLastActiveTime = TimeStamp::Now();
}

void
VRThread::Shutdown()
{
  if (mThread) {
    mThread->Shutdown();
    mThread = nullptr;
  }
  mStarted = false;
}

const nsCOMPtr<nsIThread>
VRThread::GetThread() const
{
  return mThread;
}

void
VRThread::PostTask(already_AddRefed<Runnable> aTask)
{
  PostDelayedTask(Move(aTask), 0);
}

void
VRThread::PostDelayedTask(already_AddRefed<Runnable> aTask,
                          uint32_t aTime)
{
  MOZ_ASSERT(mStarted, "Must call Start() before posting tasks.");
  MOZ_ASSERT(mThread);
  mLastActiveTime = TimeStamp::Now();

  if (!aTime) {
    mThread->Dispatch(Move(aTask), NS_DISPATCH_NORMAL);
  } else {
    mThread->DelayedDispatch(Move(aTask), aTime);
  }
}

void
VRThread::CheckLife(TimeStamp aCheckTimestamp)
{
  // VR system is going to shutdown.
  if (!mStarted) {
    Shutdown();
    return;
  }

  const TimeDuration timeout = TimeDuration::FromSeconds(mLifeTime);
  if ((aCheckTimestamp - mLastActiveTime) > timeout) {
    Shutdown();
  } else {
    RefPtr<Runnable> runnable =
      NewRunnableMethod<TimeStamp>(
        "gfx::VRThread::CheckLife", this, &VRThread::CheckLife, TimeStamp::Now());
    // Post it to the main thread for tracking the lifetime.
    nsCOMPtr<nsIThread> mainThread;
    nsresult rv = NS_GetMainThread(getter_AddRefs(mainThread));
    if (NS_FAILED(rv)) {
      NS_WARNING("VRThread::CheckLife() could not get Main thread");
      return;
    }
    mainThread->DelayedDispatch(runnable.forget(), kDelayPostTaskTime);
  }
}

void
VRThread::SetLifeTime(uint32_t aLifeTime)
{
  mLifeTime = aLifeTime;
}

uint32_t
VRThread::GetLifeTime()
{
  return mLifeTime;
}

bool
VRThread::IsActive()
{
  return !!mThread;
}

} // namespace gfx
} // namespace mozilla
