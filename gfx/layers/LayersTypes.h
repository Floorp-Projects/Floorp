/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSTYPES_H
#define GFX_LAYERSTYPES_H

#include "nsPoint.h"

#if defined(DEBUG) || defined(PR_LOGGING)
#  include <stdio.h>            // FILE
#  include "prlog.h"
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

namespace mozilla {
namespace layers {

class SurfaceDescriptor;

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

// The kinds of mask layer a shader can support
// We rely on the items in this enum being sequential
enum MaskType {
  MaskNone = 0,   // no mask layer
  Mask2d,         // mask layer for layers with 2D transforms
  Mask3d,         // mask layer for layers with 3D transforms
  NumMaskTypes
};

// LayerRenderState for Composer2D
enum LayerRenderStateFlags {
  LAYER_RENDER_STATE_Y_FLIPPED = 1 << 0,
  LAYER_RENDER_STATE_BUFFER_ROTATION = 1 << 1
};

struct LayerRenderState {
  LayerRenderState() : mSurface(nullptr), mFlags(0), mHasOwnOffset(false)
  {}

  LayerRenderState(SurfaceDescriptor* aSurface, uint32_t aFlags = 0)
    : mSurface(aSurface)
    , mFlags(aFlags)
    , mHasOwnOffset(false)
  {}

  LayerRenderState(SurfaceDescriptor* aSurface, nsIntPoint aOffset, uint32_t aFlags = 0)
    : mSurface(aSurface)
    , mFlags(aFlags)
    , mOffset(aOffset)
    , mHasOwnOffset(true)
  {}

  bool YFlipped() const
  { return mFlags & LAYER_RENDER_STATE_Y_FLIPPED; }

  bool BufferRotated() const
  { return mFlags & LAYER_RENDER_STATE_BUFFER_ROTATION; }

  SurfaceDescriptor* mSurface;
  uint32_t mFlags;
  nsIntPoint mOffset;
  bool mHasOwnOffset;
};

} // namespace
} // namespace

#endif /* GFX_LAYERSTYPES_H */
