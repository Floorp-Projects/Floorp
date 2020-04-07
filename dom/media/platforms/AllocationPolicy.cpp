/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AllocationPolicy.h"

#include "ImageContainer.h"
#include "MediaInfo.h"
#include "PDMFactory.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SchedulerGroup.h"
#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/jni/Utils.h"
#endif

namespace mozilla {

using TrackType = TrackInfo::TrackType;

class AllocPolicyImpl::AutoDeallocToken : public Token {
 public:
  explicit AutoDeallocToken(const RefPtr<AllocPolicyImpl>& aPolicy)
      : mPolicy(aPolicy) {}

 private:
  ~AutoDeallocToken() { mPolicy->Dealloc(); }

  RefPtr<AllocPolicyImpl> mPolicy;
};

AllocPolicyImpl::AllocPolicyImpl(int aDecoderLimit)
    : mMaxDecoderLimit(aDecoderLimit),
      mMonitor("AllocPolicyImpl"),
      mDecoderLimit(aDecoderLimit) {}
AllocPolicyImpl::~AllocPolicyImpl() { RejectAll(); }

auto AllocPolicyImpl::Alloc() -> RefPtr<Promise> {
  ReentrantMonitorAutoEnter mon(mMonitor);
  // No decoder limit set.
  if (mDecoderLimit < 0) {
    return Promise::CreateAndResolve(new Token(), __func__);
  }

  RefPtr<PromisePrivate> p = new PromisePrivate(__func__);
  mPromises.push(p);
  ResolvePromise(mon);
  return p;
}

void AllocPolicyImpl::Dealloc() {
  ReentrantMonitorAutoEnter mon(mMonitor);
  ++mDecoderLimit;
  ResolvePromise(mon);
}

void AllocPolicyImpl::ResolvePromise(ReentrantMonitorAutoEnter& aProofOfLock) {
  MOZ_ASSERT(mDecoderLimit >= 0);

  if (mDecoderLimit > 0 && !mPromises.empty()) {
    --mDecoderLimit;
    RefPtr<PromisePrivate> p = std::move(mPromises.front());
    mPromises.pop();
    p->Resolve(new AutoDeallocToken(this), __func__);
  }
}

void AllocPolicyImpl::RejectAll() {
  ReentrantMonitorAutoEnter mon(mMonitor);
  while (!mPromises.empty()) {
    RefPtr<PromisePrivate> p = std::move(mPromises.front());
    mPromises.pop();
    p->Reject(true, __func__);
  }
}

static int32_t MediaDecoderLimitDefault() {
#ifdef MOZ_WIDGET_ANDROID
  if (jni::GetAPIVersion() < 18) {
    // Older Android versions have broken support for multiple simultaneous
    // decoders, see bug 1278574.
    return 1;
  }
#endif
  // Otherwise, set no decoder limit.
  return -1;
}

StaticMutex GlobalAllocPolicy::sMutex;

NotNull<AllocPolicy*> GlobalAllocPolicy::Instance(TrackType aTrack) {
  StaticMutexAutoLock lock(sMutex);
  if (aTrack == TrackType::kAudioTrack) {
    static RefPtr<AllocPolicyImpl> sAudioPolicy = []() {
      SchedulerGroup::Dispatch(
          TaskCategory::Other,
          NS_NewRunnableFunction(
              "GlobalAllocPolicy::GlobalAllocPolicy:Audio", []() {
                ClearOnShutdown(&sAudioPolicy, ShutdownPhase::ShutdownThreads);
              }));
      return new AllocPolicyImpl(MediaDecoderLimitDefault());
    }();
    return WrapNotNull(sAudioPolicy.get());
  }
  static RefPtr<AllocPolicyImpl> sVideoPolicy = []() {
    SchedulerGroup::Dispatch(
        TaskCategory::Other,
        NS_NewRunnableFunction(
            "GlobalAllocPolicy::GlobalAllocPolicy:Audio", []() {
              ClearOnShutdown(&sVideoPolicy, ShutdownPhase::ShutdownThreads);
            }));
    return new AllocPolicyImpl(MediaDecoderLimitDefault());
  }();
  return WrapNotNull(sVideoPolicy.get());
}

class SingleAllocPolicy::AutoDeallocCombinedToken : public Token {
 public:
  AutoDeallocCombinedToken(already_AddRefed<Token> aSingleAllocPolicyToken,
                           already_AddRefed<Token> aGlobalAllocPolicyToken)
      : mSingleToken(aSingleAllocPolicyToken),
        mGlobalToken(aGlobalAllocPolicyToken) {}

