/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BasicLayers.h"
#include "gfx3DMatrix.h"
#ifdef MOZ_ENABLE_D3D9_LAYER
# include "LayerManagerD3D9.h"
#endif //MOZ_ENABLE_D3D9_LAYER
#include "mozilla/BrowserElementParent.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "nsContentUtils.h"
#include "nsFrameLoader.h"
#include "nsIObserver.h"
#include "nsSubDocumentFrame.h"
#include "nsView.h"
#include "nsViewportFrame.h"
#include "RenderFrameParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/CompositorChild.h"
#include "ClientLayerManager.h"

typedef nsContentView::ViewConfig ViewConfig;
using namespace mozilla::dom;
using namespace mozilla::layers;

namespace mozilla {
namespace layout {

typedef FrameMetrics::ViewID ViewID;
typedef RenderFrameParent::ViewMap ViewMap;

// Represents (affine) transforms that are calculated from a content view.
struct ViewTransform {
  ViewTransform(nsIntPoint aTranslation = nsIntPoint(0, 0), float aXScale = 1, float aYScale = 1)
    : mTranslation(aTranslation)
    , mXScale(aXScale)
    , mYScale(aYScale)
  {}

  operator gfx3DMatrix() const
  {
    return
      gfx3DMatrix::Translation(mTranslation.x, mTranslation.y, 0) *
      gfx3DMatrix::ScalingMatrix(mXScale, mYScale, 1);
  }

