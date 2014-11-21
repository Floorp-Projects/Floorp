/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Layers.h"
#include <algorithm>                    // for max, min
#include "apz/src/AsyncPanZoomController.h"
#include "CompositableHost.h"           // for CompositableHost
#include "ImageContainer.h"             // for ImageContainer, etc
#include "ImageLayers.h"                // for ImageLayer
#include "LayerSorter.h"                // for SortLayersBy3DZOrder
#include "LayersLogging.h"              // for AppendToString
#include "ReadbackLayer.h"              // for ReadbackLayer
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "gfx2DGlue.h"
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "mozilla/Telemetry.h"          // for Accumulate
#include "mozilla/dom/AnimationPlayer.h" // for ComputedTimingFunction
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/layers/LayersMessages.h"  // for TransformFunction, etc
#include "nsAString.h"
#include "nsCSSValue.h"                 // for nsCSSValue::Array, etc
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsStyleStruct.h"              // for nsTimingFunction, etc
#include "protobuf/LayerScopePacket.pb.h"

uint8_t gLayerManagerLayerBuilder;

namespace mozilla {
namespace layers {

FILE*
FILEOrDefault(FILE* aFile)
{
  return aFile ? aFile : stderr;
}

typedef FrameMetrics::ViewID ViewID;
const ViewID FrameMetrics::NULL_SCROLL_ID = 0;
const FrameMetrics FrameMetrics::sNullMetrics;

using namespace mozilla::gfx;

//--------------------------------------------------
// LayerManager
FrameMetrics::ViewID
LayerManager::GetRootScrollableLayerId()
{
  if (!mRoot) {
    return FrameMetrics::NULL_SCROLL_ID;
  }

  nsTArray<LayerMetricsWrapper> queue;
  queue.AppendElement(LayerMetricsWrapper(mRoot));
  while (queue.Length()) {
    LayerMetricsWrapper layer = queue[0];
    queue.RemoveElementAt(0);

    const FrameMetrics& frameMetrics = layer.Metrics();
    if (frameMetrics.IsScrollable()) {
      return frameMetrics.GetScrollId();
    }

    LayerMetricsWrapper child = layer.GetFirstChild();
    while (child) {
      queue.AppendElement(child);
      child = child.GetNextSibling();
    }
  }

  return FrameMetrics::NULL_SCROLL_ID;
}

void
LayerManager::GetRootScrollableLayers(nsTArray<Layer*>& aArray)
{
  if (!mRoot) {
    return;
  }

  FrameMetrics::ViewID rootScrollableId = GetRootScrollableLayerId();
  if (rootScrollableId == FrameMetrics::NULL_SCROLL_ID) {
    aArray.AppendElement(mRoot);
    return;
  }

  nsTArray<Layer*> queue;
  queue.AppendElement(mRoot);
  while (queue.Length()) {
    Layer* layer = queue[0];
    queue.RemoveElementAt(0);

    if (LayerMetricsWrapper::TopmostScrollableMetrics(layer).GetScrollId() == rootScrollableId) {
      aArray.AppendElement(layer);
      continue;
    }

    for (Layer* child = layer->GetFirstChild(); child; child = child->GetNextSibling()) {
      queue.AppendElement(child);
    }
  }
}

void
LayerManager::GetScrollableLayers(nsTArray<Layer*>& aArray)
{
  if (!mRoot) {
    return;
  }

  nsTArray<Layer*> queue;
  queue.AppendElement(mRoot);
  while (!queue.IsEmpty()) {
    Layer* layer = queue.LastElement();
    queue.RemoveElementAt(queue.Length() - 1);

    if (layer->HasScrollableFrameMetrics()) {
      aArray.AppendElement(layer);
      continue;
    }

    for (Layer* child = layer->GetFirstChild(); child; child = child->GetNextSibling()) {
      queue.AppendElement(child);
    }
  }
}

TemporaryRef<DrawTarget>
LayerManager::CreateOptimalDrawTarget(const gfx::IntSize &aSize,
                                      SurfaceFormat aFormat)
{
  return gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(aSize,
                                                                      aFormat);
}

TemporaryRef<DrawTarget>
LayerManager::CreateOptimalMaskDrawTarget(const gfx::IntSize &aSize)
{
  return CreateOptimalDrawTarget(aSize, SurfaceFormat::A8);
}

TemporaryRef<DrawTarget>
LayerManager::CreateDrawTarget(const IntSize &aSize,
                               SurfaceFormat aFormat)
{
  return gfxPlatform::GetPlatform()->
    CreateOffscreenCanvasDrawTarget(aSize, aFormat);
}

#ifdef DEBUG
void
LayerManager::Mutated(Layer* aLayer)
{
}
#endif  // DEBUG

already_AddRefed<ImageContainer>
LayerManager::CreateImageContainer()
{
  nsRefPtr<ImageContainer> container = new ImageContainer(ImageContainer::DISABLE_ASYNC);
  return container.forget();
}

already_AddRefed<ImageContainer>
LayerManager::CreateAsynchronousImageContainer()
{
  nsRefPtr<ImageContainer> container = new ImageContainer(ImageContainer::ENABLE_ASYNC);
  return container.forget();
}

bool
LayerManager::AreComponentAlphaLayersEnabled()
{
  return gfxPrefs::ComponentAlphaEnabled();
}

//--------------------------------------------------
// Layer

Layer::Layer(LayerManager* aManager, void* aImplData) :
  mManager(aManager),
  mParent(nullptr),
  mNextSibling(nullptr),
  mPrevSibling(nullptr),
  mImplData(aImplData),
  mMaskLayer(nullptr),
  mPostXScale(1.0f),
  mPostYScale(1.0f),
  mOpacity(1.0),
  mMixBlendMode(CompositionOp::OP_OVER),
  mForceIsolatedGroup(false),
  mContentFlags(0),
  mUseClipRect(false),
  mUseTileSourceRect(false),
  mIsFixedPosition(false),
  mMargins(0, 0, 0, 0),
  mStickyPositionData(nullptr),
  mScrollbarTargetId(FrameMetrics::NULL_SCROLL_ID),
  mScrollbarDirection(ScrollDirection::NONE),
  mDebugColorIndex(0),
  mAnimationGeneration(0)
{}

Layer::~Layer()
{}

Animation*
Layer::AddAnimation()
{
  MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) AddAnimation", this));

  MOZ_ASSERT(!mPendingAnimations, "should have called ClearAnimations first");

  Animation* anim = mAnimations.AppendElement();

  Mutated();
  return anim;
}

void
Layer::ClearAnimations()
{
  mPendingAnimations = nullptr;

  if (mAnimations.IsEmpty() && mAnimationData.IsEmpty()) {
    return;
  }

  MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) ClearAnimations", this));
  mAnimations.Clear();
  mAnimationData.Clear();
  Mutated();
}

Animation*
Layer::AddAnimationForNextTransaction()
{
  MOZ_ASSERT(mPendingAnimations,
             "should have called ClearAnimationsForNextTransaction first");

  Animation* anim = mPendingAnimations->AppendElement();

  return anim;
}

void
Layer::ClearAnimationsForNextTransaction()
{
  // Ensure we have a non-null mPendingAnimations to mark a future clear.
  if (!mPendingAnimations) {
    mPendingAnimations = new AnimationArray;
  }

  mPendingAnimations->Clear();
}

