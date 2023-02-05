/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFCDMChild.h"

#include "mozilla/EMEUtils.h"
#include "mozilla/KeySystemConfig.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WindowsVersion.h"
#include "nsString.h"
#include "RemoteDecoderManagerChild.h"

namespace mozilla {

#define LOG(msg, ...) \
  EME_LOG("MFCDMChild[%p]@%s: " msg, this, __func__, ##__VA_ARGS__)
#define SLOG(msg, ...) EME_LOG("MFCDMChild@%s: " msg, __func__, ##__VA_ARGS__)

MFCDMChild::MFCDMChild(const nsAString& aKeySystem)
    : mKeySystem(aKeySystem),
      mManagerThread(RemoteDecoderManagerChild::GetManagerThread()),
      mState(NS_ERROR_NOT_INITIALIZED),
      mShutdown(false) {
  mRemotePromise = EnsureRemote();
}

RefPtr<MFCDMChild::RemotePromise> MFCDMChild::EnsureRemote() {
  if (!mManagerThread) {
    LOG("no manager thread");
    mState = NS_ERROR_NOT_AVAILABLE;
    return RemotePromise::CreateAndReject(mState, __func__);
  }

  if (!IsWin10OrLater()) {
    LOG("only support MF CDM on Windows 10+");
    mState = NS_ERROR_NOT_AVAILABLE;
    return RemotePromise::CreateAndReject(mState, __func__);
  }

  RefPtr<MFCDMChild> self = this;
  RemoteDecoderManagerChild::LaunchUtilityProcessIfNeeded(
      RemoteDecodeIn::UtilityProcess_MFMediaEngineCDM)
      ->Then(
          mManagerThread, __func__,
          [self, this](bool) {
            mRemoteRequest.Complete();
            RefPtr<RemoteDecoderManagerChild> manager =
                RemoteDecoderManagerChild::GetSingleton(
                    RemoteDecodeIn::UtilityProcess_MFMediaEngineCDM);
            if (!manager || !manager->CanSend()) {
              LOG("manager not exists or can't send");
              mState = NS_ERROR_NOT_AVAILABLE;
              mRemotePromiseHolder.RejectIfExists(mState, __func__);
              return;
            }

            mIPDLSelfRef = this;
            Unused << manager->SendPMFCDMConstructor(this, mKeySystem);
            mState = NS_OK;
            mRemotePromiseHolder.ResolveIfExists(true, __func__);
          },
          [self, this](nsresult rv) {
            mRemoteRequest.Complete();
            LOG("fail to launch MFCDM process");
            mState = rv;
            mRemotePromiseHolder.RejectIfExists(rv, __func__);
          })
      ->Track(mRemoteRequest);
  return mRemotePromiseHolder.Ensure(__func__);
}

void MFCDMChild::Shutdown() {
  MOZ_ASSERT(!mShutdown);

  mShutdown = true;

  mRemoteRequest.DisconnectIfExists();
  mInitRequest.DisconnectIfExists();
  mRemotePromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
  mCapabilitiesPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
  mInitPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);

  if (mState == NS_OK) {
    mManagerThread->Dispatch(NS_NewRunnableFunction(
        __func__, [self = RefPtr{this}, this]() { Send__delete__(this); }));
  }
}

RefPtr<MFCDMChild::CapabilitiesPromise> MFCDMChild::GetCapabilities() {
  MOZ_ASSERT(mManagerThread);

  if (mShutdown) {
    return CapabilitiesPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  if (mState != NS_OK && mState != NS_ERROR_NOT_INITIALIZED) {
    LOG("error=%x", nsresult(mState));
    return CapabilitiesPromise::CreateAndReject(mState, __func__);
  }

  auto doSend = [self = RefPtr{this}, this]() {
    SendGetCapabilities()->Then(
        mManagerThread, __func__,
        [self, this](MFCDMCapabilitiesResult&& aResult) {
          if (aResult.type() == MFCDMCapabilitiesResult::Tnsresult) {
            mCapabilitiesPromiseHolder.RejectIfExists(aResult.get_nsresult(),
                                                      __func__);
            return;
          }
          mCapabilitiesPromiseHolder.ResolveIfExists(
              std::move(aResult.get_MFCDMCapabilitiesIPDL()), __func__);
        },
        [self, this](const mozilla::ipc::ResponseRejectReason& aReason) {
          mCapabilitiesPromiseHolder.RejectIfExists(NS_ERROR_FAILURE, __func__);
        });
  };

  return InvokeAsync(doSend, __func__, mCapabilitiesPromiseHolder);
}

// Neither error nor shutdown.
void MFCDMChild::AssertSendable() {
  MOZ_ASSERT((mState == NS_ERROR_NOT_INITIALIZED || mState == NS_OK) &&
             mShutdown == false);
}

template <typename PromiseType>
already_AddRefed<PromiseType> MFCDMChild::InvokeAsync(
    std::function<void()>&& aCall, const char* aCallerName,
    MozPromiseHolder<PromiseType>& aPromise) {
  AssertSendable();

  if (mState == NS_OK) {
    // mRemotePromise is resolved, send on manager thread.
    mManagerThread->Dispatch(
        NS_NewRunnableFunction(aCallerName, std::move(aCall)));
  } else if (mState == NS_ERROR_NOT_INITIALIZED) {
    // mRemotePromise is pending, chain to it.
    mRemotePromise->Then(
        mManagerThread, __func__, std::move(aCall),
        [self = RefPtr{this}, this, &aPromise, aCallerName](nsresult rv) {
          LOG("error=%x", rv);
          mState = rv;
          aPromise.RejectIfExists(rv, aCallerName);
        });
  }

  return aPromise.Ensure(aCallerName);
}

RefPtr<MFCDMChild::InitPromise> MFCDMChild::Init(
    const nsAString& aOrigin,
    const KeySystemConfig::Requirement aPersistentState,
    const KeySystemConfig::Requirement aDistinctiveID, const bool aHWSecure) {
  MOZ_ASSERT(mManagerThread);

  if (mShutdown) {
    return InitPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  if (mState != NS_OK && mState != NS_ERROR_NOT_INITIALIZED) {
    LOG("error=%x", nsresult(mState));
    return InitPromise::CreateAndReject(mState, __func__);
  }

  MFCDMInitParamsIPDL params{nsString(aOrigin), aPersistentState,
                             aDistinctiveID, aHWSecure};
  auto doSend = [self = RefPtr{this}, this, params]() {
    SendInit(params)
        ->Then(
            mManagerThread, __func__,
            [self, this](MFCDMInitResult&& aResult) {
              if (aResult.type() == MFCDMInitResult::Tnsresult) {
                nsresult rv = aResult.get_nsresult();
                mInitPromiseHolder.RejectIfExists(rv, __func__);
                return;
              }
              mId = aResult.get_MFCDMInitIPDL().id();
              mInitPromiseHolder.ResolveIfExists(aResult.get_MFCDMInitIPDL(),
                                                 __func__);
            },
            [self, this](const mozilla::ipc::ResponseRejectReason& aReason) {
              mInitPromiseHolder.RejectIfExists(NS_ERROR_FAILURE, __func__);
            })
        ->Track(mInitRequest);
  };

  return InvokeAsync(std::move(doSend), __func__, mInitPromiseHolder);
}

#undef SLOG
#undef LOG

}  // namespace mozilla
