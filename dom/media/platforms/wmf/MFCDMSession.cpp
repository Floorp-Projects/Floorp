/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFCDMSession.h"
#include <vcruntime.h>
#include <winerror.h>

#include "MFMediaEngineUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

#define LOG(msg, ...)                         \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug, \
          ("MFCDMSession=%p, " msg, this, ##__VA_ARGS__))

static inline MF_MEDIAKEYSESSION_TYPE ConvertSessionType(SessionType aType) {
  switch (aType) {
    case SessionType::Temporary:
      return MF_MEDIAKEYSESSION_TYPE_TEMPORARY;
    case SessionType::PersistentLicense:
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

// TODO : define a status type in ipdl in order to pass an enum over IPC, not a
// uint.
static uint32_t ToKeyStatus(MF_MEDIAKEY_STATUS aStatus) {
  // https://learn.microsoft.com/en-us/windows/win32/api/mfidl/ne-mfidl-mf_mediakey_status
  switch (aStatus) {
    case MF_MEDIAKEY_STATUS_USABLE:
      return 0;
    case MF_MEDIAKEY_STATUS_EXPIRED:
      return 1;
    case MF_MEDIAKEY_STATUS_OUTPUT_DOWNSCALED:
      return 2;
    // TODO : This is for legacy use and should not happen in normal cases. Map
    // it to internal error in case it happens.
    case MF_MEDIAKEY_STATUS_OUTPUT_NOT_ALLOWED:
      return 3;
    case MF_MEDIAKEY_STATUS_STATUS_PENDING:
      return 4;
    case MF_MEDIAKEY_STATUS_INTERNAL_ERROR:
      return 5;
    case MF_MEDIAKEY_STATUS_RELEASED:
      return 6;
    case MF_MEDIAKEY_STATUS_OUTPUT_RESTRICTED:
      return 7;
  }
  MOZ_ASSERT_UNREACHABLE("Invalid MF_MEDIAKEY_STATUS enum value");
  return 5;
}

void ByteArrayFromGUID(REFGUID aGuid, nsTArray<uint8_t>& aByteArrayOut) {
  aByteArrayOut.SetCapacity(sizeof(GUID));
  // GUID is little endian. The byte array in network order is big endian.
  GUID* reversedGuid = reinterpret_cast<GUID*>(aByteArrayOut.Elements());
  *reversedGuid = aGuid;
  reversedGuid->Data1 = _byteswap_ulong(aGuid.Data1);
  reversedGuid->Data2 = _byteswap_ushort(aGuid.Data2);
  reversedGuid->Data3 = _byteswap_ushort(aGuid.Data3);
  // Data4 is already a byte array so no need to byte swap.
}

// The callback interface which IMFContentDecryptionModuleSession uses for
// communicating with the session.
class MFCDMSession::SessionCallbacks final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModuleSessionCallbacks> {
 public:
  SessionCallbacks() = default;
  SessionCallbacks(const SessionCallbacks&) = delete;
  SessionCallbacks& operator=(const SessionCallbacks&) = delete;
  ~SessionCallbacks() = default;

  HRESULT RuntimeClassInitialize() { return S_OK; }

  // IMFContentDecryptionModuleSessionCallbacks
  STDMETHODIMP KeyMessage(MF_MEDIAKEYSESSION_MESSAGETYPE aType,
                          const BYTE* aMessage, DWORD aMessageSize,
                          LPCWSTR aUrl) final {
    mKeyMessageEvent.Notify(KeyMessageInfo{aType, aMessage, aMessageSize});
    return S_OK;
  }

  STDMETHODIMP KeyStatusChanged() final {
    mKeyChangeEvent.Notify();
    return S_OK;
  }

  MediaEventSource<KeyMessageInfo>& KeyMessageEvent() {
    return mKeyMessageEvent;
  }
  MediaEventSource<void>& KeyChangeEvent() { return mKeyChangeEvent; }

 private:
  MediaEventProducer<KeyMessageInfo> mKeyMessageEvent;
  MediaEventProducer<void> mKeyChangeEvent;
};

/* static*/
MFCDMSession* MFCDMSession::Create(SessionType aSessionType,
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
      mKeyMessageEvent(mManagerThread) {
  MOZ_ASSERT(aSession);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(aManagerThread);
  // It's ok to capture `this` here because the forwarder will be disconnected
  // before this class get destroyed, which means `this` is always valid inside
  // lambda.
  mKeyMessageEvent.ForwardIf(aCallback->KeyMessageEvent(),
                             [this]() { return RetrieveSessionId(); });
  mKeyChangeListener = aCallback->KeyChangeEvent().Connect(
      mManagerThread, [this]() { OnSessionKeysChange(); });
}

MFCDMSession::~MFCDMSession() {
  // TODO : maybe disconnect them in `Close()`?
  mKeyMessageEvent.DisconnectAll();
  mKeyChangeListener.DisconnectIfExists();
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

  CopyableTArray<KeyInfo> keyInfos;
  keyInfos.SetCapacity(count);
  for (uint32_t idx = 0; idx < count; idx++) {
    const MFMediaKeyStatus& keyStatus = keyStatuses[idx];
    if (keyStatus.cbKeyId != sizeof(GUID)) {
      LOG("Key ID with unsupported size ignored");
      continue;
    }
    KeyInfo* info = keyInfos.AppendElement();
    ByteArrayFromGUID(reinterpret_cast<REFGUID>(keyStatus.pbKeyId),
                      info->mKeyId);
    info->mKeyStatus = ToKeyStatus(keyStatus.eMediaKeyStatus);
  }
  mKeyChangeEvent.Notify(std::move(keyInfos));

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

  if (newExpiredEpochTimeMs == mExpiredTimeMilliSecondsSinceEpoch) {
    return S_OK;
  }

  LOG("Session expiration change from %f to %f",
      mExpiredTimeMilliSecondsSinceEpoch, newExpiredEpochTimeMs);
  mExpiredTimeMilliSecondsSinceEpoch = newExpiredEpochTimeMs;
  mExpirationEvent.Notify(
      ExpirationInfo{*mSessionId, mExpiredTimeMilliSecondsSinceEpoch});
  return S_OK;
}

#undef LOG

}  // namespace mozilla
