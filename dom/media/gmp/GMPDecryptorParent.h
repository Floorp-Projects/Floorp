/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPDecryptorParent_h_
#define GMPDecryptorParent_h_

#include "mozilla/gmp/PGMPDecryptorParent.h"
#include "mozilla/RefPtr.h"
#include "gmp-decryption.h"
#include "GMPDecryptorProxy.h"
#include "GMPCrashHelperHolder.h"

namespace mozilla {

class CryptoSample;

namespace gmp {

class GMPContentParent;

class GMPDecryptorParent final : public GMPDecryptorProxy
                               , public PGMPDecryptorParent
                               , public GMPCrashHelperHolder
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPDecryptorParent)

  explicit GMPDecryptorParent(GMPContentParent *aPlugin);

  // GMPDecryptorProxy

  uint32_t GetPluginId() const override { return mPluginId; }

  nsresult Init(GMPDecryptorProxyCallback* aCallback,
                bool aDistinctiveIdentifierRequired,
                bool aPersistentStateRequired) override;

  void CreateSession(uint32_t aCreateSessionToken,
                     uint32_t aPromiseId,
                     const nsCString& aInitDataType,
                     const nsTArray<uint8_t>& aInitData,
                     GMPSessionType aSessionType) override;

  void LoadSession(uint32_t aPromiseId,
                   const nsCString& aSessionId) override;

  void UpdateSession(uint32_t aPromiseId,
                     const nsCString& aSessionId,
                     const nsTArray<uint8_t>& aResponse) override;

  void CloseSession(uint32_t aPromiseId,
                    const nsCString& aSessionId) override;

  void RemoveSession(uint32_t aPromiseId,
                     const nsCString& aSessionId) override;

  void SetServerCertificate(uint32_t aPromiseId,
                            const nsTArray<uint8_t>& aServerCert) override;

  void Decrypt(uint32_t aId,
               const CryptoSample& aCrypto,
               const nsTArray<uint8_t>& aBuffer) override;

  void Close() override;

  void Shutdown();

private:
  ~GMPDecryptorParent();

  // PGMPDecryptorParent

  bool RecvSetSessionId(const uint32_t& aCreateSessionToken,
                        const nsCString& aSessionId) override;

  bool RecvResolveLoadSessionPromise(const uint32_t& aPromiseId,
                                     const bool& aSuccess) override;

  bool RecvResolvePromise(const uint32_t& aPromiseId) override;

  bool RecvRejectPromise(const uint32_t& aPromiseId,
                         const GMPDOMException& aException,
                         const nsCString& aMessage) override;

  bool RecvSessionMessage(const nsCString& aSessionId,
                          const GMPSessionMessageType& aMessageType,
                          nsTArray<uint8_t>&& aMessage) override;

  bool RecvExpirationChange(const nsCString& aSessionId,
                            const double& aExpiryTime) override;

  bool RecvSessionClosed(const nsCString& aSessionId) override;

  bool RecvSessionError(const nsCString& aSessionId,
                        const GMPDOMException& aException,
                        const uint32_t& aSystemCode,
                        const nsCString& aMessage) override;

  bool RecvDecrypted(const uint32_t& aId,
                     const GMPErr& aErr,
                     InfallibleTArray<uint8_t>&& aBuffer) override;

  bool RecvBatchedKeyStatusChanged(const nsCString& aSessionId,
                                   InfallibleTArray<GMPKeyInformation>&& aKeyInfos) override;

  bool RecvShutdown() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  bool Recv__delete__() override;

  bool mIsOpen;
  bool mShuttingDown;
  bool mActorDestroyed;
  RefPtr<GMPContentParent> mPlugin;
  uint32_t mPluginId;
  GMPDecryptorProxyCallback* mCallback;
#ifdef DEBUG
  nsIThread* const mGMPThread;
#endif
};

} // namespace gmp
} // namespace mozilla

#endif // GMPDecryptorChild_h_