static nsCSSValueSharedList*
CreateCSSValueList(const InfallibleTArray<TransformFunction>& aFunctions)
{
  nsAutoPtr<nsCSSValueList> result;
  nsCSSValueList** resultTail = getter_Transfers(result);
  for (uint32_t i = 0; i < aFunctions.Length(); i++) {
    nsRefPtr<nsCSSValue::Array> arr;
    switch (aFunctions[i].type()) {
      case TransformFunction::TRotationX:
      {
        float theta = aFunctions[i].get_RotationX().radians();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotatex,
                                                           resultTail);
        arr->Item(1).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TRotationY:
      {
        float theta = aFunctions[i].get_RotationY().radians();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotatey,
                                                           resultTail);
        arr->Item(1).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TRotationZ:
      {
        float theta = aFunctions[i].get_RotationZ().radians();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotatez,
                                                           resultTail);
        arr->Item(1).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TRotation:
      {
        float theta = aFunctions[i].get_Rotation().radians();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotate,
                                                           resultTail);
        arr->Item(1).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TRotation3D:
      {
        float x = aFunctions[i].get_Rotation3D().x();
        float y = aFunctions[i].get_Rotation3D().y();
        float z = aFunctions[i].get_Rotation3D().z();
        float theta = aFunctions[i].get_Rotation3D().radians();
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotate3d,
                                                       resultTail);
        arr->Item(1).SetFloatValue(x, eCSSUnit_Number);
        arr->Item(2).SetFloatValue(y, eCSSUnit_Number);
        arr->Item(3).SetFloatValue(z, eCSSUnit_Number);
        arr->Item(4).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TScale:
      {
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_scale3d,
                                                       resultTail);
        arr->Item(1).SetFloatValue(aFunctions[i].get_Scale().x(), eCSSUnit_Number);
        arr->Item(2).SetFloatValue(aFunctions[i].get_Scale().y(), eCSSUnit_Number);
        arr->Item(3).SetFloatValue(aFunctions[i].get_Scale().z(), eCSSUnit_Number);
        break;
      }
      case TransformFunction::TTranslation:
      {
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_translate3d,
                                                       resultTail);
        arr->Item(1).SetFloatValue(aFunctions[i].get_Translation().x(), eCSSUnit_Pixel);
        arr->Item(2).SetFloatValue(aFunctions[i].get_Translation().y(), eCSSUnit_Pixel);
        arr->Item(3).SetFloatValue(aFunctions[i].get_Translation().z(), eCSSUnit_Pixel);
        break;
      }
      case TransformFunction::TSkewX:
      {
        float x = aFunctions[i].get_SkewX().x();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_skewx,
                                                           resultTail);
        arr->Item(1).SetFloatValue(x, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TSkewY:
      {
        float y = aFunctions[i].get_SkewY().y();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_skewy,
                                                           resultTail);
        arr->Item(1).SetFloatValue(y, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TSkew:
      {
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_skew,
                                                           resultTail);
        arr->Item(1).SetFloatValue(aFunctions[i].get_Skew().x(), eCSSUnit_Radian);
        arr->Item(2).SetFloatValue(aFunctions[i].get_Skew().y(), eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TTransformMatrix:
      {
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_matrix3d,
                                                       resultTail);
        const gfx::Matrix4x4& matrix = aFunctions[i].get_TransformMatrix().value();
        arr->Item(1).SetFloatValue(matrix._11, eCSSUnit_Number);
        arr->Item(2).SetFloatValue(matrix._12, eCSSUnit_Number);
        arr->Item(3).SetFloatValue(matrix._13, eCSSUnit_Number);
        arr->Item(4).SetFloatValue(matrix._14, eCSSUnit_Number);
        arr->Item(5).SetFloatValue(matrix._21, eCSSUnit_Number);
        arr->Item(6).SetFloatValue(matrix._22, eCSSUnit_Number);
        arr->Item(7).SetFloatValue(matrix._23, eCSSUnit_Number);
        arr->Item(8).SetFloatValue(matrix._24, eCSSUnit_Number);
        arr->Item(9).SetFloatValue(matrix._31, eCSSUnit_Number);
        arr->Item(10).SetFloatValue(matrix._32, eCSSUnit_Number);
        arr->Item(11).SetFloatValue(matrix._33, eCSSUnit_Number);
        arr->Item(12).SetFloatValue(matrix._34, eCSSUnit_Number);
        arr->Item(13).SetFloatValue(matrix._41, eCSSUnit_Number);
        arr->Item(14).SetFloatValue(matrix._42, eCSSUnit_Number);
        arr->Item(15).SetFloatValue(matrix._43, eCSSUnit_Number);
        arr->Item(16).SetFloatValue(matrix._44, eCSSUnit_Number);
        break;
      }
      case TransformFunction::TPerspective:
      {
        float perspective = aFunctions[i].get_Perspective().value();
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_perspective,
                                                       resultTail);
        arr->Item(1).SetFloatValue(perspective, eCSSUnit_Pixel);
        break;
      }
      default:
        NS_ASSERTION(false, "All functions should be implemented?");
    }
  }
  if (aFunctions.Length() == 0) {
    result = new nsCSSValueList();
    result->mValue.SetNoneValue();
  }
  return new nsCSSValueSharedList(result.forget());
}

void
Layer::SetAnimations(const AnimationArray& aAnimations)
{
  MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) SetAnimations", this));

  mAnimations = aAnimations;
  mAnimationData.Clear();
  for (uint32_t i = 0; i < mAnimations.Length(); i++) {
    AnimData* data = mAnimationData.AppendElement();
    InfallibleTArray<nsAutoPtr<ComputedTimingFunction> >& functions =
      data->mFunctions;
    const InfallibleTArray<AnimationSegment>& segments =
      mAnimations.ElementAt(i).segments();
    for (uint32_t j = 0; j < segments.Length(); j++) {
      TimingFunction tf = segments.ElementAt(j).sampleFn();
      ComputedTimingFunction* ctf = new ComputedTimingFunction();
      switch (tf.type()) {
        case TimingFunction::TCubicBezierFunction: {
          CubicBezierFunction cbf = tf.get_CubicBezierFunction();
          ctf->Init(nsTimingFunction(cbf.x1(), cbf.y1(), cbf.x2(), cbf.y2()));
          break;
        }
        default: {
          NS_ASSERTION(tf.type() == TimingFunction::TStepFunction,
                       "Function must be bezier or step");
          StepFunction sf = tf.get_StepFunction();
          nsTimingFunction::Type type = sf.type() == 1 ? nsTimingFunction::StepStart
                                                       : nsTimingFunction::StepEnd;
          ctf->Init(nsTimingFunction(type, sf.steps()));
          break;
        }
      }
      functions.AppendElement(ctf);
    }

    // Precompute the StyleAnimationValues that we need if this is a transform
    // animation.
    InfallibleTArray<StyleAnimationValue>& startValues = data->mStartValues;
    InfallibleTArray<StyleAnimationValue>& endValues = data->mEndValues;
    for (uint32_t j = 0; j < mAnimations[i].segments().Length(); j++) {
      const AnimationSegment& segment = mAnimations[i].segments()[j];
      StyleAnimationValue* startValue = startValues.AppendElement();
      StyleAnimationValue* endValue = endValues.AppendElement();
      if (segment.endState().type() == Animatable::TArrayOfTransformFunction) {
        const InfallibleTArray<TransformFunction>& startFunctions =
          segment.startState().get_ArrayOfTransformFunction();
        startValue->SetTransformValue(CreateCSSValueList(startFunctions));

        const InfallibleTArray<TransformFunction>& endFunctions =
          segment.endState().get_ArrayOfTransformFunction();
        endValue->SetTransformValue(CreateCSSValueList(endFunctions));
      } else {
        NS_ASSERTION(segment.endState().type() == Animatable::Tfloat,
                     "Unknown Animatable type");
        startValue->SetFloatValue(segment.startState().get_float());
        endValue->SetFloatValue(segment.endState().get_float());
      }
    }
  }

  Mutated();
}

void
Layer::SetAsyncPanZoomController(uint32_t aIndex, AsyncPanZoomController *controller)
{
  MOZ_ASSERT(aIndex < GetFrameMetricsCount());
  mApzcs[aIndex] = controller;
}

AsyncPanZoomController*
Layer::GetAsyncPanZoomController(uint32_t aIndex) const
{
  MOZ_ASSERT(aIndex < GetFrameMetricsCount());
#ifdef DEBUG
  if (mApzcs[aIndex]) {
    MOZ_ASSERT(GetFrameMetrics(aIndex).IsScrollable());
  }
#endif
  return mApzcs[aIndex];
}

void
Layer::FrameMetricsChanged()
{
  mApzcs.SetLength(GetFrameMetricsCount());
}

void
Layer::ApplyPendingUpdatesToSubtree()
{
  ApplyPendingUpdatesForThisTransaction();
  for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
    child->ApplyPendingUpdatesToSubtree();
  }
}

