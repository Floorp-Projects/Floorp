/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPPlatform_h_
#define GMPPlatform_h_

#include "mozilla/Mutex.h"
#include "gmp-platform.h"
#include "base/thread.h"
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {
namespace gmp {

class GMPChild;

void InitPlatformAPI(GMPPlatformAPI& aPlatformAPI, GMPChild* aChild);

GMPErr RunOnMainThread(GMPTask* aTask);

class GMPThreadImpl : public GMPThread
{
public:
  GMPThreadImpl();
  virtual ~GMPThreadImpl();

  // GMPThread
  void Post(GMPTask* aTask) override;
  void Join() override;

private:
  Mutex mMutex;
  base::Thread mThread;
};

class GMPMutexImpl : public GMPMutex
{
public:
  GMPMutexImpl();
  virtual ~GMPMutexImpl();

  // GMPMutex
  void Acquire() override;
  void Release() override;
  void Destroy() override;

private:
  ReentrantMonitor mMonitor;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPPlatform_h_
