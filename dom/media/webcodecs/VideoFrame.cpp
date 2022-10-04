/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/VideoFrame.h"
#include "mozilla/dom/VideoFrameBinding.h"

namespace mozilla::dom {

// Only needed for refcounted objects.
NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(VideoFrame)
NS_IMPL_CYCLE_COLLECTING_ADDREF(VideoFrame)
NS_IMPL_CYCLE_COLLECTING_RELEASE(VideoFrame)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(VideoFrame)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

VideoFrame::VideoFrame() {
  // Add |MOZ_COUNT_CTOR(VideoFrame);| for a non-refcounted object.
}

VideoFrame::~VideoFrame() {
  // Add |MOZ_COUNT_DTOR(VideoFrame);| for a non-refcounted object.
}

// Add to make it buildable.
nsIGlobalObject* VideoFrame::GetParentObject() const { return nullptr; }

JSObject* VideoFrame::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return VideoFrame_Binding::Wrap(aCx, this, aGivenProto);
}

// The followings are added to make it buildable.

/* static */
already_AddRefed<VideoFrame> VideoFrame::Constructor(
    const GlobalObject& global, HTMLImageElement& imageElement,
    const VideoFrameInit& init, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

/* static */
already_AddRefed<VideoFrame> VideoFrame::Constructor(
    const GlobalObject& global, SVGImageElement& svgImageElement,
    const VideoFrameInit& init, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

/* static */
already_AddRefed<VideoFrame> VideoFrame::Constructor(
    const GlobalObject& global, HTMLCanvasElement& canvasElement,
    const VideoFrameInit& init, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

/* static */
already_AddRefed<VideoFrame> VideoFrame::Constructor(
    const GlobalObject& global, HTMLVideoElement& videoElement,
    const VideoFrameInit& init, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

/* static */
already_AddRefed<VideoFrame> VideoFrame::Constructor(
    const GlobalObject& global, OffscreenCanvas& offscreenCanvas,
    const VideoFrameInit& init, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

/* static */
already_AddRefed<VideoFrame> VideoFrame::Constructor(const GlobalObject& global,
                                                     ImageBitmap& imageBitmap,
                                                     const VideoFrameInit& init,
                                                     ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

/* static */
already_AddRefed<VideoFrame> VideoFrame::Constructor(const GlobalObject& global,
                                                     VideoFrame& videoFrame,
                                                     const VideoFrameInit& init,
                                                     ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

/* static */
already_AddRefed<VideoFrame> VideoFrame::Constructor(
    const GlobalObject& global, const ArrayBufferView& bufferView,
    const VideoFrameBufferInit& init, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

/* static */
already_AddRefed<VideoFrame> VideoFrame::Constructor(
    const GlobalObject& global, const ArrayBuffer& buffer,
    const VideoFrameBufferInit& init, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

Nullable<VideoPixelFormat> VideoFrame::GetFormat() const { return nullptr; }

uint32_t VideoFrame::CodedWidth() const { return 0; }

uint32_t VideoFrame::CodedHeight() const { return 0; }

already_AddRefed<DOMRectReadOnly> VideoFrame::GetCodedRect() const {
  return nullptr;
}

already_AddRefed<DOMRectReadOnly> VideoFrame::GetVisibleRect() const {
  return nullptr;
}

uint32_t VideoFrame::DisplayWidth() const { return 0; }

uint32_t VideoFrame::DisplayHeight() const { return 0; }

Nullable<uint64_t> VideoFrame::GetDuration() const { return nullptr; }

Nullable<int64_t> VideoFrame::GetTimestamp() const { return nullptr; }

already_AddRefed<VideoColorSpace> VideoFrame::ColorSpace() const {
  return nullptr;
}

uint32_t VideoFrame::AllocationSize(const VideoFrameCopyToOptions& options,
                                    ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return 0;
}

already_AddRefed<Promise> VideoFrame::CopyTo(
    const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& destination,
    const VideoFrameCopyToOptions& options, ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

already_AddRefed<VideoFrame> VideoFrame::Clone(ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

void VideoFrame::Close() {}

}  // namespace mozilla::dom