 private:
  // Release tokens allocated from GlobalAllocPolicy and LocalAllocPolicy
  // and process next token request if any.
  ~AutoDeallocCombinedToken() = default;
  const RefPtr<Token> mSingleToken;
  const RefPtr<Token> mGlobalToken;
};

auto SingleAllocPolicy::Alloc() -> RefPtr<Promise> {
  MOZ_DIAGNOSTIC_ASSERT(MaxDecoderLimit() == 1,
                        "We can only handle at most one token out at a time.");
  RefPtr<SingleAllocPolicy> self = this;
  return AllocPolicyImpl::Alloc()->Then(
      mOwnerThread, __func__,
      [self](RefPtr<Token> aToken) {
        RefPtr<Token> localToken = std::move(aToken);
        RefPtr<Promise> p = self->mPendingPromise.Ensure(__func__);
        GlobalAllocPolicy::Instance(self->mTrack)
            ->Alloc()
            ->Then(
                self->mOwnerThread, __func__,
                [self, localToken = std::move(localToken)](
                    RefPtr<Token> aToken) mutable {
                  self->mTokenRequest.Complete();
                  RefPtr<Token> combinedToken = new AutoDeallocCombinedToken(
                      localToken.forget(), aToken.forget());
                  self->mPendingPromise.Resolve(combinedToken, __func__);
                },
                [self]() {
                  self->mTokenRequest.Complete();
                  self->mPendingPromise.Reject(true, __func__);
                })
            ->Track(self->mTokenRequest);
        return p;
      },
      []() { return Promise::CreateAndReject(true, __func__); });
}

SingleAllocPolicy::~SingleAllocPolicy() {
  mPendingPromise.RejectIfExists(true, __func__);
  mTokenRequest.DisconnectIfExists();
}

void SingleAllocPolicy::Cancel() {
  MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  mPendingPromise.RejectIfExists(true, __func__);
  mTokenRequest.DisconnectIfExists();
  RejectAll();
}

AllocationWrapper::AllocationWrapper(
    already_AddRefed<MediaDataDecoder> aDecoder, already_AddRefed<Token> aToken)
    : mDecoder(aDecoder), mToken(aToken) {
  DecoderDoctorLogger::LogConstructionAndBase(
      "AllocationWrapper", this, static_cast<const MediaDataDecoder*>(this));
  DecoderDoctorLogger::LinkParentAndChild("AllocationWrapper", this, "decoder",
                                          mDecoder.get());
}

AllocationWrapper::~AllocationWrapper() {
  DecoderDoctorLogger::LogDestruction("AllocationWrapper", this);
}

RefPtr<ShutdownPromise> AllocationWrapper::Shutdown() {
  RefPtr<MediaDataDecoder> decoder = std::move(mDecoder);
  RefPtr<Token> token = std::move(mToken);
  return decoder->Shutdown()->Then(
      AbstractThread::GetCurrent(), __func__,
      [token]() { return ShutdownPromise::CreateAndResolve(true, __func__); });
}
/* static */ RefPtr<AllocationWrapper::AllocateDecoderPromise>
AllocationWrapper::CreateDecoder(const CreateDecoderParams& aParams,
                                 AllocPolicy* aPolicy) {
  // aParams.mConfig is guaranteed to stay alive during the lifetime of the
  // MediaDataDecoder, so keeping a pointer to the object is safe.
  const TrackInfo* config = &aParams.mConfig;
  RefPtr<TaskQueue> taskQueue = aParams.mTaskQueue;
  DecoderDoctorDiagnostics* diagnostics = aParams.mDiagnostics;
  RefPtr<layers::ImageContainer> imageContainer = aParams.mImageContainer;
  RefPtr<layers::KnowsCompositor> knowsCompositor = aParams.mKnowsCompositor;
  RefPtr<GMPCrashHelper> crashHelper = aParams.mCrashHelper;
  CreateDecoderParams::UseNullDecoder useNullDecoder = aParams.mUseNullDecoder;
  CreateDecoderParams::NoWrapper noWrapper = aParams.mNoWrapper;
  TrackInfo::TrackType type = aParams.mType;
  MediaEventProducer<TrackInfo::TrackType>* onWaitingForKeyEvent =
      aParams.mOnWaitingForKeyEvent;
  CreateDecoderParams::OptionSet options = aParams.mOptions;
  CreateDecoderParams::VideoFrameRate rate = aParams.mRate;

  RefPtr<AllocateDecoderPromise> p =
      (aPolicy ? aPolicy : GlobalAllocPolicy::Instance(aParams.mType))
          ->Alloc()
          ->Then(
              AbstractThread::GetCurrent(), __func__,
              [=](RefPtr<Token> aToken) {
                // result may not always be updated by
                // PDMFactory::CreateDecoder either when the creation
                // succeeded or failed, as such it must be initialized to a
                // fatal error by default.
                MediaResult result =
                    MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                nsPrintfCString("error creating %s decoder",
                                                TrackTypeToStr(type)));
                RefPtr<PDMFactory> pdm = new PDMFactory();
                CreateDecoderParams params{*config,
                                           taskQueue,
                                           diagnostics,
                                           imageContainer,
                                           &result,
                                           knowsCompositor,
                                           crashHelper,
                                           useNullDecoder,
                                           noWrapper,
                                           type,
                                           onWaitingForKeyEvent,
                                           options,
                                           rate};
                RefPtr<MediaDataDecoder> decoder = pdm->CreateDecoder(params);
                if (decoder) {
                  RefPtr<AllocationWrapper> wrapper =
                      new AllocationWrapper(decoder.forget(), aToken.forget());
                  return AllocateDecoderPromise::CreateAndResolve(wrapper,
                                                                  __func__);
                }
                return AllocateDecoderPromise::CreateAndReject(result,
                                                               __func__);
              },
              []() {
                return AllocateDecoderPromise::CreateAndReject(
                    MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                                "Allocation policy expired"),
                    __func__);
              });
  return p;
}

}  // namespace mozilla