bool
Layer::CanUseOpaqueSurface()
{
  // If the visible content in the layer is opaque, there is no need
  // for an alpha channel.
  if (GetContentFlags() & CONTENT_OPAQUE)
    return true;
  // Also, if this layer is the bottommost layer in a container which
  // doesn't need an alpha channel, we can use an opaque surface for this
  // layer too. Any transparent areas must be covered by something else
  // in the container.
  ContainerLayer* parent = GetParent();
  return parent && parent->GetFirstChild() == this &&
    parent->CanUseOpaqueSurface();
}

// NB: eventually these methods will be defined unconditionally, and
// can be moved into Layers.h
const nsIntRect*
Layer::GetEffectiveClipRect()
{
  if (LayerComposite* shadow = AsLayerComposite()) {
    return shadow->GetShadowClipRect();
  }
  return GetClipRect();
}

const nsIntRegion&
Layer::GetEffectiveVisibleRegion()
{
  if (LayerComposite* shadow = AsLayerComposite()) {
    return shadow->GetShadowVisibleRegion();
  }
  return GetVisibleRegion();
}

Matrix4x4
Layer::SnapTransformTranslation(const Matrix4x4& aTransform,
                                Matrix* aResidualTransform)
{
  if (aResidualTransform) {
    *aResidualTransform = Matrix();
  }

  Matrix matrix2D;
  Matrix4x4 result;
  if (mManager->IsSnappingEffectiveTransforms() &&
      aTransform.Is2D(&matrix2D) &&
      !matrix2D.HasNonTranslation() &&
      matrix2D.HasNonIntegerTranslation()) {
    IntPoint snappedTranslation = RoundedToInt(matrix2D.GetTranslation());
    Matrix snappedMatrix = Matrix::Translation(snappedTranslation.x,
                                               snappedTranslation.y);
    result = Matrix4x4::From2D(snappedMatrix);
    if (aResidualTransform) {
      // set aResidualTransform so that aResidual * snappedMatrix == matrix2D.
      // (I.e., appying snappedMatrix after aResidualTransform gives the
      // ideal transform.)
      *aResidualTransform =
        Matrix::Translation(matrix2D._31 - snappedTranslation.x,
                            matrix2D._32 - snappedTranslation.y);
    }
  } else {
    result = aTransform;
  }
  return result;
}

Matrix4x4
Layer::SnapTransform(const Matrix4x4& aTransform,
                     const gfxRect& aSnapRect,
                     Matrix* aResidualTransform)
{
  if (aResidualTransform) {
    *aResidualTransform = Matrix();
  }

  Matrix matrix2D;
  Matrix4x4 result;
  if (mManager->IsSnappingEffectiveTransforms() &&
      aTransform.Is2D(&matrix2D) &&
      gfx::Size(1.0, 1.0) <= ToSize(aSnapRect.Size()) &&
      matrix2D.PreservesAxisAlignedRectangles()) {
    IntPoint transformedTopLeft = RoundedToInt(matrix2D * ToPoint(aSnapRect.TopLeft()));
    IntPoint transformedTopRight = RoundedToInt(matrix2D * ToPoint(aSnapRect.TopRight()));
    IntPoint transformedBottomRight = RoundedToInt(matrix2D * ToPoint(aSnapRect.BottomRight()));

    Matrix snappedMatrix = gfxUtils::TransformRectToRect(aSnapRect,
      transformedTopLeft, transformedTopRight, transformedBottomRight);

    result = Matrix4x4::From2D(snappedMatrix);
    if (aResidualTransform && !snappedMatrix.IsSingular()) {
      // set aResidualTransform so that aResidual * snappedMatrix == matrix2D.
      // (i.e., appying snappedMatrix after aResidualTransform gives the
      // ideal transform.
      Matrix snappedMatrixInverse = snappedMatrix;
      snappedMatrixInverse.Invert();
      *aResidualTransform = matrix2D * snappedMatrixInverse;
    }
  } else {
    result = aTransform;
  }
  return result;
}

static bool
AncestorLayerMayChangeTransform(Layer* aLayer)
{
  for (Layer* l = aLayer; l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_MAY_CHANGE_TRANSFORM) {
      return true;
    }
  }
  return false;
}

bool
Layer::MayResample()
{
  Matrix transform2d;
  return !GetEffectiveTransform().Is2D(&transform2d) ||
         ThebesMatrix(transform2d).HasNonIntegerTranslation() ||
         AncestorLayerMayChangeTransform(this);
}

RenderTargetIntRect
Layer::CalculateScissorRect(const RenderTargetIntRect& aCurrentScissorRect)
{
  ContainerLayer* container = GetParent();
  NS_ASSERTION(container, "This can't be called on the root!");

  // Establish initial clip rect: it's either the one passed in, or
  // if the parent has an intermediate surface, it's the extents of that surface.
  RenderTargetIntRect currentClip;
  if (container->UseIntermediateSurface()) {
    currentClip.SizeTo(container->GetIntermediateSurfaceRect().Size());
  } else {
    currentClip = aCurrentScissorRect;
  }

  if (!GetEffectiveClipRect()) {
    return currentClip;
  }

  const RenderTargetIntRect clipRect = RenderTargetPixel::FromUntyped(*GetEffectiveClipRect());
  if (clipRect.IsEmpty()) {
    // We might have a non-translation transform in the container so we can't
    // use the code path below.
    return RenderTargetIntRect(currentClip.TopLeft(), RenderTargetIntSize(0, 0));
  }

  RenderTargetIntRect scissor = clipRect;
  if (!container->UseIntermediateSurface()) {
    gfx::Matrix matrix;
    DebugOnly<bool> is2D = container->GetEffectiveTransform().Is2D(&matrix);
    // See DefaultComputeEffectiveTransforms below
    NS_ASSERTION(is2D && matrix.PreservesAxisAlignedRectangles(),
                 "Non preserves axis aligned transform with clipped child should have forced intermediate surface");
    gfx::Rect r(scissor.x, scissor.y, scissor.width, scissor.height);
    gfxRect trScissor = gfx::ThebesRect(matrix.TransformBounds(r));
    trScissor.Round();
    nsIntRect tmp;
    if (!gfxUtils::GfxRectToIntRect(trScissor, &tmp)) {
      return RenderTargetIntRect(currentClip.TopLeft(), RenderTargetIntSize(0, 0));
    }
    scissor = RenderTargetPixel::FromUntyped(tmp);

    // Find the nearest ancestor with an intermediate surface
    do {
      container = container->GetParent();
    } while (container && !container->UseIntermediateSurface());
  }

  if (container) {
    scissor.MoveBy(-container->GetIntermediateSurfaceRect().TopLeft());
  }
  return currentClip.Intersect(scissor);
}

const FrameMetrics&
Layer::GetFrameMetrics(uint32_t aIndex) const
{
  MOZ_ASSERT(aIndex < GetFrameMetricsCount());
  return mFrameMetrics[aIndex];
}

bool
Layer::HasScrollableFrameMetrics() const
{
  for (uint32_t i = 0; i < GetFrameMetricsCount(); i++) {
    if (GetFrameMetrics(i).IsScrollable()) {
      return true;
    }
  }
  return false;
}

bool
Layer::IsScrollInfoLayer() const
{
  // A scrollable container layer with no children
  return AsContainerLayer()
      && HasScrollableFrameMetrics()
      && !GetFirstChild();
}

const Matrix4x4
Layer::GetTransform() const
{
  Matrix4x4 transform = mTransform;
  transform.PostScale(mPostXScale, mPostYScale, 1.0f);
  if (const ContainerLayer* c = AsContainerLayer()) {
    transform.PreScale(c->GetPreXScale(), c->GetPreYScale(), 1.0f);
  }
  return transform;
}

const Matrix4x4
Layer::GetLocalTransform()
{
  Matrix4x4 transform;
  if (LayerComposite* shadow = AsLayerComposite())
    transform = shadow->GetShadowTransform();
  else
    transform = mTransform;

  transform.PostScale(mPostXScale, mPostYScale, 1.0f);
  if (ContainerLayer* c = AsContainerLayer()) {
    transform.PreScale(c->GetPreXScale(), c->GetPreYScale(), 1.0f);
  }

  return transform;
}

