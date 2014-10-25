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
namespace mp4_demuxer {
class CryptoSample;
}

namespace mozilla {
namespace gmp {

class GMPParent;

class GMPDecryptorParent MOZ_FINAL : public GMPDecryptorProxy
                                   , public PGMPDecryptorParent
{
public:
  NS_INLINE_DECL_REFCOUNTING(GMPDecryptorParent)

  explicit GMPDecryptorParent(GMPParent *aPlugin);

  // GMPDecryptorProxy
  virtual nsresult Init(GMPDecryptorProxyCallback* aCallback) MOZ_OVERRIDE;

  virtual void CreateSession(uint32_t aPromiseId,
                             const nsCString& aInitDataType,
                             const nsTArray<uint8_t>& aInitData,
                             GMPSessionType aSessionType) MOZ_OVERRIDE;

  virtual void LoadSession(uint32_t aPromiseId,
                           const nsCString& aSessionId) MOZ_OVERRIDE;

  virtual void UpdateSession(uint32_t aPromiseId,
                             const nsCString& aSessionId,
                             const nsTArray<uint8_t>& aResponse) MOZ_OVERRIDE;

  virtual void CloseSession(uint32_t aPromiseId,
                            const nsCString& aSessionId) MOZ_OVERRIDE;

  virtual void RemoveSession(uint32_t aPromiseId,
                             const nsCString& aSessionId) MOZ_OVERRIDE;

  virtual void SetServerCertificate(uint32_t aPromiseId,
                                    const nsTArray<uint8_t>& aServerCert) MOZ_OVERRIDE;

  virtual void Decrypt(uint32_t aId,
                       const mp4_demuxer::CryptoSample& aCrypto,
                       const nsTArray<uint8_t>& aBuffer) MOZ_OVERRIDE;

  virtual void Close() MOZ_OVERRIDE;

  void Shutdown();

private:
  ~GMPDecryptorParent();

  // PGMPDecryptorParent

  virtual bool RecvResolveNewSessionPromise(const uint32_t& aPromiseId,
                                            const nsCString& aSessionId) MOZ_OVERRIDE;

  virtual bool RecvResolveLoadSessionPromise(const uint32_t& aPromiseId,
                                             const bool& aSuccess) MOZ_OVERRIDE;

  virtual bool RecvResolvePromise(const uint32_t& aPromiseId) MOZ_OVERRIDE;

  virtual bool RecvRejectPromise(const uint32_t& aPromiseId,
                                 const GMPDOMException& aException,
                                 const nsCString& aMessage) MOZ_OVERRIDE;

  virtual bool RecvSessionMessage(const nsCString& aSessionId,
                                  const nsTArray<uint8_t>& aMessage,
                                  const nsCString& aDestinationURL) MOZ_OVERRIDE;

  virtual bool RecvExpirationChange(const nsCString& aSessionId,
                                    const double& aExpiryTime) MOZ_OVERRIDE;

  virtual bool RecvSessionClosed(const nsCString& aSessionId) MOZ_OVERRIDE;

  virtual bool RecvSessionError(const nsCString& aSessionId,
                                const GMPDOMException& aException,
                                const uint32_t& aSystemCode,
                                const nsCString& aMessage) MOZ_OVERRIDE;

  virtual bool RecvKeyIdUsable(const nsCString& aSessionId,
                                const nsTArray<uint8_t>& aKeyId) MOZ_OVERRIDE;

  virtual bool RecvKeyIdNotUsable(const nsCString& aSessionId,
                                  const nsTArray<uint8_t>& aKeyId) MOZ_OVERRIDE;

  virtual bool RecvDecrypted(const uint32_t& aId,
                             const GMPErr& aErr,
                             const nsTArray<uint8_t>& aBuffer) MOZ_OVERRIDE;

  virtual bool RecvSetCaps(const uint64_t& aCaps) MOZ_OVERRIDE;

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;
  virtual bool Recv__delete__() MOZ_OVERRIDE;

  bool mIsOpen;
  bool mShuttingDown;
  nsRefPtr<GMPParent> mPlugin;
  GMPDecryptorProxyCallback* mCallback;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPDecryptorChild_h_
