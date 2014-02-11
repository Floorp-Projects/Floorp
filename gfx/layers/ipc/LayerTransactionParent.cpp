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
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
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
#include "mozilla/layers/ThebesLayerComposite.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsCoord.h"                    // for NSAppUnitsToFloatPixels
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsISupportsImpl.h"            // for Layer::Release, etc
#include "nsLayoutUtils.h"              // for nsLayoutUtils
#include "nsMathUtils.h"                // for NS_round
#include "nsPoint.h"                    // for nsPoint
#include "nsTArray.h"                   // for nsTArray, nsTArray_Impl, etc
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc
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

static CompositableParent*
cast(const PCompositableParent* in)
{
  return const_cast<CompositableParent*>(
    static_cast<const CompositableParent*>(in));
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
ShadowContainer(const OpAppendChild& op)
{
  return cast(op.containerParent());
}
static ShadowLayerParent*
ShadowChild(const OpAppendChild& op)
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

/* virtual */
bool
LayerTransactionParent::RecvUpdateNoSwap(const InfallibleTArray<Edit>& cset,
                                         const TargetConfig& targetConfig,
                                         const bool& isFirstPaint,
                                         const bool& scheduleComposite)
{
  return RecvUpdate(cset, targetConfig, isFirstPaint, scheduleComposite, nullptr);
}

bool
LayerTransactionParent::RecvUpdate(const InfallibleTArray<Edit>& cset,
                                   const TargetConfig& targetConfig,
                                   const bool& isFirstPaint,
                                   const bool& scheduleComposite,
                                   InfallibleTArray<EditReply>* reply)
{
  profiler_tracing("Paint", "Composite", TRACING_INTERVAL_START);
  PROFILER_LABEL("LayerTransactionParent", "RecvUpdate");
#ifdef COMPOSITOR_PERFORMANCE_WARNING
  TimeStamp updateStart = TimeStamp::Now();
#endif

  MOZ_LAYERS_LOG(("[ParentSide] received txn with %d edits", cset.Length()));

  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    return true;
  }

  EditReplyVector replyv;

  {
    AutoResolveRefLayers resolve(mShadowLayersManager->GetCompositionManager());
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
      Layer* layer = AsLayerComposite(osla)->AsLayer();
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

        ThebesLayerComposite* thebesLayer =
          static_cast<ThebesLayerComposite*>(layer);
        const ThebesLayerAttributes& attrs =
          specific.get_ThebesLayerAttributes();

        thebesLayer->SetValidRegion(attrs.validRegion());

        break;
      }
      case Specific::TContainerLayerAttributes: {
        MOZ_LAYERS_LOG(("[ParentSide]   container layer"));

        ContainerLayer* containerLayer =
          static_cast<ContainerLayer*>(layer);
        const ContainerLayerAttributes& attrs =
          specific.get_ContainerLayerAttributes();
        containerLayer->SetFrameMetrics(attrs.metrics());
        containerLayer->SetPreScale(attrs.preXScale(), attrs.preYScale());
        containerLayer->SetInheritedScale(attrs.inheritedXScale(), attrs.inheritedYScale());
        break;
      }
      case Specific::TColorLayerAttributes:
        MOZ_LAYERS_LOG(("[ParentSide]   color layer"));

        static_cast<ColorLayer*>(layer)->SetColor(
          specific.get_ColorLayerAttributes().color().value());
        static_cast<ColorLayer*>(layer)->SetBounds(
          specific.get_ColorLayerAttributes().bounds());
        break;

      case Specific::TCanvasLayerAttributes:
        MOZ_LAYERS_LOG(("[ParentSide]   canvas layer"));

        static_cast<CanvasLayer*>(layer)->SetFilter(
          specific.get_CanvasLayerAttributes().filter());
        static_cast<CanvasLayerComposite*>(layer)->SetBounds(
          specific.get_CanvasLayerAttributes().bounds());
        break;

      case Specific::TRefLayerAttributes:
        MOZ_LAYERS_LOG(("[ParentSide]   ref layer"));

        static_cast<RefLayer*>(layer)->SetReferentId(
          specific.get_RefLayerAttributes().id());
        break;

      case Specific::TImageLayerAttributes: {
        MOZ_LAYERS_LOG(("[ParentSide]   image layer"));

        ImageLayer* imageLayer = static_cast<ImageLayer*>(layer);
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

      mRoot = AsLayerComposite(edit.get_OpSetRoot())->AsLayer();
      break;
    }
    case Edit::TOpInsertAfter: {
      MOZ_LAYERS_LOG(("[ParentSide] InsertAfter"));

      const OpInsertAfter& oia = edit.get_OpInsertAfter();
      ShadowContainer(oia)->AsContainer()->InsertAfter(
        ShadowChild(oia)->AsLayer(), ShadowAfter(oia)->AsLayer());
      break;
    }
    case Edit::TOpAppendChild: {
      MOZ_LAYERS_LOG(("[ParentSide] AppendChild"));

      const OpAppendChild& oac = edit.get_OpAppendChild();
      ShadowContainer(oac)->AsContainer()->InsertAfter(
        ShadowChild(oac)->AsLayer(), nullptr);
      break;
    }
    case Edit::TOpRemoveChild: {
      MOZ_LAYERS_LOG(("[ParentSide] RemoveChild"));

      const OpRemoveChild& orc = edit.get_OpRemoveChild();
      Layer* childLayer = ShadowChild(orc)->AsLayer();
      ShadowContainer(orc)->AsContainer()->RemoveChild(childLayer);
      break;
    }
    case Edit::TOpRepositionChild: {
      MOZ_LAYERS_LOG(("[ParentSide] RepositionChild"));

      const OpRepositionChild& orc = edit.get_OpRepositionChild();
      ShadowContainer(orc)->AsContainer()->RepositionChild(
        ShadowChild(orc)->AsLayer(), ShadowAfter(orc)->AsLayer());
      break;
    }
    case Edit::TOpRaiseToTopChild: {
      MOZ_LAYERS_LOG(("[ParentSide] RaiseToTopChild"));

      const OpRaiseToTopChild& rtc = edit.get_OpRaiseToTopChild();
      ShadowContainer(rtc)->AsContainer()->RepositionChild(
        ShadowChild(rtc)->AsLayer(), nullptr);
      break;
    }
    case Edit::TCompositableOperation: {
      ReceiveCompositableUpdate(edit.get_CompositableOperation(),
                                replyv);
      break;
    }
    case Edit::TOpAttachCompositable: {
      const OpAttachCompositable& op = edit.get_OpAttachCompositable();
      Attach(cast(op.layerParent()), cast(op.compositableParent()), false);
      cast(op.compositableParent())->SetCompositorID(
        mLayerManager->GetCompositor()->GetCompositorID());
      break;
    }
    case Edit::TOpAttachAsyncCompositable: {
      const OpAttachAsyncCompositable& op = edit.get_OpAttachAsyncCompositable();
      CompositableParent* compositableParent = CompositableMap::Get(op.containerID());
      MOZ_ASSERT(compositableParent, "CompositableParent not found in the map");
      Attach(cast(op.layerParent()), compositableParent, true);
      compositableParent->SetCompositorID(mLayerManager->GetCompositor()->GetCompositorID());
      break;
    }
    default:
      NS_RUNTIMEABORT("not reached");
    }
  }

  {
    AutoResolveRefLayers resolve(mShadowLayersManager->GetCompositionManager());
    layer_manager()->EndTransaction(nullptr, nullptr, LayerManager::END_NO_IMMEDIATE_REDRAW);
  }

  if (reply) {
    reply->SetCapacity(replyv.size());
    if (replyv.size() > 0) {
      reply->AppendElements(&replyv.front(), replyv.size());
    }
  }

  // Ensure that any pending operations involving back and front
  // buffers have completed, so that neither process stomps on the
  // other's buffer contents.
  LayerManagerComposite::PlatformSyncBeforeReplyUpdate();

  mShadowLayersManager->ShadowLayersUpdated(this, targetConfig, isFirstPaint, scheduleComposite);

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  int compositeTime = (int)(mozilla::TimeStamp::Now() - updateStart).ToMilliseconds();
  if (compositeTime > 15) {
    printf_stderr("Compositor: Layers update took %i ms (blocking gecko).\n", compositeTime);
  }
