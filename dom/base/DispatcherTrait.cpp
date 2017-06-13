/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DispatcherTrait.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/SchedulerGroup.h"
#include "nsINamed.h"

using namespace mozilla;
using namespace mozilla::dom;

nsresult
DispatcherTrait::Dispatch(const char* aName,
                          TaskCategory aCategory,
                          already_AddRefed<nsIRunnable>&& aRunnable)
{
  return SchedulerGroup::UnlabeledDispatch(aName, aCategory, Move(aRunnable));
}

nsIEventTarget*
DispatcherTrait::EventTargetFor(TaskCategory aCategory) const
{
  return GetMainThreadEventTarget();
}

AbstractThread*
DispatcherTrait::AbstractMainThreadFor(TaskCategory aCategory)
{
  // Return non DocGroup version by default.
  return AbstractThread::MainThread();
}
