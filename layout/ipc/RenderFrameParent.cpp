/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "BasicLayers.h"
#include "gfxPrefs.h"
#ifdef MOZ_ENABLE_D3D9_LAYER
# include "LayerManagerD3D9.h"
#endif //MOZ_ENABLE_D3D9_LAYER
#include "mozilla/BrowserElementParent.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/LayerTransactionParent.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsFrameLoader.h"
#include "nsIObserver.h"
#include "nsStyleStructInlines.h"
#include "nsSubDocumentFrame.h"
#include "nsView.h"
#include "nsViewportFrame.h"
#include "RenderFrameParent.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/CompositorChild.h"
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
static nsIntPoint
GetContentRectLayerOffset(nsIFrame* aContainerFrame, nsDisplayListBuilder* aBuilder)
{
  nscoord auPerDevPixel = aContainerFrame->PresContext()->AppUnitsPerDevPixel();

  // Offset to the content rect in case we have borders or padding
  // Note that aContainerFrame could be a reference frame itself, so
  // we need to be careful here to ensure that we call ToReferenceFrame
  // on aContainerFrame and not its parent.
  nsPoint frameOffset = aBuilder->ToReferenceFrame(aContainerFrame) +
    aContainerFrame->GetContentRectRelativeToSelf().TopLeft();

  return frameOffset.ToNearestPixels(auPerDevPixel);
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
GetFrom(nsFrameLoader* aFrameLoader)
{
  nsIDocument* doc = aFrameLoader->GetOwnerDoc();
  if (!doc) {
    return nullptr;
  }
  return nsContentUtils::LayerManagerForDocument(doc);
}

class RemoteContentController : public GeckoContentController {
public:
  explicit RemoteContentController(RenderFrameParent* aRenderFrame)
    : mUILoop(MessageLoop::current())
    , mRenderFrame(aRenderFrame)
    , mHaveZoomConstraints(false)
  { }

  virtual void RequestContentRepaint(const FrameMetrics& aFrameMetrics) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mRenderFrame) {
      TabParent* browser = TabParent::GetFrom(mRenderFrame->Manager());
      browser->UpdateFrame(aFrameMetrics);
    }
  }

  virtual void AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                       const uint32_t& aScrollGeneration) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      // We have to send this message from the "UI thread" (main
      // thread).
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::AcknowledgeScrollUpdate,
                          aScrollId, aScrollGeneration));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = TabParent::GetFrom(mRenderFrame->Manager());
      browser->AcknowledgeScrollUpdate(aScrollId, aScrollGeneration);
    }
  }

  virtual void HandleDoubleTap(const CSSPoint& aPoint,
                               int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      // We have to send this message from the "UI thread" (main
      // thread).
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::HandleDoubleTap,
                          aPoint, aModifiers, aGuid));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = TabParent::GetFrom(mRenderFrame->Manager());
      browser->HandleDoubleTap(aPoint, aModifiers, aGuid);
    }
  }

  virtual void HandleSingleTap(const CSSPoint& aPoint,
                               int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      // We have to send this message from the "UI thread" (main
      // thread).
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::HandleSingleTap,
                          aPoint, aModifiers, aGuid));
      return;
    }
    if (mRenderFrame) {
      mRenderFrame->TakeFocusForClick();
      TabParent* browser = TabParent::GetFrom(mRenderFrame->Manager());
      browser->HandleSingleTap(aPoint, aModifiers, aGuid);
    }
  }

  virtual void HandleLongTap(const CSSPoint& aPoint,
                             int32_t aModifiers,
                             const ScrollableLayerGuid& aGuid,
                             uint64_t aInputBlockId) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      // We have to send this message from the "UI thread" (main
      // thread).
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::HandleLongTap,
                          aPoint, aModifiers, aGuid, aInputBlockId));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = TabParent::GetFrom(mRenderFrame->Manager());
      browser->HandleLongTap(aPoint, aModifiers, aGuid, aInputBlockId);
    }
  }

  virtual void HandleLongTapUp(const CSSPoint& aPoint,
                               int32_t aModifiers,
                               const ScrollableLayerGuid& aGuid) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      // We have to send this message from the "UI thread" (main
      // thread).
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::HandleLongTapUp,
                          aPoint, aModifiers, aGuid));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = TabParent::GetFrom(mRenderFrame->Manager());
      browser->HandleLongTapUp(aPoint, aModifiers, aGuid);
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
      TabParent* browser = TabParent::GetFrom(mRenderFrame->Manager());
      BrowserElementParent::DispatchAsyncScrollEvent(browser, aContentRect,
                                                     aContentSize);
    }
  }

  virtual void PostDelayedTask(Task* aTask, int aDelayMs) MOZ_OVERRIDE
  {
    MessageLoop::current()->PostDelayedTask(FROM_HERE, aTask, aDelayMs);
  }

  virtual bool GetRootZoomConstraints(ZoomConstraints* aOutConstraints) MOZ_OVERRIDE
  {
    if (mHaveZoomConstraints && aOutConstraints) {
      *aOutConstraints = mZoomConstraints;
    }
    return mHaveZoomConstraints;
  }

  virtual bool GetTouchSensitiveRegion(CSSRect* aOutRegion) MOZ_OVERRIDE
  {
    if (mTouchSensitiveRegion.IsEmpty())
      return false;

    *aOutRegion = CSSRect::FromAppUnits(mTouchSensitiveRegion.GetBounds());
    return true;
  }

  virtual void NotifyAPZStateChange(const ScrollableLayerGuid& aGuid,
                                    APZStateChange aChange,
                                    int aArg) MOZ_OVERRIDE
  {
    if (MessageLoop::current() != mUILoop) {
      mUILoop->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &RemoteContentController::NotifyAPZStateChange,
                          aGuid, aChange, aArg));
      return;
    }
    if (mRenderFrame) {
      TabParent* browser = TabParent::GetFrom(mRenderFrame->Manager());
      browser->NotifyAPZStateChange(aGuid.mScrollId, aChange, aArg);
    }
  }

  // Methods used by RenderFrameParent to set fields stored here.

  void SaveZoomConstraints(const ZoomConstraints& aConstraints)
  {
    mHaveZoomConstraints = true;
    mZoomConstraints = aConstraints;
  }

  void SetTouchSensitiveRegion(const nsRegion& aRegion)
  {
    mTouchSensitiveRegion = aRegion;
  }
