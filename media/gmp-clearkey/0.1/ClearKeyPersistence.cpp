/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClearKeyPersistence.h"
#include "ClearKeyUtils.h"
#include "ClearKeyStorage.h"
#include "ClearKeySessionManager.h"
#include "mozilla/RefPtr.h"

#include <stdint.h>
#include <string.h>
#include <set>
#include <vector>
#include <sstream>

using namespace mozilla;
using namespace std;

// Whether we've loaded the persistent session ids from GMPStorage yet.
enum PersistentKeyState {
  UNINITIALIZED,
  LOADING,
  LOADED
};
static PersistentKeyState sPersistentKeyState = UNINITIALIZED;

// Set of session Ids of the persistent sessions created or residing in
// storage.
static set<uint32_t> sPersistentSessionIds;

static vector<GMPTask*> sTasksBlockedOnSessionIdLoad;

static void
ReadAllRecordsFromIterator(GMPRecordIterator* aRecordIterator,
                           void* aUserArg,
                           GMPErr aStatus)
{
  MOZ_ASSERT(sPersistentKeyState == LOADING);
  if (GMP_SUCCEEDED(aStatus)) {
    // Extract the record names which are valid uint32_t's; they're
    // the persistent session ids.
    const char* name = nullptr;
    uint32_t len = 0;
    while (GMP_SUCCEEDED(aRecordIterator->GetName(&name, &len))) {
      if (ClearKeyUtils::IsValidSessionId(name, len)) {
        MOZ_ASSERT(name[len] == 0);
        sPersistentSessionIds.insert(atoi(name));
      }
      aRecordIterator->NextRecord();
    }
  }
  sPersistentKeyState = LOADED;
  aRecordIterator->Close();

  for (size_t i = 0; i < sTasksBlockedOnSessionIdLoad.size(); i++) {
    sTasksBlockedOnSessionIdLoad[i]->Run();
    sTasksBlockedOnSessionIdLoad[i]->Destroy();
  }
  sTasksBlockedOnSessionIdLoad.clear();
}

/* static */ void
ClearKeyPersistence::EnsureInitialized()
{
  if (sPersistentKeyState == UNINITIALIZED) {
    sPersistentKeyState = LOADING;
    if (GMP_FAILED(EnumRecordNames(&ReadAllRecordsFromIterator))) {
      sPersistentKeyState = LOADED;
    }
  }
}

/* static */ string
ClearKeyPersistence::GetNewSessionId(GMPSessionType aSessionType)
{
  static uint32_t sNextSessionId = 1;

  // Ensure we don't re-use a session id that was persisted.
  while (Contains(sPersistentSessionIds, sNextSessionId)) {
    sNextSessionId++;
  }

  string sessionId;
  stringstream ss;
  ss << sNextSessionId;
  ss >> sessionId;

  if (aSessionType == kGMPPersistentSession) {
    sPersistentSessionIds.insert(sNextSessionId);
  }

  sNextSessionId++;

  return sessionId;
}


class CreateSessionTask : public GMPTask {
public:
  CreateSessionTask(ClearKeySessionManager* aTarget,
                    uint32_t aCreateSessionToken,
                    uint32_t aPromiseId,
                    const uint8_t* aInitData,
                    uint32_t aInitDataSize,
                    GMPSessionType aSessionType)
    : mTarget(aTarget)
    , mCreateSessionToken(aCreateSessionToken)
    , mPromiseId(aPromiseId)
    , mSessionType(aSessionType)
  {
    mInitData.insert(mInitData.end(),
                     aInitData,
                     aInitData + aInitDataSize);
  }
  virtual void Run() MOZ_OVERRIDE {
    mTarget->CreateSession(mCreateSessionToken,
                           mPromiseId,
                           "cenc",
                           strlen("cenc"),
                           &mInitData.front(),
                           mInitData.size(),
                           mSessionType);
  }
  virtual void Destroy() MOZ_OVERRIDE {
    delete this;
  }
private:
  RefPtr<ClearKeySessionManager> mTarget;
  uint32_t mCreateSessionToken;
  uint32_t mPromiseId;
  vector<uint8_t> mInitData;
  GMPSessionType mSessionType;
};