  nsIntPoint mTranslation;
  float mXScale;
  float mYScale;
};

// Matrix helpers
// For our simple purposes, these helpers apply to 2D affine transformations
// that can be represented by a scale and a translation. This makes the math
// much easier because we only expect the diagonals and the translation
// coordinates of the matrix to be non-zero.

static double GetXScale(const gfx3DMatrix& aTransform)
{
  return aTransform._11;
}
 
static double GetYScale(const gfx3DMatrix& aTransform)
{
  return aTransform._22;
}

static void Scale(gfx3DMatrix& aTransform, double aXScale, double aYScale)
{
  aTransform._11 *= aXScale;
  aTransform._22 *= aYScale;
}

static void ReverseTranslate(gfx3DMatrix& aTransform, const gfxPoint& aOffset)
{
  aTransform._41 -= aOffset.x;
  aTransform._42 -= aOffset.y;
}


static void ApplyTransform(nsRect& aRect,
                           gfx3DMatrix& aTransform,
                           nscoord auPerDevPixel)
{
  aRect.x = aRect.x * aTransform._11 + aTransform._41 * auPerDevPixel;
  aRect.y = aRect.y * aTransform._22 + aTransform._42 * auPerDevPixel;
  aRect.width = aRect.width * aTransform._11;
  aRect.height = aRect.height * aTransform._22;
}
 
static void
AssertInTopLevelChromeDoc(ContainerLayer* aContainer,
                          nsIFrame* aContainedFrame)
{
  NS_ASSERTION(
    (aContainer->Manager()->GetBackendType() != mozilla::layers::LAYERS_BASIC) ||
    (aContainedFrame->GetNearestWidget() ==
     static_cast<BasicLayerManager*>(aContainer->Manager())->GetRetainerWidget()),
    "Expected frame to be in top-level chrome document");
}

// Return view for given ID in aMap, nullptr if not found.
static nsContentView*
FindViewForId(const ViewMap& aMap, ViewID aId)
{
  ViewMap::const_iterator iter = aMap.find(aId);
  return iter != aMap.end() ? iter->second : nullptr;
}

// Return the root content view in aMap, nullptr if not found.
static nsContentView*
FindRootView(const ViewMap& aMap)
{
  for (ViewMap::const_iterator iter = aMap.begin(), end = aMap.end();
       iter != end;
       ++iter) {
    if (iter->second->IsRoot())
      return iter->second;
  }
  return nullptr;
}

static const FrameMetrics*
GetFrameMetrics(Layer* aLayer)
{
  ContainerLayer* container = aLayer->AsContainerLayer();
  return container ? &container->GetFrameMetrics() : nullptr;
}

/**
 * Gets the layer-pixel offset of aContainerFrame's content rect top-left
 * from the nearest display item reference frame (which we assume will be inducing
 * a ContainerLayer).
 */
static nsIntPoint
GetContentRectLayerOffset(nsIFrame* aContainerFrame, nsDisplayListBuilder* aBuilder)
{
  nscoord auPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();

  // Offset to the content rect in case we have borders or padding
  // Note that aContainerFrame could be a reference frame itself, so
  // we need to be careful here to ensure that we call ToReferenceFrame
  // on aContainerFrame and not its parent.
  nsPoint frameOffset = aBuilder->ToReferenceFrame(aContainerFrame) +
    (aContainerFrame->GetContentRect().TopLeft() - aContainerFrame->GetPosition());

  return frameOffset.ToNearestPixels(auPerDevPixel);
}

// Compute the transform of the shadow tree contained by
// |aContainerFrame| to widget space.  We transform because the
// subprocess layer manager renders to a different top-left than where
// the shadow tree is drawn here and because a scale can be set on the
// shadow tree.
static ViewTransform
ComputeShadowTreeTransform(nsIFrame* aContainerFrame,
                           nsFrameLoader* aRootFrameLoader,
                           const FrameMetrics* aMetrics,
                           const ViewConfig& aConfig,
                           float aTempScaleX = 1.0,
                           float aTempScaleY = 1.0)
{
  // |aMetrics->mViewportScrollOffset| The frame's scroll offset when it was
  //                                   painted, in content document pixels.
  // |aConfig.mScrollOffset|           What our user expects, or wants, the
  //                                   frame scroll offset to be in chrome
  //                                   document app units.
  //
  // So we set a compensating translation that moves the content document
  // pixels to where the user wants them to be.
  //
  nscoord auPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();
  nsIntPoint scrollOffset =
    aConfig.mScrollOffset.ToNearestPixels(auPerDevPixel);
  LayerIntPoint metricsScrollOffset = RoundedToInt(aMetrics->GetScrollOffsetInLayerPixels());

  if (aRootFrameLoader->AsyncScrollEnabled() && !aMetrics->mDisplayPort.IsEmpty()) {
    // Only use asynchronous scrolling if it is enabled and there is a
    // displayport defined. It is useful to have a scroll layer that is
    // synchronously scrolled for identifying a scroll area before it is
    // being actively scrolled.
    nsIntPoint scrollCompensation(
      (scrollOffset.x / aTempScaleX - metricsScrollOffset.x),
      (scrollOffset.y / aTempScaleY - metricsScrollOffset.y));

    return ViewTransform(-scrollCompensation, aConfig.mXScale, aConfig.mYScale);
  } else {
    return ViewTransform(nsIntPoint(0, 0), 1, 1);
  }
}

// Use shadow layer tree to build display list for the browser's frame.
static void
BuildListForLayer(Layer* aLayer,
                  nsFrameLoader* aRootFrameLoader,
                  const gfx3DMatrix& aTransform,
                  nsDisplayListBuilder* aBuilder,
                  nsDisplayList& aShadowTree,
                  nsIFrame* aSubdocFrame)
{
  const FrameMetrics* metrics = GetFrameMetrics(aLayer);

  gfx3DMatrix transform;

  if (metrics && metrics->IsScrollable()) {
    const ViewID scrollId = metrics->mScrollId;

    // We need to figure out the bounds of the scrollable region using the
    // shadow layer tree from the remote process. The metrics viewport is
    // defined based on all the transformations of its parent layers and
    // the scale of the current layer.

    // Calculate transform for this layer.
    nsContentView* view =
      aRootFrameLoader->GetCurrentRemoteFrame()->GetContentView(scrollId);
    // XXX why don't we include aLayer->GetTransform() in the inverse-scale here?
    // This seems wrong, but it doesn't seem to cause bugs!
    gfx3DMatrix applyTransform = ComputeShadowTreeTransform(
      aSubdocFrame, aRootFrameLoader, metrics, view->GetViewConfig(),
      1 / GetXScale(aTransform), 1 / GetYScale(aTransform));
    transform = applyTransform * aLayer->GetTransform() * aTransform;

    // As mentioned above, bounds calculation also depends on the scale
    // of this layer.
    gfx3DMatrix tmpTransform = aTransform;
    Scale(tmpTransform, GetXScale(applyTransform), GetYScale(applyTransform));

    // Calculate rect for this layer based on aTransform.
    nsRect bounds;
    {
      bounds = CSSRect::ToAppUnits(metrics->mViewport);
      nscoord auPerDevPixel = aSubdocFrame->PresContext()->AppUnitsPerDevPixel();
      ApplyTransform(bounds, tmpTransform, auPerDevPixel);
    }

    aShadowTree.AppendToTop(
      new (aBuilder) nsDisplayRemoteShadow(aBuilder, aSubdocFrame, bounds, scrollId));

  } else {
    transform = aLayer->GetTransform() * aTransform;
  }

  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    BuildListForLayer(child, aRootFrameLoader, transform,
                      aBuilder, aShadowTree, aSubdocFrame);
  }
}

// Go down shadow layer tree and apply transformations for scrollable layers.
static void
TransformShadowTree(nsDisplayListBuilder* aBuilder, nsFrameLoader* aFrameLoader,
                    nsIFrame* aFrame, Layer* aLayer,
                    const ViewTransform& aTransform,
                    float aTempScaleDiffX = 1.0,
                    float aTempScaleDiffY = 1.0)
{
  LayerComposite* shadow = aLayer->AsLayerComposite();
  shadow->SetShadowClipRect(aLayer->GetClipRect());
  shadow->SetShadowVisibleRegion(aLayer->GetVisibleRegion());
  shadow->SetShadowOpacity(aLayer->GetOpacity());

  const FrameMetrics* metrics = GetFrameMetrics(aLayer);

  gfx3DMatrix shadowTransform = aLayer->GetTransform();
  ViewTransform layerTransform = aTransform;

  if (metrics && metrics->IsScrollable()) {
    const ViewID scrollId = metrics->mScrollId;
    const nsContentView* view =
      aFrameLoader->GetCurrentRemoteFrame()->GetContentView(scrollId);
    NS_ABORT_IF_FALSE(view, "Array of views should be consistent with layer tree");
    const gfx3DMatrix& currentTransform = aLayer->GetTransform();

    const ViewConfig& config = view->GetViewConfig();
    // With temporary scale we should compensate translation
    // using temporary scale value
    aTempScaleDiffX *= GetXScale(shadowTransform) * config.mXScale;
    aTempScaleDiffY *= GetYScale(shadowTransform) * config.mYScale;
    ViewTransform viewTransform = ComputeShadowTreeTransform(
      aFrame, aFrameLoader, metrics, view->GetViewConfig(),
      aTempScaleDiffX, aTempScaleDiffY
    );

    // Apply the layer's own transform *before* the view transform
    shadowTransform = gfx3DMatrix(viewTransform) * currentTransform;

    layerTransform = viewTransform;
    if (metrics->IsRootScrollable()) {
      // Apply the translation *before* we do the rest of the transforms.
      nsIntPoint offset = GetContentRectLayerOffset(aFrame, aBuilder);
      shadowTransform = shadowTransform *
          gfx3DMatrix::Translation(float(offset.x), float(offset.y), 0.0);
    }
  }

  if (aLayer->GetIsFixedPosition() &&
      !aLayer->GetParent()->GetIsFixedPosition()) {
    // Alter the shadow transform of fixed position layers in the situation
    // that the view transform's scroll position doesn't match the actual
    // scroll position, due to asynchronous layer scrolling.
    float offsetX = layerTransform.mTranslation.x;
    float offsetY = layerTransform.mTranslation.y;
    ReverseTranslate(shadowTransform, gfxPoint(offsetX, offsetY));
    const nsIntRect* clipRect = shadow->GetShadowClipRect();
    if (clipRect) {
      nsIntRect transformedClipRect(*clipRect);
      transformedClipRect.MoveBy(-offsetX, -offsetY);
      shadow->SetShadowClipRect(&transformedClipRect);
    }
  }

  // The transform already takes the resolution scale into account.  Since we
  // will apply the resolution scale again when computing the effective
  // transform, we must apply the inverse resolution scale here.
  if (ContainerLayer* c = aLayer->AsContainerLayer()) {
    shadowTransform.Scale(1.0f/c->GetPreXScale(),
                          1.0f/c->GetPreYScale(),
                          1);
  }
  shadowTransform.ScalePost(1.0f/aLayer->GetPostXScale(),
                            1.0f/aLayer->GetPostYScale(),
                            1);

  shadow->SetShadowTransform(shadowTransform);
  for (Layer* child = aLayer->GetFirstChild();
       child; child = child->GetNextSibling()) {
    TransformShadowTree(aBuilder, aFrameLoader, aFrame, child, layerTransform,
                        aTempScaleDiffX, aTempScaleDiffY);
  }
}

static void
ClearContainer(ContainerLayer* aContainer)
{
  while (Layer* layer = aContainer->GetFirstChild()) {
    aContainer->RemoveChild(layer);
  }
}

// Return true iff |aManager| is a "temporary layer manager".  They're
// used for small software rendering tasks, like drawWindow.  That's
// currently implemented by a BasicLayerManager without a backing
// widget, and hence in non-retained mode.
inline static bool
IsTempLayerManager(LayerManager* aManager)
{
  return (mozilla::layers::LAYERS_BASIC == aManager->GetBackendType() &&
          !static_cast<BasicLayerManager*>(aManager)->IsRetained());
}

// Recursively create a new array of scrollables, preserving any scrollables
// that are still in the layer tree.
//
// aXScale and aYScale are used to calculate any values that need to be in
// chrome-document CSS pixels and aren't part of the rendering loop, such as
// the initial scroll offset for a new view.
static void
BuildViewMap(ViewMap& oldContentViews, ViewMap& newContentViews,
             nsFrameLoader* aFrameLoader, Layer* aLayer,
             float aXScale = 1, float aYScale = 1,
             float aAccConfigXScale = 1, float aAccConfigYScale = 1)
{
  ContainerLayer* container = aLayer->AsContainerLayer();
  if (!container)
    return;
  const FrameMetrics metrics = container->GetFrameMetrics();
  const ViewID scrollId = metrics.mScrollId;
  const gfx3DMatrix transform = aLayer->GetTransform();
  aXScale *= GetXScale(transform);
  aYScale *= GetYScale(transform);

  if (metrics.IsScrollable()) {
    nscoord auPerDevPixel = aFrameLoader->GetPrimaryFrameOfOwningContent()
                                        ->PresContext()->AppUnitsPerDevPixel();
    nscoord auPerCSSPixel = auPerDevPixel * metrics.mDevPixelsPerCSSPixel.scale;
    nsContentView* view = FindViewForId(oldContentViews, scrollId);
    if (view) {
      // View already exists. Be sure to propagate scales for any values
      // that need to be calculated something in chrome-doc CSS pixels.
      ViewConfig config = view->GetViewConfig();
      aXScale *= config.mXScale;
      aYScale *= config.mYScale;
      view->mFrameLoader = aFrameLoader;
      // If scale has changed, then we should update
      // current scroll offset to new scaled value
      if (aAccConfigXScale != view->mParentScaleX ||
          aAccConfigYScale != view->mParentScaleY) {
        float xscroll = 0, yscroll = 0;
        view->GetScrollX(&xscroll);
        view->GetScrollY(&yscroll);
        xscroll = xscroll * (aAccConfigXScale / view->mParentScaleX);
        yscroll = yscroll * (aAccConfigYScale / view->mParentScaleY);
        view->ScrollTo(xscroll, yscroll);
        view->mParentScaleX = aAccConfigXScale;
        view->mParentScaleY = aAccConfigYScale;
      }
      // Collect only config scale values for scroll compensation
      aAccConfigXScale *= config.mXScale;
      aAccConfigYScale *= config.mYScale;
    } else {
      // View doesn't exist, so generate one. We start the view scroll offset at
      // the same position as the framemetric's scroll offset from the layer.
      // The default scale is 1, so no need to propagate scale down.
      ViewConfig config;
      config.mScrollOffset = nsPoint(
        NSIntPixelsToAppUnits(metrics.mScrollOffset.x, auPerCSSPixel) * aXScale,
        NSIntPixelsToAppUnits(metrics.mScrollOffset.y, auPerCSSPixel) * aYScale);
      view = new nsContentView(aFrameLoader, scrollId, metrics.mIsRoot, config);
      view->mParentScaleX = aAccConfigXScale;
      view->mParentScaleY = aAccConfigYScale;
    }

    // I don't know what units mViewportSize is in, hence use ToUnknownRect
    // here to mark the current frontier in type info propagation
    gfx::Rect viewport = metrics.mViewport.ToUnknownRect();
    view->mViewportSize = nsSize(
      NSIntPixelsToAppUnits(viewport.width, auPerDevPixel) * aXScale,
      NSIntPixelsToAppUnits(viewport.height, auPerDevPixel) * aYScale);
    view->mContentSize = nsSize(
      NSFloatPixelsToAppUnits(metrics.mScrollableRect.width, auPerCSSPixel) * aXScale,
      NSFloatPixelsToAppUnits(metrics.mScrollableRect.height, auPerCSSPixel) * aYScale);

    newContentViews[scrollId] = view;
  }

  for (Layer* child = aLayer->GetFirstChild();
       child; child = child->GetNextSibling()) {
    BuildViewMap(oldContentViews, newContentViews, aFrameLoader, child,
                 aXScale, aYScale, aAccConfigXScale, aAccConfigYScale);
  }
}

static void
BuildBackgroundPatternFor(ContainerLayer* aContainer,
                          Layer* aShadowRoot,
                          const ViewConfig& aConfig,
                          const gfxRGBA& aColor,
                          LayerManager* aManager,
                          nsIFrame* aFrame)
{
  LayerComposite* shadowRoot = aShadowRoot->AsLayerComposite();
  gfxMatrix t;
  if (!shadowRoot->GetShadowTransform().Is2D(&t)) {
    return;
  }

  // Get the rect bounding the shadow content, transformed into the
  // same space as |aFrame|
  nsIntRect contentBounds = shadowRoot->GetShadowVisibleRegion().GetBounds();
  gfxRect contentVis(contentBounds.x, contentBounds.y,
                     contentBounds.width, contentBounds.height);
  gfxRect localContentVis(t.Transform(contentVis));
  // Round *in* here because this area is punched out of the background
  localContentVis.RoundIn();
  nsIntRect localIntContentVis(localContentVis.X(), localContentVis.Y(),
                               localContentVis.Width(), localContentVis.Height());

  // Get the frame's rect
  nscoord auPerDevPixel = aFrame->PresContext()->AppUnitsPerDevPixel();
  nsIntRect frameRect = aFrame->GetRect().ToOutsidePixels(auPerDevPixel);

  // If the shadow tree covers the frame rect, don't bother building
  // the background, it wouldn't be visible
  if (localIntContentVis.Contains(frameRect)) {
    return;
  }
  nsRefPtr<ColorLayer> layer = aManager->CreateColorLayer();
  layer->SetColor(aColor);

  // The visible area of the background is the frame's area minus the
  // content area
  nsIntRegion bgRgn(frameRect);
  bgRgn.Sub(bgRgn, localIntContentVis);
  bgRgn.MoveBy(-frameRect.TopLeft());
  layer->SetVisibleRegion(bgRgn);

  aContainer->InsertAfter(layer, nullptr);
}

already_AddRefed<LayerManager>
GetFrom(nsFrameLoader* aFrameLoader)
{
  nsIDocument* doc = aFrameLoader->GetOwnerDoc();
  return nsContentUtils::LayerManagerForDocument(doc);
}

class RemoteContentController : public GeckoContentController {
public:
  RemoteContentController(RenderFrameParent* aRenderFrame)
    : mUILoop(MessageLoop::current())
    , mRenderFrame(aRenderFrame)
    , mHaveZoomConstraints(false)
    , mAllowZoom(true)
  { }

  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) MOZ_OVERRIDE
  {
    // We always need to post requests into the "UI thread" otherwise the
    // requests may get processed out of order.
    mUILoop->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &RemoteContentController::DoRequestContentRepaint,
                        aFrameMetrics));
  }

  virtual void HandleDoubleTap(const CSSIntPoint& aPoint,
                               int32_t aModifiers) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      // We have to send this message from the "UI thread" (main
      // thread).
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::HandleDoubleTap,
                          aPoint, aModifiers));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = static_cast<TabParent*>(mRenderFrame->Manager());
      browser->HandleDoubleTap(aPoint, aModifiers);
    }
  }

  virtual void HandleSingleTap(const CSSIntPoint& aPoint,
                               int32_t aModifiers) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      // We have to send this message from the "UI thread" (main
      // thread).
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::HandleSingleTap,
                          aPoint, aModifiers));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = static_cast<TabParent*>(mRenderFrame->Manager());
      browser->HandleSingleTap(aPoint, aModifiers);
    }
  }

  virtual void HandleLongTap(const CSSIntPoint& aPoint,
                             int32_t aModifiers) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      // We have to send this message from the "UI thread" (main
      // thread).
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::HandleLongTap,
                          aPoint, aModifiers));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = static_cast<TabParent*>(mRenderFrame->Manager());
      browser->HandleLongTap(aPoint, aModifiers);
    }
  }

  void ClearRenderFrame() { mRenderFrame = nullptr; }

  virtual void SendAsyncScrollDOMEvent(bool aIsRoot,
                                       const CSSRect& aContentRect,
                                       const CSSSize& aContentSize) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &RemoteContentController::SendAsyncScrollDOMEvent,
                          aIsRoot, aContentRect, aContentSize));
      return;
    }
    if (mRenderFrame && aIsRoot) {
      TabParent* browser = static_cast<TabParent*>(mRenderFrame->Manager());
      BrowserElementParent::DispatchAsyncScrollEvent(browser, aContentRect,
                                                     aContentSize);
    }
  }

  virtual void PostDelayedTask(Task* aTask, int aDelayMs) MOZ_OVERRIDE
  {
    MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
  }

  void SaveZoomConstraints(bool aAllowZoom,
                           const CSSToScreenScale& aMinZoom,
                           const CSSToScreenScale& aMaxZoom)
  {
    mHaveZoomConstraints = true;
    mAllowZoom = aAllowZoom;
    mMinZoom = aMinZoom;
    mMaxZoom = aMaxZoom;
  }

  virtual bool GetRootZoomConstraints(bool* aOutAllowZoom,
                                      CSSToScreenScale* aOutMinZoom,
                                      CSSToScreenScale* aOutMaxZoom)
  {
    if (mHaveZoomConstraints) {
      *aOutAllowZoom = mAllowZoom;
      *aOutMinZoom = mMinZoom;
      *aOutMaxZoom = mMaxZoom;
    }
    return mHaveZoomConstraints;
  }

  virtual void NotifyTransformBegin(const ScrollableLayerGuid& aGuid)
  {
    if (MessageLoop::current() != mUILoop) {
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::NotifyTransformBegin,
                          aGuid));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = static_cast<TabParent*>(mRenderFrame->Manager());
      browser->NotifyTransformBegin(aGuid.mScrollId);
    }
  }

  virtual void NotifyTransformEnd(const ScrollableLayerGuid& aGuid)
  {
    if (MessageLoop::current() != mUILoop) {
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::NotifyTransformEnd,
                          aGuid));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = static_cast<TabParent*>(mRenderFrame->Manager());
      browser->NotifyTransformEnd(aGuid.mScrollId);
    }
  }

