/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoDecoder.h"
#include "mozilla/dom/VideoDecoderBinding.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(VideoDecoder, DOMEventTargetHelper)
NS_IMPL_ADDREF_INHERITED(VideoDecoder, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(VideoDecoder, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VideoDecoder)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject* VideoDecoder::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return VideoDecoder_Binding::Wrap(aCx, this, aGivenProto);
}

/* static */
already_AddRefed<VideoDecoder> VideoDecoder::Constructor(
    const GlobalObject& global, const VideoDecoderInit& init,
    ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

CodecState VideoDecoder::State() const { return CodecState::EndGuard_; }

uint32_t VideoDecoder::DecodeQueueSize() const { return 0; }

already_AddRefed<EventHandlerNonNull> VideoDecoder::GetOndequeue() const {
  return nullptr;
}

void VideoDecoder::SetOndequeue(EventHandlerNonNull* arg) {}

void VideoDecoder::Configure(const VideoDecoderConfig& config,
                             ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

void VideoDecoder::Decode(EncodedVideoChunk& chunk, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

already_AddRefed<Promise> VideoDecoder::Flush(ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

void VideoDecoder::Reset(ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

void VideoDecoder::Close(ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
}

/* static */
already_AddRefed<Promise> VideoDecoder::IsConfigSupported(
    const GlobalObject& global, const VideoDecoderConfig& config,
    ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

}  // namespace mozilla::dom
