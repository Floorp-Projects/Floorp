/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PlatformDecoderModule_h_)
#define PlatformDecoderModule_h_

#include "MediaDecoderReader.h"
#include "mozilla/layers/LayersTypes.h"
#include "nsTArray.h"

namespace mp4_demuxer {
class VideoDecoderConfig;
class AudioDecoderConfig;
struct MP4Sample;
}

namespace mozilla {

namespace layers {
class ImageContainer;
}

class MediaDataDecoder;
typedef int64_t Microseconds;

// The PlatformDecoderModule interface is used by the MP4Reader to abstract
// access to the H264 and AAC decoders provided by various platforms. It
// may be extended to support other codecs in future. Each platform (Windows,
// MacOSX, Linux etc) must implement a PlatformDecoderModule to provide access
// to its decoders in order to get decompressed H.264/AAC from the MP4Reader.
//
// Platforms that don't have a corresponding PlatformDecoderModule won't be
// able to play the H.264/AAC data output by the MP4Reader. In practice this
// means that we won't have fragmented MP4 supported in Media Source
// Extensions on platforms without PlatformDecoderModules.
//
// A cross-platform decoder module that discards input and produces "blank"
// output samples exists for testing, and is created if the pref
// "media.fragmented-mp4.use-blank-decoder" is true.
class PlatformDecoderModule {
public:
  // Call on the main thread to initialize the static state
  // needed by Create().
  static void Init();

  // Factory method that creates the appropriate PlatformDecoderModule for
  // the platform we're running on. Caller is responsible for deleting this
  // instance. It's expected that there will be multiple
  // PlatformDecoderModules alive at the same time. There is one
  // PlatformDecoderModule's created per MP4Reader.
  // This is called on the main thread.
  static PlatformDecoderModule* Create();

  // Called to shutdown the decoder module and cleanup state. This should
  // block until shutdown is complete. This is called after Shutdown() has
  // been called on all MediaDataDecoders created from this
  // PlatformDecoderModule.
  // Called on the main thread only.
  virtual nsresult Shutdown() = 0;

  // Creates and initializes an H.264 decoder. The layers backend is
  // passed in so that decoders can determine whether hardware accelerated
  // decoding can be used. Returns nullptr if the decoder can't be
  // initialized.
  // Called on decode thread.
  virtual MediaDataDecoder* CreateH264Decoder(const mp4_demuxer::VideoDecoderConfig& aConfig,
                                              layers::LayersBackend aLayersBackend,
                                              layers::ImageContainer* aImageContainer) = 0;

  // Creates and initializes an AAC decoder with the specified properties.
  // The raw AAC AudioSpecificConfig as contained in the esds box. Some
  // decoders need that to initialize. The caller owns the AAC config,
  // so it must be copied if it is to be retained by the decoder.
  // Returns nullptr if the decoder can't be initialized.
  // Called on decode thread.
  virtual MediaDataDecoder* CreateAACDecoder(const mp4_demuxer::AudioDecoderConfig& aConfig) = 0;

  // Called when a decode thread is started. Called on decode thread.
  virtual void OnDecodeThreadStart() {}

  // Called just before a decode thread is finishing. Called on decode thread.
  virtual void OnDecodeThreadFinish() {}

  virtual ~PlatformDecoderModule() {}
protected:
  PlatformDecoderModule() {}
  // Caches pref media.fragmented-mp4.use-blank-decoder
  static bool sUseBlankDecoder;
};

// Return value of the MediaDataDecoder functions.
enum DecoderStatus {
  DECODE_STATUS_NOT_ACCEPTING, // Can't accept input at this time. Decoder can produce output.
  DECODE_STATUS_NEED_MORE_INPUT, // Can't produce output. Decoder can accept input.
  DECODE_STATUS_OK,
  DECODE_STATUS_ERROR
};

// MediaDataDecoder is the interface exposed by decoders created by the
// PlatformDecoderModule's Create*Decoder() functions. The type of
// media data that the decoder accepts as valid input and produces as
// output is determined when the MediaDataDecoder is created.
// The decoder is assumed to be in one of three mutually exclusive and
// implicit states: able to accept input, able to produce output, and
// shutdown. The decoder is assumed to be able to accept input by the time
// that it's returned by PlatformDecoderModule::Create*Decoder().
class MediaDataDecoder {
public:
  virtual ~MediaDataDecoder() {};

