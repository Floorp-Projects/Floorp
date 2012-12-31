/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"
#include "mozilla/Telemetry.h"

#include "ImageLayers.h"
#include "ImageContainer.h"
#include "Layers.h"
#include "gfxPlatform.h"
#include "ReadbackLayer.h"
#include "gfxUtils.h"
#include "nsPrintfCString.h"
#include "LayerSorter.h"
#include "AnimationCommon.h"

using namespace mozilla::layers;
using namespace mozilla::gfx;

typedef FrameMetrics::ViewID ViewID;
const ViewID FrameMetrics::NULL_SCROLL_ID = 0;
const ViewID FrameMetrics::ROOT_SCROLL_ID = 1;
const ViewID FrameMetrics::START_SCROLL_ID = 2;

uint8_t gLayerManagerLayerBuilder;

#ifdef MOZ_LAYERS_HAVE_LOG
FILE*
FILEOrDefault(FILE* aFile)
{
  return aFile ? aFile : stderr;
}
#endif // MOZ_LAYERS_HAVE_LOG

namespace {

// XXX pretty general utilities, could centralize

nsACString&
AppendToString(nsACString& s, const void* p,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString("%p", p);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const gfxPattern::GraphicsFilter& f,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  switch (f) {
  case gfxPattern::FILTER_FAST:      s += "fast"; break;
  case gfxPattern::FILTER_GOOD:      s += "good"; break;
  case gfxPattern::FILTER_BEST:      s += "best"; break;
  case gfxPattern::FILTER_NEAREST:   s += "nearest"; break;
  case gfxPattern::FILTER_BILINEAR:  s += "bilinear"; break;
  case gfxPattern::FILTER_GAUSSIAN:  s += "gaussian"; break;
  default:
    NS_ERROR("unknown filter type");
    s += "???";
  }
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, ViewID n,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s.AppendInt(n);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const gfxRGBA& c,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString(
    "rgba(%d, %d, %d, %g)",
    uint8_t(c.r*255.0), uint8_t(c.g*255.0), uint8_t(c.b*255.0), c.a);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const gfx3DMatrix& m,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  if (m.IsIdentity())
    s += "[ I ]";
  else {
    gfxMatrix matrix;
    if (m.Is2D(&matrix)) {
      s += nsPrintfCString(
        "[ %g %g; %g %g; %g %g; ]",
        matrix.xx, matrix.yx, matrix.xy, matrix.yy, matrix.x0, matrix.y0);
    } else {
      s += nsPrintfCString(
        "[ %g %g %g %g; %g %g %g %g; %g %g %g %g; %g %g %g %g; ]",
        m._11, m._12, m._13, m._14,
        m._21, m._22, m._23, m._24,
        m._31, m._32, m._33, m._34,
        m._41, m._42, m._43, m._44);
    }
  }
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntPoint& p,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString("(x=%d, y=%d)", p.x, p.y);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const Point& p,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString("(x=%f, y=%f)", p.x, p.y);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntRect& r,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString(
    "(x=%d, y=%d, w=%d, h=%d)",
    r.x, r.y, r.width, r.height);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const Rect& r,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s.AppendPrintf(
    "(x=%f, y=%f, w=%f, h=%f)",
    r.x, r.y, r.width, r.height);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntRegion& r,
               const char* pfx="", const char* sfx="")
{
  s += pfx;

  nsIntRegionRectIterator it(r);
  s += "< ";
  while (const nsIntRect* sr = it.Next())
    AppendToString(s, *sr) += "; ";
  s += ">";

  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const nsIntSize& sz,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  s += nsPrintfCString("(w=%d, h=%d)", sz.width, sz.height);
  return s += sfx;
}

nsACString&
AppendToString(nsACString& s, const FrameMetrics& m,
               const char* pfx="", const char* sfx="")
{
  s += pfx;
  AppendToString(s, m.mViewport, "{ viewport=");
  AppendToString(s, m.mScrollOffset, " viewportScroll=");
  AppendToString(s, m.mDisplayPort, " displayport=");
  AppendToString(s, m.mScrollId, " scrollId=", " }");
  return s += sfx;
}

} // namespace <anon>

