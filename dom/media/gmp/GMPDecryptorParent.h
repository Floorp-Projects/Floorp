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

  mozilla::ipc::IPCResult RecvSetDecryptorId(const uint32_t& aId) override;

  mozilla::ipc::IPCResult RecvSetSessionId(const uint32_t& aCreateSessionToken,
                                           const nsCString& aSessionId) override;

  mozilla::ipc::IPCResult RecvResolveLoadSessionPromise(const uint32_t& aPromiseId,
                                                        const bool& aSuccess) override;

  mozilla::ipc::IPCResult RecvResolvePromise(const uint32_t& aPromiseId) override;

  mozilla::ipc::IPCResult RecvRejectPromise(const uint32_t& aPromiseId,
                                            const GMPDOMException& aException,
                                            const nsCString& aMessage) override;

  mozilla::ipc::IPCResult RecvSessionMessage(const nsCString& aSessionId,
                                             const GMPSessionMessageType& aMessageType,
                                             nsTArray<uint8_t>&& aMessage) override;

  mozilla::ipc::IPCResult RecvExpirationChange(const nsCString& aSessionId,
                                               const double& aExpiryTime) override;

  mozilla::ipc::IPCResult RecvSessionClosed(const nsCString& aSessionId) override;

  mozilla::ipc::IPCResult RecvSessionError(const nsCString& aSessionId,
                                           const GMPDOMException& aException,
                                           const uint32_t& aSystemCode,
                                           const nsCString& aMessage) override;

  mozilla::ipc::IPCResult RecvDecrypted(const uint32_t& aId,
                                        const GMPErr& aErr,
                                        InfallibleTArray<uint8_t>&& aBuffer) override;

  mozilla::ipc::IPCResult RecvBatchedKeyStatusChanged(const nsCString& aSessionId,
                                                      InfallibleTArray<GMPKeyInformation>&& aKeyInfos) override;

  mozilla::ipc::IPCResult RecvShutdown() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
  mozilla::ipc::IPCResult Recv__delete__() override;

  bool mIsOpen;
  bool mShuttingDown;
  bool mActorDestroyed;
  RefPtr<GMPContentParent> mPlugin;
  uint32_t mPluginId;
  GMPDecryptorProxyCallback* mCallback;
#ifdef DEBUG
  nsCOMPtr<nsISerialEventTarget> const mGMPEventTarget;
#endif
};

} // namespace gmp
} // namespace mozilla

#endif // GMPDecryptorChild_h_
