/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BasicLayers.h"
#include "gfxPrefs.h"
#include "mozilla/BrowserElementParent.h"
#include "mozilla/EventForwards.h"  // for Modifiers
#include "mozilla/ViewportFrame.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsIObserver.h"
#include "nsStyleStructInlines.h"
#include "nsSubDocumentFrame.h"
#include "nsView.h"
#include "RenderFrameParent.h"
#include "mozilla/gfx/GPUProcessManager.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/WebRenderLayerManager.h"
#include "mozilla/layers/WebRenderScrollData.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "ClientLayerManager.h"
#include "FrameLayerBuilder.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::layers;

namespace mozilla {
namespace layout {

typedef FrameMetrics::ViewID ViewID;

/**
 * Gets the layer-pixel offset of aContainerFrame's content rect top-left
 * from the nearest display item reference frame (which we assume will be inducing
 * a ContainerLayer).
 */
static LayoutDeviceIntPoint
GetContentRectLayerOffset(nsIFrame* aContainerFrame, nsDisplayListBuilder* aBuilder)
{
  nscoord auPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();

  // Offset to the content rect in case we have borders or padding
  // Note that aContainerFrame could be a reference frame itself, so
  // we need to be careful here to ensure that we call ToReferenceFrame
  // on aContainerFrame and not its parent.
  nsPoint frameOffset = aBuilder->ToReferenceFrame(aContainerFrame) +
    aContainerFrame->GetContentRectRelativeToSelf().TopLeft();

  return LayoutDeviceIntPoint::FromAppUnitsToNearest(frameOffset, auPerDevPixel);
}

// Return true iff |aManager| is a "temporary layer manager".  They're
// used for small software rendering tasks, like drawWindow.  That's
// currently implemented by a BasicLayerManager without a backing
// widget, and hence in non-retained mode.
inline static bool
IsTempLayerManager(LayerManager* aManager)
{
  return (mozilla::layers::LayersBackend::LAYERS_BASIC == aManager->GetBackendType() &&
          !static_cast<BasicLayerManager*>(aManager)->IsRetained());
}

already_AddRefed<LayerManager>
GetLayerManager(nsFrameLoader* aFrameLoader)
{
  if (nsIContent* content = aFrameLoader->GetOwnerContent()) {
    RefPtr<LayerManager> lm = nsContentUtils::LayerManagerForContent(content);
    if (lm) {
      return lm.forget();
    }
  }

  nsIDocument* doc = aFrameLoader->GetOwnerDoc();
  if (!doc) {
    return nullptr;
  }
  return nsContentUtils::LayerManagerForDocument(doc);
}

RenderFrameParent::RenderFrameParent(nsFrameLoader* aFrameLoader)
  : mLayersId(0)
  , mLayersConnected(false)
  , mFrameLoader(aFrameLoader)
  , mFrameLoaderDestroyed(false)
  , mAsyncPanZoomEnabled(false)
  , mInitted(false)
{
  mInitted = Init(aFrameLoader);
}

RenderFrameParent::~RenderFrameParent()
{}

bool
RenderFrameParent::Init(nsFrameLoader* aFrameLoader)
{
  if (mInitted || !aFrameLoader) {
    return false;
  }

  mFrameLoader = aFrameLoader;

  RefPtr<LayerManager> lm = GetLayerManager(mFrameLoader);

  mAsyncPanZoomEnabled = lm && lm->AsyncPanZoomEnabled();

  TabParent* browser = TabParent::GetFrom(mFrameLoader);
  if (XRE_IsParentProcess()) {
    PCompositorBridgeChild* compositor = nullptr;
    if (lm) {
      compositor = lm->GetCompositorBridgeChild();
    }

    // Our remote frame will push layers updates to the compositor,
    // and we'll keep an indirect reference to that tree.
    GPUProcessManager* gpm = GPUProcessManager::Get();
    mLayersConnected = gpm->AllocateAndConnectLayerTreeId(
      compositor,
      browser->Manager()->AsContentParent()->OtherPid(),
      &mLayersId,
      &mCompositorOptions);
  } else if (XRE_IsContentProcess()) {
    ContentChild::GetSingleton()->SendAllocateLayerTreeId(browser->Manager()->ChildID(), browser->GetTabId(), &mLayersId);
    mLayersConnected = CompositorBridgeChild::Get()->SendNotifyChildCreated(mLayersId, &mCompositorOptions);
  }

  mInitted = true;
  return true;
}

bool
RenderFrameParent::IsInitted()
{
  return mInitted;
}

void
RenderFrameParent::Destroy()
{
  mFrameLoaderDestroyed = true;
  mLayerManager = nullptr;
}

already_AddRefed<Layer>
RenderFrameParent::BuildLayer(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame,
                              LayerManager* aManager,
                              nsDisplayItem* aItem,
                              const ContainerLayerParameters& aContainerParameters)
{
  MOZ_ASSERT(aFrame,
             "makes no sense to have a shadow tree without a frame");
  MOZ_ASSERT(!mContainer ||
             IsTempLayerManager(aManager) ||
             mContainer->Manager() == aManager,
             "retaining manager changed out from under us ... HELP!");

  if (IsTempLayerManager(aManager) ||
      (mContainer && mContainer->Manager() != aManager)) {
    // This can happen if aManager is a "temporary" manager, or if the
    // widget's layer manager changed out from under us.  We need to
    // FIXME handle the former case somehow, probably with an API to
    // draw a manager's subtree.  The latter is bad bad bad, but the the
    // MOZ_ASSERT() above will flag it.  Returning nullptr here will just
    // cause the shadow subtree not to be rendered.
    if (!aContainerParameters.mForEventsAndPluginsOnly) {
      NS_WARNING("Remote iframe not rendered");
    }
    return nullptr;
  }

  if (!mLayersId) {
    return nullptr;
  }

  RefPtr<Layer> layer =
    (aManager->GetLayerBuilder()->GetLeafLayerFor(aBuilder, aItem));
  if (!layer) {
    layer = aManager->CreateRefLayer();
  }
  if (!layer) {
    // Probably a temporary layer manager that doesn't know how to
    // use ref layers.
    return nullptr;
  }
  static_cast<RefLayer*>(layer.get())->SetReferentId(mLayersId);
  LayoutDeviceIntPoint offset = GetContentRectLayerOffset(aFrame, aBuilder);
  // We can only have an offset if we're a child of an inactive
  // container, but our display item is LAYER_ACTIVE_FORCE which
  // forces all layers above to be active.
  MOZ_ASSERT(aContainerParameters.mOffset == nsIntPoint());
  gfx::Matrix4x4 m = gfx::Matrix4x4::Translation(offset.x, offset.y, 0.0);
  // Remote content can't be repainted by us, so we multiply down
  // the resolution that our container expects onto our container.
  m.PreScale(aContainerParameters.mXScale, aContainerParameters.mYScale, 1.0);
  layer->SetBaseTransform(m);

  return layer.forget();
}

LayerManager*
RenderFrameParent::AttachLayerManager()
{
  RefPtr<LayerManager> lm;
  if (mFrameLoader) {
    lm = GetLayerManager(mFrameLoader);
  }

  // Perhaps the document containing this frame currently has no presentation?
  if (lm && lm->GetCompositorBridgeChild() && lm != mLayerManager) {
    mLayersConnected = lm->GetCompositorBridgeChild()->SendAdoptChild(mLayersId);
    FrameLayerBuilder::InvalidateAllLayers(lm);
  }

  mLayerManager = lm.forget();
  return mLayerManager;
}

void
RenderFrameParent::OwnerContentChanged(nsIContent* aContent)
{
  MOZ_ASSERT(!mFrameLoader || mFrameLoader->GetOwnerContent() == aContent,
             "Don't build new map if owner is same!");

  Unused << AttachLayerManager();
}

void
RenderFrameParent::ActorDestroy(ActorDestroyReason why)
{
  if (mLayersId != 0) {
    if (XRE_IsParentProcess()) {
      GPUProcessManager::Get()->UnmapLayerTreeId(mLayersId, OtherPid());
    } else if (XRE_IsContentProcess()) {
      TabParent* browser = TabParent::GetFrom(mFrameLoader);
      ContentChild::GetSingleton()->SendDeallocateLayerTreeId(browser->Manager()->ChildID(), mLayersId);
    }
  }

  mFrameLoader = nullptr;
  mLayerManager = nullptr;
}

mozilla::ipc::IPCResult
RenderFrameParent::RecvNotifyCompositorTransaction()
{
  TriggerRepaint();
  return IPC_OK();
}

void
RenderFrameParent::TriggerRepaint()
{
  nsIFrame* docFrame = mFrameLoader->GetPrimaryFrameOfOwningContent();
  if (!docFrame) {
    // Bad, but nothing we can do about it (XXX/cjones: or is there?
    // maybe bug 589337?).  When the new frame is created, we'll
    // probably still be the current render frame and will get to draw
    // our content then.  Or, we're shutting down and this update goes
    // to /dev/null.
    return;
  }

  docFrame->InvalidateLayer(DisplayItemType::TYPE_REMOTE);
}

void
RenderFrameParent::BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                    nsSubDocumentFrame* aFrame,
                                    const nsDisplayListSet& aLists)
{
  // We're the subdoc for <browser remote="true"> and it has
  // painted content.  Display its shadow layer tree.
  DisplayListClipState::AutoSaveRestore clipState(aBuilder);

  nsPoint offset = aBuilder->ToReferenceFrame(aFrame);
  nsRect bounds = aFrame->EnsureInnerView()->GetBounds() + offset;
  clipState.ClipContentDescendants(bounds);

  aLists.Content()->AppendToTop(
    new (aBuilder) nsDisplayRemote(aBuilder, aFrame, this));
}

void
RenderFrameParent::GetTextureFactoryIdentifier(TextureFactoryIdentifier* aTextureFactoryIdentifier)
{
  RefPtr<LayerManager> lm = mFrameLoader ? GetLayerManager(mFrameLoader) : nullptr;
  // Perhaps the document containing this frame currently has no presentation?
  if (lm) {
    *aTextureFactoryIdentifier = lm->GetTextureFactoryIdentifier();
  } else {
    *aTextureFactoryIdentifier = TextureFactoryIdentifier();
  }
}

void
RenderFrameParent::TakeFocusForClickFromTap()
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return;
  }
  nsCOMPtr<nsIContent> owner = mFrameLoader->GetOwnerContent();
  if (!owner) {
    return;
  }
  nsCOMPtr<nsIDOMElement> element = do_QueryInterface(owner);
  if (!element) {
    return;
  }
  fm->SetFocus(element, nsIFocusManager::FLAG_BYMOUSE |
                        nsIFocusManager::FLAG_BYTOUCH |
                        nsIFocusManager::FLAG_NOSCROLL);
}

