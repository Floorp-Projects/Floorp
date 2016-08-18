/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerTransactionParent.h"
#include <vector>                       // for vector
#include "apz/src/AsyncPanZoomController.h"
#include "CompositableHost.h"           // for CompositableParent, Get, etc
#include "ImageLayers.h"                // for ImageLayer
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "ShadowLayerParent.h"          // for ShadowLayerParent
#include "CompositableTransactionParent.h"  // for EditReplyVector
#include "gfxPrefs.h"
#include "mozilla/gfx/BasePoint3D.h"    // for BasePoint3D
#include "mozilla/layers/CanvasLayerComposite.h"
#include "mozilla/layers/ColorLayerComposite.h"
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/ContainerLayerComposite.h"
#include "mozilla/layers/ImageBridgeParent.h" // for ImageBridgeParent
#include "mozilla/layers/ImageLayerComposite.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply, etc
#include "mozilla/layers/LayersSurfaces.h"  // for PGrallocBufferParent
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/PCompositableParent.h"
#include "mozilla/layers/PLayerParent.h"  // for PLayerParent
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/PaintedLayerComposite.h"
#include "mozilla/layers/ShadowLayersManager.h" // for ShadowLayersManager
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "mozilla/unused.h"
#include "nsCoord.h"                    // for NSAppUnitsToFloatPixels
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsDeviceContext.h"            // for AppUnitsPerCSSPixel
#include "nsISupportsImpl.h"            // for Layer::Release, etc
#include "nsLayoutUtils.h"              // for nsLayoutUtils
#include "nsMathUtils.h"                // for NS_round
#include "nsPoint.h"                    // for nsPoint
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "TreeTraversal.h"              // for ForEachNode
#include "GeckoProfiler.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/AsyncCompositionManager.h"

typedef std::vector<mozilla::layers::EditReply> EditReplyVector;

using mozilla::layout::RenderFrameParent;

