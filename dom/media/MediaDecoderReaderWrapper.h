/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDecoderReaderWrapper_h_
#define MediaDecoderReaderWrapper_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"

#include "MediaDecoderReader.h"
#include "MediaCallbackID.h"

namespace mozilla {

class StartTimeRendezvous;

typedef MozPromise<bool, bool, /* isExclusive = */ false> HaveStartTimePromise;

/**
 * A wrapper around MediaDecoderReader to offset the timestamps of Audio/Video
 * samples by the start time to ensure MDSM can always assume zero start time.
 * It also adjusts the seek target passed to Seek() to ensure correct seek time
 * is passed to the underlying reader.
 */
class MediaDecoderReaderWrapper {
  typedef MediaDecoderReader::MetadataPromise MetadataPromise;
  typedef MediaDecoderReader::MediaDataPromise MediaDataPromise;
  typedef MediaDecoderReader::SeekPromise SeekPromise;
  typedef MediaDecoderReader::WaitForDataPromise WaitForDataPromise;
  typedef MediaDecoderReader::BufferedUpdatePromise BufferedUpdatePromise;
  typedef MediaDecoderReader::TargetQueues TargetQueues;
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderReaderWrapper);

  /*
   * Type 1: void(MediaData*)
   *         void(RefPtr<MediaData>)
   */
  template <typename T>
  class ArgType1CheckHelper {
    template<typename C, typename... Ts>
    static TrueType
    test(void(C::*aMethod)(Ts...),
         decltype((DeclVal<C>().*aMethod)(DeclVal<MediaData*>()), 0));

    template <typename F>
    static TrueType
    test(F&&, decltype(DeclVal<F>()(DeclVal<MediaData*>()), 0));

    static FalseType test(...);
  public:
    typedef decltype(test(DeclVal<T>(), 0)) Type;
  };

  template <typename T>
  struct ArgType1Check : public ArgType1CheckHelper<T>::Type {};

  /*
   * Type 2: void(MediaData*, TimeStamp)
   *         void(RefPtr<MediaData>, TimeStamp)
   *         void(MediaData*, TimeStamp&)
   *         void(RefPtr<MediaData>, const TimeStamp&&)
   */
  template <typename T>
  class ArgType2CheckHelper {

    template<typename C, typename... Ts>
    static TrueType
    test(void(C::*aMethod)(Ts...),
         decltype((DeclVal<C>().*aMethod)(DeclVal<MediaData*>(), DeclVal<TimeStamp>()), 0));

    template <typename F>
    static TrueType
    test(F&&, decltype(DeclVal<F>()(DeclVal<MediaData*>(), DeclVal<TimeStamp>()), 0));

    static FalseType test(...);
  public:
    typedef decltype(test(DeclVal<T>(), 0)) Type;
  };

  template <typename T>
  struct ArgType2Check : public ArgType2CheckHelper<T>::Type {};

  struct CallbackBase
  {
    virtual ~CallbackBase() {}
    virtual void OnResolved(MediaData*, TimeStamp) = 0;
    virtual void OnRejected(MediaDecoderReader::NotDecodedReason) = 0;
  };

  template<typename ThisType, typename ResolveMethodType, typename RejectMethodType>
  struct MethodCallback : public CallbackBase
  {
    MethodCallback(ThisType* aThis, ResolveMethodType aResolveMethod, RejectMethodType aRejectMethod)
      : mThis(aThis), mResolveMethod(aResolveMethod), mRejectMethod(aRejectMethod)
    {
    }

    template<typename F>
    typename EnableIf<ArgType1Check<F>::value, void>::Type
    CallHelper(MediaData* aSample, TimeStamp)
    {
      (mThis->*mResolveMethod)(aSample);
    }

    template<typename F>
    typename EnableIf<ArgType2Check<F>::value, void>::Type
    CallHelper(MediaData* aSample, TimeStamp aDecodeStartTime)
    {
      (mThis->*mResolveMethod)(aSample, aDecodeStartTime);
    }

    void OnResolved(MediaData* aSample, TimeStamp aDecodeStartTime) override
    {
      CallHelper<ResolveMethodType>(aSample, aDecodeStartTime);
    }

    void OnRejected(MediaDecoderReader::NotDecodedReason aReason) override
    {
      (mThis->*mRejectMethod)(aReason);
    }

