/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSTYPES_H
#define GFX_LAYERSTYPES_H

#include <stdint.h>                     // for uint32_t
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRegion.h"

#include "mozilla/TypedEnumBits.h"

#ifdef MOZ_WIDGET_GONK
#include <ui/GraphicBuffer.h>
#endif
#include <stdio.h>            // FILE
#include "prlog.h"            // for PR_LOG
#ifndef MOZ_LAYERS_HAVE_LOG
#  define MOZ_LAYERS_HAVE_LOG
#endif
#define MOZ_LAYERS_LOG(_args)                             \
  PR_LOG(LayerManager::GetLog(), PR_LOG_DEBUG, _args)
#define MOZ_LAYERS_LOG_IF_SHADOWABLE(layer, _args)         \
  do { if (layer->AsShadowableLayer()) { PR_LOG(LayerManager::GetLog(), PR_LOG_DEBUG, _args); } } while (0)

#define INVALID_OVERLAY -1

namespace android {
class GraphicBuffer;
}

namespace mozilla {
namespace layers {

class TextureHost;

#undef NONE
#undef OPAQUE

enum class LayersBackend : int8_t {
  LAYERS_NONE = 0,
  LAYERS_BASIC,
  LAYERS_OPENGL,
  LAYERS_D3D9,
  LAYERS_D3D11,
  LAYERS_CLIENT,
  LAYERS_LAST
};

enum class BufferMode : int8_t {
  BUFFER_NONE,
  BUFFERED
};

enum class DrawRegionClip : int8_t {
  DRAW,
  NONE
};

enum class SurfaceMode : int8_t {
  SURFACE_NONE = 0,
  SURFACE_OPAQUE,
  SURFACE_SINGLE_CHANNEL_ALPHA,
  SURFACE_COMPONENT_ALPHA
};

// LayerRenderState for Composer2D
// We currently only support Composer2D using gralloc. If we want to be backed
// by other surfaces we will need a more generic LayerRenderState.
enum class LayerRenderStateFlags : int8_t {
  LAYER_RENDER_STATE_DEFAULT = 0,
  ORIGIN_BOTTOM_LEFT = 1 << 0,
  BUFFER_ROTATION = 1 << 1,
  // Notify Composer2D to swap the RB pixels of gralloc buffer
  FORMAT_RB_SWAP = 1 << 2,
  // We record opaqueness here alongside the actual surface we're going to
  // render. This avoids confusion when a layer might return different kinds
  // of surfaces over time (e.g. video frames).
  OPAQUE = 1 << 3
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(LayerRenderStateFlags)

// The 'ifdef MOZ_WIDGET_GONK' sadness here is because we don't want to include
// android::sp unless we have to.
struct LayerRenderState {
  LayerRenderState()
#ifdef MOZ_WIDGET_GONK
    : mFlags(LayerRenderStateFlags::LAYER_RENDER_STATE_DEFAULT)
    , mHasOwnOffset(false)
    , mSurface(nullptr)
    , mOverlayId(INVALID_OVERLAY)
    , mTexture(nullptr)
#endif
  {}

#ifdef MOZ_WIDGET_GONK
  LayerRenderState(android::GraphicBuffer* aSurface,
                   const nsIntSize& aSize,
                   LayerRenderStateFlags aFlags,
                   TextureHost* aTexture)
    : mFlags(aFlags)
    , mHasOwnOffset(false)
    , mSurface(aSurface)
    , mOverlayId(INVALID_OVERLAY)
    , mSize(aSize)
    , mTexture(aTexture)
  {}

  bool OriginBottomLeft() const
  { return bool(mFlags & LayerRenderStateFlags::ORIGIN_BOTTOM_LEFT); }

  bool BufferRotated() const
  { return bool(mFlags & LayerRenderStateFlags::BUFFER_ROTATION); }

  bool FormatRBSwapped() const
  { return bool(mFlags & LayerRenderStateFlags::FORMAT_RB_SWAP); }