void
Layer::ApplyPendingUpdatesForThisTransaction()
{
  if (mPendingTransform && *mPendingTransform != mTransform) {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) PendingUpdatesForThisTransaction", this));
    mTransform = *mPendingTransform;
    Mutated();
  }
  mPendingTransform = nullptr;

  if (mPendingAnimations) {
    MOZ_LAYERS_LOG_IF_SHADOWABLE(this, ("Layer::Mutated(%p) PendingUpdatesForThisTransaction", this));
    mPendingAnimations->SwapElements(mAnimations);
    mPendingAnimations = nullptr;
    Mutated();
  }
}

const float
Layer::GetLocalOpacity()
{
   if (LayerComposite* shadow = AsLayerComposite())
    return shadow->GetShadowOpacity();
  return mOpacity;
}

float
Layer::GetEffectiveOpacity()
{
  float opacity = GetLocalOpacity();
  for (ContainerLayer* c = GetParent(); c && !c->UseIntermediateSurface();
       c = c->GetParent()) {
    opacity *= c->GetLocalOpacity();
  }
  return opacity;
}
  
CompositionOp
Layer::GetEffectiveMixBlendMode()
{
  if(mMixBlendMode != CompositionOp::OP_OVER)
    return mMixBlendMode;
  for (ContainerLayer* c = GetParent(); c && !c->UseIntermediateSurface();
    c = c->GetParent()) {
    if(c->mMixBlendMode != CompositionOp::OP_OVER)
      return c->mMixBlendMode;
  }

  return mMixBlendMode;
}

gfxContext::GraphicsOperator
Layer::DeprecatedGetEffectiveMixBlendMode()
{
  return ThebesOp(GetEffectiveMixBlendMode());
}

void
Layer::ComputeEffectiveTransformForMaskLayer(const Matrix4x4& aTransformToSurface)
{
  if (mMaskLayer) {
    mMaskLayer->mEffectiveTransform = aTransformToSurface;

#ifdef DEBUG
    bool maskIs2D = mMaskLayer->GetTransform().CanDraw2D();
    NS_ASSERTION(maskIs2D, "How did we end up with a 3D transform here?!");
#endif
    // Use our shadow transform and base transform to compute a delta for the
    // mask layer's effective transform, as though it was also transformed by
    // the APZ.
    //
    // Note: This will fail if the base transform is degenerate. Currently, this
    //       is not expected for OMTA transformed layers.
    mMaskLayer->mEffectiveTransform = mMaskLayer->GetTransform() *
      GetTransform().Inverse() * GetLocalTransform() *
      mMaskLayer->mEffectiveTransform;
  }
}

RenderTargetRect
Layer::TransformRectToRenderTarget(const LayerIntRect& aRect)
{
  LayerRect rect(aRect);
  RenderTargetRect quad = RenderTargetRect::FromUnknown(
    GetEffectiveTransform().TransformBounds(
      LayerPixel::ToUnknown(rect)));
  return quad;
}

ContainerLayer::ContainerLayer(LayerManager* aManager, void* aImplData)
  : Layer(aManager, aImplData),
    mFirstChild(nullptr),
    mLastChild(nullptr),
    mPreXScale(1.0f),
    mPreYScale(1.0f),
    mInheritedXScale(1.0f),
    mInheritedYScale(1.0f),
    mUseIntermediateSurface(false),
    mSupportsComponentAlphaChildren(false),
    mMayHaveReadbackChild(false),
    mChildrenChanged(false)
{
  mContentFlags = 0; // Clear NO_TEXT, NO_TEXT_OVER_TRANSPARENT
}

ContainerLayer::~ContainerLayer() {}

bool
ContainerLayer::InsertAfter(Layer* aChild, Layer* aAfter)
{
  if(aChild->Manager() != Manager()) {
    NS_ERROR("Child has wrong manager");
    return false;
  }
  if(aChild->GetParent()) {
    NS_ERROR("aChild already in the tree");
    return false;
  }
  if (aChild->GetNextSibling() || aChild->GetPrevSibling()) {
    NS_ERROR("aChild already has siblings?");
    return false;
  }
  if (aAfter && (aAfter->Manager() != Manager() ||
                 aAfter->GetParent() != this))
  {
    NS_ERROR("aAfter is not our child");
    return false;
  }

  aChild->SetParent(this);
  if (aAfter == mLastChild) {
    mLastChild = aChild;
  }
  if (!aAfter) {
    aChild->SetNextSibling(mFirstChild);
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(aChild);
    }
    mFirstChild = aChild;
    NS_ADDREF(aChild);
    DidInsertChild(aChild);
    return true;
  }

  Layer* next = aAfter->GetNextSibling();
  aChild->SetNextSibling(next);
  aChild->SetPrevSibling(aAfter);
  if (next) {
    next->SetPrevSibling(aChild);
  }
  aAfter->SetNextSibling(aChild);
  NS_ADDREF(aChild);
  DidInsertChild(aChild);
  return true;
}

bool
ContainerLayer::RemoveChild(Layer *aChild)
{
  if (aChild->Manager() != Manager()) {
    NS_ERROR("Child has wrong manager");
    return false;
  }
  if (aChild->GetParent() != this) {
    NS_ERROR("aChild not our child");
    return false;
  }

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    this->mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  } else {
    this->mLastChild = prev;
  }

  aChild->SetNextSibling(nullptr);
  aChild->SetPrevSibling(nullptr);
  aChild->SetParent(nullptr);

  this->DidRemoveChild(aChild);
  NS_RELEASE(aChild);
  return true;
}


bool
ContainerLayer::RepositionChild(Layer* aChild, Layer* aAfter)
{
  if (aChild->Manager() != Manager()) {
    NS_ERROR("Child has wrong manager");
    return false;
  }
  if (aChild->GetParent() != this) {
    NS_ERROR("aChild not our child");
    return false;
  }
  if (aAfter && (aAfter->Manager() != Manager() ||
                 aAfter->GetParent() != this))
  {
    NS_ERROR("aAfter is not our child");
    return false;
  }
  if (aChild == aAfter) {
    NS_ERROR("aChild cannot be the same as aAfter");
    return false;
  }

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev == aAfter) {
    // aChild is already in the correct position, nothing to do.
    return true;
  }
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  } else {
    mLastChild = prev;
  }
  if (!aAfter) {
    aChild->SetPrevSibling(nullptr);
    aChild->SetNextSibling(mFirstChild);
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(aChild);
    }
    mFirstChild = aChild;
    return true;
  }

  Layer* afterNext = aAfter->GetNextSibling();
  if (afterNext) {
    afterNext->SetPrevSibling(aChild);
  } else {
    mLastChild = aChild;
  }
  aAfter->SetNextSibling(aChild);
  aChild->SetPrevSibling(aAfter);
  aChild->SetNextSibling(afterNext);
  return true;
}

void
ContainerLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = ContainerLayerAttributes(mPreXScale, mPreYScale,
                                    mInheritedXScale, mInheritedYScale);
}

bool
ContainerLayer::HasMultipleChildren()
{
  uint32_t count = 0;
  for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
    const nsIntRect *clipRect = child->GetEffectiveClipRect();
    if (clipRect && clipRect->IsEmpty())
      continue;
    if (child->GetVisibleRegion().IsEmpty())
      continue;
    ++count;
    if (count > 1)
      return true;
  }

  return false;
}

void
ContainerLayer::SortChildrenBy3DZOrder(nsTArray<Layer*>& aArray)
{
  nsAutoTArray<Layer*, 10> toSort;

  for (Layer* l = GetFirstChild(); l; l = l->GetNextSibling()) {
    ContainerLayer* container = l->AsContainerLayer();
    if (container && container->GetContentFlags() & CONTENT_PRESERVE_3D) {
      toSort.AppendElement(l);
    } else {
      if (toSort.Length() > 0) {
        SortLayersBy3DZOrder(toSort);
        aArray.MoveElementsFrom(toSort);
      }
      aArray.AppendElement(l);
    }
  }
  if (toSort.Length() > 0) {
    SortLayersBy3DZOrder(toSort);
    aArray.MoveElementsFrom(toSort);
  }
}

