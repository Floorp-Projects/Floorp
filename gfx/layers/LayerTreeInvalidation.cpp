/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LayerTreeInvalidation.h"

#include <stdint.h>                // for uint32_t
#include "ImageContainer.h"        // for ImageContainer
#include "ImageLayers.h"           // for ImageLayer, etc
#include "Layers.h"                // for Layer, ContainerLayer, etc
#include "Units.h"                 // for ParentLayerIntRect
#include "gfxRect.h"               // for gfxRect
#include "gfxUtils.h"              // for gfxUtils
#include "mozilla/ArrayUtils.h"    // for ArrayEqual
#include "mozilla/gfx/BaseSize.h"  // for BaseSize
#include "mozilla/gfx/Point.h"     // for IntSize
#include "mozilla/mozalloc.h"      // for operator new, etc
#include "nsTHashMap.h"            // for nsTHashMap
#include "nsDebug.h"               // for NS_ASSERTION
#include "nsHashKeys.h"            // for nsPtrHashKey
#include "nsISupportsImpl.h"       // for Layer::AddRef, etc
#include "nsRect.h"                // for IntRect
#include "nsTArray.h"              // for AutoTArray, nsTArray_Impl
#include "mozilla/Poison.h"
#include "mozilla/layers/ImageHost.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "TreeTraversal.h"  // for ForEachNode

// LayerTreeInvalidation debugging
#define LTI_DEBUG 0

#if LTI_DEBUG
#  define LTI_DEEPER(aPrefix) nsPrintfCString("%s  ", aPrefix).get()
#  define LTI_DUMP(rgn, label)                                                \
    if (!(rgn).IsEmpty())                                                     \
      printf_stderr("%s%p: " label " portion is %s\n", aPrefix, mLayer.get(), \
                    Stringify(rgn).c_str());
#  define LTI_LOG(...) printf_stderr(__VA_ARGS__)
#else
#  define LTI_DEEPER(aPrefix) nullptr
#  define LTI_DUMP(rgn, label)
#  define LTI_LOG(...)
#endif

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

struct LayerPropertiesBase;
UniquePtr<LayerPropertiesBase> CloneLayerTreePropertiesInternal(
    Layer* aRoot, bool aIsMask = false);

/**
 * Get accumulated transform of from the context creating layer to the
 * given layer.
 */
static Matrix4x4 GetTransformIn3DContext(Layer* aLayer) {
  Matrix4x4 transform = aLayer->GetLocalTransform();
  for (Layer* layer = aLayer->GetParent(); layer && layer->Extend3DContext();
       layer = layer->GetParent()) {
    transform = transform * layer->GetLocalTransform();
  }
  return transform;
}

/**
 * Get a transform for the given layer depending on extending 3D
 * context.
 *
 * @return local transform for layers not participating 3D rendering
 * context, or the accmulated transform in the context for else.
 */
static Matrix4x4Flagged GetTransformForInvalidation(Layer* aLayer) {
  return (!aLayer->Is3DContextLeaf() && !aLayer->Extend3DContext()
              ? aLayer->GetLocalTransform()
              : GetTransformIn3DContext(aLayer));
}

static IntRect TransformRect(const IntRect& aRect,
                             const Matrix4x4Flagged& aTransform) {
  if (aRect.IsEmpty()) {
    return IntRect();
  }

  Rect rect(aRect.X(), aRect.Y(), aRect.Width(), aRect.Height());
  rect = aTransform.TransformAndClipBounds(rect, Rect::MaxIntRect());
  rect.RoundOut();

  IntRect intRect;
  if (!rect.ToIntRect(&intRect)) {
    intRect = IntRect::MaxIntRect();
  }

  return intRect;
}

static void AddTransformedRegion(nsIntRegion& aDest, const nsIntRegion& aSource,
                                 const Matrix4x4Flagged& aTransform) {
  for (auto iter = aSource.RectIter(); !iter.Done(); iter.Next()) {
    aDest.Or(aDest, TransformRect(iter.Get(), aTransform));
  }
  aDest.SimplifyOutward(20);
}

