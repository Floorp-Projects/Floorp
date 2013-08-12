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
namespace gfx {
class DataSourceSurface;
}
}

namespace mozilla {
namespace layers {

class Compositor;
class SurfaceDescriptor;
class ISurfaceAllocator;
class TextureSourceOGL;
class TextureSourceD3D9;
class TextureSourceD3D11;
class TextureSourceBasic;
class TextureParent;
class DataTextureSource;

/**
 * A view on a TextureHost where the texture is internally represented as tiles
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
   * Return the size of the texture in texels.
   * If this is a tile iterator, GetSize must return the size of the current tile.
   */
  virtual gfx::IntSize GetSize() const = 0;

  /**
   * Return the pixel format of this texture
   */
  virtual gfx::SurfaceFormat GetFormat() const { return gfx::FORMAT_UNKNOWN; }

  /**
   * Cast to a TextureSource for for each backend..
   */
  virtual TextureSourceOGL* AsSourceOGL() { return nullptr; }
  virtual TextureSourceD3D9* AsSourceD3D9() { return nullptr; }
  virtual TextureSourceD3D11* AsSourceD3D11() { return nullptr; }
  virtual TextureSourceBasic* AsSourceBasic() { return nullptr; }

  /**
   * Cast to a DataTextureSurce.
   */
  virtual DataTextureSource* AsDataTextureSource() { return nullptr; }

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
 * XXX - merge this class with TextureSource when deprecated texture classes
 * are completely removed.
 */
class NewTextureSource : public TextureSource
{
public:
  NewTextureSource()
  {
    MOZ_COUNT_CTOR(NewTextureSource);
  }
  virtual ~NewTextureSource()
  {
    MOZ_COUNT_DTOR(NewTextureSource);
  }

  /**
   * Should be overriden in order to deallocate the data that is associated
   * with the rendering backend, such as GL textures.
   */
  virtual void DeallocateDeviceData() = 0;

  void SetNextSibling(NewTextureSource* aTexture)
  {
    mNextSibling = aTexture;
  }

  NewTextureSource* GetNextSibling() const
  {
    return mNextSibling;
  }

  // temporary adapter to use the same SubSource API as the old TextureSource
  virtual TextureSource* GetSubSource(int index) MOZ_OVERRIDE
  {
    switch (index) {
      case 0: return this;
      case 1: return GetNextSibling();
      case 2: return GetNextSibling() ? GetNextSibling()->GetNextSibling() : nullptr;
    }
    return nullptr;
  }

protected:
  RefPtr<NewTextureSource> mNextSibling;
};

/**
 * Interface for TextureSources that can be updated from a DataSourceSurface.
 *
 * All backend should implement at least one DataTextureSource.
 */
class DataTextureSource : public NewTextureSource
{
public:
  DataTextureSource()
    : mUpdateSerial(0)
  {}

  virtual DataTextureSource* AsDataTextureSource() MOZ_OVERRIDE { return this; }

  /**
   * Upload a (portion of) surface to the TextureSource.
   *
   * The DataTextureSource doesn't own aSurface, although it owns and manage
   * the device texture it uploads to internally.
   */
  virtual bool Update(gfx::DataSourceSurface* aSurface,
                      TextureFlags aFlags,
                      nsIntRegion* aDestRegion = nullptr,
                      gfx::IntPoint* aSrcOffset = nullptr) = 0;

  /**
   * A facility to avoid reuploading when it is not necessary.
   * The caller of Update can use GetUpdateSerial to see if the number has changed
   * since last update, and call SetUpdateSerial after each successful update.
   * The caller is responsible for managing the update serial except when the
   * texture data is deallocated in which case the TextureSource should always
   * reset the update serial to zero.
   */
  uint32_t GetUpdateSerial() const { return mUpdateSerial; }
  void SetUpdateSerial(uint32_t aValue) { mUpdateSerial = aValue; }

  // By default at least set the update serial to zero.
  // overloaded versions should do that too.
  virtual void DeallocateDeviceData() MOZ_OVERRIDE
  {
    SetUpdateSerial(0);
  }

