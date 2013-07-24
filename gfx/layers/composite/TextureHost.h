/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREHOST_H
#define MOZILLA_GFX_TEXTUREHOST_H

#include "mozilla/layers/LayersTypes.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/layers/CompositorTypes.h"
#include "nsAutoPtr.h"
#include "mozilla/RefPtr.h"
#include "mozilla/layers/ISurfaceAllocator.h"

class gfxReusableSurfaceWrapper;
class gfxImageSurface;

namespace mozilla {
namespace layers {

class Compositor;
class SurfaceDescriptor;
class ISurfaceAllocator;
class TextureSourceOGL;
class TextureSourceD3D11;
class TextureSourceBasic;
class TextureParent;

/**
 * A view on a DeprecatedTextureHost where the texture is internally represented as tiles
 * (contrast with a tiled buffer, where each texture is a tile). For iteration by
 * the texture's buffer host.
 * This is only useful when the underlying surface is too big to fit in one
 * device texture, which forces us to split it in smaller parts.
 * Tiled Compositable is a different thing.
 */
class TileIterator
{
public:
  virtual void BeginTileIteration() = 0;
  virtual void EndTileIteration() {};
  virtual nsIntRect GetTileRect() = 0;
  virtual size_t GetTileCount() = 0;
  virtual bool NextTile() = 0;
};

/**
 * TextureSource is the interface for texture objects that can be composited
 * by a given compositor backend. Since the drawing APIs are different
 * between backends, the TextureSource interface is split into different
 * interfaces (TextureSourceOGL, etc.), and TextureSource mostly provide
 * access to these interfaces.
 *
 * This class is used on the compositor side.
 */
class TextureSource : public RefCounted<TextureSource>
{
public:
  TextureSource()
  {
    MOZ_COUNT_CTOR(TextureSource);
  }
  virtual ~TextureSource()
  {
    MOZ_COUNT_DTOR(TextureSource);
  }

  /**
   * Returns the size of the texture in texels.
   * If the underlying texture host is a tile iterator, GetSize must return the
   * size of the current tile.
   */
  virtual gfx::IntSize GetSize() const = 0;

  virtual gfx::SurfaceFormat GetFormat() const { return gfx::FORMAT_UNKNOWN; }

  /**
   * Cast to an TextureSource for the OpenGL backend.
   */
  virtual TextureSourceOGL* AsSourceOGL() { return nullptr; }

  /**
   * Cast to an TextureSource for the D3D11 backend.
   */
  virtual TextureSourceD3D11* AsSourceD3D11() { return nullptr; }

  virtual TextureSourceBasic* AsSourceBasic() { return nullptr; }

  /**
   * In some rare cases we currently need to consider a group of textures as one
   * TextureSource, that can be split in sub-TextureSources.
   */
  virtual TextureSource* GetSubSource(int index) { return nullptr; }
  /**
   * Overload this if the TextureSource supports big textures that don't fit in
   * one device texture and must be tiled internally.
   */
  virtual TileIterator* AsTileIterator() { return nullptr; }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif
};

/**
 * XXX - This class is deprectaed, will be removed soon.
 *
 * DeprecatedTextureHost is a thin abstraction over texture data that need to be shared
 * or transfered from the content process to the compositor process. It is the
 * compositor-side half of a DeprecatedTextureClient/DeprecatedTextureHost pair. A corresponding
 * DeprecatedTextureClient lives on the client-side.
 *
 * DeprecatedTextureHost only knows how to deserialize or synchronize generic image data
 * (SurfaceDescriptor) and provide access to one or more TextureSource objects
 * (these provide the necessary APIs for compositor backends to composite the
 * image).
 *
 * A DeprecatedTextureHost should mostly correspond to one or several SurfaceDescriptor
 * types. This means that for YCbCr planes, even though they are represented as
 * 3 textures internally, use 1 DeprecatedTextureHost and not 3, because the 3 planes
 * arrive in the same IPC message.
 *
 * The Lock/Unlock mecanism here mirrors Lock/Unlock in DeprecatedTextureClient. These two
 * methods don't always have to use blocking locks, unless a resource is shared
 * between the two sides (like shared texture handles). For instance, in some
 * cases the data received in Update(...) is a copy in shared memory of the data
 * owned by the content process, in which case no blocking lock is required.
 *
 * DeprecatedTextureHosts can be changed at any time, for example if we receive a
 * SurfaceDescriptor type that was not expected. This should be an incentive
 * to keep the ownership model simple (especially on the OpenGL case, where
 * we have additionnal constraints).
 *
 * There are two fundamental operations carried out on texture hosts - update
 * from the content thread and composition. Texture upload can occur in either
 * phase. Update happens in response to an IPDL message from content and
 * composition when the compositor 'ticks'. We may composite many times before
 * update.
 *
 * Update ends up at DeprecatedTextureHost::UpdateImpl. It always occurs in a layers
 * transacton. (TextureParent should call EnsureTexture before updating to
 * ensure the DeprecatedTextureHost exists and is of the correct type).
 *
 * CompositableHost::Composite does compositing. It should check the texture
 * host exists (and give up otherwise), then lock the texture host
 * (DeprecatedTextureHost::Lock). Then it passes the texture host to the Compositor in an
 * effect as a texture source, which does the actual composition. Finally the
 * compositable calls Unlock on the DeprecatedTextureHost.
 *
 * The class TextureImageDeprecatedTextureHostOGL is a good example of a DeprecatedTextureHost
 * implementation.
 *
 * This class is used only on the compositor side.
 */
class DeprecatedTextureHost : public TextureSource
{
public:
  /**
   * Create a new texture host to handle surfaces of aDescriptorType
   *
   * @param aDescriptorType The SurfaceDescriptor type being passed
   * @param aDeprecatedTextureHostFlags Modifier flags that specify changes in
   * the usage of a aDescriptorType, see DeprecatedTextureHostFlags
   * @param aTextureFlags Flags to pass to the new DeprecatedTextureHost
   */
  static TemporaryRef<DeprecatedTextureHost> CreateDeprecatedTextureHost(SurfaceDescriptorType aDescriptorType,
                                                     uint32_t aDeprecatedTextureHostFlags,
                                                     uint32_t aTextureFlags);

