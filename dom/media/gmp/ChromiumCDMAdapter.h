/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumAdapter_h_
#define ChromiumAdapter_h_

#include "GMPLoader.h"
#include "prlink.h"
#include "GMPUtils.h"

struct GMPPlatformAPI;

namespace mozilla {

class ChromiumCDMAdapter : public gmp::GMPAdapter
{
public:

  void SetAdaptee(PRLibrary* aLib) override;

  // These are called in place of the corresponding GMP API functions.
  GMPErr GMPInit(const GMPPlatformAPI* aPlatformAPI) override;
  GMPErr GMPGetAPI(const char* aAPIName,
                   void* aHostAPI,
                   void** aPluginAPI,
                   uint32_t aDecryptorId) override;
  void GMPShutdown() override;

  static bool Supports(int32_t aModuleVersion,
                       int32_t aInterfaceVersion,
                       int32_t aHostVersion);

private:
  PRLibrary* mLib = nullptr;
};

} // namespace mozilla

#endif // ChromiumAdapter_h_
