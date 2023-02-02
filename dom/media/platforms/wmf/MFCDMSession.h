/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORM_WMF_MFCDMSESSION_H
#define DOM_MEDIA_PLATFORM_WMF_MFCDMSESSION_H

#include <vector>
#include <wrl.h>
#include <wrl/client.h>

#include "MFCDMExtra.h"
#include "MediaEventSource.h"
#include "nsAString.h"

namespace mozilla {

// TODO : remove this and use the ones in KeySystemConfig.h after landing
// 1810817.
enum class SessionType {
  Temporary = 1,
  PersistentLicense = 2,
};

// TODO: declare them in the ipdl so that we can send them over IPC directly.
// Also, refine them if needed because now I am not sure what exact information
// caller expects to see from these events.
struct KeyMessageInfo {
  KeyMessageInfo(MF_MEDIAKEYSESSION_MESSAGETYPE aType, const BYTE* aMessage,
                 DWORD aMessageSize)
      : mMessageType(aType), mMessage(aMessage, aMessage + aMessageSize) {}
  const MF_MEDIAKEYSESSION_MESSAGETYPE mMessageType;
  const std::vector<uint8_t> mMessage;
};

struct ExpirationInfo {
  ExpirationInfo(const nsString& aSessionId, double aExpiredTime)
      : mSessionId(aSessionId),
        mExpiredTimeMilliSecondsSinceEpoch(aExpiredTime) {}
  const nsString mSessionId;
  const double mExpiredTimeMilliSecondsSinceEpoch;
};

struct KeyInfo {
  CopyableTArray<uint8_t> mKeyId;
  uint32_t mKeyStatus;
};

// MFCDMSession represents a key session defined by the EME spec, it operates
// the IMFContentDecryptionModuleSession directly and forward events from
// IMFContentDecryptionModuleSession to its caller. It's not thread-safe and
// can only be used on the manager thread for now.
class MFCDMSession final {
 public:
  ~MFCDMSession();

  static MFCDMSession* Create(SessionType aSessionType,
                              IMFContentDecryptionModule* aCdm,
                              nsISerialEventTarget* aManagerThread);

  // APIs corresponding to EME APIs (MediaKeySession)
  HRESULT GenerateRequest(const nsAString& aInitDataType,
                          const uint8_t* aInitData, uint32_t aInitDataSize);
  HRESULT Load(const nsAString& aSessionId);
  HRESULT Update(const nsTArray<uint8_t>& aMessage);
  HRESULT Close();
  HRESULT Remove();

  // Session status related events
  MediaEventSource<KeyMessageInfo>& KeyMessageEvent() {
    return mKeyMessageEvent;
  }
  MediaEventSource<CopyableTArray<KeyInfo>>& KeyChangeEvent() {
    return mKeyChangeEvent;
  }
  MediaEventSource<ExpirationInfo>& ExpirationEvent() {
    return mExpirationEvent;
  }

  const Maybe<nsString>& SessionID() const { return mSessionId; }

 private:
  class SessionCallbacks;

  MFCDMSession(IMFContentDecryptionModuleSession* aSession,
               SessionCallbacks* aCallback,
               nsISerialEventTarget* aManagerThread);
  MFCDMSession(const MFCDMSession&) = delete;
  MFCDMSession& operator=(const MFCDMSession&) = delete;

  bool RetrieveSessionId();
  void OnSessionKeysChange();

  HRESULT UpdateExpirationIfNeeded();

  void AssertOnManagerThread() const {
    MOZ_ASSERT(mManagerThread->IsOnCurrentThread());
  }

  const Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSession> mSession;
  const nsCOMPtr<nsISerialEventTarget> mManagerThread;

  MediaEventForwarder<KeyMessageInfo> mKeyMessageEvent;
  MediaEventProducer<CopyableTArray<KeyInfo>> mKeyChangeEvent;
  MediaEventProducer<ExpirationInfo> mExpirationEvent;
  MediaEventListener mKeyChangeListener;

  // IMFContentDecryptionModuleSession's id might not be ready immediately after
  // the session gets created.
  Maybe<nsString> mSessionId;

  double mExpiredTimeMilliSecondsSinceEpoch = 0.0;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFCDMSESSION_H
