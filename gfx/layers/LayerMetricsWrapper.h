/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERMETRICSWRAPPER_H
#define GFX_LAYERMETRICSWRAPPER_H

#include "Layers.h"
#include "UnitTransforms.h"

namespace mozilla {
namespace layers {

/**
 * A wrapper class around a target Layer with that allows user code to
 * walk through the FrameMetrics objects on the layer the same way it
 * would walk through a ContainerLayer hierarchy. Consider the following
 * layer tree:
 *
 *                    +---+
 *                    | A |
 *                    +---+
 *                   /  |  \
 *                  /   |   \
 *                 /    |    \
 *            +---+  +-----+  +---+
 *            | B |  |  C  |  | D |
 *            +---+  +-----+  +---+
 *                   | FMn |
 *                   |  .  |
 *                   |  .  |
 *                   |  .  |
 *                   | FM1 |
 *                   | FM0 |
 *                   +-----+
 *                   /     \
 *                  /       \
 *             +---+         +---+
 *             | E |         | F |
 *             +---+         +---+
 *
 * In this layer tree, there are six layers with A being the root and B,D,E,F
 * being leaf nodes. Layer C is in the middle and has n+1 FrameMetrics, labelled
 * FM0...FMn. FM0 is the FrameMetrics you get by calling c->GetFrameMetrics(0)
 * and FMn is the FrameMetrics you can obtain by calling
 * c->GetFrameMetrics(c->GetScrollMetadataCount() - 1). This layer tree is
 * conceptually equivalent to this one below:
 *
 *                    +---+
 *                    | A |
 *                    +---+
 *                   /  |  \
 *                  /   |   \
 *                 /    |    \
 *            +---+  +-----+  +---+
 *            | B |  | Cn  |  | D |
 *            +---+  +-----+  +---+
 *                      |
 *                      .
 *                      .
 *                      .
 *                      |
 *                   +-----+
 *                   | C1  |
 *                   +-----+
 *                      |
 *                   +-----+
 *                   | C0  |
 *                   +-----+
 *                   /     \
 *                  /       \
 *             +---+         +---+
 *             | E |         | F |
 *             +---+         +---+
 *
 * In this layer tree, the layer C has been expanded into a stack of container
 * layers C1...Cn, where C1 has FrameMetrics FM1 and Cn has FrameMetrics Fn.
 * Although in this example C (in the first layer tree) and C0 (in the second
 * layer tree) are both ContainerLayers (because they have children), they
 * do not have to be. They may just be PaintedLayers or ColorLayers, for example,
 * which do not have any children. However, the type of C will always be the
 * same as the type of C0.
 *
 * The LayerMetricsWrapper class allows client code to treat the first layer
 * tree as though it were the second. That is, instead of client code having
 * to iterate through the FrameMetrics objects directly, it can use a
 * LayerMetricsWrapper to encapsulate that aspect of the layer tree and just
 * walk the tree as if it were a stack of ContainerLayers.
 *
 * The functions on this class do different things depending on which
 * simulated ContainerLayer is being wrapped. For example, if the
 * LayerMetricsWrapper is pretending to be C0, the GetNextSibling() function
 * will return null even though the underlying layer C does actually have
 * a next sibling. The LayerMetricsWrapper pretending to be Cn will return
 * D as the next sibling.
 *
 * Implementation notes:
 *
 * The AtTopLayer() and AtBottomLayer() functions in this class refer to
 * Cn and C0 in the second layer tree above; that is, they are predicates
 * to test if the LayerMetricsWrapper is simulating the topmost or bottommost
 * layer, as those will have special behaviour.
 *
 * It is possible to wrap a nullptr in a LayerMetricsWrapper, in which case
 * the IsValid() function will return false. This is required to allow
 * LayerMetricsWrapper to be a MOZ_STACK_CLASS (desirable because it is used
 * in loops and recursion).
 *
 * This class purposely does not expose the wrapped layer directly to avoid
 * user code from accidentally calling functions directly on it. Instead
 * any necessary functions should be wrapped in this class. It does expose
 * the wrapped layer as a void* for printf purposes.
 *
 * The implementation may look like it special-cases mIndex == 0 and/or
 * GetScrollMetadataCount() == 0. This is an artifact of the fact that both
 * mIndex and GetScrollMetadataCount() are uint32_t and GetScrollMetadataCount()
 * can return 0 but mIndex cannot store -1. This seems better than the
 * alternative of making mIndex a int32_t that can store -1, but then having
 * to cast to uint32_t all over the place.
 */
class MOZ_STACK_CLASS LayerMetricsWrapper {
public:
  enum StartAt {
    TOP,
    BOTTOM,
  };

