/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "QueueObject.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/TaskQueue.h"
#include "nsThreadUtils.h"

namespace mozilla {

QueueObject::QueueObject(RefPtr<AbstractThread> aThread) : mThread(aThread) {}

QueueObject::~QueueObject() {}

void
QueueObject::Dispatch(nsIRunnable* aRunnable)
{
  Dispatch(do_AddRef(aRunnable));
}

void
QueueObject::Dispatch(already_AddRefed<nsIRunnable> aRunnable)
{
  mThread->Dispatch(std::move(aRunnable));
}

bool
QueueObject::OnThread()
{
  return mThread->IsCurrentThreadIn();
}

AbstractThread*
QueueObject::Thread()
{
  return mThread;
}
}