void
ContainerLayer::DefaultComputeEffectiveTransforms(const Matrix4x4& aTransformToSurface)
{
  Matrix residual;
  Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;
  idealTransform.ProjectTo2D();
  mEffectiveTransform = SnapTransformTranslation(idealTransform, &residual);

  bool useIntermediateSurface;
  if (GetMaskLayer() ||
      GetForceIsolatedGroup()) {
    useIntermediateSurface = true;
#ifdef MOZ_DUMP_PAINTING
  } else if (gfxUtils::sDumpPainting) {
    useIntermediateSurface = true;
#endif
  } else {
    float opacity = GetEffectiveOpacity();
    CompositionOp blendMode = GetEffectiveMixBlendMode();
    if ((opacity != 1.0f || blendMode != CompositionOp::OP_OVER) && HasMultipleChildren()) {
      useIntermediateSurface = true;
    } else {
      useIntermediateSurface = false;
      gfx::Matrix contTransform;
      if (!mEffectiveTransform.Is2D(&contTransform) ||
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
        !contTransform.PreservesAxisAlignedRectangles()) {
#else
        gfx::ThebesMatrix(contTransform).HasNonIntegerTranslation()) {
#endif
        for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
          const nsIntRect *clipRect = child->GetEffectiveClipRect();
          /* We can't (easily) forward our transform to children with a non-empty clip
           * rect since it would need to be adjusted for the transform. See
           * the calculations performed by CalculateScissorRect above.
           * Nor for a child with a mask layer.
           */
          if ((clipRect && !clipRect->IsEmpty() && !child->GetVisibleRegion().IsEmpty()) ||
              child->GetMaskLayer()) {
            useIntermediateSurface = true;
            break;
          }
        }
      }
    }
  }

  mUseIntermediateSurface = useIntermediateSurface && !GetEffectiveVisibleRegion().IsEmpty();
  if (useIntermediateSurface) {
    ComputeEffectiveTransformsForChildren(Matrix4x4::From2D(residual));
  } else {
    ComputeEffectiveTransformsForChildren(idealTransform);
  }

  if (idealTransform.CanDraw2D()) {
    ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
  } else {
    ComputeEffectiveTransformForMaskLayer(Matrix4x4());
  }
}

void
ContainerLayer::DefaultComputeSupportsComponentAlphaChildren(bool* aNeedsSurfaceCopy)
{
  if (!(GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA_DESCENDANT) ||
      !Manager()->AreComponentAlphaLayersEnabled()) {
    mSupportsComponentAlphaChildren = false;
    if (aNeedsSurfaceCopy) {
      *aNeedsSurfaceCopy = false;
    }
    return;
  }

  mSupportsComponentAlphaChildren = false;
  bool needsSurfaceCopy = false;
  CompositionOp blendMode = GetEffectiveMixBlendMode();
  if (UseIntermediateSurface()) {
    if (GetEffectiveVisibleRegion().GetNumRects() == 1 &&
        (GetContentFlags() & Layer::CONTENT_OPAQUE))
    {
      mSupportsComponentAlphaChildren = true;
    } else {
      gfx::Matrix transform;
      if (HasOpaqueAncestorLayer(this) &&
          GetEffectiveTransform().Is2D(&transform) &&
          !gfx::ThebesMatrix(transform).HasNonIntegerTranslation() &&
          blendMode == gfx::CompositionOp::OP_OVER) {
        mSupportsComponentAlphaChildren = true;
        needsSurfaceCopy = true;
      }
    }
  } else if (blendMode == gfx::CompositionOp::OP_OVER) {
    mSupportsComponentAlphaChildren =
      (GetContentFlags() & Layer::CONTENT_OPAQUE) ||
      (GetParent() && GetParent()->SupportsComponentAlphaChildren());
  }

  if (aNeedsSurfaceCopy) {
    *aNeedsSurfaceCopy = mSupportsComponentAlphaChildren && needsSurfaceCopy;
  }
}

void
ContainerLayer::ComputeEffectiveTransformsForChildren(const Matrix4x4& aTransformToSurface)
{
  for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
    l->ComputeEffectiveTransforms(aTransformToSurface);
  }
}

/* static */ bool
ContainerLayer::HasOpaqueAncestorLayer(Layer* aLayer)
{
  for (Layer* l = aLayer->GetParent(); l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_OPAQUE)
      return true;
  }
  return false;
}

void
ContainerLayer::DidRemoveChild(Layer* aLayer)
{
  PaintedLayer* tl = aLayer->AsPaintedLayer();
  if (tl && tl->UsedForReadback()) {
    for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
      if (l->GetType() == TYPE_READBACK) {
        static_cast<ReadbackLayer*>(l)->NotifyPaintedLayerRemoved(tl);
      }
    }
  }
  if (aLayer->GetType() == TYPE_READBACK) {
    static_cast<ReadbackLayer*>(aLayer)->NotifyRemoved();
  }
}

void
ContainerLayer::DidInsertChild(Layer* aLayer)
{
  if (aLayer->GetType() == TYPE_READBACK) {
    mMayHaveReadbackChild = true;
  }
}

void
RefLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = RefLayerAttributes(GetReferentId());
}

/** 
 * StartFrameTimeRecording, together with StopFrameTimeRecording
 * enable recording of frame intervals.
 *
 * To allow concurrent consumers, a cyclic array is used which serves all
 * consumers, practically stateless with regard to consumers.
 *
 * To save resources, the buffer is allocated on first call to StartFrameTimeRecording
 * and recording is paused if no consumer which called StartFrameTimeRecording is able
 * to get valid results (because the cyclic buffer was overwritten since that call).
 *
 * To determine availability of the data upon StopFrameTimeRecording:
 * - mRecording.mNextIndex increases on each PostPresent, and never resets.
 * - Cyclic buffer position is realized as mNextIndex % bufferSize.
 * - StartFrameTimeRecording returns mNextIndex. When StopFrameTimeRecording is called,
 *   the required start index is passed as an arg, and we're able to calculate the required
 *   length. If this length is bigger than bufferSize, it means data was overwritten.
 *   otherwise, we can return the entire sequence.
 * - To determine if we need to pause, mLatestStartIndex is updated to mNextIndex
 *   on each call to StartFrameTimeRecording. If this index gets overwritten,
 *   it means that all earlier start indices obtained via StartFrameTimeRecording
 *   were also overwritten, hence, no point in recording, so pause.
 * - mCurrentRunStartIndex indicates the oldest index of the recording after which
 *   the recording was not paused. If StopFrameTimeRecording is invoked with a start index
 *   older than this, it means that some frames were not recorded, so data is invalid.
 */
uint32_t
LayerManager::StartFrameTimeRecording(int32_t aBufferSize)
{
  if (mRecording.mIsPaused) {
    mRecording.mIsPaused = false;

    if (!mRecording.mIntervals.Length()) { // Initialize recording buffers
      mRecording.mIntervals.SetLength(aBufferSize);
    }

    // After being paused, recent values got invalid. Update them to now.
    mRecording.mLastFrameTime = TimeStamp::Now();

    // Any recording which started before this is invalid, since we were paused.
    mRecording.mCurrentRunStartIndex = mRecording.mNextIndex;
  }

  // If we'll overwrite this index, there are no more consumers with aStartIndex
  // for which we're able to provide the full recording, so no point in keep recording.
  mRecording.mLatestStartIndex = mRecording.mNextIndex;
  return mRecording.mNextIndex;
}

void
LayerManager::RecordFrame()
{
  if (!mRecording.mIsPaused) {
    TimeStamp now = TimeStamp::Now();
    uint32_t i = mRecording.mNextIndex % mRecording.mIntervals.Length();
    mRecording.mIntervals[i] = static_cast<float>((now - mRecording.mLastFrameTime)
                                                  .ToMilliseconds());
    mRecording.mNextIndex++;
    mRecording.mLastFrameTime = now;

    if (mRecording.mNextIndex > (mRecording.mLatestStartIndex + mRecording.mIntervals.Length())) {
      // We've just overwritten the most recent recording start -> pause.
      mRecording.mIsPaused = true;
    }
  }
}

