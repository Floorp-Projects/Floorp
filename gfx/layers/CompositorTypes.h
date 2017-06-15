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
#include "mozilla/gfx/Types.h"
#include "mozilla/EnumSet.h"

#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace layers {

/**
 * Flags used by texture clients and texture hosts. These are passed from client
 * side to host side when textures and compositables are created. Usually set
 * by the compositableCient, they may be modified by either the compositable or
 * texture clients.
 */
enum class TextureFlags : uint32_t {
  NO_FLAGS           = 0,
  // Use nearest-neighbour texture filtering (as opposed to linear filtering).
  USE_NEAREST_FILTER = 1 << 0,
  // The compositor assumes everything is origin-top-left by default.
  ORIGIN_BOTTOM_LEFT = 1 << 1,
  // Force the texture to be represented using a single tile (note that this means
  // tiled textures, not tiled layers).
  DISALLOW_BIGIMAGE  = 1 << 2,
  // The buffer will be treated as if the RB bytes are swapped.
  // This is useful for rendering using Cairo/Thebes, because there is no
  // BGRX Android pixel format, and so we have to do byte swapping.
  //
  // For example, if the GraphicBuffer has an Android pixel format of
  // PIXEL_FORMAT_RGBA_8888 and isRBSwapped is true, when it is sampled
  // (for example, with GL), a BGRA shader should be used.
  RB_SWAPPED         = 1 << 3,
  // Data in this texture has not been alpha-premultiplied.
  // XXX - Apparently only used with ImageClient/Host
  NON_PREMULTIPLIED  = 1 << 4,
  // The TextureClient should be recycled with recycle callback when no longer
  // in used. When the texture is used in host side, ref count of TextureClient
  // is transparently added by ShadowLayerForwarder or ImageBridgeChild.
  RECYCLE            = 1 << 5,
  // If DEALLOCATE_CLIENT is set, the shared data is deallocated on the
  // client side and requires some extra synchronizaion to ensure race-free
  // deallocation.
  // The default behaviour is to deallocate on the host side.
  DEALLOCATE_CLIENT  = 1 << 6,
  DEALLOCATE_SYNC    = 1 << 6, // XXX - make it a separate flag.
  DEALLOCATE_MAIN_THREAD = 1 << 8,
  // After being shared ith the compositor side, an immutable texture is never
  // modified, it can only be read. It is safe to not Lock/Unlock immutable
  // textures.
  IMMUTABLE          = 1 << 9,
  // The contents of the texture must be uploaded or copied immediately
  // during the transaction, because the producer may want to write
  // to it again.
  IMMEDIATE_UPLOAD   = 1 << 10,
  // The texture is part of a component-alpha pair
  COMPONENT_ALPHA    = 1 << 11,
  // The texture is being allocated for a compositor that no longer exists.
  // This flag is only used in the parent process.
  INVALID_COMPOSITOR = 1 << 12,
  // The texture was created by converting from YCBCR to RGB
  RGB_FROM_YCBCR     = 1 << 13,
  // The texture is used for snapshot.
  SNAPSHOT           = 1 << 14,

  // OR union of all valid bits
  ALL_BITS           = (1 << 15) - 1,
  // the default flags
  DEFAULT = NO_FLAGS
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(TextureFlags)

static inline bool
TextureRequiresLocking(TextureFlags aFlags)
{
  // If we're not double buffered, or uploading
  // within a transaction, then we need to support
  // locking correctly.
  return !(aFlags & (TextureFlags::IMMEDIATE_UPLOAD |
                     TextureFlags::IMMUTABLE));
}

/**
 * The type of debug diagnostic to enable.
 */
enum class DiagnosticTypes : uint8_t {
  NO_DIAGNOSTIC    = 0,
  TILE_BORDERS     = 1 << 0,
  LAYER_BORDERS    = 1 << 1,
  BIGIMAGE_BORDERS = 1 << 2,
  FLASH_BORDERS    = 1 << 3,
  ALL_BITS         = (1 << 4) - 1
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(DiagnosticTypes)

#define DIAGNOSTIC_FLASH_COUNTER_MAX 100

/**
 * Information about the object that is being diagnosed.
 */
enum class DiagnosticFlags : uint16_t {
  NO_DIAGNOSTIC   = 0,
  IMAGE           = 1 << 0,
  CONTENT         = 1 << 1,
  CANVAS          = 1 << 2,
  COLOR           = 1 << 3,
  CONTAINER       = 1 << 4,
  TILE            = 1 << 5,
  BIGIMAGE        = 1 << 6,
  COMPONENT_ALPHA = 1 << 7,
  REGION_RECT     = 1 << 8,
  NV12            = 1 << 9,
  YCBCR           = 1 << 10
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(DiagnosticFlags)

/**
 * See gfx/layers/Effects.h
 */
enum class EffectTypes : uint8_t {
  MASK,
  BLEND_MODE,
  COLOR_MATRIX,
  MAX_SECONDARY, // sentinel for the count of secondary effect types
  RGB,
  YCBCR,
  NV12,
  COMPONENT_ALPHA,
  SOLID_COLOR,
  RENDER_TARGET,
  MAX  //sentinel for the count of all effect types
};

/**
 * How the Compositable should manage textures.
 */
enum class CompositableType : uint8_t {
  UNKNOWN,
  CONTENT_TILED,   // tiled painted layer
  IMAGE,           // image with single buffering
  IMAGE_BRIDGE,    // ImageBridge protocol
  CONTENT_SINGLE,  // painted layer interface, single buffering
  CONTENT_DOUBLE,  // painted layer interface, double buffering
  COUNT
};

#ifdef XP_WIN
typedef void* SyncHandle;
#else
typedef uintptr_t SyncHandle;
#endif // XP_WIN

/**
 * Sent from the compositor to the content-side LayerManager, includes properties
 * of the compositor and should (in the future) include information about what
 * kinds of buffer and texture clients to create.
 */
struct TextureFactoryIdentifier
{
  LayersBackend mParentBackend;
  GeckoProcessType mParentProcessType;
  int32_t mMaxTextureSize;
  bool mCompositorUseANGLE;
  bool mSupportsTextureBlitting;
  bool mSupportsPartialUploads;
  bool mSupportsComponentAlpha;
  SyncHandle mSyncHandle;

