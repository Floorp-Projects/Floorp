/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "mozilla/PresShell.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "nsFrameLoader.h"
#include "nsStyleStructInlines.h"
#include "nsSubDocumentFrame.h"
#include "RenderFrame.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/webrender/WebRenderAPI.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {
namespace layout {

static already_AddRefed<LayerManager> GetLayerManager(
    BrowserParent* aBrowserParent) {
  if (Element* element = aBrowserParent->GetOwnerElement()) {
    if (RefPtr<LayerManager> lm =
            nsContentUtils::LayerManagerForContent(element)) {
      return lm.forget();
    }
    return nsContentUtils::LayerManagerForDocument(element->OwnerDoc());
  }
  return nullptr;
}

RenderFrame::RenderFrame()
    : mLayersId{0},
      mBrowserParent(nullptr),
      mLayerManager(nullptr),
      mInitialized(false),
      mLayersConnected(false) {}

RenderFrame::~RenderFrame() {}

bool RenderFrame::Initialize(BrowserParent* aBrowserParent) {
  if (mInitialized || !aBrowserParent) {
    return false;
  }

  mBrowserParent = aBrowserParent;
  RefPtr<LayerManager> lm = GetLayerManager(mBrowserParent);
  PCompositorBridgeChild* compositor =
      lm ? lm->GetCompositorBridgeChild() : nullptr;
  mTabProcessId = mBrowserParent->Manager()->OtherPid();

  // Our remote frame will push layers updates to the compositor,
  // and we'll keep an indirect reference to that tree.
  GPUProcessManager* gpm = GPUProcessManager::Get();
  mLayersConnected = gpm->AllocateAndConnectLayerTreeId(
      compositor, mTabProcessId, &mLayersId, &mCompositorOptions);

  mInitialized = true;
  return true;
}

void RenderFrame::Destroy() {
  if (mLayersId.IsValid()) {
    GPUProcessManager::Get()->UnmapLayerTreeId(mLayersId, mTabProcessId);
  }

  mBrowserParent = nullptr;
  mLayerManager = nullptr;
}

void RenderFrame::EnsureLayersConnected(CompositorOptions* aCompositorOptions) {
  RefPtr<LayerManager> lm = GetLayerManager(mBrowserParent);
  if (!lm) {
    return;
  }

  if (!lm->GetCompositorBridgeChild()) {
    return;
  }

  mLayersConnected = lm->GetCompositorBridgeChild()->SendNotifyChildRecreated(
      mLayersId, &mCompositorOptions);
  *aCompositorOptions = mCompositorOptions;
}

LayerManager* RenderFrame::AttachLayerManager() {
  RefPtr<LayerManager> lm;
  if (mBrowserParent) {
    lm = GetLayerManager(mBrowserParent);
  }

  // Perhaps the document containing this frame currently has no presentation?
  if (lm && lm->GetCompositorBridgeChild() && lm != mLayerManager) {
    mLayersConnected =
        lm->GetCompositorBridgeChild()->SendAdoptChild(mLayersId);
    FrameLayerBuilder::InvalidateAllLayers(lm);
  }

  mLayerManager = lm.forget();
  return mLayerManager;
}

void RenderFrame::OwnerContentChanged() { Unused << AttachLayerManager(); }

void RenderFrame::GetTextureFactoryIdentifier(
    TextureFactoryIdentifier* aTextureFactoryIdentifier) const {
  RefPtr<LayerManager> lm =
      mBrowserParent ? GetLayerManager(mBrowserParent) : nullptr;
  // Perhaps the document containing this frame currently has no presentation?
  if (lm) {
    *aTextureFactoryIdentifier = lm->GetTextureFactoryIdentifier();
  } else {
    *aTextureFactoryIdentifier = TextureFactoryIdentifier();
  }
}

}  // namespace layout
}  // namespace mozilla

/**
 * Gets the layer-pixel offset of aContainerFrame's content rect top-left
 * from the nearest display item reference frame (which we assume will be
 * inducing a ContainerLayer).
 */
static mozilla::LayoutDeviceIntPoint GetContentRectLayerOffset(
    nsIFrame* aContainerFrame, nsDisplayListBuilder* aBuilder) {
  nscoord auPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();

  // Offset to the content rect in case we have borders or padding
  // Note that aContainerFrame could be a reference frame itself, so
  // we need to be careful here to ensure that we call ToReferenceFrame
  // on aContainerFrame and not its parent.
  nsPoint frameOffset =
      aBuilder->ToReferenceFrame(aContainerFrame) +
      aContainerFrame->GetContentRectRelativeToSelf().TopLeft();

  return mozilla::LayoutDeviceIntPoint::FromAppUnitsToNearest(frameOffset,
                                                              auPerDevPixel);
}

// Return true iff |aManager| is a "temporary layer manager".  They're
// used for small software rendering tasks, like drawWindow.  That's
// currently implemented by a BasicLayerManager without a backing
// widget, and hence in non-retained mode.
inline static bool IsTempLayerManager(mozilla::layers::LayerManager* aManager) {
  return (mozilla::layers::LayersBackend::LAYERS_BASIC ==
              aManager->GetBackendType() &&
          !static_cast<BasicLayerManager*>(aManager)->IsRetained());
}

