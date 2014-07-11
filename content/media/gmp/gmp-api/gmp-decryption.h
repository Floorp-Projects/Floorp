/*
* Copyright 2013, Mozilla Foundation and contributors
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

#ifndef GMP_DECRYPTION_h_
#define GMP_DECRYPTION_h_

#include "gmp-platform.h"

class GMPEncryptedBufferData {
public:
  // Key ID to identify the decryption key.
  virtual const uint8_t* KeyId() const = 0;

  // Size (in bytes) of |KeyId()|.
  virtual uint32_t KeyIdSize() const = 0;

  // Initialization vector.
  virtual const uint8_t* IV() const = 0;

  // Size (in bytes) of |IV|.
  virtual uint32_t IVSize() const = 0;

  // Number of enties returned by ClearBytes and CipherBytes().
  virtual uint32_t NumSubsamples() const = 0;

  virtual const uint32_t* ClearBytes() const = 0;

  virtual const uint32_t* CipherBytes() const = 0;
};

// These match to the DOMException codes as per:
// http://www.w3.org/TR/dom/#domexception
enum GMPDOMException {
  kGMPNoModificationAllowedError = 7,
  kGMPNotFoundError = 8,
  kGMPNotSupportedError = 9,
  kGMPInvalidStateError = 11,
  kGMPSyntaxError = 12,
  kGMPInvalidModificationError = 13,
  kGMPInvalidAccessError = 15,
  kGMPSecurityError = 18,
  kGMPAbortError = 20,
  kGMPQuotaExceededError = 22,
  kGMPTimeoutError = 23
};

// Time in milliseconds, as offset from epoch, 1 Jan 1970.
typedef int64_t GMPTimestamp;

class GMPDecryptorCallback {
public:
  // Resolves a promise for a session created or loaded.
  // Passes the session id to be exposed to JavaScript.
  // Must be called before OnSessionMessage().
  // aSessionId must be null terminated.
  virtual void OnResolveNewSessionPromise(uint32_t aPromiseId,
                                          const char* aSessionId,
                                          uint32_t aSessionIdLength) = 0;

  // Called to resolve a specified promise with "undefined".
  virtual void OnResolvePromise(uint32_t aPromiseId) = 0;

  // Called to reject a promise with a DOMException.
  // aMessage is logged to the WebConsole.
  // aMessage is optional, but if present must be null terminated.
  virtual void OnRejectPromise(uint32_t aPromiseId,
                               GMPDOMException aException,
                               const char* aMessage,
                               uint32_t aMessageLength) = 0;

  // Called by the CDM when it has a message for session |session_id|.
  // Length parameters should not include null termination.
  // aSessionId must be null terminated.
  virtual void OnSessionMessage(const char* aSessionId,
                                uint32_t aSessionIdLength,
                                const uint8_t* aMessage,
                                uint32_t aMessageLength,
                                const char* aDestinationURL,
                                uint32_t aDestinationURLLength) = 0;

  // aSessionId must be null terminated.
   virtual void OnExpirationChange(const char* aSessionId,
                                   uint32_t aSessionIdLength,
                                   GMPTimestamp aExpiryTime) = 0;

  // Called by the GMP when a session is closed. All file IO
  // that a session requires should be complete before calling this.
  // aSessionId must be null terminated.
  virtual void OnSessionClosed(const char* aSessionId,
                               uint32_t aSessionIdLength) = 0;

  // Called by the GMP when an error occurs in a session.
  // aSessionId must be null terminated.
  // aMessage is logged to the WebConsole.
  // aMessage is optional, but if present must be null terminated.
  virtual void OnSessionError(const char* aSessionId,
                              uint32_t aSessionIdLength,
                              GMPDOMException aException,
                              uint32_t aSystemCode,
                              const char* aMessage,
                              uint32_t aMessageLength) = 0;

  virtual void OnKeyIdUsable(const char* aSessionId,
                             uint32_t aSessionIdLength,
                             const uint8_t* aKeyId,
                             uint32_t aKeyIdLength) = 0;

  // Marks a key as no longer usable.
  // Note: Keys are assumed to be not usable when a session is closed or removed.
  virtual void OnKeyIdNotUsable(const char* aSessionId,
                                uint32_t aSessionIdLength,
                                const uint8_t* aKeyId,
                                uint32_t aKeyIdLength) = 0;

};

// Host interface, passed to GetAPIFunc(), with "decrypt".
class GMPDecryptorHost {
public:

  // Returns an origin specific string uniquely identifying the device.
  // The node id contains a random component, and is consistent between
  // plugin instantiations, unless the user clears it.
  // Different origins have different node ids.
  // The node id pointer returned here remains valid for the until shutdown
  // begins.
  // *aOutNodeId is null terminated.
  virtual void GetNodeId(const char** aOutNodeId,
                         uint32_t* aOutNodeIdLength) = 0;

  virtual void GetSandboxVoucher(const uint8_t** aVoucher,
                                 uint8_t* aVoucherLength) = 0;

  virtual void GetPluginVoucher(const uint8_t** aVoucher,
                                uint8_t* aVoucherLength) = 0;
};

enum GMPSessionType {
  kGMPTemporySession = 0,
  kGMPPersistentSession = 1
};

// API exposed by plugin library to manage decryption sessions.
// When the Host requests this by calling GMPGetAPIFunc().
//
// API name: "eme-decrypt".
// Host API: GMPDecryptorHost
class GMPDecryptor {
public:

  // Sets the callback to use with the decryptor to return results
  // to Gecko.
  virtual void Init(GMPDecryptorCallback* aCallback) = 0;

  // Requests the creation of a session given |aType| and |aInitData|.
  // Decryptor should callback GMPDecryptorCallback::OnSessionCreated()
  // with the web session ID on success, or OnSessionError() on failure,
  // and then call OnSessionReady() once all keys for that session are
  // available.
  virtual void CreateSession(uint32_t aPromiseId,
                             const char* aInitDataType,
                             uint32_t aInitDataTypeSize,
                             const uint8_t* aInitData,
                             uint32_t aInitDataSize,
                             GMPSessionType aSessionType) = 0;

  // Loads a previously loaded persistent session.
  virtual void LoadSession(uint32_t aPromiseId,
                           const char* aSessionId,
                           uint32_t aSessionIdLength) = 0;

  // Updates the session with |aResponse|.
  virtual void UpdateSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength,
                             const uint8_t* aResponse,
                             uint32_t aResponseSize) = 0;

  // Releases the resources (keys) for the specified session.
  virtual void CloseSession(uint32_t aPromiseId,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) = 0;

  // Removes the resources (keys) for the specified session.
  virtual void RemoveSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength) = 0;

  // Resolve/reject promise on completion.
  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const uint8_t* aServerCert,
                                    uint32_t aServerCertSize) = 0;
};

#endif // GMP_DECRYPTION_h_
