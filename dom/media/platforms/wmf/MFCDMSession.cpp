/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFCDMSession.h"

#include <limits>
#include <vcruntime.h>
#include <winerror.h>

#include "MFMediaEngineUtils.h"
#include "GMPUtils.h"  // ToHexString
#include "mozilla/EMEUtils.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"
#include "mozilla/dom/MediaKeyStatusMapBinding.h"
#include "nsThreadUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

#define LOG(msg, ...) EME_LOG("MFCDMSession=%p, " msg, this, ##__VA_ARGS__)

static inline MF_MEDIAKEYSESSION_TYPE ConvertSessionType(
    KeySystemConfig::SessionType aType) {
  switch (aType) {
    case KeySystemConfig::SessionType::Temporary:
      return MF_MEDIAKEYSESSION_TYPE_TEMPORARY;
    case KeySystemConfig::SessionType::PersistentLicense:
      return MF_MEDIAKEYSESSION_TYPE_PERSISTENT_LICENSE;
  }
}

static inline LPCWSTR InitDataTypeToString(const nsAString& aInitDataType) {
  // The strings are defined in https://www.w3.org/TR/eme-initdata-registry/
  if (aInitDataType.EqualsLiteral("webm")) {
    return L"webm";
  } else if (aInitDataType.EqualsLiteral("cenc")) {
    return L"cenc";
  } else if (aInitDataType.EqualsLiteral("keyids")) {
    return L"keyids";
  } else {
    return L"unknown";
  }
}

// The callback interface which IMFContentDecryptionModuleSession uses for
// communicating with the session.
class MFCDMSession::SessionCallbacks final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModuleSessionCallbacks> {
 public:
  SessionCallbacks() { MOZ_COUNT_CTOR(SessionCallbacks); };
  SessionCallbacks(const SessionCallbacks&) = delete;
  SessionCallbacks& operator=(const SessionCallbacks&) = delete;
  ~SessionCallbacks() { MOZ_COUNT_DTOR(SessionCallbacks); }

  HRESULT RuntimeClassInitialize() { return S_OK; }

  // IMFContentDecryptionModuleSessionCallbacks
  STDMETHODIMP KeyMessage(MF_MEDIAKEYSESSION_MESSAGETYPE aType,
                          const BYTE* aMessage, DWORD aMessageSize,
                          LPCWSTR aUrl) final {
    CopyableTArray<uint8_t> msg{static_cast<const uint8_t*>(aMessage),
                                aMessageSize};
    mKeyMessageEvent.Notify(aType, std::move(msg));
    return S_OK;
  }

  STDMETHODIMP KeyStatusChanged() final {
    mKeyChangeEvent.Notify();
    return S_OK;
  }

  MediaEventSource<MF_MEDIAKEYSESSION_MESSAGETYPE, nsTArray<uint8_t>>&
  KeyMessageEvent() {
    return mKeyMessageEvent;
  }
  MediaEventSource<void>& KeyChangeEvent() { return mKeyChangeEvent; }

 private:
  MediaEventProducer<MF_MEDIAKEYSESSION_MESSAGETYPE, nsTArray<uint8_t>>
      mKeyMessageEvent;
  MediaEventProducer<void> mKeyChangeEvent;
};

/* static*/
MFCDMSession* MFCDMSession::Create(KeySystemConfig::SessionType aSessionType,
                                   IMFContentDecryptionModule* aCdm,
                                   nsISerialEventTarget* aManagerThread) {
  MOZ_ASSERT(aCdm);
  MOZ_ASSERT(aManagerThread);
  ComPtr<SessionCallbacks> callbacks;
  RETURN_PARAM_IF_FAILED(MakeAndInitialize<SessionCallbacks>(&callbacks),
                         nullptr);

  ComPtr<IMFContentDecryptionModuleSession> session;
  RETURN_PARAM_IF_FAILED(aCdm->CreateSession(ConvertSessionType(aSessionType),
                                             callbacks.Get(), &session),
                         nullptr);
  return new MFCDMSession(session.Get(), callbacks.Get(), aManagerThread);
}

MFCDMSession::MFCDMSession(IMFContentDecryptionModuleSession* aSession,
                           SessionCallbacks* aCallback,
                           nsISerialEventTarget* aManagerThread)
    : mSession(aSession),
      mManagerThread(aManagerThread),
      mExpiredTimeMilliSecondsSinceEpoch(
          std::numeric_limits<double>::quiet_NaN()) {
  MOZ_ASSERT(aSession);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(aManagerThread);
  MOZ_COUNT_CTOR(MFCDMSession);
  LOG("MFCDMSession created");
  mKeyMessageListener = aCallback->KeyMessageEvent().Connect(
      mManagerThread, this, &MFCDMSession::OnSessionKeyMessage);
  mKeyChangeListener = aCallback->KeyChangeEvent().Connect(
      mManagerThread, this, &MFCDMSession::OnSessionKeysChange);
}

