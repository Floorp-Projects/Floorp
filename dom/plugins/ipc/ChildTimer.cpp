/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildTimer.h"
#include "PluginInstanceChild.h"
#include "nsComponentManagerUtils.h"

namespace mozilla {
namespace plugins {

ChildTimer::ChildTimer(PluginInstanceChild* instance,
                       uint32_t interval,
                       bool repeat,
                       TimerFunc func)
  : mInstance(instance)
  , mFunc(func)
  , mRepeating(repeat)
  , mID(gNextTimerID++)
{
  mTimer.Start(base::TimeDelta::FromMilliseconds(interval),
               this, &ChildTimer::Run);
}

uint32_t
ChildTimer::gNextTimerID = 1;

void
ChildTimer::Run()
{
  if (!mRepeating)
    mTimer.Stop();
  mFunc(mInstance->GetNPP(), mID);
}

} // namespace plugins
} // namespace mozilla