void
LayerManager::PostPresent()
{
  if (!mTabSwitchStart.IsNull()) {
    Telemetry::Accumulate(Telemetry::FX_TAB_SWITCH_TOTAL_MS,
                          uint32_t((TimeStamp::Now() - mTabSwitchStart).ToMilliseconds()));
    mTabSwitchStart = TimeStamp();
  }
}

void
LayerManager::StopFrameTimeRecording(uint32_t         aStartIndex,
                                     nsTArray<float>& aFrameIntervals)
{
  uint32_t bufferSize = mRecording.mIntervals.Length();
  uint32_t length = mRecording.mNextIndex - aStartIndex;
  if (mRecording.mIsPaused || length > bufferSize || aStartIndex < mRecording.mCurrentRunStartIndex) {
    // aStartIndex is too old. Also if aStartIndex was issued before mRecordingNextIndex overflowed (uint32_t)
    //   and stopped after the overflow (would happen once every 828 days of constant 60fps).
    length = 0;
  }

  if (!length) {
    aFrameIntervals.Clear();
    return; // empty recording, return empty arrays.
  }
  // Set length in advance to avoid possibly repeated reallocations
  aFrameIntervals.SetLength(length);

  uint32_t cyclicPos = aStartIndex % bufferSize;
  for (uint32_t i = 0; i < length; i++, cyclicPos++) {
    if (cyclicPos == bufferSize) {
      cyclicPos = 0;
    }
    aFrameIntervals[i] = mRecording.mIntervals[cyclicPos];
  }
}

void
LayerManager::BeginTabSwitch()
{
  mTabSwitchStart = TimeStamp::Now();
}

static void PrintInfo(std::stringstream& aStream, LayerComposite* aLayerComposite);

#ifdef MOZ_DUMP_PAINTING
template <typename T>
void WriteSnapshotLinkToDumpFile(T* aObj, std::stringstream& aStream)
{
  if (!aObj) {
    return;
  }
  nsCString string(aObj->Name());
  string.Append('-');
  string.AppendInt((uint64_t)aObj);
  aStream << nsPrintfCString("href=\"javascript:ViewImage('%s')\"", string.BeginReading()).get();
}

template <typename T>
void WriteSnapshotToDumpFile_internal(T* aObj, DataSourceSurface* aSurf)
{
  nsCString string(aObj->Name());
  string.Append('-');
  string.AppendInt((uint64_t)aObj);
  if (gfxUtils::sDumpPaintFile) {
    fprintf_stderr(gfxUtils::sDumpPaintFile, "array[\"%s\"]=\"", string.BeginReading());
  }
  gfxUtils::DumpAsDataURI(aSurf, gfxUtils::sDumpPaintFile);
  if (gfxUtils::sDumpPaintFile) {
    fprintf_stderr(gfxUtils::sDumpPaintFile, "\";");
  }
}

void WriteSnapshotToDumpFile(Layer* aLayer, DataSourceSurface* aSurf)
{
  WriteSnapshotToDumpFile_internal(aLayer, aSurf);
}

void WriteSnapshotToDumpFile(LayerManager* aManager, DataSourceSurface* aSurf)
{
  WriteSnapshotToDumpFile_internal(aManager, aSurf);
}

void WriteSnapshotToDumpFile(Compositor* aCompositor, DrawTarget* aTarget)
{
  RefPtr<SourceSurface> surf = aTarget->Snapshot();
  RefPtr<DataSourceSurface> dSurf = surf->GetDataSurface();
  WriteSnapshotToDumpFile_internal(aCompositor, dSurf);
}
#endif

void
Layer::Dump(std::stringstream& aStream, const char* aPrefix, bool aDumpHtml)
{
  if (aDumpHtml) {
    aStream << nsPrintfCString("<li><a id=\"%p\" ", this).get();
#ifdef MOZ_DUMP_PAINTING
    if (GetType() == TYPE_CONTAINER || GetType() == TYPE_PAINTED) {
      WriteSnapshotLinkToDumpFile(this, aStream);
    }
#endif
    aStream << ">";
  }
  DumpSelf(aStream, aPrefix);

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting && AsLayerComposite() && AsLayerComposite()->GetCompositableHost()) {
    AsLayerComposite()->GetCompositableHost()->Dump(aStream, aPrefix, aDumpHtml);
  }
#endif

  if (aDumpHtml) {
    aStream << "</a>";
  }

  if (Layer* mask = GetMaskLayer()) {
    aStream << nsPrintfCString("%s  Mask layer:\n", aPrefix).get();
    nsAutoCString pfx(aPrefix);
    pfx += "    ";
    mask->Dump(aStream, pfx.get(), aDumpHtml);
  }

#ifdef MOZ_DUMP_PAINTING
  for (size_t i = 0; i < mExtraDumpInfo.Length(); i++) {
    const nsCString& str = mExtraDumpInfo[i];
    aStream << aPrefix << "  Info:\n" << str.get();
  }
#endif

  if (Layer* kid = GetFirstChild()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    if (aDumpHtml) {
      aStream << "<ul>";
    }
    kid->Dump(aStream, pfx.get(), aDumpHtml);
    if (aDumpHtml) {
      aStream << "</ul>";
    }
  }

  if (aDumpHtml) {
    aStream << "</li>";
  }
  if (Layer* next = GetNextSibling())
    next->Dump(aStream, aPrefix, aDumpHtml);
}

void
Layer::DumpSelf(std::stringstream& aStream, const char* aPrefix)
{
  PrintInfo(aStream, aPrefix);
  aStream << "\n";
}

void
Layer::Dump(layerscope::LayersPacket* aPacket, const void* aParent)
{
  DumpPacket(aPacket, aParent);

  if (Layer* kid = GetFirstChild()) {
    kid->Dump(aPacket, this);
  }

  if (Layer* next = GetNextSibling()) {
    next->Dump(aPacket, aParent);
  }
}

void
Layer::Log(const char* aPrefix)
{
  if (!IsLogEnabled())
    return;

  LogSelf(aPrefix);

  if (Layer* kid = GetFirstChild()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    kid->Log(pfx.get());
  }

  if (Layer* next = GetNextSibling())
    next->Log(aPrefix);
}

void
Layer::LogSelf(const char* aPrefix)
{
  if (!IsLogEnabled())
    return;

  std::stringstream ss;
  PrintInfo(ss, aPrefix);
  MOZ_LAYERS_LOG(("%s", ss.str().c_str()));

  if (mMaskLayer) {
    nsAutoCString pfx(aPrefix);
    pfx += "   \\ MaskLayer ";
    mMaskLayer->LogSelf(pfx.get());
  }
}

