/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_QUEUE_OBJECT_H
#define MOZILLA_QUEUE_OBJECT_H

#include "mozilla/RefPtr.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

namespace mozilla {

class AbstractThread;

class QueueObject
{
public:
  explicit QueueObject(RefPtr<AbstractThread> aThread);
  ~QueueObject();
  void Dispatch(nsIRunnable* aRunnable);
  void Dispatch(already_AddRefed<nsIRunnable> aRunnable);
  bool OnThread();
  AbstractThread* Thread();

private:
  RefPtr<AbstractThread> mThread;
};
}

#endif
