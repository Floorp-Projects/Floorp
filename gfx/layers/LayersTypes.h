/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSTYPES_H
#define GFX_LAYERSTYPES_H

#include <stdint.h>                     // for uint32_t
#include "nsPoint.h"                    // for nsIntPoint

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

namespace android {
class GraphicBuffer;
}

namespace mozilla {
namespace layers {


typedef uint32_t TextureFlags;

enum LayersBackend {
  LAYERS_NONE = 0,
  LAYERS_BASIC,
  LAYERS_OPENGL,
  LAYERS_D3D9,
  LAYERS_D3D10,
  LAYERS_D3D11,
  LAYERS_CLIENT,
  LAYERS_LAST
};

enum BufferMode {
  BUFFER_NONE,
  BUFFER_BUFFERED
};

// LayerRenderState for Composer2D
// We currently only support Composer2D using gralloc. If we want to be backed
// by other surfaces we will need a more generic LayerRenderState.
enum LayerRenderStateFlags {
  LAYER_RENDER_STATE_Y_FLIPPED = 1 << 0,
  LAYER_RENDER_STATE_BUFFER_ROTATION = 1 << 1,
  // Notify Composer2D to swap the RB pixels of gralloc buffer
  LAYER_RENDER_STATE_FORMAT_RB_SWAP = 1 << 2
};

// The 'ifdef MOZ_WIDGET_GONK' sadness here is because we don't want to include
// android::sp unless we have to.
struct LayerRenderState {
  LayerRenderState()
#ifdef MOZ_WIDGET_GONK
    : mSurface(nullptr), mFlags(0), mHasOwnOffset(false)
#endif
  {}

#ifdef MOZ_WIDGET_GONK
  LayerRenderState(android::GraphicBuffer* aSurface,
                   const nsIntSize& aSize,
                   uint32_t aFlags)
    : mSurface(aSurface)
    , mSize(aSize)
    , mFlags(aFlags)
    , mHasOwnOffset(false)
  {}

  bool YFlipped() const
  { return mFlags & LAYER_RENDER_STATE_Y_FLIPPED; }

  bool BufferRotated() const
  { return mFlags & LAYER_RENDER_STATE_BUFFER_ROTATION; }

  bool FormatRBSwapped() const
  { return mFlags & LAYER_RENDER_STATE_FORMAT_RB_SWAP; }
#endif

  void SetOffset(const nsIntPoint& aOffset)
  {
    mOffset = aOffset;
    mHasOwnOffset = true;
  }

#ifdef MOZ_WIDGET_GONK
  // surface to render
  android::sp<android::GraphicBuffer> mSurface;
  // size of mSurface 
  nsIntSize mSize;
#endif
  // see LayerRenderStateFlags
  uint32_t mFlags;
  // the location of the layer's origin on mSurface
  nsIntPoint mOffset;
  // true if mOffset is applicable
  bool mHasOwnOffset;
};

} // namespace
} // namespace

#endif /* GFX_LAYERSTYPES_H */