namespace mozilla {
namespace layers {

class PGrallocBufferParent;

//--------------------------------------------------
// Convenience accessors
static ShadowLayerParent*
cast(const PLayerParent* in)
{
  return const_cast<ShadowLayerParent*>(
    static_cast<const ShadowLayerParent*>(in));
}

template<class OpCreateT>
static ShadowLayerParent*
AsLayerComposite(const OpCreateT& op)
{
  return cast(op.layerParent());
}

static ShadowLayerParent*
AsLayerComposite(const OpSetRoot& op)
{
  return cast(op.rootParent());
}

static ShadowLayerParent*
ShadowContainer(const OpInsertAfter& op)
{
  return cast(op.containerParent());
}
static ShadowLayerParent*
ShadowChild(const OpInsertAfter& op)
{
  return cast(op.childLayerParent());
}
static ShadowLayerParent*
ShadowAfter(const OpInsertAfter& op)
{
  return cast(op.afterParent());
}

static ShadowLayerParent*
ShadowContainer(const OpPrependChild& op)
{
  return cast(op.containerParent());
}
static ShadowLayerParent*
ShadowChild(const OpPrependChild& op)
{
  return cast(op.childLayerParent());
}

static ShadowLayerParent*
ShadowContainer(const OpRemoveChild& op)
{
  return cast(op.containerParent());
}
static ShadowLayerParent*
ShadowChild(const OpRemoveChild& op)
{
  return cast(op.childLayerParent());
}

static ShadowLayerParent*
ShadowContainer(const OpRepositionChild& op)
{
  return cast(op.containerParent());
}
static ShadowLayerParent*
ShadowChild(const OpRepositionChild& op)
{
  return cast(op.childLayerParent());
}
static ShadowLayerParent*
ShadowAfter(const OpRepositionChild& op)
{
  return cast(op.afterParent());
}

static ShadowLayerParent*
ShadowContainer(const OpRaiseToTopChild& op)
{
  return cast(op.containerParent());
}
static ShadowLayerParent*
ShadowChild(const OpRaiseToTopChild& op)
{
  return cast(op.childLayerParent());
}

//--------------------------------------------------
// LayerTransactionParent
LayerTransactionParent::LayerTransactionParent(LayerManagerComposite* aManager,
                                               ShadowLayersManager* aLayersManager,
                                               uint64_t aId)
  : mLayerManager(aManager)
  , mShadowLayersManager(aLayersManager)
  , mId(aId)
  , mPendingTransaction(0)
  , mPendingCompositorUpdates(0)
  , mDestroyed(false)
  , mIPCOpen(false)
{
}

LayerTransactionParent::~LayerTransactionParent()
{
}

bool
LayerTransactionParent::RecvShutdown()
{
  Destroy();
  return Send__delete__(this);
}

void
LayerTransactionParent::Destroy()
{
  const ManagedContainer<PLayerParent>& layers = ManagedPLayerParent();
  for (auto iter = layers.ConstIter(); !iter.Done(); iter.Next()) {
    ShadowLayerParent* slp =
      static_cast<ShadowLayerParent*>(iter.Get()->GetKey());
    slp->Destroy();
  }
  mDestroyed = true;
}

bool
LayerTransactionParent::RecvUpdateNoSwap(InfallibleTArray<Edit>&& cset,
                                         InfallibleTArray<OpDestroy>&& aToDestroy,
                                         const uint64_t& aFwdTransactionId,
                                         const uint64_t& aTransactionId,
                                         const TargetConfig& targetConfig,
                                         PluginsArray&& aPlugins,
                                         const bool& isFirstPaint,
                                         const bool& scheduleComposite,
                                         const uint32_t& paintSequenceNumber,
                                         const bool& isRepeatTransaction,
                                         const mozilla::TimeStamp& aTransactionStart,
                                         const int32_t& aPaintSyncId)
{
  return RecvUpdate(Move(cset), Move(aToDestroy), aFwdTransactionId,
      aTransactionId, targetConfig, Move(aPlugins), isFirstPaint,
      scheduleComposite, paintSequenceNumber, isRepeatTransaction,
      aTransactionStart, aPaintSyncId, nullptr);
}

class MOZ_STACK_CLASS AutoLayerTransactionParentAsyncMessageSender
{
public:
  explicit AutoLayerTransactionParentAsyncMessageSender(LayerTransactionParent* aLayerTransaction,
                                                        InfallibleTArray<OpDestroy>* aDestroyActors = nullptr)
    : mLayerTransaction(aLayerTransaction)
    , mActorsToDestroy(aDestroyActors)
  {
    mLayerTransaction->SetAboutToSendAsyncMessages();
    ImageBridgeParent::SetAboutToSendAsyncMessages(mLayerTransaction->GetChildProcessId());
  }

  ~AutoLayerTransactionParentAsyncMessageSender()
  {
    mLayerTransaction->SendPendingAsyncMessages();
    ImageBridgeParent::SendPendingAsyncMessages(mLayerTransaction->GetChildProcessId());
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
  InfallibleTArray<OpDestroy>* mActorsToDestroy;
};

bool
LayerTransactionParent::RecvPaintTime(const uint64_t& aTransactionId,
                                      const TimeDuration& aPaintTime)
{
  mShadowLayersManager->UpdatePaintTime(this, aPaintTime);
  return true;
}

bool
LayerTransactionParent::RecvUpdate(InfallibleTArray<Edit>&& cset,
                                   InfallibleTArray<OpDestroy>&& aToDestroy,
                                   const uint64_t& aFwdTransactionId,
                                   const uint64_t& aTransactionId,
                                   const TargetConfig& targetConfig,
                                   PluginsArray&& aPlugins,
                                   const bool& isFirstPaint,
                                   const bool& scheduleComposite,
                                   const uint32_t& paintSequenceNumber,
                                   const bool& isRepeatTransaction,
                                   const mozilla::TimeStamp& aTransactionStart,
                                   const int32_t& aPaintSyncId,
                                   InfallibleTArray<EditReply>* reply)
{
  profiler_tracing("Paint", "LayerTransaction", TRACING_INTERVAL_START);
  PROFILER_LABEL("LayerTransactionParent", "RecvUpdate",
    js::ProfileEntry::Category::GRAPHICS);

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp updateStart = TimeStamp::Now();
#endif

  MOZ_LAYERS_LOG(("[ParentSide] received txn with %d edits", cset.Length()));

  UpdateFwdTransactionId(aFwdTransactionId);

  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    for (const auto& op : aToDestroy) {
      DestroyActor(op);
    }
    return true;
  }

