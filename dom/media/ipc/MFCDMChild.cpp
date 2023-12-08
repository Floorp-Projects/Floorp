/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFCDMChild.h"

#include "mozilla/EMEUtils.h"
#include "mozilla/KeySystemConfig.h"
#include "mozilla/RefPtr.h"
#include "mozilla/WMFCDMProxyCallback.h"
#include "nsString.h"
#include "RemoteDecoderManagerChild.h"

namespace mozilla {

#define LOG(msg, ...) \
  EME_LOG("MFCDMChild[%p]@%s: " msg, this, __func__, ##__VA_ARGS__)
#define SLOG(msg, ...) EME_LOG("MFCDMChild@%s: " msg, __func__, ##__VA_ARGS__)

#define HANDLE_PENDING_PROMISE(method, callsite, promise, promiseId)         \
  do {                                                                       \
    promise->Then(                                                           \
        self->mManagerThread, callsite,                                      \
        [self, promiseId, callsite](                                         \
            PMFCDMChild::method##Promise::ResolveOrRejectValue&& result) {   \
          auto iter = self->mPendingGenericPromises.find(promiseId);         \
          if (iter == self->mPendingGenericPromises.end()) {                 \
            return;                                                          \
          }                                                                  \
          auto& promiseHolder = iter->second;                                \
          if (result.IsResolve()) {                                          \
            if (NS_SUCCEEDED(result.ResolveValue())) {                       \
              promiseHolder.ResolveIfExists(true, callsite);                 \
            } else {                                                         \
              promiseHolder.RejectIfExists(result.ResolveValue(), callsite); \
            }                                                                \
          } else {                                                           \
            /* IPC die */                                                    \
            promiseHolder.RejectIfExists(NS_ERROR_FAILURE, callsite);        \
          }                                                                  \
          self->mPendingGenericPromises.erase(iter);                         \
        });                                                                  \
  } while (0)

#define INVOKE_ASYNC(method, promiseId, param1)                      \
  do {                                                               \
    auto callsite = __func__;                                        \
    using ParamType = std::remove_reference<decltype(param1)>::type; \
    mManagerThread->Dispatch(NS_NewRunnableFunction(                 \
        callsite, [self = RefPtr{this}, callsite, promiseId,         \
                   param_1 = std::forward<ParamType>(param1)] {      \
          auto p = self->Send##method(param_1);                      \
          HANDLE_PENDING_PROMISE(method, callsite, p, promiseId);    \
        }));                                                         \
  } while (0)

#define INVOKE_ASYNC2(method, promiseId, param1, param2)              \
  do {                                                                \
    auto callsite = __func__;                                         \
    using ParamType1 = std::remove_reference<decltype(param1)>::type; \
    using ParamType2 = std::remove_reference<decltype(param2)>::type; \
    mManagerThread->Dispatch(NS_NewRunnableFunction(                  \
        callsite, [self = RefPtr{this}, callsite, promiseId,          \
                   param_1 = std::forward<ParamType1>(param1),        \
                   param_2 = std::forward<ParamType2>(param2)] {      \
          auto p = self->Send##method(param_1, param_2);              \
          HANDLE_PENDING_PROMISE(method, callsite, p, promiseId);     \
        }));                                                          \
  } while (0)

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
            MOZ_ALWAYS_TRUE(manager->SendPMFCDMConstructor(this, mKeySystem));
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

  if (mState == NS_OK) {
    mManagerThread->Dispatch(
        NS_NewRunnableFunction(__func__, [self = RefPtr{this}, this]() {
          mRemoteRequest.DisconnectIfExists();
          mInitRequest.DisconnectIfExists();

          for (auto& promise : mPendingSessionPromises) {
            promise.second.RejectIfExists(NS_ERROR_ABORT, __func__);
          }
          mPendingSessionPromises.clear();

          for (auto& promise : mPendingGenericPromises) {
            promise.second.RejectIfExists(NS_ERROR_ABORT, __func__);
          }
          mPendingGenericPromises.clear();

          mRemotePromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);
          mCapabilitiesPromiseHolder.RejectIfExists(NS_ERROR_ABORT, __func__);

          Send__delete__(this);
        }));
  }
}

