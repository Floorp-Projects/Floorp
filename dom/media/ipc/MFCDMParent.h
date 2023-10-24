/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_IPC_MFCDMPARENT_H_
#define DOM_MEDIA_IPC_MFCDMPARENT_H_

#include <wrl.h>

#include "mozilla/Assertions.h"
#include "mozilla/PMFCDMParent.h"
#include "MFCDMExtra.h"
#include "MFCDMSession.h"
#include "MFPMPHostWrapper.h"
#include "RemoteDecoderManagerParent.h"

namespace mozilla {

class MFCDMProxy;

/**
 * MFCDMParent is a wrapper class for the Media Foundation CDM in the utility
 * process.
 * It's responsible to create and manage a CDM and its sessions, and acts as a
 * proxy to the Media Foundation interfaces
 * (https://learn.microsoft.com/en-us/windows/win32/api/mfcontentdecryptionmodule/)
 * by accepting calls from and calling back to MFCDMChild in the content
 * process.
 */
class MFCDMParent final : public PMFCDMParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MFCDMParent);

  MFCDMParent(const nsAString& aKeySystem, RemoteDecoderManagerParent* aManager,
              nsISerialEventTarget* aManagerThread);

  static MFCDMParent* GetCDMById(uint64_t aId) {
    MOZ_ASSERT(sRegisteredCDMs.Contains(aId));
    return sRegisteredCDMs.Get(aId);
  }
  uint64_t Id() const { return mId; }

  mozilla::ipc::IPCResult RecvGetCapabilities(
      const bool aIsHWSecured, GetCapabilitiesResolver&& aResolver);

  mozilla::ipc::IPCResult RecvInit(const MFCDMInitParamsIPDL& aParams,
                                   InitResolver&& aResolver);

  mozilla::ipc::IPCResult RecvCreateSessionAndGenerateRequest(
      const MFCDMCreateSessionParamsIPDL& aParams,
      CreateSessionAndGenerateRequestResolver&& aResolver);

  mozilla::ipc::IPCResult RecvLoadSession(
      const KeySystemConfig::SessionType& aSessionType,
      const nsString& aSessionId, LoadSessionResolver&& aResolver);

  mozilla::ipc::IPCResult RecvUpdateSession(
      const nsString& aSessionId, const CopyableTArray<uint8_t>& aResponse,
      UpdateSessionResolver&& aResolver);

  mozilla::ipc::IPCResult RecvCloseSession(const nsString& aSessionId,
                                           UpdateSessionResolver&& aResolver);

  mozilla::ipc::IPCResult RecvRemoveSession(const nsString& aSessionId,
                                            UpdateSessionResolver&& aResolver);

  mozilla::ipc::IPCResult RecvSetServerCertificate(
      const CopyableTArray<uint8_t>& aCertificate,
      UpdateSessionResolver&& aResolver);

  nsISerialEventTarget* ManagerThread() { return mManagerThread; }
  void AssertOnManagerThread() const {
    MOZ_ASSERT(mManagerThread->IsOnCurrentThread());
  }

  already_AddRefed<MFCDMProxy> GetMFCDMProxy();

  void ShutdownCDM();

  void Destroy();

 private:
  ~MFCDMParent();

  LPCWSTR GetCDMLibraryName() const;

  HRESULT LoadFactory();

  void Register();
  void Unregister();

  void ConnectSessionEvents(MFCDMSession* aSession);

  MFCDMSession* GetSession(const nsString& aSessionId);

  nsString mKeySystem;

  const RefPtr<RemoteDecoderManagerParent> mManager;
  const RefPtr<nsISerialEventTarget> mManagerThread;

  static inline nsTHashMap<nsUint64HashKey, MFCDMParent*> sRegisteredCDMs;

  static inline uint64_t sNextId = 1;
  const uint64_t mId;

  RefPtr<MFCDMParent> mIPDLSelfRef;
  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory> mFactory;
  Microsoft::WRL::ComPtr<IMFContentDecryptionModule> mCDM;
  Microsoft::WRL::ComPtr<MFPMPHostWrapper> mPMPHostWrapper;

  std::map<nsString, UniquePtr<MFCDMSession>> mSessions;

  MediaEventForwarder<MFCDMKeyMessage> mKeyMessageEvents;
  MediaEventForwarder<MFCDMKeyStatusChange> mKeyChangeEvents;
  MediaEventForwarder<MFCDMKeyExpiration> mExpirationEvents;

  MediaEventListener mKeyMessageListener;
  MediaEventListener mKeyChangeListener;
  MediaEventListener mExpirationListener;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_IPC_MFCDMPARENT_H_