void
RenderFrameParent::EnsureLayersConnected(CompositorOptions* aCompositorOptions)
{
  RefPtr<LayerManager> lm = GetLayerManager(mFrameLoader);
  if (!lm) {
    return;
  }

  if (!lm->GetCompositorBridgeChild()) {
    return;
  }

  mLayersConnected = lm->GetCompositorBridgeChild()->SendNotifyChildRecreated(mLayersId, &mCompositorOptions);
  *aCompositorOptions = mCompositorOptions;
}

} // namespace layout
} // namespace mozilla

nsDisplayRemote::nsDisplayRemote(nsDisplayListBuilder* aBuilder,
                                 nsSubDocumentFrame* aFrame,
                                 RenderFrameParent* aRemoteFrame)
  : nsDisplayItem(aBuilder, aFrame)
  , mRemoteFrame(aRemoteFrame)
  , mEventRegionsOverride(EventRegionsOverride::NoOverride)
{
  if (aBuilder->IsBuildingLayerEventRegions()) {
    bool frameIsPointerEventsNone =
      aFrame->StyleUserInterface()->GetEffectivePointerEvents(aFrame) ==
        NS_STYLE_POINTER_EVENTS_NONE;
    if (aBuilder->IsInsidePointerEventsNoneDoc() || frameIsPointerEventsNone) {
      mEventRegionsOverride |= EventRegionsOverride::ForceEmptyHitRegion;
    }
    if (nsLayoutUtils::HasDocumentLevelListenersForApzAwareEvents(aFrame->PresContext()->PresShell())) {
      mEventRegionsOverride |= EventRegionsOverride::ForceDispatchToContent;
    }
  }
}

