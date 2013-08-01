/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_COMPOSITORTYPES_H
#define MOZILLA_LAYERS_COMPOSITORTYPES_H

#include "LayersTypes.h"
#include "nsXULAppAPI.h"


namespace mozilla {
namespace layers {

typedef int32_t SurfaceDescriptorType;
const SurfaceDescriptorType SURFACEDESCRIPTOR_UNKNOWN = 0;

/**
 * Flags used by texture clients and texture hosts. These are passed from client
 * side to host side when textures and compositables are created. Usually set
 * by the compositableCient, they may be modified by either the compositable or
 * texture clients.
 *
 * XXX - switch to all caps constant names which seems to be the standard in gecko
 */
typedef uint32_t TextureFlags;
// Use nearest-neighbour texture filtering (as opposed to linear filtering).
const TextureFlags UseNearestFilter           = 1 << 0;
// The texture should be flipped around the y-axis when composited.
const TextureFlags NeedsYFlip                 = 1 << 1;
// Force the texture to be represented using a single tile (note that this means
// tiled textures, not tiled layers).
const TextureFlags ForceSingleTile            = 1 << 2;
// Allow using 'repeat' mode for wrapping.
const TextureFlags AllowRepeat                = 1 << 3;
// The texture represents a tile which is newly created.
const TextureFlags NewTile                    = 1 << 4;
// The texture is part of a component-alpha pair
const TextureFlags ComponentAlpha             = 1 << 5;
// The buffer will be treated as if the RB bytes are swapped.
// This is useful for rendering using Cairo/Thebes, because there is no
// BGRX Android pixel format, and so we have to do byte swapping.
//
// For example, if the GraphicBuffer has an Android pixel format of
// PIXEL_FORMAT_RGBA_8888 and isRBSwapped is true, when it is sampled
// (for example, with GL), a BGRA shader should be used.
const TextureFlags TEXTURE_RB_SWAPPED         = 1 << 6;

// A texture host that supports tiling
const TextureFlags TEXTURE_FRONT              = 1 << 12;
// A texture host on white for component alpha
const TextureFlags TEXTURE_ON_WHITE           = 1 << 13;
 // A texture host on black for component alpha
const TextureFlags TEXTURE_ON_BLACK           = 1 << 14;
// A texture host that supports tiling
const TextureFlags TEXTURE_TILE               = 1 << 15;
// Texture contents should be initialized
// from the previous texture.
const TextureFlags TEXTURE_COPY_PREVIOUS      = 1 << 24;
// Who is responsible for deallocating the shared data.
// if none of the following two flags is set, the shared data will not be
// deallocated by the layers system. It is not necessarily a leak, it could
// be a choice from another part of gecko that wants to keep the data alive
// for some reason. The default behaviour is to deallocate on the host side.
const TextureFlags TEXTURE_DEALLOCATE_CLIENT  = 1 << 25;
const TextureFlags TEXTURE_DEALLOCATE_HOST    = 1 << 26;
// After being shared ith the compositor side, an immutable texture is never
// modified, it can only be read. It is safe to not Lock/Unlock immutable
// textures.
const TextureFlags TEXTURE_IMMUTABLE          = 1 << 27;

// the default flags
const TextureFlags TEXTURE_FLAGS_DEFAULT = TEXTURE_DEALLOCATE_HOST
                                         | TEXTURE_FRONT;

/**
 * See gfx/layers/Effects.h
 */
enum EffectTypes
{
  EFFECT_MASK,
  EFFECT_MAX_SECONDARY, // sentinel for the count of secondary effect types
  EFFECT_BGRX,
  EFFECT_RGBX,
  EFFECT_BGRA,
  EFFECT_RGBA,
  EFFECT_YCBCR,
  EFFECT_COMPONENT_ALPHA,
  EFFECT_SOLID_COLOR,
  EFFECT_RENDER_TARGET,
  EFFECT_MAX  //sentinel for the count of all effect types
};

/**
 * The kind of memory held by the texture client/host pair. This will
 * determine how the texture client is drawn into and how the memory
 * is shared between client and host.
 */
enum DeprecatedTextureClientType
{
  TEXTURE_CONTENT,            // dynamically drawn content
  TEXTURE_SHMEM,              // shared memory
  TEXTURE_YCBCR,              // Deprecated YCbCr texture
  TEXTURE_SHARED_GL,          // GLContext::SharedTextureHandle
  TEXTURE_SHARED_GL_EXTERNAL, // GLContext::SharedTextureHandle, the ownership of
                              // the SurfaceDescriptor passed to the texture
                              // remains with whoever passed it.
  TEXTURE_STREAM_GL,          // WebGL streaming buffer
  TEXTURE_FALLBACK            // A fallback path appropriate for the platform
};

/**
 * How the Compositable should manage textures.
 */
enum CompositableType
{
  BUFFER_UNKNOWN,
  // the deprecated compositable types
  BUFFER_IMAGE_SINGLE,    // image/canvas with a single texture, single buffered
  BUFFER_IMAGE_BUFFERED,  // canvas, double buffered
  BUFFER_BRIDGE,          // image bridge protocol
  BUFFER_CONTENT,         // thebes layer interface, single buffering
  BUFFER_CONTENT_DIRECT,  // thebes layer interface, double buffering
  BUFFER_CONTENT_INC,     // thebes layer interface, only sends incremental
                          // updates to a texture on the compositor side.
  BUFFER_TILED,           // tiled thebes layer
  // the new compositable types
  COMPOSITABLE_IMAGE,     // image with single buffering
  BUFFER_COUNT
};

/**
 * How the texture host is used for composition,
 */
enum DeprecatedTextureHostFlags
{
  TEXTURE_HOST_DEFAULT = 0,       // The default texture host for the given
                                  // SurfaceDescriptor
  TEXTURE_HOST_TILED = 1 << 0,    // A texture host that supports tiling
  TEXTURE_HOST_COPY_PREVIOUS = 1 << 1 // Texture contents should be initialized
                                      // from the previous texture.
};

/**
 * Sent from the compositor to the content-side LayerManager, includes properties
 * of the compositor and should (in the future) include information about what
 * kinds of buffer and texture clients to create.
 */
struct TextureFactoryIdentifier
{
  LayersBackend mParentBackend;
  GeckoProcessType mParentProcessId;
  int32_t mMaxTextureSize;
  bool mSupportsTextureBlitting;
  bool mSupportsPartialUploads;