RefPtr<MFCDMChild::CapabilitiesPromise> MFCDMChild::GetCapabilities(
    bool aIsHWSecured) {
  MOZ_ASSERT(mManagerThread);

  if (mShutdown) {
    return CapabilitiesPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  if (mState != NS_OK && mState != NS_ERROR_NOT_INITIALIZED) {
    LOG("error=%x", uint32_t(nsresult(mState)));
    return CapabilitiesPromise::CreateAndReject(mState, __func__);
  }

  auto doSend = [self = RefPtr{this}, aIsHWSecured, this]() {
    SendGetCapabilities(aIsHWSecured)
        ->Then(
            mManagerThread, __func__,
            [self, this](MFCDMCapabilitiesResult&& aResult) {
              if (aResult.type() == MFCDMCapabilitiesResult::Tnsresult) {
                mCapabilitiesPromiseHolder.RejectIfExists(
                    aResult.get_nsresult(), __func__);
                return;
              }
              mCapabilitiesPromiseHolder.ResolveIfExists(
                  std::move(aResult.get_MFCDMCapabilitiesIPDL()), __func__);
            },
            [self, this](const mozilla::ipc::ResponseRejectReason& aReason) {
              mCapabilitiesPromiseHolder.RejectIfExists(NS_ERROR_FAILURE,
                                                        __func__);
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
          LOG("error=%x", uint32_t(rv));
          mState = rv;
          aPromise.RejectIfExists(rv, aCallerName);
        });
  }

  return aPromise.Ensure(aCallerName);
}

RefPtr<MFCDMChild::InitPromise> MFCDMChild::Init(
    const nsAString& aOrigin, const CopyableTArray<nsString>& aInitDataTypes,
    const KeySystemConfig::Requirement aPersistentState,
    const KeySystemConfig::Requirement aDistinctiveID,
    const CopyableTArray<MFCDMMediaCapability>& aAudioCapabilities,
    const CopyableTArray<MFCDMMediaCapability>& aVideoCapabilities,
    WMFCDMProxyCallback* aProxyCallback) {
  MOZ_ASSERT(mManagerThread);

  if (mShutdown) {
    return InitPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  if (mState != NS_OK && mState != NS_ERROR_NOT_INITIALIZED) {
    LOG("error=%x", uint32_t(nsresult(mState)));
    return InitPromise::CreateAndReject(mState, __func__);
  }

  mProxyCallback = aProxyCallback;
  MFCDMInitParamsIPDL params{nsString(aOrigin),  aInitDataTypes,
                             aDistinctiveID,     aPersistentState,
                             aAudioCapabilities, aVideoCapabilities};
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
    uint32_t aPromiseId, KeySystemConfig::SessionType aSessionType,
    const nsAString& aInitDataType, const nsTArray<uint8_t>& aInitData) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  if (mShutdown) {
    return MFCDMChild::SessionPromise::CreateAndReject(NS_ERROR_ABORT,
                                                       __func__);
  }

  MOZ_ASSERT(mPendingSessionPromises.find(aPromiseId) ==
             mPendingSessionPromises.end());
  mPendingSessionPromises.emplace(aPromiseId,
                                  MozPromiseHolder<SessionPromise>{});
  mManagerThread->Dispatch(NS_NewRunnableFunction(
      __func__, [self = RefPtr{this}, this,
                 params =
                     MFCDMCreateSessionParamsIPDL{
                         aSessionType, nsString{aInitDataType}, aInitData},
                 aPromiseId] {
        SendCreateSessionAndGenerateRequest(params)->Then(
            mManagerThread, __func__,
            [self, aPromiseId, this](const MFCDMSessionResult& result) {
              auto iter = mPendingSessionPromises.find(aPromiseId);
              if (iter == mPendingSessionPromises.end()) {
                return;
              }
              auto& promiseHolder = iter->second;
              if (result.type() == MFCDMSessionResult::Tnsresult) {
                promiseHolder.RejectIfExists(result.get_nsresult(), __func__);
              } else {
                LOG("session ID=[%zu]%s", result.get_nsString().Length(),
                    NS_ConvertUTF16toUTF8(result.get_nsString()).get());
                promiseHolder.ResolveIfExists(result.get_nsString(), __func__);
              }
              mPendingSessionPromises.erase(iter);
            },
            [self, aPromiseId,
             this](const mozilla::ipc::ResponseRejectReason& aReason) {
              auto iter = mPendingSessionPromises.find(aPromiseId);
              if (iter == mPendingSessionPromises.end()) {
                return;
              }
              auto& promiseHolder = iter->second;
              promiseHolder.RejectIfExists(NS_ERROR_FAILURE, __func__);
              mPendingSessionPromises.erase(iter);
            });
      }));
  return mPendingSessionPromises[aPromiseId].Ensure(__func__);
}

RefPtr<GenericPromise> MFCDMChild::LoadSession(
    uint32_t aPromiseId, const KeySystemConfig::SessionType aSessionType,
    const nsAString& aSessionId) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  if (mShutdown) {
    return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  MOZ_ASSERT(mPendingGenericPromises.find(aPromiseId) ==
             mPendingGenericPromises.end());
  mPendingGenericPromises.emplace(aPromiseId,
                                  MozPromiseHolder<GenericPromise>{});
  INVOKE_ASYNC2(LoadSession, aPromiseId, aSessionType, nsString{aSessionId});
  return mPendingGenericPromises[aPromiseId].Ensure(__func__);
}

RefPtr<GenericPromise> MFCDMChild::UpdateSession(uint32_t aPromiseId,
                                                 const nsAString& aSessionId,
                                                 nsTArray<uint8_t>& aResponse) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  if (mShutdown) {
    return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  MOZ_ASSERT(mPendingGenericPromises.find(aPromiseId) ==
             mPendingGenericPromises.end());
  mPendingGenericPromises.emplace(aPromiseId,
                                  MozPromiseHolder<GenericPromise>{});
  INVOKE_ASYNC2(UpdateSession, aPromiseId, nsString{aSessionId},
                std::move(aResponse));
  return mPendingGenericPromises[aPromiseId].Ensure(__func__);
}

RefPtr<GenericPromise> MFCDMChild::CloseSession(uint32_t aPromiseId,
                                                const nsAString& aSessionId) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  if (mShutdown) {
    return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  MOZ_ASSERT(mPendingGenericPromises.find(aPromiseId) ==
             mPendingGenericPromises.end());
  mPendingGenericPromises.emplace(aPromiseId,
                                  MozPromiseHolder<GenericPromise>{});
  INVOKE_ASYNC(CloseSession, aPromiseId, nsString{aSessionId});
  return mPendingGenericPromises[aPromiseId].Ensure(__func__);
}

RefPtr<GenericPromise> MFCDMChild::RemoveSession(uint32_t aPromiseId,
                                                 const nsAString& aSessionId) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  if (mShutdown) {
    return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  MOZ_ASSERT(mPendingGenericPromises.find(aPromiseId) ==
             mPendingGenericPromises.end());
  mPendingGenericPromises.emplace(aPromiseId,
                                  MozPromiseHolder<GenericPromise>{});
  INVOKE_ASYNC(RemoveSession, aPromiseId, nsString{aSessionId});
  return mPendingGenericPromises[aPromiseId].Ensure(__func__);
}

RefPtr<GenericPromise> MFCDMChild::SetServerCertificate(
    uint32_t aPromiseId, nsTArray<uint8_t>& aCert) {
  MOZ_ASSERT(mManagerThread);
  MOZ_ASSERT(mId > 0, "Should call Init() first and wait for it");

  if (mShutdown) {
    return GenericPromise::CreateAndReject(NS_ERROR_ABORT, __func__);
  }

  MOZ_ASSERT(mPendingGenericPromises.find(aPromiseId) ==
             mPendingGenericPromises.end());
  mPendingGenericPromises.emplace(aPromiseId,
                                  MozPromiseHolder<GenericPromise>{});
  INVOKE_ASYNC(SetServerCertificate, aPromiseId, std::move(aCert));
  return mPendingGenericPromises[aPromiseId].Ensure(__func__);
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
