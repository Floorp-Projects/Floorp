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
#include "mozilla/NotNull.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/VideoColorSpaceBinding.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class nsIGlobalObject;
class nsIURI;

namespace mozilla {

namespace layers {
class Image;
}  // namespace layers

namespace dom {

class DOMRectReadOnly;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmap;
class MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class OffscreenCanvas;
class OwningMaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer;
class Promise;
class SVGImageElement;
class StructuredCloneHolder;
class VideoColorSpace;
class VideoFrame;
enum class VideoPixelFormat : uint8_t;
struct VideoFrameBufferInit;
struct VideoFrameInit;
struct VideoFrameCopyToOptions;

}  // namespace dom
}  // namespace mozilla

namespace mozilla::dom {

struct VideoFrameData {
  VideoFrameData(layers::Image* aImage, const VideoPixelFormat& aFormat,
                 gfx::IntRect aVisibleRect, gfx::IntSize aDisplaySize,
                 Maybe<uint64_t> aDuration, int64_t aTimestamp,
                 const VideoColorSpaceInit& aColorSpace);

  const RefPtr<layers::Image> mImage;
  const VideoPixelFormat mFormat;
  const gfx::IntRect mVisibleRect;
  const gfx::IntSize mDisplaySize;
  const Maybe<uint64_t> mDuration;
  const int64_t mTimestamp;
  const VideoColorSpaceInit mColorSpace;
};

struct VideoFrameSerializedData : VideoFrameData {
  VideoFrameSerializedData(layers::Image* aImage,
                           const VideoPixelFormat& aFormat,
                           gfx::IntSize aCodedSize, gfx::IntRect aVisibleRect,
                           gfx::IntSize aDisplaySize, Maybe<uint64_t> aDuration,
                           int64_t aTimestamp,
                           const VideoColorSpaceInit& aColorSpace,
                           already_AddRefed<nsIURI> aPrincipalURI);

  const gfx::IntSize mCodedSize;
  const nsCOMPtr<nsIURI> mPrincipalURI;
};

class VideoFrame final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(VideoFrame)

 public:
  VideoFrame(nsIGlobalObject* aParent, const RefPtr<layers::Image>& aImage,
             const VideoPixelFormat& aFormat, gfx::IntSize aCodedSize,
             gfx::IntRect aVisibleRect, gfx::IntSize aDisplaySize,
             const Maybe<uint64_t>& aDuration, int64_t aTimestamp,
             const VideoColorSpaceInit& aColorSpace);

  VideoFrame(const VideoFrame& aOther);

 protected:
  ~VideoFrame() = default;