void
Layer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix;
  aStream << nsPrintfCString("%s%s (0x%p)", mManager->Name(), Name(), this).get();

  layers::PrintInfo(aStream, AsLayerComposite());

  if (mUseClipRect) {
    AppendToString(aStream, mClipRect, " [clip=", "]");
  }
  if (1.0 != mPostXScale || 1.0 != mPostYScale) {
    aStream << nsPrintfCString(" [postScale=%g, %g]", mPostXScale, mPostYScale).get();
  }
  if (!mTransform.IsIdentity()) {
    AppendToString(aStream, mTransform, " [transform=", "]");
  }
  if (!mLayerBounds.IsEmpty()) {
    AppendToString(aStream, mLayerBounds, " [bounds=", "]");
  }
  if (!mVisibleRegion.IsEmpty()) {
    AppendToString(aStream, mVisibleRegion, " [visible=", "]");
  } else {
    aStream << " [not visible]";
  }
  if (!mEventRegions.IsEmpty()) {
    AppendToString(aStream, mEventRegions, " ", "");
  }
  if (1.0 != mOpacity) {
    aStream << nsPrintfCString(" [opacity=%g]", mOpacity).get();
  }
  if (GetContentFlags() & CONTENT_OPAQUE) {
    aStream << " [opaqueContent]";
  }
  if (GetContentFlags() & CONTENT_COMPONENT_ALPHA) {
    aStream << " [componentAlpha]";
  }
  if (GetScrollbarDirection() == VERTICAL) {
    aStream << nsPrintfCString(" [vscrollbar=%lld]", GetScrollbarTargetContainerId()).get();
  }
  if (GetScrollbarDirection() == HORIZONTAL) {
    aStream << nsPrintfCString(" [hscrollbar=%lld]", GetScrollbarTargetContainerId()).get();
  }
  if (GetIsFixedPosition()) {
    aStream << nsPrintfCString(" [isFixedPosition anchor=%s margin=%f,%f,%f,%f]",
                     ToString(mAnchor).c_str(),
                     mMargins.top, mMargins.right, mMargins.bottom, mMargins.left).get();
  }
  if (GetIsStickyPosition()) {
    aStream << nsPrintfCString(" [isStickyPosition scrollId=%d outer=%f,%f %fx%f "
                     "inner=%f,%f %fx%f]", mStickyPositionData->mScrollId,
                     mStickyPositionData->mOuter.x, mStickyPositionData->mOuter.y,
                     mStickyPositionData->mOuter.width, mStickyPositionData->mOuter.height,
                     mStickyPositionData->mInner.x, mStickyPositionData->mInner.y,
                     mStickyPositionData->mInner.width, mStickyPositionData->mInner.height).get();
  }
  if (mMaskLayer) {
    aStream << nsPrintfCString(" [mMaskLayer=%p]", mMaskLayer.get()).get();
  }
  for (uint32_t i = 0; i < mFrameMetrics.Length(); i++) {
    if (!mFrameMetrics[i].IsDefault()) {
      aStream << nsPrintfCString(" [metrics%d=", i).get();
      AppendToString(aStream, mFrameMetrics[i], "", "]");
    }
  }
}

// The static helper function sets the transform matrix into the packet
static void
DumpTransform(layerscope::LayersPacket::Layer::Matrix* aLayerMatrix, const Matrix4x4& aMatrix)
{
  aLayerMatrix->set_is2d(aMatrix.Is2D());
  if (aMatrix.Is2D()) {
    Matrix m = aMatrix.As2D();
    aLayerMatrix->set_isid(m.IsIdentity());
    if (!m.IsIdentity()) {
      aLayerMatrix->add_m(m._11), aLayerMatrix->add_m(m._12);
      aLayerMatrix->add_m(m._21), aLayerMatrix->add_m(m._22);
      aLayerMatrix->add_m(m._31), aLayerMatrix->add_m(m._32);
    }
  } else {
    aLayerMatrix->add_m(aMatrix._11), aLayerMatrix->add_m(aMatrix._12);
    aLayerMatrix->add_m(aMatrix._13), aLayerMatrix->add_m(aMatrix._14);
    aLayerMatrix->add_m(aMatrix._21), aLayerMatrix->add_m(aMatrix._22);
    aLayerMatrix->add_m(aMatrix._23), aLayerMatrix->add_m(aMatrix._24);
    aLayerMatrix->add_m(aMatrix._31), aLayerMatrix->add_m(aMatrix._32);
    aLayerMatrix->add_m(aMatrix._33), aLayerMatrix->add_m(aMatrix._34);
    aLayerMatrix->add_m(aMatrix._41), aLayerMatrix->add_m(aMatrix._42);
    aLayerMatrix->add_m(aMatrix._43), aLayerMatrix->add_m(aMatrix._44);
  }
}

// The static helper function sets the nsIntRect into the packet
static void
DumpRect(layerscope::LayersPacket::Layer::Rect* aLayerRect, const nsIntRect& aRect)
{
  aLayerRect->set_x(aRect.x);
  aLayerRect->set_y(aRect.y);
  aLayerRect->set_w(aRect.width);
  aLayerRect->set_h(aRect.height);
}

// The static helper function sets the nsIntRegion into the packet
static void
DumpRegion(layerscope::LayersPacket::Layer::Region* aLayerRegion, const nsIntRegion& aRegion)
{
  nsIntRegionRectIterator it(aRegion);
  while (const nsIntRect* sr = it.Next()) {
    DumpRect(aLayerRegion->add_r(), *sr);
  }
}

void
Layer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent)
{
  // Add a new layer (UnknownLayer)
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->add_layer();
  // Basic information
  layer->set_type(LayersPacket::Layer::UnknownLayer);
  layer->set_ptr(reinterpret_cast<uint64_t>(this));
  layer->set_parentptr(reinterpret_cast<uint64_t>(aParent));
  // Shadow
  if (LayerComposite* lc = AsLayerComposite()) {
    LayersPacket::Layer::Shadow* s = layer->mutable_shadow();
    if (const nsIntRect* clipRect = lc->GetShadowClipRect()) {
      DumpRect(s->mutable_clip(), *clipRect);
    }
    if (!lc->GetShadowTransform().IsIdentity()) {
      DumpTransform(s->mutable_transform(), lc->GetShadowTransform());
    }
    if (!lc->GetShadowVisibleRegion().IsEmpty()) {
      DumpRegion(s->mutable_vregion(), lc->GetShadowVisibleRegion());
    }
  }
  // Clip
  if (mUseClipRect) {
    DumpRect(layer->mutable_clip(), mClipRect);
  }
  // Transform
  if (!mTransform.IsIdentity()) {
    DumpTransform(layer->mutable_transform(), mTransform);
  }
  // Visible region
  if (!mVisibleRegion.IsEmpty()) {
    DumpRegion(layer->mutable_vregion(), mVisibleRegion);
  }
  // Opacity
  layer->set_opacity(mOpacity);
  // Content opaque
  layer->set_copaque(static_cast<bool>(GetContentFlags() & CONTENT_OPAQUE));
  // Component alpha
  layer->set_calpha(static_cast<bool>(GetContentFlags() & CONTENT_COMPONENT_ALPHA));
  // Vertical or horizontal bar
  if (GetScrollbarDirection() != NONE) {
    layer->set_direct(GetScrollbarDirection() == VERTICAL ?
                      LayersPacket::Layer::VERTICAL :
                      LayersPacket::Layer::HORIZONTAL);
    layer->set_barid(GetScrollbarTargetContainerId());
  }
  // Mask layer
  if (mMaskLayer) {
    layer->set_mask(reinterpret_cast<uint64_t>(mMaskLayer.get()));
  }
}

void
PaintedLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  Layer::PrintInfo(aStream, aPrefix);
  if (!mValidRegion.IsEmpty()) {
    AppendToString(aStream, mValidRegion, " [valid=", "]");
  }
}

void
PaintedLayer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent)
{
  Layer::DumpPacket(aPacket, aParent);
  // get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->mutable_layer(aPacket->layer_size()-1);
  layer->set_type(LayersPacket::Layer::PaintedLayer);
  if (!mValidRegion.IsEmpty()) {
    DumpRegion(layer->mutable_valid(), mValidRegion);
  }
}

void
ContainerLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  Layer::PrintInfo(aStream, aPrefix);
  if (UseIntermediateSurface()) {
    aStream << " [usesTmpSurf]";
  }
  if (1.0 != mPreXScale || 1.0 != mPreYScale) {
    aStream << nsPrintfCString(" [preScale=%g, %g]", mPreXScale, mPreYScale).get();
  }
}

void
ContainerLayer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent)
{
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->mutable_layer(aPacket->layer_size()-1);
  layer->set_type(LayersPacket::Layer::ContainerLayer);
}

void
ColorLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  Layer::PrintInfo(aStream, aPrefix);
  AppendToString(aStream, mColor, " [color=", "]");
}

void
ColorLayer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent)
{
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->mutable_layer(aPacket->layer_size()-1);
  layer->set_type(LayersPacket::Layer::ColorLayer);
  layer->set_color(mColor.Packed());
}

void
CanvasLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  Layer::PrintInfo(aStream, aPrefix);
  if (mFilter != GraphicsFilter::FILTER_GOOD) {
    AppendToString(aStream, mFilter, " [filter=", "]");
  }
}

