/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPPlatform_h_
#define GMPPlatform_h_

#include "gmp-platform.h"
#include <functional>

namespace mozilla::ipc {
class ByteBuf;
}  // namespace mozilla::ipc

namespace mozilla::gmp {

class GMPChild;

void InitPlatformAPI(GMPPlatformAPI& aPlatformAPI, GMPChild* aChild);

GMPErr RunOnMainThread(GMPTask* aTask);

GMPTask* NewGMPTask(std::function<void()>&& aFunction);

GMPErr SetTimerOnMainThread(GMPTask* aTask, int64_t aTimeoutMS);

void SendFOGData(ipc::ByteBuf&& buf);

}  // namespace mozilla::gmp

#endif  // GMPPlatform_h_
