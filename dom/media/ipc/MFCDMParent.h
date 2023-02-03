/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_IPC_MFCDMPARENT_H_
#define DOM_MEDIA_IPC_MFCDMPARENT_H_

#include <mfidl.h>
#include <winnt.h>
#include <wrl.h>

#include "mozilla/PMFCDMParent.h"
#include "MFCDMExtra.h"

namespace mozilla {
class RemoteDecoderManagerParent;

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
              nsISerialEventTarget* aManagerThread)
      : mKeySystem(aKeySystem),
        mManager(aManager),
        mManagerThread(aManagerThread) {
    mIPDLSelfRef = this;
    LoadFactory();
  }

  mozilla::ipc::IPCResult RecvGetCapabilities(
      GetCapabilitiesResolver&& aResolver);

  nsISerialEventTarget* ManagerThread() { return mManagerThread; }
  void AssertOnManagerThread() const {
    MOZ_ASSERT(mManagerThread->IsOnCurrentThread());
  }

  void Destroy() {
    AssertOnManagerThread();
    mIPDLSelfRef = nullptr;
  }

 private:
  ~MFCDMParent() = default;

  HRESULT LoadFactory();

  nsString mKeySystem;

  const RefPtr<RemoteDecoderManagerParent> mManager;
  const RefPtr<nsISerialEventTarget> mManagerThread;

  RefPtr<MFCDMParent> mIPDLSelfRef;
  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleFactory> mFactory;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_IPC_MFCDMPARENT_H_
