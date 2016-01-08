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

namespace mozilla {

class CryptoSample;

namespace gmp {

class GMPContentParent;

class GMPDecryptorParent final : public GMPDecryptorProxy
                               , public PGMPDecryptorParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPDecryptorParent)

  explicit GMPDecryptorParent(GMPContentParent *aPlugin);

  // GMPDecryptorProxy

  virtual uint32_t GetPluginId() const override { return mPluginId; }

  virtual nsresult Init(GMPDecryptorProxyCallback* aCallback) override;

  virtual void CreateSession(uint32_t aCreateSessionToken,
                             uint32_t aPromiseId,
                             const nsCString& aInitDataType,
                             const nsTArray<uint8_t>& aInitData,
                             GMPSessionType aSessionType) override;

  virtual void LoadSession(uint32_t aPromiseId,
                           const nsCString& aSessionId) override;

  virtual void UpdateSession(uint32_t aPromiseId,
                             const nsCString& aSessionId,
                             const nsTArray<uint8_t>& aResponse) override;

  virtual void CloseSession(uint32_t aPromiseId,
                            const nsCString& aSessionId) override;

  virtual void RemoveSession(uint32_t aPromiseId,
                             const nsCString& aSessionId) override;

  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const nsTArray<uint8_t>& aServerCert) override;

  virtual void Decrypt(uint32_t aId,
                       const CryptoSample& aCrypto,
                       const nsTArray<uint8_t>& aBuffer) override;

  virtual void Close() override;

  void Shutdown();

private:
  ~GMPDecryptorParent();

  // PGMPDecryptorParent

  virtual bool RecvSetSessionId(const uint32_t& aCreateSessionToken,
                                const nsCString& aSessionId) override;

  virtual bool RecvResolveLoadSessionPromise(const uint32_t& aPromiseId,
                                             const bool& aSuccess) override;

  virtual bool RecvResolvePromise(const uint32_t& aPromiseId) override;

  virtual bool RecvRejectPromise(const uint32_t& aPromiseId,
                                 const GMPDOMException& aException,
                                 const nsCString& aMessage) override;

  virtual bool RecvSessionMessage(const nsCString& aSessionId,
                                  const GMPSessionMessageType& aMessageType,
                                  nsTArray<uint8_t>&& aMessage) override;

  virtual bool RecvExpirationChange(const nsCString& aSessionId,
                                    const double& aExpiryTime) override;

  virtual bool RecvSessionClosed(const nsCString& aSessionId) override;

  virtual bool RecvSessionError(const nsCString& aSessionId,
                                const GMPDOMException& aException,
                                const uint32_t& aSystemCode,
                                const nsCString& aMessage) override;

  virtual bool RecvKeyStatusChanged(const nsCString& aSessionId,
                                    InfallibleTArray<uint8_t>&& aKeyId,
                                    const GMPMediaKeyStatus& aStatus) override;

  virtual bool RecvDecrypted(const uint32_t& aId,
                             const GMPErr& aErr,
                             InfallibleTArray<uint8_t>&& aBuffer) override;

  virtual bool RecvSetCaps(const uint64_t& aCaps) override;

  virtual bool RecvShutdown() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
  virtual bool Recv__delete__() override;

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