  TextureFactoryIdentifier(LayersBackend aLayersBackend = LAYERS_NONE,
                           GeckoProcessType aParentProcessId = GeckoProcessType_Default,
                           int32_t aMaxTextureSize = 0,
                           bool aSupportsTextureBlitting = false,
                           bool aSupportsPartialUploads = false)
    : mParentBackend(aLayersBackend)
    , mParentProcessId(aParentProcessId)
    , mMaxTextureSize(aMaxTextureSize)
    , mSupportsTextureBlitting(aSupportsTextureBlitting)
    , mSupportsPartialUploads(aSupportsPartialUploads)
  {}
};

/**
 * Identify a texture to a compositable. Many textures can have the same id, but
 * the id is unique for any texture owned by a particular compositable.
 * XXX - This is now redundant with TextureFlags. it ill be removed along with
 * deprecated texture classes.
 */
typedef uint32_t TextureIdentifier;
const TextureIdentifier TextureFront = 1;
const TextureIdentifier TextureBack = 2;
const TextureIdentifier TextureOnWhiteFront = 3;
const TextureIdentifier TextureOnWhiteBack = 4;

/**
 * Information required by the compositor from the content-side for creating or
 * using compositables and textures.
 * XXX - TextureInfo is a bad name: this information is useful for the compositable,
 * not the Texture. And ith new Textures, only the compositable type is really
 * useful. This may (should) be removed in the near future.
 */
struct TextureInfo
{
  CompositableType mCompositableType;
  uint32_t mDeprecatedTextureHostFlags;
  uint32_t mTextureFlags;

  TextureInfo()
    : mCompositableType(BUFFER_UNKNOWN)
    , mDeprecatedTextureHostFlags(0)
    , mTextureFlags(0)
  {}

  TextureInfo(CompositableType aType)
    : mCompositableType(aType)
    , mDeprecatedTextureHostFlags(0)
    , mTextureFlags(0)
  {}

  bool operator==(const TextureInfo& aOther) const
  {
    return mCompositableType == aOther.mCompositableType &&
           mDeprecatedTextureHostFlags == aOther.mDeprecatedTextureHostFlags &&
           mTextureFlags == aOther.mTextureFlags;
  }
};

/**
 * How a SurfaceDescriptor will be opened.
 *
 * See ShadowLayerForwarder::OpenDescriptor for example.
 */
enum OpenMode {
  OPEN_READ_ONLY  = 0x1,
  OPEN_READ_WRITE = 0x2
};

// The kinds of mask texture a shader can support
// We rely on the items in this enum being sequential
enum MaskType {
  MaskNone = 0,   // no mask layer
  Mask2d,         // mask layer for layers with 2D transforms
  Mask3d,         // mask layer for layers with 3D transforms
  NumMaskTypes
};

} // namespace layers
} // namespace mozilla

#endif
