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
#include "UnitTransforms.h"             // for ViewAs
#include "gfxEnv.h"
#include "gfxPlatform.h"                // for gfxPlatform
#include "gfxPrefs.h"
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "gfx2DGlue.h"
#include "mozilla/DebugOnly.h"          // for DebugOnly
#include "mozilla/Telemetry.h"          // for Accumulate
#include "mozilla/dom/Animation.h"      // for ComputedTimingFunction
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/layers/AsyncCanvasRenderer.h"
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayerAnimationUtils.h"  // for TimingFunctionToComputedTimingFunction
#include "mozilla/layers/LayerManagerComposite.h"  // for LayerComposite
#include "mozilla/layers/LayerMetricsWrapper.h" // for LayerMetricsWrapper
#include "mozilla/layers/LayersMessages.h"  // for TransformFunction, etc
#include "mozilla/layers/LayersTypes.h"  // for TextureDumpMode
#include "mozilla/layers/PersistentBufferProvider.h"
#include "mozilla/layers/ShadowLayers.h"  // for ShadowableLayer
#include "nsAString.h"
#include "nsCSSValue.h"                 // for nsCSSValue::Array, etc
#include "nsPrintfCString.h"            // for nsPrintfCString
#include "nsStyleStruct.h"              // for nsTimingFunction, etc
#include "protobuf/LayerScopePacket.pb.h"
#include "mozilla/Compression.h"

uint8_t gLayerManagerLayerBuilder;

