/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsICanvasRenderingContextInternal_h___
#define nsICanvasRenderingContextInternal_h___

#include "mozilla/gfx/2D.h"
#include "nsISupports.h"
#include "nsIInputStream.h"
#include "nsIDocShell.h"
#include "nsRefreshDriver.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/NotNull.h"

#define NS_ICANVASRENDERINGCONTEXTINTERNAL_IID \
{ 0xb84f2fed, 0x9d4b, 0x430b, \
  { 0xbd, 0xfb, 0x85, 0x57, 0x8a, 0xc2, 0xb4, 0x4b } }

class nsDisplayListBuilder;

namespace mozilla {
namespace layers {
class CanvasLayer;
class Layer;
class LayerManager;
} // namespace layers
namespace gfx {
class SourceSurface;
} // namespace gfx
} // namespace mozilla

class nsICanvasRenderingContextInternal :
  public nsISupports,
  public nsAPostRefreshObserver
{
public:
  typedef mozilla::layers::CanvasLayer CanvasLayer;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ICANVASRENDERINGCONTEXTINTERNAL_IID)

  void SetCanvasElement(mozilla::dom::HTMLCanvasElement* parentCanvas)
  {
    RemovePostRefreshObserver();
    mCanvasElement = parentCanvas;
    AddPostRefreshObserverIfNecessary();
  }

  virtual nsIPresShell *GetPresShell() {
    if (mCanvasElement) {
      return mCanvasElement->OwnerDoc()->GetShell();
    }
    return nullptr;
  }

  void RemovePostRefreshObserver()
  {
    if (mRefreshDriver) {
      mRefreshDriver->RemovePostRefreshObserver(this);
      mRefreshDriver = nullptr;
    }
  }

  void AddPostRefreshObserverIfNecessary()
  {
    if (!GetPresShell() ||
        !GetPresShell()->GetPresContext() ||
        !GetPresShell()->GetPresContext()->RefreshDriver()) {
      return;
    }
    mRefreshDriver = GetPresShell()->GetPresContext()->RefreshDriver();
    mRefreshDriver->AddPostRefreshObserver(this);
  }

  mozilla::dom::HTMLCanvasElement* GetParentObject() const
  {
    return mCanvasElement;
  }

  void SetOffscreenCanvas(mozilla::dom::OffscreenCanvas* aOffscreenCanvas)
  {
    mOffscreenCanvas = aOffscreenCanvas;
  }

  // Dimensions of the canvas, in pixels.
  virtual int32_t GetWidth() const = 0;
  virtual int32_t GetHeight() const = 0;

  // Sets the dimensions of the canvas, in pixels.  Called
  // whenever the size of the element changes.
  NS_IMETHOD SetDimensions(int32_t width, int32_t height) = 0;

  // Initializes with an nsIDocShell and DrawTarget. The size is taken from the
  // DrawTarget.
  NS_IMETHOD InitializeWithDrawTarget(nsIDocShell *aDocShell,
                                      mozilla::NotNull<mozilla::gfx::DrawTarget*> aTarget) = 0;

  // Creates an image buffer. Returns null on failure.
  virtual mozilla::UniquePtr<uint8_t[]> GetImageBuffer(int32_t* format) = 0;

  // Gives you a stream containing the image represented by this context.
  // The format is given in mimeTime, for example "image/png".
  //
  // If the image format does not support transparency or includeTransparency
  // is false, alpha will be discarded and the result will be the image
  // composited on black.
  NS_IMETHOD GetInputStream(const char *mimeType,
                            const char16_t *encoderOptions,
                            nsIInputStream **stream) = 0;

  // This gets an Azure SourceSurface for the canvas, this will be a snapshot
  // of the canvas at the time it was called.
  // If premultAlpha is provided, then it assumed the callee can handle
  // un-premultiplied surfaces, and *premultAlpha will be set to false
  // if one is returned.
  virtual already_AddRefed<mozilla::gfx::SourceSurface> GetSurfaceSnapshot(bool* premultAlpha = nullptr) = 0;

  // If this context is opaque, the backing store of the canvas should
  // be created as opaque; all compositing operators should assume the
  // dst alpha is always 1.0.  If this is never called, the context
  // defaults to false (not opaque).
  NS_IMETHOD SetIsOpaque(bool isOpaque) = 0;
  virtual bool GetIsOpaque() = 0;

  // Invalidate this context and release any held resources, in preperation
  // for possibly reinitializing with SetDimensions/InitializeWithSurface.
  NS_IMETHOD Reset() = 0;

  // Return the CanvasLayer for this context, creating
  // one for the given layer manager if not available.
  virtual already_AddRefed<Layer> GetCanvasLayer(nsDisplayListBuilder* builder,
                                                 Layer *oldLayer,
                                                 LayerManager *manager,
                                                 bool aMirror = false) = 0;

  // Return true if the canvas should be forced to be "inactive" to ensure
  // it can be drawn to the screen even if it's too large to be blitted by
  // an accelerated CanvasLayer.
  virtual bool ShouldForceInactiveLayer(LayerManager *manager) { return false; }

  virtual void MarkContextClean() = 0;

  // Called when a frame is captured.
  virtual void MarkContextCleanForFrameCapture() = 0;

  // Whether the context is clean or has been invalidated since the last frame
  // was captured.
  virtual bool IsContextCleanForFrameCapture() = 0;

  // Redraw the dirty rectangle of this canvas.
  NS_IMETHOD Redraw(const gfxRect &dirty) = 0;

  NS_IMETHOD SetContextOptions(JSContext* cx, JS::Handle<JS::Value> options,
                               mozilla::ErrorResult& aRvForDictionaryInit)
  {
    return NS_OK;
  }

  // return true and fills in the bounding rect if elementis a child and has a hit region.
  virtual bool GetHitRegionRect(mozilla::dom::Element* element, nsRect& rect) { return false; }

  // Given a point, return hit region ID if it exists or an empty string if it doesn't
  virtual nsString GetHitRegion(const mozilla::gfx::Point& point) { return nsString(); }

  virtual void OnVisibilityChange() {}

  virtual void OnMemoryPressure() {}

  //
  // shmem support
  //

  // If this context can be set to use Mozilla's Shmem segments as its backing
  // store, this will set it to that state. Note that if you have drawn
  // anything into this canvas before changing the shmem state, it will be
  // lost.
  NS_IMETHOD SetIsIPC(bool isIPC) = 0;

protected:
  RefPtr<mozilla::dom::HTMLCanvasElement> mCanvasElement;
  RefPtr<mozilla::dom::OffscreenCanvas> mOffscreenCanvas;
  RefPtr<nsRefreshDriver> mRefreshDriver;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsICanvasRenderingContextInternal,
                              NS_ICANVASRENDERINGCONTEXTINTERNAL_IID)

#endif /* nsICanvasRenderingContextInternal_h___ */
