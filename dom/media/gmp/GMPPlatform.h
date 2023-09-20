/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPPlatform_h_
#define GMPPlatform_h_

#include "mozilla/RefPtr.h"
#include "gmp-platform.h"
#include <functional>
#include "mozilla/gmp/PGMPChild.h"

namespace mozilla {
#ifdef XP_WIN
struct ModulePaths;
#endif

namespace ipc {
class ByteBuf;
}  // namespace ipc

namespace gmp {

class GMPChild;

void InitPlatformAPI(GMPPlatformAPI& aPlatformAPI, GMPChild* aChild);

GMPErr RunOnMainThread(GMPTask* aTask);

GMPTask* NewGMPTask(std::function<void()>&& aFunction);

GMPErr SetTimerOnMainThread(GMPTask* aTask, int64_t aTimeoutMS);

void SendFOGData(ipc::ByteBuf&& buf);

#ifdef XP_WIN
RefPtr<PGMPChild::GetModulesTrustPromise> SendGetModulesTrust(
    ModulePaths&& aModules, bool aRunNormal);
#endif

}  // namespace gmp
}  // namespace mozilla

#endif  // GMPPlatform_h_
