/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChildAsyncCall.h"
#include "PluginInstanceChild.h"

namespace mozilla {
namespace plugins {

ChildAsyncCall::ChildAsyncCall(PluginInstanceChild* instance,
                               PluginThreadCallback aFunc, void* aUserData)
  : mInstance(instance)
  , mFunc(aFunc)
  , mData(aUserData)
{
}

void
ChildAsyncCall::Cancel()
{
  mInstance = nullptr;
  mFunc = nullptr;
  mData = nullptr;
}

void
ChildAsyncCall::RemoveFromAsyncList()
{
  if (mInstance) {
    MutexAutoLock lock(mInstance->mAsyncCallMutex);
    mInstance->mPendingAsyncCalls.RemoveElement(this);
  }
}

void
ChildAsyncCall::Run()
{
  RemoveFromAsyncList();

  if (mFunc)
    mFunc(mData);
}

} // namespace plugins
} // namespace mozilla
