/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_COMPOSITORTYPES_H
#define MOZILLA_LAYERS_COMPOSITORTYPES_H

#include <stdint.h>                     // for uint32_t
#include <sys/types.h>                  // for int32_t
#include "LayersTypes.h"                // for LayersBackend, etc
#include "nsXULAppAPI.h"                // for GeckoProcessType, etc

#include "mozilla/TypedEnum.h"
#include "mozilla/TypedEnumBits.h"

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
MOZ_BEGIN_ENUM_CLASS(TextureFlags, uint32_t)
  NO_FLAGS           = 0,
  // Use nearest-neighbour texture filtering (as opposed to linear filtering).
  USE_NEAREST_FILTER = 1 << 0,
  // The texture should be flipped around the y-axis when composited.
  NEEDS_Y_FLIP       = 1 << 1,
  // Force the texture to be represented using a single tile (note that this means
  // tiled textures, not tiled layers).
  DISALLOW_BIGIMAGE  = 1 << 2,
  // Allow using 'repeat' mode for wrapping.
  ALLOW_REPEAT       = 1 << 3,
  // The texture represents a tile which is newly created.
  NEW_TILE           = 1 << 4,
  // The texture is part of a component-alpha pair
  COMPONENT_ALPHA    = 1 << 5,
  // The buffer will be treated as if the RB bytes are swapped.
  // This is useful for rendering using Cairo/Thebes, because there is no
  // BGRX Android pixel format, and so we have to do byte swapping.
  //
  // For example, if the GraphicBuffer has an Android pixel format of
  // PIXEL_FORMAT_RGBA_8888 and isRBSwapped is true, when it is sampled
  // (for example, with GL), a BGRA shader should be used.
  RB_SWAPPED         = 1 << 6,

  FRONT              = 1 << 7,
  // A texture host on white for component alpha
  ON_WHITE           = 1 << 8,
  // A texture host on black for component alpha
  ON_BLACK           = 1 << 9,
  // A texture host that supports tiling
  TILE               = 1 << 10,
  // A texture should be recycled when no longer in used
  RECYCLE            = 1 << 11,
  // Texture contents should be initialized
  // from the previous texture.
  COPY_PREVIOUS      = 1 << 12,
  // Who is responsible for deallocating the shared data.
  // if DEALLOCATE_CLIENT is set, the shared data is deallocated on the
  // client side and requires some extra synchronizaion to ensure race-free
  // deallocation.
  // The default behaviour is to deallocate on the host side.
  DEALLOCATE_CLIENT  = 1 << 13,
  // After being shared ith the compositor side, an immutable texture is never
  // modified, it can only be read. It is safe to not Lock/Unlock immutable
  // textures.
  IMMUTABLE          = 1 << 14,
  // The contents of the texture must be uploaded or copied immediately
  // during the transaction, because the producer may want to write
  // to it again.
  IMMEDIATE_UPLOAD   = 1 << 15,
  // The texture is going to be used as part of a double
  // buffered pair, and so we can guarantee that the producer/consumer
  // won't be racing to access its contents.
  DOUBLE_BUFFERED    = 1 << 16,
  // We've previously tried a texture and it didn't work for some reason. If there
  // is a fallback available, try that.
  ALLOC_FALLBACK     = 1 << 17,
  // OR union of all valid bits
  ALL_BITS           = (1 << 18) - 1,
  // the default flags
  DEFAULT = FRONT
MOZ_END_ENUM_CLASS(TextureFlags)
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(TextureFlags)

static inline bool
TextureRequiresLocking(TextureFlags aFlags)
{
  // If we're not double buffered, or uploading
  // within a transaction, then we need to support
  // locking correctly.
  return !(aFlags & (TextureFlags::IMMEDIATE_UPLOAD |
                     TextureFlags::DOUBLE_BUFFERED |
                     TextureFlags::IMMUTABLE));
}

/**
 * The type of debug diagnostic to enable.
 */
typedef uint32_t DiagnosticTypes;
const DiagnosticTypes DIAGNOSTIC_NONE             = 0;
const DiagnosticTypes DIAGNOSTIC_TILE_BORDERS     = 1 << 0;
const DiagnosticTypes DIAGNOSTIC_LAYER_BORDERS    = 1 << 1;
const DiagnosticTypes DIAGNOSTIC_BIGIMAGE_BORDERS = 1 << 2;
const DiagnosticTypes DIAGNOSTIC_FLASH_BORDERS    = 1 << 3;

#define DIAGNOSTIC_FLASH_COUNTER_MAX 100

/**
 * Information about the object that is being diagnosed.
 */
typedef uint32_t DiagnosticFlags;
const DiagnosticFlags DIAGNOSTIC_IMAGE      = 1 << 0;
const DiagnosticFlags DIAGNOSTIC_CONTENT    = 1 << 1;
const DiagnosticFlags DIAGNOSTIC_CANVAS     = 1 << 2;
const DiagnosticFlags DIAGNOSTIC_COLOR      = 1 << 3;
const DiagnosticFlags DIAGNOSTIC_CONTAINER  = 1 << 4;
const DiagnosticFlags DIAGNOSTIC_TILE       = 1 << 5;
const DiagnosticFlags DIAGNOSTIC_BIGIMAGE   = 1 << 6;
const DiagnosticFlags DIAGNOSTIC_COMPONENT_ALPHA = 1 << 7;
const DiagnosticFlags DIAGNOSTIC_REGION_RECT = 1 << 8;

/**
 * See gfx/layers/Effects.h
 */
enum EffectTypes
{
  EFFECT_MASK,
  EFFECT_MAX_SECONDARY, // sentinel for the count of secondary effect types
  EFFECT_RGB,
  EFFECT_YCBCR,
  EFFECT_COMPONENT_ALPHA,
  EFFECT_SOLID_COLOR,
  EFFECT_RENDER_TARGET,
  EFFECT_MAX  //sentinel for the count of all effect types
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
  BUFFER_CONTENT_INC,     // thebes layer interface, only sends incremental
                          // updates to a texture on the compositor side.
  // somewhere in the middle
  BUFFER_TILED,           // tiled thebes layer
  BUFFER_SIMPLE_TILED,
  // the new compositable types
  COMPOSITABLE_IMAGE,     // image with single buffering
  COMPOSITABLE_CONTENT_SINGLE,  // thebes layer interface, single buffering
  COMPOSITABLE_CONTENT_DOUBLE,  // thebes layer interface, double buffering
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

  TextureFactoryIdentifier(LayersBackend aLayersBackend = LayersBackend::LAYERS_NONE,
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
  TextureFlags mTextureFlags;

  TextureInfo()
    : mCompositableType(BUFFER_UNKNOWN)
    , mDeprecatedTextureHostFlags(0)
    , mTextureFlags(TextureFlags::NO_FLAGS)
  {}

  TextureInfo(CompositableType aType)
    : mCompositableType(aType)
    , mDeprecatedTextureHostFlags(0)
    , mTextureFlags(TextureFlags::NO_FLAGS)
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
typedef uint32_t OpenMode;
const OpenMode OPEN_READ        = 0x1;
const OpenMode OPEN_WRITE       = 0x2;
const OpenMode OPEN_READ_WRITE  = OPEN_READ|OPEN_WRITE;
const OpenMode OPEN_READ_ONLY   = OPEN_READ;
const OpenMode OPEN_WRITE_ONLY  = OPEN_WRITE;

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
