/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VsyncSource.h"
#include "gfxPlatform.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/VsyncDispatcher.h"
#include "MainThreadUtils.h"

using namespace mozilla;
using namespace mozilla::gfx;

void
VsyncSource::AddCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());
  GetGlobalDisplay().AddCompositorVsyncDispatcher(aCompositorVsyncDispatcher);
}

void
VsyncSource::RemoveCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());
  GetGlobalDisplay().RemoveCompositorVsyncDispatcher(aCompositorVsyncDispatcher);
}

VsyncSource::Display&
VsyncSource::FindDisplay(CompositorVsyncDispatcher* aCompositorVsyncDispatcher)
{
  return GetGlobalDisplay();
}

void
VsyncSource::Display::NotifyVsync(TimeStamp aVsyncTimestamp)
{
  // Called on the vsync thread
  for (size_t i = 0; i < mCompositorVsyncDispatchers.Length(); i++) {
    mCompositorVsyncDispatchers[i]->NotifyVsync(aVsyncTimestamp);
  }
}

VsyncSource::Display::Display()
{
  MOZ_ASSERT(NS_IsMainThread());
}

VsyncSource::Display::~Display()
{
  MOZ_ASSERT(NS_IsMainThread());
  mCompositorVsyncDispatchers.Clear();
}

void
VsyncSource::Display::AddCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());
  mCompositorVsyncDispatchers.AppendElement(aCompositorVsyncDispatcher);
}

void
VsyncSource::Display::RemoveCompositorVsyncDispatcher(CompositorVsyncDispatcher* aCompositorVsyncDispatcher)
{
  MOZ_ASSERT(NS_IsMainThread());
  mCompositorVsyncDispatchers.RemoveElement(aCompositorVsyncDispatcher);
}
