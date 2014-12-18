/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ClearKeyPersistence_h__
#define __ClearKeyPersistence_h__

#include <string>
#include "gmp-decryption.h"

class ClearKeyDecryptionManager;

class ClearKeyPersistence {
public:
  static void EnsureInitialized();

  static std::string GetNewSessionId(GMPSessionType aSessionType);

  static bool DeferCreateSessionIfNotReady(ClearKeyDecryptionManager* aInstance,
                                           uint32_t aPromiseId,
                                           const uint8_t* aInitData,
                                           uint32_t aInitDataSize,
                                           GMPSessionType aSessionType);

  static bool DeferLoadSessionIfNotReady(ClearKeyDecryptionManager* aInstance,
                                         uint32_t aPromiseId,
                                         const char* aSessionId,
                                         uint32_t aSessionIdLength);

  static bool IsPersistentSessionId(const std::string& aSid);

  static void LoadSessionData(ClearKeyDecryptionManager* aInstance,
                              const std::string& aSid,
                              uint32_t aPromiseId);

  static void PersistentSessionRemoved(const std::string& aSid);
};

#endif // __ClearKeyPersistence_h__