static void AddRegion(nsIntRegion& aDest, const nsIntRegion& aSource) {
  aDest.Or(aDest, aSource);
  aDest.SimplifyOutward(20);
}

Maybe<IntRect> TransformedBounds(Layer* aLayer) {
  if (aLayer->Extend3DContext()) {
    ContainerLayer* container = aLayer->AsContainerLayer();
    MOZ_ASSERT(container);
    IntRect result;
    for (Layer* child = container->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      Maybe<IntRect> childBounds = TransformedBounds(child);
      if (!childBounds) {
        return Nothing();
      }
      Maybe<IntRect> combined = result.SafeUnion(childBounds.value());
      if (!combined) {
        LTI_LOG("overflowed bounds of container %p accumulating child %p\n",
                container, child);
        return Nothing();
      }
      result = combined.value();
    }
    return Some(result);
  }

  return Some(
      TransformRect(aLayer->GetLocalVisibleRegion().GetBounds().ToUnknownRect(),
                    GetTransformForInvalidation(aLayer)));
}

/**
 * Walks over this layer, and all descendant layers.
 * If any of these are a ContainerLayer that reports invalidations to a
 * PresShell, then report that the entire bounds have changed.
 */
static void NotifySubdocumentInvalidation(
    Layer* aLayer, NotifySubDocInvalidationFunc aCallback) {
  ForEachNode<ForwardIterator>(
      aLayer,
      [aCallback](Layer* layer) {
        layer->ClearInvalidRegion();
        if (layer->GetMaskLayer()) {
          NotifySubdocumentInvalidation(layer->GetMaskLayer(), aCallback);
        }
        for (size_t i = 0; i < layer->GetAncestorMaskLayerCount(); i++) {
          Layer* maskLayer = layer->GetAncestorMaskLayerAt(i);
          NotifySubdocumentInvalidation(maskLayer, aCallback);
        }
      },
      [aCallback](Layer* layer) {
        ContainerLayer* container = layer->AsContainerLayer();
        if (container && !container->Extend3DContext()) {
          nsIntRegion region =
              container->GetLocalVisibleRegion().ToUnknownRegion();
          aCallback(container, &region);
        }
      });
}

static void SetChildrenChangedRecursive(Layer* aLayer) {
  ForEachNode<ForwardIterator>(aLayer, [](Layer* layer) {
    ContainerLayer* container = layer->AsContainerLayer();
    if (container) {
      container->SetChildrenChanged(true);
      container->SetInvalidCompositeRect(nullptr);
    }
  });
}

struct LayerPropertiesBase : public LayerProperties {
  explicit LayerPropertiesBase(Layer* aLayer)
      : mLayer(aLayer),
        mMaskLayer(nullptr),
        mVisibleRegion(mLayer->Extend3DContext()
                           ? nsIntRegion()
                           : mLayer->GetLocalVisibleRegion().ToUnknownRegion()),
        mPostXScale(aLayer->GetPostXScale()),
        mPostYScale(aLayer->GetPostYScale()),
        mOpacity(aLayer->GetLocalOpacity()),
        mUseClipRect(!!aLayer->GetLocalClipRect()) {
    MOZ_COUNT_CTOR(LayerPropertiesBase);
    if (aLayer->GetMaskLayer()) {
      mMaskLayer =
          CloneLayerTreePropertiesInternal(aLayer->GetMaskLayer(), true);
    }
    for (size_t i = 0; i < aLayer->GetAncestorMaskLayerCount(); i++) {
      Layer* maskLayer = aLayer->GetAncestorMaskLayerAt(i);
      mAncestorMaskLayers.AppendElement(
          CloneLayerTreePropertiesInternal(maskLayer, true));
    }
    if (mUseClipRect) {
      mClipRect = *aLayer->GetLocalClipRect();
    }
    mTransform = GetTransformForInvalidation(aLayer);
  }
  LayerPropertiesBase()
      : mLayer(nullptr),
        mMaskLayer(nullptr),
        mPostXScale(0.0),
        mPostYScale(0.0),
        mOpacity(0.0),
        mUseClipRect(false) {
    MOZ_COUNT_CTOR(LayerPropertiesBase);
  }
  MOZ_COUNTED_DTOR_OVERRIDE(LayerPropertiesBase)

