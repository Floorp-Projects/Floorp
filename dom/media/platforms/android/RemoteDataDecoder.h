/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RemoteDataDecoder_h_
#define RemoteDataDecoder_h_

#include "AndroidDecoderModule.h"
#include "FennecJNIWrappers.h"
#include "SurfaceTexture.h"
#include "TimeUnits.h"
#include "mozilla/Maybe.h"
#include "mozilla/Monitor.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(RemoteDataDecoder, MediaDataDecoder);

class RemoteDataDecoder : public MediaDataDecoder,
                          public DecoderDoctorLifeLogger<RemoteDataDecoder> {
 public:
  static already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams, const nsString& aDrmStubId,
      CDMProxy* aProxy);

  static already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams, const nsString& aDrmStubId,
      CDMProxy* aProxy);

  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override {
    return NS_LITERAL_CSTRING("android decoder (remote)");
  }

 protected:
  virtual ~RemoteDataDecoder() {}
  RemoteDataDecoder(MediaData::Type aType, const nsACString& aMimeType,
                    java::sdk::MediaFormat::Param aFormat,
                    const nsString& aDrmStubId, TaskQueue* aTaskQueue);

  // Methods only called on mTaskQueue.
  RefPtr<FlushPromise> ProcessFlush();
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);
  RefPtr<ShutdownPromise> ProcessShutdown();
  void UpdateInputStatus(int64_t aTimestamp, bool aProcessed);
  void UpdateOutputStatus(RefPtr<MediaData>&& aSample);
  void ReturnDecodedData();
  void DrainComplete();
  void Error(const MediaResult& aError);
  void AssertOnTaskQueue() const {
    MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  }

  enum class State { DRAINED, DRAINABLE, DRAINING, SHUTDOWN };
  void SetState(State aState) {
    AssertOnTaskQueue();
    mState = aState;
  }
  State GetState() const {
    AssertOnTaskQueue();
    return mState;
  }

  // Whether the sample will be used.
  virtual bool IsUsefulData(const RefPtr<MediaData>& aSample) { return true; }

  MediaData::Type mType;

  nsAutoCString mMimeType;
  java::sdk::MediaFormat::GlobalRef mFormat;

  java::CodecProxy::GlobalRef mJavaDecoder;
  java::CodecProxy::NativeCallbacks::GlobalRef mJavaCallbacks;
  nsString mDrmStubId;

  RefPtr<TaskQueue> mTaskQueue;

  // Preallocated Java object used as a reusable storage for input buffer
  // information. Contents must be changed only on mTaskQueue.
  java::sdk::BufferInfo::GlobalRef mInputBufferInfo;

 private:
  enum class PendingOp { INCREASE, DECREASE, CLEAR };
  void UpdatePendingInputStatus(PendingOp aOp);
  size_t HasPendingInputs() {
    AssertOnTaskQueue();
    return mNumPendingInputs > 0;
  }

  // The following members must only be accessed on mTaskqueue.
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseHolder<DecodePromise> mDrainPromise;
  DecodedData mDecodedData;
  State mState = State::DRAINED;
  size_t mNumPendingInputs;
};

}  // namespace mozilla

#endif