  DeprecatedTextureHost();
  virtual ~DeprecatedTextureHost();

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  virtual bool IsValid() const { return true; }

  /**
   * Update the texture host using the data from aSurfaceDescriptor.
   *
   * @param aImage Source image to update with.
   * @param aRegion Region of the texture host to update.
   * @param aOffset Offset in the source to update from
   */
  void Update(const SurfaceDescriptor& aImage,
              nsIntRegion *aRegion = nullptr,
              nsIntPoint* aOffset = nullptr);

  /**
   * Change the current surface of the texture host to aImage. aResult will return
   * the previous surface.
   */
  void SwapTextures(const SurfaceDescriptor& aImage,
                    SurfaceDescriptor* aResult = nullptr,
                    nsIntRegion *aRegion = nullptr);

  /**
   * Update for tiled texture hosts could probably have a better signature, but we
   * will replace it with PTexture stuff anyway, so nm.
   */
  virtual void Update(gfxReusableSurfaceWrapper* aReusableSurface,
  	                  TextureFlags aFlags,
  	                  const gfx::IntSize& aSize) {}

  /**
   * Lock the texture host for compositing, returns true if the DeprecatedTextureHost is
   * valid for composition.
   */
  virtual bool Lock() { return IsValid(); }

  /**
   * Unlock the texture host after compositing.
   * Should handle the case where Lock failed without crashing.
   */
  virtual void Unlock() {}

  void SetFlags(TextureFlags aFlags) { mFlags = aFlags; }
  void AddFlag(TextureFlags aFlag) { mFlags |= aFlag; }
  TextureFlags GetFlags() { return mFlags; }

  /**
   * Sets ths DeprecatedTextureHost's compositor.
   * A DeprecatedTextureHost can change compositor on certain occasions, in particular if
   * it belongs to an async Compositable.
   * aCompositor can be null, in which case the DeprecatedTextureHost must cleanup  all
   * of it's device textures.
   */
  virtual void SetCompositor(Compositor* aCompositor) {}

  ISurfaceAllocator* GetDeAllocator()
  {
    return mDeAllocator;
  }

  bool operator== (const DeprecatedTextureHost& o) const
  {
    return GetIdentifier() == o.GetIdentifier();
  }
  bool operator!= (const DeprecatedTextureHost& o) const
  {
    return GetIdentifier() != o.GetIdentifier();
  }

  virtual LayerRenderState GetRenderState()
  {
    return LayerRenderState();
  }

  virtual already_AddRefed<gfxImageSurface> GetAsSurface() = 0;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char *Name() = 0;
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix);
#endif

  /**
   * TEMPORARY.
   *
   * Ensure that a buffer of the given size/type has been allocated so that
   * we can update it using Update and/or CopyTo.
   */
  virtual void EnsureBuffer(const nsIntSize& aSize, gfxASurface::gfxContentType aType)
  {
    NS_RUNTIMEABORT("DeprecatedTextureHost doesn't support EnsureBuffer");
  }