  /**
   * Provide read access to the data as a DataSourceSurface.
   *
   * This is expected to be very slow and should be used for mostly debugging.
   * XXX - implement everywhere and make it pure virtual.
   */
  virtual TemporaryRef<gfx::DataSourceSurface> ReadBack() { return nullptr; };
private:
  uint32_t mUpdateSerial;
};

/**
 * TextureHost is a thin abstraction over texture data that need to be shared
 * between the content process and the compositor process. It is the
 * compositor-side half of a TextureClient/TextureHost pair. A corresponding
 * TextureClient lives on the content-side.
 *
 * TextureHost only knows how to deserialize or synchronize generic image data
 * (SurfaceDescriptor) and provide access to one or more TextureSource objects
 * (these provide the necessary APIs for compositor backends to composite the
 * image).
 *
 * A TextureHost implementation corresponds to one SurfaceDescriptor type, as
 * opposed to TextureSource that corresponds to device textures.
 * This means that for YCbCr planes, even though they are represented as
 * 3 textures internally (3 TextureSources), we use 1 TextureHost and not 3,
 * because the 3 planes are stored in the same buffer of shared memory, before
 * they are uploaded separately.
 *
 * There is always one and only one TextureHost per TextureClient, and the
 * TextureClient/Host pair only owns one buffer of image data through its
 * lifetime. This means that the lifetime of the underlying shared data
 * matches the lifetime of the TextureClient/Host pair. It also means
 * TextureClient/Host do not implement double buffering, which is the
 * reponsibility of the compositable (which would use two Texture pairs).
 *
 * The Lock/Unlock mecanism here mirrors Lock/Unlock in TextureClient.
 *
 */
class TextureHost : public RefCounted<TextureHost>
{
public:
  TextureHost(uint64_t aID,
              TextureFlags aFlags)
    : mID(aID)
    , mNextTexture(nullptr)
    , mFlags(aFlags)
  {}

  virtual ~TextureHost() {}

  /**
   * Factory method.
   */
  static TemporaryRef<TextureHost> Create(uint64_t aID,
                                          const SurfaceDescriptor& aDesc,
                                          ISurfaceAllocator* aDeallocator,
                                          TextureFlags aFlags);

  /**
   * Lock the texture host for compositing.
   */
  virtual bool Lock() { return true; }

  /**
   * Unlock the texture host after compositing.
   */
  virtual void Unlock() {}

  /**
   * Note that the texture host format can be different from its corresponding
   * texture source's. For example a ShmemTextureHost can have the ycbcr
   * format and produce 3 "alpha" textures sources.
   */
  virtual gfx::SurfaceFormat GetFormat() const = 0;

  /**
   * Return a list of TextureSources for use with a Compositor.
   *
   * This can trigger texture uploads, so do not call it inside transactions
   * so as to not upload textures while the main thread is blocked.
   * Must not be called while this TextureHost is not sucessfully Locked.
   */
  virtual NewTextureSource* GetTextureSources() = 0;

  /**
   * Is called before compositing if the shared data has changed since last
   * composition.
   * This method should be overload in cases like when we need to do a texture
   * upload for example.
   *
   * @param aRegion The region that has been changed, if nil, it means that the
   * entire surface should be updated.
   */
  virtual void Updated(const nsIntRegion* aRegion) {}

  /**
   * Sets this TextureHost's compositor.
   * A TextureHost can change compositor on certain occasions, in particular if
   * it belongs to an async Compositable.
   * aCompositor can be null, in which case the TextureHost must cleanup  all
   * of it's device textures.
   */
  virtual void SetCompositor(Compositor* aCompositor) {}

  /**
   * Should be overriden in order to deallocate the data that is associated
   * with the rendering backend, such as GL textures.
   */
  virtual void DeallocateDeviceData() {}

  /**
   * Should be overriden in order to deallocate the data that is shared with
   * the content side, such as shared memory.
   */
  virtual void DeallocateSharedData() {}

  /**
   * An ID to differentiate TextureHosts of a given CompositableHost.
   *
   * A TextureHost and its corresponding TextureClient always have the same ID.
   * TextureHosts of a given CompositableHost always have different IDs.
   * TextureHosts of different CompositableHosts, may have the same ID.
   * Zero is always an invalid ID.
   */
  uint64_t GetID() const { return mID; }

  virtual gfx::IntSize GetSize() const = 0;

  /**
   * TextureHosts are kept as a linked list in their compositable
   * XXX - This is just a poor man's PTexture. The purpose of this list is
   * to keep TextureHost alive which should be independent from compositables.
   * It will be removed when we add the PTetxure protocol (which will more
   * gracefully handle the lifetime of textures). See bug 897452
   */
  TextureHost* GetNextSibling() const { return mNextTexture; }
  void SetNextSibling(TextureHost* aNext) { mNextTexture = aNext; }

  /**
   * Debug facility.
   * XXX - cool kids use Moz2D
   */
  virtual already_AddRefed<gfxImageSurface> GetAsSurface() = 0;

  /**
   * XXX - Flags should only be set at creation time, this will be removed.
   */
  void SetFlags(TextureFlags aFlags) { mFlags = aFlags; }