  explicit TextureFactoryIdentifier(LayersBackend aLayersBackend = LayersBackend::LAYERS_NONE,
                                    GeckoProcessType aParentProcessType = GeckoProcessType_Default,
                                    int32_t aMaxTextureSize = 4096,
                                    bool aCompositorUseANGLE = false,
                                    bool aSupportsTextureBlitting = false,
                                    bool aSupportsPartialUploads = false,
                                    bool aSupportsComponentAlpha = true,
                                    SyncHandle aSyncHandle = 0)
    : mParentBackend(aLayersBackend)
    , mParentProcessType(aParentProcessType)
    , mMaxTextureSize(aMaxTextureSize)
    , mCompositorUseANGLE(aCompositorUseANGLE)
    , mSupportsTextureBlitting(aSupportsTextureBlitting)
    , mSupportsPartialUploads(aSupportsPartialUploads)
    , mSupportsComponentAlpha(aSupportsComponentAlpha)
    , mSyncHandle(aSyncHandle)
  {}

  bool operator==(const TextureFactoryIdentifier& aOther) const {
    return
      mParentBackend == aOther.mParentBackend &&
      mParentProcessType == aOther.mParentProcessType &&
      mMaxTextureSize == aOther.mMaxTextureSize &&
      mCompositorUseANGLE == aOther.mCompositorUseANGLE &&
      mSupportsTextureBlitting == aOther.mSupportsTextureBlitting &&
      mSupportsPartialUploads == aOther.mSupportsPartialUploads &&
      mSupportsComponentAlpha == aOther.mSupportsComponentAlpha &&
      mSyncHandle == aOther.mSyncHandle;
  }
};

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
  TextureFlags mTextureFlags;

  TextureInfo()
    : mCompositableType(CompositableType::UNKNOWN)
    , mTextureFlags(TextureFlags::NO_FLAGS)
  {}

  explicit TextureInfo(CompositableType aType,
                       TextureFlags aTextureFlags = TextureFlags::DEFAULT)
    : mCompositableType(aType)
    , mTextureFlags(aTextureFlags)
  {}

  bool operator==(const TextureInfo& aOther) const
  {
    return mCompositableType == aOther.mCompositableType &&
           mTextureFlags == aOther.mTextureFlags;
  }
};

/**
 * How a SurfaceDescriptor will be opened.
 *
 * See ShadowLayerForwarder::OpenDescriptor for example.
 */
enum class OpenMode : uint8_t {
  OPEN_NONE        = 0,
  OPEN_READ        = 0x1,
  OPEN_WRITE       = 0x2,
  OPEN_READ_WRITE  = OPEN_READ|OPEN_WRITE,
  OPEN_READ_ONLY   = OPEN_READ,
  OPEN_WRITE_ONLY  = OPEN_WRITE
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(OpenMode)

// The kinds of mask texture a shader can support
// We rely on the items in this enum being sequential
enum class MaskType : uint8_t {
  MaskNone = 0,   // no mask layer
  Mask,           // mask layer
  NumMaskTypes
};

} // namespace layers
} // namespace mozilla

#endif
