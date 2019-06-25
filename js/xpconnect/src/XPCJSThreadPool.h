/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_JSThreadPool_h
#define mozilla_JSThreadPool_h

#include "nsIThreadPool.h"
#include "js/Utility.h"

namespace mozilla {
/*
 * Since XPCOM thread pools dispatch nsIRunnable*, we want a way to take the
 * specific job that JS wants to dispatch offthread, and run it via an
 * nsIRunnable(). RunnableTasks should self-destroy.
 */
class HelperThreadTaskHandler : public Runnable {
 public:
  NS_IMETHOD Run() override {
    mOffThreadTask->runTask();
    mOffThreadTask = nullptr;
    return NS_OK;
  }
  explicit HelperThreadTaskHandler(RunnableTask* task)
      : Runnable("HelperThreadTaskHandler"), mOffThreadTask(task) {}

 private:
  ~HelperThreadTaskHandler() = default;
  RunnableTask* mOffThreadTask;
};

/*
 * HelperThreadPool is a thread pool infrastructure for JS offthread tasks. It
 * holds the actual instance of a thread pool that we instantiate. The intention
 * is to have a way to manage some of the things GlobalHelperThreadState does
 * independently of nsThreadPool.
 */
class HelperThreadPool {
 public:
  nsresult Dispatch(already_AddRefed<HelperThreadTaskHandler> aRunnable);
  void Shutdown();
  HelperThreadPool();
  virtual ~HelperThreadPool() = default;

 private:
  RefPtr<nsIThreadPool> mPool;
};

}  // namespace mozilla

#endif
