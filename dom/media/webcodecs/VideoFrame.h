/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VideoFrame_h
#define mozilla_dom_VideoFrame_h

#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/TypedArray.h"  // Add to make it buildable.
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;  // Add to make it buildable.

namespace mozilla {
namespace dom {

class DOMRectReadOnly;
class HTMLCanvasElement;  // Add to make it buildable.
class HTMLImageElement;   // Add to make it buildable.
class HTMLVideoElement;   // Add to make it buildable.
class ImageBitmap;        // Add to make it buildable.
class MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class OffscreenCanvas;  // Add to make it buildable.
class OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class Promise;
class SVGImageElement;  // Add to make it buildable.
class VideoColorSpace;
class VideoFrame;
enum class VideoPixelFormat : uint8_t;  // Add to make it buildable.
struct VideoFrameBufferInit;            // Add to make it buildable.
struct VideoFrameInit;                  // Add to make it buildable.
struct VideoFrameCopyToOptions;

}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

class VideoFrame final
    : public nsISupports /* or NonRefcountedDOMObject if this is a
                            non-refcounted object */
    ,
      public nsWrapperCache /* Change wrapperCache in the binding configuration
                               if you don't want this */
{
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(VideoFrame)

 public:
  VideoFrame();

 protected:
  ~VideoFrame();

 public:
  // This should return something that eventually allows finding a
  // path to the global this object is associated with.  Most simply,
  // returning an actual global works.
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& global, HTMLImageElement& imageElement,
      const VideoFrameInit& init, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& global, SVGImageElement& svgImageElement,
      const VideoFrameInit& init, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& global, HTMLCanvasElement& canvasElement,
      const VideoFrameInit& init, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& global, HTMLVideoElement& videoElement,
      const VideoFrameInit& init, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& global, OffscreenCanvas& offscreenCanvas,
      const VideoFrameInit& init, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(const GlobalObject& global,
                                                  ImageBitmap& imageBitmap,
                                                  const VideoFrameInit& init,
                                                  ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(const GlobalObject& global,
                                                  VideoFrame& videoFrame,
                                                  const VideoFrameInit& init,
                                                  ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& global, const ArrayBufferView& bufferView,
      const VideoFrameBufferInit& init, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& global, const ArrayBuffer& buffer,
      const VideoFrameBufferInit& init, ErrorResult& aRv);

  Nullable<VideoPixelFormat> GetFormat() const;

  uint32_t CodedWidth() const;

  uint32_t CodedHeight() const;

  // Return a raw pointer here to avoid refcounting, but make sure it's safe
  // (the object should be kept alive by the callee).
  already_AddRefed<DOMRectReadOnly> GetCodedRect() const;

  // Return a raw pointer here to avoid refcounting, but make sure it's safe
  // (the object should be kept alive by the callee).
  already_AddRefed<DOMRectReadOnly> GetVisibleRect() const;

  uint32_t DisplayWidth() const;

  uint32_t DisplayHeight() const;

  Nullable<uint64_t> GetDuration() const;

  Nullable<int64_t> GetTimestamp() const;

  // Return a raw pointer here to avoid refcounting, but make sure it's safe
  // (the object should be kept alive by the callee).
  already_AddRefed<VideoColorSpace> ColorSpace() const;

  uint32_t AllocationSize(const VideoFrameCopyToOptions& options,
                          ErrorResult& aRv);

  // Return a raw pointer here to avoid refcounting, but make sure it's safe
  // (the object should be kept alive by the callee).
  already_AddRefed<Promise> CopyTo(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& destination,
      const VideoFrameCopyToOptions& options, ErrorResult& aRv);

  // Return a raw pointer here to avoid refcounting, but make sure it's safe
  // (the object should be kept alive by the callee).
  already_AddRefed<VideoFrame> Clone(ErrorResult& aRv);

  void Close();
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoFrame_h
