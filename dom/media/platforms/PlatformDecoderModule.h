/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PlatformDecoderModule_h_)
#define PlatformDecoderModule_h_

#include "MediaDecoderReader.h"
#include "mozilla/MozPromise.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsTArray.h"
#include "mozilla/RefPtr.h"
#include "GMPService.h"
#include <queue>
#include "MediaResult.h"

namespace mozilla {
class TrackInfo;
class AudioInfo;
class VideoInfo;
class MediaRawData;
class DecoderDoctorDiagnostics;

namespace layers {
class ImageContainer;
} // namespace layers

class MediaDataDecoder;
class MediaDataDecoderCallback;
class TaskQueue;
class CDMProxy;

static LazyLogModule sPDMLog("PlatformDecoderModule");

struct CreateDecoderParams {
  explicit CreateDecoderParams(const TrackInfo& aConfig)
    : mConfig(aConfig)
  {}

  template <typename T1, typename... Ts>
  CreateDecoderParams(const TrackInfo& aConfig, T1&& a1, Ts&&... args)
    : mConfig(aConfig)
  {
    Set(mozilla::Forward<T1>(a1), mozilla::Forward<Ts>(args)...);
  }

  const VideoInfo& VideoConfig() const
  {
    MOZ_ASSERT(mConfig.IsVideo());
    return *mConfig.GetAsVideoInfo();
  }

  const AudioInfo& AudioConfig() const
  {
    MOZ_ASSERT(mConfig.IsAudio());
    return *mConfig.GetAsAudioInfo();
  }

  const TrackInfo& mConfig;
  TaskQueue* mTaskQueue = nullptr;
  MediaDataDecoderCallback* mCallback = nullptr;
  DecoderDoctorDiagnostics* mDiagnostics = nullptr;
  layers::ImageContainer* mImageContainer = nullptr;
  layers::LayersBackend mLayersBackend = layers::LayersBackend::LAYERS_NONE;
  RefPtr<GMPCrashHelper> mCrashHelper;
  bool mUseBlankDecoder = false;

private:
  void Set(TaskQueue* aTaskQueue) { mTaskQueue = aTaskQueue; }
  void Set(MediaDataDecoderCallback* aCallback) { mCallback = aCallback; }
  void Set(DecoderDoctorDiagnostics* aDiagnostics) { mDiagnostics = aDiagnostics; }
  void Set(layers::ImageContainer* aImageContainer) { mImageContainer = aImageContainer; }
  void Set(layers::LayersBackend aLayersBackend) { mLayersBackend = aLayersBackend; }
  void Set(GMPCrashHelper* aCrashHelper) { mCrashHelper = aCrashHelper; }
  void Set(bool aUseBlankDecoder) { mUseBlankDecoder = aUseBlankDecoder; }
  template <typename T1, typename T2, typename... Ts>
  void Set(T1&& a1, T2&& a2, Ts&&... args)
  {
    Set(mozilla::Forward<T1>(a1));
    Set(mozilla::Forward<T2>(a2), mozilla::Forward<Ts>(args)...);
  }
};

// The PlatformDecoderModule interface is used by the MediaFormatReader to
// abstract access to decoders provided by various
// platforms.
// Each platform (Windows, MacOSX, Linux, B2G etc) must implement a
// PlatformDecoderModule to provide access to its decoders in order to get
// decompressed H.264/AAC from the MediaFormatReader.
//
// Decoding is asynchronous, and should be performed on the task queue
// provided if the underlying platform isn't already exposing an async API.
//
// A cross-platform decoder module that discards input and produces "blank"
// output samples exists for testing, and is created when the pref
// "media.use-blank-decoder" is true.

class PlatformDecoderModule {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PlatformDecoderModule)

  // Perform any per-instance initialization.
  // This is called on the decode task queue.
  virtual nsresult Startup() { return NS_OK; };

  // Indicates if the PlatformDecoderModule supports decoding of aMimeType.
  virtual bool SupportsMimeType(const nsACString& aMimeType,
                                DecoderDoctorDiagnostics* aDiagnostics) const = 0;

  enum class ConversionRequired : uint8_t {
    kNeedNone,
    kNeedAVCC,
    kNeedAnnexB,
  };

  // Indicates that the decoder requires a specific format.
  // The PlatformDecoderModule will convert the demuxed data accordingly before
  // feeding it to MediaDataDecoder::Input.
  virtual ConversionRequired DecoderNeedsConversion(const TrackInfo& aConfig) const = 0;

protected:
  PlatformDecoderModule() {}
  virtual ~PlatformDecoderModule() {}

  friend class H264Converter;
  friend class PDMFactory;

  // Creates a Video decoder. The layers backend is passed in so that
  // decoders can determine whether hardware accelerated decoding can be used.
  // Asynchronous decoding of video should be done in runnables dispatched
  // to aVideoTaskQueue. If the task queue isn't needed, the decoder should
  // not hold a reference to it.
  // Output and errors should be returned to the reader via aCallback.
  // On Windows the task queue's threads in have MSCOM initialized with
  // COINIT_MULTITHREADED.
  // Returns nullptr if the decoder can't be created.
  // It is safe to store a reference to aConfig.
  // This is called on the decode task queue.
  virtual already_AddRefed<MediaDataDecoder>
  CreateVideoDecoder(const CreateDecoderParams& aParams) = 0;