namespace mozilla {
namespace layers {

//--------------------------------------------------
// LayerManager
Layer*
LayerManager::GetPrimaryScrollableLayer()
{
  if (!mRoot) {
    return nullptr;
  }

  nsTArray<Layer*> queue;
  queue.AppendElement(mRoot);
  while (queue.Length()) {
    ContainerLayer* containerLayer = queue[0]->AsContainerLayer();
    queue.RemoveElementAt(0);
    if (!containerLayer) {
      continue;
    }

    const FrameMetrics& frameMetrics = containerLayer->GetFrameMetrics();
    if (frameMetrics.IsScrollable()) {
      return containerLayer;
    }

    Layer* child = containerLayer->GetFirstChild();
    while (child) {
      queue.AppendElement(child);
      child = child->GetNextSibling();
    }
  }

  return mRoot;
}

already_AddRefed<gfxASurface>
LayerManager::CreateOptimalSurface(const gfxIntSize &aSize,
                                   gfxASurface::gfxImageFormat aFormat)
{
  return gfxPlatform::GetPlatform()->
    CreateOffscreenSurface(aSize, gfxASurface::ContentFromFormat(aFormat));
}

already_AddRefed<gfxASurface>
LayerManager::CreateOptimalMaskSurface(const gfxIntSize &aSize)
{
  return CreateOptimalSurface(aSize, gfxASurface::ImageFormatA8);
}

TemporaryRef<DrawTarget>
LayerManager::CreateDrawTarget(const IntSize &aSize,
                               SurfaceFormat aFormat)
{
  return gfxPlatform::GetPlatform()->
    CreateOffscreenDrawTarget(aSize, aFormat);
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
  mContentFlags(0),
  mUseClipRect(false),
  mUseTileSourceRect(false),
  mIsFixedPosition(false),
  mDebugColorIndex(0),
  mAnimationGeneration(0)
{}

Layer::~Layer()
{}

Animation*
Layer::AddAnimation(TimeStamp aStart, TimeDuration aDuration, float aIterations,
                    int aDirection, nsCSSProperty aProperty, const AnimationData& aData)
{
  Animation* anim = mAnimations.AppendElement();
  anim->startTime() = aStart;
  anim->duration() = aDuration;
  anim->numIterations() = aIterations;
  anim->direction() = aDirection;
  anim->property() = aProperty;
  anim->data() = aData;

  Mutated();
  return anim;
}

void
Layer::ClearAnimations()
{
  mAnimations.Clear();
  mAnimationData.Clear();
  Mutated();
}

static nsCSSValueList*
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
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_rotatex, resultTail);
        arr->Item(1).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TRotationY:
      {
        float theta = aFunctions[i].get_RotationY().radians();
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_rotatey, resultTail);
        arr->Item(1).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TRotationZ:
      {
        float theta = aFunctions[i].get_RotationZ().radians();
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_rotatez, resultTail);
        arr->Item(1).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TRotation:
      {
        float theta = aFunctions[i].get_Rotation().radians();
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_rotate, resultTail);
        arr->Item(1).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TRotation3D:
      {
        float x = aFunctions[i].get_Rotation3D().x();
        float y = aFunctions[i].get_Rotation3D().y();
        float z = aFunctions[i].get_Rotation3D().z();
        float theta = aFunctions[i].get_Rotation3D().radians();
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_rotate3d, resultTail);
        arr->Item(1).SetFloatValue(x, eCSSUnit_Number);
        arr->Item(2).SetFloatValue(y, eCSSUnit_Number);
        arr->Item(3).SetFloatValue(z, eCSSUnit_Number);
        arr->Item(4).SetFloatValue(theta, eCSSUnit_Radian);
        break;
      }
      case TransformFunction::TScale:
      {
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_scale3d, resultTail);
        arr->Item(1).SetFloatValue(aFunctions[i].get_Scale().x(), eCSSUnit_Number);
        arr->Item(2).SetFloatValue(aFunctions[i].get_Scale().y(), eCSSUnit_Number);
        arr->Item(3).SetFloatValue(aFunctions[i].get_Scale().z(), eCSSUnit_Number);
        break;
      }
      case TransformFunction::TTranslation:
      {
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_translate3d, resultTail);
        arr->Item(1).SetFloatValue(aFunctions[i].get_Translation().x(), eCSSUnit_Pixel);
        arr->Item(2).SetFloatValue(aFunctions[i].get_Translation().y(), eCSSUnit_Pixel);
        arr->Item(3).SetFloatValue(aFunctions[i].get_Translation().z(), eCSSUnit_Pixel);
        break;
      }
      case TransformFunction::TSkewX:
      {
        float x = aFunctions[i].get_SkewX().x();
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_skewx, resultTail);
        arr->Item(1).SetFloatValue(x, eCSSUnit_Number);
        break;
      }
      case TransformFunction::TSkewY:
      {
        float y = aFunctions[i].get_SkewY().y();
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_skewy, resultTail);
        arr->Item(1).SetFloatValue(y, eCSSUnit_Number);
        break;
      }
      case TransformFunction::TTransformMatrix:
      {
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_matrix3d, resultTail);
        const gfx3DMatrix& matrix = aFunctions[i].get_TransformMatrix().value();
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
        arr = nsStyleAnimation::AppendTransformFunction(eCSSKeyword_perspective, resultTail);
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
  return result.forget();
}

