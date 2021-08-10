/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ImageBitmapRenderingContext_h
#define ImageBitmapRenderingContext_h

#include "mozilla/dom/ImageBitmap.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DataSurfaceHelpers.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/WebRenderUserData.h"
#include "imgIEncoder.h"
#include "ImageEncoder.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsWrapperCache.h"

namespace mozilla {

namespace gfx {
class DataSourceSurface;
class DrawTarget;
class SourceSurface;
}  // namespace gfx

namespace layers {
class Image;
class ImageContainer;
}  // namespace layers

namespace dom {

/**
 * The purpose of ImageBitmapRenderingContext is to provide a faster and
 * efficient way to display ImageBitmap. Simply call TransferFromImageBitmap()
 * then we'll transfer the surface of ImageBitmap to this context and then to
 * use it to display.
 *
 * See more details in spec: https://wiki.whatwg.org/wiki/OffscreenCanvas
 */
class ImageBitmapRenderingContext final
    : public nsICanvasRenderingContextInternal,
      public nsWrapperCache {
  virtual ~ImageBitmapRenderingContext();

 public:
  ImageBitmapRenderingContext();

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // nsISupports interface + CC
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(ImageBitmapRenderingContext)

  void TransferImageBitmap(ImageBitmap& aImageBitmap);
  void TransferFromImageBitmap(ImageBitmap& aImageBitmap);

  // nsICanvasRenderingContextInternal
  virtual int32_t GetWidth() override { return mWidth; }
  virtual int32_t GetHeight() override { return mHeight; }

  NS_IMETHOD SetDimensions(int32_t aWidth, int32_t aHeight) override;

  NS_IMETHOD InitializeWithDrawTarget(
      nsIDocShell* aDocShell, NotNull<gfx::DrawTarget*> aTarget) override;

  virtual mozilla::UniquePtr<uint8_t[]> GetImageBuffer(
      int32_t* aFormat) override;
  NS_IMETHOD GetInputStream(const char* aMimeType,
                            const nsAString& aEncoderOptions,
                            nsIInputStream** aStream) override;

  virtual already_AddRefed<mozilla::gfx::SourceSurface> GetSurfaceSnapshot(
      gfxAlphaType* aOutAlphaType) override;

  virtual void SetOpaqueValueFromOpaqueAttr(bool aOpaqueAttrValue) override;
  virtual bool GetIsOpaque() override;
  NS_IMETHOD Reset() override;
  virtual already_AddRefed<Layer> GetCanvasLayer(
      nsDisplayListBuilder* aBuilder, Layer* aOldLayer,
      LayerManager* aManager) override;
  virtual already_AddRefed<layers::Image> GetAsImage() override {
    return ClipToIntrinsicSize();
  }
  bool UpdateWebRenderCanvasData(nsDisplayListBuilder* aBuilder,
                                 WebRenderCanvasData* aCanvasData) override;
  virtual void MarkContextClean() override;

  NS_IMETHOD Redraw(const gfxRect& aDirty) override;
  NS_IMETHOD SetIsIPC(bool aIsIPC) override;

  virtual void DidRefresh() override;

  virtual void MarkContextCleanForFrameCapture() override;
  virtual bool IsContextCleanForFrameCapture() override;

 protected:
  already_AddRefed<gfx::DataSourceSurface> MatchWithIntrinsicSize();
  already_AddRefed<layers::Image> ClipToIntrinsicSize();
  int32_t mWidth;
  int32_t mHeight;

  RefPtr<layers::Image> mImage;

  /**
   * Flag to avoid unnecessary surface copies to FrameCaptureListeners in the
   * case when the canvas is not currently being drawn into and not rendered
   * but canvas capturing is still ongoing.
   */
  bool mIsCapturedFrameInvalid;
};

}  // namespace dom
}  // namespace mozilla

#endif /* ImageBitmapRenderingContext_h */
