/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(PlatformDecoderModule_h_)
#define PlatformDecoderModule_h_

#include "MediaDecoderReader.h"
#include "mozilla/layers/LayersTypes.h"

namespace mozilla {

namespace layers {
class ImageContainer;
}

typedef int64_t Microseconds;

enum DecoderStatus {
  DECODE_STATUS_NOT_ACCEPTING, // Can't accept input at this time.
  DECODE_STATUS_NEED_MORE_INPUT, // Nothing in pipeline to produce output with at this time.
  DECODE_STATUS_OK,
  DECODE_STATUS_ERROR
};

class MediaDataDecoder {
public:
  virtual ~MediaDataDecoder() {};

  virtual nsresult Shutdown() = 0;

  // Returns true if future decodes may produce output, or false
  // on end of segment.
  // Inserts data into the decoder's pipeline.
  virtual DecoderStatus Input(const uint8_t* aData,
                              uint32_t aLength,
                              Microseconds aDTS,
                              Microseconds aPTS,
                              int64_t aOffsetInStream) = 0;

  // Blocks until decoded sample is produced by the deoder.
  virtual DecoderStatus Output(nsAutoPtr<MediaData>& aOutData) = 0;

  virtual DecoderStatus Flush() = 0;
};

class PlatformDecoderModule {
public:

  // Creates the appropriate PlatformDecoderModule for this platform.
  // Caller is responsible for deleting this instance. It's safe to have
  // multiple PlatformDecoderModules alive at the same time.
  // There is one PlatformDecoderModule's created per media decoder.
  static PlatformDecoderModule* Create();

  // Called when the decoders have shutdown. Main thread only.
  virtual nsresult Shutdown() = 0;

  // TODO: Parameters for codec type.
  // Decode thread.
  virtual MediaDataDecoder* CreateVideoDecoder(layers::LayersBackend aLayersBackend,
                                               layers::ImageContainer* aImageContainer) = 0;

  // Decode thread.
  virtual MediaDataDecoder* CreateAudioDecoder(uint32_t aChannelCount,
                                               uint32_t aSampleRate,
                                               uint16_t aBitsPerSample,
                                               const uint8_t* aUserData,
                                               uint32_t aUserDataLength) = 0;

  // Platform decoders can override these. Base implementation does nothing.
  virtual void OnDecodeThreadStart();
  virtual void OnDecodeThreadFinish();

  virtual ~PlatformDecoderModule() {}
protected:
  PlatformDecoderModule() {}
};

} // namespace mozilla

#endif
