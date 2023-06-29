/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoDecoder_h
#define mozilla_dom_VideoDecoder_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class EncodedVideoChunk;
class EventHandlerNonNull;
class GlobalObject;
class Promise;
class VideoFrameOutputCallback;
class WebCodecsErrorCallback;
enum class CodecState : uint8_t;
struct VideoDecoderConfig;
struct VideoDecoderInit;

}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class VideoDecoder final : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VideoDecoder, DOMEventTargetHelper)

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

  already_AddRefed<EventHandlerNonNull> GetOndequeue() const;

  void SetOndequeue(EventHandlerNonNull* arg);

  void Configure(const VideoDecoderConfig& config, ErrorResult& aRv);

  void Decode(EncodedVideoChunk& chunk, ErrorResult& aRv);

  already_AddRefed<Promise> Flush(ErrorResult& aRv);

  void Reset(ErrorResult& aRv);

  void Close(ErrorResult& aRv);

  static already_AddRefed<Promise> IsConfigSupported(
      const GlobalObject& aGlobal, const VideoDecoderConfig& aConfig,
      ErrorResult& aRv);

 private:
  // VideoDecoder can run on either main thread or worker thread.
  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(VideoDecoder); }

  // Constant in practice, only set in ::Constructor.
  RefPtr<WebCodecsErrorCallback> mErrorCallback;
  RefPtr<VideoFrameOutputCallback> mOutputCallback;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoDecoder_h