 protected:
  LayerPropertiesBase(const LayerPropertiesBase& a) = delete;
  LayerPropertiesBase& operator=(const LayerPropertiesBase& a) = delete;

 public:
  bool ComputeDifferences(Layer* aRoot, nsIntRegion& aOutRegion,
                          NotifySubDocInvalidationFunc aCallback) override;

  void MoveBy(const IntPoint& aOffset) override;

  bool ComputeChange(const char* aPrefix, nsIntRegion& aOutRegion,
                     NotifySubDocInvalidationFunc aCallback) {
    // Bug 1251615: This canary is sometimes hit. We're still not sure why.
    mCanary.Check();
    bool transformChanged =
        !mTransform.FuzzyEqual(GetTransformForInvalidation(mLayer)) ||
        mLayer->GetPostXScale() != mPostXScale ||
        mLayer->GetPostYScale() != mPostYScale;
    const Maybe<ParentLayerIntRect>& otherClip = mLayer->GetLocalClipRect();
    nsIntRegion result;

    bool ancestorMaskChanged =
        mAncestorMaskLayers.Length() != mLayer->GetAncestorMaskLayerCount();
    if (!ancestorMaskChanged) {
      for (size_t i = 0; i < mAncestorMaskLayers.Length(); i++) {
        if (mLayer->GetAncestorMaskLayerAt(i) !=
            mAncestorMaskLayers[i]->mLayer) {
          ancestorMaskChanged = true;
          break;
        }
      }
    }

    // Note that we don't bailout early in general since this function
    // clears some persistent state at the end. Instead we set an overflow
    // flag and check it right before returning.
    bool areaOverflowed = false;

    Layer* otherMask = mLayer->GetMaskLayer();
    if ((mMaskLayer ? mMaskLayer->mLayer : nullptr) != otherMask ||
        ancestorMaskChanged || (mUseClipRect != !!otherClip) ||
        mLayer->GetLocalOpacity() != mOpacity || transformChanged) {
      Maybe<IntRect> oldBounds = OldTransformedBounds();
      Maybe<IntRect> newBounds = NewTransformedBounds();
      if (oldBounds && newBounds) {
        LTI_DUMP(oldBounds.value(), "oldtransform");
        LTI_DUMP(newBounds.value(), "newtransform");
        result = oldBounds.value();
        AddRegion(result, newBounds.value());
      } else {
        areaOverflowed = true;
      }

      // We can't bail out early because we might need to update some internal
      // layer state.
    }

    nsIntRegion internal;
    if (!ComputeChangeInternal(aPrefix, internal, aCallback)) {
      areaOverflowed = true;
    }

    LTI_DUMP(internal, "internal");
    AddRegion(result, internal);
    LTI_DUMP(mLayer->GetInvalidRegion().GetRegion(), "invalid");
    AddTransformedRegion(result, mLayer->GetInvalidRegion().GetRegion(),
                         mTransform);

    if (mMaskLayer && otherMask) {
      nsIntRegion mask;
      if (!mMaskLayer->ComputeChange(aPrefix, mask, aCallback)) {
        areaOverflowed = true;
      }
      LTI_DUMP(mask, "mask");
      AddRegion(result, mask);
    }

    for (size_t i = 0; i < std::min(mAncestorMaskLayers.Length(),
                                    mLayer->GetAncestorMaskLayerCount());
         i++) {
      nsIntRegion mask;
      if (!mAncestorMaskLayers[i]->ComputeChange(aPrefix, mask, aCallback)) {
        areaOverflowed = true;
      }
      LTI_DUMP(mask, "ancestormask");
      AddRegion(result, mask);
    }

    if (mUseClipRect && otherClip) {
      if (!mClipRect.IsEqualInterior(*otherClip)) {
        nsIntRegion tmp;
        tmp.Xor(mClipRect.ToUnknownRect(), otherClip->ToUnknownRect());
        LTI_DUMP(tmp, "clip");
        AddRegion(result, tmp);
      }
    }

    mLayer->ClearInvalidRegion();

    if (areaOverflowed) {
      return false;
    }

    aOutRegion = std::move(result);
    return true;
  }