// This help function is used to assign the correct enum value
// to the packet
static void
DumpFilter(layerscope::LayersPacket::Layer* aLayer, const GraphicsFilter& aFilter)
{
  using namespace layerscope;
  switch (aFilter) {
    case GraphicsFilter::FILTER_FAST:
      aLayer->set_filter(LayersPacket::Layer::FILTER_FAST);
      break;
    case GraphicsFilter::FILTER_GOOD:
      aLayer->set_filter(LayersPacket::Layer::FILTER_GOOD);
      break;
    case GraphicsFilter::FILTER_BEST:
      aLayer->set_filter(LayersPacket::Layer::FILTER_BEST);
      break;
    case GraphicsFilter::FILTER_NEAREST:
      aLayer->set_filter(LayersPacket::Layer::FILTER_NEAREST);
      break;
    case GraphicsFilter::FILTER_BILINEAR:
      aLayer->set_filter(LayersPacket::Layer::FILTER_BILINEAR);
      break;
    case GraphicsFilter::FILTER_GAUSSIAN:
      aLayer->set_filter(LayersPacket::Layer::FILTER_GAUSSIAN);
      break;
    default:
      // ignore it
      break;
  }
}

void
CanvasLayer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent)
{
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->mutable_layer(aPacket->layer_size()-1);
  layer->set_type(LayersPacket::Layer::CanvasLayer);
  DumpFilter(layer, mFilter);
}

void
ImageLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  Layer::PrintInfo(aStream, aPrefix);
  if (mFilter != GraphicsFilter::FILTER_GOOD) {
    AppendToString(aStream, mFilter, " [filter=", "]");
  }
}

void
ImageLayer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent)
{
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->mutable_layer(aPacket->layer_size()-1);
  layer->set_type(LayersPacket::Layer::ImageLayer);
  DumpFilter(layer, mFilter);
}

void
RefLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  ContainerLayer::PrintInfo(aStream, aPrefix);
  if (0 != mId) {
    AppendToString(aStream, mId, " [id=", "]");
  }
}

void
RefLayer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent)
{
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->mutable_layer(aPacket->layer_size()-1);
  layer->set_type(LayersPacket::Layer::RefLayer);
  layer->set_refid(mId);
}

void
ReadbackLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  Layer::PrintInfo(aStream, aPrefix);
  AppendToString(aStream, mSize, " [size=", "]");
  if (mBackgroundLayer) {
    AppendToString(aStream, mBackgroundLayer, " [backgroundLayer=", "]");
    AppendToString(aStream, mBackgroundLayerOffset, " [backgroundOffset=", "]");
  } else if (mBackgroundColor.a == 1.0) {
    AppendToString(aStream, mBackgroundColor, " [backgroundColor=", "]");
  } else {
    aStream << " [nobackground]";
  }
}

void
ReadbackLayer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent)
{
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->mutable_layer(aPacket->layer_size()-1);
  layer->set_type(LayersPacket::Layer::ReadbackLayer);
  LayersPacket::Layer::Size* size = layer->mutable_size();
  size->set_w(mSize.width);
  size->set_h(mSize.height);
}

//--------------------------------------------------
// LayerManager

void
LayerManager::Dump(std::stringstream& aStream, const char* aPrefix, bool aDumpHtml)
{
#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml) {
    aStream << "<ul><li><a ";
    WriteSnapshotLinkToDumpFile(this, aStream);
    aStream << ">";
  }
#endif
  DumpSelf(aStream, aPrefix);
#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml) {
    aStream << "</a>";
  }
#endif

  nsAutoCString pfx(aPrefix);
  pfx += "  ";
  if (!GetRoot()) {
    aStream << nsPrintfCString("%s(null)", pfx.get()).get();
    if (aDumpHtml) {
      aStream << "</li></ul>";
    }
    return;
  }

  if (aDumpHtml) {
    aStream << "<ul>";
  }
  GetRoot()->Dump(aStream, pfx.get(), aDumpHtml);
  if (aDumpHtml) {
    aStream << "</ul></li></ul>";
  }
  aStream << "\n";
}

void
LayerManager::DumpSelf(std::stringstream& aStream, const char* aPrefix)
{
  PrintInfo(aStream, aPrefix);
  aStream << "\n";
}

void
LayerManager::Dump()
{
  std::stringstream ss;
  Dump(ss);
  print_stderr(ss);
}

void
LayerManager::Dump(layerscope::LayersPacket* aPacket)
{
  DumpPacket(aPacket);

  if (GetRoot()) {
    GetRoot()->Dump(aPacket, this);
  }
}

void
LayerManager::Log(const char* aPrefix)
{
  if (!IsLogEnabled())
    return;

  LogSelf(aPrefix);

  nsAutoCString pfx(aPrefix);
  pfx += "  ";
  if (!GetRoot()) {
    MOZ_LAYERS_LOG(("%s(null)", pfx.get()));
    return;
  }

  GetRoot()->Log(pfx.get());
}

void
LayerManager::LogSelf(const char* aPrefix)
{
  nsAutoCString str;
  std::stringstream ss;
  PrintInfo(ss, aPrefix);
  MOZ_LAYERS_LOG(("%s", ss.str().c_str()));
}

void
LayerManager::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  aStream << aPrefix << nsPrintfCString("%sLayerManager (0x%p)", Name(), this).get();
}

void
LayerManager::DumpPacket(layerscope::LayersPacket* aPacket)
{
  using namespace layerscope;
  // Add a new layer data (LayerManager)
  LayersPacket::Layer* layer = aPacket->add_layer();
  layer->set_type(LayersPacket::Layer::LayerManager);
  layer->set_ptr(reinterpret_cast<uint64_t>(this));
  // Layer Tree Root
  layer->set_parentptr(0);
}

/*static*/ void
LayerManager::InitLog()
{
  if (!sLog)
    sLog = PR_NewLogModule("Layers");
}

/*static*/ bool
LayerManager::IsLogEnabled()
{
  NS_ABORT_IF_FALSE(!!sLog,
                    "layer manager must be created before logging is allowed");
  return PR_LOG_TEST(sLog, PR_LOG_DEBUG);
}

void
PrintInfo(std::stringstream& aStream, LayerComposite* aLayerComposite)
{
  if (!aLayerComposite) {
    return;
  }
  if (const nsIntRect* clipRect = aLayerComposite->GetShadowClipRect()) {
    AppendToString(aStream, *clipRect, " [shadow-clip=", "]");
  }
  if (!aLayerComposite->GetShadowTransform().IsIdentity()) {
    AppendToString(aStream, aLayerComposite->GetShadowTransform(), " [shadow-transform=", "]");
  }
  if (!aLayerComposite->GetShadowVisibleRegion().IsEmpty()) {
    AppendToString(aStream, aLayerComposite->GetShadowVisibleRegion(), " [shadow-visible=", "]");
  }
}

void
SetAntialiasingFlags(Layer* aLayer, DrawTarget* aTarget)
{
  bool permitSubpixelAA = !(aLayer->GetContentFlags() & Layer::CONTENT_DISABLE_SUBPIXEL_AA);
  if (aTarget->GetFormat() != SurfaceFormat::B8G8R8A8) {
    aTarget->SetPermitSubpixelAA(permitSubpixelAA);
    return;
  }

  const nsIntRect& bounds = aLayer->GetVisibleRegion().GetBounds();
  gfx::Rect transformedBounds = aTarget->GetTransform().TransformBounds(gfx::Rect(Float(bounds.x), Float(bounds.y),
                                                                                  Float(bounds.width), Float(bounds.height)));
  transformedBounds.RoundOut();
  IntRect intTransformedBounds;
  transformedBounds.ToIntRect(&intTransformedBounds);
  permitSubpixelAA &= !(aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA) ||
                      aTarget->GetOpaqueRect().Contains(intTransformedBounds);
  aTarget->SetPermitSubpixelAA(permitSubpixelAA);
}

nsIntRect
ToOutsideIntRect(const gfxRect &aRect)
{
  gfxRect r = aRect;
  r.RoundOut();
  return nsIntRect(r.X(), r.Y(), r.Width(), r.Height());
}

PRLogModuleInfo* LayerManager::sLog;

} // namespace layers
} // namespace mozilla
