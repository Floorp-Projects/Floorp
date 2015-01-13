/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncSource.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"
#include "mozilla/VsyncDispatcher.h"
#include "MainThreadUtils.h"

namespace mozilla {
namespace gfx {

void
VsyncSource::AddCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  // Just use the global display until we have enough information to get the
  // corresponding display for compositor.
  GetGlobalDisplay().AddCompositorVsyncDispatcher(aCompositorVsyncDispatcher);
}

void
VsyncSource::RemoveCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher)
{
  MOZ_ASSERT(XRE_IsParentProcess());
  MOZ_ASSERT(NS_IsMainThread());
  // See also AddCompositorVsyncDispatcher().
  GetGlobalDisplay().RemoveCompositorVsyncDispatcher(aCompositorVsyncDispatcher);
}

nsRefPtr<RefreshTimerVsyncDispatcher>
VsyncSource::GetRefreshTimerVsyncDispatcher()
{
  MOZ_ASSERT(XRE_IsParentProcess());
  // See also AddCompositorVsyncDispatcher().
  return GetGlobalDisplay().GetRefreshTimerVsyncDispatcher();
}

VsyncSource::Display::Display()
  : mDispatcherLock("display dispatcher lock")
{
  MOZ_ASSERT(NS_IsMainThread());
  mRefreshTimerVsyncDispatcher = new RefreshTimerVsyncDispatcher();
}

VsyncSource::Display::~Display()
{
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mDispatcherLock);
  mRefreshTimerVsyncDispatcher = nullptr;
  mCompositorVsyncDispatchers.Clear();
}

void
VsyncSource::Display::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // Called on the vsync thread
  MutexAutoLock lock(mDispatcherLock);

  for (size_t i = 0; i < mCompositorVsyncDispatchers.Length(); i++) {
    mCompositorVsyncDispatchers[i]->NotifyVsync(aVsyncTimestamp);
  }

  mRefreshTimerVsyncDispatcher->NotifyVsync(aVsyncTimestamp);
}

void
VsyncSource::Display::AddCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCompositorVsyncDispatcher);
  MutexAutoLock lock(mDispatcherLock);
  if (!mCompositorVsyncDispatchers.Contains(aCompositorVsyncDispatcher)) {
    mCompositorVsyncDispatchers.AppendElement(aCompositorVsyncDispatcher);
  }
}

void
VsyncSource::Display::RemoveCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCompositorVsyncDispatcher);
  MutexAutoLock lock(mDispatcherLock);
  mCompositorVsyncDispatchers.RemoveElement(aCompositorVsyncDispatcher);
}

nsRefPtr<RefreshTimerVsyncDispatcher>
VsyncSource::Display::GetRefreshTimerVsyncDispatcher()
{
  return mRefreshTimerVsyncDispatcher;
}

} //namespace gfx
} //namespace mozilla