private:
  MessageLoop* mUILoop;
  RenderFrameParent* mRenderFrame;

  bool mHaveZoomConstraints;
  ZoomConstraints mZoomConstraints;
  nsRegion mTouchSensitiveRegion;
};

RenderFrameParent::RenderFrameParent(nsFrameLoader* aFrameLoader,
                                     ScrollingBehavior aScrollingBehavior,
                                     TextureFactoryIdentifier* aTextureFactoryIdentifier,
                                     uint64_t* aId,
                                     bool* aSuccess)
  : mLayersId(0)
  , mFrameLoader(aFrameLoader)
  , mFrameLoaderDestroyed(false)
  , mBackgroundColor(gfxRGBA(1, 1, 1))
{
  *aSuccess = false;
  if (!mFrameLoader) {
    return;
  }

  *aId = 0;

  nsRefPtr<LayerManager> lm = GetFrom(mFrameLoader);
  // Perhaps the document containing this frame currently has no presentation?
  if (lm && lm->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
    *aTextureFactoryIdentifier =
      static_cast<ClientLayerManager*>(lm.get())->GetTextureFactoryIdentifier();
  } else {
    *aTextureFactoryIdentifier = TextureFactoryIdentifier();
  }

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // Our remote frame will push layers updates to the compositor,
    // and we'll keep an indirect reference to that tree.
    *aId = mLayersId = CompositorParent::AllocateLayerTreeId();
    if (lm && lm->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
      ClientLayerManager *clientManager =
        static_cast<ClientLayerManager*>(lm.get());
      clientManager->GetRemoteRenderer()->SendNotifyChildCreated(mLayersId);
    }
    if (aScrollingBehavior == ASYNC_PAN_ZOOM) {
      mContentController = new RemoteContentController(this);
      CompositorParent::SetControllerForLayerTree(mLayersId, mContentController);
    }
  } else if (XRE_GetProcessType() == GeckoProcessType_Content) {
    ContentChild::GetSingleton()->SendAllocateLayerTreeId(aId);
    mLayersId = *aId;
    CompositorChild::Get()->SendNotifyChildCreated(mLayersId);
  }
  // Set a default RenderFrameParent
  mFrameLoader->SetCurrentRemoteFrame(this);
  *aSuccess = true;
}

