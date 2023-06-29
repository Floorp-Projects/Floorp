/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_WEBCODECS_DECODERAGENT_H
#define DOM_MEDIA_WEBCODECS_DECODERAGENT_H

#include "MediaResult.h"
#include "PlatformDecoderModule.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/UniquePtr.h"

class nsISerialEventTarget;

namespace mozilla {

class PDMFactory;
class TrackInfo;

namespace layers {
class ImageContainer;
}  // namespace layers

// DecoderAgent is a wrapper that contains a MediaDataDecoder. It adapts the
// MediaDataDecoder APIs for use in WebCodecs.
//
// If Configure() is called, Shutdown() must be called to release the resources
// gracefully. Except Shutdown(), all the methods can't be called concurrently,
// meaning a method can only be called when the previous API call has completed.
// The responsability of arranging the method calls is on the caller.
//
// When Shutdown() is called, all the operations in flight are canceled and the
// MediaDataDecoder is shut down. On the other hand, errors are final. A new
// DecoderAgent must be created when an error is encountered.
//
// All the methods need to be called on the DecoderAgent's owner thread. In
// WebCodecs, it's either on the main thread or worker thread.
class DecoderAgent final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DecoderAgent);

  using Id = uint32_t;
  DecoderAgent(Id aId, UniquePtr<TrackInfo>&& aInfo);

  // The following APIs are owner thread only.

  using ConfigurePromise = MozPromise<bool, MediaResult, true /* exclusive */>;
  RefPtr<ConfigurePromise> Configure(bool aPreferSoftwareDecoder,
                                     bool aLowLatency);
  RefPtr<ShutdownPromise> Shutdown();
  using DecodePromise = MediaDataDecoder::DecodePromise;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample);
  // WebCodecs's flush() flushes out all the pending decoded data in the
  // MediaDataDecoder so it is a combination of Drain and Flush. To distinguish
  // the term from MediaDataDecoder's one, we call it DrainAndFlush() here.
  RefPtr<DecodePromise> DrainAndFlush();

  const Id mId;  // A unique id.
  const UniquePtr<TrackInfo> mInfo;

 private:
  ~DecoderAgent();

  // Push out all the data in the MediaDataDecoder's pipeline.
  // TODO: MediaDataDecoder should implement this, instead of asking call site
  // to run `Drain` multiple times.
  RefPtr<DecodePromise> Dry();
  void DrainUntilDry();

  enum class State {
    Unconfigured,
    Configuring,
    Configured,
    Decoding,
    Flushing,
    ShuttingDown,
    Error,
  };
  void SetState(State aState);

  const RefPtr<nsISerialEventTarget> mOwnerThread;
  const RefPtr<PDMFactory> mPDMFactory;
  const RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<MediaDataDecoder> mDecoder;
  State mState;

  // Configure
  MozPromiseHolder<ConfigurePromise> mConfigurePromise;
  using CreateDecoderPromise = PlatformDecoderModule::CreateDecoderPromise;
  MozPromiseRequestHolder<CreateDecoderPromise> mCreateRequest;
  using InitPromise = MediaDataDecoder::InitPromise;
  MozPromiseRequestHolder<InitPromise> mInitRequest;

  // Shutdown
  MozPromiseHolder<ShutdownPromise> mShutdownWhileCreationPromise;

  // Decode
  MozPromiseHolder<DecodePromise> mDecodePromise;
  MozPromiseRequestHolder<DecodePromise> mDecodeRequest;

  // DrainAndFlush
  MozPromiseHolder<DecodePromise> mDrainAndFlushPromise;
  MediaDataDecoder::DecodedData mDrainAndFlushData;
  MozPromiseRequestHolder<DecodePromise> mDryRequest;
  MozPromiseHolder<DecodePromise> mDryPromise;
  MediaDataDecoder::DecodedData mDryData;
  MozPromiseRequestHolder<DecodePromise> mDrainRequest;
  using FlushPromise = MediaDataDecoder::FlushPromise;
  MozPromiseRequestHolder<FlushPromise> mFlushRequest;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_WEBCODECS_DECODERAGENT_H