  LayerMetricsWrapper()
    : mLayer(nullptr)
    , mIndex(0)
  {
  }

  explicit LayerMetricsWrapper(Layer* aRoot, StartAt aStart = StartAt::TOP)
    : mLayer(aRoot)
    , mIndex(0)
  {
    if (!mLayer) {
      return;
    }

    switch (aStart) {
      case StartAt::TOP:
        mIndex = mLayer->GetScrollMetadataCount();
        if (mIndex > 0) {
          mIndex--;
        }
        break;
      case StartAt::BOTTOM:
        mIndex = 0;
        break;
      default:
        MOZ_ASSERT_UNREACHABLE("Unknown startAt value");
        break;
    }
  }

  explicit LayerMetricsWrapper(Layer* aLayer, uint32_t aMetricsIndex)
    : mLayer(aLayer)
    , mIndex(aMetricsIndex)
  {
    MOZ_ASSERT(mLayer);
    MOZ_ASSERT(mIndex == 0 || mIndex < mLayer->GetScrollMetadataCount());
  }

  bool IsValid() const
  {
    return mLayer != nullptr;
  }

  explicit operator bool() const
  {
    return IsValid();
  }

  bool IsScrollInfoLayer() const
  {
    MOZ_ASSERT(IsValid());

    return Metrics().IsScrollInfoLayer();
  }

  LayerMetricsWrapper GetParent() const
  {
    MOZ_ASSERT(IsValid());

    if (!AtTopLayer()) {
      return LayerMetricsWrapper(mLayer, mIndex + 1);
    }
    if (mLayer->GetParent()) {
      return LayerMetricsWrapper(mLayer->GetParent(), StartAt::BOTTOM);
    }
    return LayerMetricsWrapper(nullptr);
  }

  LayerMetricsWrapper GetFirstChild() const
  {
    MOZ_ASSERT(IsValid());

    if (!AtBottomLayer()) {
      return LayerMetricsWrapper(mLayer, mIndex - 1);
    }
    return LayerMetricsWrapper(mLayer->GetFirstChild());
  }

  LayerMetricsWrapper GetLastChild() const
  {
    MOZ_ASSERT(IsValid());

    if (!AtBottomLayer()) {
      return LayerMetricsWrapper(mLayer, mIndex - 1);
    }
    return LayerMetricsWrapper(mLayer->GetLastChild());
  }

  LayerMetricsWrapper GetPrevSibling() const
  {
    MOZ_ASSERT(IsValid());

    if (AtTopLayer()) {
      return LayerMetricsWrapper(mLayer->GetPrevSibling());
    }
    return LayerMetricsWrapper(nullptr);
  }

  LayerMetricsWrapper GetNextSibling() const
  {
    MOZ_ASSERT(IsValid());

    if (AtTopLayer()) {
      return LayerMetricsWrapper(mLayer->GetNextSibling());
    }
    return LayerMetricsWrapper(nullptr);
  }

  const ScrollMetadata& Metadata() const
  {
    MOZ_ASSERT(IsValid());

    if (mIndex >= mLayer->GetScrollMetadataCount()) {
      return *ScrollMetadata::sNullMetadata;
    }
    return mLayer->GetScrollMetadata(mIndex);
  }

  const FrameMetrics& Metrics() const
  {
    return Metadata().GetMetrics();
  }

  AsyncPanZoomController* GetApzc() const
  {
    MOZ_ASSERT(IsValid());

    if (mIndex >= mLayer->GetScrollMetadataCount()) {
      return nullptr;
    }
    return mLayer->GetAsyncPanZoomController(mIndex);
  }

  void SetApzc(AsyncPanZoomController* aApzc) const
  {
    MOZ_ASSERT(IsValid());

    if (mLayer->GetScrollMetadataCount() == 0) {
      MOZ_ASSERT(mIndex == 0);
      MOZ_ASSERT(aApzc == nullptr);
      return;
    }
    MOZ_ASSERT(mIndex < mLayer->GetScrollMetadataCount());
    mLayer->SetAsyncPanZoomController(mIndex, aApzc);
  }

  const char* Name() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->Name();
    }
    return "DummyContainerLayer";
  }

  LayerManager* Manager() const
  {
    MOZ_ASSERT(IsValid());

    return mLayer->Manager();
  }

