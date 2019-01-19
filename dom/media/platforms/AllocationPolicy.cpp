/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AllocationPolicy.h"

#include "PDMFactory.h"
#include "MediaInfo.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SystemGroup.h"
#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/jni/Utils.h"
#endif

namespace mozilla {

using TrackType = TrackInfo::TrackType;

StaticMutex GlobalAllocPolicy::sMutex;

class GlobalAllocPolicy::AutoDeallocToken : public Token {
 public:
  explicit AutoDeallocToken(GlobalAllocPolicy& aPolicy) : mPolicy(aPolicy) {}

 private:
  ~AutoDeallocToken() { mPolicy.Dealloc(); }

  GlobalAllocPolicy& mPolicy;  // reference to a singleton object.
};

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

GlobalAllocPolicy::GlobalAllocPolicy()
    : mMonitor("DecoderAllocPolicy::mMonitor"),
      mDecoderLimit(MediaDecoderLimitDefault()) {
  SystemGroup::Dispatch(
      TaskCategory::Other,
      NS_NewRunnableFunction("GlobalAllocPolicy::GlobalAllocPolicy", [this]() {
        ClearOnShutdown(this, ShutdownPhase::ShutdownThreads);
      }));
}

GlobalAllocPolicy::~GlobalAllocPolicy() {
  while (!mPromises.empty()) {
    RefPtr<PromisePrivate> p = mPromises.front().forget();
    mPromises.pop();
    p->Reject(true, __func__);
  }
}

GlobalAllocPolicy& GlobalAllocPolicy::Instance(TrackType aTrack) {
  StaticMutexAutoLock lock(sMutex);
  if (aTrack == TrackType::kAudioTrack) {
    static auto sAudioPolicy = new GlobalAllocPolicy();
    return *sAudioPolicy;
  } else {
    static auto sVideoPolicy = new GlobalAllocPolicy();
    return *sVideoPolicy;
  }
}

auto GlobalAllocPolicy::Alloc() -> RefPtr<Promise> {
  // No decoder limit set.
  if (mDecoderLimit < 0) {
    return Promise::CreateAndResolve(new Token(), __func__);
  }

  ReentrantMonitorAutoEnter mon(mMonitor);
  RefPtr<PromisePrivate> p = new PromisePrivate(__func__);
  mPromises.push(p);
  ResolvePromise(mon);
  return p.forget();
}

void GlobalAllocPolicy::Dealloc() {
  ReentrantMonitorAutoEnter mon(mMonitor);
  ++mDecoderLimit;
  ResolvePromise(mon);
}

void GlobalAllocPolicy::ResolvePromise(
    ReentrantMonitorAutoEnter& aProofOfLock) {
  MOZ_ASSERT(mDecoderLimit >= 0);

  if (mDecoderLimit > 0 && !mPromises.empty()) {
    --mDecoderLimit;
    RefPtr<PromisePrivate> p = mPromises.front().forget();
    mPromises.pop();
    p->Resolve(new AutoDeallocToken(*this), __func__);
  }
}

void GlobalAllocPolicy::operator=(std::nullptr_t) { delete this; }

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
  RefPtr<MediaDataDecoder> decoder = mDecoder.forget();
  RefPtr<Token> token = mToken.forget();
  return decoder->Shutdown()->Then(
      AbstractThread::GetCurrent(), __func__,
      [token]() { return ShutdownPromise::CreateAndResolve(true, __func__); });
}
/* static */ RefPtr<AllocationWrapper::AllocateDecoderPromise>
AllocationWrapper::CreateDecoder(const CreateDecoderParams& aParams) {
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
      GlobalAllocPolicy::Instance(aParams.mType)
          .Alloc()
          ->Then(AbstractThread::GetCurrent(), __func__,
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
                   RefPtr<MediaDataDecoder> decoder =
                       pdm->CreateDecoder(params);
                   if (decoder) {
                     RefPtr<AllocationWrapper> wrapper = new AllocationWrapper(
                         decoder.forget(), aToken.forget());
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