nsDisplayRemote::nsDisplayRemote(nsDisplayListBuilder* aBuilder,
                                 nsSubDocumentFrame* aFrame)
    : nsPaintedDisplayItem(aBuilder, aFrame),
      mTabId{0},
      mEventRegionsOverride(EventRegionsOverride::NoOverride) {
  bool frameIsPointerEventsNone = aFrame->StyleUI()->GetEffectivePointerEvents(
                                      aFrame) == NS_STYLE_POINTER_EVENTS_NONE;
  if (aBuilder->IsInsidePointerEventsNoneDoc() || frameIsPointerEventsNone) {
    mEventRegionsOverride |= EventRegionsOverride::ForceEmptyHitRegion;
  }
  if (nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(
          aFrame->PresShell())) {
    mEventRegionsOverride |= EventRegionsOverride::ForceDispatchToContent;
  }

  nsFrameLoader* frameLoader = GetFrameLoader();
  MOZ_ASSERT(frameLoader && frameLoader->IsRemoteFrame());
  mLayersId = frameLoader->GetLayersId();

  if (nsFrameLoader* frameLoader = GetFrameLoader()) {
    // TODO: We need to handle acquiring a TabId in the remote sub-frame case
    // for fission.
    if (BrowserParent* browser = BrowserParent::GetFrom(frameLoader)) {
      mTabId = browser->GetTabId();
    }
  }
}

mozilla::LayerState nsDisplayRemote::GetLayerState(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aParameters) {
  if (IsTempLayerManager(aManager)) {
    return mozilla::LayerState::LAYER_NONE;
  }
  return mozilla::LayerState::LAYER_ACTIVE_FORCE;
}

already_AddRefed<Layer> nsDisplayRemote::BuildLayer(
    nsDisplayListBuilder* aBuilder, LayerManager* aManager,
    const ContainerLayerParameters& aContainerParameters) {
  MOZ_ASSERT(mFrame, "Makes no sense to have a shadow tree without a frame");

  if (IsTempLayerManager(aManager)) {
    // This can happen if aManager is a "temporary" manager, or if the
    // widget's layer manager changed out from under us.  We need to
    // FIXME handle the former case somehow, probably with an API to
    // draw a manager's subtree.  The latter is bad bad bad, but the the
    // MOZ_ASSERT() above will flag it.  Returning nullptr here will just
    // cause the shadow subtree not to be rendered.
    NS_WARNING("Remote iframe not rendered");
    return nullptr;
  }

  if (!mLayersId.IsValid()) {
    return nullptr;
  }

  RefPtr<Layer> layer =
      aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, this);

  if (!layer) {
    layer = aManager->CreateRefLayer();
  }

  if (!layer) {
    // Probably a temporary layer manager that doesn't know how to
    // use ref layers.
    return nullptr;
  }

  static_cast<RefLayer*>(layer.get())->SetReferentId(mLayersId);
  LayoutDeviceIntPoint offset = GetContentRectLayerOffset(Frame(), aBuilder);
  // We can only have an offset if we're a child of an inactive
  // container, but our display item is LAYER_ACTIVE_FORCE which
  // forces all layers above to be active.
  MOZ_ASSERT(aContainerParameters.mOffset == nsIntPoint());
  Matrix4x4 m = Matrix4x4::Translation(offset.x, offset.y, 0.0);
  // Remote content can't be repainted by us, so we multiply down
  // the resolution that our container expects onto our container.
  m.PreScale(aContainerParameters.mXScale, aContainerParameters.mYScale, 1.0);
  layer->SetBaseTransform(m);

  if (layer->AsRefLayer()) {
    layer->AsRefLayer()->SetEventRegionsOverride(mEventRegionsOverride);
  }

  return layer.forget();
}

void nsDisplayRemote::Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {
  DrawTarget* target = aCtx->GetDrawTarget();
  if (!target->IsRecording() || mTabId == 0) {
    NS_WARNING("Remote iframe not rendered");
    return;
  }

  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  Rect destRect = mozilla::NSRectToSnappedRect(GetContentRect(),
                                               appUnitsPerDevPixel, *target);
  target->DrawDependentSurface(mTabId, destRect);
}

bool nsDisplayRemote::CreateWebRenderCommands(
    mozilla::wr::DisplayListBuilder& aBuilder,
    mozilla::wr::IpcResourceUpdateQueue& aResources,
    const StackingContextHelper& aSc,
    mozilla::layers::RenderRootStateManager* aManager,
    nsDisplayListBuilder* aDisplayListBuilder) {
  if (!mLayersId.IsValid()) {
    return true;
  }

  mOffset = GetContentRectLayerOffset(mFrame, aDisplayListBuilder);

  LayoutDeviceRect rect = LayoutDeviceRect::FromAppUnits(
      mFrame->GetContentRectRelativeToSelf(),
      mFrame->PresContext()->AppUnitsPerDevPixel());
  rect += mOffset;

  aBuilder.PushIFrame(mozilla::wr::ToRoundedLayoutRect(rect),
                      !BackfaceIsHidden(), mozilla::wr::AsPipelineId(mLayersId),
                      /*ignoreMissingPipelines*/ true);

  return true;
}

bool nsDisplayRemote::UpdateScrollData(
    mozilla::layers::WebRenderScrollData* aData,
    mozilla::layers::WebRenderLayerScrollData* aLayerData) {
  if (!mLayersId.IsValid()) {
    return true;
  }

  if (aLayerData) {
    aLayerData->SetReferentId(mLayersId);
    aLayerData->SetTransform(
        mozilla::gfx::Matrix4x4::Translation(mOffset.x, mOffset.y, 0.0));
    aLayerData->SetEventRegionsOverride(mEventRegionsOverride);
  }
  return true;
}

nsFrameLoader* nsDisplayRemote::GetFrameLoader() const {
  return mFrame ? static_cast<nsSubDocumentFrame*>(mFrame)->FrameLoader()
                : nullptr;
}
