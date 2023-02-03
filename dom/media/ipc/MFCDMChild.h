/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_IPC_MFCDMCHILD_H_
#define DOM_MEDIA_IPC_MFCDMCHILD_H_

#include "mozilla/Atomics.h"
#include "mozilla/MozPromise.h"
#include "mozilla/PMFCDMChild.h"

namespace mozilla {

/**
 * MFCDMChild is a content process proxy to MFCDMParent and the actual CDM
 * running in utility process.
 */
class MFCDMChild final : public PMFCDMChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MFCDMChild);

  explicit MFCDMChild(const nsAString& aKeySystem);

  using CapabilitiesPromise = MozPromise<MFCDMCapabilitiesIPDL, nsresult, true>;
  RefPtr<CapabilitiesPromise> GetCapabilities();

  void IPDLActorDestroyed() {
    AssertOnManagerThread();
    mIPDLSelfRef = nullptr;
    if (!mShutdown) {
      // Remote crashed!
      mState = NS_ERROR_NOT_AVAILABLE;
    }
  }
  void Shutdown();

  nsISerialEventTarget* ManagerThread() { return mManagerThread; }
  void AssertOnManagerThread() const {
    MOZ_ASSERT(mManagerThread->IsOnCurrentThread());
  }

 private:
  ~MFCDMChild() = default;

  using RemotePromise = GenericNonExclusivePromise;
  RefPtr<RemotePromise> EnsureRemote();

  const nsString mKeySystem;

  const RefPtr<nsISerialEventTarget> mManagerThread;
  RefPtr<MFCDMChild> mIPDLSelfRef;

  RefPtr<RemotePromise> mRemotePromise;
  MozPromiseHolder<RemotePromise> mRemotePromiseHolder;
  MozPromiseRequestHolder<RemotePromise> mRemoteRequest;
  // Before EnsureRemote(): NS_ERROR_NOT_INITIALIZED; After EnsureRemote():
  // NS_OK or some error code.
  Atomic<nsresult> mState;

  MozPromiseHolder<CapabilitiesPromise> mCapabilitiesPromiseHolder;

  Atomic<bool> mShutdown;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_IPC_MFCDMCHILD_H_