MFCDMSession::~MFCDMSession() {
  MOZ_COUNT_DTOR(MFCDMSession);
  LOG("MFCDMSession destroyed");
  // TODO : maybe disconnect them in `Close()`?
  mKeyChangeListener.DisconnectIfExists();
  mKeyMessageListener.DisconnectIfExists();
}

HRESULT MFCDMSession::GenerateRequest(const nsAString& aInitDataType,
                                      const uint8_t* aInitData,
                                      uint32_t aInitDataSize) {
  AssertOnManagerThread();
  LOG("GenerateRequest for %s (init sz=%u)",
      NS_ConvertUTF16toUTF8(aInitDataType).get(), aInitDataSize);
  RETURN_IF_FAILED(mSession->GenerateRequest(
      InitDataTypeToString(aInitDataType), aInitData, aInitDataSize));
  Unused << RetrieveSessionId();
  return S_OK;
}

HRESULT MFCDMSession::Load(const nsAString& aSessionId) {
  AssertOnManagerThread();
  // TODO : do we need to implement this? Chromium doesn't implement this one.
  // Also, how do we know is this given session ID is equal to the session Id
  // asked from CDM session or not?
  BOOL rv = FALSE;
  mSession->Load(char16ptr_t(aSessionId.BeginReading()), &rv);
  LOG("Load, id=%s, rv=%s", NS_ConvertUTF16toUTF8(aSessionId).get(),
      rv ? "success" : "fail");
  return rv ? S_OK : S_FALSE;
}

HRESULT MFCDMSession::Update(const nsTArray<uint8_t>& aMessage) {
  AssertOnManagerThread();
  LOG("Update");
  RETURN_IF_FAILED(mSession->Update(
      static_cast<const BYTE*>(aMessage.Elements()), aMessage.Length()));
  RETURN_IF_FAILED(UpdateExpirationIfNeeded());
  return S_OK;
}

HRESULT MFCDMSession::Close() {
  AssertOnManagerThread();
  LOG("Close");
  RETURN_IF_FAILED(mSession->Close());
  return S_OK;
}

HRESULT MFCDMSession::Remove() {
  AssertOnManagerThread();
  LOG("Remove");
  RETURN_IF_FAILED(mSession->Remove());
  RETURN_IF_FAILED(UpdateExpirationIfNeeded());
  return S_OK;
}

bool MFCDMSession::RetrieveSessionId() {
  AssertOnManagerThread();
  if (mSessionId) {
    return true;
  }
  ScopedCoMem<wchar_t> sessionId;
  if (FAILED(mSession->GetSessionId(&sessionId)) || !sessionId) {
    LOG("Can't get session id or empty session ID!");
    return false;
  }
  LOG("Set session Id %ls", sessionId.Get());
  mSessionId = Some(sessionId.Get());
  return true;
}

