/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDrmProxySupport.h"
#include "mozilla/EMEUtils.h"
#include "FennecJNINatives.h"
#include "MediaCodec.h" // For MediaDrm::KeyStatus
#include "MediaPrefs.h"

using namespace mozilla::java;

namespace mozilla {

LogModule* GetMDRMNLog() {
  static LazyLogModule log("MediaDrmProxySupport");
  return log;
}

class MediaDrmJavaCallbacksSupport
  : public MediaDrmProxy::NativeMediaDrmProxyCallbacks::Natives<MediaDrmJavaCallbacksSupport>
{
public:
  typedef MediaDrmProxy::NativeMediaDrmProxyCallbacks::Natives<MediaDrmJavaCallbacksSupport> MediaDrmProxyNativeCallbacks;
  using MediaDrmProxyNativeCallbacks::DisposeNative;
  using MediaDrmProxyNativeCallbacks::AttachNative;

  MediaDrmJavaCallbacksSupport(DecryptorProxyCallback* aDecryptorProxyCallback)
    : mDecryptorProxyCallback(aDecryptorProxyCallback)
  {
    MOZ_ASSERT(aDecryptorProxyCallback);
  }
  /*
   * Native implementation, called by Java.
   */
  void OnSessionCreated(int aCreateSessionToken,
                        int aPromiseId,
                        jni::ByteArray::Param aSessionId,
                        jni::ByteArray::Param aRequest);

  void OnSessionUpdated(int aPromiseId, jni::ByteArray::Param aSessionId);

  void OnSessionClosed(int aPromiseId, jni::ByteArray::Param aSessionId);

  void OnSessionMessage(jni::ByteArray::Param aSessionId,
                        int /*mozilla::dom::MediaKeyMessageType*/ aSessionMessageType,
                        jni::ByteArray::Param aRequest);

  void OnSessionError(jni::ByteArray::Param aSessionId,
                      jni::String::Param aMessage);

  void OnSessionBatchedKeyChanged(jni::ByteArray::Param,
                                  jni::ObjectArray::Param);

  void OnRejectPromise(int aPromiseId, jni::String::Param aMessage);

private:
  DecryptorProxyCallback* mDecryptorProxyCallback;
}; // MediaDrmJavaCallbacksSupport

void
MediaDrmJavaCallbacksSupport::OnSessionCreated(int aCreateSessionToken,
                                               int aPromiseId,
                                               jni::ByteArray::Param aSessionId,
                                               jni::ByteArray::Param aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  auto reqDataArray = aRequest->GetElements();
  nsCString sessionId(reinterpret_cast<char*>(aSessionId->GetElements().Elements()),
                      aSessionId->Length());
  MDRMN_LOG("SessionId(%s) closed", sessionId.get());

  mDecryptorProxyCallback->SetSessionId(aCreateSessionToken, sessionId);
  mDecryptorProxyCallback->ResolvePromise(aPromiseId);
}

void
MediaDrmJavaCallbacksSupport::OnSessionUpdated(int aPromiseId,
                                               jni::ByteArray::Param aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  MDRMN_LOG("SessionId(%s) closed",
            nsCString(reinterpret_cast<char*>(aSessionId->GetElements().Elements()),
                      aSessionId->Length()).get());
  mDecryptorProxyCallback->ResolvePromise(aPromiseId);
}

void
MediaDrmJavaCallbacksSupport::OnSessionClosed(int aPromiseId,
                                              jni::ByteArray::Param aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCString sessionId(reinterpret_cast<char*>(aSessionId->GetElements().Elements()),
                      aSessionId->Length());
  MDRMN_LOG("SessionId(%s) closed", sessionId.get());
  mDecryptorProxyCallback->ResolvePromise(aPromiseId);
  mDecryptorProxyCallback->SessionClosed(sessionId);
}

void
MediaDrmJavaCallbacksSupport::OnSessionMessage(jni::ByteArray::Param aSessionId,
                                               int /*mozilla::dom::MediaKeyMessageType*/ aMessageType,
                                               jni::ByteArray::Param aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCString sessionId(reinterpret_cast<char*>(aSessionId->GetElements().Elements()),
                      aSessionId->Length());
  auto reqDataArray = aRequest->GetElements();

  nsTArray<uint8_t> retRequest;
  retRequest.AppendElements(reinterpret_cast<uint8_t*>(reqDataArray.Elements()),
                            reqDataArray.Length());

  mDecryptorProxyCallback->SessionMessage(sessionId,
                                          static_cast<dom::MediaKeyMessageType>(aMessageType),
                                          retRequest);
}

void
MediaDrmJavaCallbacksSupport::OnSessionError(jni::ByteArray::Param aSessionId,
                                             jni::String::Param aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCString sessionId(reinterpret_cast<char*>(aSessionId->GetElements().Elements()),
                      aSessionId->Length());
  nsCString errorMessage = aMessage->ToCString();
  MDRMN_LOG("SessionId(%s)", sessionId.get());
  // TODO: We cannot get system error code from media drm API.
  // Currently use -1 as an error code.
  mDecryptorProxyCallback->SessionError(sessionId,
                                        NS_ERROR_DOM_INVALID_STATE_ERR,
                                        -1,
                                        errorMessage);
}

// TODO: MediaDrm.KeyStatus defined the status code not included
// dom::MediaKeyStatus::Released and dom::MediaKeyStatus::Output_downscaled.
// Should keep tracking for this if it will be changed in the future.
static dom::MediaKeyStatus
MediaDrmKeyStatusToMediaKeyStatus(int aStatusCode)
{
  using mozilla::java::sdk::KeyStatus;
  switch (aStatusCode) {
    case KeyStatus::STATUS_USABLE: return dom::MediaKeyStatus::Usable;
    case KeyStatus::STATUS_EXPIRED: return dom::MediaKeyStatus::Expired;
    case KeyStatus::STATUS_OUTPUT_NOT_ALLOWED: return dom::MediaKeyStatus::Output_restricted;
    case KeyStatus::STATUS_INTERNAL_ERROR: return dom::MediaKeyStatus::Internal_error;
    case KeyStatus::STATUS_PENDING: return dom::MediaKeyStatus::Status_pending;
    default: return dom::MediaKeyStatus::Internal_error;
  }
}

void
MediaDrmJavaCallbacksSupport::OnSessionBatchedKeyChanged(jni::ByteArray::Param aSessionId,
                                                         jni::ObjectArray::Param aKeyInfos)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCString sessionId(reinterpret_cast<char*>(aSessionId->GetElements().Elements()),
                      aSessionId->Length());
  nsTArray<jni::Object::LocalRef> keyInfosObjectArray(aKeyInfos->GetElements());