private:
  void DoRequestContentRepaint(const FrameMetrics& aFrameMetrics)
  {
    if (mRenderFrame) {
      TabParent* browser = static_cast<TabParent*>(mRenderFrame->Manager());
      browser->UpdateFrame(aFrameMetrics);
    }
  }

  MessageLoop* mUILoop;
  RenderFrameParent* mRenderFrame;

  bool mHaveZoomConstraints;
  bool mAllowZoom;
  CSSToScreenScale mMinZoom;
  CSSToScreenScale mMaxZoom;
};

RenderFrameParent::RenderFrameParent(nsFrameLoader* aFrameLoader,
                                     ScrollingBehavior aScrollingBehavior,
                                     TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                     uint64_t* aId)
  : mLayersId(0)
  , mFrameLoader(aFrameLoader)
  , mFrameLoaderDestroyed(false)
  , mBackgroundColor(gfxRGBA(1, 1, 1))
{
  *aId = 0;

  nsRefPtr<LayerManager> lm = GetFrom(mFrameLoader);
  // Perhaps the document containing this frame currently has no presentation?
  if (lm && lm->GetBackendType() == LAYERS_CLIENT) {
    *aTextureFactoryIdentifier =
      static_cast<ClientLayerManager*>(lm.get())->GetTextureFactoryIdentifier();
  } else {
    *aTextureFactoryIdentifier = TextureFactoryIdentifier();
  }

  if (lm && lm->GetRoot() && lm->GetRoot()->AsContainerLayer()) {
    ViewID rootScrollId = lm->GetRoot()->AsContainerLayer()->GetFrameMetrics().mScrollId;
    if (rootScrollId != FrameMetrics::NULL_SCROLL_ID) {
      mContentViews[rootScrollId] = new nsContentView(aFrameLoader, rootScrollId, true);
    }
  }

  if (CompositorParent::CompositorLoop()) {
    // Our remote frame will push layers updates to the compositor,
    // and we'll keep an indirect reference to that tree.
    *aId = mLayersId = CompositorParent::AllocateLayerTreeId();
    if (lm && lm->GetBackendType() == LAYERS_CLIENT) {
      ClientLayerManager *clientManager = static_cast<ClientLayerManager*>(lm.get());
      clientManager->GetRemoteRenderer()->SendNotifyChildCreated(mLayersId);
    }
    if (aScrollingBehavior == ASYNC_PAN_ZOOM) {
      mContentController = new RemoteContentController(this);
      CompositorParent::SetControllerForLayerTree(mLayersId, mContentController);
    }
  }
  // Set a default RenderFrameParent
  mFrameLoader->SetCurrentRemoteFrame(this);
}