  void CheckCanary() {
    mCanary.Check();
    mLayer->CheckCanary();
  }

  IntRect NewTransformedBoundsForLeaf() {
    return TransformRect(
        mLayer->GetLocalVisibleRegion().GetBounds().ToUnknownRect(),
        GetTransformForInvalidation(mLayer));
  }

  IntRect OldTransformedBoundsForLeaf() {
    return TransformRect(mVisibleRegion.GetBounds().ToUnknownRect(),
                         mTransform);
  }

  Maybe<IntRect> NewTransformedBounds() { return TransformedBounds(mLayer); }

  virtual Maybe<IntRect> OldTransformedBounds() {
    return Some(
        TransformRect(mVisibleRegion.GetBounds().ToUnknownRect(), mTransform));
  }

  virtual bool ComputeChangeInternal(const char* aPrefix,
                                     nsIntRegion& aOutRegion,
                                     NotifySubDocInvalidationFunc aCallback) {
    if (mLayer->AsHostLayer() &&
        !mLayer->GetLocalVisibleRegion().ToUnknownRegion().IsEqual(
            mVisibleRegion)) {
      IntRect result = NewTransformedBoundsForLeaf();
      result = result.Union(OldTransformedBoundsForLeaf());
      aOutRegion = result;
    }
    return true;
  }

  RefPtr<Layer> mLayer;
  UniquePtr<LayerPropertiesBase> mMaskLayer;
  nsTArray<UniquePtr<LayerPropertiesBase>> mAncestorMaskLayers;
  nsIntRegion mVisibleRegion;
  Matrix4x4Flagged mTransform;
  float mPostXScale;
  float mPostYScale;
  float mOpacity;
  ParentLayerIntRect mClipRect;
  bool mUseClipRect;
  mozilla::CorruptionCanary mCanary;
};

struct ContainerLayerProperties : public LayerPropertiesBase {
  explicit ContainerLayerProperties(ContainerLayer* aLayer)
      : LayerPropertiesBase(aLayer),
        mPreXScale(aLayer->GetPreXScale()),
        mPreYScale(aLayer->GetPreYScale()) {
    for (Layer* child = aLayer->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      child->CheckCanary();
      mChildren.AppendElement(CloneLayerTreePropertiesInternal(child));
    }
  }

 protected:
  ContainerLayerProperties(const ContainerLayerProperties& a) = delete;
  ContainerLayerProperties& operator=(const ContainerLayerProperties& a) =
      delete;