  // Initialize the decoder. The decoder should be ready to decode after
  // this returns. The decoder should do any initialization here, rather
  // than in its constructor, so that if the MP4Reader needs to Shutdown()
  // during initialization it can call Shutdown() to cancel this.
  // Any initialization that requires blocking the calling thread *must*
  // be done here so that it can be canceled by calling Shutdown()!
  virtual nsresult Init() = 0;

  // Inserts aData into the decoding pipeline. Decoding may begin
  // asynchronously.
  //
  // If the decoder needs to assume ownership of the sample it may do so by
  // calling forget() on aSample.
  //
  // If Input() returns DECODE_STATUS_NOT_ACCEPTING without forget()ing
  // aSample, then the next call will have the same aSample. Otherwise
  // the caller will delete aSample after Input() returns.
  //
  // The MP4Reader calls Input() in a loop until Input() stops returning
  // DECODE_STATUS_OK. Input() should return DECODE_STATUS_NOT_ACCEPTING
  // once the underlying decoder should have enough data to output decoded
  // data.
  //
  // Called on the media decode thread.
  // Returns:
  //  - DECODE_STATUS_OK if input was successfully inserted into
  //    the decode pipeline.
  //  - DECODE_STATUS_NOT_ACCEPTING if the decoder cannot accept any input
  //    at this time. The MP4Reader will assume that the decoder can now
  //    produce one or more output samples, and call the Output() function.
  //    The MP4Reader will call Input() again with the same data later,
  //    after the decoder's Output() function has stopped producing output,
  //    except if Input() called forget() on aSample, whereupon a new sample
  //    will come in next call.
  //  - DECODE_STATUS_ERROR if the decoder has been shutdown, or some
  //    unspecified error.
  // This function should not return DECODE_STATUS_NEED_MORE_INPUT.
  virtual DecoderStatus Input(nsAutoPtr<mp4_demuxer::MP4Sample>& aSample) = 0;

  // Blocks until a decoded sample is produced by the deoder. The MP4Reader
  // calls this until it stops returning DECODE_STATUS_OK.
  // Called on the media decode thread.
  // Returns:
  //  - DECODE_STATUS_OK if an output sample was successfully placed in
  //    aOutData. More samples for output may still be available, the
  //    MP4Reader will call again to check.
  //  - DECODE_STATUS_NEED_MORE_INPUT if the decoder needs more input to
  //    produce a sample. The decoder should return this once it can no
  //    longer produce output. This signals to the MP4Reader that it should
  //    start feeding in data via the Input() function.
  //  - DECODE_STATUS_ERROR if the decoder has been shutdown, or some
  //    unspecified error.
  // This function should not return DECODE_STATUS_NOT_ACCEPTING.
  virtual DecoderStatus Output(nsAutoPtr<MediaData>& aOutData) = 0;

  // Causes all samples in the decoding pipeline to be discarded. When
  // this function returns, the decoder must be ready to accept new input
  // for decoding. This function is called when the demuxer seeks, before
  // decoding resumes after the seek.
  // Called on the media decode thread.
  virtual DecoderStatus Flush() = 0;

  // Cancels all decode operations, and shuts down the decoder. This should
  // block until shutdown is complete. The decoder should return
  // DECODE_STATUS_ERROR for all calls to its functions once this is called.
  // Called on main thread.
  virtual nsresult Shutdown() = 0;

};

} // namespace mozilla

#endif