  AutoLayerTransactionParentAsyncMessageSender autoAsyncMessageSender(this, &aToDestroy);
  EditReplyVector replyv;

  {
    AutoResolveRefLayers resolve(mShadowLayersManager->GetCompositionManager(this));
    layer_manager()->BeginTransaction();
  }

  // not all edits require an update to the hit testing tree
  bool updateHitTestingTree = false;

  for (EditArray::index_type i = 0; i < cset.Length(); ++i) {
    const Edit& edit = cset[i];

    switch (edit.type()) {
    // Create* ops
    case Edit::TOpCreatePaintedLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreatePaintedLayer"));

      RefPtr<PaintedLayerComposite> layer =
        layer_manager()->CreatePaintedLayerComposite();
      AsLayerComposite(edit.get_OpCreatePaintedLayer())->Bind(layer);

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpCreateContainerLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateContainerLayer"));

      RefPtr<ContainerLayer> layer = layer_manager()->CreateContainerLayerComposite();
      AsLayerComposite(edit.get_OpCreateContainerLayer())->Bind(layer);

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpCreateImageLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateImageLayer"));

      RefPtr<ImageLayerComposite> layer =
        layer_manager()->CreateImageLayerComposite();
      AsLayerComposite(edit.get_OpCreateImageLayer())->Bind(layer);

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpCreateColorLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateColorLayer"));

      RefPtr<ColorLayerComposite> layer = layer_manager()->CreateColorLayerComposite();
      AsLayerComposite(edit.get_OpCreateColorLayer())->Bind(layer);

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpCreateCanvasLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateCanvasLayer"));

      RefPtr<CanvasLayerComposite> layer =
        layer_manager()->CreateCanvasLayerComposite();
      AsLayerComposite(edit.get_OpCreateCanvasLayer())->Bind(layer);

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpCreateRefLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateRefLayer"));

      RefPtr<RefLayerComposite> layer =
        layer_manager()->CreateRefLayerComposite();
      AsLayerComposite(edit.get_OpCreateRefLayer())->Bind(layer);

      updateHitTestingTree = true;
      break;
    }

    // Attributes
    case Edit::TOpSetLayerAttributes: {
      MOZ_LAYERS_LOG(("[ParentSide] SetLayerAttributes"));

      const OpSetLayerAttributes& osla = edit.get_OpSetLayerAttributes();
      ShadowLayerParent* layerParent = AsLayerComposite(osla);
      Layer* layer = layerParent->AsLayer();
      if (!layer) {
        return false;
      }
      const LayerAttributes& attrs = osla.attrs();

      const CommonLayerAttributes& common = attrs.common();
      layer->SetLayerBounds(common.layerBounds());
      layer->SetVisibleRegion(common.visibleRegion());
      layer->SetEventRegions(common.eventRegions());
      layer->SetContentFlags(common.contentFlags());
      layer->SetOpacity(common.opacity());
      layer->SetClipRect(common.useClipRect() ? Some(common.clipRect()) : Nothing());
      layer->SetScrolledClip(common.scrolledClip());
      layer->SetBaseTransform(common.transform().value());
      layer->SetTransformIsPerspective(common.transformIsPerspective());
      layer->SetPostScale(common.postXScale(), common.postYScale());
      layer->SetIsFixedPosition(common.isFixedPosition());
      if (common.isFixedPosition()) {
        layer->SetFixedPositionData(common.fixedPositionScrollContainerId(),
                                    common.fixedPositionAnchor(),
                                    common.fixedPositionSides());
      }
      if (common.isStickyPosition()) {
        layer->SetStickyPositionData(common.stickyScrollContainerId(),
                                     common.stickyScrollRangeOuter(),
                                     common.stickyScrollRangeInner());
      }
      layer->SetScrollbarData(common.scrollbarTargetContainerId(),
        static_cast<Layer::ScrollDirection>(common.scrollbarDirection()),
        common.scrollbarThumbRatio());
      if (common.isScrollbarContainer()) {
        layer->SetIsScrollbarContainer();
      }
      layer->SetMixBlendMode((gfx::CompositionOp)common.mixBlendMode());
      layer->SetForceIsolatedGroup(common.forceIsolatedGroup());
      if (PLayerParent* maskLayer = common.maskLayerParent()) {
        layer->SetMaskLayer(cast(maskLayer)->AsLayer());
      } else {
        layer->SetMaskLayer(nullptr);
      }
      layer->SetAnimations(common.animations());
      layer->SetScrollMetadata(common.scrollMetadata());
      layer->SetDisplayListLog(common.displayListLog().get());

      // The updated invalid region is added to the existing one, since we can
      // update multiple times before the next composite.
      layer->AddInvalidRegion(common.invalidRegion());

      nsTArray<RefPtr<Layer>> maskLayers;
      for (size_t i = 0; i < common.ancestorMaskLayersParent().Length(); i++) {
        Layer* maskLayer = cast(common.ancestorMaskLayersParent().ElementAt(i))->AsLayer();
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

        PaintedLayerComposite* paintedLayer = layerParent->AsPaintedLayerComposite();
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

        ContainerLayerComposite* containerLayer = layerParent->AsContainerLayerComposite();
        if (!containerLayer) {
          return false;
        }
        const ContainerLayerAttributes& attrs =
          specific.get_ContainerLayerAttributes();
        containerLayer->SetPreScale(attrs.preXScale(), attrs.preYScale());
        containerLayer->SetInheritedScale(attrs.inheritedXScale(), attrs.inheritedYScale());
        containerLayer->SetScaleToResolution(attrs.scaleToResolution(),
                                             attrs.presShellResolution());
        containerLayer->SetEventRegionsOverride(attrs.eventRegionsOverride());

        break;
      }
      case Specific::TColorLayerAttributes: {
        MOZ_LAYERS_LOG(("[ParentSide]   color layer"));

        ColorLayerComposite* colorLayer = layerParent->AsColorLayerComposite();
        if (!colorLayer) {
          return false;
        }
        colorLayer->SetColor(specific.get_ColorLayerAttributes().color().value());
        colorLayer->SetBounds(specific.get_ColorLayerAttributes().bounds());
        break;
      }
      case Specific::TCanvasLayerAttributes: {
        MOZ_LAYERS_LOG(("[ParentSide]   canvas layer"));

        CanvasLayerComposite* canvasLayer = layerParent->AsCanvasLayerComposite();
        if (!canvasLayer) {
          return false;
        }
        canvasLayer->SetSamplingFilter(specific.get_CanvasLayerAttributes().samplingFilter());
        canvasLayer->SetBounds(specific.get_CanvasLayerAttributes().bounds());
        break;
      }
      case Specific::TRefLayerAttributes: {
        MOZ_LAYERS_LOG(("[ParentSide]   ref layer"));

        RefLayerComposite* refLayer = layerParent->AsRefLayerComposite();
        if (!refLayer) {
          return false;
        }
        refLayer->SetReferentId(specific.get_RefLayerAttributes().id());
        refLayer->SetEventRegionsOverride(specific.get_RefLayerAttributes().eventRegionsOverride());
        break;
      }
      case Specific::TImageLayerAttributes: {
        MOZ_LAYERS_LOG(("[ParentSide]   image layer"));

        ImageLayerComposite* imageLayer = layerParent->AsImageLayerComposite();
        if (!imageLayer) {
          return false;
        }
        const ImageLayerAttributes& attrs = specific.get_ImageLayerAttributes();
        imageLayer->SetSamplingFilter(attrs.samplingFilter());
        imageLayer->SetScaleToSize(attrs.scaleToSize(), attrs.scaleMode());
        break;
      }
      default:
        NS_RUNTIMEABORT("not reached");
      }

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpSetDiagnosticTypes: {
      mLayerManager->GetCompositor()->SetDiagnosticTypes(
        edit.get_OpSetDiagnosticTypes().diagnostics());
      break;
    }
    case Edit::TOpWindowOverlayChanged: {
      mLayerManager->SetWindowOverlayChanged();
      break;
    }
    // Tree ops
    case Edit::TOpSetRoot: {
      MOZ_LAYERS_LOG(("[ParentSide] SetRoot"));

      Layer* newRoot = AsLayerComposite(edit.get_OpSetRoot())->AsLayer();
      if (!newRoot) {
        return false;
      }
      if (newRoot->GetParent()) {
        // newRoot is not a root!
        return false;
      }
      mRoot = newRoot;

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpInsertAfter: {
      MOZ_LAYERS_LOG(("[ParentSide] InsertAfter"));

      const OpInsertAfter& oia = edit.get_OpInsertAfter();
      Layer* child = ShadowChild(oia)->AsLayer();
      if (!child) {
        return false;
      }
      ContainerLayerComposite* container = ShadowContainer(oia)->AsContainerLayerComposite();
      if (!container ||
          !container->InsertAfter(child, ShadowAfter(oia)->AsLayer()))
      {
        return false;
      }

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpPrependChild: {
      MOZ_LAYERS_LOG(("[ParentSide] PrependChild"));

      const OpPrependChild& oac = edit.get_OpPrependChild();
      Layer* child = ShadowChild(oac)->AsLayer();
      if (!child) {
        return false;
      }
      ContainerLayerComposite* container = ShadowContainer(oac)->AsContainerLayerComposite();
      if (!container ||
          !container->InsertAfter(child, nullptr))
      {
        return false;
      }

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpRemoveChild: {
      MOZ_LAYERS_LOG(("[ParentSide] RemoveChild"));

      const OpRemoveChild& orc = edit.get_OpRemoveChild();
      Layer* childLayer = ShadowChild(orc)->AsLayer();
      if (!childLayer) {
        return false;
      }
      ContainerLayerComposite* container = ShadowContainer(orc)->AsContainerLayerComposite();
      if (!container ||
          !container->RemoveChild(childLayer))
      {
        return false;
      }

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpRepositionChild: {
      MOZ_LAYERS_LOG(("[ParentSide] RepositionChild"));

      const OpRepositionChild& orc = edit.get_OpRepositionChild();
      Layer* child = ShadowChild(orc)->AsLayer();
      if (!child) {
        return false;
      }
      ContainerLayerComposite* container = ShadowContainer(orc)->AsContainerLayerComposite();
      if (!container ||
          !container->RepositionChild(child, ShadowAfter(orc)->AsLayer()))
      {
        return false;
      }

      updateHitTestingTree = true;
      break;
    }
    case Edit::TOpRaiseToTopChild: {
      MOZ_LAYERS_LOG(("[ParentSide] RaiseToTopChild"));

      const OpRaiseToTopChild& rtc = edit.get_OpRaiseToTopChild();
      Layer* child = ShadowChild(rtc)->AsLayer();
      if (!child) {
        return false;
      }
      ContainerLayerComposite* container = ShadowContainer(rtc)->AsContainerLayerComposite();
      if (!container ||
          !container->RepositionChild(child, nullptr))
      {
        return false;
      }

      updateHitTestingTree = true;
      break;
    }
    case Edit::TCompositableOperation: {
      if (!ReceiveCompositableUpdate(edit.get_CompositableOperation(),
                                replyv)) {
        return false;
      }
      break;
    }
    case Edit::TOpAttachCompositable: {
      const OpAttachCompositable& op = edit.get_OpAttachCompositable();
      CompositableHost* host = CompositableHost::FromIPDLActor(op.compositableParent());
      if (mPendingCompositorUpdates) {
        // Do not attach compositables from old layer trees. Return true since
        // content cannot handle errors.
        return true;
      }
      if (!Attach(cast(op.layerParent()), host, false)) {
        return false;
      }
      host->SetCompositorID(mLayerManager->GetCompositor()->GetCompositorID());
      break;
    }
    case Edit::TOpAttachAsyncCompositable: {
      const OpAttachAsyncCompositable& op = edit.get_OpAttachAsyncCompositable();
      PCompositableParent* compositableParent = CompositableMap::Get(op.containerID());
      if (!compositableParent) {
        NS_ERROR("CompositableParent not found in the map");
        return false;
      }
      if (mPendingCompositorUpdates) {
        // Do not attach compositables from old layer trees. Return true since
        // content cannot handle errors.
        return true;
      }
      CompositableHost* host = CompositableHost::FromIPDLActor(compositableParent);
      if (!Attach(cast(op.layerParent()), host, true)) {
        return false;
      }
      host->SetCompositorID(mLayerManager->GetCompositor()->GetCompositorID());
      break;
    }
    default:
      NS_RUNTIMEABORT("not reached");
    }
  }

  mShadowLayersManager->ShadowLayersUpdated(this, aTransactionId, targetConfig,
                                            aPlugins, isFirstPaint, scheduleComposite,
                                            paintSequenceNumber, isRepeatTransaction,
                                            aPaintSyncId, updateHitTestingTree);

  {
    AutoResolveRefLayers resolve(mShadowLayersManager->GetCompositionManager(this));
    layer_manager()->EndTransaction(TimeStamp(), LayerManager::END_NO_IMMEDIATE_REDRAW);
  }

  if (reply) {
    reply->SetCapacity(replyv.size());
    if (replyv.size() > 0) {
      reply->AppendElements(&replyv.front(), replyv.size());
    }
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
    TimeDuration latency = TimeStamp::Now() - aTransactionStart;
    if (latency > TimeDuration::FromMilliseconds(visualWarningTrigger)) {
      float severity = (latency - TimeDuration::FromMilliseconds(visualWarningTrigger)).ToMilliseconds() /
                         (4 * visualWarningTrigger);
      if (severity > 1.f) {
        severity = 1.f;
      }
      mLayerManager->VisualFrameWarning(severity);
      PR_LogPrint("LayerTransactionParent::RecvUpdate transaction from process %d took %f ms",
                  OtherPid(),
                  latency.ToMilliseconds());
    }
  }

  profiler_tracing("Paint", "LayerTransaction", TRACING_INTERVAL_END);
  return true;
}

bool
LayerTransactionParent::RecvSetTestSampleTime(const TimeStamp& aTime)
{
  return mShadowLayersManager->SetTestSampleTime(this, aTime);
}

bool
LayerTransactionParent::RecvLeaveTestMode()
{
  mShadowLayersManager->LeaveTestMode(this);
  return true;
}

bool
LayerTransactionParent::RecvGetOpacity(PLayerParent* aParent,
                                       float* aOpacity)
{
  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    return false;
  }

  Layer* layer = cast(aParent)->AsLayer();
  if (!layer) {
    return false;
  }

  mShadowLayersManager->ApplyAsyncProperties(this);

  *aOpacity = layer->GetLocalOpacity();
  return true;
}

bool
LayerTransactionParent::RecvGetAnimationTransform(PLayerParent* aParent,
                                                  MaybeTransform* aTransform)
{
  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    return false;
  }

  Layer* layer = cast(aParent)->AsLayer();
  if (!layer) {
    return false;
  }

  // Make sure we apply the latest animation style or else we can end up with
  // a race between when we temporarily clear the animation transform (in
  // CompositorBridgeParent::SetShadowProperties) and when animation recalculates
  // the value.
  mShadowLayersManager->ApplyAsyncProperties(this);

  // This method is specific to transforms applied by animation.
  // This is because this method uses the information stored with an animation
  // such as the origin of the reference frame corresponding to the layer, to
  // recover the untranslated transform from the shadow transform. For
  // transforms that are not set by animation we don't have this information
  // available.
  if (!layer->AsLayerComposite()->GetShadowTransformSetByAnimation()) {
    *aTransform = mozilla::void_t();
    return true;
  }

  // The following code recovers the untranslated transform
  // from the shadow transform by undoing the translations in
  // AsyncCompositionManager::SampleValue.

  Matrix4x4 transform = layer->AsLayerComposite()->GetShadowBaseTransform();
  if (ContainerLayer* c = layer->AsContainerLayer()) {
    // Undo the scale transform applied by AsyncCompositionManager::SampleValue
    transform.PostScale(1.0f/c->GetInheritedXScale(),
                        1.0f/c->GetInheritedYScale(),
                        1.0f);
  }
  float scale = 1;
  Point3D scaledOrigin;
  Point3D transformOrigin;
  for (uint32_t i=0; i < layer->GetAnimations().Length(); i++) {
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

  // Undo the rebasing applied by
  // nsDisplayTransform::GetResultingTransformMatrixInternal
  transform.ChangeBasis(-transformOrigin);

  // Convert to CSS pixels (this undoes the operations performed by
  // nsStyleTransformMatrix::ProcessTranslatePart which is called from
  // nsDisplayTransform::GetResultingTransformMatrix)
  double devPerCss =
    double(scale) / double(nsDeviceContext::AppUnitsPerCSSPixel());
  transform._41 *= devPerCss;
  transform._42 *= devPerCss;
  transform._43 *= devPerCss;

  *aTransform = transform;
  return true;
}

static AsyncPanZoomController*
GetAPZCForViewID(Layer* aLayer, FrameMetrics::ViewID aScrollID)
{
  AsyncPanZoomController* resultApzc = nullptr;
  ForEachNode<ForwardIterator>(
      aLayer,
      [aScrollID, &resultApzc] (Layer* layer)
      {
        for (uint32_t i = 0; i < layer->GetScrollMetadataCount(); i++) {
          if (layer->GetFrameMetrics(i).GetScrollId() == aScrollID) {
            resultApzc = layer->GetAsyncPanZoomController(i);
            return TraversalFlag::Abort;
          }
        }
        return TraversalFlag::Continue;
      });
  return resultApzc;
}

bool
LayerTransactionParent::RecvSetAsyncScrollOffset(const FrameMetrics::ViewID& aScrollID,
                                                 const float& aX, const float& aY)
{
  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    return false;
  }

  AsyncPanZoomController* controller = GetAPZCForViewID(mRoot, aScrollID);
  if (!controller) {
    return false;
  }
  controller->SetTestAsyncScrollOffset(CSSPoint(aX, aY));
  return true;
}

bool
LayerTransactionParent::RecvSetAsyncZoom(const FrameMetrics::ViewID& aScrollID,
                                         const float& aValue)
{
  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    return false;
  }