APZCTreeManager*
RenderFrameParent::GetApzcTreeManager()
{
  // We can't get a ref to the APZCTreeManager until after the child is
  // created and the static getter knows which CompositorParent is
  // instantiated with this layers ID. That's why try to fetch it when
  // we first need it and cache the result.
  if (!mApzcTreeManager && gfxPrefs::AsyncPanZoomEnabled()) {
    mApzcTreeManager = CompositorParent::GetAPZCTreeManager(mLayersId);
  }
  return mApzcTreeManager.get();
}

RenderFrameParent::~RenderFrameParent()
{}

void
RenderFrameParent::Destroy()
{
  mFrameLoaderDestroyed = true;
}

already_AddRefed<Layer>
RenderFrameParent::BuildLayer(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame,
                              LayerManager* aManager,
                              const nsIntRect& aVisibleRect,
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
    NS_WARNING("Remote iframe not rendered");
    return nullptr;
  }

  uint64_t id = GetLayerTreeId();
  if (!id) {
    return nullptr;
  }

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

void
RenderFrameParent::OwnerContentChanged(nsIContent* aContent)
{
  MOZ_ASSERT(mFrameLoader->GetOwnerContent() == aContent,
             "Don't build new map if owner is same!");

  nsRefPtr<LayerManager> lm = GetFrom(mFrameLoader);
  // Perhaps the document containing this frame currently has no presentation?
  if (lm && lm->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
    ClientLayerManager *clientManager =
      static_cast<ClientLayerManager*>(lm.get());
    clientManager->GetRemoteRenderer()->SendAdoptChild(mLayersId);
  }
}

