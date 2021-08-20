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

// This include is required in order for content_decryption_module to work
// on Unix systems.
#include <stddef.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "content_decryption_module.h"

#include "RefCounted.h"

class ClearKeySessionManager;

// Whether we've loaded the persistent session ids yet.
enum PersistentKeyState { UNINITIALIZED, LOADING, LOADED };

class ClearKeyPersistence : public RefCounted {
 public:
  explicit ClearKeyPersistence(cdm::Host_10* aHost);

  void EnsureInitialized(bool aPersistentStateAllowed,
                         std::function<void()>&& aOnInitialized);

  bool IsLoaded() const;

  std::string GetNewSessionId(cdm::SessionType aSessionType);

  bool IsPersistentSessionId(const std::string& aSid);

  void PersistentSessionRemoved(std::string& aSid);

 private:
  cdm::Host_10* mHost = nullptr;

  PersistentKeyState mPersistentKeyState = PersistentKeyState::UNINITIALIZED;

  std::set<uint32_t> mPersistentSessionIds;

  void ReadAllRecordsFromIndex(std::function<void()>&& aOnComplete);
  void WriteIndex();
};

#endif  // __ClearKeyPersistence_h__