    RefPtr<ThisType> mThis;
    ResolveMethodType mResolveMethod;
    RejectMethodType mRejectMethod;
  };

  template<typename ResolveFunctionType, typename RejectFunctionType>
  struct FunctionCallback : public CallbackBase
  {
    FunctionCallback(ResolveFunctionType&& aResolveFuntion, RejectFunctionType&& aRejectFunction)
      : mResolveFuntion(Move(aResolveFuntion)), mRejectFunction(Move(aRejectFunction))
    {
    }

    template<typename F>
    typename EnableIf<ArgType1Check<F>::value, void>::Type
    CallHelper(MediaData* aSample, TimeStamp)
    {
      mResolveFuntion(aSample);
    }

    template<typename F>
    typename EnableIf<ArgType2Check<F>::value, void>::Type
    CallHelper(MediaData* aSample, TimeStamp aDecodeStartTime)
    {
      mResolveFuntion(aSample, aDecodeStartTime);
    }

    void OnResolved(MediaData* aSample, TimeStamp aDecodeStartTime) override
    {
      CallHelper<ResolveFunctionType>(aSample, aDecodeStartTime);
    }

    void OnRejected(MediaDecoderReader::NotDecodedReason aReason) override
    {
      mRejectFunction(aReason);
    }

    ResolveFunctionType mResolveFuntion;
    RejectFunctionType mRejectFunction;
  };

  struct WaitForDataCallbackBase
  {
    virtual ~WaitForDataCallbackBase() {}
    virtual void OnResolved(MediaData::Type aType) = 0;
    virtual void OnRejected(WaitForDataRejectValue aRejection) = 0;
  };

  template<typename ThisType, typename ResolveMethodType, typename RejectMethodType>
  struct WaitForDataMethodCallback : public WaitForDataCallbackBase
  {
    WaitForDataMethodCallback(ThisType* aThis, ResolveMethodType aResolveMethod, RejectMethodType aRejectMethod)
      : mThis(aThis), mResolveMethod(aResolveMethod), mRejectMethod(aRejectMethod)
    {
    }

    void OnResolved(MediaData::Type aType) override
    {
      (mThis->*mResolveMethod)(aType);
    }

    void OnRejected(WaitForDataRejectValue aRejection) override
    {
      (mThis->*mRejectMethod)(aRejection);
    }

    RefPtr<ThisType> mThis;
    ResolveMethodType mResolveMethod;
    RejectMethodType mRejectMethod;
  };

  template<typename ResolveFunctionType, typename RejectFunctionType>
  struct WaitForDataFunctionCallback : public WaitForDataCallbackBase
  {
    WaitForDataFunctionCallback(ResolveFunctionType&& aResolveFuntion, RejectFunctionType&& aRejectFunction)
      : mResolveFuntion(Move(aResolveFuntion)), mRejectFunction(Move(aRejectFunction))
    {
    }

    void OnResolved(MediaData::Type aType) override
    {
      mResolveFuntion(aType);
    }

    void OnRejected(WaitForDataRejectValue aRejection) override
    {
      mRejectFunction(aRejection);
    }

    ResolveFunctionType mResolveFuntion;
    RejectFunctionType mRejectFunction;
  };

public:
  MediaDecoderReaderWrapper(bool aIsRealTime,
                            AbstractThread* aOwnerThread,
                            MediaDecoderReader* aReader);

  media::TimeUnit StartTime() const;
  RefPtr<MetadataPromise> ReadMetadata();
  RefPtr<HaveStartTimePromise> AwaitStartTime();

  template<typename ThisType, typename ResolveMethodType, typename RejectMethodType>
  CallbackID
  SetAudioCallback(ThisType* aThisVal,
                   ResolveMethodType aResolveMethod,
                   RejectMethodType aRejectMethod)
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
    MOZ_ASSERT(!mRequestAudioDataCB,
               "Please cancel the original callback before setting a new one.");

    mRequestAudioDataCB.reset(
      new MethodCallback<ThisType, ResolveMethodType, RejectMethodType>(
            aThisVal, aResolveMethod, aRejectMethod));