 public:
  bool ComputeChangeInternal(const char* aPrefix, nsIntRegion& aOutRegion,
                             NotifySubDocInvalidationFunc aCallback) override {
    // Make sure we got our virtual call right
    mSubtypeCanary.Check();
    ContainerLayer* container = mLayer->AsContainerLayer();
    nsIntRegion invalidOfLayer;  // Invalid regions of this layer.
    nsIntRegion result;          // Invliad regions for children only.

    container->CheckCanary();

    bool childrenChanged = false;
    bool invalidateWholeLayer = false;
    bool areaOverflowed = false;
    if (mPreXScale != container->GetPreXScale() ||
        mPreYScale != container->GetPreYScale()) {
      Maybe<IntRect> oldBounds = OldTransformedBounds();
      Maybe<IntRect> newBounds = NewTransformedBounds();
      if (oldBounds && newBounds) {
        invalidOfLayer = oldBounds.value();
        AddRegion(invalidOfLayer, newBounds.value());
      } else {
        areaOverflowed = true;
      }
      childrenChanged = true;
      invalidateWholeLayer = true;

      // Can't bail out early, we need to update the child container layers
    }

    // A low frame rate is especially visible to users when scrolling, so we
    // particularly want to avoid unnecessary invalidation at that time. For us
    // here, that means avoiding unnecessary invalidation of child items when
    // other children are added to or removed from our container layer, since
    // that may be caused by children being scrolled in or out of view. We are
    // less concerned with children changing order.
    // TODO: Consider how we could avoid unnecessary invalidation when children
    // change order, and whether the overhead would be worth it.

    nsTHashMap<nsPtrHashKey<Layer>, uint32_t> oldIndexMap(mChildren.Length());
    for (uint32_t i = 0; i < mChildren.Length(); ++i) {
      mChildren[i]->CheckCanary();
      oldIndexMap.InsertOrUpdate(mChildren[i]->mLayer, i);
    }

    uint32_t i = 0;  // cursor into the old child list mChildren
    for (Layer* child = container->GetFirstChild(); child;
         child = child->GetNextSibling()) {
      bool invalidateChildsCurrentArea = false;
      if (i < mChildren.Length()) {
        uint32_t childsOldIndex;
        if (oldIndexMap.Get(child, &childsOldIndex)) {
          if (childsOldIndex >= i) {
            // Invalidate the old areas of layers that used to be between the
            // current |child| and the previous |child| that was also in the
            // old list mChildren (if any of those children have been reordered
            // rather than removed, we will invalidate their new area when we
            // encounter them in the new list):
            for (uint32_t j = i; j < childsOldIndex; ++j) {
              if (Maybe<IntRect> bounds =
                      mChildren[j]->OldTransformedBounds()) {
                LTI_DUMP(bounds.value(), "reordered child");
                AddRegion(result, bounds.value());
              } else {
                areaOverflowed = true;
              }
              childrenChanged |= true;
            }
            if (childsOldIndex >= mChildren.Length()) {
              MOZ_CRASH("Out of bounds");
            }
            // Invalidate any regions of the child that have changed:
            nsIntRegion region;
            if (!mChildren[childsOldIndex]->ComputeChange(LTI_DEEPER(aPrefix),
                                                          region, aCallback)) {
              areaOverflowed = true;
            }
            i = childsOldIndex + 1;
            if (!region.IsEmpty()) {
              LTI_LOG("%s%p: child %p produced %s\n", aPrefix, mLayer.get(),
                      mChildren[childsOldIndex]->mLayer.get(),
                      Stringify(region).c_str());
              AddRegion(result, region);
              childrenChanged |= true;
            }
          } else {
            // We've already seen this child in mChildren (which means it must
            // have been reordered) and invalidated its old area. We need to
            // invalidate its new area too:
            invalidateChildsCurrentArea = true;
          }
        } else {
          // |child| is new
          invalidateChildsCurrentArea = true;
          SetChildrenChangedRecursive(child);
        }
      } else {
        // |child| is new, or was reordered to a higher index
        invalidateChildsCurrentArea = true;
        if (!oldIndexMap.Contains(child)) {
          SetChildrenChangedRecursive(child);
        }
      }
      if (invalidateChildsCurrentArea) {
        LTI_DUMP(child->GetLocalVisibleRegion().ToUnknownRegion(),
                 "invalidateChildsCurrentArea");
        if (Maybe<IntRect> bounds = TransformedBounds(child)) {
          AddRegion(result, bounds.value());
        } else {
          areaOverflowed = true;
        }
        if (aCallback) {
          NotifySubdocumentInvalidation(child, aCallback);
        } else {
          ClearInvalidations(child);
        }
      }
      childrenChanged |= invalidateChildsCurrentArea;
    }

    // Process remaining removed children.
    while (i < mChildren.Length()) {
      childrenChanged |= true;
      if (Maybe<IntRect> bounds = mChildren[i]->OldTransformedBounds()) {
        LTI_DUMP(bounds.value(), "removed child");
        AddRegion(result, bounds.value());
      } else {
        areaOverflowed = true;
      }
      i++;
    }

    if (aCallback) {
      aCallback(container, areaOverflowed ? nullptr : &result);
    }

    if (childrenChanged || areaOverflowed) {
      container->SetChildrenChanged(true);
    }

    if (container->UseIntermediateSurface()) {
      Maybe<IntRect> bounds;
      if (!invalidateWholeLayer && !areaOverflowed) {
        bounds = Some(result.GetBounds());

        // Process changes in the visible region.
        IntRegion newVisible =
            mLayer->GetLocalVisibleRegion().ToUnknownRegion();
        if (!newVisible.IsEqual(mVisibleRegion)) {
          newVisible.XorWith(mVisibleRegion);
          bounds = bounds->SafeUnion(newVisible.GetBounds());
        }
      }
      container->SetInvalidCompositeRect(bounds ? bounds.ptr() : nullptr);
    }

    // Safe to bail out early now, persistent state has been set.
    if (areaOverflowed) {
      return false;
    }

    if (!mLayer->Extend3DContext()) {
      // |result| contains invalid regions only of children.
      result.Transform(GetTransformForInvalidation(mLayer).GetMatrix());
    }
    // else, effective transforms have applied on children.

    LTI_DUMP(invalidOfLayer, "invalidOfLayer");
    result.OrWith(invalidOfLayer);

    aOutRegion = std::move(result);
    return true;
  }

