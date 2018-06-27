/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerTransactionParent.h"
#include <vector>                       // for vector
#include "CompositableHost.h"           // for CompositableParent, Get, etc
#include "ImageLayers.h"                // for ImageLayer
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "CompositableTransactionParent.h"  // for EditReplyVector
#include "CompositorBridgeParent.h"
#include "gfxPrefs.h"
#include "mozilla/gfx/BasePoint3D.h"    // for BasePoint3D
#include "mozilla/layers/AnimationHelper.h" // for GetAnimatedPropValue
#include "mozilla/layers/CanvasLayerComposite.h"
#include "mozilla/layers/ColorLayerComposite.h"
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/ContainerLayerComposite.h"
#include "mozilla/layers/ImageBridgeParent.h" // for ImageBridgeParent
#include "mozilla/layers/ImageLayerComposite.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply, etc
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/PaintedLayerComposite.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "mozilla/Unused.h"
#include "nsCoord.h"                    // for NSAppUnitsToFloatPixels
#include "nsISupportsImpl.h"            // for Layer::Release, etc
#include "nsLayoutUtils.h"              // for nsLayoutUtils
#include "nsMathUtils.h"                // for NS_round
#include "nsPoint.h"                    // for nsPoint
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "TreeTraversal.h"              // for ForEachNode
#include "GeckoProfiler.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/AsyncCompositionManager.h"

using mozilla::layout::RenderFrameParent;

namespace mozilla {
namespace layers {

//--------------------------------------------------
// LayerTransactionParent
LayerTransactionParent::LayerTransactionParent(HostLayerManager* aManager,
                                               CompositorBridgeParentBase* aBridge,
                                               CompositorAnimationStorage* aAnimStorage,
                                               LayersId aId)
  : mLayerManager(aManager)
  , mCompositorBridge(aBridge)
  , mAnimStorage(aAnimStorage)
  , mId(aId)
  , mChildEpoch(0)
  , mParentEpoch(0)
  , mPendingTransaction{0}
  , mDestroyed(false)
  , mIPCOpen(false)
{
  MOZ_ASSERT(mId.IsValid());
}

LayerTransactionParent::~LayerTransactionParent()
{
}

void
LayerTransactionParent::SetLayerManager(HostLayerManager* aLayerManager, CompositorAnimationStorage* aAnimStorage)
{
  if (mDestroyed) {
    return;
  }
  mLayerManager = aLayerManager;
  for (auto iter = mLayerMap.Iter(); !iter.Done(); iter.Next()) {
    auto layer = iter.Data();
    if (mAnimStorage &&
        layer->GetCompositorAnimationsId()) {
      mAnimStorage->ClearById(layer->GetCompositorAnimationsId());
    }
    layer->AsHostLayer()->SetLayerManager(aLayerManager);
  }
  mAnimStorage = aAnimStorage;
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvShutdown()
{
  Destroy();
  IProtocol* mgr = Manager();
  if (!Send__delete__(this)) {
    return IPC_FAIL_NO_REASON(mgr);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvShutdownSync()
{
  return RecvShutdown();
}

void
LayerTransactionParent::Destroy()
{
  if (mDestroyed) {
    return;
  }
  mDestroyed = true;
  if (mAnimStorage) {
    for (auto iter = mLayerMap.Iter(); !iter.Done(); iter.Next()) {
      auto layer = iter.Data();
      if (layer->GetCompositorAnimationsId()) {
        mAnimStorage->ClearById(layer->GetCompositorAnimationsId());
      }
      layer->Disconnect();
    }
  }
  mCompositables.clear();
  mAnimStorage = nullptr;
}

class MOZ_STACK_CLASS AutoLayerTransactionParentAsyncMessageSender
{
public:
  explicit AutoLayerTransactionParentAsyncMessageSender(LayerTransactionParent* aLayerTransaction,
                                                        const InfallibleTArray<OpDestroy>* aDestroyActors = nullptr)
    : mLayerTransaction(aLayerTransaction)
    , mActorsToDestroy(aDestroyActors)
  {
    mLayerTransaction->SetAboutToSendAsyncMessages();
  }

  ~AutoLayerTransactionParentAsyncMessageSender()
  {
    mLayerTransaction->SendPendingAsyncMessages();
    if (mActorsToDestroy) {
      // Destroy the actors after sending the async messages because the latter may contain
      // references to some actors.
      for (const auto& op : *mActorsToDestroy) {
        mLayerTransaction->DestroyActor(op);
      }
    }
  }
private:
  LayerTransactionParent* mLayerTransaction;
  const InfallibleTArray<OpDestroy>* mActorsToDestroy;
};

mozilla::ipc::IPCResult
LayerTransactionParent::RecvPaintTime(const TransactionId& aTransactionId,
                                      const TimeDuration& aPaintTime)
{
  mCompositorBridge->UpdatePaintTime(this, aPaintTime);
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvUpdate(const TransactionInfo& aInfo)
{
  AUTO_PROFILER_TRACING("Paint", "LayerTransaction");
  AUTO_PROFILER_LABEL("LayerTransactionParent::RecvUpdate", GRAPHICS);

  TimeStamp updateStart = TimeStamp::Now();

  MOZ_LAYERS_LOG(("[ParentSide] received txn with %zu edits", aInfo.cset().Length()));

  UpdateFwdTransactionId(aInfo.fwdTransactionId());

  if (mDestroyed || !mLayerManager || mLayerManager->IsDestroyed()) {
    for (const auto& op : aInfo.toDestroy()) {
      DestroyActor(op);
    }
    return IPC_OK();
  }

  // This ensures that destroy operations are always processed. It is not safe
  // to early-return from RecvUpdate without doing so.
  AutoLayerTransactionParentAsyncMessageSender autoAsyncMessageSender(this, &aInfo.toDestroy());

  {
    AutoResolveRefLayers resolve(mCompositorBridge->GetCompositionManager(this));
    mLayerManager->BeginTransaction();
  }

  // Not all edits require an update to the hit testing tree.
  mUpdateHitTestingTree = false;

  for (EditArray::index_type i = 0; i < aInfo.cset().Length(); ++i) {
    const Edit& edit = const_cast<Edit&>(aInfo.cset()[i]);

    switch (edit.type()) {
    // Create* ops
    case Edit::TOpCreatePaintedLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreatePaintedLayer"));

      RefPtr<PaintedLayer> layer = mLayerManager->CreatePaintedLayer();
      if (!BindLayer(layer, edit.get_OpCreatePaintedLayer())) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "CreatePaintedLayer");
      break;
    }
    case Edit::TOpCreateContainerLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateContainerLayer"));

      RefPtr<ContainerLayer> layer = mLayerManager->CreateContainerLayer();
      if (!BindLayer(layer, edit.get_OpCreateContainerLayer())) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "CreateContainerLayer");
      break;
    }
    case Edit::TOpCreateImageLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateImageLayer"));

      RefPtr<ImageLayer> layer = mLayerManager->CreateImageLayer();
      if (!BindLayer(layer, edit.get_OpCreateImageLayer())) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "CreateImageLayer");
      break;
    }
    case Edit::TOpCreateColorLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateColorLayer"));

      RefPtr<ColorLayer> layer = mLayerManager->CreateColorLayer();
      if (!BindLayer(layer, edit.get_OpCreateColorLayer())) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "CreateColorLayer");
      break;
    }
    case Edit::TOpCreateBorderLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateBorderLayer"));