    return mAudioCallbackID;
  }

  template<typename ResolveFunction, typename RejectFunction>
  CallbackID
  SetAudioCallback(ResolveFunction&& aResolveFunction,
                   RejectFunction&& aRejectFunction)
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
    MOZ_ASSERT(!mRequestAudioDataCB,
               "Please cancel the original callback before setting a new one.");

    mRequestAudioDataCB.reset(
      new FunctionCallback<ResolveFunction, RejectFunction>(
            Move(aResolveFunction), Move(aRejectFunction)));

    return mAudioCallbackID;
  }

  template<typename ThisType, typename ResolveMethodType, typename RejectMethodType>
  CallbackID
  SetVideoCallback(ThisType* aThisVal,
                   ResolveMethodType aResolveMethod,
                   RejectMethodType aRejectMethod)
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
    MOZ_ASSERT(!mRequestVideoDataCB,
               "Please cancel the original callback before setting a new one.");

    mRequestVideoDataCB.reset(
      new MethodCallback<ThisType, ResolveMethodType, RejectMethodType>(
            aThisVal, aResolveMethod, aRejectMethod));

    return mVideoCallbackID;
  }

  template<typename ResolveFunction, typename RejectFunction>
  CallbackID
  SetVideoCallback(ResolveFunction&& aResolveFunction,
                   RejectFunction&& aRejectFunction)
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
    MOZ_ASSERT(!mRequestVideoDataCB,
               "Please cancel the original callback before setting a new one.");

    mRequestVideoDataCB.reset(
      new FunctionCallback<ResolveFunction, RejectFunction>(
            Move(aResolveFunction), Move(aRejectFunction)));

    return mVideoCallbackID;
  }

  template<typename ThisType, typename ResolveMethodType, typename RejectMethodType>
  CallbackID
  SetWaitAudioCallback(ThisType* aThisVal,
                       ResolveMethodType aResolveMethod,
                       RejectMethodType aRejectMethod)
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
    MOZ_ASSERT(!mWaitAudioDataCB,
               "Please cancel the original callback before setting a new one.");

    mWaitAudioDataCB.reset(
      new WaitForDataMethodCallback<ThisType, ResolveMethodType, RejectMethodType>(
            aThisVal, aResolveMethod, aRejectMethod));

    return mWaitAudioCallbackID;
  }

  template<typename ResolveFunction, typename RejectFunction>
  CallbackID
  SetWaitAudioCallback(ResolveFunction&& aResolveFunction,
                       RejectFunction&& aRejectFunction)
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
    MOZ_ASSERT(!mWaitAudioDataCB,
               "Please cancel the original callback before setting a new one.");

    mWaitAudioDataCB.reset(
      new WaitForDataFunctionCallback<ResolveFunction, RejectFunction>(
            Move(aResolveFunction), Move(aRejectFunction)));

    return mWaitAudioCallbackID;
  }

  template<typename ThisType, typename ResolveMethodType, typename RejectMethodType>
  CallbackID
  SetWaitVideoCallback(ThisType* aThisVal,
                       ResolveMethodType aResolveMethod,
                       RejectMethodType aRejectMethod)
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
    MOZ_ASSERT(!mWaitVideoDataCB,
               "Please cancel the original callback before setting a new one.");

    mWaitVideoDataCB.reset(
      new WaitForDataMethodCallback<ThisType, ResolveMethodType, RejectMethodType>(
            aThisVal, aResolveMethod, aRejectMethod));

    return mWaitVideoCallbackID;
  }

  template<typename ResolveFunction, typename RejectFunction>
  CallbackID
  SetWaitVideoCallback(ResolveFunction&& aResolveFunction,
                       RejectFunction&& aRejectFunction)
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
    MOZ_ASSERT(!mWaitVideoDataCB,
               "Please cancel the original callback before setting a new one.");

    mWaitVideoDataCB.reset(
      new WaitForDataFunctionCallback<ResolveFunction, RejectFunction>(
            Move(aResolveFunction), Move(aRejectFunction)));

    return mWaitVideoCallbackID;
  }

  void CancelAudioCallback(CallbackID aID);
  void CancelVideoCallback(CallbackID aID);
  void CancelWaitAudioCallback(CallbackID aID);
  void CancelWaitVideoCallback(CallbackID aID);

  // NOTE: please set callbacks before requesting audio/video data!
  void RequestAudioData();
  void RequestVideoData(bool aSkipToNextKeyframe, media::TimeUnit aTimeThreshold);

  // NOTE: please set callbacks before invoking WaitForData()!
  void WaitForData(MediaData::Type aType);

  bool IsRequestingAudioData() const;
  bool IsRequestingVideoData() const;
  bool IsWaitingAudioData() const;
  bool IsWaitingVideoData() const;

  RefPtr<SeekPromise> Seek(SeekTarget aTarget, media::TimeUnit aEndTime);
  RefPtr<BufferedUpdatePromise> UpdateBufferedWithPromise();
  RefPtr<ShutdownPromise> Shutdown();

  void ReleaseMediaResources();
  void SetIdle();
  void ResetDecode(TargetQueues aQueues);

  nsresult Init() { return mReader->Init(); }
  bool IsWaitForDataSupported() const { return mReader->IsWaitForDataSupported(); }
  bool IsAsync() const { return mReader->IsAsync(); }
  bool UseBufferingHeuristics() const { return mReader->UseBufferingHeuristics(); }
  bool ForceZeroStartTime() const { return mReader->ForceZeroStartTime(); }

  bool VideoIsHardwareAccelerated() const {
    return mReader->VideoIsHardwareAccelerated();
  }
  TimedMetadataEventSource& TimedMetadataEvent() {
    return mReader->TimedMetadataEvent();
  }
  MediaEventSource<void>& OnMediaNotSeekable() {
    return mReader->OnMediaNotSeekable();
  }
  size_t SizeOfVideoQueueInBytes() const {
    return mReader->SizeOfVideoQueueInBytes();
  }
  size_t SizeOfAudioQueueInBytes() const {
    return mReader->SizeOfAudioQueueInBytes();
  }
  size_t SizeOfAudioQueueInFrames() const {
    return mReader->SizeOfAudioQueueInFrames();
  }
  size_t SizeOfVideoQueueInFrames() const {
    return mReader->SizeOfVideoQueueInFrames();
  }
  void ReadUpdatedMetadata(MediaInfo* aInfo) {
    mReader->ReadUpdatedMetadata(aInfo);
  }
  AbstractCanonical<media::TimeIntervals>* CanonicalBuffered() {
    return mReader->CanonicalBuffered();
  }

