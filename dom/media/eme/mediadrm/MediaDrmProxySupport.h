/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDrmProxySupport_H
#define MediaDrmProxySupport_H

#include "mozilla/DecryptorProxyCallback.h"
#include "mozilla/java/MediaDrmProxyWrappers.h"
#include "mozilla/Logging.h"
#include "mozilla/UniquePtr.h"
#include "nsString.h"

namespace mozilla {

enum MediaDrmSessionType {
  kKeyStreaming = 1,
  kKeyOffline = 2,
  kKeyRelease = 3,
};

#ifndef MDRMN_LOG
LogModule* GetMDRMNLog();
#  define MDRMN_LOG(x, ...)                          \
    MOZ_LOG(GetMDRMNLog(), mozilla::LogLevel::Debug, \
            ("[MediaDrmProxySupport][%s]" x, __FUNCTION__, ##__VA_ARGS__))
#endif

class MediaDrmCDMCallbackProxy;

class MediaDrmProxySupport final {
 public:
  explicit MediaDrmProxySupport(const nsAString& aKeySystem);
  ~MediaDrmProxySupport();

  /*
   * APIs to act as GMPDecryptorAPI, discarding unnecessary calls.
   */
  nsresult Init(UniquePtr<MediaDrmCDMCallbackProxy>&& aCallback);

  void CreateSession(uint32_t aCreateSessionToken, uint32_t aPromiseId,
                     const nsCString& aInitDataType,
                     const nsTArray<uint8_t>& aInitData,
                     MediaDrmSessionType aSessionType);

  void UpdateSession(uint32_t aPromiseId, const nsCString& aSessionId,
                     const nsTArray<uint8_t>& aResponse);

  void CloseSession(uint32_t aPromiseId, const nsCString& aSessionId);

  void Shutdown();

  const nsString& GetMediaDrmStubId() const { return mMediaDrmStubId; }

  bool SetServerCertificate(const nsTArray<uint8_t>& aCert);

 private:
  const nsString mKeySystem;
  java::MediaDrmProxy::GlobalRef mBridgeProxy;
  java::MediaDrmProxy::NativeMediaDrmProxyCallbacks::GlobalRef mJavaCallbacks;
  bool mDestroyed;
  nsString mMediaDrmStubId;
};

}  // namespace mozilla
#endif  // MediaDrmProxySupport_H
