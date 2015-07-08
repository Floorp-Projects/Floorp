/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __ClearKeyPersistence_h__
#define __ClearKeyPersistence_h__

#include <string>
#include "gmp-api/gmp-decryption.h"

class ClearKeySessionManager;

class ClearKeyPersistence {
public:
  static void EnsureInitialized();

  static std::string GetNewSessionId(GMPSessionType aSessionType);

  static bool DeferCreateSessionIfNotReady(ClearKeySessionManager* aInstance,
                                           uint32_t aCreateSessionToken,
                                           uint32_t aPromiseId,
                                           const uint8_t* aInitData,
                                           uint32_t aInitDataSize,
                                           GMPSessionType aSessionType);

  static bool DeferLoadSessionIfNotReady(ClearKeySessionManager* aInstance,
                                         uint32_t aPromiseId,
                                         const char* aSessionId,
                                         uint32_t aSessionIdLength);

  static bool IsPersistentSessionId(const std::string& aSid);

  static void LoadSessionData(ClearKeySessionManager* aInstance,
                              const std::string& aSid,
                              uint32_t aPromiseId);

  static void PersistentSessionRemoved(const std::string& aSid);
};

#endif // __ClearKeyPersistence_h__
