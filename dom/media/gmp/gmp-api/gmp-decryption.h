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

class GMPStringList {
public:
  virtual uint32_t Size() const = 0;

  virtual void StringAt(uint32_t aIndex,
                        const char** aOutString, uint32_t* aOutLength) const = 0;

  virtual ~GMPStringList() { }
};

class GMPEncryptedBufferMetadata {
public:
  // Key ID to identify the decryption key.
  virtual const uint8_t* KeyId() const = 0;

  // Size (in bytes) of |KeyId()|.
  virtual uint32_t KeyIdSize() const = 0;

  // Initialization vector.
  virtual const uint8_t* IV() const = 0;

  // Size (in bytes) of |IV|.
  virtual uint32_t IVSize() const = 0;

  // Number of entries returned by ClearBytes() and CipherBytes().
  virtual uint32_t NumSubsamples() const = 0;

  virtual const uint16_t* ClearBytes() const = 0;

  virtual const uint32_t* CipherBytes() const = 0;

  virtual ~GMPEncryptedBufferMetadata() {}

  // The set of MediaKeySession IDs associated with this decryption key in
  // the current stream.
  virtual const GMPStringList* SessionIds() const = 0;
};

class GMPBuffer {
public:
  virtual uint32_t Id() const = 0;
  virtual uint8_t* Data() = 0;
  virtual uint32_t Size() const = 0;
  virtual void Resize(uint32_t aSize) = 0;
  virtual ~GMPBuffer() {}
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

enum GMPSessionMessageType {
  kGMPLicenseRequest = 0,
  kGMPLicenseRenewal = 1,
  kGMPLicenseRelease = 2,
  kGMPIndividualizationRequest = 3,
  kGMPMessageInvalid = 4 // Must always be last.
};

enum GMPMediaKeyStatus {
  kGMPUsable = 0,
  kGMPExpired = 1,
  kGMPOutputDownscaled = 2,
  kGMPOutputRestricted = 3,
  kGMPInternalError = 4,
  kGMPUnknown = 5, // Removes key from MediaKeyStatusMap
  kGMPReleased = 6,
  kGMPStatusPending = 7,
  kGMPMediaKeyStatusInvalid = 8 // Must always be last.
};

// Time in milliseconds, as offset from epoch, 1 Jan 1970.
typedef int64_t GMPTimestamp;

// Callbacks to be called from the CDM. Threadsafe.
class GMPDecryptorCallback {
public:

  // The GMPDecryptor should call this in response to a call to
  // GMPDecryptor::CreateSession(). The GMP host calls CreateSession() when
  // MediaKeySession.generateRequest() is called by JavaScript.
  // After CreateSession() is called, the GMPDecryptor should call
  // GMPDecryptorCallback::SetSessionId() to set the sessionId exposed to
  // JavaScript on the MediaKeySession on which the generateRequest() was
  // called. SetSessionId() must be called before
  // GMPDecryptorCallback::SessionMessage() will work.
  // aSessionId must be null terminated.
  // Note: pass the aCreateSessionToken from the CreateSession() call,
  // and then once the session has sent any messages required for the
  // license request to be sent, then resolve the aPromiseId that was passed
  // to GMPDecryptor::CreateSession().
  // Note: GMPDecryptor::LoadSession() does *not* need to call SetSessionId()
  // for GMPDecryptorCallback::SessionMessage() to work.
  virtual void SetSessionId(uint32_t aCreateSessionToken,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) = 0;

  // Resolves a promise for a session loaded.
  // Resolves to false if we don't have any session data stored for the given
  // session ID.
  // Must be called before SessionMessage().
  virtual void ResolveLoadSessionPromise(uint32_t aPromiseId,
                                         bool aSuccess) = 0;

  // Called to resolve a specified promise with "undefined".
  virtual void ResolvePromise(uint32_t aPromiseId) = 0;

  // Called to reject a promise with a DOMException.
  // aMessage is logged to the WebConsole.
  // aMessage is optional, but if present must be null terminated.
  virtual void RejectPromise(uint32_t aPromiseId,
                             GMPDOMException aException,
                             const char* aMessage,
                             uint32_t aMessageLength) = 0;

  // Called by the CDM when it has a message for a session.
  // Length parameters should not include null termination.
  // aSessionId must be null terminated.
  virtual void SessionMessage(const char* aSessionId,
                              uint32_t aSessionIdLength,
                              GMPSessionMessageType aMessageType,
                              const uint8_t* aMessage,
                              uint32_t aMessageLength) = 0;

  // aSessionId must be null terminated.
   virtual void ExpirationChange(const char* aSessionId,
                                 uint32_t aSessionIdLength,
                                 GMPTimestamp aExpiryTime) = 0;

  // Called by the GMP when a session is closed. All file IO
  // that a session requires should be complete before calling this.
  // aSessionId must be null terminated.
  virtual void SessionClosed(const char* aSessionId,
                             uint32_t aSessionIdLength) = 0;

  // Called by the GMP when an error occurs in a session.
  // aSessionId must be null terminated.
  // aMessage is logged to the WebConsole.
  // aMessage is optional, but if present must be null terminated.
  virtual void SessionError(const char* aSessionId,
                            uint32_t aSessionIdLength,
                            GMPDOMException aException,
                            uint32_t aSystemCode,
                            const char* aMessage,
                            uint32_t aMessageLength) = 0;

  // Notifies the status of a key. Gecko will not call into the CDM to decrypt
  // or decode content encrypted with a key unless the CDM has marked it
  // usable first. So a CDM *MUST* mark its usable keys as usable!
  virtual void KeyStatusChanged(const char* aSessionId,
                                uint32_t aSessionIdLength,
                                const uint8_t* aKeyId,
                                uint32_t aKeyIdLength,
                                GMPMediaKeyStatus aStatus) = 0;

