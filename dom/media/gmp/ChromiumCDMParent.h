/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMParent_h_
#define ChromiumCDMParent_h_

#include "GMPCrashHelper.h"
#include "GMPCrashHelperHolder.h"
#include "GMPMessageUtils.h"
#include "mozilla/gmp/PChromiumCDMParent.h"
#include "mozilla/RefPtr.h"

namespace mozilla {

class ChromiumCDMProxy;

namespace gmp {

class GMPContentParent;

class ChromiumCDMParent final
  : public PChromiumCDMParent
  , public GMPCrashHelperHolder
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChromiumCDMParent)

  ChromiumCDMParent(GMPContentParent* aContentParent, uint32_t aPluginId);

  uint32_t PluginId() const { return mPluginId; }

  bool Init(ChromiumCDMProxy* aProxy,
            bool aAllowDistinctiveIdentifier,
            bool aAllowPersistentState);

  // TODO: Add functions for clients to send data to CDM, and
  // a Close() function.

protected:
  ~ChromiumCDMParent() {}

  ipc::IPCResult Recv__delete__() override;
  ipc::IPCResult RecvOnResolveNewSessionPromise(
    const uint32_t& aPromiseId,
    const nsCString& aSessionId) override;
  ipc::IPCResult RecvOnResolvePromise(const uint32_t& aPromiseId) override;
  ipc::IPCResult RecvOnRejectPromise(const uint32_t& aPromiseId,
                                     const uint32_t& aError,
                                     const uint32_t& aSystemCode,
                                     const nsCString& aErrorMessage) override;
  ipc::IPCResult RecvOnSessionMessage(const nsCString& aSessionId,
                                      const uint32_t& aMessageType,
                                      nsTArray<uint8_t>&& aMessage) override;
  ipc::IPCResult RecvOnSessionKeysChange(
    const nsCString& aSessionId,
    nsTArray<CDMKeyInformation>&& aKeysInfo) override;
  ipc::IPCResult RecvOnExpirationChange(
    const nsCString& aSessionId,
    const double& aSecondsSinceEpoch) override;
  ipc::IPCResult RecvOnSessionClosed(const nsCString& aSessionId) override;
  ipc::IPCResult RecvOnLegacySessionError(const nsCString& aSessionId,
                                          const uint32_t& aError,
                                          const uint32_t& aSystemCode,
                                          const nsCString& aMessage) override;
  ipc::IPCResult RecvDecrypted(const uint32_t& aStatus,
                               nsTArray<uint8_t>&& aData) override;
  ipc::IPCResult RecvDecoded(const CDMVideoFrame& aFrame) override;
  ipc::IPCResult RecvDecodeFailed(const uint32_t& aStatus) override;
  ipc::IPCResult RecvShutdown() override;
  void ActorDestroy(ActorDestroyReason aWhy) override;

  const uint32_t mPluginId;
  GMPContentParent* mContentParent;
  // Note: this pointer is a weak reference because otherwise it would cause
  // a cycle, as ChromiumCDMProxy has a strong reference to the
  // ChromiumCDMParent.
  ChromiumCDMProxy* mProxy = nullptr;
};

} // namespace gmp
} // namespace mozilla

#endif // ChromiumCDMParent_h_
