/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerHolder.h"
#include "WorkerPrivate.h"

BEGIN_WORKERS_NAMESPACE

WorkerHolder::WorkerHolder()
  : mWorkerPrivate(nullptr)
{
}

WorkerHolder::~WorkerHolder()
{
  ReleaseWorkerInternal();
  MOZ_ASSERT(mWorkerPrivate == nullptr);
}

bool
WorkerHolder::HoldWorker(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  if (!aWorkerPrivate->AddHolder(this)) {
    return false;
  }

  mWorkerPrivate = aWorkerPrivate;
  return true;
}

void
WorkerHolder::ReleaseWorker()
{
  MOZ_ASSERT(mWorkerPrivate);
  ReleaseWorkerInternal();
}

void
WorkerHolder::ReleaseWorkerInternal()
{
  if (mWorkerPrivate) {
    mWorkerPrivate->AssertIsOnWorkerThread();
    mWorkerPrivate->RemoveHolder(this);
    mWorkerPrivate = nullptr;
  }
}

END_WORKERS_NAMESPACE