  Maybe<IntRect> OldTransformedBounds() override {
    if (mLayer->Extend3DContext()) {
      IntRect result;
      for (UniquePtr<LayerPropertiesBase>& child : mChildren) {
        Maybe<IntRect> childBounds = child->OldTransformedBounds();
        if (!childBounds) {
          return Nothing();
        }
        Maybe<IntRect> combined = result.SafeUnion(childBounds.value());
        if (!combined) {
          LTI_LOG("overflowed bounds of container %p accumulating child %p\n",
                  this, child->mLayer.get());
          return Nothing();
        }
        result = combined.value();
      }
      return Some(result);
    }
    return LayerPropertiesBase::OldTransformedBounds();
  }

  // The old list of children:
  mozilla::CorruptionCanary mSubtypeCanary;
  nsTArray<UniquePtr<LayerPropertiesBase>> mChildren;
  float mPreXScale;
  float mPreYScale;
};

struct ColorLayerProperties : public LayerPropertiesBase {
  explicit ColorLayerProperties(ColorLayer* aLayer)
      : LayerPropertiesBase(aLayer),
        mColor(aLayer->GetColor()),
        mBounds(aLayer->GetBounds()) {}

 protected:
  ColorLayerProperties(const ColorLayerProperties& a) = delete;
  ColorLayerProperties& operator=(const ColorLayerProperties& a) = delete;

 public:
  bool ComputeChangeInternal(const char* aPrefix, nsIntRegion& aOutRegion,
                             NotifySubDocInvalidationFunc aCallback) override {
    ColorLayer* color = static_cast<ColorLayer*>(mLayer.get());

    if (mColor != color->GetColor()) {
      LTI_DUMP(NewTransformedBoundsForLeaf(), "color");
      aOutRegion = NewTransformedBoundsForLeaf();
      return true;
    }

    nsIntRegion boundsDiff;
    boundsDiff.Xor(mBounds, color->GetBounds());
    LTI_DUMP(boundsDiff, "colorbounds");

    AddTransformedRegion(aOutRegion, boundsDiff, mTransform);
    return true;
  }

