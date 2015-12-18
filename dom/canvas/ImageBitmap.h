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
#include "nsCycleCollectionParticipant.h"

struct JSContext;
struct JSStructuredCloneReader;
struct JSStructuredCloneWriter;

class nsIGlobalObject;

namespace mozilla {

class ErrorResult;

namespace gfx {
class DataSourceSurface;
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

class CanvasRenderingContext2D;
class File;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageData;
class Promise;
class PostMessageEvent; // For StructuredClone between windows.
class CreateImageBitmapFromBlob;
class CreateImageBitmapFromBlobTask;
class CreateImageBitmapFromBlobWorkerTask;

struct ImageBitmapCloneData final
{
  RefPtr<gfx::DataSourceSurface> mSurface;
  gfx::IntRect mPictureRect;
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

  ImageBitmapCloneData*
  ToCloneData();

  static already_AddRefed<ImageBitmap>
  CreateFromCloneData(nsIGlobalObject* aGlobal, ImageBitmapCloneData* aData);

  static already_AddRefed<ImageBitmap>
  CreateFromOffscreenCanvas(nsIGlobalObject* aGlobal,
                            OffscreenCanvas& aOffscreenCanvas,
                            ErrorResult& aRv);

  static already_AddRefed<Promise>
  Create(nsIGlobalObject* aGlobal, const ImageBitmapSource& aSrc,
         const Maybe<gfx::IntRect>& aCropRect, ErrorResult& aRv);

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

  friend CreateImageBitmapFromBlob;
  friend CreateImageBitmapFromBlobTask;
  friend CreateImageBitmapFromBlobWorkerTask;

protected:

  ImageBitmap(nsIGlobalObject* aGlobal, layers::Image* aData);

  virtual ~ImageBitmap();

  void SetPictureRect(const gfx::IntRect& aRect, ErrorResult& aRv);

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
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImageBitmap_h


