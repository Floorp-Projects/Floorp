/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFCDMChild.h"

#include "mozilla/EMEUtils.h"
#include "mozilla/KeySystemConfig.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WindowsVersion.h"
#include "mozilla/WMFCDMProxyCallback.h"
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

MFCDMChild::~MFCDMChild() {}

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
  mProxyCallback = nullptr;

  mRemoteRequest.DisconnectIfExists();
  mInitRequest.DisconnectIfExists();
  mCreateSessionRequest.DisconnectIfExists();
  mLoadSessionRequest.DisconnectIfExists();
  mUpdateSessionRequest.DisconnectIfExists();
  mCloseSessionRequest.DisconnectIfExists();

  if (mState == NS_OK) {
    mManagerThread->Dispatch(
        NS_NewRunnableFunction(__func__, [self = RefPtr{this}, this]() {
          mRemotePromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
          mCapabilitiesPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
          mInitPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
          mCreateSessionPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
          mLoadSessionPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
          mUpdateSessionPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
          mCloseSessionPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);

          Send__delete__(this);
        }));
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
    const nsAString& aOrigin, const CopyableTArray<nsString>& aInitDataTypes,
    const KeySystemConfig::Requirement aPersistentState,
    const KeySystemConfig::Requirement aDistinctiveID, const bool aHWSecure,
    WMFCDMProxyCallback* aProxyCallback) {
  MOZ_ASSERT(mManagerThread);

  if (mShutdown) {
    return InitPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  if (mState != NS_OK && mState != NS_ERROR_NOT_INITIALIZED) {
    LOG("error=%x", nsresult(mState));
    return InitPromise::CreateAndReject(mState, __func__);
  }

  mProxyCallback = aProxyCallback;
  MFCDMInitParamsIPDL params{nsString(aOrigin), aInitDataTypes, aDistinctiveID,
                             aPersistentState, aHWSecure};
  auto doSend = [self = RefPtr{this}, this, params]() {
    SendInit(params)
        ->Then(
            mManagerThread, __func__,
            [self, this](MFCDMInitResult&& aResult) {
              mInitRequest.Complete();
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
              mInitRequest.Complete();
              mInitPromiseHolder.RejectIfExists(NS_ERROR_FAILURE, __func__);
            })
        ->Track(mInitRequest);
  };

  return InvokeAsync(std::move(doSend), __func__, mInitPromiseHolder);
}

RefPtr<MFCDMChild::SessionPromise> MFCDMChild::CreateSessionAndGenerateRequest(
    KeySystemConfig::SessionType aSessionType, const nsAString& aInitDataType,
    const nsTArray<uint8_t>& aInitData) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  mManagerThread->Dispatch(NS_NewRunnableFunction(
      __func__, [self = RefPtr{this}, this,
                 params = MFCDMCreateSessionParamsIPDL{
                     aSessionType, nsString{aInitDataType}, aInitData}] {
        SendCreateSessionAndGenerateRequest(params)
            ->Then(
                mManagerThread, __func__,
                [self, this](const MFCDMSessionResult& result) {
                  mCreateSessionRequest.Complete();
                  if (result.type() == MFCDMSessionResult::Tnsresult) {
                    mCreateSessionPromiseHolder.RejectIfExists(
                        result.get_nsresult(), __func__);
                    return;
                  }
                  LOG("session ID=[%zu]%s", result.get_nsString().Length(),
                      NS_ConvertUTF16toUTF8(result.get_nsString()).get());
                  mCreateSessionPromiseHolder.ResolveIfExists(
                      result.get_nsString(), __func__);
                },
                [self,
                 this](const mozilla::ipc::ResponseRejectReason& aReason) {
                  mCreateSessionRequest.Complete();
                  mCreateSessionPromiseHolder.RejectIfExists(NS_ERROR_FAILURE,
                                                             __func__);
                })
            ->Track(mCreateSessionRequest);
      }));
  return mCreateSessionPromiseHolder.Ensure(__func__);
}

RefPtr<GenericPromise> MFCDMChild::LoadSession(
    const KeySystemConfig::SessionType aSessionType,
    const nsAString& aSessionId) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  mManagerThread->Dispatch(
      NS_NewRunnableFunction(__func__, [self = RefPtr{this}, this, aSessionType,
                                        sessionId = nsString{aSessionId}] {
        SendLoadSession(aSessionType, sessionId)
            ->Then(
                mManagerThread, __func__,
                [self,
                 this](PMFCDMChild::LoadSessionPromise::ResolveOrRejectValue&&
                           aResult) {
                  mLoadSessionRequest.Complete();
                  if (aResult.IsResolve()) {
                    if (NS_SUCCEEDED(aResult.ResolveValue())) {
                      mLoadSessionPromiseHolder.ResolveIfExists(true, __func__);
                    } else {
                      mLoadSessionPromiseHolder.RejectIfExists(
                          aResult.ResolveValue(), __func__);
                    }
                  } else {
                    // IPC died
                    mLoadSessionPromiseHolder.RejectIfExists(NS_ERROR_FAILURE,
                                                             __func__);
                  }
                })
            ->Track(mLoadSessionRequest);
      }));
  return mLoadSessionPromiseHolder.Ensure(__func__);
}