  DeviceColor mColor;
  IntRect mBounds;
};

static ImageHost* GetImageHost(Layer* aLayer) {
  HostLayer* compositor = aLayer->AsHostLayer();
  if (compositor) {
    return static_cast<ImageHost*>(compositor->GetCompositableHost());
  }
  return nullptr;
}

struct ImageLayerProperties : public LayerPropertiesBase {
  explicit ImageLayerProperties(ImageLayer* aImage, bool aIsMask)
      : LayerPropertiesBase(aImage),
        mContainer(aImage->GetContainer()),
        mImageHost(GetImageHost(aImage)),
        mSamplingFilter(aImage->GetSamplingFilter()),
        mScaleToSize(aImage->GetScaleToSize()),
        mScaleMode(aImage->GetScaleMode()),
        mLastProducerID(-1),
        mLastFrameID(-1),
        mIsMask(aIsMask) {
    if (mImageHost) {
      if (aIsMask) {
        // Mask layers never set the 'last' producer/frame
        // id, since they never get composited as their own
        // layer.
        mLastProducerID = mImageHost->GetProducerID();
        mLastFrameID = mImageHost->GetFrameID();
      } else {
        mLastProducerID = mImageHost->GetLastProducerID();
        mLastFrameID = mImageHost->GetLastFrameID();
      }
    }
  }

  bool ComputeChangeInternal(const char* aPrefix, nsIntRegion& aOutRegion,
                             NotifySubDocInvalidationFunc aCallback) override {
    ImageLayer* imageLayer = static_cast<ImageLayer*>(mLayer.get());

    if (!imageLayer->GetLocalVisibleRegion().ToUnknownRegion().IsEqual(
            mVisibleRegion)) {
      IntRect result = NewTransformedBoundsForLeaf();
      result = result.Union(OldTransformedBoundsForLeaf());
      aOutRegion = result;
      return true;
    }

    ImageContainer* container = imageLayer->GetContainer();
    ImageHost* host = GetImageHost(imageLayer);
    if (mContainer != container ||
        mSamplingFilter != imageLayer->GetSamplingFilter() ||
        mScaleToSize != imageLayer->GetScaleToSize() ||
        mScaleMode != imageLayer->GetScaleMode() || host != mImageHost ||
        (host && host->GetProducerID() != mLastProducerID) ||
        (host && host->GetFrameID() != mLastFrameID)) {
      if (mIsMask) {
        // Mask layers have an empty visible region, so we have to
        // use the image size instead.
        IntSize size;
        if (container) {
          size = container->GetCurrentSize();
        }
        if (host) {
          size = host->GetImageSize();
        }
        IntRect rect(0, 0, size.width, size.height);
        LTI_DUMP(rect, "mask");
        aOutRegion = TransformRect(rect, GetTransformForInvalidation(mLayer));
        return true;
      }
      LTI_DUMP(NewTransformedBoundsForLeaf(), "bounds");
      aOutRegion = NewTransformedBoundsForLeaf();
      return true;
    }

    return true;
  }

  RefPtr<ImageContainer> mContainer;
  RefPtr<ImageHost> mImageHost;
  SamplingFilter mSamplingFilter;
  gfx::IntSize mScaleToSize;
  ScaleMode mScaleMode;
  int32_t mLastProducerID;
  int32_t mLastFrameID;
  bool mIsMask;
};

struct CanvasLayerProperties : public LayerPropertiesBase {
  explicit CanvasLayerProperties(CanvasLayer* aCanvas)
      : LayerPropertiesBase(aCanvas), mImageHost(GetImageHost(aCanvas)) {
    mFrameID = mImageHost ? mImageHost->GetFrameID() : -1;
  }

