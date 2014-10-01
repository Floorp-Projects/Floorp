/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSTYPES_H
#define GFX_LAYERSTYPES_H

#include <stdint.h>                     // for uint32_t
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRegion.h"

#include "mozilla/TypedEnum.h"
#include "mozilla/TypedEnumBits.h"

#ifdef MOZ_WIDGET_GONK
#include <ui/GraphicBuffer.h>
#endif
#if defined(DEBUG) || defined(PR_LOGGING)
#  include <stdio.h>            // FILE
#  include "prlog.h"            // for PR_LOG
#  ifndef MOZ_LAYERS_HAVE_LOG
#    define MOZ_LAYERS_HAVE_LOG
#  endif
#  define MOZ_LAYERS_LOG(_args)                             \
  PR_LOG(LayerManager::GetLog(), PR_LOG_DEBUG, _args)
#  define MOZ_LAYERS_LOG_IF_SHADOWABLE(layer, _args)         \
  do { if (layer->AsShadowableLayer()) { PR_LOG(LayerManager::GetLog(), PR_LOG_DEBUG, _args); } } while (0)
#else
struct PRLogModuleInfo;
#  define MOZ_LAYERS_LOG(_args)
#  define MOZ_LAYERS_LOG_IF_SHADOWABLE(layer, _args)
#endif  // if defined(DEBUG) || defined(PR_LOGGING)

#define INVALID_OVERLAY -1

namespace android {
class GraphicBuffer;
}

namespace mozilla {
namespace layers {

class TextureHost;

#undef NONE
#undef OPAQUE

MOZ_BEGIN_ENUM_CLASS(LayersBackend, int8_t)
  LAYERS_NONE = 0,
  LAYERS_BASIC,
  LAYERS_OPENGL,
  LAYERS_D3D9,
  LAYERS_D3D10,
  LAYERS_D3D11,
  LAYERS_CLIENT,
  LAYERS_LAST
MOZ_END_ENUM_CLASS(LayersBackend)

MOZ_BEGIN_ENUM_CLASS(BufferMode, int8_t)
  BUFFER_NONE,
  BUFFERED
MOZ_END_ENUM_CLASS(BufferMode)

MOZ_BEGIN_ENUM_CLASS(DrawRegionClip, int8_t)
  DRAW,
  NONE
MOZ_END_ENUM_CLASS(DrawRegionClip)

MOZ_BEGIN_ENUM_CLASS(SurfaceMode, int8_t)
  SURFACE_NONE = 0,
  SURFACE_OPAQUE,
  SURFACE_SINGLE_CHANNEL_ALPHA,
  SURFACE_COMPONENT_ALPHA
MOZ_END_ENUM_CLASS(SurfaceMode)

// LayerRenderState for Composer2D
// We currently only support Composer2D using gralloc. If we want to be backed
// by other surfaces we will need a more generic LayerRenderState.
MOZ_BEGIN_ENUM_CLASS(LayerRenderStateFlags, int8_t)
  LAYER_RENDER_STATE_DEFAULT = 0,
  Y_FLIPPED = 1 << 0,
  BUFFER_ROTATION = 1 << 1,
  // Notify Composer2D to swap the RB pixels of gralloc buffer
  FORMAT_RB_SWAP = 1 << 2
MOZ_END_ENUM_CLASS(LayerRenderStateFlags)
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

  bool YFlipped() const
  { return bool(mFlags & LayerRenderStateFlags::Y_FLIPPED); }

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

MOZ_BEGIN_ENUM_CLASS(ScaleMode, int8_t)
  SCALE_NONE,
  STRETCH,
  SENTINEL
// Unimplemented - PRESERVE_ASPECT_RATIO_CONTAIN
MOZ_END_ENUM_CLASS(ScaleMode)

struct EventRegions {
  nsIntRegion mHitRegion;
  nsIntRegion mDispatchToContentHitRegion;

  bool operator==(const EventRegions& aRegions) const
  {
    return mHitRegion == aRegions.mHitRegion &&
           mDispatchToContentHitRegion == aRegions.mDispatchToContentHitRegion;
  }
  bool operator!=(const EventRegions& aRegions) const
  {
    return !(*this == aRegions);
  }

  nsCString ToString() const
  {
    nsCString result = mHitRegion.ToString();
    result.AppendLiteral(";dispatchToContent=");
    result.Append(mDispatchToContentHitRegion.ToString());
    return result;
  }
};

} // namespace
} // namespace

#endif /* GFX_LAYERSTYPES_H */