  void SetOverlayId(const int32_t& aId)
  { mOverlayId = aId; }
#endif

  void SetOffset(const nsIntPoint& aOffset)
  {
    mOffset = aOffset;
    mHasOwnOffset = true;
  }

  // see LayerRenderStateFlags
  LayerRenderStateFlags mFlags;
  // true if mOffset is applicable
  bool mHasOwnOffset;
  // the location of the layer's origin on mSurface
  nsIntPoint mOffset;
#ifdef MOZ_WIDGET_GONK
  // surface to render
  android::sp<android::GraphicBuffer> mSurface;
  int32_t mOverlayId;
  // size of mSurface
  nsIntSize mSize;
  TextureHost* mTexture;
#endif
};

enum class ScaleMode : int8_t {
  SCALE_NONE,
  STRETCH,
  SENTINEL
// Unimplemented - PRESERVE_ASPECT_RATIO_CONTAIN
};

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

  EventRegions()
  {
  }

  explicit EventRegions(nsIntRegion aHitRegion)
    : mHitRegion(aHitRegion)
  {
  }

  bool operator==(const EventRegions& aRegions) const
  {
    return mHitRegion == aRegions.mHitRegion &&
           mDispatchToContentHitRegion == aRegions.mDispatchToContentHitRegion;
  }
  bool operator!=(const EventRegions& aRegions) const
  {
    return !(*this == aRegions);
  }

  void OrWith(const EventRegions& aOther)
  {
    mHitRegion.OrWith(aOther.mHitRegion);
    mDispatchToContentHitRegion.OrWith(aOther.mDispatchToContentHitRegion);
  }

  void AndWith(const nsIntRegion& aRegion)
  {
    mHitRegion.AndWith(aRegion);
    mDispatchToContentHitRegion.AndWith(aRegion);
  }

  void Sub(const EventRegions& aMinuend, const nsIntRegion& aSubtrahend)
  {
    mHitRegion.Sub(aMinuend.mHitRegion, aSubtrahend);
    mDispatchToContentHitRegion.Sub(aMinuend.mDispatchToContentHitRegion, aSubtrahend);
  }

  void ApplyTranslationAndScale(float aXTrans, float aYTrans, float aXScale, float aYScale)
  {
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

  void Transform(const gfx3DMatrix& aTransform)
  {
    mHitRegion.Transform(aTransform);
    mDispatchToContentHitRegion.Transform(aTransform);
  }

  bool IsEmpty() const
  {
    return mHitRegion.IsEmpty()
        && mDispatchToContentHitRegion.IsEmpty();
  }

  nsCString ToString() const
  {
    nsCString result = mHitRegion.ToString();
    result.AppendLiteral(";dispatchToContent=");
    result.Append(mDispatchToContentHitRegion.ToString());
    return result;
  }
};

// Bit flags that go on a ContainerLayer (or RefLayer) and override the
// event regions in the entire subtree below. This is needed for propagating
// various flags across processes since the child-process layout code doesn't
// know about parent-process listeners or CSS rules.
enum EventRegionsOverride {
  // The default, no flags set
  NoOverride             = 0,
  // Treat all hit regions in the subtree as dispatch-to-content
  ForceDispatchToContent = (1 << 0),
  // Treat all hit regions in the subtree as empty
  ForceEmptyHitRegion    = (1 << 1),
  // OR union of all valid bit flags, for use in BitFlagsEnumSerializer
  ALL_BITS               = (1 << 2) - 1
};

MOZ_ALWAYS_INLINE EventRegionsOverride
operator|(EventRegionsOverride a, EventRegionsOverride b)
{
  return (EventRegionsOverride)((int)a | (int)b);
}

MOZ_ALWAYS_INLINE EventRegionsOverride&
operator|=(EventRegionsOverride& a, EventRegionsOverride b)
{
  a = a | b;
  return a;
}

} // namespace
} // namespace

#endif /* GFX_LAYERSTYPES_H */
