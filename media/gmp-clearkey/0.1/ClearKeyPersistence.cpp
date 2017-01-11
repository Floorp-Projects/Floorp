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

#include "ClearKeyPersistence.h"

#include "ClearKeyUtils.h"
#include "ClearKeyStorage.h"
#include "ClearKeySessionManager.h"
#include "RefCounted.h"

#include <assert.h>
#include <stdint.h>
#include <sstream>
#include <string.h>

using namespace std;
using namespace cdm;

void
ClearKeyPersistence::ReadAllRecordsFromIndex(function<void()>&& aOnComplete) {
  // Clear what we think the index file contains, we're about to read it again.
  mPersistentSessionIds.clear();

  // Hold a reference to the persistence manager, so it isn't released before
  // we try and use it.
  RefPtr<ClearKeyPersistence> self(this);
  function<void(const uint8_t*, uint32_t)> onIndexSuccess =
    [self, aOnComplete] (const uint8_t* data, uint32_t size)
  {
    CK_LOGD("ClearKeyPersistence: Loaded index file!");
    const char* charData = (const char*)data;

    stringstream ss(string(charData, charData + size));
    string name;
    while (getline(ss, name)) {
      if (ClearKeyUtils::IsValidSessionId(name.data(), name.size())) {
        self->mPersistentSessionIds.insert(atoi(name.c_str()));
      }
    }

    self->mPersistentKeyState = PersistentKeyState::LOADED;
    aOnComplete();
  };

  function<void()> onIndexFailed =
    [self, aOnComplete] ()
  {
    CK_LOGD("ClearKeyPersistence: Failed to load index file (it might not exist");
    self->mPersistentKeyState = PersistentKeyState::LOADED;
    aOnComplete();
  };

  string filename = "index";
  ReadData(mHost, filename, move(onIndexSuccess), move(onIndexFailed));
}

void
ClearKeyPersistence::WriteIndex() {
  function <void()> onIndexSuccess =
    [] ()
  {
    CK_LOGD("ClearKeyPersistence: Wrote index file");
  };

  function <void()> onIndexFail =
    [] ()
  {
    CK_LOGD("ClearKeyPersistence: Failed to write index file (this is bad)");
  };

  stringstream ss;

  for (const uint32_t& sessionId : mPersistentSessionIds) {
    ss << sessionId;
    ss << '\n';
  }

  string dataString = ss.str();
  uint8_t* dataArray = (uint8_t*)dataString.data();
  vector<uint8_t> data(dataArray, dataArray + dataString.size());

  string filename = "index";
  WriteData(mHost,
            filename,
            data,
            move(onIndexSuccess),
            move(onIndexFail));
}


ClearKeyPersistence::ClearKeyPersistence(Host_8* aHost)
{
  this->mHost = aHost;
}

void
ClearKeyPersistence::EnsureInitialized(bool aPersistentStateAllowed,
                                       function<void()>&& aOnInitialized)
{
  if (aPersistentStateAllowed &&
      mPersistentKeyState == PersistentKeyState::UNINITIALIZED) {
    mPersistentKeyState = LOADING;
    ReadAllRecordsFromIndex(move(aOnInitialized));
  } else {
    mPersistentKeyState = PersistentKeyState::LOADED;
    aOnInitialized();
  }
}

bool ClearKeyPersistence::IsLoaded() const
{
  return mPersistentKeyState == PersistentKeyState::LOADED;
}

string
ClearKeyPersistence::GetNewSessionId(SessionType aSessionType)
{
  static uint32_t sNextSessionId = 1;

  // Ensure we don't re-use a session id that was persisted.
  while (Contains(mPersistentSessionIds, sNextSessionId)) {
    sNextSessionId++;
  }

  string sessionId;
  stringstream ss;
  ss << sNextSessionId;
  ss >> sessionId;

  if (aSessionType == SessionType::kPersistentLicense) {
    mPersistentSessionIds.insert(sNextSessionId);

    // Save the updated index file.
    WriteIndex();
  }

  sNextSessionId++;

  return sessionId;
}

bool
ClearKeyPersistence::IsPersistentSessionId(const string& aSessionId)
{
  return Contains(mPersistentSessionIds, atoi(aSessionId.c_str()));
}

void
ClearKeyPersistence::PersistentSessionRemoved(string& aSessionId)
{
  mPersistentSessionIds.erase(atoi(aSessionId.c_str()));

  // Update the index file.
  WriteIndex();
}
