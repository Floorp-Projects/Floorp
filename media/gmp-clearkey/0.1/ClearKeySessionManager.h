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

#ifndef __ClearKeyDecryptor_h__
#define __ClearKeyDecryptor_h__

// This include is required in order for content_decryption_module to work
// on Unix systems.
#include <stddef.h>

#include <functional>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <string>

#include "ClearKeyDecryptionManager.h"
#include "ClearKeyPersistence.h"
#include "ClearKeySession.h"
#include "ClearKeyUtils.h"
#include "RefCounted.h"
#include "content_decryption_module.h"
#include "mozilla/TimeStamp.h"

class ClearKeySessionManager final : public RefCounted {
 public:
  explicit ClearKeySessionManager(cdm::Host_10* aHost);

  void Init(bool aDistinctiveIdentifierAllowed, bool aPersistentStateAllowed);

  void CreateSession(uint32_t aPromiseId, cdm::InitDataType aInitDataType,
                     const uint8_t* aInitData, uint32_t aInitDataSize,
                     cdm::SessionType aSessionType);

  void LoadSession(uint32_t aPromiseId, const char* aSessionId,
                   uint32_t aSessionIdLength);

  void UpdateSession(uint32_t aPromiseId, const char* aSessionId,
                     uint32_t aSessionIdLength, const uint8_t* aResponse,
                     uint32_t aResponseSize);

  void CloseSession(uint32_t aPromiseId, const char* aSessionId,
                    uint32_t aSessionIdLength);

  void RemoveSession(uint32_t aPromiseId, const char* aSessionId,
                     uint32_t aSessionIdLength);

  void SetServerCertificate(uint32_t aPromiseId, const uint8_t* aServerCert,
                            uint32_t aServerCertSize);

  cdm::Status Decrypt(const cdm::InputBuffer_2& aBuffer,
                      cdm::DecryptedBlock* aDecryptedBlock);

  void DecryptingComplete();

  void PersistentSessionDataLoaded(uint32_t aPromiseId,
                                   const std::string& aSessionId,
                                   const uint8_t* aKeyData,
                                   uint32_t aKeyDataSize);

  // Receives the result of an output protection query from the user agent.
  // This may trigger a key status change.
  // @param aResult indicates if the query succeeded or not. If a query did
  // not succeed then that other arguments are ignored.
  // @param aLinkMask is used to indicate if output could be captured by the
  // user agent. It should be set to `kLinkTypeNetwork` if capture is possible,
  // otherwise it should be zero.
  // @param aOutputProtectionMask this argument is unused.
  void OnQueryOutputProtectionStatus(cdm::QueryResult aResult,
                                     uint32_t aLinkMask,
                                     uint32_t aOutputProtectionMask);

  // Prompts the session manager to query the output protection status if we
  // haven't yet, or if enough time has passed since the last check. Will also
  // notify if a check has not been responded to on time.
  void QueryOutputProtectionStatusIfNeeded();

 private:
  ~ClearKeySessionManager();

  void ClearInMemorySessionData(ClearKeySession* aSession);
  bool MaybeDeferTillInitialized(std::function<void()>&& aMaybeDefer);
  void Serialize(const ClearKeySession* aSession,
                 std::vector<uint8_t>& aOutKeyData);

  // Signals the host to perform an output protection check.
  void QueryOutputProtectionStatusFromHost();

  // Called to notify the result of an output protection status call. The
  // following arguments are expected, along with their intended use:
  // - KeyStatus::kUsable indicates that the query was responded to and the
  //   response showed output is protected.
  // - KeyStatus::kOutputRestricted indicates that the query was responded to
  //   and the response showed output is not protected.
  // - KeyStatus::kInternalError indicates a query was not repsonded to on
  //   time, or that a query was responded to with a failed cdm::QueryResult.
  // The status passed to this function will be used to update the status of
  // the keyId "output-protection", which tests an observe.
  void NotifyOutputProtectionStatus(cdm::KeyStatus aStatus);

  RefPtr<ClearKeyDecryptionManager> mDecryptionManager;
  RefPtr<ClearKeyPersistence> mPersistence;

  cdm::Host_10* mHost = nullptr;

  std::set<KeyId> mKeyIds;
  std::map<std::string, ClearKeySession*> mSessions;

  // The session id of the last session created or loaded from persistent
  // storage. Used to fire test messages at that session.
  std::optional<std::string> mLastSessionId;

  std::queue<std::function<void()>> mDeferredInitialize;

  // If there is an inflight query to the host to check the output protection
  // status. Multiple in flight queries should not be allowed, avoid firing
  // more if this is true.
  bool mHasOutstandingOutputProtectionQuery = false;
  // The last time the manager called QueryOutputProtectionStatus on the host.
  mozilla::TimeStamp mLastOutputProtectionQueryTime;
};

#endif  // __ClearKeyDecryptor_h__