#endif

  return true;
}

bool
LayerTransactionParent::RecvGetOpacity(PLayerParent* aParent,
                                       float* aOpacity)
{
  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    return false;
  }

  *aOpacity = cast(aParent)->AsLayer()->GetLocalOpacity();
  return true;
}

bool
LayerTransactionParent::RecvGetTransform(PLayerParent* aParent,
                                         gfx3DMatrix* aTransform)
{
  if (mDestroyed || !layer_manager() || layer_manager()->IsDestroyed()) {
    return false;
  }

  // The following code recovers the untranslated transform
  // from the shadow transform by undoing the translations in
  // AsyncCompositionManager::SampleValue.
  Layer* layer = cast(aParent)->AsLayer();
  gfx::To3DMatrix(layer->AsLayerComposite()->GetShadowTransform(), *aTransform);
  if (ContainerLayer* c = layer->AsContainerLayer()) {
    aTransform->ScalePost(1.0f/c->GetInheritedXScale(),
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
      transformOrigin = data.transformOrigin();
      break;
    }
  }

  aTransform->Translate(-scaledOrigin);
  *aTransform = nsLayoutUtils::ChangeMatrixBasis(-scaledOrigin - transformOrigin, *aTransform);
  return true;
}

void
LayerTransactionParent::Attach(ShadowLayerParent* aLayerParent,
                               CompositableParent* aCompositable,
                               bool aIsAsyncVideo)
{
  LayerComposite* layer = aLayerParent->AsLayer()->AsLayerComposite();
  MOZ_ASSERT(layer);

  Compositor* compositor
    = static_cast<LayerManagerComposite*>(aLayerParent->AsLayer()->Manager())->GetCompositor();

  CompositableHost* compositable = aCompositable->GetCompositableHost();
  MOZ_ASSERT(compositable);
  layer->SetCompositableHost(compositable);
  compositable->Attach(aLayerParent->AsLayer(),
                       compositor,
                       aIsAsyncVideo
                         ? CompositableHost::ALLOW_REATTACH
                           | CompositableHost::KEEP_ATTACHED
                         : CompositableHost::NO_FLAGS);
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

PGrallocBufferParent*
LayerTransactionParent::AllocPGrallocBufferParent(const IntSize& aSize,
                                                  const uint32_t& aFormat,
                                                  const uint32_t& aUsage,
                                                  MaybeMagicGrallocBufferHandle* aOutHandle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  return GrallocBufferActor::Create(aSize, aFormat, aUsage, aOutHandle);
#else
  NS_RUNTIMEABORT("No gralloc buffers for you");
  return nullptr;
#endif
}

bool
LayerTransactionParent::DeallocPGrallocBufferParent(PGrallocBufferParent* actor)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  delete actor;
  return true;
#else
  NS_RUNTIMEABORT("Um, how did we get here?");
  return false;
#endif
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
  return new CompositableParent(this, aInfo);
}

bool
LayerTransactionParent::DeallocPCompositableParent(PCompositableParent* actor)
{
  delete actor;
  return true;
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

} // namespace layers
} // namespace mozilla