  /**
   * Copy the contents of this DeprecatedTextureHost to aDest. aDest must already
   * have a suitable buffer allocated using EnsureBuffer.
   *
   * @param aSourceRect Area of this texture host to copy.
   * @param aDest Destination texture host.
   * @param aDestRect Destination rect.
   */
  virtual void CopyTo(const nsIntRect& aSourceRect,
                      DeprecatedTextureHost *aDest,
                      const nsIntRect& aDestRect)
  {
    NS_RUNTIMEABORT("DeprecatedTextureHost doesn't support CopyTo");
  }


  SurfaceDescriptor* GetBuffer() const { return mBuffer; }

  /**
   * Set a SurfaceDescriptor for this texture host. By setting a buffer and
   * allocator/de-allocator for the DeprecatedTextureHost, you cause the DeprecatedTextureHost to
   * retain a SurfaceDescriptor.
   * Ownership of the SurfaceDescriptor passes to this.
   */
  // only made virtual to allow overriding in GrallocDeprecatedTextureHostOGL, for hacky fix in gecko 23 for bug 862324.
  // see bug 865908 about fixing this.
  virtual void SetBuffer(SurfaceDescriptor* aBuffer, ISurfaceAllocator* aAllocator)
  {
    MOZ_ASSERT(!mBuffer, "Will leak the old mBuffer");
    mBuffer = aBuffer;
    mDeAllocator = aAllocator;
  }

  // used only for hacky fix in gecko 23 for bug 862324
  // see bug 865908 about fixing this.
  virtual void ForgetBuffer() {}

protected:
  /**
   * Should be implemented by the backend-specific DeprecatedTextureHost classes
   *
   * It should not take a reference to aImage, unless it knows the data
   * to be thread-safe.
   */
  virtual void UpdateImpl(const SurfaceDescriptor& aImage,
                          nsIntRegion *aRegion,
                          nsIntPoint *aOffset = nullptr)
  {
    NS_RUNTIMEABORT("Should not be reached");
  }

  /**
   * Should be implemented by the backend-specific DeprecatedTextureHost classes.
   *
   * Doesn't need to do the actual surface descriptor swap, just
   * any preparation work required to use the new descriptor.
   *
   * If the implementation doesn't define anything in particular
   * for handling swaps, then we can just do an update instead.
   */
  virtual void SwapTexturesImpl(const SurfaceDescriptor& aImage,
                                nsIntRegion *aRegion)
  {
    UpdateImpl(aImage, aRegion, nullptr);
  }

  // An internal identifier for this texture host. Two texture hosts
  // should be considered equal iff their identifiers match. Should
  // not be exposed publicly.
  virtual uint64_t GetIdentifier() const
  {
    return reinterpret_cast<uint64_t>(this);
  }

  // Texture info
  TextureFlags mFlags;
  SurfaceDescriptor* mBuffer; // FIXME [bjacob] it's terrible to have a SurfaceDescriptor here,
                              // because SurfaceDescriptor's may have raw pointers to IPDL actors,
                              // which can go away under our feet at any time. This is the cause
                              // of bug 862324 among others. Our current understanding is that
                              // this will be gone in Gecko 24. See bug 858914.
  gfx::SurfaceFormat mFormat;

  ISurfaceAllocator* mDeAllocator;
};

class AutoLockDeprecatedTextureHost
{
public:
  AutoLockDeprecatedTextureHost(DeprecatedTextureHost* aHost)
    : mDeprecatedTextureHost(aHost)
    , mIsValid(true)
  {
    if (mDeprecatedTextureHost) {
      mIsValid = mDeprecatedTextureHost->Lock();
    }
  }

  ~AutoLockDeprecatedTextureHost()
  {
    if (mDeprecatedTextureHost && mIsValid) {
      mDeprecatedTextureHost->Unlock();
    }
  }

  bool IsValid() { return mIsValid; }

private:
  DeprecatedTextureHost *mDeprecatedTextureHost;
  bool mIsValid;
};

/**
 * This can be used as an offscreen rendering target by the compositor, and
 * subsequently can be used as a source by the compositor.
 */
class CompositingRenderTarget : public TextureSource
{
public:
  virtual ~CompositingRenderTarget() {}

#ifdef MOZ_DUMP_PAINTING
  virtual already_AddRefed<gfxImageSurface> Dump(Compositor* aCompositor) { return nullptr; }
#endif
};

}
}
#endif