APZCTreeManager*
RenderFrameParent::GetApzcTreeManager()
{
  // We can't get a ref to the APZCTreeManager until after the child is
  // created and the static getter knows which CompositorParent is
  // instantiated with this layers ID. That's why try to fetch it when
  // we first need it and cache the result.
  if (!mApzcTreeManager) {
    mApzcTreeManager = CompositorParent::GetAPZCTreeManager(mLayersId);
  }
  return mApzcTreeManager.get();
}

RenderFrameParent::~RenderFrameParent()
{}

void
RenderFrameParent::Destroy()
{
  size_t numChildren = ManagedPLayerTransactionParent().Length();
  NS_ABORT_IF_FALSE(0 == numChildren || 1 == numChildren,
                    "render frame must only have 0 or 1 layer manager");

  if (numChildren) {
    LayerTransactionParent* layers =
      static_cast<LayerTransactionParent*>(ManagedPLayerTransactionParent()[0]);
    layers->Destroy();
  }

  mFrameLoaderDestroyed = true;
}

nsContentView*
RenderFrameParent::GetContentView(ViewID aId)
{
  return FindViewForId(mContentViews, aId);
}

nsContentView*
RenderFrameParent::GetRootContentView()
{
  return FindRootView(mContentViews);
}

void
RenderFrameParent::ContentViewScaleChanged(nsContentView* aView)
{
  // Since the scale has changed for a view, it and its descendents need their
  // shadow-space attributes updated. It's easiest to rebuild the view map.
  BuildViewMap();
}