RefPtr<GenericPromise> MFCDMChild::UpdateSession(const nsAString& aSessionId,
                                                 nsTArray<uint8_t>& aResponse) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  mManagerThread->Dispatch(NS_NewRunnableFunction(
      __func__, [self = RefPtr{this}, this, sessionId = nsString{aSessionId},
                 response = std::move(aResponse)] {
        SendUpdateSession(sessionId, response)
            ->Then(mManagerThread, __func__,
                   [self, this](
                       PMFCDMChild::UpdateSessionPromise::ResolveOrRejectValue&&
                           aResult) {
                     mUpdateSessionRequest.Complete();
                     if (aResult.IsResolve()) {
                       if (NS_SUCCEEDED(aResult.ResolveValue())) {
                         mUpdateSessionPromiseHolder.ResolveIfExists(true,
                                                                     __func__);
                       } else {
                         mUpdateSessionPromiseHolder.RejectIfExists(
                             aResult.ResolveValue(), __func__);
                       }
                     } else {
                       // IPC died
                       mUpdateSessionPromiseHolder.RejectIfExists(
                           NS_ERROR_FAILURE, __func__);
                     }
                   })
            ->Track(mUpdateSessionRequest);
      }));
  return mUpdateSessionPromiseHolder.Ensure(__func__);
}

RefPtr<GenericPromise> MFCDMChild::CloseSession(const nsAString& aSessionId) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  mManagerThread->Dispatch(NS_NewRunnableFunction(
      __func__, [self = RefPtr{this}, this, sessionId = nsString{aSessionId}] {
        SendCloseSession(sessionId)
            ->Then(mManagerThread, __func__,
                   [self, this](
                       PMFCDMChild::CloseSessionPromise::ResolveOrRejectValue&&
                           aResult) {
                     mCloseSessionRequest.Complete();
                     if (aResult.IsResolve()) {
                       if (NS_SUCCEEDED(aResult.ResolveValue())) {
                         mCloseSessionPromiseHolder.ResolveIfExists(true,
                                                                    __func__);
                       } else {
                         mCloseSessionPromiseHolder.RejectIfExists(
                             aResult.ResolveValue(), __func__);
                       }
                     } else {
                       // IPC died
                       mCloseSessionPromiseHolder.RejectIfExists(
                           NS_ERROR_FAILURE, __func__);
                     }
                   })
            ->Track(mCloseSessionRequest);
      }));
  return mCloseSessionPromiseHolder.Ensure(__func__);
}

RefPtr<GenericPromise> MFCDMChild::RemoveSession(const nsAString& aSessionId) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  mManagerThread->Dispatch(NS_NewRunnableFunction(
      __func__, [self = RefPtr{this}, this, sessionId = nsString{aSessionId}] {
        SendRemoveSession(sessionId)
            ->Then(mManagerThread, __func__,
                   [self, this](
                       PMFCDMChild::RemoveSessionPromise::ResolveOrRejectValue&&
                           aResult) {
                     mRemoveSessionRequest.Complete();
                     if (aResult.IsResolve()) {
                       if (NS_SUCCEEDED(aResult.ResolveValue())) {
                         mRemoveSessionPromiseHolder.ResolveIfExists(true,
                                                                     __func__);
                       } else {
                         mRemoveSessionPromiseHolder.RejectIfExists(
                             aResult.ResolveValue(), __func__);
                       }
                     } else {
                       // IPC died
                       mRemoveSessionPromiseHolder.RejectIfExists(
                           NS_ERROR_FAILURE, __func__);
                     }
                   })
            ->Track(mRemoveSessionRequest);
      }));
  return mRemoveSessionPromiseHolder.Ensure(__func__);
}

mozilla::ipc::IPCResult MFCDMChild::RecvOnSessionKeyMessage(
    const MFCDMKeyMessage& aMessage) {
  LOG("RecvOnSessionKeyMessage, sessionId=%s",
      NS_ConvertUTF16toUTF8(aMessage.sessionId()).get());
  MOZ_ASSERT(mProxyCallback);
  mProxyCallback->OnSessionMessage(aMessage);
  return IPC_OK();
}

mozilla::ipc::IPCResult MFCDMChild::RecvOnSessionKeyStatusesChanged(
    const MFCDMKeyStatusChange& aKeyStatuses) {
  LOG("RecvOnSessionKeyStatusesChanged, sessionId=%s",
      NS_ConvertUTF16toUTF8(aKeyStatuses.sessionId()).get());
  MOZ_ASSERT(mProxyCallback);
  mProxyCallback->OnSessionKeyStatusesChange(aKeyStatuses);
  return IPC_OK();
}

mozilla::ipc::IPCResult MFCDMChild::RecvOnSessionKeyExpiration(
    const MFCDMKeyExpiration& aExpiration) {
  LOG("RecvOnSessionKeyExpiration, sessionId=%s",
      NS_ConvertUTF16toUTF8(aExpiration.sessionId()).get());
  MOZ_ASSERT(mProxyCallback);
  mProxyCallback->OnSessionKeyExpiration(aExpiration);
  return IPC_OK();
}

#undef SLOG
#undef LOG

}  // namespace mozilla
