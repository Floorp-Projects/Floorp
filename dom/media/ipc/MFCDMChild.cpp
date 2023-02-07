/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFCDMChild.h"

#include <mfapi.h>
#include <mfidl.h>
#include <winerror.h>
#include <winnt.h>
#include <wrl.h>

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
  mRemotePromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
  mCapabilitiesPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);

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

  if (mRemotePromiseHolder.IsEmpty()) {
    mManagerThread->Dispatch(
        NS_NewRunnableFunction(__func__, std::move(doSend)));
  } else {
    mRemotePromise->Then(mManagerThread, __func__, std::move(doSend),
                         [self = RefPtr{this}, this](nsresult rv) {
                           LOG("error=%x", rv);
                           mState = rv;
                           mCapabilitiesPromiseHolder.RejectIfExists(rv,
                                                                     __func__);
                         });
  }
  return mCapabilitiesPromiseHolder.Ensure(__func__);
}

#undef SLOG
#undef LOG

}  // namespace mozilla
