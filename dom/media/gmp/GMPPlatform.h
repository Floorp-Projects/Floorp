/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPPlatform_h_
#define GMPPlatform_h_

#include "gmp-platform.h"
#include <functional>

namespace mozilla {
namespace gmp {

class GMPChild;

void InitPlatformAPI(GMPPlatformAPI& aPlatformAPI, GMPChild* aChild);

GMPErr RunOnMainThread(GMPTask* aTask);

GMPTask*
NewGMPTask(std::function<void()>&& aFunction);

GMPErr
SetTimerOnMainThread(GMPTask* aTask, int64_t aTimeoutMS);

} // namespace gmp
} // namespace mozilla

#endif // GMPPlatform_h_
