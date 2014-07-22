/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerTransactionParent.h"
#include <vector>                       // for vector
#include "CompositableHost.h"           // for CompositableParent, Get, etc
#include "ImageLayers.h"                // for ImageLayer
#include "Layers.h"                     // for Layer, ContainerLayer, etc
#include "ShadowLayerParent.h"          // for ShadowLayerParent
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxPoint3D.h"                 // for gfxPoint3D
#include "CompositableTransactionParent.h"  // for EditReplyVector
#include "ShadowLayersManager.h"        // for ShadowLayersManager
#include "mozilla/gfx/BasePoint3D.h"    // for BasePoint3D
#include "mozilla/layers/CanvasLayerComposite.h"
#include "mozilla/layers/ColorLayerComposite.h"
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/ContainerLayerComposite.h"
#include "mozilla/layers/ImageLayerComposite.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply, etc
#include "mozilla/layers/LayersSurfaces.h"  // for PGrallocBufferParent
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/PCompositableParent.h"
#include "mozilla/layers/PLayerParent.h"  // for PLayerParent
#include "mozilla/layers/TextureHostOGL.h"  // for TextureHostOGL
#include "mozilla/layers/ThebesLayerComposite.h"
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
#include "GeckoProfiler.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/layers/AsyncCompositionManager.h"
#include "mozilla/layers/AsyncPanZoomController.h"

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
                                               uint64_t aId,
                                               ProcessId aOtherProcess)
  : mLayerManager(aManager)
  , mShadowLayersManager(aLayersManager)
  , mId(aId)
  , mPendingTransaction(0)
  , mChildProcessId(aOtherProcess)
  , mDestroyed(false)
  , mIPCOpen(false)
{
  MOZ_COUNT_CTOR(LayerTransactionParent);
}

LayerTransactionParent::~LayerTransactionParent()
{
  MOZ_COUNT_DTOR(LayerTransactionParent);
}

void
LayerTransactionParent::Destroy()
{
  mDestroyed = true;
  for (size_t i = 0; i < ManagedPLayerParent().Length(); ++i) {
    ShadowLayerParent* slp =
      static_cast<ShadowLayerParent*>(ManagedPLayerParent()[i]);
    slp->Destroy();
  }
}

LayersBackend
LayerTransactionParent::GetCompositorBackendType() const
{
  return mLayerManager->GetBackendType();
}

bool
LayerTransactionParent::RecvUpdateNoSwap(const InfallibleTArray<Edit>& cset,
                                         const uint64_t& aTransactionId,
                                         const TargetConfig& targetConfig,
                                         const bool& isFirstPaint,
                                         const bool& scheduleComposite,
                                         const uint32_t& paintSequenceNumber)
{
  return RecvUpdate(cset, aTransactionId, targetConfig, isFirstPaint,
                    scheduleComposite, paintSequenceNumber, nullptr);
}

