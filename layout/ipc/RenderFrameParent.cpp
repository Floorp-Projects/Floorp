/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozilla/layers/ShadowLayersParent.h"

#include "BasicLayers.h"
#include "LayerManagerOGL.h"
#ifdef MOZ_ENABLE_D3D9_LAYER
#include "LayerManagerD3D9.h"
#endif //MOZ_ENABLE_D3D9_LAYER
#include "RenderFrameParent.h"

#include "gfx3DMatrix.h"
#include "nsFrameLoader.h"
#include "nsViewportFrame.h"
#include "nsSubDocumentFrame.h"
#include "nsIObserver.h"

typedef nsContentView::ViewConfig ViewConfig;
using namespace mozilla::layers;

namespace mozilla {
namespace layout {

typedef FrameMetrics::ViewID ViewID;
typedef RenderFrameParent::ViewMap ViewMap;

nsRefPtr<ImageContainer> sCheckerboard = nsnull;

class CheckerBoardPatternDeleter : public nsIObserver
{
public:
  NS_DECL_NSIOBSERVER
  NS_DECL_ISUPPORTS
};

NS_IMPL_ISUPPORTS1(CheckerBoardPatternDeleter, nsIObserver)

NS_IMETHODIMP
CheckerBoardPatternDeleter::Observe(nsISupports* aSubject,
                                    const char* aTopic,
                                    const PRUnichar* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    sCheckerboard = nsnull;
  }
  return NS_OK;
}

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
      gfx3DMatrix::ScalingMatrix(mXScale, mYScale, 1) *
      gfx3DMatrix::Translation(mTranslation.x, mTranslation.y, 0);
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

static void ReverseTranslate(gfx3DMatrix& aTransform, ViewTransform& aViewTransform)
{
  aTransform._41 -= aViewTransform.mTranslation.x * aViewTransform.mXScale;
  aTransform._42 -= aViewTransform.mTranslation.y * aViewTransform.mYScale;
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
    (aContainer->Manager()->GetBackendType() != LayerManager::LAYERS_BASIC) ||
    (aContainedFrame->GetNearestWidget() ==
     static_cast<BasicLayerManager*>(aContainer->Manager())->GetRetainerWidget()),
    "Expected frame to be in top-level chrome document");
}

// Return view for given ID in aArray, NULL if not found.
static nsContentView*
FindViewForId(const ViewMap& aMap, ViewID aId)
{
  ViewMap::const_iterator iter = aMap.find(aId);
  return iter != aMap.end() ? iter->second : NULL;
}

static const FrameMetrics*
GetFrameMetrics(Layer* aLayer)
{
  ContainerLayer* container = aLayer->AsContainerLayer();
  return container ? &container->GetFrameMetrics() : NULL;
}