 public:
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& aGlobal, HTMLImageElement& aImageElement,
      const VideoFrameInit& aInit, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& aGlobal, SVGImageElement& aSVGImageElement,
      const VideoFrameInit& aInit, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& aGlobal, HTMLCanvasElement& aCanvasElement,
      const VideoFrameInit& aInit, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& aGlobal, HTMLVideoElement& aVideoElement,
      const VideoFrameInit& aInit, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& aGlobal, OffscreenCanvas& aOffscreenCanvas,
      const VideoFrameInit& aInit, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(const GlobalObject& aGlobal,
                                                  ImageBitmap& aImageBitmap,
                                                  const VideoFrameInit& aInit,
                                                  ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(const GlobalObject& aGlobal,
                                                  VideoFrame& aVideoFrame,
                                                  const VideoFrameInit& aInit,
                                                  ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& aGlobal, const ArrayBufferView& aBufferView,
      const VideoFrameBufferInit& aInit, ErrorResult& aRv);
  static already_AddRefed<VideoFrame> Constructor(
      const GlobalObject& aGlobal, const ArrayBuffer& aBuffer,
      const VideoFrameBufferInit& aInit, ErrorResult& aRv);

  Nullable<VideoPixelFormat> GetFormat() const;

  uint32_t CodedWidth() const;

  uint32_t CodedHeight() const;

  already_AddRefed<DOMRectReadOnly> GetCodedRect() const;

  already_AddRefed<DOMRectReadOnly> GetVisibleRect() const;

  uint32_t DisplayWidth() const;

  uint32_t DisplayHeight() const;

  Nullable<uint64_t> GetDuration() const;

  int64_t Timestamp() const;

  already_AddRefed<VideoColorSpace> ColorSpace() const;

  uint32_t AllocationSize(const VideoFrameCopyToOptions& aOptions,
                          ErrorResult& aRv);

  already_AddRefed<Promise> CopyTo(
      const MaybeSharedArrayBufferViewOrMaybeSharedArrayBuffer& aDestination,
      const VideoFrameCopyToOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<VideoFrame> Clone(ErrorResult& aRv);

  void Close();

  // [Serializable] implementations: {Read, Write}StructuredClone
  static JSObject* ReadStructuredClone(JSContext* aCx, nsIGlobalObject* aGlobal,
                                       JSStructuredCloneReader* aReader,
                                       const VideoFrameSerializedData& aData);

  bool WriteStructuredClone(JSStructuredCloneWriter* aWriter,
                            StructuredCloneHolder* aHolder) const;

  // [Transferable] implementations: Transfer, FromTransferred
  using TransferredData = VideoFrameSerializedData;

  UniquePtr<TransferredData> Transfer();

  static already_AddRefed<VideoFrame> FromTransferred(nsIGlobalObject* aGlobal,
                                                      TransferredData* aData);

 public:
  // A VideoPixelFormat wrapper providing utilities for VideoFrame.
  class Format final {
   public:
    explicit Format(const VideoPixelFormat& aFormat);
    ~Format() = default;
    const VideoPixelFormat& PixelFormat() const;
    gfx::SurfaceFormat ToSurfaceFormat() const;
    void MakeOpaque();

    // TODO: Assign unique value for each plane?
    // The value indicates the order of the plane in format.
    enum class Plane : uint8_t { Y = 0, RGBA = Y, U = 1, UV = U, V = 2, A = 3 };
    nsTArray<Plane> Planes() const;
    const char* PlaneName(const Plane& aPlane) const;
    uint32_t SampleBytes(const Plane& aPlane) const;
    gfx::IntSize SampleSize(const Plane& aPlane) const;
    bool IsValidSize(const gfx::IntSize& aSize) const;
    size_t SampleCount(const gfx::IntSize& aSize) const;

   private:
    bool IsYUV() const;
    VideoPixelFormat mFormat;
  };

 private:
  // VideoFrame can run on either main thread or worker thread.
  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(VideoFrame); }

  already_AddRefed<nsIURI> GetPrincipalURI() const;

  // A class representing the VideoFrame's data.
  class Resource final {
   public:
    Resource(const RefPtr<layers::Image>& aImage, const Format& aFormat);
    Resource(const Resource& aOther);
    ~Resource() = default;
    uint32_t Stride(const Format::Plane& aPlane) const;
    bool CopyTo(const Format::Plane& aPlane, const gfx::IntRect& aRect,
                RangedPtr<uint8_t>&& aPlaneDest,
                size_t aDestinationStride) const;

    const RefPtr<layers::Image> mImage;
    const Format mFormat;
  };

  nsCOMPtr<nsIGlobalObject> mParent;

  // Use Maybe instead of UniquePtr to allow copy ctor.
  // The mResource's existence is used as the [[Detached]] for [Transferable].
  Maybe<const Resource> mResource;  // Nothing() after `Close()`d

  // TODO: Replace this by mResource->mImage->GetSize()?
  gfx::IntSize mCodedSize;
  gfx::IntRect mVisibleRect;
  gfx::IntSize mDisplaySize;

  Maybe<uint64_t> mDuration;  // Nothing() after `Close()`d
  int64_t mTimestamp;
  VideoColorSpaceInit mColorSpace;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_VideoFrame_h