void
Layer::SetAnimations(const AnimationArray& aAnimations)
{
  mAnimations = aAnimations;
  mAnimationData.Clear();
  for (uint32_t i = 0; i < mAnimations.Length(); i++) {
    AnimData* data = mAnimationData.AppendElement();
    InfallibleTArray<css::ComputedTimingFunction*>& functions = data->mFunctions;
    const InfallibleTArray<AnimationSegment>& segments =
      mAnimations.ElementAt(i).segments();
    for (uint32_t j = 0; j < segments.Length(); j++) {
      TimingFunction tf = segments.ElementAt(j).sampleFn();
      css::ComputedTimingFunction* ctf = new css::ComputedTimingFunction();
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

    // Precompute the nsStyleAnimation::Values that we need if this is a transform
    // animation.
    InfallibleTArray<nsStyleAnimation::Value>& startValues = data->mStartValues;
    InfallibleTArray<nsStyleAnimation::Value>& endValues = data->mEndValues;
    for (uint32_t j = 0; j < mAnimations[i].segments().Length(); j++) {
      const AnimationSegment& segment = mAnimations[i].segments()[j];
      nsStyleAnimation::Value* startValue = startValues.AppendElement();
      nsStyleAnimation::Value* endValue = endValues.AppendElement();
      if (segment.endState().type() == Animatable::TArrayOfTransformFunction) {
        const InfallibleTArray<TransformFunction>& startFunctions =
          segment.startState().get_ArrayOfTransformFunction();
        startValue->SetAndAdoptCSSValueListValue(CreateCSSValueList(startFunctions),
                                                 nsStyleAnimation::eUnit_Transform);

        const InfallibleTArray<TransformFunction>& endFunctions =
          segment.endState().get_ArrayOfTransformFunction();
        endValue->SetAndAdoptCSSValueListValue(CreateCSSValueList(endFunctions),
                                               nsStyleAnimation::eUnit_Transform);
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
  if (ShadowLayer* shadow = AsShadowLayer()) {
    return shadow->GetShadowClipRect();
  }
  return GetClipRect();
}

const nsIntRegion&
Layer::GetEffectiveVisibleRegion()
{
  if (ShadowLayer* shadow = AsShadowLayer()) {
    return shadow->GetShadowVisibleRegion();
  }
  return GetVisibleRegion();
}

gfx3DMatrix
Layer::SnapTransformTranslation(const gfx3DMatrix& aTransform,
                                gfxMatrix* aResidualTransform)
{
  if (aResidualTransform) {
    *aResidualTransform = gfxMatrix();
  }

  gfxMatrix matrix2D;
  gfx3DMatrix result;
  if (mManager->IsSnappingEffectiveTransforms() &&
      aTransform.Is2D(&matrix2D) &&
      !matrix2D.HasNonTranslation() &&
      matrix2D.HasNonIntegerTranslation()) {
    gfxPoint snappedTranslation(matrix2D.GetTranslation());
    snappedTranslation.Round();
    gfxMatrix snappedMatrix = gfxMatrix().Translate(snappedTranslation);
    result = gfx3DMatrix::From2D(snappedMatrix);
    if (aResidualTransform) {
      // set aResidualTransform so that aResidual * snappedMatrix == matrix2D.
      // (I.e., appying snappedMatrix after aResidualTransform gives the
      // ideal transform.)
      *aResidualTransform =
        gfxMatrix().Translate(matrix2D.GetTranslation() - snappedTranslation);
    }
  } else {
    result = aTransform;
  }
  return result;
}

gfx3DMatrix
Layer::SnapTransform(const gfx3DMatrix& aTransform,
                     const gfxRect& aSnapRect,
                     gfxMatrix* aResidualTransform)
{
  if (aResidualTransform) {
    *aResidualTransform = gfxMatrix();
  }

  gfxMatrix matrix2D;
  gfx3DMatrix result;
  if (mManager->IsSnappingEffectiveTransforms() &&
      aTransform.Is2D(&matrix2D) &&
      gfxSize(1.0, 1.0) <= aSnapRect.Size() &&
      matrix2D.PreservesAxisAlignedRectangles()) {
    gfxPoint transformedTopLeft = matrix2D.Transform(aSnapRect.TopLeft());
    transformedTopLeft.Round();
    gfxPoint transformedTopRight = matrix2D.Transform(aSnapRect.TopRight());
    transformedTopRight.Round();
    gfxPoint transformedBottomRight = matrix2D.Transform(aSnapRect.BottomRight());
    transformedBottomRight.Round();

    gfxMatrix snappedMatrix = gfxUtils::TransformRectToRect(aSnapRect,
      transformedTopLeft, transformedTopRight, transformedBottomRight);

    result = gfx3DMatrix::From2D(snappedMatrix);
    if (aResidualTransform && !snappedMatrix.IsSingular()) {
      // set aResidualTransform so that aResidual * snappedMatrix == matrix2D.
      // (i.e., appying snappedMatrix after aResidualTransform gives the
      // ideal transform.
      gfxMatrix snappedMatrixInverse = snappedMatrix;
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
  gfxMatrix transform2d;
  return !GetEffectiveTransform().Is2D(&transform2d) ||
         transform2d.HasNonIntegerTranslation() ||
         AncestorLayerMayChangeTransform(this);
}

nsIntRect
Layer::CalculateScissorRect(const nsIntRect& aCurrentScissorRect,
                            const gfxMatrix* aWorldTransform)
{
  ContainerLayer* container = GetParent();
  NS_ASSERTION(container, "This can't be called on the root!");

  // Establish initial clip rect: it's either the one passed in, or
  // if the parent has an intermediate surface, it's the extents of that surface.
  nsIntRect currentClip;
  if (container->UseIntermediateSurface()) {
    currentClip.SizeTo(container->GetIntermediateSurfaceRect().Size());
  } else {
    currentClip = aCurrentScissorRect;
  }

  const nsIntRect *clipRect = GetEffectiveClipRect();
  if (!clipRect)
    return currentClip;

  if (clipRect->IsEmpty()) {
    // We might have a non-translation transform in the container so we can't
    // use the code path below.
    return nsIntRect(currentClip.TopLeft(), nsIntSize(0, 0));
  }

  nsIntRect scissor = *clipRect;
  if (!container->UseIntermediateSurface()) {
    gfxMatrix matrix;
    DebugOnly<bool> is2D = container->GetEffectiveTransform().Is2D(&matrix);
    // See DefaultComputeEffectiveTransforms below
    NS_ASSERTION(is2D && matrix.PreservesAxisAlignedRectangles(),
                 "Non preserves axis aligned transform with clipped child should have forced intermediate surface");
    gfxRect r(scissor.x, scissor.y, scissor.width, scissor.height);
    gfxRect trScissor = matrix.TransformBounds(r);
    trScissor.Round();
    if (!gfxUtils::GfxRectToIntRect(trScissor, &scissor)) {
      return nsIntRect(currentClip.TopLeft(), nsIntSize(0, 0));
    }

    // Find the nearest ancestor with an intermediate surface
    do {
      container = container->GetParent();
    } while (container && !container->UseIntermediateSurface());
  }
  if (container) {
    scissor.MoveBy(-container->GetIntermediateSurfaceRect().TopLeft());
  } else if (aWorldTransform) {
    gfxRect r(scissor.x, scissor.y, scissor.width, scissor.height);
    gfxRect trScissor = aWorldTransform->TransformBounds(r);
    trScissor.Round();
    if (!gfxUtils::GfxRectToIntRect(trScissor, &scissor))
      return nsIntRect(currentClip.TopLeft(), nsIntSize(0, 0));
  }
  return currentClip.Intersect(scissor);
}

const gfx3DMatrix
Layer::GetTransform()
{
  gfx3DMatrix transform = mTransform;
  if (ContainerLayer* c = AsContainerLayer()) {
    transform.Scale(c->GetPreXScale(), c->GetPreYScale(), 1.0f);
  }
  transform.ScalePost(mPostXScale, mPostYScale, 1.0f);
  return transform;
}

const gfx3DMatrix
Layer::GetLocalTransform()
{
  gfx3DMatrix transform;
  if (ShadowLayer* shadow = AsShadowLayer())
    transform = shadow->GetShadowTransform();
  else
    transform = mTransform;
  if (ContainerLayer* c = AsContainerLayer()) {
    transform.Scale(c->GetPreXScale(), c->GetPreYScale(), 1.0f);
  }
  transform.ScalePost(mPostXScale, mPostYScale, 1.0f);
  return transform;
}

void
Layer::ApplyPendingUpdatesForThisTransaction()
{
  if (mPendingTransform && *mPendingTransform != mTransform) {
    mTransform = *mPendingTransform;
    Mutated();
  }
  mPendingTransform = nullptr;
}

const float
Layer::GetLocalOpacity()
{
   if (ShadowLayer* shadow = AsShadowLayer())
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

void
Layer::ComputeEffectiveTransformForMaskLayer(const gfx3DMatrix& aTransformToSurface)
{
  if (mMaskLayer) {
    mMaskLayer->mEffectiveTransform = aTransformToSurface;

#ifdef DEBUG
    gfxMatrix maskTranslation;
    bool maskIs2D = mMaskLayer->GetTransform().CanDraw2D(&maskTranslation);
    NS_ASSERTION(maskIs2D, "How did we end up with a 3D transform here?!");
#endif
    mMaskLayer->mEffectiveTransform.PreMultiply(mMaskLayer->GetTransform());
  }
}

void
ContainerLayer::FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
{
  aAttrs = ContainerLayerAttributes(GetFrameMetrics(), mPreXScale, mPreYScale);
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
ContainerLayer::DefaultComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
{
  gfxMatrix residual;
  gfx3DMatrix idealTransform = GetLocalTransform()*aTransformToSurface;
  idealTransform.ProjectTo2D();
  mEffectiveTransform = SnapTransformTranslation(idealTransform, &residual);

  bool useIntermediateSurface;
  if (GetMaskLayer()) {
    useIntermediateSurface = true;
#ifdef MOZ_DUMP_PAINTING
  } else if (gfxUtils::sDumpPainting) {
    useIntermediateSurface = true;
#endif
  } else {
    float opacity = GetEffectiveOpacity();
    if (opacity != 1.0f && HasMultipleChildren()) {
      useIntermediateSurface = true;
    } else {
      useIntermediateSurface = false;
      gfxMatrix contTransform;
      if (!mEffectiveTransform.Is2D(&contTransform) ||
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
        !contTransform.PreservesAxisAlignedRectangles()) {
#else
        contTransform.HasNonIntegerTranslation()) {
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

  mUseIntermediateSurface = useIntermediateSurface;
  if (useIntermediateSurface) {
    ComputeEffectiveTransformsForChildren(gfx3DMatrix::From2D(residual));
  } else {
    ComputeEffectiveTransformsForChildren(idealTransform);
  }

  if (idealTransform.CanDraw2D()) {
    ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
  } else {
    ComputeEffectiveTransformForMaskLayer(gfx3DMatrix());
  }
}

void
ContainerLayer::ComputeEffectiveTransformsForChildren(const gfx3DMatrix& aTransformToSurface)
{
  for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
    l->ComputeEffectiveTransforms(aTransformToSurface);
  }
}

void
ContainerLayer::DidRemoveChild(Layer* aLayer)
{
  ThebesLayer* tl = aLayer->AsThebesLayer();
  if (tl && tl->UsedForReadback()) {
    for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
      if (l->GetType() == TYPE_READBACK) {
        static_cast<ReadbackLayer*>(l)->NotifyThebesLayerRemoved(tl);
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

void
LayerManager::StartFrameTimeRecording()
{
  mLastFrameTime = TimeStamp::Now();
}

void
LayerManager::PostPresent()
{
  if (!mLastFrameTime.IsNull()) {
    TimeStamp now = TimeStamp::Now();
    mFrameTimes.AppendElement((now - mLastFrameTime).ToMilliseconds());
    mLastFrameTime = now;
  }
  if (!mTabSwitchStart.IsNull()) {
    Telemetry::Accumulate(Telemetry::FX_TAB_SWITCH_TOTAL_MS,
                          uint32_t((TimeStamp::Now() - mTabSwitchStart).ToMilliseconds()));
    mTabSwitchStart = TimeStamp();
  }
}

void
LayerManager::StopFrameTimeRecording(nsTArray<float>& aTimes)
{
  mLastFrameTime = TimeStamp();
  aTimes.SwapElements(mFrameTimes);
  mFrameTimes.Clear();
}

void
LayerManager::BeginTabSwitch()
{
  mTabSwitchStart = TimeStamp::Now();
}

#ifdef MOZ_LAYERS_HAVE_LOG

static nsACString& PrintInfo(nsACString& aTo, ShadowLayer* aShadowLayer);

#ifdef MOZ_DUMP_PAINTING
template <typename T>
void WriteSnapshotLinkToDumpFile(T* aObj, FILE* aFile)
{
  nsCString string(aObj->Name());
  string.Append("-");
  string.AppendInt((uint64_t)aObj);
  fprintf(aFile, "href=\"javascript:ViewImage('%s')\"", string.BeginReading());
}

template <typename T>
void WriteSnapshotToDumpFile_internal(T* aObj, gfxASurface* aSurf)
{
  nsCString string(aObj->Name());
  string.Append("-");
  string.AppendInt((uint64_t)aObj);
  if (gfxUtils::sDumpPaintFile)
    fprintf(gfxUtils::sDumpPaintFile, "array[\"%s\"]=\"", string.BeginReading());
  aSurf->DumpAsDataURL(gfxUtils::sDumpPaintFile);
  if (gfxUtils::sDumpPaintFile)
    fprintf(gfxUtils::sDumpPaintFile, "\";");
}

void WriteSnapshotToDumpFile(Layer* aLayer, gfxASurface* aSurf)
{
  WriteSnapshotToDumpFile_internal(aLayer, aSurf);
}

void WriteSnapshotToDumpFile(LayerManager* aManager, gfxASurface* aSurf)
{
  WriteSnapshotToDumpFile_internal(aManager, aSurf);
}
#endif

void
Layer::Dump(FILE* aFile, const char* aPrefix, bool aDumpHtml)
{
  if (aDumpHtml) {
    fprintf(aFile, "<li><a id=\"%p\" ", this);
#ifdef MOZ_DUMP_PAINTING
    if (GetType() == TYPE_CONTAINER || GetType() == TYPE_THEBES) {
      WriteSnapshotLinkToDumpFile(this, aFile);
    }
#endif
    fprintf(aFile, ">");
  }
  DumpSelf(aFile, aPrefix);
  if (aDumpHtml) {
    fprintf(aFile, "</a>");
  }

  if (Layer* mask = GetMaskLayer()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  Mask layer: ";
    mask->Dump(aFile, pfx.get());
  }

  if (Layer* kid = GetFirstChild()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    if (aDumpHtml) {
      fprintf(aFile, "<ul>");
    }
    kid->Dump(aFile, pfx.get());
    if (aDumpHtml) {
      fprintf(aFile, "</ul>");
    }
  }

  if (aDumpHtml) {
    fprintf(aFile, "</li>");
  }
  if (Layer* next = GetNextSibling())
    next->Dump(aFile, aPrefix, aDumpHtml);
}

void
Layer::DumpSelf(FILE* aFile, const char* aPrefix)
{
  nsAutoCString str;
  PrintInfo(str, aPrefix);
  fprintf(FILEOrDefault(aFile), "%s\n", str.get());
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

  nsAutoCString str;
  PrintInfo(str, aPrefix);
  MOZ_LAYERS_LOG(("%s", str.get()));
}

nsACString&
Layer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  aTo += nsPrintfCString("%s%s (0x%p)", mManager->Name(), Name(), this);

  ::PrintInfo(aTo, AsShadowLayer());

  if (mUseClipRect) {
    AppendToString(aTo, mClipRect, " [clip=", "]");
  }
  if (1.0 != mPostXScale || 1.0 != mPostYScale) {
    aTo.AppendPrintf(" [postScale=%g, %g]", mPostXScale, mPostYScale);
  }
  if (!mTransform.IsIdentity()) {
    AppendToString(aTo, mTransform, " [transform=", "]");
  }
  if (!mVisibleRegion.IsEmpty()) {
    AppendToString(aTo, mVisibleRegion, " [visible=", "]");
  } else {
    aTo += " [not visible]";
  }
  if (1.0 != mOpacity) {
    aTo.AppendPrintf(" [opacity=%g]", mOpacity);
  }
  if (GetContentFlags() & CONTENT_OPAQUE) {
    aTo += " [opaqueContent]";
  }
  if (GetContentFlags() & CONTENT_COMPONENT_ALPHA) {
    aTo += " [componentAlpha]";
  }
  if (GetIsFixedPosition()) {
    aTo += " [isFixedPosition]";
  }

  return aTo;
}

nsACString&
ThebesLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  if (!mValidRegion.IsEmpty()) {
    AppendToString(aTo, mValidRegion, " [valid=", "]");
  }
  return aTo;
}

nsACString&
ContainerLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  if (!mFrameMetrics.IsDefault()) {
    AppendToString(aTo, mFrameMetrics, " [metrics=", "]");
  }
  if (UseIntermediateSurface()) {
    aTo += " [usesTmpSurf]";
  }
  if (1.0 != mPreXScale || 1.0 != mPreYScale) {
    aTo.AppendPrintf(" [preScale=%g, %g]", mPreXScale, mPreYScale);
  }
  return aTo;
}

nsACString&
ColorLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  AppendToString(aTo, mColor, " [color=", "]");
  return aTo;
}

nsACString&
CanvasLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  if (mFilter != gfxPattern::FILTER_GOOD) {
    AppendToString(aTo, mFilter, " [filter=", "]");
  }
  return aTo;
}

nsACString&
ImageLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  if (mFilter != gfxPattern::FILTER_GOOD) {
    AppendToString(aTo, mFilter, " [filter=", "]");
  }
  return aTo;
}

nsACString&
RefLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  ContainerLayer::PrintInfo(aTo, aPrefix);
  if (0 != mId) {
    AppendToString(aTo, mId, " [id=", "]");
  }
  return aTo;
}

nsACString&
ReadbackLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  Layer::PrintInfo(aTo, aPrefix);
  AppendToString(aTo, mSize, " [size=", "]");
  if (mBackgroundLayer) {
    AppendToString(aTo, mBackgroundLayer, " [backgroundLayer=", "]");
    AppendToString(aTo, mBackgroundLayerOffset, " [backgroundOffset=", "]");
  } else if (mBackgroundColor.a == 1.0) {
    AppendToString(aTo, mBackgroundColor, " [backgroundColor=", "]");
  } else {
    aTo += " [nobackground]";
  }
  return aTo;
}

//--------------------------------------------------
// LayerManager

void
LayerManager::Dump(FILE* aFile, const char* aPrefix, bool aDumpHtml)
{
  FILE* file = FILEOrDefault(aFile);

#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml) {
    fprintf(file, "<ul><li><a ");
    WriteSnapshotLinkToDumpFile(this, file);
    fprintf(file, ">");
  }
#endif
  DumpSelf(file, aPrefix);
#ifdef MOZ_DUMP_PAINTING
  if (aDumpHtml) {
    fprintf(file, "</a>");
  }
#endif

  nsAutoCString pfx(aPrefix);
  pfx += "  ";
  if (!GetRoot()) {
    fprintf(file, "%s(null)", pfx.get());
    if (aDumpHtml) {
      fprintf(file, "</li></ul>");
    }
    return;
  }

  if (aDumpHtml) {
    fprintf(file, "<ul>");
  }
  GetRoot()->Dump(file, pfx.get(), aDumpHtml);
  if (aDumpHtml) {
    fprintf(file, "</ul></li></ul>");
  }
  fputc('\n', file);
}

void
LayerManager::DumpSelf(FILE* aFile, const char* aPrefix)
{
  nsAutoCString str;
  PrintInfo(str, aPrefix);
  fprintf(FILEOrDefault(aFile), "%s\n", str.get());
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
  PrintInfo(str, aPrefix);
  MOZ_LAYERS_LOG(("%s", str.get()));
}

nsACString&
LayerManager::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  aTo += aPrefix;
  return aTo += nsPrintfCString("%sLayerManager (0x%p)", Name(), this);
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

static nsACString&
PrintInfo(nsACString& aTo, ShadowLayer* aShadowLayer)
{
  if (!aShadowLayer) {
    return aTo;
  }
  if (const nsIntRect* clipRect = aShadowLayer->GetShadowClipRect()) {
    AppendToString(aTo, *clipRect, " [shadow-clip=", "]");
  }
  if (!aShadowLayer->GetShadowTransform().IsIdentity()) {
    AppendToString(aTo, aShadowLayer->GetShadowTransform(), " [shadow-transform=", "]");
  }
  if (!aShadowLayer->GetShadowVisibleRegion().IsEmpty()) {
    AppendToString(aTo, aShadowLayer->GetShadowVisibleRegion(), " [shadow-visible=", "]");
  }
  return aTo;
}

#else  // !MOZ_LAYERS_HAVE_LOG

void Layer::Dump(FILE* aFile, const char* aPrefix, bool aDumpHtml) {}
void Layer::DumpSelf(FILE* aFile, const char* aPrefix) {}
void Layer::Log(const char* aPrefix) {}
void Layer::LogSelf(const char* aPrefix) {}
nsACString&
Layer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
ThebesLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
ContainerLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
ColorLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
CanvasLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
ImageLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
RefLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

nsACString&
ReadbackLayer::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

void LayerManager::Dump(FILE* aFile, const char* aPrefix, bool aDumpHtml) {}
void LayerManager::DumpSelf(FILE* aFile, const char* aPrefix) {}
void LayerManager::Log(const char* aPrefix) {}
void LayerManager::LogSelf(const char* aPrefix) {}

nsACString&
LayerManager::PrintInfo(nsACString& aTo, const char* aPrefix)
{ return aTo; }

/*static*/ void LayerManager::InitLog() {}
/*static*/ bool LayerManager::IsLogEnabled() { return false; }

#endif // MOZ_LAYERS_HAVE_LOG

PRLogModuleInfo* LayerManager::sLog;

} // namespace layers
} // namespace mozilla