  // DEPRECATED; this function has no affect.
  virtual void SetCapabilities(uint64_t aCaps) = 0;

  // Returns decrypted buffer to Gecko, or reports failure.
  virtual void Decrypted(GMPBuffer* aBuffer, GMPErr aResult) = 0;

  virtual ~GMPDecryptorCallback() {}
};

// Host interface, passed to GetAPIFunc(), with "decrypt".
class GMPDecryptorHost {
public:
  virtual void GetSandboxVoucher(const uint8_t** aVoucher,
                                 uint32_t* aVoucherLength) = 0;

  virtual void GetPluginVoucher(const uint8_t** aVoucher,
                                uint32_t* aVoucherLength) = 0;

  virtual ~GMPDecryptorHost() {}
};

enum GMPSessionType {
  kGMPTemporySession = 0,
  kGMPPersistentSession = 1,
  kGMPSessionInvalid = 2 // Must always be last.
};

// Gecko supports the current GMPDecryptor version, and the previous.
#define GMP_API_DECRYPTOR "eme-decrypt-v8"
#define GMP_API_DECRYPTOR_BACKWARDS_COMPAT "eme-decrypt-v7"

// API exposed by plugin library to manage decryption sessions.
// When the Host requests this by calling GMPGetAPIFunc().
//
// API name macro: GMP_API_DECRYPTOR
// Host API: GMPDecryptorHost
class GMPDecryptor {
public:

  // Sets the callback to use with the decryptor to return results
  // to Gecko.
  virtual void Init(GMPDecryptorCallback* aCallback) = 0;

  // Initiates the creation of a session given |aType| and |aInitData|, and
  // the generation of a license request message.
  //
  // This corresponds to a MediaKeySession.generateRequest() call in JS.
  //
  // The GMPDecryptor must do the following, in order, upon this method
  // being called:
  //
  // 1. Generate a sessionId to expose to JS, and call
  //    GMPDecryptorCallback::SetSessionId(aCreateSessionToken, sessionId...)
  //    with the sessionId to be exposed to JS/EME on the MediaKeySession
  //    object on which generateRequest() was called, and then
  // 2. send any messages to JS/EME required to generate a license request
  //    given the supplied initData, and then
  // 3. generate a license request message, and send it to JS/EME, and then
  // 4. call GMPDecryptorCallback::ResolvePromise().
  //
  // Note: GMPDecryptorCallback::SetSessionId(aCreateSessionToken, sessionId, ...)
  // *must* be called before GMPDecryptorCallback::SendMessage(sessionId, ...)
  // will work.
  //
  // If generating the request fails, reject aPromiseId by calling
  // GMPDecryptorCallback::RejectPromise().
  virtual void CreateSession(uint32_t aCreateSessionToken,
                             uint32_t aPromiseId,
                             const char* aInitDataType,
                             uint32_t aInitDataTypeSize,
                             const uint8_t* aInitData,
                             uint32_t aInitDataSize,
                             GMPSessionType aSessionType) = 0;

  // Loads a previously loaded persistent session.
  //
  // This corresponds to a MediaKeySession.load() call in JS.
  //
  // The GMPDecryptor must do the following, in order, upon this method
  // being called:
  //
  // 1. Send any messages to JS/EME, or read from storage, whatever is
  //    required to load the session, and then
  // 2. if there is no session with the given sessionId loadable, call
  //    ResolveLoadSessionPromise(aPromiseId, false), otherwise
  // 2. mark the session's keys as usable, and then
  // 3. update the session's expiration, and then
  // 4. call GMPDecryptorCallback::ResolveLoadSessionPromise(aPromiseId, true).
  //
  // If loading the session fails due to error, reject aPromiseId by calling
  // GMPDecryptorCallback::RejectPromise().
  virtual void LoadSession(uint32_t aPromiseId,
                           const char* aSessionId,
                           uint32_t aSessionIdLength) = 0;

  // Updates the session with |aResponse|.
  // This corresponds to a MediaKeySession.update() call in JS.
  virtual void UpdateSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength,
                             const uint8_t* aResponse,
                             uint32_t aResponseSize) = 0;

  // Releases the resources (keys) for the specified session.
  // This corresponds to a MediaKeySession.close() call in JS.
  virtual void CloseSession(uint32_t aPromiseId,
                            const char* aSessionId,
                            uint32_t aSessionIdLength) = 0;

  // Removes the resources (keys) for the specified session.
  // This corresponds to a MediaKeySession.remove() call in JS.
  virtual void RemoveSession(uint32_t aPromiseId,
                             const char* aSessionId,
                             uint32_t aSessionIdLength) = 0;

  // Resolve/reject promise on completion.
  // This corresponds to a MediaKeySession.setServerCertificate() call in JS.
  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const uint8_t* aServerCert,
                                    uint32_t aServerCertSize) = 0;

  // Asynchronously decrypts aBuffer in place. When the decryption is
  // complete, GMPDecryptor should write the decrypted data back into the
  // same GMPBuffer object and return it to Gecko by calling Decrypted(),
  // with the GMPNoErr successcode. If decryption fails, call Decrypted()
  // with a failure code, and an error event will fire on the media element.
  // Note: When Decrypted() is called and aBuffer is passed back, aBuffer
  // is deleted. Don't forget to call Decrypted(), as otherwise aBuffer's
  // memory will leak!
  virtual void Decrypt(GMPBuffer* aBuffer,
                       GMPEncryptedBufferMetadata* aMetadata) = 0;

  // Called when the decryption operations are complete.
  // Do not call the GMPDecryptorCallback's functions after this is called.
  virtual void DecryptingComplete() = 0;

  virtual ~GMPDecryptor() {}
};

#endif // GMP_DECRYPTION_h_
