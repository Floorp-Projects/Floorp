/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WidevineAdapter_h_
#define WidevineAdapter_h_

#include "GMPLoader.h"
#include "prlink.h"
#include "GMPUtils.h"

struct GMPPlatformAPI;

namespace mozilla {

class WidevineAdapter : public gmp::GMPAdapter {
public:

  void SetAdaptee(PRLibrary* aLib) override;

  // These are called in place of the corresponding GMP API functions.
  GMPErr GMPInit(const GMPPlatformAPI* aPlatformAPI) override;
  GMPErr GMPGetAPI(const char* aAPIName,
                   void* aHostAPI,
                   void** aPluginAPI,
                   uint32_t aDecryptorId) override;
  void GMPShutdown() override;
  void GMPSetNodeId(const char* aNodeId, uint32_t aLength) override;

  static bool Supports(int32_t aModuleVersion,
                       int32_t aInterfaceVersion,
                       int32_t aHostVersion);

private:
  PRLibrary* mLib = nullptr;
};

GMPErr GMPCreateThread(GMPThread** aThread);
GMPErr GMPRunOnMainThread(GMPTask* aTask);
GMPErr GMPCreateMutex(GMPMutex** aMutex);

// Call on main thread only.
GMPErr GMPCreateRecord(const char* aRecordName,
                       uint32_t aRecordNameSize,
                       GMPRecord** aOutRecord,
                       GMPRecordClient* aClient);

// Call on main thread only.
GMPErr GMPSetTimerOnMainThread(GMPTask* aTask, int64_t aTimeoutMS);

GMPErr GMPGetCurrentTime(GMPTimestamp* aOutTime);

GMPErr GMPCreateRecordIterator(RecvGMPRecordIteratorPtr aRecvIteratorFunc,
                               void* aUserArg);

} // namespace mozilla

#endif // WidevineAdapter_h_