  gfx::Matrix4x4 GetTransform() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetTransform();
    }
    return gfx::Matrix4x4();
  }

  CSSTransformMatrix GetTransformTyped() const
  {
    return ViewAs<CSSTransformMatrix>(GetTransform());
  }

  bool TransformIsPerspective() const
  {
    MOZ_ASSERT(IsValid());

    // mLayer->GetTransformIsPerspective() tells us whether
    // mLayer->GetTransform() is a perspective transform. Since
    // mLayer->GetTransform() is only used at the bottom layer, we only
    // need to check GetTransformIsPerspective() at the bottom layer too.
    if (AtBottomLayer()) {
      return mLayer->GetTransformIsPerspective();
    }
    return false;
  }

  EventRegions GetEventRegions() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetEventRegions();
    }
    return EventRegions();
  }

  LayerIntRegion GetVisibleRegion() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->GetVisibleRegion();
    }

    return ViewAs<LayerPixel>(
        TransformBy(mLayer->GetTransformTyped(), mLayer->GetVisibleRegion()),
        PixelCastJustification::MovingDownToChildren);
  }

  bool HasTransformAnimation() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->HasTransformAnimation();
    }
    return false;
  }

  RefLayer* AsRefLayer() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->AsRefLayer();
    }
    return nullptr;
  }

  Maybe<uint64_t> GetReferentId() const
  {
    MOZ_ASSERT(IsValid());

    if (AtBottomLayer()) {
      return mLayer->AsRefLayer()
           ? Some(mLayer->AsRefLayer()->GetReferentId())
           : Nothing();
    }
    return Nothing();
  }

  Maybe<ParentLayerIntRect> GetClipRect() const
  {
    MOZ_ASSERT(IsValid());

    Maybe<ParentLayerIntRect> result;

    // The layer can have a clip rect and a scrolled clip, which are considered
    // to apply only to the bottommost LayerMetricsWrapper.
    // TODO: These actually apply in a different coordinate space than the
    // scroll clip of the bottommost metrics, so we shouldn't be intersecting
    // them with the scroll clip; bug 1269537 tracks fixing this.
    if (AtBottomLayer()) {
      result = mLayer->GetClipRect();
      result = IntersectMaybeRects(result, mLayer->GetScrolledClipRect());
    }

    // The scroll metadata can have a clip rect as well.
    result = IntersectMaybeRects(result, Metadata().GetClipRect());

    return result;
  }

  float GetPresShellResolution() const
  {
    MOZ_ASSERT(IsValid());

    if (AtTopLayer() && mLayer->AsContainerLayer()) {
      return mLayer->AsContainerLayer()->GetPresShellResolution();
    }

    return 1.0f;
  }

  EventRegionsOverride GetEventRegionsOverride() const
  {
    MOZ_ASSERT(IsValid());

    if (mLayer->AsContainerLayer()) {
      return mLayer->AsContainerLayer()->GetEventRegionsOverride();
    }
    return EventRegionsOverride::NoOverride;
  }

  const ScrollThumbData& GetScrollThumbData() const
  {
    MOZ_ASSERT(IsValid());

    return mLayer->GetScrollThumbData();
  }

  uint64_t GetScrollbarAnimationId() const
  {
    MOZ_ASSERT(IsValid());
    // This function is only really needed for template-compatibility with
    // WebRenderScrollDataWrapper. Although it will be called, the return
    // value is not used.
    return 0;
  }

  FrameMetrics::ViewID GetScrollbarTargetContainerId() const
  {
    MOZ_ASSERT(IsValid());

    return mLayer->GetScrollbarTargetContainerId();
  }

  bool IsScrollbarContainer() const
  {
    MOZ_ASSERT(IsValid());
    return mLayer->IsScrollbarContainer();
  }

  FrameMetrics::ViewID GetFixedPositionScrollContainerId() const
  {
    MOZ_ASSERT(IsValid());

    return mLayer->GetFixedPositionScrollContainerId();
  }

  // Expose an opaque pointer to the layer. Mostly used for printf
  // purposes. This is not intended to be a general-purpose accessor
  // for the underlying layer.
  const void* GetLayer() const
  {
    MOZ_ASSERT(IsValid());

    return (void*)mLayer;
  }

  bool operator==(const LayerMetricsWrapper& aOther) const
  {
    return mLayer == aOther.mLayer
        && mIndex == aOther.mIndex;
  }

  bool operator!=(const LayerMetricsWrapper& aOther) const
  {
    return !(*this == aOther);
  }

private:
  bool AtBottomLayer() const
  {
    return mIndex == 0;
  }

  bool AtTopLayer() const
  {
    return mLayer->GetScrollMetadataCount() == 0 || mIndex == mLayer->GetScrollMetadataCount() - 1;
  }

private:
  Layer* mLayer;
  uint32_t mIndex;
};

} // namespace layers
} // namespace mozilla

#endif /* GFX_LAYERMETRICSWRAPPER_H */