  AsyncPanZoomController* controller = GetAPZCForViewID(mRoot, aScrollID);
  if (!controller) {
    return false;
  }
  controller->SetTestAsyncZoom(LayerToParentLayerScale(aValue));
  return true;
}

bool
LayerTransactionParent::RecvFlushApzRepaints()
{
  mShadowLayersManager->FlushApzRepaints(this);
  return true;
}

bool
LayerTransactionParent::RecvGetAPZTestData(APZTestData* aOutData)
{
  mShadowLayersManager->GetAPZTestData(this, aOutData);
  return true;
}

bool
LayerTransactionParent::RecvRequestProperty(const nsString& aProperty, float* aValue)
{
  if (aProperty.Equals(NS_LITERAL_STRING("overdraw"))) {
    *aValue = layer_manager()->GetCompositor()->GetFillRatio();
  } else if (aProperty.Equals(NS_LITERAL_STRING("missed_hwc"))) {
    *aValue = layer_manager()->LastFrameMissedHWC() ? 1 : 0;
  } else {
    *aValue = -1;
  }
  return true;
}

bool
LayerTransactionParent::RecvSetConfirmedTargetAPZC(const uint64_t& aBlockId,
                                                   nsTArray<ScrollableLayerGuid>&& aTargets)
{
  mShadowLayersManager->SetConfirmedTargetAPZC(this, aBlockId, aTargets);
  return true;
}

bool
LayerTransactionParent::Attach(ShadowLayerParent* aLayerParent,
                               CompositableHost* aCompositable,
                               bool aIsAsync)
{
  if (!aCompositable) {
    return false;
  }

  Layer* baselayer = aLayerParent->AsLayer();
  if (!baselayer) {
    return false;
  }
  LayerComposite* layer = baselayer->AsLayerComposite();
  if (!layer) {
    return false;
  }

  Compositor* compositor
    = static_cast<LayerManagerComposite*>(aLayerParent->AsLayer()->Manager())->GetCompositor();

  if (!layer->SetCompositableHost(aCompositable)) {
    // not all layer types accept a compositable, see bug 967824
    return false;
  }
  aCompositable->Attach(aLayerParent->AsLayer(),
                        compositor,
                        aIsAsync
                          ? CompositableHost::ALLOW_REATTACH
                            | CompositableHost::KEEP_ATTACHED
                          : CompositableHost::NO_FLAGS);
  return true;
}

bool
LayerTransactionParent::RecvClearCachedResources()
{
  if (mRoot) {
    // NB: |mRoot| here is the *child* context's root.  In this parent
    // context, it's just a subtree root.  We need to scope the clear
    // of resources to exactly that subtree, so we specify it here.
    mLayerManager->ClearCachedResources(mRoot);
  }
  mShadowLayersManager->NotifyClearCachedResources(this);
  return true;
}

bool
LayerTransactionParent::RecvForceComposite()
{
  mShadowLayersManager->ForceComposite(this);
  return true;
}

PLayerParent*
LayerTransactionParent::AllocPLayerParent()
{
  return new ShadowLayerParent();
}

bool
LayerTransactionParent::DeallocPLayerParent(PLayerParent* actor)
{
  delete actor;
  return true;
}

PCompositableParent*
LayerTransactionParent::AllocPCompositableParent(const TextureInfo& aInfo)
{
  return CompositableHost::CreateIPDLActor(this, aInfo, 0);
}

bool
LayerTransactionParent::DeallocPCompositableParent(PCompositableParent* aActor)
{
  return CompositableHost::DestroyIPDLActor(aActor);
}

void
LayerTransactionParent::ActorDestroy(ActorDestroyReason why)
{
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

void
LayerTransactionParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

void
LayerTransactionParent::SendPendingAsyncMessages()
{
  mShadowLayersManager->AsCompositorBridgeParentIPCAllocator()->SendPendingAsyncMessages();
}

void
LayerTransactionParent::SetAboutToSendAsyncMessages()
{
  mShadowLayersManager->AsCompositorBridgeParentIPCAllocator()->SetAboutToSendAsyncMessages();
}

void
LayerTransactionParent::NotifyNotUsed(PTextureParent* aTexture, uint64_t aTransactionId)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
}

} // namespace layers
} // namespace mozilla