  nsTArray<CDMKeyInfo> keyInfosArray;

  for (auto&& keyInfoObject : keyInfosObjectArray) {
    java::SessionKeyInfo::LocalRef keyInfo(mozilla::Move(keyInfoObject));
    mozilla::jni::ByteArray::LocalRef keyIdByteArray = keyInfo->KeyId();
    nsTArray<int8_t> keyIdInt8Array = keyIdByteArray->GetElements();
    // Cast nsTArray<int8_t> to nsTArray<uint8_t>
    nsTArray<uint8_t>* keyId = reinterpret_cast<nsTArray<uint8_t>*>(&keyIdInt8Array);
    auto keyStatus = keyInfo->Status(); // int32_t
    keyInfosArray.AppendElement(CDMKeyInfo(*keyId,
                                           dom::Optional<dom::MediaKeyStatus>(
                                             MediaDrmKeyStatusToMediaKeyStatus(keyStatus)
                                                                             )
                                          )
                               );
  }

  mDecryptorProxyCallback->BatchedKeyStatusChanged(sessionId,
                                                   keyInfosArray);
}

void
MediaDrmJavaCallbacksSupport::OnRejectPromise(int aPromiseId, jni::String::Param aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsCString reason = aMessage->ToCString();
  MDRMN_LOG("OnRejectPromise aMessage(%s) ", reason.get());
  // Current implementation assume all the reject from MediaDrm is due to invalid state.
  // Other cases should be handled before calling into MediaDrmProxy API.
  mDecryptorProxyCallback->RejectPromise(aPromiseId,
                                         NS_ERROR_DOM_INVALID_STATE_ERR,
                                         reason);
}