/* static */ bool
ClearKeyPersistence::DeferCreateSessionIfNotReady(ClearKeySessionManager* aInstance,
                                                  uint32_t aCreateSessionToken,
                                                  uint32_t aPromiseId,
                                                  const uint8_t* aInitData,
                                                  uint32_t aInitDataSize,
                                                  GMPSessionType aSessionType)
{
  if (sPersistentKeyState >= LOADED)  {
    return false;
  }
  GMPTask* t = new CreateSessionTask(aInstance,
                                     aCreateSessionToken,
                                     aPromiseId,
                                     aInitData,
                                     aInitDataSize,
                                     aSessionType);
  sTasksBlockedOnSessionIdLoad.push_back(t);
  return true;
}

class LoadSessionTask : public GMPTask {
public:
  LoadSessionTask(ClearKeySessionManager* aTarget,
                  uint32_t aPromiseId,
                  const char* aSessionId,
                  uint32_t aSessionIdLength)
    : mTarget(aTarget)
    , mPromiseId(aPromiseId)
    , mSessionId(aSessionId, aSessionId + aSessionIdLength)
  {
  }
  virtual void Run() MOZ_OVERRIDE {
    mTarget->LoadSession(mPromiseId,
                         mSessionId.c_str(),
                         mSessionId.size());
  }
  virtual void Destroy() MOZ_OVERRIDE {
    delete this;
  }
private:
  RefPtr<ClearKeySessionManager> mTarget;
  uint32_t mPromiseId;
  string mSessionId;
};

/* static */ bool
ClearKeyPersistence::DeferLoadSessionIfNotReady(ClearKeySessionManager* aInstance,
                                                uint32_t aPromiseId,
                                                const char* aSessionId,
                                                uint32_t aSessionIdLength)
{
  if (sPersistentKeyState >= LOADED)  {
    return false;
  }
  GMPTask* t = new LoadSessionTask(aInstance,
                                   aPromiseId,
                                   aSessionId,
                                   aSessionIdLength);
  sTasksBlockedOnSessionIdLoad.push_back(t);
  return true;
}

/* static */ bool
ClearKeyPersistence::IsPersistentSessionId(const string& aSessionId)
{
  return Contains(sPersistentSessionIds, atoi(aSessionId.c_str()));
}

class LoadSessionFromKeysTask : public ReadContinuation {
public:
  LoadSessionFromKeysTask(ClearKeySessionManager* aTarget,
                          const string& aSessionId,
                          uint32_t aPromiseId)
    : mTarget(aTarget)
    , mSessionId(aSessionId)
    , mPromiseId(aPromiseId)
  {
  }

  virtual void ReadComplete(GMPErr aStatus,
                            const uint8_t* aData,
                            uint32_t aLength) MOZ_OVERRIDE
  {
    mTarget->PersistentSessionDataLoaded(aStatus, mPromiseId, mSessionId, aData, aLength);
  }
private:
  RefPtr<ClearKeySessionManager> mTarget;
  string mSessionId;
  uint32_t mPromiseId;
};

/* static */ void
ClearKeyPersistence::LoadSessionData(ClearKeySessionManager* aInstance,
                                     const string& aSid,
                                     uint32_t aPromiseId)
{
  LoadSessionFromKeysTask* loadTask =
    new LoadSessionFromKeysTask(aInstance, aSid, aPromiseId);
  ReadData(aSid, loadTask);
}

/* static */ void
ClearKeyPersistence::PersistentSessionRemoved(const string& aSessionId)
{
  sPersistentSessionIds.erase(atoi(aSessionId.c_str()));
}