      RefPtr<BorderLayer> layer = mLayerManager->CreateBorderLayer();
      if (!BindLayer(layer, edit.get_OpCreateBorderLayer())) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "CreateBorderLayer");
      break;
    }
    case Edit::TOpCreateCanvasLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateCanvasLayer"));

      RefPtr<CanvasLayer> layer = mLayerManager->CreateCanvasLayer();
      if (!BindLayer(layer, edit.get_OpCreateCanvasLayer())) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "CreateCanvasLayer");
      break;
    }
    case Edit::TOpCreateRefLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateRefLayer"));

      RefPtr<RefLayer> layer = mLayerManager->CreateRefLayer();
      if (!BindLayer(layer, edit.get_OpCreateRefLayer())) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "CreateRefLayer");
      break;
    }
    case Edit::TOpSetDiagnosticTypes: {
      mLayerManager->SetDiagnosticTypes(edit.get_OpSetDiagnosticTypes().diagnostics());
      break;
    }
    case Edit::TOpWindowOverlayChanged: {
      mLayerManager->SetWindowOverlayChanged();
      break;
    }
    // Tree ops
    case Edit::TOpSetRoot: {
      MOZ_LAYERS_LOG(("[ParentSide] SetRoot"));

      Layer* newRoot = AsLayer(edit.get_OpSetRoot().root());
      if (!newRoot) {
        return IPC_FAIL_NO_REASON(this);
      }
      if (newRoot->GetParent()) {
        // newRoot is not a root!
        return IPC_FAIL_NO_REASON(this);
      }
      mRoot = newRoot;

      UpdateHitTestingTree(mRoot, "SetRoot");
      break;
    }
    case Edit::TOpInsertAfter: {
      MOZ_LAYERS_LOG(("[ParentSide] InsertAfter"));

      const OpInsertAfter& oia = edit.get_OpInsertAfter();
      Layer* child = AsLayer(oia.childLayer());
      Layer* layer = AsLayer(oia.container());
      Layer* after = AsLayer(oia.after());
      if (!child || !layer || !after) {
        return IPC_FAIL_NO_REASON(this);
      }
      ContainerLayer* container = layer->AsContainerLayer();
      if (!container || !container->InsertAfter(child, after)) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "InsertAfter");
      break;
    }
    case Edit::TOpPrependChild: {
      MOZ_LAYERS_LOG(("[ParentSide] PrependChild"));

      const OpPrependChild& oac = edit.get_OpPrependChild();
      Layer* child = AsLayer(oac.childLayer());
      Layer* layer = AsLayer(oac.container());
      if (!child || !layer) {
        return IPC_FAIL_NO_REASON(this);
      }
      ContainerLayer* container = layer->AsContainerLayer();
      if (!container || !container->InsertAfter(child, nullptr)) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "PrependChild");
      break;
    }
    case Edit::TOpRemoveChild: {
      MOZ_LAYERS_LOG(("[ParentSide] RemoveChild"));

      const OpRemoveChild& orc = edit.get_OpRemoveChild();
      Layer* childLayer = AsLayer(orc.childLayer());
      Layer* layer = AsLayer(orc.container());
      if (!childLayer || !layer) {
        return IPC_FAIL_NO_REASON(this);
      }
      ContainerLayer* container = layer->AsContainerLayer();
      if (!container || !container->RemoveChild(childLayer)) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "RemoveChild");
      break;
    }
    case Edit::TOpRepositionChild: {
      MOZ_LAYERS_LOG(("[ParentSide] RepositionChild"));

      const OpRepositionChild& orc = edit.get_OpRepositionChild();
      Layer* child = AsLayer(orc.childLayer());
      Layer* after = AsLayer(orc.after());
      Layer* layer = AsLayer(orc.container());
      if (!child || !layer || !after) {
        return IPC_FAIL_NO_REASON(this);
      }
      ContainerLayer* container = layer->AsContainerLayer();
      if (!container || !container->RepositionChild(child, after)) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "RepositionChild");
      break;
    }
    case Edit::TOpRaiseToTopChild: {
      MOZ_LAYERS_LOG(("[ParentSide] RaiseToTopChild"));

      const OpRaiseToTopChild& rtc = edit.get_OpRaiseToTopChild();
      Layer* child = AsLayer(rtc.childLayer());
      if (!child) {
        return IPC_FAIL_NO_REASON(this);
      }
      Layer* layer = AsLayer(rtc.container());
      if (!layer) {
        return IPC_FAIL_NO_REASON(this);
      }
      ContainerLayer* container = layer->AsContainerLayer();
      if (!container || !container->RepositionChild(child, nullptr)) {
        return IPC_FAIL_NO_REASON(this);
      }

      UpdateHitTestingTree(layer, "RaiseToTopChild");
      break;
    }
    case Edit::TCompositableOperation: {
      if (!ReceiveCompositableUpdate(edit.get_CompositableOperation())) {
        return IPC_FAIL_NO_REASON(this);
      }
      break;
    }
    case Edit::TOpAttachCompositable: {
      const OpAttachCompositable& op = edit.get_OpAttachCompositable();
      RefPtr<CompositableHost> host = FindCompositable(op.compositable());
      if (!Attach(AsLayer(op.layer()), host, false)) {
        return IPC_FAIL_NO_REASON(this);
      }
      host->SetCompositorBridgeID(mLayerManager->GetCompositorBridgeID());
      break;
    }
    case Edit::TOpAttachAsyncCompositable: {
      const OpAttachAsyncCompositable& op = edit.get_OpAttachAsyncCompositable();
      RefPtr<ImageBridgeParent> imageBridge = ImageBridgeParent::GetInstance(OtherPid());
      if (!imageBridge) {
        return IPC_FAIL_NO_REASON(this);
      }
      RefPtr<CompositableHost> host = imageBridge->FindCompositable(op.compositable());
      if (!host) {
        // This normally should not happen, but can after a GPU process crash.
        // Media may not have had time to update the ImageContainer associated
        // with a video frame, and we may try to attach a stale CompositableHandle.
        // Rather than break the whole transaction, we just continue.
        gfxCriticalNote << "CompositableHost " << op.compositable().Value() << " not found";
        continue;
      }
      if (!Attach(AsLayer(op.layer()), host, true)) {
        return IPC_FAIL_NO_REASON(this);
      }
      host->SetCompositorBridgeID(mLayerManager->GetCompositorBridgeID());
      break;
    }
    default:
      MOZ_CRASH("not reached");
    }
  }

  // Process simple attribute updates.
  for (const auto& op : aInfo.setSimpleAttrs()) {
    MOZ_LAYERS_LOG(("[ParentSide] SetSimpleLayerAttributes"));
    Layer* layer = AsLayer(op.layer());
    if (!layer) {
      return IPC_FAIL_NO_REASON(this);
    }
    const SimpleLayerAttributes& attrs = op.attrs();
    const SimpleLayerAttributes& orig = layer->GetSimpleAttributes();
    if (!attrs.HitTestingInfoIsEqual(orig)) {
      UpdateHitTestingTree(layer, "scrolling info changed");
    }
    layer->SetSimpleAttributes(op.attrs());
  }

  // Process attribute updates.
  for (const auto& op : aInfo.setAttrs()) {
    MOZ_LAYERS_LOG(("[ParentSide] SetLayerAttributes"));
    if (!SetLayerAttributes(op)) {
      return IPC_FAIL_NO_REASON(this);
    }
  }

  // Process paints separately, after all normal edits.
  for (const auto& op : aInfo.paints()) {
    if (!ReceiveCompositableUpdate(op)) {
      return IPC_FAIL_NO_REASON(this);
    }
  }

  mCompositorBridge->ShadowLayersUpdated(this, aInfo, mUpdateHitTestingTree);

  {
    AutoResolveRefLayers resolve(mCompositorBridge->GetCompositionManager(this));
    mLayerManager->EndTransaction(TimeStamp(), LayerManager::END_NO_IMMEDIATE_REDRAW);
  }

  if (!IsSameProcess()) {
    // Ensure that any pending operations involving back and front
    // buffers have completed, so that neither process stomps on the
    // other's buffer contents.
    LayerManagerComposite::PlatformSyncBeforeReplyUpdate();
  }

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  int compositeTime = (int)(mozilla::TimeStamp::Now() - updateStart).ToMilliseconds();
  if (compositeTime > 15) {
    printf_stderr("Compositor: Layers update took %i ms (blocking gecko).\n", compositeTime);
  }