void
RenderFrameParent::ActorDestroy(ActorDestroyReason why)
{
  if (mLayersId != 0) {
    if (XRE_GetProcessType() == GeckoProcessType_Content) {
      ContentChild::GetSingleton()->SendDeallocateLayerTreeId(mLayersId);
    } else {
      CompositorParent::DeallocateLayerTreeId(mLayersId);
    }
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
  if (mContentController) {
    // Tell the content controller about the touch-sensitive region, so
    // that it can provide it to APZ. This is required for APZ to do
    // correct hit testing for a remote 'mozpasspointerevents' iframe
    // until bug 928833 is fixed.
    mContentController->SetTouchSensitiveRegion(aRegion);
  }
  return true;
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

uint64_t
RenderFrameParent::GetLayerTreeId() const
{
  return mLayersId;
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

  aLists.Content()->AppendToTop(
    new (aBuilder) nsDisplayRemote(aBuilder, aFrame, this));
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
RenderFrameParent::ContentReceivedInputBlock(const ScrollableLayerGuid& aGuid,
                                             uint64_t aInputBlockId,
                                             bool aPreventDefault)
{
  if (aGuid.mLayersId != mLayersId) {
    // Guard against bad data from hijacked child processes
    NS_ERROR("Unexpected layers id in ContentReceivedInputBlock; dropping message...");
    return;
  }
  if (GetApzcTreeManager()) {
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod(
        GetApzcTreeManager(), &APZCTreeManager::ContentReceivedInputBlock,
        aInputBlockId, aPreventDefault));
  }
}

void
RenderFrameParent::SetTargetAPZC(uint64_t aInputBlockId,
                                 const nsTArray<ScrollableLayerGuid>& aTargets)
{
  for (size_t i = 0; i < aTargets.Length(); i++) {
    if (aTargets[i].mLayersId != mLayersId) {
      // Guard against bad data from hijacked child processes
      NS_ERROR("Unexpected layers id in SetTargetAPZC; dropping message...");
      return;
    }
  }
  if (GetApzcTreeManager()) {
    // need a local var to disambiguate between the SetTargetAPZC overloads.
    void (APZCTreeManager::*setTargetApzcFunc)(uint64_t, const nsTArray<ScrollableLayerGuid>&)
        = &APZCTreeManager::SetTargetAPZC;
    APZThreadUtils::RunOnControllerThread(NewRunnableMethod(
        GetApzcTreeManager(), setTargetApzcFunc,
        aInputBlockId, aTargets));
  }
}

void
RenderFrameParent::UpdateZoomConstraints(uint32_t aPresShellId,
                                         ViewID aViewId,
                                         bool aIsRoot,
                                         const ZoomConstraints& aConstraints)
{
  if (mContentController && aIsRoot) {
    mContentController->SaveZoomConstraints(aConstraints);
  }
  if (GetApzcTreeManager()) {
    GetApzcTreeManager()->UpdateZoomConstraints(ScrollableLayerGuid(mLayersId, aPresShellId, aViewId),
                                                aConstraints);
  }
}

bool
RenderFrameParent::HitTest(const nsRect& aRect)
{
  return mTouchRegion.Contains(aRect);
}

void
RenderFrameParent::GetTextureFactoryIdentifier(TextureFactoryIdentifier* aTextureFactoryIdentifier)
{
  nsRefPtr<LayerManager> lm = GetFrom(mFrameLoader);
  // Perhaps the document containing this frame currently has no presentation?
  if (lm && lm->GetBackendType() == LayersBackend::LAYERS_CLIENT) {
    *aTextureFactoryIdentifier =
      static_cast<ClientLayerManager*>(lm.get())->GetTextureFactoryIdentifier();
  } else {
    *aTextureFactoryIdentifier = TextureFactoryIdentifier();
  }
}

void
RenderFrameParent::TakeFocusForClick()
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
                        nsIFocusManager::FLAG_NOSCROLL);
}

}  // namespace layout
}  // namespace mozilla

nsDisplayRemote::nsDisplayRemote(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame,
                                 RenderFrameParent* aRemoteFrame)
  : nsDisplayItem(aBuilder, aFrame)
  , mRemoteFrame(aRemoteFrame)
  , mEventRegionsOverride(EventRegionsOverride::NoOverride)
{
  if (aBuilder->IsBuildingLayerEventRegions()) {
    if (aBuilder->IsInsidePointerEventsNoneDoc() ||
        aFrame->StyleVisibility()->GetEffectivePointerEvents(aFrame) == NS_STYLE_POINTER_EVENTS_NONE) {
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
  int32_t appUnitsPerDevPixel = mFrame->PresContext()->AppUnitsPerDevPixel();
  nsIntRect visibleRect = GetVisibleRect().ToNearestPixels(appUnitsPerDevPixel);
  visibleRect += aContainerParameters.mOffset;
  nsRefPtr<Layer> layer = mRemoteFrame->BuildLayer(aBuilder, mFrame, aManager, visibleRect, this, aContainerParameters);
  if (layer && layer->AsContainerLayer()) {
    layer->AsContainerLayer()->SetEventRegionsOverride(mEventRegionsOverride);
  }
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