already_AddRefed<Layer>
nsDisplayRemote::BuildLayer(nsDisplayListBuilder* aBuilder,
                            LayerManager* aManager,
                            const ContainerLayerParameters& aContainerParameters)
{
  RefPtr<Layer> layer = mRemoteFrame->BuildLayer(aBuilder, mFrame, aManager, this, aContainerParameters);
  if (layer && layer->AsContainerLayer()) {
    layer->AsContainerLayer()->SetEventRegionsOverride(mEventRegionsOverride);
  }
  return layer.forget();
}

bool
nsDisplayRemote::CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                         mozilla::wr::IpcResourceUpdateQueue& aResources,
                                         const StackingContextHelper& aSc,
                                         mozilla::layers::WebRenderLayerManager* aManager,
                                         nsDisplayListBuilder* aDisplayListBuilder)
{
  MOZ_ASSERT(aManager->IsLayersFreeTransaction());

  mOffset = mozilla::layout::GetContentRectLayerOffset(mFrame, aDisplayListBuilder);

  mozilla::LayoutDeviceRect visible = mozilla::LayoutDeviceRect::FromAppUnits(
      GetVisibleRect(), mFrame->PresContext()->AppUnitsPerDevPixel());
  visible += mOffset;

  aBuilder.PushIFrame(aSc.ToRelativeLayoutRect(visible),
      mozilla::wr::AsPipelineId(GetRemoteLayersId()));

  return true;
}

bool
nsDisplayRemote::UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                  mozilla::layers::WebRenderLayerScrollData* aLayerData)
{
  if (aLayerData) {
    aLayerData->SetReferentId(GetRemoteLayersId());
    aLayerData->SetTransform(mozilla::gfx::Matrix4x4::Translation(mOffset.x, mOffset.y, 0.0));
    aLayerData->SetEventRegionsOverride(mEventRegionsOverride);
  }
  return true;
}

uint64_t
nsDisplayRemote::GetRemoteLayersId() const
{
  return mRemoteFrame->GetLayersId();
}