  bool ComputeChangeInternal(const char* aPrefix, nsIntRegion& aOutRegion,
                             NotifySubDocInvalidationFunc aCallback) override {
    CanvasLayer* canvasLayer = static_cast<CanvasLayer*>(mLayer.get());

    ImageHost* host = GetImageHost(canvasLayer);
    if (host && host->GetFrameID() != mFrameID) {
      LTI_DUMP(NewTransformedBoundsForLeaf(), "frameId");
      aOutRegion = NewTransformedBoundsForLeaf();
      return true;
    }

    return true;
  }

  RefPtr<ImageHost> mImageHost;
  int32_t mFrameID;
};

UniquePtr<LayerPropertiesBase> CloneLayerTreePropertiesInternal(
    Layer* aRoot, bool aIsMask /* = false */) {
  if (!aRoot) {
    return MakeUnique<LayerPropertiesBase>();
  }

  MOZ_ASSERT(!aIsMask || aRoot->GetType() == Layer::TYPE_IMAGE);

  aRoot->CheckCanary();

  switch (aRoot->GetType()) {
    case Layer::TYPE_CONTAINER:
    case Layer::TYPE_REF:
      return MakeUnique<ContainerLayerProperties>(aRoot->AsContainerLayer());
    case Layer::TYPE_COLOR:
      return MakeUnique<ColorLayerProperties>(static_cast<ColorLayer*>(aRoot));
    case Layer::TYPE_IMAGE:
      return MakeUnique<ImageLayerProperties>(static_cast<ImageLayer*>(aRoot),
                                              aIsMask);
    case Layer::TYPE_CANVAS:
      return MakeUnique<CanvasLayerProperties>(
          static_cast<CanvasLayer*>(aRoot));
    case Layer::TYPE_DISPLAYITEM:
    case Layer::TYPE_READBACK:
    case Layer::TYPE_SHADOW:
    case Layer::TYPE_PAINTED:
      return MakeUnique<LayerPropertiesBase>(aRoot);
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected root layer type");
  return MakeUnique<LayerPropertiesBase>(aRoot);
}

/* static */
UniquePtr<LayerProperties> LayerProperties::CloneFrom(Layer* aRoot) {
  return CloneLayerTreePropertiesInternal(aRoot);
}

/* static */
void LayerProperties::ClearInvalidations(Layer* aLayer) {
  ForEachNode<ForwardIterator>(aLayer, [](Layer* layer) {
    layer->ClearInvalidRegion();
    if (layer->GetMaskLayer()) {
      ClearInvalidations(layer->GetMaskLayer());
    }
    for (size_t i = 0; i < layer->GetAncestorMaskLayerCount(); i++) {
      ClearInvalidations(layer->GetAncestorMaskLayerAt(i));
    }
  });
}

bool LayerPropertiesBase::ComputeDifferences(
    Layer* aRoot, nsIntRegion& aOutRegion,
    NotifySubDocInvalidationFunc aCallback) {
  NS_ASSERTION(aRoot, "Must have a layer tree to compare against!");
  if (mLayer != aRoot) {
    if (aCallback) {
      NotifySubdocumentInvalidation(aRoot, aCallback);
    } else {
      ClearInvalidations(aRoot);
    }
    IntRect bounds = TransformRect(
        aRoot->GetLocalVisibleRegion().GetBounds().ToUnknownRect(),
        aRoot->GetLocalTransform());
    Maybe<IntRect> oldBounds = OldTransformedBounds();
    if (!oldBounds) {
      return false;
    }
    Maybe<IntRect> result = bounds.SafeUnion(oldBounds.value());
    if (!result) {
      LTI_LOG("overflowed bounds computing the union of layers %p and %p\n",
              mLayer.get(), aRoot);
      return false;
    }
    aOutRegion = result.value();
    return true;
  }
  return ComputeChange("  ", aOutRegion, aCallback);
}

void LayerPropertiesBase::MoveBy(const IntPoint& aOffset) {
  mTransform.PostTranslate(aOffset.x, aOffset.y, 0);
}

}  // namespace layers
}  // namespace mozilla
