/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RemoteDataDecoder_h_
#define RemoteDataDecoder_h_

#include "AndroidDecoderModule.h"
#include "SurfaceTexture.h"
#include "TimeUnits.h"
#include "mozilla/Monitor.h"
#include "mozilla/java/CodecProxyWrappers.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(RemoteDataDecoder, MediaDataDecoder);

class RemoteDataDecoder : public MediaDataDecoder,
                          public DecoderDoctorLifeLogger<RemoteDataDecoder> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RemoteDataDecoder, final);

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
    return "android decoder (remote)"_ns;
  }

 protected:
  virtual ~RemoteDataDecoder() = default;
  RemoteDataDecoder(MediaData::Type aType, const nsACString& aMimeType,
                    java::sdk::MediaFormat::Param aFormat,
                    const nsString& aDrmStubId);

  // Methods only called on mThread.
  void UpdateInputStatus(int64_t aTimestamp, bool aProcessed);
  void UpdateOutputStatus(RefPtr<MediaData>&& aSample);
  void ReturnDecodedData();
  void DrainComplete();
  void Error(const MediaResult& aError);
  void AssertOnThread() const {
    // mThread may not be set if Init hasn't been called first.
    MOZ_ASSERT(!mThread || mThread->IsOnCurrentThread());
  }

  enum class State { DRAINED, DRAINABLE, DRAINING, SHUTDOWN };
  void SetState(State aState) {
    AssertOnThread();
    mState = aState;
  }
  State GetState() const {
    AssertOnThread();
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

  nsCOMPtr<nsISerialEventTarget> mThread;

  // Preallocated Java object used as a reusable storage for input buffer
  // information. Contents must be changed only on mThread.
  java::sdk::MediaCodec::BufferInfo::GlobalRef mInputBufferInfo;

  // Session ID attached to samples. It is returned by CodecProxy::Input().
  // Accessed on mThread only.
  int64_t mSession;

 private:
  enum class PendingOp { INCREASE, DECREASE, CLEAR };
  void UpdatePendingInputStatus(PendingOp aOp);
  size_t HasPendingInputs() {
    AssertOnThread();
    return mNumPendingInputs > 0;
  }

  // Returns true if we are in a state which requires a new decoder to be
  // created. In this case all errors will be reported as
  // NS_ERROR_DOM_MEDIA_NEED_NEW_DECODER to avoid reporting errors as fatal when
  // they can be fixed with a new decoder.
  virtual bool NeedsNewDecoder() const { return false; }

  // The following members must only be accessed on mThread.
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseHolder<DecodePromise> mDrainPromise;
  DecodedData mDecodedData;
  State mState = State::DRAINED;
  size_t mNumPendingInputs;
};

}  // namespace mozilla

#endif
