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
static TimeStamp sStartTime;
static bool sFinishedVRListenerShutDown = false;

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
  sStartTime = TimeStamp::Now();
}

TimeStamp
VRListenerThreadHolder::GetStartTime()
{
  return sStartTime;
}

void
VRListenerThreadHolder::Shutdown()
{
  if (!IsActive()) {
    return;
  }

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

} // namespace gfx
} // namespace mozilla

bool
NS_IsInVRListenerThread()
{
  return mozilla::gfx::VRListenerThreadHolder::IsInVRListenerThread();
}