void
RenderFrameParent::ShadowLayersUpdated(LayerTransactionParent* aLayerTree,
                                       const TargetConfig& aTargetConfig,
                                       bool isFirstPaint)
{
  // View map must only contain views that are associated with the current
  // shadow layer tree. We must always update the map when shadow layers
  // are updated.
  BuildViewMap();

  TriggerRepaint();
}

already_AddRefed<Layer>
RenderFrameParent::BuildLayer(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame,
                              LayerManager* aManager,
                              const nsIntRect& aVisibleRect,
                              nsDisplayItem* aItem,
                              const ContainerLayerParameters& aContainerParameters)
{
  NS_ABORT_IF_FALSE(aFrame,
                    "makes no sense to have a shadow tree without a frame");
  NS_ABORT_IF_FALSE(!mContainer ||
                    IsTempLayerManager(aManager) ||
                    mContainer->Manager() == aManager,
                    "retaining manager changed out from under us ... HELP!");

  if (IsTempLayerManager(aManager) ||
      (mContainer && mContainer->Manager() != aManager)) {
    // This can happen if aManager is a "temporary" manager, or if the
    // widget's layer manager changed out from under us.  We need to
    // FIXME handle the former case somehow, probably with an API to
    // draw a manager's subtree.  The latter is bad bad bad, but the
    // the NS_ABORT_IF_FALSE() above will flag it.  Returning nullptr
    // here will just cause the shadow subtree not to be rendered.
    NS_WARNING("Remote iframe not rendered");
    return nullptr;
  }

  uint64_t id = GetLayerTreeId();
  if (0 != id) {
    MOZ_ASSERT(!GetRootLayer());

    nsRefPtr<Layer> layer =
      (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem));
    if (!layer) {
      layer = aManager->CreateRefLayer();
    }
    if (!layer) {
      // Probably a temporary layer manager that doesn't know how to
      // use ref layers.
      return nullptr;
    }
    static_cast<RefLayer*>(layer.get())->SetReferentId(id);
    nsIntPoint offset = GetContentRectLayerOffset(aFrame, aBuilder);
    layer->SetVisibleRegion(aVisibleRect - offset);
    // We can only have an offset if we're a child of an inactive
    // container, but our display item is LAYER_ACTIVE_FORCE which
    // forces all layers above to be active.
    MOZ_ASSERT(aContainerParameters.mOffset == nsIntPoint());
    gfx3DMatrix m =
      gfx3DMatrix::Translation(offset.x, offset.y, 0.0);
    // Remote content can't be repainted by us, so we multiply down
    // the resolution that our container expects onto our container.
    m.Scale(aContainerParameters.mXScale, aContainerParameters.mYScale, 1.0);
    layer->SetBaseTransform(m);

    return layer.forget();
  }

  if (mContainer) {
    ClearContainer(mContainer);
    mContainer->SetPreScale(1.0f, 1.0f);
    mContainer->SetPostScale(1.0f, 1.0f);
    mContainer->SetInheritedScale(1.0f, 1.0f);
  }

  Layer* shadowRoot = GetRootLayer();
  if (!shadowRoot) {
    mContainer = nullptr;
    return nullptr;
  }

  NS_ABORT_IF_FALSE(!shadowRoot || shadowRoot->Manager() == aManager,
                    "retaining manager changed out from under us ... HELP!");

  // Wrap the shadow layer tree in mContainer.
  if (!mContainer) {
    mContainer = aManager->CreateContainerLayer();
  }
  NS_ABORT_IF_FALSE(!mContainer->GetFirstChild(),
                    "container of shadow tree shouldn't have a 'root' here");

  mContainer->InsertAfter(shadowRoot, nullptr);

  AssertInTopLevelChromeDoc(mContainer, aFrame);
  ViewTransform transform;
  TransformShadowTree(aBuilder, mFrameLoader, aFrame, shadowRoot, transform);
  mContainer->SetClipRect(nullptr);

  if (mFrameLoader->AsyncScrollEnabled()) {
    const nsContentView* view = GetRootContentView();
    BuildBackgroundPatternFor(mContainer,
                              shadowRoot,
                              view->GetViewConfig(),
                              mBackgroundColor,
                              aManager, aFrame);
  }
  mContainer->SetVisibleRegion(aVisibleRect);

  return nsRefPtr<Layer>(mContainer).forget();
}