bool
LayerTransactionParent::RecvUpdate(const InfallibleTArray<Edit>& cset,
                                   const uint64_t& aTransactionId,
                                   const TargetConfig& targetConfig,
                                   const bool& isFirstPaint,
                                   const bool& scheduleComposite,
                                   const uint32_t& paintSequenceNumber,
                                   InfallibleTArray<EditReply>* reply)
{
  profiler_tracing("Paint", "Composite", TRACING_INTERVAL_START);
  PROFILER_LABEL("LayerTransactionParent", "RecvUpdate",
    js::ProfileEntry::Category::GRAPHICS);

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp updateStart = TimeStamp::Now();
#endif

  MOZ_LAYERS_LOG(("[ParentSide] received txn with %d edits", cset.Length()));

  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    return true;
  }

  if (mLayerManager && mLayerManager->GetCompositor() &&
      !targetConfig.naturalBounds().IsEmpty()) {
    mLayerManager->GetCompositor()->SetScreenRotation(targetConfig.rotation());
  }

  EditReplyVector replyv;

  {
    AutoResolveRefLayers resolve(mShadowLayersManager->GetCompositionManager(this));
    layer_manager()->BeginTransaction();
  }

  for (EditArray::index_type i = 0; i < cset.Length(); ++i) {
    const Edit& edit = cset[i];

    switch (edit.type()) {
    // Create* ops
    case Edit::TOpCreateThebesLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateThebesLayer"));

      nsRefPtr<ThebesLayerComposite> layer =
        layer_manager()->CreateThebesLayerComposite();
      AsLayerComposite(edit.get_OpCreateThebesLayer())->Bind(layer);
      break;
    }
    case Edit::TOpCreateContainerLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateContainerLayer"));

      nsRefPtr<ContainerLayer> layer = layer_manager()->CreateContainerLayerComposite();
      AsLayerComposite(edit.get_OpCreateContainerLayer())->Bind(layer);
      break;
    }
    case Edit::TOpCreateImageLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateImageLayer"));

      nsRefPtr<ImageLayerComposite> layer =
        layer_manager()->CreateImageLayerComposite();
      AsLayerComposite(edit.get_OpCreateImageLayer())->Bind(layer);
      break;
    }
    case Edit::TOpCreateColorLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateColorLayer"));

      nsRefPtr<ColorLayerComposite> layer = layer_manager()->CreateColorLayerComposite();
      AsLayerComposite(edit.get_OpCreateColorLayer())->Bind(layer);
      break;
    }
    case Edit::TOpCreateCanvasLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateCanvasLayer"));

      nsRefPtr<CanvasLayerComposite> layer =
        layer_manager()->CreateCanvasLayerComposite();
      AsLayerComposite(edit.get_OpCreateCanvasLayer())->Bind(layer);
      break;
    }
    case Edit::TOpCreateRefLayer: {
      MOZ_LAYERS_LOG(("[ParentSide] CreateRefLayer"));

      nsRefPtr<RefLayerComposite> layer =
        layer_manager()->CreateRefLayerComposite();
      AsLayerComposite(edit.get_OpCreateRefLayer())->Bind(layer);
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
      layer->SetVisibleRegion(common.visibleRegion());
      layer->SetEventRegions(common.eventRegions());
      layer->SetContentFlags(common.contentFlags());
      layer->SetOpacity(common.opacity());
      layer->SetClipRect(common.useClipRect() ? &common.clipRect() : nullptr);
      layer->SetBaseTransform(common.transform().value());
      layer->SetPostScale(common.postXScale(), common.postYScale());
      layer->SetIsFixedPosition(common.isFixedPosition());
      layer->SetFixedPositionAnchor(common.fixedPositionAnchor());
      layer->SetFixedPositionMargins(common.fixedPositionMargin());
      if (common.isStickyPosition()) {
        layer->SetStickyPositionData(common.stickyScrollContainerId(),
                                     common.stickyScrollRangeOuter(),
                                     common.stickyScrollRangeInner());
      }
      layer->SetScrollbarData(common.scrollbarTargetContainerId(),
        static_cast<Layer::ScrollDirection>(common.scrollbarDirection()));
      layer->SetMixBlendMode((gfx::CompositionOp)common.mixBlendMode());
      layer->SetForceIsolatedGroup(common.forceIsolatedGroup());
      if (PLayerParent* maskLayer = common.maskLayerParent()) {
        layer->SetMaskLayer(cast(maskLayer)->AsLayer());
      } else {
        layer->SetMaskLayer(nullptr);
      }
      layer->SetAnimations(common.animations());
      layer->SetInvalidRegion(common.invalidRegion());

      typedef SpecificLayerAttributes Specific;
      const SpecificLayerAttributes& specific = attrs.specific();
      switch (specific.type()) {
      case Specific::Tnull_t:
        break;

      case Specific::TThebesLayerAttributes: {
        MOZ_LAYERS_LOG(("[ParentSide]   thebes layer"));

        ThebesLayerComposite* thebesLayer = layerParent->AsThebesLayerComposite();
        if (!thebesLayer) {
          return false;
        }
        const ThebesLayerAttributes& attrs =
          specific.get_ThebesLayerAttributes();

        thebesLayer->SetValidRegion(attrs.validRegion());

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
        containerLayer->SetFrameMetrics(attrs.metrics());
        containerLayer->SetScrollHandoffParentId(attrs.scrollParentId());
        containerLayer->SetPreScale(attrs.preXScale(), attrs.preYScale());
        containerLayer->SetInheritedScale(attrs.inheritedXScale(), attrs.inheritedYScale());
        containerLayer->SetBackgroundColor(attrs.backgroundColor().value());
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
        canvasLayer->SetFilter(specific.get_CanvasLayerAttributes().filter());
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
        break;
      }
      case Specific::TImageLayerAttributes: {
        MOZ_LAYERS_LOG(("[ParentSide]   image layer"));

        ImageLayerComposite* imageLayer = layerParent->AsImageLayerComposite();
        if (!imageLayer) {
          return false;
        }
        const ImageLayerAttributes& attrs = specific.get_ImageLayerAttributes();
        imageLayer->SetFilter(attrs.filter());
        imageLayer->SetScaleToSize(attrs.scaleToSize(), attrs.scaleMode());
        break;
      }
      default:
        NS_RUNTIMEABORT("not reached");
      }
      break;
    }
    case Edit::TOpSetDiagnosticTypes: {
      mLayerManager->GetCompositor()->SetDiagnosticTypes(
        edit.get_OpSetDiagnosticTypes().diagnostics());
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
                                            isFirstPaint, scheduleComposite, paintSequenceNumber);

  {
    AutoResolveRefLayers resolve(mShadowLayersManager->GetCompositionManager(this));
    layer_manager()->EndTransaction(nullptr, nullptr, LayerManager::END_NO_IMMEDIATE_REDRAW);
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

  gfx3DMatrix transform = gfx::To3DMatrix(layer->AsLayerComposite()->GetShadowTransform());
  if (ContainerLayer* c = layer->AsContainerLayer()) {
    // Undo the scale transform applied by AsyncCompositionManager::SampleValue
    transform.ScalePost(1.0f/c->GetInheritedXScale(),
                        1.0f/c->GetInheritedYScale(),
                        1.0f);
  }
  float scale = 1;
  gfxPoint3D scaledOrigin;
  gfxPoint3D transformOrigin;
  for (uint32_t i=0; i < layer->GetAnimations().Length(); i++) {
    if (layer->GetAnimations()[i].data().type() == AnimationData::TTransformData) {
      const TransformData& data = layer->GetAnimations()[i].data().get_TransformData();
      scale = data.appUnitsPerDevPixel();
      scaledOrigin =
        gfxPoint3D(NS_round(NSAppUnitsToFloatPixels(data.origin().x, scale)),
                   NS_round(NSAppUnitsToFloatPixels(data.origin().y, scale)),
                   0.0f);
      double cssPerDev =
        double(nsDeviceContext::AppUnitsPerCSSPixel()) / double(scale);
      transformOrigin = data.transformOrigin() * cssPerDev;
      break;
    }
  }

  // Undo the translation to the origin of the reference frame applied by
  // AsyncCompositionManager::SampleValue
  transform.Translate(-scaledOrigin);

  // Undo the rebasing applied by
  // nsDisplayTransform::GetResultingTransformMatrixInternal
  transform = nsLayoutUtils::ChangeMatrixBasis(-scaledOrigin - transformOrigin,
                                               transform);

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

bool
LayerTransactionParent::RecvSetAsyncScrollOffset(PLayerParent* aLayer,
                                                 const int32_t& aX, const int32_t& aY)
{
  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    return false;
  }

  Layer* layer = cast(aLayer)->AsLayer();
  if (!layer) {
    return false;
  }
  ContainerLayer* containerLayer = layer->AsContainerLayer();
  if (!containerLayer) {
    return false;
  }
  AsyncPanZoomController* controller = containerLayer->GetAsyncPanZoomController();
  if (!controller) {
    return false;
  }
  controller->SetTestAsyncScrollOffset(CSSPoint(aX, aY));
  return true;
}

