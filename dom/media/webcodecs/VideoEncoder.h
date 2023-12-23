/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoEncoder_h
#define mozilla_dom_VideoEncoder_h

#include "js/TypeDecls.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/EncoderTemplate.h"
#include "mozilla/dom/EncoderTypes.h"
#include "mozilla/dom/VideoFrame.h"
#include "nsCycleCollectionParticipant.h"

class nsIGlobalObject;

namespace mozilla {

namespace dom {

class EncodedVideoChunk;
class EncodedVideoChunkData;
class EventHandlerNonNull;
class GlobalObject;
class Promise;
class VideoFrameOutputCallback;
class WebCodecsErrorCallback;
struct VideoEncoderConfig;
struct VideoEncoderInit;

}  // namespace dom

}  // namespace mozilla

namespace mozilla::dom {

class VideoEncoder final : public EncoderTemplate<VideoEncoderTraits> {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VideoEncoder, DOMEventTargetHelper)

 public:
  VideoEncoder(nsIGlobalObject* aParent,
               RefPtr<WebCodecsErrorCallback>&& aErrorCallback,
               RefPtr<EncodedVideoChunkOutputCallback>&& aOutputCallback);

 protected:
  ~VideoEncoder();

 public:
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<VideoEncoder> Constructor(
      const GlobalObject& aGlobal, const VideoEncoderInit& aInit,
      ErrorResult& aRv);

  static already_AddRefed<Promise> IsConfigSupported(
      const GlobalObject& aGlobal, const VideoEncoderConfig& aConfig,
      ErrorResult& aRv);

 protected:
  virtual RefPtr<EncodedVideoChunk> EncodedDataToOutputType(
      nsIGlobalObject* aGlobal, RefPtr<MediaRawData>& aData) override;

  virtual VideoDecoderConfigInternal EncoderConfigToDecoderConfig(
      nsIGlobalObject* aGlobal /* TODO: delete */,
      const RefPtr<MediaRawData>& aRawData,
      const VideoEncoderConfigInternal& mOutputConfig) const override;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoEncoder_h