void
RenderFrameParent::OwnerContentChanged(nsIContent* aContent)
{
  NS_ABORT_IF_FALSE(mFrameLoader->GetOwnerContent() == aContent,
                    "Don't build new map if owner is same!");
  BuildViewMap();
}

void
RenderFrameParent::NotifyInputEvent(const WidgetInputEvent& aEvent,
                                    ScrollableLayerGuid* aOutTargetGuid,
                                    WidgetInputEvent* aOutEvent)
{
  if (GetApzcTreeManager()) {
    GetApzcTreeManager()->ReceiveInputEvent(aEvent, aOutTargetGuid, aOutEvent);
  }
}

void
RenderFrameParent::NotifyDimensionsChanged(ScreenIntSize size)
{
  if (GetApzcTreeManager()) {
    GetApzcTreeManager()->UpdateRootCompositionBounds(
      mLayersId, ScreenIntRect(ScreenIntPoint(), size));
  }
}

void
RenderFrameParent::ActorDestroy(ActorDestroyReason why)
{
  if (mLayersId != 0) {
    CompositorParent::DeallocateLayerTreeId(mLayersId);
    if (mContentController) {
      // Stop our content controller from requesting repaints of our
      // content.
      mContentController->ClearRenderFrame();
      // TODO: notify the compositor?
    }
  }

  if (mFrameLoader && mFrameLoader->GetCurrentRemoteFrame() == this) {
    // XXX this might cause some weird issues ... we'll just not
    // redraw the part of the window covered by this until the "next"
    // remote frame has a layer-tree transaction.  For
    // why==NormalShutdown, we'll definitely want to do something
    // better, especially as nothing guarantees another Update() from
    // the "next" remote layer tree.
    mFrameLoader->SetCurrentRemoteFrame(nullptr);
  }
  mFrameLoader = nullptr;
}