bool
LayerTransactionParent::RecvGetAPZTestData(APZTestData* aOutData)
{
  mShadowLayersManager->GetAPZTestData(this, aOutData);
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

PTextureParent*
LayerTransactionParent::AllocPTextureParent(const SurfaceDescriptor& aSharedData,
                                            const TextureFlags& aFlags)
{
  return TextureHost::CreateIPDLActor(this, aSharedData, aFlags);
}

bool
LayerTransactionParent::DeallocPTextureParent(PTextureParent* actor)
{
  return TextureHost::DestroyIPDLActor(actor);
}

bool
LayerTransactionParent::RecvChildAsyncMessages(const InfallibleTArray<AsyncChildMessageData>& aMessages)
{
  for (AsyncChildMessageArray::index_type i = 0; i < aMessages.Length(); ++i) {
    const AsyncChildMessageData& message = aMessages[i];

    switch (message.type()) {
      case AsyncChildMessageData::TOpDeliverFenceFromChild: {
        const OpDeliverFenceFromChild& op = message.get_OpDeliverFenceFromChild();
#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
        FenceHandle fence = FenceHandle(op.fence());
        PTextureParent* parent = op.textureParent();

        TextureHostOGL* hostOGL = nullptr;
        RefPtr<TextureHost> texture = TextureHost::AsTextureHost(parent);
        if (texture) {
          hostOGL = texture->AsHostOGL();
        }
        if (hostOGL) {
          hostOGL->SetAcquireFence(fence.mFence);
        }
#endif
        // Send back a response.
        InfallibleTArray<AsyncParentMessageData> replies;
        replies.AppendElement(OpReplyDeliverFence(op.transactionId()));
        mozilla::unused << SendParentAsyncMessages(replies);
        break;
      }
      case AsyncChildMessageData::TOpReplyDeliverFence: {
        const OpReplyDeliverFence& op = message.get_OpReplyDeliverFence();
        TransactionCompleteted(op.transactionId());
        break;
      }
      default:
        NS_ERROR("unknown AsyncChildMessageData type");
        return false;
    }
  }
  return true;
}

void
LayerTransactionParent::ActorDestroy(ActorDestroyReason why)
{
  DestroyAsyncTransactionTrackersHolder();
}

bool LayerTransactionParent::IsSameProcess() const
{
  return OtherProcess() == ipc::kInvalidProcessHandle;
}

void
LayerTransactionParent::SendFenceHandle(AsyncTransactionTracker* aTracker,
                                        PTextureParent* aTexture,
                                        const FenceHandle& aFence)
{
  HoldUntilComplete(aTracker);
  InfallibleTArray<AsyncParentMessageData> messages;
  messages.AppendElement(OpDeliverFence(aTracker->GetId(),
                                        aTexture, nullptr,
                                        aFence));
  mozilla::unused << SendParentAsyncMessages(messages);
}

void
LayerTransactionParent::SendAsyncMessage(const InfallibleTArray<AsyncParentMessageData>& aMessage)
{
  mozilla::unused << SendParentAsyncMessages(aMessage);
}

} // namespace layers
} // namespace mozilla
