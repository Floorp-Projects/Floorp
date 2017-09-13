/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ImageBitmap_h
#define mozilla_dom_ImageBitmap_h

#include "mozilla/Attributes.h"
#include "mozilla/dom/ImageBitmapSource.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "gfxTypes.h" // for gfxAlphaType
#include "nsCycleCollectionParticipant.h"

struct JSContext;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace gfx {
class DataSourceSurface;
class DrawTarget;
class SourceSurface;
}

namespace layers {
class Image;
}

namespace dom {
class OffscreenCanvas;

namespace workers {
class WorkerStructuredCloneClosure;
}

class ArrayBufferViewOrArrayBuffer;
class CanvasRenderingContext2D;
struct ChannelPixelLayout;
class CreateImageBitmapFromBlob;
class CreateImageBitmapFromBlobTask;
class CreateImageBitmapFromBlobWorkerTask;
class File;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
enum class ImageBitmapFormat : uint8_t;
class ImageData;
class ImageUtils;
template<typename T> class MapDataIntoBufferSource;
class Promise;
class PostMessageEvent; // For StructuredClone between windows.

struct ImageBitmapCloneData final
{
  RefPtr<gfx::DataSourceSurface> mSurface;
  gfx::IntRect mPictureRect;
  gfxAlphaType mAlphaType;
  bool mIsCroppingAreaOutSideOfSourceImage;
};

/*
 * ImageBitmap is an opaque handler to several kinds of image-like objects from
 * HTMLImageElement, HTMLVideoElement, HTMLCanvasElement, ImageData to
 * CanvasRenderingContext2D and Image Blob.
 *
 * An ImageBitmap could be painted to a canvas element.
 *
 * Generally, an ImageBitmap only keeps a reference to its source object's
 * buffer, but if the source object is an ImageData, an Blob or a
 * HTMLCanvasElement with WebGL rendering context, the ImageBitmap copy the
 * source object's buffer.
 */
class ImageBitmap final : public nsISupports,
                          public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ImageBitmap)

  nsCOMPtr<nsIGlobalObject> GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  uint32_t Width() const
  {
    return mPictureRect.Width();
  }

  uint32_t Height() const
  {
    return mPictureRect.Height();
  }

  void Close();

  /*
   * The PrepareForDrawTarget() might return null if the mPictureRect does not
   * intersect with the size of mData.
   */
  already_AddRefed<gfx::SourceSurface>
  PrepareForDrawTarget(gfx::DrawTarget* aTarget);

  /*
   * Transfer ownership of buffer to caller. So this function call
   * Close() implicitly.
   */
  already_AddRefed<layers::Image>
  TransferAsImage();

  UniquePtr<ImageBitmapCloneData>
  ToCloneData() const;

  static already_AddRefed<ImageBitmap>
  CreateFromCloneData(nsIGlobalObject* aGlobal, ImageBitmapCloneData* aData);

  static already_AddRefed<ImageBitmap>
  CreateFromOffscreenCanvas(nsIGlobalObject* aGlobal,
                            OffscreenCanvas& aOffscreenCanvas,
                            ErrorResult& aRv);

  static already_AddRefed<Promise>
  Create(nsIGlobalObject* aGlobal, const ImageBitmapSource& aSrc,
         const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv);

  static already_AddRefed<Promise>
  Create(nsIGlobalObject* aGlobal,
         const ImageBitmapSource& aBuffer,
         int32_t aOffset, int32_t aLength,
         mozilla::dom::ImageBitmapFormat aFormat,
         const Sequence<mozilla::dom::ChannelPixelLayout>& aLayout,
         ErrorResult& aRv);

  static JSObject*
  ReadStructuredClone(JSContext* aCx,
                      JSStructuredCloneReader* aReader,
                      nsIGlobalObject* aParent,
                      const nsTArray<RefPtr<gfx::DataSourceSurface>>& aClonedSurfaces,
                      uint32_t aIndex);

  static bool
  WriteStructuredClone(JSStructuredCloneWriter* aWriter,
                       nsTArray<RefPtr<gfx::DataSourceSurface>>& aClonedSurfaces,
                       ImageBitmap* aImageBitmap);

  // Mozilla Extensions
  // aObj is an optional argument that isn't used by ExtensionsEnabled() and
  // only exists because the bindings layer insists on passing it to us.  All
  // other consumers of this function should only call it passing one argument.
  static bool ExtensionsEnabled(JSContext* aCx, JSObject* aObj = nullptr);

  friend CreateImageBitmapFromBlob;
  friend CreateImageBitmapFromBlobTask;
  friend CreateImageBitmapFromBlobWorkerTask;

  template<typename T>
  friend class MapDataIntoBufferSource;

  // Mozilla Extensions
  ImageBitmapFormat
  FindOptimalFormat(const Optional<Sequence<ImageBitmapFormat>>& aPossibleFormats,
                    ErrorResult& aRv);

  int32_t
  MappedDataLength(ImageBitmapFormat aFormat, ErrorResult& aRv);

  already_AddRefed<Promise>
  MapDataInto(JSContext* aCx,
              ImageBitmapFormat aFormat,
              const ArrayBufferViewOrArrayBuffer& aBuffer,
              int32_t aOffset, ErrorResult& aRv);

  size_t GetAllocatedSize() const;