MediaDrmProxySupport::MediaDrmProxySupport(const nsAString& aKeySystem)
  : mKeySystem(aKeySystem), mDestroyed(false)
{
  mJavaCallbacks = MediaDrmProxy::NativeMediaDrmProxyCallbacks::New();

  mBridgeProxy =
    MediaDrmProxy::Create(mKeySystem,
                          mJavaCallbacks,
                          MediaPrefs::PDMAndroidRemoteCodecEnabled());

  MOZ_ASSERT(mBridgeProxy, "mBridgeProxy should not be null");
  mMediaDrmStubId = mBridgeProxy->GetStubId()->ToString();
}

MediaDrmProxySupport::~MediaDrmProxySupport()
{
  MOZ_ASSERT(mDestroyed, "Shutdown() should be called before !!");
  MediaDrmJavaCallbacksSupport::DisposeNative(mJavaCallbacks);
}

nsresult
MediaDrmProxySupport::Init(DecryptorProxyCallback* aCallback)
{
  MOZ_ASSERT(mJavaCallbacks);

  mCallback = aCallback;
  MediaDrmJavaCallbacksSupport::AttachNative(mJavaCallbacks,
                                             mozilla::MakeUnique<MediaDrmJavaCallbacksSupport>(mCallback));
  return mBridgeProxy != nullptr ? NS_OK : NS_ERROR_FAILURE;
}

void
MediaDrmProxySupport::CreateSession(uint32_t aCreateSessionToken,
                                    uint32_t aPromiseId,
                                    const nsCString& aInitDataType,
                                    const nsTArray<uint8_t>& aInitData,
                                    MediaDrmSessionType aSessionType)
{
  MOZ_ASSERT(mBridgeProxy);

  auto initDataBytes =
    mozilla::jni::ByteArray::New(reinterpret_cast<const int8_t*>(&aInitData[0]),
                                 aInitData.Length());
  // TODO: aSessionType is not used here.
  // Refer to
  // http://androidxref.com/5.1.1_r6/xref/external/chromium_org/media/base/android/java/src/org/chromium/media/MediaDrmBridge.java#420
  // it is hard code to streaming type.
  mBridgeProxy->CreateSession(aCreateSessionToken,
                              aPromiseId,
                              NS_ConvertUTF8toUTF16(aInitDataType),
                              initDataBytes);
}

void
MediaDrmProxySupport::UpdateSession(uint32_t aPromiseId,
                                    const nsCString& aSessionId,
                                    const nsTArray<uint8_t>& aResponse)
{
  MOZ_ASSERT(mBridgeProxy);

  auto response =
    mozilla::jni::ByteArray::New(reinterpret_cast<const int8_t*>(aResponse.Elements()),
                                 aResponse.Length());
  mBridgeProxy->UpdateSession(aPromiseId,
                              NS_ConvertUTF8toUTF16(aSessionId),
                              response);
}

void
MediaDrmProxySupport::CloseSession(uint32_t aPromiseId,
                                   const nsCString& aSessionId)
{
  MOZ_ASSERT(mBridgeProxy);

  mBridgeProxy->CloseSession(aPromiseId, NS_ConvertUTF8toUTF16(aSessionId));
}

void
MediaDrmProxySupport::Shutdown()
{
  MOZ_ASSERT(mBridgeProxy);

  if (mDestroyed) {
    return;
  }
  mBridgeProxy->Destroy();
  mDestroyed = true;
}

} // namespace mozilla
