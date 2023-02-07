/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_IPC_MFCDMPARENT_H_
#define DOM_MEDIA_IPC_MFCDMPARENT_H_

#include <wrl.h>

#include "mozilla/Assertions.h"
#include "mozilla/PMFCDMParent.h"
#include "MFCDMExtra.h"
#include "RemoteDecoderManagerParent.h"

namespace mozilla {

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
    MOZ_ASSERT(!sRegisteredCDMs.Contains(aId));
    return sRegisteredCDMs.Get(aId);
  }
  uint64_t Id() const { return mId; }

  mozilla::ipc::IPCResult RecvGetCapabilities(
      GetCapabilitiesResolver&& aResolver);

  mozilla::ipc::IPCResult RecvInit(const MFCDMInitParamsIPDL& aParams,
                                   InitResolver&& aResolver);

  nsISerialEventTarget* ManagerThread() { return mManagerThread; }
  void AssertOnManagerThread() const {
    MOZ_ASSERT(mManagerThread->IsOnCurrentThread());
  }

  void Destroy() {
    AssertOnManagerThread();
    mIPDLSelfRef = nullptr;
  }

 private:
  ~MFCDMParent() { Unregister(); }

  HRESULT LoadFactory();

  void Register() {
    MOZ_ASSERT(!sRegisteredCDMs.Contains(this->mId));
    sRegisteredCDMs.InsertOrUpdate(this->mId, this);
  }
  void Unregister() {
    MOZ_ASSERT(sRegisteredCDMs.Contains(this->mId));
    sRegisteredCDMs.Remove(this->mId);
  }

  nsString mKeySystem;

  const RefPtr<RemoteDecoderManagerParent> mManager;
  const RefPtr<nsISerialEventTarget> mManagerThread;

  static inline nsTHashMap<nsUint64HashKey, MFCDMParent*> sRegisteredCDMs;

  static inline uint64_t sNextId = 1;
  const uint64_t mId;

  RefPtr<MFCDMParent> mIPDLSelfRef;
  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory> mFactory;
  Microsoft::WRL::ComPtr<IMFContentDecryptionModule> mCDM;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_IPC_MFCDMPARENT_H_