bool
RenderFrameParent::RecvNotifyCompositorTransaction()
{
  TriggerRepaint();
  return true;
}

bool
RenderFrameParent::RecvUpdateHitRegion(const nsRegion& aRegion)
{
  mTouchRegion = aRegion;
  return true;
}

PLayerTransactionParent*
RenderFrameParent::AllocPLayerTransactionParent()
{
  if (!mFrameLoader || mFrameLoaderDestroyed) {
    return nullptr;
  }
  nsRefPtr<LayerManager> lm = GetFrom(mFrameLoader);
  LayerTransactionParent* result = new LayerTransactionParent(lm->AsLayerManagerComposite(), this, 0);
  result->AddIPDLReference();
  return result;
}

bool
RenderFrameParent::DeallocPLayerTransactionParent(PLayerTransactionParent* aLayers)
{
  static_cast<LayerTransactionParent*>(aLayers)->ReleaseIPDLReference();
  return true;
}

void
RenderFrameParent::BuildViewMap()
{
  ViewMap newContentViews;
  // BuildViewMap assumes we have a primary frame, which may not be the case.
  if (GetRootLayer() && mFrameLoader->GetPrimaryFrameOfOwningContent()) {
    // Some of the content views in our hash map may no longer be active. To
    // tag them as inactive and to remove any chance of them using a dangling
    // pointer, we set mContentView to nullptr.
    //
    // BuildViewMap will restore mFrameLoader if the content view is still
    // in our hash table.

    for (ViewMap::const_iterator iter = mContentViews.begin();
         iter != mContentViews.end();
         ++iter) {
      iter->second->mFrameLoader = nullptr;
    }

    mozilla::layout::BuildViewMap(mContentViews, newContentViews, mFrameLoader, GetRootLayer());
  }

  // Here, we guarantee that *only* the root view is preserved in
  // case we couldn't build a new view map above. This is important because
  // the content view map should only contain the root view and content
  // views that are present in the layer tree.
  if (newContentViews.empty()) {
    nsContentView* rootView = FindRootView(mContentViews);
    if (rootView)
      newContentViews[rootView->GetId()] = rootView;
  }

  mContentViews = newContentViews;
}