protected:

  /*
   * The default value of aIsPremultipliedAlpha is TRUE because that the
   * data stored in HTMLImageElement, HTMLVideoElement, HTMLCanvasElement,
   * CanvasRenderingContext2D are alpha-premultiplied in default.
   *
   * Actually, if one HTMLCanvasElement's rendering context is WebGLContext, it
   * is possible to get un-premultipliedAlpha data out. But, we do not do it in
   * the CreateInternal(from HTMLCanvasElement) method.
   *
   * It is also possible to decode an image which is encoded with alpha channel
   * to be non-premultipliedAlpha. This could be applied in
   * 1) the CreateInternal(from HTMLImageElement) method (which might trigger
   *    re-decoding if the original decoded data is alpha-premultiplied) and
   * 2) while decoding a blob. But we do not do it in both code path too.
   *
   * ImageData's underlying data is triggered as non-premultipliedAlpha, so set
   * the aIsPremultipliedAlpha to be false in the
   * CreateInternal(from ImageData) method.
   */
  ImageBitmap(nsIGlobalObject* aGlobal, layers::Image* aData,
              gfxAlphaType aAlphaType = gfxAlphaType::Premult);

  virtual ~ImageBitmap();

  void SetPictureRect(const gfx::IntRect& aRect, ErrorResult& aRv);

  void SetIsCroppingAreaOutSideOfSourceImage(const gfx::IntSize& aSourceSize,
                                             const Maybe<gfx::IntRect>& aCroppingRect);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, HTMLImageElement& aImageEl,
                 const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, HTMLVideoElement& aVideoEl,
                 const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, HTMLCanvasElement& aCanvasEl,
                 const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, ImageData& aImageData,
                 const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, CanvasRenderingContext2D& aCanvasCtx,
                 const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv);

  static already_AddRefed<ImageBitmap>
  CreateInternal(nsIGlobalObject* aGlobal, ImageBitmap& aImageBitmap,
                 const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv);

  nsCOMPtr<nsIGlobalObject> mParent;

  /*
   * The mData is the data buffer of an ImageBitmap, so the mData must not be
   * null.
   *
   * The mSurface is a cache for drawing the ImageBitmap onto a
   * HTMLCanvasElement. The mSurface is null while the ImageBitmap is created
   * and then will be initialized while the PrepareForDrawTarget() method is
   * called first time.
   *
   * The mSurface might just be a reference to the same data buffer of the mData
   * if the are of mPictureRect is just the same as the mData's size. Or, it is
   * a independent data buffer which is copied and cropped form the mData's data
   * buffer.
   */
  RefPtr<layers::Image> mData;
  RefPtr<gfx::SourceSurface> mSurface;

  /*
   * This is used in the ImageBitmap-Extensions implementation.
   * ImageUtils is a wrapper to layers::Image, which add some common methods for
   * accessing the layers::Image's data.
   */
  UniquePtr<ImageUtils> mDataWrapper;

  /*
   * The mPictureRect is the size of the source image in default, however, if
   * users specify the cropping area while creating an ImageBitmap, then this
   * mPictureRect is the cropping area.
   *
   * Note that if the CreateInternal() copies and crops data from the source
   * image, then this mPictureRect is just the size of the final mData.
   *
   * The mPictureRect will be used at PrepareForDrawTarget() while user is going
   * to draw this ImageBitmap into a HTMLCanvasElement.
   */
  gfx::IntRect mPictureRect;

  const gfxAlphaType mAlphaType;

  /*
   * Set mIsCroppingAreaOutSideOfSourceImage if image bitmap was cropped to the
   * source rectangle so that it contains any transparent black pixels (cropping
   * area is outside of the source image).
   * This is used in mapDataInto() to check if we should reject promise with
   * IndexSizeError.
   */
  bool mIsCroppingAreaOutSideOfSourceImage;

  /*
   * Whether this object allocated allocated and owns the image data.
   */
  bool mAllocatedImageData;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImageBitmap_h