void MFCDMSession::OnSessionKeysChange() {
  AssertOnManagerThread();
  LOG("OnSessionKeysChange");

  if (!mSessionId) {
    LOG("Unexpected session keys change ignored");
    return;
  }

  ScopedCoMem<MFMediaKeyStatus> keyStatuses;
  UINT count = 0;
  RETURN_VOID_IF_FAILED(mSession->GetKeyStatuses(&keyStatuses, &count));

  static auto ToMediaKeyStatus = [](MF_MEDIAKEY_STATUS aStatus) {
    // https://learn.microsoft.com/en-us/windows/win32/api/mfidl/ne-mfidl-mf_mediakey_status
    switch (aStatus) {
      case MF_MEDIAKEY_STATUS_USABLE:
        return dom::MediaKeyStatus::Usable;
      case MF_MEDIAKEY_STATUS_EXPIRED:
        return dom::MediaKeyStatus::Expired;
      case MF_MEDIAKEY_STATUS_OUTPUT_DOWNSCALED:
        return dom::MediaKeyStatus::Output_downscaled;
      // This is for legacy use and should not happen in normal cases. Map it to
      // internal error in case it happens.
      case MF_MEDIAKEY_STATUS_OUTPUT_NOT_ALLOWED:
        return dom::MediaKeyStatus::Internal_error;
      case MF_MEDIAKEY_STATUS_STATUS_PENDING:
        return dom::MediaKeyStatus::Status_pending;
      case MF_MEDIAKEY_STATUS_INTERNAL_ERROR:
        return dom::MediaKeyStatus::Internal_error;
      case MF_MEDIAKEY_STATUS_RELEASED:
        return dom::MediaKeyStatus::Released;
      case MF_MEDIAKEY_STATUS_OUTPUT_RESTRICTED:
        return dom::MediaKeyStatus::Output_restricted;
    }
    MOZ_ASSERT_UNREACHABLE("Invalid MF_MEDIAKEY_STATUS enum value");
    return dom::MediaKeyStatus::Internal_error;
  };

  CopyableTArray<MFCDMKeyInformation> keyInfos;
  for (uint32_t idx = 0; idx < count; idx++) {
    const MFMediaKeyStatus& keyStatus = keyStatuses[idx];
    if (keyStatus.cbKeyId != sizeof(GUID)) {
      LOG("Key ID with unsupported size ignored");
      continue;
    }
    CopyableTArray<uint8_t> keyId;
    ByteArrayFromGUID(reinterpret_cast<REFGUID>(keyStatus.pbKeyId), keyId);

    nsAutoCString keyIdString(ToHexString(keyId));
    LOG("Append keyid-sz=%u, keyid=%s, status=%s", keyStatus.cbKeyId,
        keyIdString.get(),
        dom::GetEnumString(ToMediaKeyStatus(keyStatus.eMediaKeyStatus)).get());
    keyInfos.AppendElement(MFCDMKeyInformation{
        std::move(keyId), ToMediaKeyStatus(keyStatus.eMediaKeyStatus)});
  }
  LOG("Notify 'keychange' for %s", NS_ConvertUTF16toUTF8(*mSessionId).get());
  mKeyChangeEvent.Notify(
      MFCDMKeyStatusChange{*mSessionId, std::move(keyInfos)});

  // ScopedCoMem<MFMediaKeyStatus> only releases memory for |keyStatuses|. We
  // need to manually release memory for |pbKeyId| here.
  for (size_t idx = 0; idx < count; idx++) {
    if (const auto& keyStatus = keyStatuses[idx]; keyStatus.pbKeyId) {
      CoTaskMemFree(keyStatus.pbKeyId);
    }
  }
}

HRESULT MFCDMSession::UpdateExpirationIfNeeded() {
  AssertOnManagerThread();
  MOZ_ASSERT(mSessionId);

  // The msdn document doesn't mention the unit for the expiration time,
  // follow chromium's implementation to treat them as millisecond.
  double newExpiredEpochTimeMs = 0.0;
  RETURN_IF_FAILED(mSession->GetExpiration(&newExpiredEpochTimeMs));

  if (newExpiredEpochTimeMs == mExpiredTimeMilliSecondsSinceEpoch ||
      (std::isnan(newExpiredEpochTimeMs) &&
       std::isnan(mExpiredTimeMilliSecondsSinceEpoch))) {
    return S_OK;
  }

  LOG("Session expiration change from %f to %f, notify 'expiration' for %s",
      mExpiredTimeMilliSecondsSinceEpoch, newExpiredEpochTimeMs,
      NS_ConvertUTF16toUTF8(*mSessionId).get());
  mExpiredTimeMilliSecondsSinceEpoch = newExpiredEpochTimeMs;
  mExpirationEvent.Notify(
      MFCDMKeyExpiration{*mSessionId, mExpiredTimeMilliSecondsSinceEpoch});
  return S_OK;
}

void MFCDMSession::OnSessionKeyMessage(
    const MF_MEDIAKEYSESSION_MESSAGETYPE& aType,
    const nsTArray<uint8_t>& aMessage) {
  AssertOnManagerThread();
  // Only send key message after the session Id is ready.
  if (!RetrieveSessionId()) {
    return;
  }
  static auto ToMediaKeyMessageType = [](MF_MEDIAKEYSESSION_MESSAGETYPE aType) {
    switch (aType) {
      case MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_REQUEST:
        return dom::MediaKeyMessageType::License_request;
      case MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RENEWAL:
        return dom::MediaKeyMessageType::License_renewal;
      case MF_MEDIAKEYSESSION_MESSAGETYPE_LICENSE_RELEASE:
        return dom::MediaKeyMessageType::License_release;
      case MF_MEDIAKEYSESSION_MESSAGETYPE_INDIVIDUALIZATION_REQUEST:
        return dom::MediaKeyMessageType::Individualization_request;
      default:
        MOZ_CRASH("Unknown session message type");
    }
  };
  LOG("Notify 'keymessage' for %s", NS_ConvertUTF16toUTF8(*mSessionId).get());
  mKeyMessageEvent.Notify(MFCDMKeyMessage{
      *mSessionId, ToMediaKeyMessageType(aType), std::move(aMessage)});
}

#undef LOG

}  // namespace mozilla
