/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERLAYERMANAGER_H
#define GFX_WEBRENDERLAYERMANAGER_H

#include "Layers.h"
#include "mozilla/gfx/webrender.h"
#include "mozilla/layers/CompositorController.h"

class nsIWidget;

namespace mozilla {
namespace layers {

class WebRenderBridgeChild;

template<class T>
static inline WRRect toWrRect(const gfx::RectTyped<T>& rect)
{
  WRRect r;
  r.x = rect.x;
  r.y = rect.y;
  r.width = rect.width;
  r.height = rect.height;
  return r;
}

template<class T>
static inline WRRect toWrRect(const gfx::IntRectTyped<T>& rect)
{
  return toWrRect(IntRectToRect(rect));
}


class WebRenderLayerManager;
class APZCTreeManager;

class WebRenderLayer
{
public:
  virtual Layer* GetLayer() = 0;
  virtual void RenderLayer() = 0;

  virtual already_AddRefed<gfx::SourceSurface> GetAsSourceSurface() { return nullptr; }
  static inline WebRenderLayer*
  ToWebRenderLayer(Layer* aLayer)
  {
    return static_cast<WebRenderLayer*>(aLayer->ImplData());
  }

  WebRenderLayerManager* WRManager();
  WebRenderBridgeChild* WRBridge();

  gfx::Rect RelativeToVisible(gfx::Rect aRect);
  gfx::Rect RelativeToTransformedVisible(gfx::Rect aRect);
  gfx::Rect ParentStackingContextBounds(size_t aScrollMetadataIndex);
  gfx::Rect RelativeToParent(gfx::Rect aRect);
  gfx::Rect TransformedVisibleBoundsRelativeToParent();
};

class MOZ_RAII WRScrollFrameStackingContextGenerator
{
public:
  explicit WRScrollFrameStackingContextGenerator(WebRenderLayer* aLayer);
  ~WRScrollFrameStackingContextGenerator();
private:
  WebRenderLayer* mLayer;
};

class WebRenderLayerManager final : public LayerManager
{
public:
  explicit WebRenderLayerManager(nsIWidget* aWidget);
  void Initialize(PCompositorBridgeChild* aCBChild, uint64_t aLayersId);

  virtual void Destroy() override;

protected:
  virtual ~WebRenderLayerManager();

public:
  WebRenderLayerManager* AsWebRenderLayerManager() override { return this; }
  CompositorBridgeChild* GetCompositorBridgeChild();

  virtual int32_t GetMaxTextureSize() const override;

  virtual bool BeginTransactionWithTarget(gfxContext* aTarget) override;
  virtual bool BeginTransaction() override;
  virtual bool EndEmptyTransaction(EndTransactionFlags aFlags = END_DEFAULT) override;
  virtual void EndTransaction(DrawPaintedLayerCallback aCallback,
                              void* aCallbackData,
                              EndTransactionFlags aFlags = END_DEFAULT) override;

  virtual LayersBackend GetBackendType() override { return LayersBackend::LAYERS_WR; }
  virtual void GetBackendName(nsAString& name) override { name.AssignLiteral("WebRender"); }
  virtual const char* Name() const override { return "WebRender"; }

  virtual void SetRoot(Layer* aLayer) override;

  virtual already_AddRefed<PaintedLayer> CreatePaintedLayer() override;
  virtual already_AddRefed<ContainerLayer> CreateContainerLayer() override;
  virtual already_AddRefed<ImageLayer> CreateImageLayer() override;
  virtual already_AddRefed<CanvasLayer> CreateCanvasLayer() override;
  virtual already_AddRefed<ReadbackLayer> CreateReadbackLayer() override;
  virtual already_AddRefed<ColorLayer> CreateColorLayer() override;
  virtual already_AddRefed<RefLayer> CreateRefLayer() override;

  virtual bool NeedsWidgetInvalidation() override { return true; }

  DrawPaintedLayerCallback GetPaintedLayerCallback() const
  { return mPaintedLayerCallback; }

  void* GetPaintedLayerCallbackData() const
  { return mPaintedLayerCallbackData; }

  // adds an imagekey to a list of keys that will be discarded on the next
  // transaction or destruction
  void AddImageKeyForDiscard(WRImageKey);
  void DiscardImages();

  WebRenderBridgeChild* WRBridge() { return mWRChild; }

private:
  nsIWidget* MOZ_NON_OWNING_REF mWidget;
  std::vector<WRImageKey> mImageKeys;

  /* PaintedLayer callbacks; valid at the end of a transaciton,
   * while rendering */
  DrawPaintedLayerCallback mPaintedLayerCallback;
  void *mPaintedLayerCallbackData;

  RefPtr<WebRenderBridgeChild> mWRChild;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_WEBRENDERLAYERMANAGER_H */