  /**
   * XXX - Flags should only be set at creation time, this will be removed.
   */
  void AddFlag(TextureFlags aFlag) { mFlags |= aFlag; }

  TextureFlags GetFlags() { return mFlags; }

  /**
   * Specific to B2G's Composer2D
   * XXX - more doc here
   */
  virtual LayerRenderState GetRenderState()
  {
    // By default we return an empty render state, this should be overriden
    // by the TextureHost implementations that are used on B2G with Composer2D
    return LayerRenderState();
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual void PrintInfo(nsACString& aTo, const char* aPrefix)
  {
    RefPtr<TextureSource> source = GetTextureSources();
    if (source) {
      source->PrintInfo(aTo, aPrefix);
    }
  }
#endif

protected:
  uint64_t mID;
  RefPtr<TextureHost> mNextTexture;
  TextureFlags mFlags;
};

/**
 * TextureHost that wraps a random access buffer such as a Shmem or some raw
 * memory.
 *
 * This TextureHost is backend-independent and the backend-specific bits are
 * in the TextureSource.
 * This class must be inherited to implement GetBuffer and DeallocSharedData
 * (see ShmemTextureHost and MemoryTextureHost)
 *
 * Uploads happen when Lock is called.
 *
 * BufferTextureHost supports YCbCr and flavours of RGBA images (RGBX, A, etc.).
 */
class BufferTextureHost : public TextureHost
{
public:
  BufferTextureHost(uint64_t aID,
                    gfx::SurfaceFormat aFormat,
                    TextureFlags aFlags);

  ~BufferTextureHost();

  virtual uint8_t* GetBuffer() = 0;

  virtual void Updated(const nsIntRegion* aRegion) MOZ_OVERRIDE;

  virtual bool Lock() MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual NewTextureSource* GetTextureSources() MOZ_OVERRIDE;

  virtual void DeallocateDeviceData() MOZ_OVERRIDE;

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  /**
   * Return the format that is exposed to the compositor when calling
   * GetTextureSources.
   *
   * If the shared format is YCbCr and the compositor does not support it,
   * GetFormat will be RGB32 (even though mFormat is FORMAT_YUV).
   */
  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual already_AddRefed<gfxImageSurface> GetAsSurface() MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

protected:
  bool Upload(nsIntRegion *aRegion = nullptr);
  bool MaybeUpload(nsIntRegion *aRegion = nullptr);

  Compositor* mCompositor;
  RefPtr<DataTextureSource> mFirstSource;
  nsIntRegion mMaybeUpdatedRegion;
  gfx::IntSize mSize;
  // format of the data that is shared with the content process.
  gfx::SurfaceFormat mFormat;
  uint32_t mUpdateSerial;
  bool mLocked;
  bool mPartialUpdate;
};

/**
 * TextureHost that wraps shared memory.
 * the corresponding texture on the client side is ShmemTextureClient.
 * This TextureHost is backend-independent.
 */
class ShmemTextureHost : public BufferTextureHost
{
public:
  ShmemTextureHost(uint64_t aID,
                   const ipc::Shmem& aShmem,
                   gfx::SurfaceFormat aFormat,
                   ISurfaceAllocator* aDeallocator,
                   TextureFlags aFlags);

  ~ShmemTextureHost();

  virtual void DeallocateSharedData() MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() MOZ_OVERRIDE;

protected:
  ipc::Shmem* mShmem;
  ISurfaceAllocator* mDeallocator;
};

/**
 * TextureHost that wraps raw memory.
 * The corresponding texture on the client side is MemoryTextureClient.
 * Can obviously not be used in a cross process setup.
 * This TextureHost is backend-independent.
 */
class MemoryTextureHost : public BufferTextureHost
{
public:
  MemoryTextureHost(uint64_t aID,
                    uint8_t* aBuffer,
                    gfx::SurfaceFormat aFormat,
                    TextureFlags aFlags);

  ~MemoryTextureHost();

  virtual void DeallocateSharedData() MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() MOZ_OVERRIDE;

protected:
  uint8_t* mBuffer;
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
 * The Lock/Unlock mechanism here mirrors Lock/Unlock in DeprecatedTextureClient. These two
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
  ISurfaceAllocator* mDeAllocator;
  gfx::SurfaceFormat mFormat;
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

/**
 * Creates a TextureHost that can be used with any of the existing backends
 * Not all SurfaceDescriptor types are supported
 */
TemporaryRef<TextureHost>
CreateBackendIndependentTextureHost(uint64_t aID,
                                    const SurfaceDescriptor& aDesc,
                                    ISurfaceAllocator* aDeallocator,
                                    TextureFlags aFlags);

}
}

#endif