#ifdef MOZ_EME
  void SetCDMProxy(CDMProxy* aProxy) { mReader->SetCDMProxy(aProxy); }
#endif

private:
  ~MediaDecoderReaderWrapper();

  void OnMetadataRead(MetadataHolder* aMetadata);
  void OnMetadataNotRead() {}
  void OnSampleDecoded(CallbackBase* aCallback, MediaData* aSample,
                       TimeStamp aVideoDecodeStartTime);
  void OnNotDecoded(CallbackBase* aCallback,
                    MediaDecoderReader::NotDecodedReason aReason);

  UniquePtr<WaitForDataCallbackBase>& WaitCallbackRef(MediaData::Type aType);
  MozPromiseRequestHolder<WaitForDataPromise>& WaitRequestRef(MediaData::Type aType);

  const bool mForceZeroStartTime;
  const RefPtr<AbstractThread> mOwnerThread;
  const RefPtr<MediaDecoderReader> mReader;

  bool mShutdown = false;
  RefPtr<StartTimeRendezvous> mStartTimeRendezvous;

  UniquePtr<CallbackBase> mRequestAudioDataCB;
  UniquePtr<CallbackBase> mRequestVideoDataCB;
  UniquePtr<WaitForDataCallbackBase> mWaitAudioDataCB;
  UniquePtr<WaitForDataCallbackBase> mWaitVideoDataCB;
  MozPromiseRequestHolder<MediaDataPromise> mAudioDataRequest;
  MozPromiseRequestHolder<MediaDataPromise> mVideoDataRequest;
  MozPromiseRequestHolder<WaitForDataPromise> mAudioWaitRequest;
  MozPromiseRequestHolder<WaitForDataPromise> mVideoWaitRequest;

  /*
   * These callback ids are used to prevent mis-canceling callback.
   */
  CallbackID mAudioCallbackID;
  CallbackID mVideoCallbackID;
  CallbackID mWaitAudioCallbackID;
  CallbackID mWaitVideoCallbackID;
};

} // namespace mozilla

#endif // MediaDecoderReaderWrapper_h_