#endif

  // Enable visual warning for long transaction when draw FPS option is enabled
  bool drawFps = gfxPrefs::LayersDrawFPS();
  if (drawFps) {
    uint32_t visualWarningTrigger = gfxPrefs::LayerTransactionWarning();
    // The default theshold is 200ms to trigger, hit red when it take 4 times longer
    TimeDuration latency = TimeStamp::Now() - aInfo.transactionStart();
    if (latency > TimeDuration::FromMilliseconds(visualWarningTrigger)) {
      float severity = (latency - TimeDuration::FromMilliseconds(visualWarningTrigger)).ToMilliseconds() /
                         (4 * visualWarningTrigger);
      if (severity > 1.f) {
        severity = 1.f;
      }
      mLayerManager->VisualFrameWarning(severity);
      printf_stderr("LayerTransactionParent::RecvUpdate transaction from process %d took %f ms",
                    OtherPid(),
                    latency.ToMilliseconds());
    }

    mLayerManager->RecordUpdateTime((TimeStamp::Now() - updateStart).ToMilliseconds());
  }

  return IPC_OK();
}

bool
LayerTransactionParent::SetLayerAttributes(const OpSetLayerAttributes& aOp)
{
  Layer* layer = AsLayer(aOp.layer());
  if (!layer) {
    return false;
  }

  const LayerAttributes& attrs = aOp.attrs();
  const CommonLayerAttributes& common = attrs.common();
  if (common.visibleRegion() != layer->GetVisibleRegion()) {
    UpdateHitTestingTree(layer, "visible region changed");
    layer->SetVisibleRegion(common.visibleRegion());
  }
  if (common.eventRegions() != layer->GetEventRegions()) {
    UpdateHitTestingTree(layer, "event regions changed");
    layer->SetEventRegions(common.eventRegions());
  }
  Maybe<ParentLayerIntRect> clipRect = common.useClipRect() ? Some(common.clipRect()) : Nothing();
  if (clipRect != layer->GetClipRect()) {
    UpdateHitTestingTree(layer, "clip rect changed");
    layer->SetClipRect(clipRect);
  }
  if (LayerHandle maskLayer = common.maskLayer()) {
    layer->SetMaskLayer(AsLayer(maskLayer));
  } else {
    layer->SetMaskLayer(nullptr);
  }
  layer->SetCompositorAnimations(common.compositorAnimations());
  // Clean up the Animations by id in the CompositorAnimationStorage
  // if there are no active animations on the layer
  if (mAnimStorage &&
      layer->GetCompositorAnimationsId() &&
      layer->GetAnimations().IsEmpty()) {
    mAnimStorage->ClearById(layer->GetCompositorAnimationsId());
  }
  if (common.scrollMetadata() != layer->GetAllScrollMetadata()) {
    UpdateHitTestingTree(layer, "scroll metadata changed");
    layer->SetScrollMetadata(common.scrollMetadata());
  }
  layer->SetDisplayListLog(common.displayListLog().get());

  // The updated invalid region is added to the existing one, since we can
  // update multiple times before the next composite.
  layer->AddInvalidRegion(common.invalidRegion());

  nsTArray<RefPtr<Layer>> maskLayers;
  for (size_t i = 0; i < common.ancestorMaskLayers().Length(); i++) {
    Layer* maskLayer = AsLayer(common.ancestorMaskLayers().ElementAt(i));
    if (!maskLayer) {
      return false;
    }
    maskLayers.AppendElement(maskLayer);
  }
  layer->SetAncestorMaskLayers(maskLayers);

  typedef SpecificLayerAttributes Specific;
  const SpecificLayerAttributes& specific = attrs.specific();
  switch (specific.type()) {
  case Specific::Tnull_t:
    break;

  case Specific::TPaintedLayerAttributes: {
    MOZ_LAYERS_LOG(("[ParentSide]   painted layer"));

    PaintedLayer* paintedLayer = layer->AsPaintedLayer();
    if (!paintedLayer) {
      return false;
    }
    const PaintedLayerAttributes& attrs =
      specific.get_PaintedLayerAttributes();

    paintedLayer->SetValidRegion(attrs.validRegion());
    break;
  }
  case Specific::TContainerLayerAttributes: {
    MOZ_LAYERS_LOG(("[ParentSide]   container layer"));

    ContainerLayer* containerLayer = layer->AsContainerLayer();
    if (!containerLayer) {
      return false;
    }
    const ContainerLayerAttributes& attrs =
      specific.get_ContainerLayerAttributes();
    containerLayer->SetPreScale(attrs.preXScale(), attrs.preYScale());
    containerLayer->SetInheritedScale(attrs.inheritedXScale(), attrs.inheritedYScale());
    containerLayer->SetScaleToResolution(attrs.scaleToResolution(),
                                         attrs.presShellResolution());
    break;
  }
  case Specific::TColorLayerAttributes: {
    MOZ_LAYERS_LOG(("[ParentSide]   color layer"));

    ColorLayer* colorLayer = layer->AsColorLayer();
    if (!colorLayer) {
      return false;
    }
    colorLayer->SetColor(specific.get_ColorLayerAttributes().color().value());
    colorLayer->SetBounds(specific.get_ColorLayerAttributes().bounds());
    break;
  }
  case Specific::TBorderLayerAttributes: {
    MOZ_LAYERS_LOG(("[ParentSide]   border layer"));

    BorderLayer* borderLayer = layer->AsBorderLayer();
    if (!borderLayer) {
      return false;
    }
    borderLayer->SetRect(specific.get_BorderLayerAttributes().rect());
    borderLayer->SetColors(specific.get_BorderLayerAttributes().colors());
    borderLayer->SetCornerRadii(specific.get_BorderLayerAttributes().corners());
    borderLayer->SetWidths(specific.get_BorderLayerAttributes().widths());
    break;
  }
  case Specific::TCanvasLayerAttributes: {
    MOZ_LAYERS_LOG(("[ParentSide]   canvas layer"));

    CanvasLayer* canvasLayer = layer->AsCanvasLayer();
    if (!canvasLayer) {
      return false;
    }
    canvasLayer->SetSamplingFilter(specific.get_CanvasLayerAttributes().samplingFilter());
    canvasLayer->SetBounds(specific.get_CanvasLayerAttributes().bounds());
    break;
  }
  case Specific::TRefLayerAttributes: {
    MOZ_LAYERS_LOG(("[ParentSide]   ref layer"));

    RefLayer* refLayer = layer->AsRefLayer();
    if (!refLayer) {
      return false;
    }
    refLayer->SetReferentId(specific.get_RefLayerAttributes().id());
    refLayer->SetEventRegionsOverride(specific.get_RefLayerAttributes().eventRegionsOverride());
    UpdateHitTestingTree(layer, "event regions override changed");
    break;
  }
  case Specific::TImageLayerAttributes: {
    MOZ_LAYERS_LOG(("[ParentSide]   image layer"));

    ImageLayer* imageLayer = layer->AsImageLayer();
    if (!imageLayer) {
      return false;
    }
    const ImageLayerAttributes& attrs = specific.get_ImageLayerAttributes();
    imageLayer->SetSamplingFilter(attrs.samplingFilter());
    imageLayer->SetScaleToSize(attrs.scaleToSize(), attrs.scaleMode());
    break;
  }
  default:
    MOZ_CRASH("not reached");
  }

  return true;
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvSetLayerObserverEpoch(const uint64_t& aLayerObserverEpoch)
{
  mChildEpoch = aLayerObserverEpoch;
  return IPC_OK();
}

bool
LayerTransactionParent::ShouldParentObserveEpoch()
{
  if (mParentEpoch == mChildEpoch) {
    return false;
  }

  mParentEpoch = mChildEpoch;
  return true;
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvSetTestSampleTime(const TimeStamp& aTime)
{
  if (!mCompositorBridge->SetTestSampleTime(GetId(), aTime)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvLeaveTestMode()
{
  mCompositorBridge->LeaveTestMode(GetId());
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvGetAnimationOpacity(const uint64_t& aCompositorAnimationsId,
                                                float* aOpacity,
                                                bool* aHasAnimationOpacity)
{
  *aHasAnimationOpacity = false;
  if (mDestroyed || !mLayerManager || mLayerManager->IsDestroyed()) {
    return IPC_FAIL_NO_REASON(this);
  }

  mCompositorBridge->ApplyAsyncProperties(
    this, CompositorBridgeParentBase::TransformsToSkip::APZ);

  if (!mAnimStorage) {
    return IPC_FAIL_NO_REASON(this);
  }

  Maybe<float> opacity = mAnimStorage->GetAnimationOpacity(aCompositorAnimationsId);
  if (opacity) {
    *aOpacity = *opacity;
    *aHasAnimationOpacity = true;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvGetAnimationTransform(const uint64_t& aCompositorAnimationsId,
                                                  MaybeTransform* aTransform)
{
  if (mDestroyed || !mLayerManager || mLayerManager->IsDestroyed()) {
    return IPC_FAIL_NO_REASON(this);
  }

  // Make sure we apply the latest animation style or else we can end up with
  // a race between when we temporarily clear the animation transform (in
  // CompositorBridgeParent::SetShadowProperties) and when animation recalculates
  // the value.
  mCompositorBridge->ApplyAsyncProperties(
    this, CompositorBridgeParentBase::TransformsToSkip::APZ);

  if (!mAnimStorage) {
    return IPC_FAIL_NO_REASON(this);
  }

  Maybe<Matrix4x4> transform = mAnimStorage->GetAnimationTransform(aCompositorAnimationsId);
  if (transform) {
    *aTransform = *transform;
  } else {
    *aTransform = mozilla::void_t();
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvGetTransform(const LayerHandle& aLayerHandle,
                                         MaybeTransform* aTransform)
{
  if (mDestroyed || !mLayerManager || mLayerManager->IsDestroyed()) {
    return IPC_FAIL_NO_REASON(this);
  }

  Layer* layer = AsLayer(aLayerHandle);
  if (!layer) {
    return IPC_FAIL_NO_REASON(this);
  }

  mCompositorBridge->ApplyAsyncProperties(
    this, CompositorBridgeParentBase::TransformsToSkip::NoneOfThem);

  Matrix4x4 transform = layer->AsHostLayer()->GetShadowBaseTransform();
  // Undo the scale transform applied by FrameTransformToTransformInDevice in
  // AsyncCompositionManager.cpp.
  if (ContainerLayer* c = layer->AsContainerLayer()) {
    transform.PostScale(1.0f/c->GetInheritedXScale(),
                        1.0f/c->GetInheritedYScale(),
                        1.0f);
  }
  float scale = 1;
  Point3D scaledOrigin;
  Point3D transformOrigin;
  for (uint32_t i = 0; i < layer->GetAnimations().Length(); i++) {
    if (layer->GetAnimations()[i].data().type() == AnimationData::TTransformData) {
      const TransformData& data = layer->GetAnimations()[i].data().get_TransformData();
      scale = data.appUnitsPerDevPixel();
      scaledOrigin =
        Point3D(NS_round(NSAppUnitsToFloatPixels(data.origin().x, scale)),
                NS_round(NSAppUnitsToFloatPixels(data.origin().y, scale)),
                0.0f);
      transformOrigin = data.transformOrigin();
      break;
    }
  }

  // If our parent isn't a perspective layer, then the offset into reference
  // frame coordinates will have been applied to us. Add an inverse translation
  // to cancel it out.
  if (!layer->GetParent() || !layer->GetParent()->GetTransformIsPerspective()) {
    transform.PostTranslate(-scaledOrigin.x, -scaledOrigin.y, -scaledOrigin.z);
  }

  // This function is supposed to include the APZ transform, but if root scroll
  // containers are enabled, then the APZ transform might not be on |layer| but
  // instead would be on the parent of |layer|, if that is the root scrollable
  // metrics. So we special-case that behaviour.
  if (gfxPrefs::LayoutUseContainersForRootFrames() &&
      !layer->HasScrollableFrameMetrics() &&
      layer->GetParent() &&
      layer->GetParent()->HasRootScrollableFrameMetrics()) {
    transform *= layer->GetParent()->AsHostLayer()->GetShadowBaseTransform();
  }

  *aTransform = transform;

  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvSetAsyncScrollOffset(const FrameMetrics::ViewID& aScrollID,
                                                 const float& aX, const float& aY)
{
  if (mDestroyed || !mLayerManager || mLayerManager->IsDestroyed()) {
    return IPC_FAIL_NO_REASON(this);
  }

  mCompositorBridge->SetTestAsyncScrollOffset(GetId(), aScrollID, CSSPoint(aX, aY));
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvSetAsyncZoom(const FrameMetrics::ViewID& aScrollID,
                                         const float& aValue)
{
  if (mDestroyed || !mLayerManager || mLayerManager->IsDestroyed()) {
    return IPC_FAIL_NO_REASON(this);
  }

  mCompositorBridge->SetTestAsyncZoom(GetId(), aScrollID, LayerToParentLayerScale(aValue));
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvFlushApzRepaints()
{
  mCompositorBridge->FlushApzRepaints(GetId());
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvGetAPZTestData(APZTestData* aOutData)
{
  mCompositorBridge->GetAPZTestData(GetId(), aOutData);
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvRequestProperty(const nsString& aProperty, float* aValue)
{
  *aValue = -1;
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvSetConfirmedTargetAPZC(const uint64_t& aBlockId,
                                                   nsTArray<ScrollableLayerGuid>&& aTargets)
{
  mCompositorBridge->SetConfirmedTargetAPZC(GetId(), aBlockId, aTargets);
  return IPC_OK();
}

bool
LayerTransactionParent::Attach(Layer* aLayer,
                               CompositableHost* aCompositable,
                               bool aIsAsync)
{
  if (!aCompositable || !aLayer) {
    return false;
  }

  HostLayer* layer = aLayer->AsHostLayer();
  if (!layer) {
    return false;
  }

  TextureSourceProvider* provider =
    static_cast<HostLayerManager*>(aLayer->Manager())->GetTextureSourceProvider();

  MOZ_ASSERT(!aCompositable->AsWebRenderImageHost());
  if (aCompositable->AsWebRenderImageHost()) {
    gfxCriticalNote << "Use WebRenderImageHost at LayerTransactionParent.";
  }
  if (!layer->SetCompositableHost(aCompositable)) {
    // not all layer types accept a compositable, see bug 967824
    return false;
  }
  aCompositable->Attach(aLayer,
                        provider,
                        aIsAsync
                          ? CompositableHost::ALLOW_REATTACH
                            | CompositableHost::KEEP_ATTACHED
                          : CompositableHost::NO_FLAGS);
  return true;
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvClearCachedResources()
{
  if (mRoot) {
    // NB: |mRoot| here is the *child* context's root.  In this parent
    // context, it's just a subtree root.  We need to scope the clear
    // of resources to exactly that subtree, so we specify it here.
    mLayerManager->ClearCachedResources(mRoot);
  }
  mCompositorBridge->NotifyClearCachedResources(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvScheduleComposite()
{
  mCompositorBridge->ScheduleComposite(this);
  return IPC_OK();
}

void
LayerTransactionParent::ActorDestroy(ActorDestroyReason why)
{
  Destroy();
}

bool
LayerTransactionParent::AllocShmem(size_t aSize,
                                   ipc::SharedMemory::SharedMemoryType aType,
                                   ipc::Shmem* aShmem)
{
  if (!mIPCOpen || mDestroyed) {
    return false;
  }
  return PLayerTransactionParent::AllocShmem(aSize, aType, aShmem);
}

bool
LayerTransactionParent::AllocUnsafeShmem(size_t aSize,
                                         ipc::SharedMemory::SharedMemoryType aType,
                                         ipc::Shmem* aShmem)
{
  if (!mIPCOpen || mDestroyed) {
    return false;
  }

  return PLayerTransactionParent::AllocUnsafeShmem(aSize, aType, aShmem);
}

void
LayerTransactionParent::DeallocShmem(ipc::Shmem& aShmem)
{
  if (!mIPCOpen || mDestroyed) {
    return;
  }
  PLayerTransactionParent::DeallocShmem(aShmem);
}

bool LayerTransactionParent::IsSameProcess() const
{
  return OtherPid() == base::GetCurrentProcId();
}

TransactionId
LayerTransactionParent::FlushTransactionId(TimeStamp& aCompositeEnd)
{
#if defined(ENABLE_FRAME_LATENCY_LOG)
  if (mPendingTransaction.IsValid()) {
    if (mTxnStartTime) {
      uint32_t latencyMs = round((aCompositeEnd - mTxnStartTime).ToMilliseconds());
      printf_stderr("From transaction start to end of generate frame latencyMs %d this %p\n", latencyMs, this);
    }
    if (mFwdTime) {
      uint32_t latencyMs = round((aCompositeEnd - mFwdTime).ToMilliseconds());
      printf_stderr("From forwarding transaction to end of generate frame latencyMs %d this %p\n", latencyMs, this);
    }
  }
  mTxnStartTime = TimeStamp();
  mFwdTime = TimeStamp();
#endif
  TransactionId id = mPendingTransaction;
  mPendingTransaction = TransactionId{0};
  return id;
}

void
LayerTransactionParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
LayerTransactionParent::SendPendingAsyncMessages()
{
  mCompositorBridge->SendPendingAsyncMessages();
}

void
LayerTransactionParent::SetAboutToSendAsyncMessages()
{
  mCompositorBridge->SetAboutToSendAsyncMessages();
}

void
LayerTransactionParent::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

bool
LayerTransactionParent::BindLayerToHandle(RefPtr<Layer> aLayer, const LayerHandle& aHandle)
{
  if (!aHandle || !aLayer) {
    return false;
  }
  if (auto entry = mLayerMap.LookupForAdd(aHandle.Value())) {
    return false;
  } else {
    entry.OrInsert([&aLayer] () { return aLayer; });
  }
  return true;
}

Layer*
LayerTransactionParent::AsLayer(const LayerHandle& aHandle)
{
  if (!aHandle) {
    return nullptr;
  }
  return mLayerMap.GetWeak(aHandle.Value());
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvNewCompositable(const CompositableHandle& aHandle, const TextureInfo& aInfo)
{
  if (!AddCompositable(aHandle, aInfo, /* aUseWebRender */ false)) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvReleaseLayer(const LayerHandle& aHandle)
{
  RefPtr<Layer> layer;
  if (!aHandle || !mLayerMap.Remove(aHandle.Value(), getter_AddRefs(layer))) {
    return IPC_FAIL_NO_REASON(this);
  }
  if (mAnimStorage &&
      layer->GetCompositorAnimationsId()) {
    mAnimStorage->ClearById(layer->GetCompositorAnimationsId());
    layer->ClearCompositorAnimations();
  }
  layer->Disconnect();
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvReleaseCompositable(const CompositableHandle& aHandle)
{
  ReleaseCompositable(aHandle);
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvRecordPaintTimes(const PaintTiming& aTiming)
{
  // Currently we only add paint timings for remote layers. In the future
  // we could be smarter and use paint timings from the UI process, either
  // as a separate overlay or if no remote layers are attached.
  if (mLayerManager && mCompositorBridge->IsRemote()) {
    mLayerManager->RecordPaintTimes(aTiming);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
LayerTransactionParent::RecvGetTextureFactoryIdentifier(TextureFactoryIdentifier* aIdentifier)
{
  if (!mLayerManager) {
    // Default constructor sets mParentBackend to LAYERS_NONE.
    return IPC_OK();
  }

  *aIdentifier = mLayerManager->GetTextureFactoryIdentifier();
  return IPC_OK();
}

} // namespace layers
} // namespace mozilla