  // Creates an Audio decoder with the specified properties.
  // Asynchronous decoding of audio should be done in runnables dispatched to
  // aAudioTaskQueue. If the task queue isn't needed, the decoder should
  // not hold a reference to it.
  // Output and errors should be returned to the reader via aCallback.
  // Returns nullptr if the decoder can't be created.
  // On Windows the task queue's threads in have MSCOM initialized with
  // COINIT_MULTITHREADED.
  // It is safe to store a reference to aConfig.
  // This is called on the decode task queue.
  virtual already_AddRefed<MediaDataDecoder>
  CreateAudioDecoder(const CreateDecoderParams& aParams) = 0;
};

// A callback used by MediaDataDecoder to return output/errors to the
// MediaFormatReader.
// Implementation is threadsafe, and can be called on any thread.
class MediaDataDecoderCallback {
public:
  virtual ~MediaDataDecoderCallback() {}

  // Called by MediaDataDecoder when a sample has been decoded.
  virtual void Output(MediaData* aData) = 0;

  // Denotes an error in the decoding process. The reader will stop calling
  // the decoder.
  virtual void Error(const MediaResult& aError) = 0;

  // Denotes that the last input sample has been inserted into the decoder,
  // and no more output can be produced unless more input is sent.
  // A frame decoding session is completed once InputExhausted has been called.
  // MediaDataDecoder::Input will not be called again until InputExhausted has
  // been called.
  virtual void InputExhausted() = 0;

  virtual void DrainComplete() = 0;

  virtual void ReleaseMediaResources() {}

  virtual bool OnReaderTaskQueue() = 0;

  // Denotes that a pending encryption key is preventing more input being fed
  // into the decoder. This only needs to be overridden for callbacks that
  // handle encryption. E.g. benchmarking does not use eme, so this need
  // not be overridden in that case.
  virtual void WaitingForKey() {}
};

// MediaDataDecoder is the interface exposed by decoders created by the
// PlatformDecoderModule's Create*Decoder() functions. The type of
// media data that the decoder accepts as valid input and produces as
// output is determined when the MediaDataDecoder is created.
//
// Unless otherwise noted, all functions are only called on the decode task
// queue.  An exception is the MediaDataDecoder in
// MediaFormatReader::IsVideoAccelerated() for which all calls (Init(),
// IsHardwareAccelerated(), and Shutdown()) are from the main thread.
//
// Don't block inside these functions, unless it's explicitly noted that you
// should (like in Flush()).
//
// Decoding is done asynchronously. Any async work can be done on the
// TaskQueue passed into the PlatformDecoderModules's Create*Decoder()
// function. This may not be necessary for platforms with async APIs
// for decoding.
//
// If an error occurs at any point after the Init promise has been
// completed, then Error() must be called on the associated
// MediaDataDecoderCallback.
class MediaDataDecoder {
protected:
  virtual ~MediaDataDecoder() {};

public:
  typedef TrackInfo::TrackType TrackType;
  typedef MozPromise<TrackType, MediaResult, /* IsExclusive = */ true> InitPromise;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataDecoder)

  // Initialize the decoder. The decoder should be ready to decode once
  // promise resolves. The decoder should do any initialization here, rather
  // than in its constructor or PlatformDecoderModule::Create*Decoder(),
  // so that if the MediaFormatReader needs to shutdown during initialization,
  // it can call Shutdown() to cancel this operation. Any initialization
  // that requires blocking the calling thread in this function *must*
  // be done here so that it can be canceled by calling Shutdown()!
  virtual RefPtr<InitPromise> Init() = 0;

  // Inserts a sample into the decoder's decode pipeline.
  virtual void Input(MediaRawData* aSample) = 0;

  // Causes all samples in the decoding pipeline to be discarded. When
  // this function returns, the decoder must be ready to accept new input
  // for decoding. This function is called when the demuxer seeks, before
  // decoding resumes after the seek.
  // While the reader calls Flush(), it ignores all output sent to it;
  // it is safe (but pointless) to send output while Flush is called.
  // The MediaFormatReader will not call Input() while it's calling Flush().
  virtual void Flush() = 0;

  // Causes all complete samples in the pipeline that can be decoded to be
  // output. If the decoder can't produce samples from the current output,
  // it drops the input samples. The decoder may be holding onto samples
  // that are required to decode samples that it expects to get in future.
  // This is called when the demuxer reaches end of stream.
  // The MediaFormatReader will not call Input() while it's calling Drain().
  // This function is asynchronous. The MediaDataDecoder must call
  // MediaDataDecoderCallback::DrainComplete() once all remaining
  // samples have been output.
  virtual void Drain() = 0;

  // Cancels all init/input/drain operations, and shuts down the
  // decoder. The platform decoder should clean up any resources it's using
  // and release memory etc. Shutdown() must block until the decoder has
  // completed shutdown. The reader calls Flush() before calling Shutdown().
  // The reader will delete the decoder once Shutdown() returns.
  // The MediaDataDecoderCallback *must* not be called after Shutdown() has
  // returned.
  virtual void Shutdown() = 0;

  // Called from the state machine task queue or main thread.
  // Decoder needs to decide whether or not hardware accelearation is supported
  // after creating. It doesn't need to call Init() before calling this function.
  virtual bool IsHardwareAccelerated(nsACString& aFailureReason) const { return false; }

  // Return the name of the MediaDataDecoder, only used for decoding.
  // Only return a static const string, as the information may be accessed
  // in a non thread-safe fashion.
  virtual const char* GetDescriptionName() const = 0;

  // Set a hint of seek target time to decoder. Decoder will drop any decoded
  // data which pts is smaller than this value. This threshold needs to be clear
  // after reset decoder.
  // Decoder may not honor this value. However, it'd be better that
  // video decoder implements this API to improve seek performance.
  // Note: it should be called before Input() or after Flush().
  virtual void SetSeekThreshold(const media::TimeUnit& aTime) {}
};

} // namespace mozilla

#endif
