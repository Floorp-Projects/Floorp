/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMChild_h_
#define ChromiumCDMChild_h_

#include "mozilla/gmp/PChromiumCDMChild.h"

namespace mozilla {
namespace gmp {

class GMPContentChild;

class ChromiumCDMChild : public PChromiumCDMChild
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChromiumCDMChild);

  explicit ChromiumCDMChild(GMPContentChild* aPlugin);

  void Init(cdm::ContentDecryptionModule_8* aCDM);
protected:
  ~ChromiumCDMChild() {}

  ipc::IPCResult RecvInit(const bool& aAllowDistinctiveIdentifier,
                          const bool& aAllowPersistentState) override;
  ipc::IPCResult RecvSetServerCertificate(
    const uint32_t& aPromiseId,
    nsTArray<uint8_t>&& aServerCert) override;
  ipc::IPCResult RecvCreateSessionAndGenerateRequest(
    const uint32_t& aPromiseId,
    const uint32_t& aSessionType,
    const uint32_t& aInitDataType,
    nsTArray<uint8_t>&& aInitData) override;
  ipc::IPCResult RecvUpdateSession(const uint32_t& aPromiseId,
                                   const nsCString& aSessionId,
                                   nsTArray<uint8_t>&& aResponse) override;
  ipc::IPCResult RecvCloseSession(const uint32_t& aPromiseId,
                                  const nsCString& aSessionId) override;
  ipc::IPCResult RecvRemoveSession(const uint32_t& aPromiseId,
                                   const nsCString& aSessionId) override;
  ipc::IPCResult RecvDecrypt(const CDMInputBuffer& aBuffer) override;
  ipc::IPCResult RecvInitializeVideoDecoder(
    const CDMVideoDecoderConfig& aConfig) override;
  ipc::IPCResult RecvDeinitializeVideoDecoder() override;
  ipc::IPCResult RecvResetVideoDecoder() override;
  ipc::IPCResult RecvDecryptAndDecodeFrame(
    const CDMInputBuffer& aBuffer) override;
  ipc::IPCResult RecvDestroy() override;

  GMPContentChild* mPlugin = nullptr;
  cdm::ContentDecryptionModule_8* mCDM = nullptr;
};

} // namespace gmp
} // namespace mozilla

#endif // ChromiumCDMChild_h_
