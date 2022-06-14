/*
 *  Copyright (c) 2012, The WebRTC project authors. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *
 *    * Neither the name of Google nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBRTCGMPVIDEOCODEC_H_
#define WEBRTCGMPVIDEOCODEC_H_

#include <queue>
#include <string>

#include "nsThreadUtils.h"
#include "mozilla/Monitor.h"
#include "mozilla/Mutex.h"
#include "mozilla/Telemetry.h"

#include "mozIGeckoMediaPluginService.h"
#include "MediaConduitInterface.h"
#include "AudioConduit.h"
#include "VideoConduit.h"
#include "api/video/video_frame_type.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "common_video/h264/h264_bitstream_parser.h"

#include "gmp-video-host.h"
#include "GMPVideoDecoderProxy.h"
#include "GMPVideoEncoderProxy.h"

#include "jsapi/PeerConnectionImpl.h"

namespace mozilla {

class GmpInitDoneRunnable : public Runnable {
 public:
  explicit GmpInitDoneRunnable(std::string aPCHandle)
      : Runnable("GmpInitDoneRunnable"),
        mResult(WEBRTC_VIDEO_CODEC_OK),
        mPCHandle(std::move(aPCHandle)) {}

  NS_IMETHOD Run() override {
    Telemetry::Accumulate(Telemetry::WEBRTC_GMP_INIT_SUCCESS, mResult == WEBRTC_VIDEO_CODEC_OK);
    if (mResult == WEBRTC_VIDEO_CODEC_OK) {
      // Might be useful to notify the PeerConnection about successful init
      // someday.
      return NS_OK;
    }

    PeerConnectionWrapper wrapper(mPCHandle);
    if (wrapper.impl()) {
      wrapper.impl()->OnMediaError(mError);
    }
    return NS_OK;
  }

  void Dispatch(int32_t aResult, const std::string& aError = "") {
    mResult = aResult;
    mError = aError;
    nsCOMPtr<nsIThread> mainThread(do_GetMainThread());
    if (mainThread) {
      // For some reason, the compiler on CI is treating |this| as a const
      // pointer, despite the fact that we're in a non-const function. And,
      // interestingly enough, correcting this doesn't require a const_cast.
      mainThread->Dispatch(do_AddRef(static_cast<nsIRunnable*>(this)),
                           NS_DISPATCH_NORMAL);
    }
  }

  int32_t Result() { return mResult; }

 private:
  int32_t mResult;
  const std::string mPCHandle;
  std::string mError;
};

// Hold a frame for later decode
class GMPDecodeData {
 public:
  GMPDecodeData(const webrtc::EncodedImage& aInputImage, bool aMissingFrames,
                int64_t aRenderTimeMs)
      : mImage(aInputImage),
        mMissingFrames(aMissingFrames),
        mRenderTimeMs(aRenderTimeMs) {
    // We want to use this for queuing, and the calling code recycles the
    // buffer on return from Decode()
    MOZ_RELEASE_ASSERT(aInputImage.size() <
                       (std::numeric_limits<size_t>::max() >> 1));
  }

  ~GMPDecodeData() = default;

  const webrtc::EncodedImage mImage;
  const bool mMissingFrames;
  const int64_t mRenderTimeMs;
};

class RefCountedWebrtcVideoEncoder {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RefCountedWebrtcVideoEncoder);

  // Implement sort of WebrtcVideoEncoder interface and support refcounting.
  // (We cannot use |Release|, since that's needed for nsRefPtr)
  virtual int32_t InitEncode(
      const webrtc::VideoCodec* aCodecSettings,
      const webrtc::VideoEncoder::Settings& aSettings) = 0;

  virtual int32_t Encode(
      const webrtc::VideoFrame& aInputImage,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) = 0;

  virtual int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) = 0;

  virtual int32_t Shutdown() = 0;

  virtual int32_t SetRates(
      const webrtc::VideoEncoder::RateControlParameters& aParameters) = 0;

  virtual MediaEventSource<uint64_t>* InitPluginEvent() = 0;

  virtual MediaEventSource<uint64_t>* ReleasePluginEvent() = 0;

  virtual WebrtcVideoEncoder::EncoderInfo GetEncoderInfo() const = 0;

 protected:
  virtual ~RefCountedWebrtcVideoEncoder() = default;
};

class WebrtcGmpVideoEncoder : public GMPVideoEncoderCallbackProxy,
                              public RefCountedWebrtcVideoEncoder {
 public:
  explicit WebrtcGmpVideoEncoder(std::string aPCHandle);

  // Implement VideoEncoder interface, sort of.
  // (We cannot use |Release|, since that's needed for nsRefPtr)
  int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                     const webrtc::VideoEncoder::Settings& aSettings) override;

  int32_t Encode(
      const webrtc::VideoFrame& aInputImage,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) override;

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) override;

  int32_t Shutdown() override;

  int32_t SetRates(
      const webrtc::VideoEncoder::RateControlParameters& aParameters) override;

  WebrtcVideoEncoder::EncoderInfo GetEncoderInfo() const override;

  MediaEventSource<uint64_t>* InitPluginEvent() override {
    return &mInitPluginEvent;
  }

  MediaEventSource<uint64_t>* ReleasePluginEvent() override {
    return &mReleasePluginEvent;
  }

  // GMPVideoEncoderCallback virtual functions.
  virtual void Terminated() override;

  virtual void Encoded(GMPVideoEncodedFrame* aEncodedFrame,
                       const nsTArray<uint8_t>& aCodecSpecificInfo) override;

  virtual void Error(GMPErr aError) override {}

 private:
  virtual ~WebrtcGmpVideoEncoder();

  static void InitEncode_g(const RefPtr<WebrtcGmpVideoEncoder>& aThis,
                           const GMPVideoCodec& aCodecParams,
                           int32_t aNumberOfCores, uint32_t aMaxPayloadSize,
                           const RefPtr<GmpInitDoneRunnable>& aInitDone);
  int32_t GmpInitDone(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost,
                      const GMPVideoCodec& aCodecParams,
                      std::string* aErrorOut);
  int32_t GmpInitDone(GMPVideoEncoderProxy* aGMP, GMPVideoHost* aHost,
                      std::string* aErrorOut);
  int32_t InitEncoderForSize(unsigned short aWidth, unsigned short aHeight,
                             std::string* aErrorOut);
  static void ReleaseGmp_g(const RefPtr<WebrtcGmpVideoEncoder>& aEncoder);
  void Close_g();

  class InitDoneCallback : public GetGMPVideoEncoderCallback {
   public:
    InitDoneCallback(const RefPtr<WebrtcGmpVideoEncoder>& aEncoder,
                     const RefPtr<GmpInitDoneRunnable>& aInitDone,
                     const GMPVideoCodec& aCodecParams)
        : mEncoder(aEncoder),
          mInitDone(aInitDone),
          mCodecParams(aCodecParams) {}

    virtual void Done(GMPVideoEncoderProxy* aGMP,
                      GMPVideoHost* aHost) override {
      std::string errorOut;
      int32_t result =
          mEncoder->GmpInitDone(aGMP, aHost, mCodecParams, &errorOut);

      mInitDone->Dispatch(result, errorOut);
    }

   private:
    const RefPtr<WebrtcGmpVideoEncoder> mEncoder;
    const RefPtr<GmpInitDoneRunnable> mInitDone;
    const GMPVideoCodec mCodecParams;
  };

  static void Encode_g(const RefPtr<WebrtcGmpVideoEncoder>& aEncoder,
                       webrtc::VideoFrame aInputImage,
                       std::vector<webrtc::VideoFrameType> aFrameTypes);
  void RegetEncoderForResolutionChange(
      uint32_t aWidth, uint32_t aHeight,
      const RefPtr<GmpInitDoneRunnable>& aInitDone);

  class InitDoneForResolutionChangeCallback
      : public GetGMPVideoEncoderCallback {
   public:
    InitDoneForResolutionChangeCallback(
        const RefPtr<WebrtcGmpVideoEncoder>& aEncoder,
        const RefPtr<GmpInitDoneRunnable>& aInitDone, uint32_t aWidth,
        uint32_t aHeight)
        : mEncoder(aEncoder),
          mInitDone(aInitDone),
          mWidth(aWidth),
          mHeight(aHeight) {}

    virtual void Done(GMPVideoEncoderProxy* aGMP,
                      GMPVideoHost* aHost) override {
      std::string errorOut;
      int32_t result = mEncoder->GmpInitDone(aGMP, aHost, &errorOut);
      if (result != WEBRTC_VIDEO_CODEC_OK) {
        mInitDone->Dispatch(result, errorOut);
        return;
      }

      result = mEncoder->InitEncoderForSize(mWidth, mHeight, &errorOut);
      mInitDone->Dispatch(result, errorOut);
    }

   private:
    const RefPtr<WebrtcGmpVideoEncoder> mEncoder;
    const RefPtr<GmpInitDoneRunnable> mInitDone;
    const uint32_t mWidth;
    const uint32_t mHeight;
  };

  static int32_t SetRates_g(RefPtr<WebrtcGmpVideoEncoder> aThis,
                            uint32_t aNewBitRateKbps, Maybe<double> aFrameRate);

  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  nsCOMPtr<nsIThread> mGMPThread;
  GMPVideoEncoderProxy* mGMP;
  // Used to handle a race where Release() is called while init is in progress
  bool mInitting;
  GMPVideoHost* mHost;
  GMPVideoCodec mCodecParams;
  uint32_t mMaxPayloadSize;
  webrtc::CodecSpecificInfo mCodecSpecificInfo;
  webrtc::H264BitstreamParser mH264BitstreamParser;
  // Protects mCallback
  Mutex mCallbackMutex MOZ_UNANNOTATED;
  webrtc::EncodedImageCallback* mCallback;
  Maybe<uint64_t> mCachedPluginId;
  const std::string mPCHandle;

  struct InputImageData {
    int64_t timestamp_us;
  };
  // Map rtp time -> input image data
  DataMutex<std::map<uint32_t, InputImageData>> mInputImageMap;

  MediaEventProducer<uint64_t> mInitPluginEvent;
  MediaEventProducer<uint64_t> mReleasePluginEvent;
};

// Basically a strong ref to a RefCountedWebrtcVideoEncoder, that also
// translates from Release() to RefCountedWebrtcVideoEncoder::Shutdown(),
// since we need RefCountedWebrtcVideoEncoder::Release() for managing the
// refcount. The webrtc.org code gets one of these, so it doesn't unilaterally
// delete the "real" encoder.
class WebrtcVideoEncoderProxy : public WebrtcVideoEncoder {
 public:
  explicit WebrtcVideoEncoderProxy(
      RefPtr<RefCountedWebrtcVideoEncoder> aEncoder)
      : mEncoderImpl(std::move(aEncoder)) {}

  virtual ~WebrtcVideoEncoderProxy() {
    RegisterEncodeCompleteCallback(nullptr);
  }

  MediaEventSource<uint64_t>* InitPluginEvent() override {
    return mEncoderImpl->InitPluginEvent();
  }

  MediaEventSource<uint64_t>* ReleasePluginEvent() override {
    return mEncoderImpl->ReleasePluginEvent();
  }

  int32_t InitEncode(const webrtc::VideoCodec* aCodecSettings,
                     const WebrtcVideoEncoder::Settings& aSettings) override {
    return mEncoderImpl->InitEncode(aCodecSettings, aSettings);
  }

  int32_t Encode(
      const webrtc::VideoFrame& aInputImage,
      const std::vector<webrtc::VideoFrameType>* aFrameTypes) override {
    return mEncoderImpl->Encode(aInputImage, aFrameTypes);
  }

  int32_t RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* aCallback) override {
    return mEncoderImpl->RegisterEncodeCompleteCallback(aCallback);
  }

  int32_t Release() override { return mEncoderImpl->Shutdown(); }

  void SetRates(const RateControlParameters& aParameters) override {
    mEncoderImpl->SetRates(aParameters);
  }

  EncoderInfo GetEncoderInfo() const override {
    return mEncoderImpl->GetEncoderInfo();
  }

 private:
  const RefPtr<RefCountedWebrtcVideoEncoder> mEncoderImpl;
};

class WebrtcGmpVideoDecoder : public GMPVideoDecoderCallbackProxy {
 public:
  explicit WebrtcGmpVideoDecoder(std::string aPCHandle);
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebrtcGmpVideoDecoder);

  // Implement VideoEncoder interface, sort of.
  // (We cannot use |Release|, since that's needed for nsRefPtr)
  virtual int32_t InitDecode(const webrtc::VideoCodec* aCodecSettings,
                             int32_t aNumberOfCores);
  virtual int32_t Decode(const webrtc::EncodedImage& aInputImage,
                         bool aMissingFrames, int64_t aRenderTimeMs);
  virtual int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* aCallback);

  virtual int32_t ReleaseGmp();

  MediaEventSource<uint64_t>* InitPluginEvent() { return &mInitPluginEvent; }

  MediaEventSource<uint64_t>* ReleasePluginEvent() {
    return &mReleasePluginEvent;
  }

  // GMPVideoDecoderCallbackProxy
  virtual void Terminated() override;

  virtual void Decoded(GMPVideoi420Frame* aDecodedFrame) override;

  virtual void ReceivedDecodedReferenceFrame(
      const uint64_t aPictureId) override {
    MOZ_CRASH();
  }

  virtual void ReceivedDecodedFrame(const uint64_t aPictureId) override {
    MOZ_CRASH();
  }

  virtual void InputDataExhausted() override {}

  virtual void DrainComplete() override {}

  virtual void ResetComplete() override {}

  virtual void Error(GMPErr aError) override { mDecoderStatus = aError; }

 private:
  virtual ~WebrtcGmpVideoDecoder();

  static void InitDecode_g(const RefPtr<WebrtcGmpVideoDecoder>& aThis,
                           const webrtc::VideoCodec* aCodecSettings,
                           int32_t aNumberOfCores,
                           const RefPtr<GmpInitDoneRunnable>& aInitDone);
  int32_t GmpInitDone(GMPVideoDecoderProxy* aGMP, GMPVideoHost* aHost,
                      std::string* aErrorOut);
  static void ReleaseGmp_g(const RefPtr<WebrtcGmpVideoDecoder>& aDecoder);
  void Close_g();

  class InitDoneCallback : public GetGMPVideoDecoderCallback {
   public:
    explicit InitDoneCallback(const RefPtr<WebrtcGmpVideoDecoder>& aDecoder,
                              const RefPtr<GmpInitDoneRunnable>& aInitDone)
        : mDecoder(aDecoder), mInitDone(aInitDone) {}

    virtual void Done(GMPVideoDecoderProxy* aGMP,
                      GMPVideoHost* aHost) override {
      std::string errorOut;
      int32_t result = mDecoder->GmpInitDone(aGMP, aHost, &errorOut);

      mInitDone->Dispatch(result, errorOut);
    }

   private:
    const RefPtr<WebrtcGmpVideoDecoder> mDecoder;
    const RefPtr<GmpInitDoneRunnable> mInitDone;
  };

  static void Decode_g(const RefPtr<WebrtcGmpVideoDecoder>& aThis,
                       UniquePtr<GMPDecodeData>&& aDecodeData);

  nsCOMPtr<mozIGeckoMediaPluginService> mMPS;
  nsCOMPtr<nsIThread> mGMPThread;
  GMPVideoDecoderProxy* mGMP;  // Addref is held for us
  // Used to handle a race where Release() is called while init is in progress
  bool mInitting;
  // Frames queued for decode while mInitting is true
  nsTArray<UniquePtr<GMPDecodeData>> mQueuedFrames;
  GMPVideoHost* mHost;
  // Protects mCallback
  Mutex mCallbackMutex MOZ_UNANNOTATED;
  webrtc::DecodedImageCallback* mCallback;
  Maybe<uint64_t> mCachedPluginId;
  Atomic<GMPErr, ReleaseAcquire> mDecoderStatus;
  const std::string mPCHandle;

  MediaEventProducer<uint64_t> mInitPluginEvent;
  MediaEventProducer<uint64_t> mReleasePluginEvent;
};

// Basically a strong ref to a WebrtcGmpVideoDecoder, that also translates
// from Release() to WebrtcGmpVideoDecoder::ReleaseGmp(), since we need
// WebrtcGmpVideoDecoder::Release() for managing the refcount.
// The webrtc.org code gets one of these, so it doesn't unilaterally delete
// the "real" encoder.
class WebrtcVideoDecoderProxy : public WebrtcVideoDecoder {
 public:
  explicit WebrtcVideoDecoderProxy(std::string aPCHandle)
      : mDecoderImpl(new WebrtcGmpVideoDecoder(std::move(aPCHandle))) {}

  virtual ~WebrtcVideoDecoderProxy() {
    RegisterDecodeCompleteCallback(nullptr);
  }

  MediaEventSource<uint64_t>* InitPluginEvent() override {
    return mDecoderImpl->InitPluginEvent();
  }

  MediaEventSource<uint64_t>* ReleasePluginEvent() override {
    return mDecoderImpl->ReleasePluginEvent();
  }

  int32_t InitDecode(const webrtc::VideoCodec* aCodecSettings,
                     int32_t aNumberOfCores) override {
    return mDecoderImpl->InitDecode(aCodecSettings, aNumberOfCores);
  }

  int32_t Decode(const webrtc::EncodedImage& aInputImage, bool aMissingFrames,
                 int64_t aRenderTimeMs) override {
    return mDecoderImpl->Decode(aInputImage, aMissingFrames, aRenderTimeMs);
  }

  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* aCallback) override {
    return mDecoderImpl->RegisterDecodeCompleteCallback(aCallback);
  }

  int32_t Release() override { return mDecoderImpl->ReleaseGmp(); }

 private:
  const RefPtr<WebrtcGmpVideoDecoder> mDecoderImpl;
};

}  // namespace mozilla

#endif
