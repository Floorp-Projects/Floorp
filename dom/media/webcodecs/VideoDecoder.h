/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoDecoder_h
#define mozilla_dom_VideoDecoder_h

#include "DecoderTemplate.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/VideoFrame.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {

class MediaData;
class VideoInfo;

namespace dom {

class EncodedVideoChunk;
class EncodedVideoChunkData;
class EventHandlerNonNull;
class GlobalObject;
class Promise;
class VideoFrameOutputCallback;
class WebCodecsErrorCallback;
// enum class CodecState : uint8_t;
enum class HardwareAcceleration : uint8_t;
struct VideoDecoderConfig;
struct VideoDecoderInit;

}  // namespace dom

}  // namespace mozilla

namespace mozilla::dom {

class VideoDecoderConfigInternal;

class VideoDecoderTraits {
 public:
  using ConfigTypeInternal = VideoDecoderConfigInternal;
  using InputTypeInternal = EncodedVideoChunkData;
  using OutputType = VideoFrame;
  using OutputCallbackType = VideoFrameOutputCallback;

  static bool IsSupported(const ConfigTypeInternal& aConfig);
  static Result<UniquePtr<TrackInfo>, nsresult> CreateTrackInfo(
      const ConfigTypeInternal& aConfig);
};

class VideoDecoder final : public DecoderTemplate<VideoDecoderTraits> {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VideoDecoder, DOMEventTargetHelper)

  IMPL_EVENT_HANDLER(dequeue)

 public:
  VideoDecoder(nsIGlobalObject* aParent,
               RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
               RefPtr<VideoFrameOutputCallback>&& aOutputCallback);

 protected:
  ~VideoDecoder();

 public:
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<VideoDecoder> Constructor(
      const GlobalObject& aGlobal, const VideoDecoderInit& aInit,
      ErrorResult& aRv);

  CodecState State() const;

  uint32_t DecodeQueueSize() const;

  void Configure(const VideoDecoderConfig& aConfig, ErrorResult& aRv);

  void Decode(EncodedVideoChunk& aChunk, ErrorResult& aRv);

  already_AddRefed<Promise> Flush(ErrorResult& aRv);

  void Reset(ErrorResult& aRv);

  void Close(ErrorResult& aRv);

  static already_AddRefed<Promise> IsConfigSupported(
      const GlobalObject& aGlobal, const VideoDecoderConfig& aConfig,
      ErrorResult& aRv);

 protected:
  virtual nsTArray<RefPtr<VideoFrame>> DecodedDataToOutputType(
      nsIGlobalObject* aGlobalObject, nsTArray<RefPtr<MediaData>>&& aData,
      VideoDecoderConfigInternal& aConfig) override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoDecoder_h
