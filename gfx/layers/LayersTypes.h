/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSTYPES_H
#define GFX_LAYERSTYPES_H

#include <stdint.h>  // for uint32_t

#include "Units.h"
#include "mozilla/DefineEnum.h"  // for MOZ_DEFINE_ENUM
#include "mozilla/gfx/Point.h"   // for IntPoint
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"  // for TimeStamp
#include "mozilla/TypedEnumBits.h"
#include "nsRegion.h"
#include "nsStyleConsts.h"

#include <stdio.h>            // FILE
#include "mozilla/Logging.h"  // for PR_LOG

#ifndef MOZ_LAYERS_HAVE_LOG
#  define MOZ_LAYERS_HAVE_LOG
#endif
#define MOZ_LAYERS_LOG(_args) \
  MOZ_LOG(LayerManager::GetLog(), LogLevel::Debug, _args)
#define MOZ_LAYERS_LOG_IF_SHADOWABLE(layer, _args)             \
  do {                                                         \
    if (layer->AsShadowableLayer()) {                          \
      MOZ_LOG(LayerManager::GetLog(), LogLevel::Debug, _args); \
    }                                                          \
  } while (0)

#define INVALID_OVERLAY -1

//#define ENABLE_FRAME_LATENCY_LOG

namespace IPC {
template <typename T>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {
namespace layers {

class TextureHost;

#undef NONE
#undef OPAQUE

struct LayersId {
  uint64_t mId;

  bool IsValid() const { return mId != 0; }

  // Allow explicit cast to a uint64_t for now
  explicit operator uint64_t() const { return mId; }

  // Implement some operators so this class can be used as a key in
  // stdlib classes.
  bool operator<(const LayersId& aOther) const { return mId < aOther.mId; }

  bool operator==(const LayersId& aOther) const { return mId == aOther.mId; }

  bool operator!=(const LayersId& aOther) const { return !(*this == aOther); }

  // Helper struct that allow this class to be used as a key in
  // std::unordered_map like so:
  //   std::unordered_map<LayersId, ValueType, LayersId::HashFn> myMap;
  struct HashFn {
    std::size_t operator()(const LayersId& aKey) const {
      return std::hash<uint64_t>{}(aKey.mId);
    }
  };
};

template <typename T>
struct BaseTransactionId {
  uint64_t mId = 0;

  bool IsValid() const { return mId != 0; }

  MOZ_MUST_USE BaseTransactionId<T> Next() const {
    return BaseTransactionId<T>{mId + 1};
  }

  MOZ_MUST_USE BaseTransactionId<T> Prev() const {
    return BaseTransactionId<T>{mId - 1};
  }

  int64_t operator-(const BaseTransactionId<T>& aOther) const {
    return mId - aOther.mId;
  }

  // Allow explicit cast to a uint64_t for now
  explicit operator uint64_t() const { return mId; }

  bool operator<(const BaseTransactionId<T>& aOther) const {
    return mId < aOther.mId;
  }

  bool operator<=(const BaseTransactionId<T>& aOther) const {
    return mId <= aOther.mId;
  }

  bool operator>(const BaseTransactionId<T>& aOther) const {
    return mId > aOther.mId;
  }

  bool operator>=(const BaseTransactionId<T>& aOther) const {
    return mId >= aOther.mId;
  }

  bool operator==(const BaseTransactionId<T>& aOther) const {
    return mId == aOther.mId;
  }
};

class TransactionIdType {};
typedef BaseTransactionId<TransactionIdType> TransactionId;

struct LayersObserverEpoch {
  uint64_t mId;

  MOZ_MUST_USE LayersObserverEpoch Next() const {
    return LayersObserverEpoch{mId + 1};
  }

  bool operator<=(const LayersObserverEpoch& aOther) const {
    return mId <= aOther.mId;
  }

  bool operator>=(const LayersObserverEpoch& aOther) const {
    return mId >= aOther.mId;
  }

  bool operator==(const LayersObserverEpoch& aOther) const {
    return mId == aOther.mId;
  }

  bool operator!=(const LayersObserverEpoch& aOther) const {
    return mId != aOther.mId;
  }
};

enum class LayersBackend : int8_t {
  LAYERS_NONE = 0,
  LAYERS_BASIC,
  LAYERS_OPENGL,
  LAYERS_D3D11,
  LAYERS_CLIENT,
  LAYERS_WR,
  LAYERS_LAST
};

enum class BufferMode : int8_t { BUFFER_NONE, BUFFERED };

enum class DrawRegionClip : int8_t { DRAW, NONE };

enum class SurfaceMode : int8_t {
  SURFACE_NONE = 0,
  SURFACE_OPAQUE,
  SURFACE_SINGLE_CHANNEL_ALPHA,
  SURFACE_COMPONENT_ALPHA
};

// clang-format off
MOZ_DEFINE_ENUM_CLASS_WITH_BASE(
  ScaleMode, int8_t, (
    SCALE_NONE,
    STRETCH
// Unimplemented - PRESERVE_ASPECT_RATIO_CONTAIN
));
// clang-format on

struct EventRegions {
  // The hit region for a layer contains all areas on the layer that are
  // sensitive to events. This region is an over-approximation and may
  // contain regions that are not actually sensitive, but any such regions
  // will be included in the mDispatchToContentHitRegion.
  nsIntRegion mHitRegion;
  // The mDispatchToContentHitRegion for a layer contains all areas for
  // which the main-thread must be consulted before responding to events.
  // This region will be a subregion of mHitRegion.
  nsIntRegion mDispatchToContentHitRegion;

  // The following regions represent the touch-action areas of this layer.
  // All of these regions are approximations to the true region, but any
  // variance between the approximation and the true region is guaranteed
  // to be included in the mDispatchToContentHitRegion.
  nsIntRegion mNoActionRegion;
  nsIntRegion mHorizontalPanRegion;
  nsIntRegion mVerticalPanRegion;

  // Set to true if events targeting the dispatch-to-content region
  // require target confirmation.
  // See CompositorHitTestFlags::eRequiresTargetConfirmation.
  // We don't bother tracking a separate region for this (which would
  // be a sub-region of the dispatch-to-content region), because the added
  // overhead of region computations is not worth it, and because
  // EventRegions are going to be deprecated anyways.
  bool mDTCRequiresTargetConfirmation;

  EventRegions() : mDTCRequiresTargetConfirmation(false) {}

  explicit EventRegions(nsIntRegion aHitRegion)
      : mHitRegion(aHitRegion), mDTCRequiresTargetConfirmation(false) {}

  // This constructor takes the maybe-hit region and uses it to update the
  // hit region and dispatch-to-content region. It is useful from converting
  // from the display item representation to the layer representation.
  EventRegions(const nsIntRegion& aHitRegion,
               const nsIntRegion& aMaybeHitRegion,
               const nsIntRegion& aDispatchToContentRegion,
               const nsIntRegion& aNoActionRegion,
               const nsIntRegion& aHorizontalPanRegion,
               const nsIntRegion& aVerticalPanRegion,
               bool aDTCRequiresTargetConfirmation);

  bool operator==(const EventRegions& aRegions) const {
    return mHitRegion == aRegions.mHitRegion &&
           mDispatchToContentHitRegion ==
               aRegions.mDispatchToContentHitRegion &&
           mNoActionRegion == aRegions.mNoActionRegion &&
           mHorizontalPanRegion == aRegions.mHorizontalPanRegion &&
           mVerticalPanRegion == aRegions.mVerticalPanRegion &&
           mDTCRequiresTargetConfirmation ==
               aRegions.mDTCRequiresTargetConfirmation;
  }
  bool operator!=(const EventRegions& aRegions) const {
    return !(*this == aRegions);
  }

  void ApplyTranslationAndScale(float aXTrans, float aYTrans, float aXScale,
                                float aYScale) {
    mHitRegion.ScaleRoundOut(aXScale, aYScale);
    mDispatchToContentHitRegion.ScaleRoundOut(aXScale, aYScale);
    mNoActionRegion.ScaleRoundOut(aXScale, aYScale);
    mHorizontalPanRegion.ScaleRoundOut(aXScale, aYScale);
    mVerticalPanRegion.ScaleRoundOut(aXScale, aYScale);

    mHitRegion.MoveBy(aXTrans, aYTrans);
    mDispatchToContentHitRegion.MoveBy(aXTrans, aYTrans);
    mNoActionRegion.MoveBy(aXTrans, aYTrans);
    mHorizontalPanRegion.MoveBy(aXTrans, aYTrans);
    mVerticalPanRegion.MoveBy(aXTrans, aYTrans);
  }

  void Transform(const gfx::Matrix4x4& aTransform) {
    mHitRegion.Transform(aTransform);
    mDispatchToContentHitRegion.Transform(aTransform);
    mNoActionRegion.Transform(aTransform);
    mHorizontalPanRegion.Transform(aTransform);
    mVerticalPanRegion.Transform(aTransform);
  }

  void OrWith(const EventRegions& aOther) {
    mHitRegion.OrWith(aOther.mHitRegion);
    mDispatchToContentHitRegion.OrWith(aOther.mDispatchToContentHitRegion);
    // See the comment in nsDisplayList::AddFrame, where the touch action
    // regions are handled. The same thing applies here.
    bool alreadyHadRegions = !mNoActionRegion.IsEmpty() ||
                             !mHorizontalPanRegion.IsEmpty() ||
                             !mVerticalPanRegion.IsEmpty();
    mNoActionRegion.OrWith(aOther.mNoActionRegion);
    mHorizontalPanRegion.OrWith(aOther.mHorizontalPanRegion);
    mVerticalPanRegion.OrWith(aOther.mVerticalPanRegion);
    if (alreadyHadRegions) {
      nsIntRegion combinedActionRegions;
      combinedActionRegions.Or(mHorizontalPanRegion, mVerticalPanRegion);
      combinedActionRegions.OrWith(mNoActionRegion);
      mDispatchToContentHitRegion.OrWith(combinedActionRegions);
    }
    mDTCRequiresTargetConfirmation |= aOther.mDTCRequiresTargetConfirmation;
  }

  bool IsEmpty() const {
    return mHitRegion.IsEmpty() && mDispatchToContentHitRegion.IsEmpty() &&
           mNoActionRegion.IsEmpty() && mHorizontalPanRegion.IsEmpty() &&
           mVerticalPanRegion.IsEmpty();
  }

  void SetEmpty() {
    mHitRegion.SetEmpty();
    mDispatchToContentHitRegion.SetEmpty();
    mNoActionRegion.SetEmpty();
    mHorizontalPanRegion.SetEmpty();
    mVerticalPanRegion.SetEmpty();
  }

  nsCString ToString() const {
    nsCString result = mHitRegion.ToString();
    result.AppendLiteral(";dispatchToContent=");
    result.Append(mDispatchToContentHitRegion.ToString());
    return result;
  }
};

// Bit flags that go on a RefLayer and override the
// event regions in the entire subtree below. This is needed for propagating
// various flags across processes since the child-process layout code doesn't
// know about parent-process listeners or CSS rules.
enum EventRegionsOverride {
  // The default, no flags set
  NoOverride = 0,
  // Treat all hit regions in the subtree as dispatch-to-content
  ForceDispatchToContent = (1 << 0),
  // Treat all hit regions in the subtree as empty
  ForceEmptyHitRegion = (1 << 1),
  // OR union of all valid bit flags, for use in BitFlagsEnumSerializer
  ALL_BITS = (1 << 2) - 1
};

MOZ_ALWAYS_INLINE EventRegionsOverride operator|(EventRegionsOverride a,
                                                 EventRegionsOverride b) {
  return (EventRegionsOverride)((int)a | (int)b);
}

MOZ_ALWAYS_INLINE EventRegionsOverride& operator|=(EventRegionsOverride& a,
                                                   EventRegionsOverride b) {
  a = a | b;
  return a;
}

// Flags used as an argument to functions that dump textures.
enum TextureDumpMode {
  Compress,      // dump texture with LZ4 compression
  DoNotCompress  // dump texture uncompressed
};

typedef uint32_t TouchBehaviorFlags;

// Some specialized typedefs of Matrix4x4Typed.
typedef gfx::Matrix4x4Typed<LayerPixel, CSSTransformedLayerPixel>
    CSSTransformMatrix;
// Several different async transforms can contribute to a layer's transform
// (specifically, an async animation can contribute a transform, and each APZC
// that scrolls a layer can contribute async scroll/zoom and overscroll
// transforms).
// To try to model this with typed units, we represent individual async
// transforms as ParentLayer -> ParentLayer transforms (aliased as
// AsyncTransformComponentMatrix), and we represent the product of all of them
// as a CSSTransformLayer -> ParentLayer transform (aliased as
// AsyncTransformMatrix). To create an AsyncTransformMatrix from component
// matrices, a ViewAs operation is needed. A MultipleAsyncTransforms
// PixelCastJustification is provided for this purpose.
typedef gfx::Matrix4x4Typed<ParentLayerPixel, ParentLayerPixel>
    AsyncTransformComponentMatrix;
typedef gfx::Matrix4x4Typed<CSSTransformedLayerPixel, ParentLayerPixel>
    AsyncTransformMatrix;

typedef Array<gfx::Color, 4> BorderColors;
typedef Array<LayerSize, 4> BorderCorners;
typedef Array<LayerCoord, 4> BorderWidths;
typedef Array<StyleBorderStyle, 4> BorderStyles;

typedef Maybe<LayerRect> MaybeLayerRect;

// This is used to communicate Layers across IPC channels. The Handle is valid
// for layers in the same PLayerTransaction. Handles are created by
// ClientLayerManager, and are cached in LayerTransactionParent on first use.
class LayerHandle final {
  friend struct IPC::ParamTraits<mozilla::layers::LayerHandle>;

 public:
  LayerHandle() : mHandle(0) {}
  LayerHandle(const LayerHandle& aOther) : mHandle(aOther.mHandle) {}
  explicit LayerHandle(uint64_t aHandle) : mHandle(aHandle) {}
  bool IsValid() const { return mHandle != 0; }
  explicit operator bool() const { return IsValid(); }
  bool operator==(const LayerHandle& aOther) const {
    return mHandle == aOther.mHandle;
  }
  uint64_t Value() const { return mHandle; }

 private:
  uint64_t mHandle;
};

// This is used to communicate Compositables across IPC channels. The Handle is
// valid for layers in the same PLayerTransaction or PImageBridge. Handles are
// created by ClientLayerManager or ImageBridgeChild, and are cached in the
// parent side on first use.
class CompositableHandle final {
  friend struct IPC::ParamTraits<mozilla::layers::CompositableHandle>;

 public:
  CompositableHandle() : mHandle(0) {}
  CompositableHandle(const CompositableHandle& aOther)
      : mHandle(aOther.mHandle) {}
  explicit CompositableHandle(uint64_t aHandle) : mHandle(aHandle) {}
  bool IsValid() const { return mHandle != 0; }
  explicit operator bool() const { return IsValid(); }
  bool operator==(const CompositableHandle& aOther) const {
    return mHandle == aOther.mHandle;
  }
  uint64_t Value() const { return mHandle; }

 private:
  uint64_t mHandle;
};

// clang-format off
MOZ_DEFINE_ENUM_CLASS_WITH_BASE(ScrollDirection, uint32_t, (
  eVertical,
  eHorizontal
));

MOZ_DEFINE_ENUM_CLASS_WITH_BASE(CompositionPayloadType, uint8_t, (
  eKeyPress,
  eAPZScroll,
  eAPZPinchZoom,
  eContentPaint
));
// clang-format on

struct CompositionPayload {
  bool operator==(const CompositionPayload& aOther) const {
    return mType == aOther.mType && mTimeStamp == aOther.mTimeStamp;
  }
  /* The type of payload that is in this composition */
  CompositionPayloadType mType;
  /* When this payload was generated */
  TimeStamp mTimeStamp;
};

}  // namespace layers
}  // namespace mozilla

#endif /* GFX_LAYERSTYPES_H */