namespace mozilla {
namespace layers {

FILE*
FILEOrDefault(FILE* aFile)
{
  return aFile ? aFile : stderr;
}

typedef FrameMetrics::ViewID ViewID;

using namespace mozilla::gfx;
using namespace mozilla::Compression;

//--------------------------------------------------
// LayerManager

/* static */ mozilla::LogModule*
LayerManager::GetLog()
{
  static LazyLogModule sLog("Layers");
  return sLog;
}

FrameMetrics::ViewID
LayerManager::GetRootScrollableLayerId()
{
  if (!mRoot) {
    return FrameMetrics::NULL_SCROLL_ID;
  }

  nsTArray<LayerMetricsWrapper> queue = { LayerMetricsWrapper(mRoot) };
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

  nsTArray<Layer*> queue = { mRoot };
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

  nsTArray<Layer*> queue = { mRoot };
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

already_AddRefed<DrawTarget>
LayerManager::CreateOptimalDrawTarget(const gfx::IntSize &aSize,
                                      SurfaceFormat aFormat)
{
  return gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(aSize,
                                                                      aFormat);
}

already_AddRefed<DrawTarget>
LayerManager::CreateOptimalMaskDrawTarget(const gfx::IntSize &aSize)
{
  return CreateOptimalDrawTarget(aSize, SurfaceFormat::A8);
}

already_AddRefed<DrawTarget>
LayerManager::CreateDrawTarget(const IntSize &aSize,
                               SurfaceFormat aFormat)
{
  return gfxPlatform::GetPlatform()->
    CreateOffscreenCanvasDrawTarget(aSize, aFormat);
}

already_AddRefed<PersistentBufferProvider>
LayerManager::CreatePersistentBufferProvider(const mozilla::gfx::IntSize &aSize,
                                             mozilla::gfx::SurfaceFormat aFormat)
{
  RefPtr<PersistentBufferProviderBasic> bufferProvider =
    new PersistentBufferProviderBasic(aSize, aFormat,
                                      gfxPlatform::GetPlatform()->GetPreferredCanvasBackend());

  if (!bufferProvider->IsValid()) {
    bufferProvider =
      new PersistentBufferProviderBasic(aSize, aFormat,
                                        gfxPlatform::GetPlatform()->GetFallbackCanvasBackend());
  }

  if (!bufferProvider->IsValid()) {
    return nullptr;
  }

  return bufferProvider.forget();
}

#ifdef DEBUG
void
LayerManager::Mutated(Layer* aLayer)
{
}
#endif  // DEBUG

already_AddRefed<ImageContainer>
LayerManager::CreateImageContainer(ImageContainer::Mode flag)
{
  RefPtr<ImageContainer> container = new ImageContainer(flag);
  return container.forget();
}

bool
LayerManager::AreComponentAlphaLayersEnabled()
{
  return gfxPrefs::ComponentAlphaEnabled();
}

/*static*/ void
LayerManager::LayerUserDataDestroy(void* data)
{
  delete static_cast<LayerUserData*>(data);
}

nsAutoPtr<LayerUserData>
LayerManager::RemoveUserData(void* aKey)
{
  nsAutoPtr<LayerUserData> d(static_cast<LayerUserData*>(mUserData.Remove(static_cast<gfx::UserDataKey*>(aKey))));
  return d;
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
  mUseTileSourceRect(false),
  mIsFixedPosition(false),
  mTransformIsPerspective(false),
  mFixedPositionData(nullptr),
  mStickyPositionData(nullptr),
  mScrollbarTargetId(FrameMetrics::NULL_SCROLL_ID),
  mScrollbarDirection(ScrollDirection::NONE),
  mScrollbarThumbRatio(0.0f),
  mIsScrollbarContainer(false),
#ifdef DEBUG
  mDebugColorIndex(0),
#endif
  mAnimationGeneration(0)
{
  MOZ_COUNT_CTOR(Layer);
}

Layer::~Layer()
{
  MOZ_COUNT_DTOR(Layer);
}

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

static inline void
SetCSSAngle(const CSSAngle& aAngle, nsCSSValue& aValue)
{
  aValue.SetFloatValue(aAngle.value(), nsCSSUnit(aAngle.unit()));
}

static nsCSSValueSharedList*
CreateCSSValueList(const InfallibleTArray<TransformFunction>& aFunctions)
{
  nsAutoPtr<nsCSSValueList> result;
  nsCSSValueList** resultTail = getter_Transfers(result);
  for (uint32_t i = 0; i < aFunctions.Length(); i++) {
    RefPtr<nsCSSValue::Array> arr;
    switch (aFunctions[i].type()) {
      case TransformFunction::TRotationX:
      {
        const CSSAngle& angle = aFunctions[i].get_RotationX().angle();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotatex,
                                                           resultTail);
        SetCSSAngle(angle, arr->Item(1));
        break;
      }
      case TransformFunction::TRotationY:
      {
        const CSSAngle& angle = aFunctions[i].get_RotationY().angle();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotatey,
                                                           resultTail);
        SetCSSAngle(angle, arr->Item(1));
        break;
      }
      case TransformFunction::TRotationZ:
      {
        const CSSAngle& angle = aFunctions[i].get_RotationZ().angle();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotatez,
                                                           resultTail);
        SetCSSAngle(angle, arr->Item(1));
        break;
      }
      case TransformFunction::TRotation:
      {
        const CSSAngle& angle = aFunctions[i].get_Rotation().angle();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotate,
                                                           resultTail);
        SetCSSAngle(angle, arr->Item(1));
        break;
      }
      case TransformFunction::TRotation3D:
      {
        float x = aFunctions[i].get_Rotation3D().x();
        float y = aFunctions[i].get_Rotation3D().y();
        float z = aFunctions[i].get_Rotation3D().z();
        const CSSAngle& angle = aFunctions[i].get_Rotation3D().angle();
        arr =
          StyleAnimationValue::AppendTransformFunction(eCSSKeyword_rotate3d,
                                                       resultTail);
        arr->Item(1).SetFloatValue(x, eCSSUnit_Number);
        arr->Item(2).SetFloatValue(y, eCSSUnit_Number);
        arr->Item(3).SetFloatValue(z, eCSSUnit_Number);
        SetCSSAngle(angle, arr->Item(4));
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
        const CSSAngle& x = aFunctions[i].get_SkewX().x();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_skewx,
                                                           resultTail);
        SetCSSAngle(x, arr->Item(1));
        break;
      }
      case TransformFunction::TSkewY:
      {
        const CSSAngle& y = aFunctions[i].get_SkewY().y();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_skewy,
                                                           resultTail);
        SetCSSAngle(y, arr->Item(1));
        break;
      }
      case TransformFunction::TSkew:
      {
        const CSSAngle& x = aFunctions[i].get_Skew().x();
        const CSSAngle& y = aFunctions[i].get_Skew().y();
        arr = StyleAnimationValue::AppendTransformFunction(eCSSKeyword_skew,
                                                           resultTail);
        SetCSSAngle(x, arr->Item(1));
        SetCSSAngle(y, arr->Item(2));
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
    InfallibleTArray<Maybe<ComputedTimingFunction>>& functions =
      data->mFunctions;
    const InfallibleTArray<AnimationSegment>& segments =
      mAnimations.ElementAt(i).segments();
    for (uint32_t j = 0; j < segments.Length(); j++) {
      TimingFunction tf = segments.ElementAt(j).sampleFn();

      Maybe<ComputedTimingFunction> ctf =
        AnimationUtils::TimingFunctionToComputedTimingFunction(tf);
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
Layer::StartPendingAnimations(const TimeStamp& aReadyTime)
{
  bool updated = false;
  for (size_t animIdx = 0, animEnd = mAnimations.Length();
       animIdx < animEnd; animIdx++) {
    Animation& anim = mAnimations[animIdx];
    if (anim.startTime().IsNull()) {
      anim.startTime() = aReadyTime - anim.initialCurrentTime();
      updated = true;
    }
  }

  if (updated) {
    Mutated();
  }

  for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
    child->StartPendingAnimations(aReadyTime);
  }
}

void
Layer::SetAsyncPanZoomController(uint32_t aIndex, AsyncPanZoomController *controller)
{
  MOZ_ASSERT(aIndex < GetScrollMetadataCount());
  mApzcs[aIndex] = controller;
}

AsyncPanZoomController*
Layer::GetAsyncPanZoomController(uint32_t aIndex) const
{
  MOZ_ASSERT(aIndex < GetScrollMetadataCount());
#ifdef DEBUG
  if (mApzcs[aIndex]) {
    MOZ_ASSERT(GetFrameMetrics(aIndex).IsScrollable());
  }
#endif
  return mApzcs[aIndex];
}

void
Layer::ScrollMetadataChanged()
{
  mApzcs.SetLength(GetScrollMetadataCount());
}

void
Layer::ApplyPendingUpdatesToSubtree()
{
  ApplyPendingUpdatesForThisTransaction();
  for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
    child->ApplyPendingUpdatesToSubtree();
  }
  if (!GetParent()) {
    // Once we're done recursing through the whole tree, clear the pending
    // updates from the manager.
    Manager()->ClearPendingScrollInfoUpdate();
  }
}

bool
Layer::IsOpaqueForVisibility()
{
  return GetEffectiveOpacity() == 1.0f &&
         GetEffectiveMixBlendMode() == CompositionOp::OP_OVER;
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
const Maybe<ParentLayerIntRect>&
Layer::GetLocalClipRect()
{
  if (LayerComposite* shadow = AsLayerComposite()) {
    return shadow->GetShadowClipRect();
  }
  return GetClipRect();
}

const LayerIntRegion&
Layer::GetLocalVisibleRegion()
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

  if (!mManager->IsSnappingEffectiveTransforms()) {
    return aTransform;
  }

  Matrix matrix2D;
  Matrix4x4 result;
  if (aTransform.CanDraw2D(&matrix2D) &&
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
    return result;
  }

  if(aTransform.IsSingular() ||
     aTransform.HasPerspectiveComponent() ||
     aTransform.HasNonTranslation() ||
     !aTransform.HasNonIntegerTranslation()) {
    // For a singular transform, there is no reversed matrix, so we
    // don't snap it.
    // For a perspective transform, the content is transformed in
    // non-linear, so we don't snap it too.
    return aTransform;
  }

  // Snap for 3D Transforms

  Point3D transformedOrigin = aTransform * Point3D();

  // Compute the transformed snap by rounding the values of
  // transformed origin.
  IntPoint transformedSnapXY =
    RoundedToInt(Point(transformedOrigin.x, transformedOrigin.y));
  Matrix4x4 inverse = aTransform;
  inverse.Invert();
  // see Matrix4x4::ProjectPoint()
  Float transformedSnapZ =
    inverse._33 == 0 ? 0 : (-(transformedSnapXY.x * inverse._13 +
                              transformedSnapXY.y * inverse._23 +
                              inverse._43) / inverse._33);
  Point3D transformedSnap =
    Point3D(transformedSnapXY.x, transformedSnapXY.y, transformedSnapZ);
  if (transformedOrigin == transformedSnap) {
    return aTransform;
  }

  // Compute the snap from the transformed snap.
  Point3D snap = inverse * transformedSnap;
  if (snap.z > 0.001 || snap.z < -0.001) {
    // Allow some level of accumulated computation error.
    MOZ_ASSERT(inverse._33 == 0.0);
    return aTransform;
  }

  // The difference between the origin and snap is the residual transform.
  if (aResidualTransform) {
    // The residual transform is to translate the snap to the origin
    // of the content buffer.
    *aResidualTransform = Matrix::Translation(-snap.x, -snap.y);
  }

  // Translate transformed origin to transformed snap since the
  // residual transform would trnslate the snap to the origin.
  Point3D transformedShift = transformedSnap - transformedOrigin;
  result = aTransform;
  result.PostTranslate(transformedShift.x,
                       transformedShift.y,
                       transformedShift.z);

  // For non-2d transform, residual translation could be more than
  // 0.5 pixels for every axis.

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
      gfxSize(1.0, 1.0) <= aSnapRect.Size() &&
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
  ContainerLayer* containerChild = nullptr;
  NS_ASSERTION(GetParent(), "This can't be called on the root!");

  // Find the layer creating the 3D context.
  while (container->Extend3DContext() &&
         !container->UseIntermediateSurface()) {
    containerChild = container;
    container = container->GetParent();
    MOZ_ASSERT(container);
  }

  // Find the nearest layer with a clip, or this layer.
  // ContainerState::SetupScrollingMetadata() may install a clip on
  // the layer.
  Layer *clipLayer =
    containerChild && containerChild->GetLocalClipRect() ?
    containerChild : this;

  // Establish initial clip rect: it's either the one passed in, or
  // if the parent has an intermediate surface, it's the extents of that surface.
  RenderTargetIntRect currentClip;
  if (container->UseIntermediateSurface()) {
    currentClip.SizeTo(container->GetIntermediateSurfaceRect().Size());
  } else {
    currentClip = aCurrentScissorRect;
  }

  if (!clipLayer->GetLocalClipRect()) {
    return currentClip;
  }

  if (GetVisibleRegion().IsEmpty()) {
    // When our visible region is empty, our parent may not have created the
    // intermediate surface that we would require for correct clipping; however,
    // this does not matter since we are invisible.
    // Note that we do not use GetLocalVisibleRegion(), because that can be
    // empty for a layer whose rendered contents have been async-scrolled
    // completely offscreen, but for which we still need to draw a
    // checkerboarding backround color, and calculating an empty scissor rect
    // for such a layer would prevent that (see bug 1247452 comment 10).
    return RenderTargetIntRect(currentClip.TopLeft(), RenderTargetIntSize(0, 0));
  }

  const RenderTargetIntRect clipRect =
    ViewAs<RenderTargetPixel>(*clipLayer->GetLocalClipRect(),
                              PixelCastJustification::RenderTargetIsParentLayerForRoot);
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
    IntRect tmp;
    if (!gfxUtils::GfxRectToIntRect(trScissor, &tmp)) {
      return RenderTargetIntRect(currentClip.TopLeft(), RenderTargetIntSize(0, 0));
    }
    scissor = ViewAs<RenderTargetPixel>(tmp);

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

const ScrollMetadata&
Layer::GetScrollMetadata(uint32_t aIndex) const
{
  MOZ_ASSERT(aIndex < GetScrollMetadataCount());
  return mScrollMetadata[aIndex];
}

const FrameMetrics&
Layer::GetFrameMetrics(uint32_t aIndex) const
{
  return GetScrollMetadata(aIndex).GetMetrics();
}

bool
Layer::HasScrollableFrameMetrics() const
{
  for (uint32_t i = 0; i < GetScrollMetadataCount(); i++) {
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

Matrix4x4
Layer::GetTransform() const
{
  Matrix4x4 transform = mTransform;
  transform.PostScale(GetPostXScale(), GetPostYScale(), 1.0f);
  if (const ContainerLayer* c = AsContainerLayer()) {
    transform.PreScale(c->GetPreXScale(), c->GetPreYScale(), 1.0f);
  }
  return transform;
}

const CSSTransformMatrix
Layer::GetTransformTyped() const
{
  return ViewAs<CSSTransformMatrix>(GetTransform());
}

Matrix4x4
Layer::GetLocalTransform()
{
  if (LayerComposite* shadow = AsLayerComposite())
    return shadow->GetShadowTransform();
  else
    return GetTransform();
}

const LayerToParentLayerMatrix4x4
Layer::GetLocalTransformTyped()
{
  return ViewAs<LayerToParentLayerMatrix4x4>(GetLocalTransform());
}

bool
Layer::HasTransformAnimation() const
{
  for (uint32_t i = 0; i < mAnimations.Length(); i++) {
    if (mAnimations[i].property() == eCSSProperty_transform) {
      return true;
    }
  }
  return false;
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

  for (size_t i = 0; i < mScrollMetadata.Length(); i++) {
    FrameMetrics& fm = mScrollMetadata[i].GetMetrics();
    Maybe<ScrollUpdateInfo> update = Manager()->GetPendingScrollInfoUpdate(fm.GetScrollId());
    if (update) {
      fm.UpdatePendingScrollInfo(update.value());
      Mutated();
    }
  }
}

float
Layer::GetLocalOpacity()
{
  float opacity = mOpacity;
  if (LayerComposite* shadow = AsLayerComposite())
    opacity = shadow->GetShadowOpacity();
  return std::min(std::max(opacity, 0.0f), 1.0f);
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

void
Layer::ComputeEffectiveTransformForMaskLayers(const gfx::Matrix4x4& aTransformToSurface)
{
  if (GetMaskLayer()) {
    ComputeEffectiveTransformForMaskLayer(GetMaskLayer(), aTransformToSurface);
  }
  for (size_t i = 0; i < GetAncestorMaskLayerCount(); i++) {
    Layer* maskLayer = GetAncestorMaskLayerAt(i);
    ComputeEffectiveTransformForMaskLayer(maskLayer, aTransformToSurface);
  }
}

/* static */ void
Layer::ComputeEffectiveTransformForMaskLayer(Layer* aMaskLayer, const gfx::Matrix4x4& aTransformToSurface)
{
  aMaskLayer->mEffectiveTransform = aTransformToSurface;

#ifdef DEBUG
  bool maskIs2D = aMaskLayer->GetTransform().CanDraw2D();
  NS_ASSERTION(maskIs2D, "How did we end up with a 3D transform here?!");
#endif
  // The mask layer can have an async transform applied to it in some
  // situations, so be sure to use its GetLocalTransform() rather than
  // its GetTransform().
  aMaskLayer->mEffectiveTransform = aMaskLayer->GetLocalTransform() *
    aMaskLayer->mEffectiveTransform;
}

RenderTargetRect
Layer::TransformRectToRenderTarget(const LayerIntRect& aRect)
{
  LayerRect rect(aRect);
  RenderTargetRect quad = RenderTargetRect::FromUnknownRect(
    GetEffectiveTransform().TransformBounds(rect.ToUnknownRect()));
  return quad;
}

bool
Layer::GetVisibleRegionRelativeToRootLayer(nsIntRegion& aResult,
                                           nsIntPoint* aLayerOffset)
{
  MOZ_ASSERT(aLayerOffset, "invalid offset pointer");

  if (!GetParent()) {
    return false;
  }

  IntPoint offset;
  aResult = GetLocalVisibleRegion().ToUnknownRegion();
  for (Layer* layer = this; layer; layer = layer->GetParent()) {
    gfx::Matrix matrix;
    if (!layer->GetLocalTransform().Is2D(&matrix) ||
        !matrix.IsTranslation()) {
      return false;
    }

    // The offset of |layer| to its parent.
    IntPoint currentLayerOffset = RoundedToInt(matrix.GetTranslation());

    // Translate the accumulated visible region of |this| by the offset of
    // |layer|.
    aResult.MoveBy(currentLayerOffset.x, currentLayerOffset.y);

    // If the parent layer clips its lower layers, clip the visible region
    // we're accumulating.
    if (layer->GetLocalClipRect()) {
      aResult.AndWith(layer->GetLocalClipRect()->ToUnknownRect());
    }

    // Now we need to walk across the list of siblings for this parent layer,
    // checking to see if any of these layer trees obscure |this|. If so,
    // remove these areas from the visible region as well. This will pick up
    // chrome overlays like a tab modal prompt.
    Layer* sibling;
    for (sibling = layer->GetNextSibling(); sibling;
         sibling = sibling->GetNextSibling()) {
      gfx::Matrix siblingMatrix;
      if (!sibling->GetLocalTransform().Is2D(&siblingMatrix) ||
          !siblingMatrix.IsTranslation()) {
        continue;
      }

      // Retreive the translation from sibling to |layer|. The accumulated
      // visible region is currently oriented with |layer|.
      IntPoint siblingOffset = RoundedToInt(siblingMatrix.GetTranslation());
      nsIntRegion siblingVisibleRegion(sibling->GetLocalVisibleRegion().ToUnknownRegion());
      // Translate the siblings region to |layer|'s origin.
      siblingVisibleRegion.MoveBy(-siblingOffset.x, -siblingOffset.y);
      // Apply the sibling's clip.
      // Layer clip rects are not affected by the layer's transform.
      Maybe<ParentLayerIntRect> clipRect = sibling->GetLocalClipRect();
      if (clipRect) {
        siblingVisibleRegion.AndWith(clipRect->ToUnknownRect());
      }
      // Subtract the sibling visible region from the visible region of |this|.
      aResult.SubOut(siblingVisibleRegion);
    }

    // Keep track of the total offset for aLayerOffset.  We use this in plugin
    // positioning code.
    offset += currentLayerOffset;
  }

  *aLayerOffset = nsIntPoint(offset.x, offset.y);
  return true;
}

Maybe<ParentLayerIntRect>
Layer::GetCombinedClipRect() const
{
  Maybe<ParentLayerIntRect> clip = GetClipRect();

  clip = IntersectMaybeRects(clip, GetScrolledClipRect());

  for (size_t i = 0; i < mScrollMetadata.Length(); i++) {
    clip = IntersectMaybeRects(clip, mScrollMetadata[i].GetClipRect());
  }

  return clip;
}

ContainerLayer::ContainerLayer(LayerManager* aManager, void* aImplData)
  : Layer(aManager, aImplData),
    mFirstChild(nullptr),
    mLastChild(nullptr),
    mPreXScale(1.0f),
    mPreYScale(1.0f),
    mInheritedXScale(1.0f),
    mInheritedYScale(1.0f),
    mPresShellResolution(1.0f),
    mScaleToResolution(false),
    mUseIntermediateSurface(false),
    mSupportsComponentAlphaChildren(false),
    mMayHaveReadbackChild(false),
    mChildrenChanged(false),
    mEventRegionsOverride(EventRegionsOverride::NoOverride),
    mVRDeviceID(0),
    mInputFrameID(0)
{
  MOZ_COUNT_CTOR(ContainerLayer);
  mContentFlags = 0; // Clear NO_TEXT, NO_TEXT_OVER_TRANSPARENT
}

ContainerLayer::~ContainerLayer()
{
  MOZ_COUNT_DTOR(ContainerLayer);
}

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
                                    mInheritedXScale, mInheritedYScale,
                                    mPresShellResolution, mScaleToResolution,
                                    mEventRegionsOverride,
                                    mVRDeviceID,
                                    mInputFrameID);
}

bool
ContainerLayer::Creates3DContextWithExtendingChildren()
{
  if (Extend3DContext()) {
    return false;
  }
  for (Layer* child = GetFirstChild();
       child;
       child = child->GetNextSibling()) {
      if (child->Extend3DContext()) {
        return true;
      }
  }
  return false;
}

bool
ContainerLayer::HasMultipleChildren()
{
  uint32_t count = 0;
  for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
    const Maybe<ParentLayerIntRect>& clipRect = child->GetLocalClipRect();
    if (clipRect && clipRect->IsEmpty())
      continue;
    if (child->GetLocalVisibleRegion().IsEmpty())
      continue;
    ++count;
    if (count > 1)
      return true;
  }

  return false;
}

/**
 * Collect all leaf descendants of the current 3D context.
 */
void
ContainerLayer::Collect3DContextLeaves(nsTArray<Layer*>& aToSort)
{
  for (Layer* l = GetFirstChild(); l; l = l->GetNextSibling()) {
    ContainerLayer* container = l->AsContainerLayer();
    if (container && container->Extend3DContext() &&
        !container->UseIntermediateSurface()) {
      container->Collect3DContextLeaves(aToSort);
    } else {
      aToSort.AppendElement(l);
    }
  }
}

void
ContainerLayer::SortChildrenBy3DZOrder(nsTArray<Layer*>& aArray)
{
  AutoTArray<Layer*, 10> toSort;

  for (Layer* l = GetFirstChild(); l; l = l->GetNextSibling()) {
    ContainerLayer* container = l->AsContainerLayer();
    if (container && container->Extend3DContext() &&
        !container->UseIntermediateSurface()) {
      container->Collect3DContextLeaves(toSort);
    } else {
      if (toSort.Length() > 0) {
        SortLayersBy3DZOrder(toSort);
        aArray.AppendElements(Move(toSort));
        // XXX The move analysis gets confused here, because toSort gets moved
        // here, and then gets used again outside of the loop. To clarify that
        // we realize that the array is going to be empty to the move checker,
        // we clear it again here. (This method renews toSort for the move
        // analysis)
        toSort.ClearAndRetainStorage();
      }
      aArray.AppendElement(l);
    }
  }
  if (toSort.Length() > 0) {
    SortLayersBy3DZOrder(toSort);
    aArray.AppendElements(Move(toSort));
  }
}

void
ContainerLayer::DefaultComputeEffectiveTransforms(const Matrix4x4& aTransformToSurface)
{
  Matrix residual;
  Matrix4x4 idealTransform = GetLocalTransform() * aTransformToSurface;

  // Keep 3D transforms for leaves to keep z-order sorting correct.
  if (!Extend3DContext() && !Is3DContextLeaf()) {
    idealTransform.ProjectTo2D();
  }

  bool useIntermediateSurface;
  if (HasMaskLayers() ||
      GetForceIsolatedGroup()) {
    useIntermediateSurface = true;
#ifdef MOZ_DUMP_PAINTING
  } else if (gfxEnv::DumpPaintIntermediate() && !Extend3DContext()) {
    useIntermediateSurface = true;
#endif
  } else {
    /* Don't use an intermediate surface for opacity when it's within a 3d
     * context, since we'd rather keep the 3d effects. This matches the
     * WebKit/blink behaviour, but is changing in the latest spec.
     */
    float opacity = GetEffectiveOpacity();
    CompositionOp blendMode = GetEffectiveMixBlendMode();
    if ((HasMultipleChildren() || Creates3DContextWithExtendingChildren()) &&
        ((opacity != 1.0f && !Extend3DContext()) ||
         (blendMode != CompositionOp::OP_OVER))) {
      useIntermediateSurface = true;
    } else if (!idealTransform.Is2D() && Creates3DContextWithExtendingChildren()) {
      useIntermediateSurface = true;
    } else {
      useIntermediateSurface = false;
      gfx::Matrix contTransform;
      bool checkClipRect = false;
      bool checkMaskLayers = false;

      if (!idealTransform.Is2D(&contTransform)) {
        // In 3D case, always check if we should use IntermediateSurface.
        checkClipRect = true;
        checkMaskLayers = true;
      } else {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
        if (!contTransform.PreservesAxisAlignedRectangles()) {
#else
        if (gfx::ThebesMatrix(contTransform).HasNonIntegerTranslation()) {
#endif
          checkClipRect = true;
        }
        /* In 2D case, only translation and/or positive scaling can be done w/o using IntermediateSurface.
         * Otherwise, when rotation or flip happen, we should check whether to use IntermediateSurface.
         */
        if (contTransform.HasNonAxisAlignedTransform() || contTransform.HasNegativeScaling()) {
          checkMaskLayers = true;
        }
      }

      if (checkClipRect || checkMaskLayers) {
        for (Layer* child = GetFirstChild(); child; child = child->GetNextSibling()) {
          const Maybe<ParentLayerIntRect>& clipRect = child->GetLocalClipRect();
          /* We can't (easily) forward our transform to children with a non-empty clip
           * rect since it would need to be adjusted for the transform. See
           * the calculations performed by CalculateScissorRect above.
           * Nor for a child with a mask layer.
           */
          if (checkClipRect && (clipRect && !clipRect->IsEmpty() && !child->GetLocalVisibleRegion().IsEmpty())) {
            useIntermediateSurface = true;
            break;
          }
          if (checkMaskLayers && child->HasMaskLayers()) {
            useIntermediateSurface = true;
            break;
          }
        }
      }
    }
  }

  if (useIntermediateSurface) {
    mEffectiveTransform = SnapTransformTranslation(idealTransform, &residual);
  } else {
    mEffectiveTransform = idealTransform;
  }

  // For layers extending 3d context, its ideal transform should be
  // applied on children.
  if (!Extend3DContext()) {
    // Without this projection, non-container children would get a 3D
    // transform while 2D is expected.
    idealTransform.ProjectTo2D();
  }
  mUseIntermediateSurface = useIntermediateSurface && !GetLocalVisibleRegion().IsEmpty();
  if (useIntermediateSurface) {
    ComputeEffectiveTransformsForChildren(Matrix4x4::From2D(residual));
  } else {
    ComputeEffectiveTransformsForChildren(idealTransform);
  }

  ComputeEffectiveTransformForMaskLayers(aTransformToSurface);
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
    if (GetLocalVisibleRegion().GetNumRects() == 1 &&
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
  aAttrs = RefLayerAttributes(GetReferentId(), mEventRegionsOverride);
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
void WriteSnapshotToDumpFile_internal(T* aObj, DataSourceSurface* aSurf)
{
  nsCString string(aObj->Name());
  string.Append('-');
  string.AppendInt((uint64_t)aObj);
  if (gfxUtils::sDumpPaintFile != stderr) {
    fprintf_stderr(gfxUtils::sDumpPaintFile, "array[\"%s\"]=\"", string.BeginReading());
  }
  gfxUtils::DumpAsDataURI(aSurf, gfxUtils::sDumpPaintFile);
  if (gfxUtils::sDumpPaintFile != stderr) {
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
Layer::Dump(std::stringstream& aStream, const char* aPrefix,
            bool aDumpHtml, bool aSorted)
{
#ifdef MOZ_DUMP_PAINTING
  bool dumpCompositorTexture = gfxEnv::DumpCompositorTextures() && AsLayerComposite() &&
                               AsLayerComposite()->GetCompositableHost();
  bool dumpClientTexture = gfxEnv::DumpPaint() && AsShadowableLayer() &&
                           AsShadowableLayer()->GetCompositableClient();
  nsCString layerId(Name());
  layerId.Append('-');
  layerId.AppendInt((uint64_t)this);
#endif
  if (aDumpHtml) {
    aStream << nsPrintfCString("<li><a id=\"%p\" ", this).get();
#ifdef MOZ_DUMP_PAINTING
    if (dumpCompositorTexture || dumpClientTexture) {
      aStream << nsPrintfCString("href=\"javascript:ViewImage('%s')\"", layerId.BeginReading()).get();
    }
#endif
    aStream << ">";
  }
  DumpSelf(aStream, aPrefix);

#ifdef MOZ_DUMP_PAINTING
  if (dumpCompositorTexture) {
    AsLayerComposite()->GetCompositableHost()->Dump(aStream, aPrefix, aDumpHtml);
  } else if (dumpClientTexture) {
    if (aDumpHtml) {
      aStream << nsPrintfCString("<script>array[\"%s\"]=\"", layerId.BeginReading()).get();
    }
    AsShadowableLayer()->GetCompositableClient()->Dump(aStream, aPrefix,
        aDumpHtml, TextureDumpMode::DoNotCompress);
    if (aDumpHtml) {
      aStream << "\";</script>";
    }
  }
#endif

  if (aDumpHtml) {
    aStream << "</a>";
#ifdef MOZ_DUMP_PAINTING
    if (dumpClientTexture) {
      aStream << nsPrintfCString("<br><img id=\"%s\">\n", layerId.BeginReading()).get();
    }
#endif
  }

  if (Layer* mask = GetMaskLayer()) {
    aStream << nsPrintfCString("%s  Mask layer:\n", aPrefix).get();
    nsAutoCString pfx(aPrefix);
    pfx += "    ";
    mask->Dump(aStream, pfx.get(), aDumpHtml);
  }

  for (size_t i = 0; i < GetAncestorMaskLayerCount(); i++) {
    aStream << nsPrintfCString("%s  Ancestor mask layer %d:\n", aPrefix, uint32_t(i)).get();
    nsAutoCString pfx(aPrefix);
    pfx += "    ";
    GetAncestorMaskLayerAt(i)->Dump(aStream, pfx.get(), aDumpHtml);
  }

#ifdef MOZ_DUMP_PAINTING
  for (size_t i = 0; i < mExtraDumpInfo.Length(); i++) {
    const nsCString& str = mExtraDumpInfo[i];
    aStream << aPrefix << "  Info:\n" << str.get();
  }
#endif

  if (ContainerLayer* container = AsContainerLayer()) {
    AutoTArray<Layer*, 12> children;
    if (aSorted) {
      container->SortChildrenBy3DZOrder(children);
    } else {
      for (Layer* l = container->GetFirstChild(); l; l = l->GetNextSibling()) {
        children.AppendElement(l);
      }
    }
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    if (aDumpHtml) {
      aStream << "<ul>";
    }

    for (Layer* child : children) {
      child->Dump(aStream, pfx.get(), aDumpHtml, aSorted);
    }

    if (aDumpHtml) {
      aStream << "</ul>";
    }
  }

  if (aDumpHtml) {
    aStream << "</li>";
  }
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
Layer::SetDisplayListLog(const char* log)
{
  if (gfxUtils::DumpDisplayList()) {
    mDisplayListLog = log;
  }
}

void
Layer::GetDisplayListLog(nsCString& log)
{
  log.SetLength(0);

  if (gfxUtils::DumpDisplayList()) {
    // This function returns a plain text string which consists of two things
    //   1. DisplayList log.
    //   2. Memory address of this layer.
    // We know the target layer of each display item by information in #1.
    // Here is an example of a Text display item line log in #1
    //   Text p=0xa9850c00 f=0x0xaa405b00(.....
    // f keeps the address of the target client layer of a display item.
    // For LayerScope, display-item-to-client-layer mapping is not enough since
    // LayerScope, which lives in the chrome process, knows only composite layers.
    // As so, we need display-item-to-client-layer-to-layer-composite
    // mapping. That's the reason we insert #2 into the log
    log.AppendPrintf("0x%p\n%s",(void*) this, mDisplayListLog.get());
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

  if (mClipRect) {
    AppendToString(aStream, *mClipRect, " [clip=", "]");
  }
  if (mScrolledClip) {
    AppendToString(aStream, mScrolledClip->GetClipRect(), " [scrolled-clip=", "]");
  }
  if (1.0 != mPostXScale || 1.0 != mPostYScale) {
    aStream << nsPrintfCString(" [postScale=%g, %g]", mPostXScale, mPostYScale).get();
  }
  if (!mTransform.IsIdentity()) {
    AppendToString(aStream, mTransform, " [transform=", "]");
  }
  if (!GetEffectiveTransform().IsIdentity()) {
    AppendToString(aStream, GetEffectiveTransform(), " [effective-transform=", "]");
  }
  if (mTransformIsPerspective) {
    aStream << " [perspective]";
  }
  if (!mLayerBounds.IsEmpty()) {
    AppendToString(aStream, mLayerBounds, " [bounds=", "]");
  }
  if (!mVisibleRegion.IsEmpty()) {
    AppendToString(aStream, mVisibleRegion.ToUnknownRegion(), " [visible=", "]");
  } else {
    aStream << " [not visible]";
  }
  if (!mEventRegions.IsEmpty()) {
    AppendToString(aStream, mEventRegions, " ", "");
  }
  if (1.0 != mOpacity) {
    aStream << nsPrintfCString(" [opacity=%g]", mOpacity).get();
  }
  if (IsOpaque()) {
    aStream << " [opaqueContent]";
  }
  if (GetContentFlags() & CONTENT_COMPONENT_ALPHA) {
    aStream << " [componentAlpha]";
  }
  if (GetContentFlags() & CONTENT_BACKFACE_HIDDEN) {
    aStream << " [backfaceHidden]";
  }
  if (Extend3DContext()) {
    aStream << " [extend3DContext]";
  }
  if (Combines3DTransformWithAncestors()) {
    aStream << " [combines3DTransformWithAncestors]";
  }
  if (Is3DContextLeaf()) {
    aStream << " [is3DContextLeaf]";
  }
  if (IsScrollbarContainer()) {
    aStream << " [scrollbar]";
  }
  if (GetScrollbarDirection() == VERTICAL) {
    aStream << nsPrintfCString(" [vscrollbar=%lld]", GetScrollbarTargetContainerId()).get();
  }
  if (GetScrollbarDirection() == HORIZONTAL) {
    aStream << nsPrintfCString(" [hscrollbar=%lld]", GetScrollbarTargetContainerId()).get();
  }
  if (GetIsFixedPosition()) {
    LayerPoint anchor = GetFixedPositionAnchor();
    aStream << nsPrintfCString(" [isFixedPosition scrollId=%lld sides=0x%x anchor=%s%s]",
                     GetFixedPositionScrollContainerId(),
                     GetFixedPositionSides(),
                     ToString(anchor).c_str(),
                     IsClipFixed() ? "" : " scrollingClip").get();
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
  for (uint32_t i = 0; i < mScrollMetadata.Length(); i++) {
    if (!mScrollMetadata[i].IsDefault()) {
      aStream << nsPrintfCString(" [metrics%d=", i).get();
      AppendToString(aStream, mScrollMetadata[i], "", "]");
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

// The static helper function sets the IntRect into the packet
template <typename T, typename Sub, typename Point, typename SizeT, typename MarginT>
static void
DumpRect(layerscope::LayersPacket::Layer::Rect* aLayerRect,
         const BaseRect<T, Sub, Point, SizeT, MarginT>& aRect)
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
  for (auto iter = aRegion.RectIter(); !iter.Done(); iter.Next()) {
    DumpRect(aLayerRegion->add_r(), iter.Get());
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
    if (const Maybe<ParentLayerIntRect>& clipRect = lc->GetShadowClipRect()) {
      DumpRect(s->mutable_clip(), *clipRect);
    }
    if (!lc->GetShadowBaseTransform().IsIdentity()) {
      DumpTransform(s->mutable_transform(), lc->GetShadowBaseTransform());
    }
    if (!lc->GetShadowVisibleRegion().IsEmpty()) {
      DumpRegion(s->mutable_vregion(), lc->GetShadowVisibleRegion().ToUnknownRegion());
    }
  }
  // Clip
  if (mClipRect) {
    DumpRect(layer->mutable_clip(), *mClipRect);
  }
  // Transform
  if (!mTransform.IsIdentity()) {
    DumpTransform(layer->mutable_transform(), mTransform);
  }
  // Visible region
  if (!mVisibleRegion.ToUnknownRegion().IsEmpty()) {
    DumpRegion(layer->mutable_vregion(), mVisibleRegion.ToUnknownRegion());
  }
  // EventRegions
  if (!mEventRegions.IsEmpty()) {
    const EventRegions &e = mEventRegions;
    if (!e.mHitRegion.IsEmpty()) {
      DumpRegion(layer->mutable_hitregion(), e.mHitRegion);
    }
    if (!e.mDispatchToContentHitRegion.IsEmpty()) {
      DumpRegion(layer->mutable_dispatchregion(), e.mDispatchToContentHitRegion);
    }
    if (!e.mNoActionRegion.IsEmpty()) {
      DumpRegion(layer->mutable_noactionregion(), e.mNoActionRegion);
    }
    if (!e.mHorizontalPanRegion.IsEmpty()) {
      DumpRegion(layer->mutable_hpanregion(), e.mHorizontalPanRegion);
    }
    if (!e.mVerticalPanRegion.IsEmpty()) {
      DumpRegion(layer->mutable_vpanregion(), e.mVerticalPanRegion);
    }
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

  // DisplayList log.
  if (mDisplayListLog.Length() > 0) {
    layer->set_displaylistloglength(mDisplayListLog.Length());
    auto compressedData =
      MakeUnique<char[]>(LZ4::maxCompressedSize(mDisplayListLog.Length()));
    int compressedSize = LZ4::compress((char*)mDisplayListLog.get(),
                                       mDisplayListLog.Length(),
                                       compressedData.get());
    layer->set_displaylistlog(compressedData.get(), compressedSize);
  }
}

bool
Layer::IsBackfaceHidden()
{
  if (GetContentFlags() & CONTENT_BACKFACE_HIDDEN) {
    Layer* container = AsContainerLayer() ? this : GetParent();
    if (container) {
      // The effective transform can include non-preserve-3d parent
      // transforms, since we don't always require an intermediate.
      if (container->Extend3DContext() || container->Is3DContextLeaf()) {
        return container->GetEffectiveTransform().IsBackfaceVisible();
      }
      return container->GetBaseTransform().IsBackfaceVisible();
    }
  }
  return false;
}

nsAutoPtr<LayerUserData>
Layer::RemoveUserData(void* aKey)
{
  nsAutoPtr<LayerUserData> d(static_cast<LayerUserData*>(mUserData.Remove(static_cast<gfx::UserDataKey*>(aKey))));
  return d;
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
  if (mScaleToResolution) {
    aStream << nsPrintfCString(" [presShellResolution=%g]", mPresShellResolution).get();
  }
  if (mEventRegionsOverride & EventRegionsOverride::ForceDispatchToContent) {
    aStream << " [force-dtc]";
  }
  if (mEventRegionsOverride & EventRegionsOverride::ForceEmptyHitRegion) {
    aStream << " [force-ehr]";
  }
  if (mVRDeviceID) {
    aStream << nsPrintfCString(" [hmd=%lu] [hmdframe=%l]", mVRDeviceID, mInputFrameID).get();
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
  AppendToString(aStream, mBounds, " [bounds=", "]");
}

void
ColorLayer::DumpPacket(layerscope::LayersPacket* aPacket, const void* aParent)
{
  Layer::DumpPacket(aPacket, aParent);
  // Get this layer data
  using namespace layerscope;
  LayersPacket::Layer* layer = aPacket->mutable_layer(aPacket->layer_size()-1);
  layer->set_type(LayersPacket::Layer::ColorLayer);
  layer->set_color(mColor.ToABGR());
}

CanvasLayer::CanvasLayer(LayerManager* aManager, void* aImplData)
  : Layer(aManager, aImplData)
  , mPreTransCallback(nullptr)
  , mPreTransCallbackData(nullptr)
  , mPostTransCallback(nullptr)
  , mPostTransCallbackData(nullptr)
  , mFilter(gfx::Filter::GOOD)
  , mDirty(false)
{}

CanvasLayer::~CanvasLayer()
{}

void
CanvasLayer::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  Layer::PrintInfo(aStream, aPrefix);
  if (mFilter != Filter::GOOD) {
    AppendToString(aStream, mFilter, " [filter=", "]");
  }
}

// This help function is used to assign the correct enum value
// to the packet
static void
DumpFilter(layerscope::LayersPacket::Layer* aLayer, const Filter& aFilter)
{
  using namespace layerscope;
  switch (aFilter) {
    case Filter::GOOD:
      aLayer->set_filter(LayersPacket::Layer::FILTER_GOOD);
      break;
    case Filter::LINEAR:
      aLayer->set_filter(LayersPacket::Layer::FILTER_LINEAR);
      break;
    case Filter::POINT:
      aLayer->set_filter(LayersPacket::Layer::FILTER_POINT);
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
  if (mFilter != Filter::GOOD) {
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
  } else if (mBackgroundColor.a == 1.f) {
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
LayerManager::Dump(std::stringstream& aStream, const char* aPrefix,
                   bool aDumpHtml, bool aSorted)
{
#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml) {
    aStream << "<ul><li>";
  }
#endif
  DumpSelf(aStream, aPrefix, aSorted);

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
LayerManager::DumpSelf(std::stringstream& aStream, const char* aPrefix, bool aSorted)
{
  PrintInfo(aStream, aPrefix);
  aStream << " --- in " << (aSorted ? "3D-sorted rendering order" : "content order");
  aStream << "\n";
}

void
LayerManager::Dump(bool aSorted)
{
  std::stringstream ss;
  Dump(ss, "", false, aSorted);
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

/*static*/ bool
LayerManager::IsLogEnabled()
{
  return MOZ_LOG_TEST(GetLog(), LogLevel::Debug);
}

void
LayerManager::SetPendingScrollUpdateForNextTransaction(FrameMetrics::ViewID aScrollId,
                                                       const ScrollUpdateInfo& aUpdateInfo)
{
  mPendingScrollUpdates[aScrollId] = aUpdateInfo;
}

Maybe<ScrollUpdateInfo>
LayerManager::GetPendingScrollInfoUpdate(FrameMetrics::ViewID aScrollId)
{
  auto it = mPendingScrollUpdates.find(aScrollId);
  if (it != mPendingScrollUpdates.end()) {
    return Some(it->second);
  }
  return Nothing();
}

void
LayerManager::ClearPendingScrollInfoUpdate()
{
  mPendingScrollUpdates.clear();
}

void
PrintInfo(std::stringstream& aStream, LayerComposite* aLayerComposite)
{
  if (!aLayerComposite) {
    return;
  }
  if (const Maybe<ParentLayerIntRect>& clipRect = aLayerComposite->GetShadowClipRect()) {
    AppendToString(aStream, *clipRect, " [shadow-clip=", "]");
  }
  if (!aLayerComposite->GetShadowBaseTransform().IsIdentity()) {
    AppendToString(aStream, aLayerComposite->GetShadowBaseTransform(), " [shadow-transform=", "]");
  }
  if (!aLayerComposite->GetShadowVisibleRegion().IsEmpty()) {
    AppendToString(aStream, aLayerComposite->GetShadowVisibleRegion().ToUnknownRegion(), " [shadow-visible=", "]");
  }
}

void
SetAntialiasingFlags(Layer* aLayer, DrawTarget* aTarget)
{
  bool permitSubpixelAA = !(aLayer->GetContentFlags() & Layer::CONTENT_DISABLE_SUBPIXEL_AA);
  if (aTarget->IsCurrentGroupOpaque()) {
    aTarget->SetPermitSubpixelAA(permitSubpixelAA);
    return;
  }

  const IntRect& bounds = aLayer->GetVisibleRegion().ToUnknownRegion().GetBounds();
  gfx::Rect transformedBounds = aTarget->GetTransform().TransformBounds(gfx::Rect(Float(bounds.x), Float(bounds.y),
                                                                                  Float(bounds.width), Float(bounds.height)));
  transformedBounds.RoundOut();
  IntRect intTransformedBounds;
  transformedBounds.ToIntRect(&intTransformedBounds);
  permitSubpixelAA &= !(aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA) ||
                      aTarget->GetOpaqueRect().Contains(intTransformedBounds);
  aTarget->SetPermitSubpixelAA(permitSubpixelAA);
}

IntRect
ToOutsideIntRect(const gfxRect &aRect)
{
  gfxRect r = aRect;
  r.RoundOut();
  return IntRect(r.X(), r.Y(), r.Width(), r.Height());
}

} // namespace layers
} // namespace mozilla
