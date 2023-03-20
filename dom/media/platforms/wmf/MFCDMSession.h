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
#include "mozilla/PMFCDM.h"
#include "mozilla/KeySystemConfig.h"
#include "nsAString.h"

namespace mozilla {

// MFCDMSession represents a key session defined by the EME spec, it operates
// the IMFContentDecryptionModuleSession directly and forward events from
// IMFContentDecryptionModuleSession to its caller. It's not thread-safe and
// can only be used on the manager thread for now.
class MFCDMSession final {
 public:
  ~MFCDMSession();

  static MFCDMSession* Create(KeySystemConfig::SessionType aSessionType,
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
  MediaEventSource<MFCDMKeyMessage>& KeyMessageEvent() {
    return mKeyMessageEvent;
  }
  MediaEventSource<MFCDMKeyStatusChange>& KeyChangeEvent() {
    return mKeyChangeEvent;
  }
  MediaEventSource<MFCDMKeyExpiration>& ExpirationEvent() {
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
  void OnSessionKeyMessage(const MF_MEDIAKEYSESSION_MESSAGETYPE& aType,
                           const nsTArray<uint8_t>& aMessage);

  HRESULT UpdateExpirationIfNeeded();

  void AssertOnManagerThread() const {
    MOZ_ASSERT(mManagerThread->IsOnCurrentThread());
  }

  const Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSession> mSession;
  const nsCOMPtr<nsISerialEventTarget> mManagerThread;

  MediaEventProducer<MFCDMKeyMessage> mKeyMessageEvent;
  MediaEventProducer<MFCDMKeyStatusChange> mKeyChangeEvent;
  MediaEventProducer<MFCDMKeyExpiration> mExpirationEvent;
  MediaEventListener mKeyMessageListener;
  MediaEventListener mKeyChangeListener;

  // IMFContentDecryptionModuleSession's id might not be ready immediately after
  // the session gets created.
  Maybe<nsString> mSessionId;

  // NaN when the CDM doesn't explicitly define the time or the time never
  // expires.
  double mExpiredTimeMilliSecondsSinceEpoch;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORM_WMF_MFCDMSESSION_H
