/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PlatformDecoderModule_h_)
#define PlatformDecoderModule_h_

#include "FlushableTaskQueue.h"
#include "MediaDecoderReader.h"
#include "mozilla/MozPromise.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsTArray.h"
#include "mozilla/RefPtr.h"
#include <queue>

namespace mozilla {
class TrackInfo;
class AudioInfo;
class VideoInfo;
class MediaRawData;

namespace layers {
class ImageContainer;
} // namespace layers

class MediaDataDecoder;
class MediaDataDecoderCallback;
class FlushableTaskQueue;
class CDMProxy;

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
// "media.fragmented-mp4.use-blank-decoder" is true.

class PlatformDecoderModule {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PlatformDecoderModule)

  // Call on the main thread to initialize the static state
  // needed by Create().
  static void Init();

  // Factory method that creates the appropriate PlatformDecoderModule for
  // the platform we're running on. Caller is responsible for deleting this
  // instance. It's expected that there will be multiple
  // PlatformDecoderModules alive at the same time.
  // This is called on the decode task queue.
  static already_AddRefed<PlatformDecoderModule> Create();

  // Perform any per-instance initialization.
  // This is called on the decode task queue.
  virtual nsresult Startup() { return NS_OK; };

  // Creates a decoder.
  // See CreateVideoDecoder and CreateAudioDecoder for implementation details.
  virtual already_AddRefed<MediaDataDecoder>
  CreateDecoder(const TrackInfo& aConfig,
                FlushableTaskQueue* aTaskQueue,
                MediaDataDecoderCallback* aCallback,
                layers::LayersBackend aLayersBackend = layers::LayersBackend::LAYERS_NONE,
                layers::ImageContainer* aImageContainer = nullptr)
  {
    MOZ_CRASH();
  }

  // Indicates if the PlatformDecoderModule supports decoding of aMimeType.
  virtual bool SupportsMimeType(const nsACString& aMimeType) = 0;

  enum ConversionRequired {
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
  CreateVideoDecoder(const VideoInfo& aConfig,
                     layers::LayersBackend aLayersBackend,
                     layers::ImageContainer* aImageContainer,
                     FlushableTaskQueue* aVideoTaskQueue,
                     MediaDataDecoderCallback* aCallback) = 0;

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
  CreateAudioDecoder(const AudioInfo& aConfig,
                     FlushableTaskQueue* aAudioTaskQueue,
                     MediaDataDecoderCallback* aCallback) = 0;
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
  virtual void Error() = 0;

  // Denotes that the last input sample has been inserted into the decoder,
  // and no more output can be produced unless more input is sent.
  virtual void InputExhausted() = 0;

  virtual void DrainComplete() = 0;

  virtual void ReleaseMediaResources() {};

  virtual bool OnReaderTaskQueue() = 0;
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
class MediaDataDecoder {
protected:
  virtual ~MediaDataDecoder() {};

public:
  enum DecoderFailureReason {
    INIT_ERROR,
    CANCELED
  };

  typedef TrackInfo::TrackType TrackType;
  typedef MozPromise<TrackType, DecoderFailureReason, /* IsExclusive = */ true> InitPromise;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDataDecoder)

  // Initialize the decoder. The decoder should be ready to decode once
  // promise resolves. The decoder should do any initialization here, rather
  // than in its constructor or PlatformDecoderModule::Create*Decoder(),
  // so that if the MediaFormatReader needs to shutdown during initialization,
  // it can call Shutdown() to cancel this operation. Any initialization
  // that requires blocking the calling thread in this function *must*
  // be done here so that it can be canceled by calling Shutdown()!
  virtual nsRefPtr<InitPromise> Init() = 0;

  // Inserts a sample into the decoder's decode pipeline.
  virtual nsresult Input(MediaRawData* aSample) = 0;

  // Causes all samples in the decoding pipeline to be discarded. When
  // this function returns, the decoder must be ready to accept new input
  // for decoding. This function is called when the demuxer seeks, before
  // decoding resumes after the seek.
  // While the reader calls Flush(), it ignores all output sent to it;
  // it is safe (but pointless) to send output while Flush is called.
  // The MediaFormatReader will not call Input() while it's calling Flush().
  virtual nsresult Flush() = 0;

  // Causes all complete samples in the pipeline that can be decoded to be
  // output. If the decoder can't produce samples from the current output,
  // it drops the input samples. The decoder may be holding onto samples
  // that are required to decode samples that it expects to get in future.
  // This is called when the demuxer reaches end of stream.
  // The MediaFormatReader will not call Input() while it's calling Drain().
  // This function is asynchronous. The MediaDataDecoder must call
  // MediaDataDecoderCallback::DrainComplete() once all remaining
  // samples have been output.
  virtual nsresult Drain() = 0;

  // Cancels all init/input/drain operations, and shuts down the
  // decoder. The platform decoder should clean up any resources it's using
  // and release memory etc. Shutdown() must block until the decoder has
  // completed shutdown. The reader calls Flush() before calling Shutdown().
  // The reader will delete the decoder once Shutdown() returns.
  // The MediaDataDecoderCallback *must* not be called after Shutdown() has
  // returned.
  virtual nsresult Shutdown() = 0;

  // Called from the state machine task queue or main thread.
  // Decoder needs to decide whether or not hardware accelearation is supported
  // after creating. It doesn't need to call Init() before calling this function.
  virtual bool IsHardwareAccelerated(nsACString& aFailureReason) const { return false; }

  // ConfigurationChanged will be called to inform the video or audio decoder
  // that the format of the next input sample is about to change.
  // If video decoder, aConfig will be a VideoInfo object.
  // If audio decoder, aConfig will be a AudioInfo object.
  virtual nsresult ConfigurationChanged(const TrackInfo& aConfig)
  {
    return NS_OK;
  }
};

} // namespace mozilla

#endif
