/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_COMPOSITORTYPES_H
#define MOZILLA_LAYERS_COMPOSITORTYPES_H

#include "LayersTypes.h"

namespace mozilla {
namespace layers {

typedef int32_t SurfaceDescriptorType;
const SurfaceDescriptorType SURFACEDESCRIPTOR_UNKNOWN = 0;

/**
 * Flags used by texture clients and texture hosts. These are passed from client
 * side to host side when textures and compositables are created. Usually set
 * by the compositableCient, they may be modified by either the compositable or
 * texture clients.
 */
typedef uint32_t TextureFlags;
// Use nearest-neighbour texture filtering (as opposed to linear filtering).
const TextureFlags UseNearestFilter   = 0x1;
// The texture should be flipped around the y-axis when composited.
const TextureFlags NeedsYFlip         = 0x2;
// Force the texture to be represented using a single tile (note that this means
// tiled textures, not tiled layers).
const TextureFlags ForceSingleTile    = 0x4;
// Allow using 'repeat' mode for wrapping.
const TextureFlags AllowRepeat        = 0x8;
// The texture represents a tile which is newly created.
const TextureFlags NewTile            = 0x10;
// The host is responsible for tidying up any shared resources.
const TextureFlags HostRelease        = 0x20;
// The texture is part of a component-alpha pair
const TextureFlags ComponentAlpha     = 0x40;

/**
 * The kind of memory held by the texture client/host pair. This will
 * determine how the texture client is drawn into and how the memory
 * is shared between client and host.
 */
enum TextureClientType
{
  TEXTURE_CONTENT,            // dynamically drawn content
  TEXTURE_SHMEM,              // shared memory
  TEXTURE_YCBCR,              // ShmemYCbCrImage
  TEXTURE_SHARED_GL,          // GLContext::SharedTextureHandle
  TEXTURE_SHARED_GL_EXTERNAL, // GLContext::SharedTextureHandle, the ownership of
                              // the SurfaceDescriptor passed to the texture
                              // remains with whoever passed it.
  TEXTURE_STREAM_GL           // WebGL streaming buffer
};

/**
 * How the Compositable should manage textures.
 */
enum CompositableType
{
  BUFFER_UNKNOWN,
  BUFFER_IMAGE_SINGLE,    // image/canvas with a single texture, single buffered
  BUFFER_IMAGE_BUFFERED,  // image/canvas, double buffered
  BUFFER_BRIDGE,          // image bridge protocol
  BUFFER_CONTENT,         // thebes layer interface, single buffering
  BUFFER_CONTENT_DIRECT,  // thebes layer interface, double buffering
  BUFFER_TILED,           // tiled thebes layer
  BUFFER_COUNT
};

/**
 * How the texture host is used for composition,
 */
enum TextureHostFlags
{
  TEXTURE_HOST_DEFAULT = 0,       // The default texture host for the given
                                  // SurfaceDescriptor
  TEXTURE_HOST_TILED = 1 << 0     // A texture host that supports tiling
};

/**
 * Sent from the compositor to the content-side LayerManager, includes properties
 * of the compositor and should (in the future) include information about what
 * kinds of buffer and texture clients to create.
 */
struct TextureFactoryIdentifier
{
  LayersBackend mParentBackend;
  int32_t mMaxTextureSize;

  TextureFactoryIdentifier(LayersBackend aLayersBackend = LAYERS_NONE,
                           int32_t aMaxTextureSize = 0)
    : mParentBackend(aLayersBackend)
    , mMaxTextureSize(aMaxTextureSize)
  {}
};

/**
 * Identify a texture to a compositable. Many textures can have the same id, but
 * the id is unique for any texture owned by a particular compositable.
 */
typedef uint32_t TextureIdentifier;
const TextureIdentifier TextureFront = 1;
const TextureIdentifier TextureBack = 2;
const TextureIdentifier TextureOnWhiteFront = 3;
const TextureIdentifier TextureOnWhiteBack = 4;

/**
 * Information required by the compositor from the content-side for creating or
 * using compositables and textures.
 */
struct TextureInfo
{
  CompositableType mCompositableType;
  uint32_t mTextureHostFlags;
  uint32_t mTextureFlags;

  TextureInfo()
    : mCompositableType(BUFFER_UNKNOWN)
    , mTextureHostFlags(0)
    , mTextureFlags(0)
  {}

  TextureInfo(CompositableType aType)
    : mCompositableType(aType)
    , mTextureHostFlags(0)
    , mTextureFlags(0)
  {}

  bool operator==(const TextureInfo& aOther) const
  {
    return mCompositableType == aOther.mCompositableType &&
           mTextureHostFlags == aOther.mTextureHostFlags &&
           mTextureFlags == aOther.mTextureFlags;
  }
};

/**
 * How a SurfaceDescriptor will be opened.
 *
 * See ShadowLayerForwarder::OpenDescriptor for example.
 */
enum OpenMode {
  OPEN_READ_ONLY,
  OPEN_READ_WRITE
};

} // namespace layers
} // namespace mozilla

#endif
