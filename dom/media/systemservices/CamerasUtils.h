/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CameraUtils_h
#define mozilla_CameraUtils_h

#include "nsThreadUtils.h"
#include "nsCOMPtr.h"
#include "mozilla/UniquePtr.h"

#include "base/thread.h"

namespace mozilla {
namespace camera {

nsresult SynchronouslyCreatePBackground();

class ThreadDestructor : public nsRunnable
{
  DISALLOW_COPY_AND_ASSIGN(ThreadDestructor);

public:
  explicit ThreadDestructor(nsIThread* aThread)
    : mThread(aThread) {}

  NS_IMETHOD Run() override
  {
    if (mThread) {
      mThread->Shutdown();
    }
    return NS_OK;
  }

private:
  ~ThreadDestructor() {}
  nsCOMPtr<nsIThread> mThread;
};

class RunnableTask : public Task
{
public:
  explicit RunnableTask(nsRunnable* aRunnable)
    : mRunnable(aRunnable) {}

  void Run() override {
    mRunnable->Run();
  }

private:
  ~RunnableTask() {}
  nsRefPtr<nsRunnable> mRunnable;
};

}
}

#endif // mozilla_CameraUtils_h