static nsIntPoint
GetRootFrameOffset(nsIFrame* aContainerFrame, nsDisplayListBuilder* aBuilder)
{
  nscoord auPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();

  // Offset to the content rect in case we have borders or padding
  nsPoint frameOffset =
    (aBuilder->ToReferenceFrame(aContainerFrame->GetParent()) +
     aContainerFrame->GetContentRect().TopLeft());

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
                           float aInverseScaleX,
                           float aInverseScaleY)
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
  // metricsScrollOffset is in layer coordinates.
  nsIntPoint metricsScrollOffset = aMetrics->mViewportScrollOffset;

  if (aRootFrameLoader->AsyncScrollEnabled() && !aMetrics->mDisplayPort.IsEmpty()) {
    // Only use asynchronous scrolling if it is enabled and there is a
    // displayport defined. It is useful to have a scroll layer that is
    // synchronously scrolled for identifying a scroll area before it is
    // being actively scrolled.
    nsIntPoint scrollCompensation(
      scrollOffset.x * aInverseScaleX - metricsScrollOffset.x * aConfig.mXScale,
      scrollOffset.y * aInverseScaleY - metricsScrollOffset.y * aConfig.mYScale);

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
      nscoord auPerDevPixel = aSubdocFrame->PresContext()->AppUnitsPerDevPixel();
      bounds = metrics->mViewport.ToAppUnits(auPerDevPixel);
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
                    const ViewTransform& aTransform)
{
  ShadowLayer* shadow = aLayer->AsShadowLayer();
  shadow->SetShadowClipRect(aLayer->GetClipRect());
  shadow->SetShadowVisibleRegion(aLayer->GetVisibleRegion());

  const FrameMetrics* metrics = GetFrameMetrics(aLayer);

  gfx3DMatrix shadowTransform;
  ViewTransform layerTransform = aTransform;

  if (metrics && metrics->IsScrollable()) {
    const ViewID scrollId = metrics->mScrollId;
    const nsContentView* view =
      aFrameLoader->GetCurrentRemoteFrame()->GetContentView(scrollId);
    NS_ABORT_IF_FALSE(view, "Array of views should be consistent with layer tree");
    const gfx3DMatrix& currentTransform = aLayer->GetTransform();

    ViewTransform viewTransform = ComputeShadowTreeTransform(
      aFrame, aFrameLoader, metrics, view->GetViewConfig(),
      1 / (GetXScale(currentTransform)*layerTransform.mXScale),
      1 / (GetYScale(currentTransform)*layerTransform.mYScale)
    );

    // Apply the layer's own transform *before* the view transform
    shadowTransform = gfx3DMatrix(viewTransform) * currentTransform;

    if (metrics->IsRootScrollable()) {
      layerTransform.mTranslation = viewTransform.mTranslation;
      // Apply the root frame translation *before* we do the rest of the transforms.
      nsIntPoint rootFrameOffset = GetRootFrameOffset(aFrame, aBuilder);
      shadowTransform = shadowTransform *
          gfx3DMatrix::Translation(float(rootFrameOffset.x), float(rootFrameOffset.y), 0.0);
      layerTransform.mXScale *= GetXScale(currentTransform);
      layerTransform.mYScale *= GetYScale(currentTransform);
    }
  } else {
    shadowTransform = aLayer->GetTransform();
  }

  if (aLayer->GetIsFixedPosition() &&
      !aLayer->GetParent()->GetIsFixedPosition()) {
    ReverseTranslate(shadowTransform, layerTransform);
    const nsIntRect* clipRect = shadow->GetShadowClipRect();
    if (clipRect) {
      nsIntRect transformedClipRect(*clipRect);
      transformedClipRect.MoveBy(shadowTransform._41, shadowTransform._42);
      shadow->SetShadowClipRect(&transformedClipRect);
    }
  }

  shadow->SetShadowTransform(shadowTransform);
  for (Layer* child = aLayer->GetFirstChild();
       child; child = child->GetNextSibling()) {
    TransformShadowTree(aBuilder, aFrameLoader, aFrame, child, layerTransform);
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
static bool
IsTempLayerManager(LayerManager* aManager)
{
  return (LayerManager::LAYERS_BASIC == aManager->GetBackendType() &&
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
             float aXScale = 1, float aYScale = 1)
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
    nsContentView* view = FindViewForId(oldContentViews, scrollId);
    if (view) {
      // View already exists. Be sure to propagate scales for any values
      // that need to be calculated something in chrome-doc CSS pixels.
      ViewConfig config = view->GetViewConfig();
      aXScale *= config.mXScale;
      aYScale *= config.mYScale;
      view->mFrameLoader = aFrameLoader;
    } else {
      // View doesn't exist, so generate one. We start the view scroll offset at
      // the same position as the framemetric's scroll offset from the layer.
      // The default scale is 1, so no need to propagate scale down.
      ViewConfig config;
      config.mScrollOffset = nsPoint(
        NSIntPixelsToAppUnits(metrics.mViewportScrollOffset.x, auPerDevPixel) * aXScale,
        NSIntPixelsToAppUnits(metrics.mViewportScrollOffset.y, auPerDevPixel) * aYScale);
      view = new nsContentView(aFrameLoader, scrollId, config);
    }

    view->mViewportSize = nsSize(
      NSIntPixelsToAppUnits(metrics.mViewport.width, auPerDevPixel) * aXScale,
      NSIntPixelsToAppUnits(metrics.mViewport.height, auPerDevPixel) * aYScale);
    view->mContentSize = nsSize(
      NSIntPixelsToAppUnits(metrics.mContentSize.width, auPerDevPixel) * aXScale,
      NSIntPixelsToAppUnits(metrics.mContentSize.height, auPerDevPixel) * aYScale);

    newContentViews[scrollId] = view;
  }

  for (Layer* child = aLayer->GetFirstChild();
       child; child = child->GetNextSibling()) {
    BuildViewMap(oldContentViews, newContentViews, aFrameLoader, child,
                 aXScale, aYScale);
  }
}

#define BOARDSIZE 32
#define CHECKERSIZE 16
already_AddRefed<gfxASurface>
GetBackgroundImage()
{
  static unsigned int data[BOARDSIZE * BOARDSIZE];
  static bool initialized = false;
  if (!initialized) {
    initialized = true;
    for (unsigned int y = 0; y < BOARDSIZE; y++) {
      for (unsigned int x = 0; x < BOARDSIZE; x++) {
        bool col_odd = (x / CHECKERSIZE) & 1;
        bool row_odd = (y / CHECKERSIZE) & 1;
        if (col_odd ^ row_odd) { // xor
          data[y * BOARDSIZE + x] = 0xFFFFFFFF;
        }
        else {
          data[y * BOARDSIZE + x] = 0xFFDDDDDD;
        }
      }
    }
  }

  nsRefPtr<gfxASurface> s =
    new gfxImageSurface((unsigned char*) data,
                        gfxIntSize(BOARDSIZE, BOARDSIZE),
                        BOARDSIZE * sizeof(unsigned int),
                        gfxASurface::ImageFormatARGB32);
  return s.forget();
}

static void
BuildBackgroundPatternFor(ContainerLayer* aContainer,
                          ContainerLayer* aShadowRoot,
                          const FrameMetrics& aMetrics,
                          const ViewConfig& aConfig,
                          LayerManager* aManager,
                          nsIFrame* aFrame,
                          nsDisplayListBuilder* aBuilder)
{
  // We tile a visible region that is the frame's area \setminus the
  // rect in our frame onto which valid pixels from remote content
  // will be drawn.  It's just a waste of CPU cycles to draw a
  // checkerboard behind that content.
  //
  // We want to give the background the illusion of moving while the
  // user pans, so we nudge the tiling area a bit based on the
  // "desired" scroll offset.
  //
  // The background-image layer is added to the layer tree "behind"
  // the shadow tree.  It doesn't matter in theory which is behind/in
  // front, except that having the background in front of content
  // means we have to be more careful about snapping boundaries,
  // whereas having it behind allows us to trade off simplicity for
  // "wasted" drawing of a few extra pixels.
  ShadowLayer* shadowRoot = aShadowRoot->AsShadowLayer();
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

  nsRefPtr<gfxASurface> bgImage = GetBackgroundImage();
  gfxIntSize bgImageSize = bgImage->GetSize();

  // Set up goop needed to get a cairo image into its own layer
  if (!sCheckerboard) {
    sCheckerboard = aManager->CreateImageContainer().get();
    const Image::Format fmts[] = { Image::CAIRO_SURFACE };
    nsRefPtr<Image> img = sCheckerboard->CreateImage(fmts, 1);
    CairoImage::Data data = { bgImage.get(), bgImageSize };
    static_cast<CairoImage*>(img.get())->SetData(data);
    sCheckerboard->SetCurrentImage(img);
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (!observerService) {
      return;
    }
    nsresult rv = observerService->AddObserver(new CheckerBoardPatternDeleter, "xpcom-shutdown", PR_FALSE);
    if (NS_FAILED(rv)) {
      return;
    }
  }

  nsRefPtr<ImageLayer> layer = aManager->CreateImageLayer();
  layer->SetContainer(sCheckerboard);

  // The tile source is the entire background image
  nsIntRect tileSource(0, 0, bgImageSize.width, bgImageSize.height);
  layer->SetTileSourceRect(&tileSource);

  // The origin of the tiling plane, top-left of the tile source rect,
  // is at layer-space point <0,0>.  Set up a translation from that
  // origin to the frame top-left, with the little nudge included.
  nsIntPoint translation = frameRect.TopLeft();
  nsIntPoint panNudge = aConfig.mScrollOffset.ToNearestPixels(auPerDevPixel);
  // This offset must be positive to ensure that the tiling rect
  // contains the frame's visible rect.  The "desired" scroll offset
  // is allowed to be negative, however, so we fix that up here.
  panNudge.x = (panNudge.x % bgImageSize.width);
  if (panNudge.x < 0) panNudge.x += bgImageSize.width;
  panNudge.y = (panNudge.y % bgImageSize.height);
  if (panNudge.y < 0) panNudge.y += bgImageSize.height;

  translation -= panNudge;
  layer->SetTransform(gfx3DMatrix::Translation(translation.x, translation.y, 0));

  // The visible area of the background is the frame's area minus the
  // content area
  nsIntRegion bgRgn(frameRect);
  bgRgn.Sub(bgRgn, localIntContentVis);
  bgRgn.MoveBy(-translation);
  layer->SetVisibleRegion(bgRgn);

  aContainer->InsertAfter(layer, nsnull);
}

RenderFrameParent::RenderFrameParent(nsFrameLoader* aFrameLoader)
  : mFrameLoader(aFrameLoader)
  , mFrameLoaderDestroyed(false)
{
  if (aFrameLoader) {
    mContentViews[FrameMetrics::ROOT_SCROLL_ID] =
      new nsContentView(aFrameLoader, FrameMetrics::ROOT_SCROLL_ID);
  }
}

RenderFrameParent::~RenderFrameParent()
{}

void
RenderFrameParent::Destroy()
{
  size_t numChildren = ManagedPLayersParent().Length();
  NS_ABORT_IF_FALSE(0 == numChildren || 1 == numChildren,
                    "render frame must only have 0 or 1 layer manager");

  if (numChildren) {
    ShadowLayersParent* layers =
      static_cast<ShadowLayersParent*>(ManagedPLayersParent()[0]);
    layers->Destroy();
  }

  mFrameLoaderDestroyed = true;
}

nsContentView*
RenderFrameParent::GetContentView(ViewID aId)
{
  return FindViewForId(mContentViews, aId);
}

void
RenderFrameParent::ContentViewScaleChanged(nsContentView* aView)
{
  // Since the scale has changed for a view, it and its descendents need their
  // shadow-space attributes updated. It's easiest to rebuild the view map.
  BuildViewMap();
}

void
RenderFrameParent::ShadowLayersUpdated()
{
  mFrameLoader->SetCurrentRemoteFrame(this);

  // View map must only contain views that are associated with the current
  // shadow layer tree. We must always update the map when shadow layers
  // are updated.
  BuildViewMap();

  nsIFrame* docFrame = mFrameLoader->GetPrimaryFrameOfOwningContent();
  if (!docFrame) {
    // Bad, but nothing we can do about it (XXX/cjones: or is there?
    // maybe bug 589337?).  When the new frame is created, we'll
    // probably still be the current render frame and will get to draw
    // our content then.  Or, we're shutting down and this update goes
    // to /dev/null.
    return;
  }

  // FIXME/cjones: we should collect the rects/regions updated for
  // Painted*Layer() calls and pass that region to here, then only
  // invalidate that rect
  //
  // We pass INVALIDATE_NO_THEBES_LAYERS here because we're
  // invalidating the <browser> on behalf of its counterpart in the
  // content process.  Not only do we not need to invalidate the
  // shadow layers, things would just break if we did --- we have no
  // way to repaint shadow layers from this process.
  nsRect rect = nsRect(nsPoint(0, 0), docFrame->GetRect().Size());
  docFrame->InvalidateWithFlags(rect, nsIFrame::INVALIDATE_NO_THEBES_LAYERS);
}

already_AddRefed<Layer>
RenderFrameParent::BuildLayer(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame,
                              LayerManager* aManager,
                              const nsIntRect& aVisibleRect)
{
  NS_ABORT_IF_FALSE(aFrame,
                    "makes no sense to have a shadow tree without a frame");
  NS_ABORT_IF_FALSE(!mContainer ||
                    IsTempLayerManager(aManager) ||
                    mContainer->Manager() == aManager,
                    "retaining manager changed out from under us ... HELP!");

  if (mContainer && mContainer->Manager() != aManager) {
    // This can happen if aManager is a "temporary" manager, or if the
    // widget's layer manager changed out from under us.  We need to
    // FIXME handle the former case somehow, probably with an API to
    // draw a manager's subtree.  The latter is bad bad bad, but the
    // the NS_ABORT_IF_FALSE() above will flag it.  Returning NULL
    // here will just cause the shadow subtree not to be rendered.
    return nsnull;
  }

  if (mContainer) {
    ClearContainer(mContainer);
  }

  ContainerLayer* shadowRoot = GetRootLayer();
  if (!shadowRoot) {
    mContainer = nsnull;
    return nsnull;
  }

  NS_ABORT_IF_FALSE(!shadowRoot || shadowRoot->Manager() == aManager,
                    "retaining manager changed out from under us ... HELP!");

  // Wrap the shadow layer tree in mContainer.
  if (!mContainer) {
    mContainer = aManager->CreateContainerLayer();
  }
  NS_ABORT_IF_FALSE(!mContainer->GetFirstChild(),
                    "container of shadow tree shouldn't have a 'root' here");

  mContainer->InsertAfter(shadowRoot, nsnull);

  AssertInTopLevelChromeDoc(mContainer, aFrame);
  ViewTransform transform;
  TransformShadowTree(aBuilder, mFrameLoader, aFrame, shadowRoot, transform);
  mContainer->SetClipRect(nsnull);

  if (mFrameLoader->AsyncScrollEnabled()) {
    const nsContentView* view = GetContentView(FrameMetrics::ROOT_SCROLL_ID);
    BuildBackgroundPatternFor(mContainer,
                              shadowRoot,
                              shadowRoot->GetFrameMetrics(),
                              view->GetViewConfig(),
                              aManager, aFrame, aBuilder);
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
RenderFrameParent::ActorDestroy(ActorDestroyReason why)
{
  if (mFrameLoader && mFrameLoader->GetCurrentRemoteFrame() == this) {
    // XXX this might cause some weird issues ... we'll just not
    // redraw the part of the window covered by this until the "next"
    // remote frame has a layer-tree transaction.  For
    // why==NormalShutdown, we'll definitely want to do something
    // better, especially as nothing guarantees another Update() from
    // the "next" remote layer tree.
    mFrameLoader->SetCurrentRemoteFrame(nsnull);
  }
  mFrameLoader = nsnull;
}

PLayersParent*
RenderFrameParent::AllocPLayers(LayerManager::LayersBackend* aBackendType)
{
  if (!mFrameLoader || mFrameLoaderDestroyed) {
    *aBackendType = LayerManager::LAYERS_NONE;
    return nsnull;
  }
  LayerManager* lm = GetLayerManager();
  ShadowLayerManager* slm = lm->AsShadowManager();
  if (!slm) {
    *aBackendType = LayerManager::LAYERS_NONE;
     return nsnull;
  }
  *aBackendType = lm->GetBackendType();
  return new ShadowLayersParent(slm);
}

bool
RenderFrameParent::DeallocPLayers(PLayersParent* aLayers)
{
  delete aLayers;
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
    // pointer, we set mContentView to NULL.
    //
    // BuildViewMap will restore mFrameLoader if the content view is still
    // in our hash table.

    for (ViewMap::const_iterator iter = mContentViews.begin();
         iter != mContentViews.end();
         ++iter) {
      iter->second->mFrameLoader = NULL;
    }

    mozilla::layout::BuildViewMap(mContentViews, newContentViews, mFrameLoader, GetRootLayer());
  }

  // Here, we guarantee that *only* the root view is preserved in
  // case we couldn't build a new view map above. This is important because
  // the content view map should only contain the root view and content
  // views that are present in the layer tree.
  if (newContentViews.empty()) {
    newContentViews[FrameMetrics::ROOT_SCROLL_ID] =
      FindViewForId(mContentViews, FrameMetrics::ROOT_SCROLL_ID);
  }

  mContentViews = newContentViews;
}

LayerManager*
RenderFrameParent::GetLayerManager() const
{
  nsIDocument* doc = mFrameLoader->GetOwnerDoc();
  return doc->GetShell()->GetLayerManager();
}

ShadowLayersParent*
RenderFrameParent::GetShadowLayers() const
{
  const nsTArray<PLayersParent*>& shadowParents = ManagedPLayersParent();
  NS_ABORT_IF_FALSE(shadowParents.Length() <= 1,
                    "can only support at most 1 ShadowLayersParent");
  return (shadowParents.Length() == 1) ?
    static_cast<ShadowLayersParent*>(shadowParents[0]) : nsnull;
}

ContainerLayer*
RenderFrameParent::GetRootLayer() const
{
  ShadowLayersParent* shadowLayers = GetShadowLayers();
  return shadowLayers ? shadowLayers->GetRoot() : nsnull;
}

NS_IMETHODIMP
RenderFrameParent::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                    nsSubDocumentFrame* aFrame,
                                    const nsRect& aDirtyRect,
                                    const nsDisplayListSet& aLists)
{
  // We're the subdoc for <browser remote="true"> and it has
  // painted content.  Display its shadow layer tree.
  nsDisplayList shadowTree;
  ContainerLayer* container = GetRootLayer();
  if (aBuilder->IsForEventDelivery() && container) {
    nsRect bounds = aFrame->EnsureInnerView()->GetBounds();
    ViewTransform offset =
      ViewTransform(GetRootFrameOffset(aFrame, aBuilder), 1, 1);
    BuildListForLayer(container, mFrameLoader, offset,
                      aBuilder, shadowTree, aFrame);
  } else {
    shadowTree.AppendToTop(
      new (aBuilder) nsDisplayRemote(aBuilder, aFrame, this));
  }

  // Clip the shadow layers to subdoc bounds
  nsPoint offset = aFrame->GetOffsetToCrossDoc(aBuilder->ReferenceFrame());
  nsRect bounds = aFrame->EnsureInnerView()->GetBounds() + offset;

  return aLists.Content()->AppendNewToTop(
    new (aBuilder) nsDisplayClip(aBuilder, aFrame, &shadowTree,
                                 bounds));
}

}  // namespace layout
}  // namespace mozilla

already_AddRefed<Layer>
nsDisplayRemote::BuildLayer(nsDisplayListBuilder* aBuilder,
                            LayerManager* aManager,
                            const ContainerParameters& aContainerParameters)
{
  PRInt32 appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsIntRect visibleRect = GetVisibleRect().ToNearestPixels(appUnitsPerDevPixel);
  nsRefPtr<Layer> layer = mRemoteFrame->BuildLayer(aBuilder, mFrame, aManager, visibleRect);
  return layer.forget();
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