void
RenderFrameParent::TriggerRepaint()
{
  mFrameLoader->SetCurrentRemoteFrame(this);

  nsIFrame* docFrame = mFrameLoader->GetPrimaryFrameOfOwningContent();
  if (!docFrame) {
    // Bad, but nothing we can do about it (XXX/cjones: or is there?
    // maybe bug 589337?).  When the new frame is created, we'll
    // probably still be the current render frame and will get to draw
    // our content then.  Or, we're shutting down and this update goes
    // to /dev/null.
    return;
  }

  docFrame->InvalidateLayer(nsDisplayItem::TYPE_REMOTE);
}

LayerTransactionParent*
RenderFrameParent::GetShadowLayers() const
{
  const InfallibleTArray<PLayerTransactionParent*>& shadowParents = ManagedPLayerTransactionParent();
  NS_ABORT_IF_FALSE(shadowParents.Length() <= 1,
                    "can only support at most 1 LayerTransactionParent");
  return (shadowParents.Length() == 1) ?
    static_cast<LayerTransactionParent*>(shadowParents[0]) : nullptr;
}

uint64_t
RenderFrameParent::GetLayerTreeId() const
{
  return mLayersId;
}

Layer*
RenderFrameParent::GetRootLayer() const
{
  LayerTransactionParent* shadowLayers = GetShadowLayers();
  return shadowLayers ? shadowLayers->GetRoot() : nullptr;
}

void
RenderFrameParent::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                    nsSubDocumentFrame* aFrame,
                                    const nsRect& aDirtyRect,
                                    const nsDisplayListSet& aLists)
{
  // We're the subdoc for <browser remote="true"> and it has
  // painted content.  Display its shadow layer tree.
  DisplayListClipState::AutoSaveRestore clipState(aBuilder);

  nsPoint offset = aBuilder->ToReferenceFrame(aFrame);
  nsRect bounds = aFrame->EnsureInnerView()->GetBounds() + offset;
  clipState.ClipContentDescendants(bounds);

  Layer* container = GetRootLayer();
  if (aBuilder->IsForEventDelivery() && container) {
    ViewTransform offset =
      ViewTransform(GetContentRectLayerOffset(aFrame, aBuilder));
    BuildListForLayer(container, mFrameLoader, offset,
                      aBuilder, *aLists.Content(), aFrame);
  } else {
    aLists.Content()->AppendToTop(
      new (aBuilder) nsDisplayRemote(aBuilder, aFrame, this));
  }
}

void
RenderFrameParent::ZoomToRect(uint32_t aPresShellId, ViewID aViewId,
                              const CSSRect& aRect)
{
  if (GetApzcTreeManager()) {
    GetApzcTreeManager()->ZoomToRect(ScrollableLayerGuid(mLayersId, aPresShellId, aViewId),
                                     aRect);
  }
}

void
RenderFrameParent::ContentReceivedTouch(const ScrollableLayerGuid& aGuid,
                                        bool aPreventDefault)
{
  if (GetApzcTreeManager()) {
    GetApzcTreeManager()->ContentReceivedTouch(aGuid, aPreventDefault);
  }
}

void
RenderFrameParent::UpdateZoomConstraints(uint32_t aPresShellId,
                                         ViewID aViewId,
                                         bool aIsRoot,
                                         bool aAllowZoom,
                                         const CSSToScreenScale& aMinZoom,
                                         const CSSToScreenScale& aMaxZoom)
{
  if (mContentController && aIsRoot) {
    mContentController->SaveZoomConstraints(aAllowZoom, aMinZoom, aMaxZoom);
  }
  if (GetApzcTreeManager()) {
    GetApzcTreeManager()->UpdateZoomConstraints(ScrollableLayerGuid(mLayersId, aPresShellId, aViewId),
                                                aAllowZoom, aMinZoom, aMaxZoom);
  }
}

void
RenderFrameParent::UpdateScrollOffset(uint32_t aPresShellId, ViewID aViewId, const CSSIntPoint& aScrollOffset)
{
  if (GetApzcTreeManager()) {
    GetApzcTreeManager()->UpdateScrollOffset(ScrollableLayerGuid(mLayersId, aPresShellId, aViewId),
                                             aScrollOffset);
  }
}

bool
RenderFrameParent::HitTest(const nsRect& aRect)
{
  return mTouchRegion.Contains(aRect);
}

}  // namespace layout
}  // namespace mozilla

already_AddRefed<Layer>
nsDisplayRemote::BuildLayer(nsDisplayListBuilder* aBuilder,
                            LayerManager* aManager,
                            const ContainerLayerParameters& aContainerParameters)
{
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsIntRect visibleRect = GetVisibleRect().ToNearestPixels(appUnitsPerDevPixel);
  visibleRect += aContainerParameters.mOffset;
  nsRefPtr<Layer> layer = mRemoteFrame->BuildLayer(aBuilder, mFrame, aManager, visibleRect, this, aContainerParameters);
  return layer.forget();
}

void
nsDisplayRemote::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                         HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  if (mRemoteFrame->HitTest(aRect)) {
    aOutFrames->AppendElement(mFrame);
  }
}

void
nsDisplayRemoteShadow::HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                         HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames)
{
  // If we are here, then rects have intersected.
  //
  // XXX I think iframes and divs can be rounded like anything else but we don't
  //     cover that case here.
  //
  if (aState->mShadows) {
    aState->mShadows->AppendElement(mId);
  }
}
