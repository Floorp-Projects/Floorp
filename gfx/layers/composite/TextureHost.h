/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTUREHOST_H
#define MOZILLA_GFX_TEXTUREHOST_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint64_t, uint32_t, uint8_t
#include "gfxTypes.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr, TemporaryRef, etc
#include "mozilla/gfx/2D.h"             // for DataSourceSurface
#include "mozilla/gfx/Point.h"          // for IntSize, IntPoint
#include "mozilla/gfx/Types.h"          // for SurfaceFormat, etc
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags, etc
#include "mozilla/layers/FenceUtils.h"  // for FenceHandle
#include "mozilla/layers/LayersTypes.h"  // for LayerRenderState, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_RUNTIMEABORT
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc
#include "nscore.h"                     // for nsACString
#include "mozilla/layers/AtomicRefCountedWithFinalize.h"

class gfxReusableSurfaceWrapper;
struct nsIntPoint;
struct nsIntSize;
struct nsIntRect;

namespace mozilla {
namespace gfx {
class SurfaceStream;
}
namespace ipc {
class Shmem;
}

namespace layers {

class Compositor;
class CompositableHost;
class CompositableBackendSpecificData;
class CompositableParentManager;
class SurfaceDescriptor;
class SurfaceStreamDescriptor;
class ISurfaceAllocator;
class TextureHostOGL;
class TextureSourceOGL;
class TextureSourceD3D9;
class TextureSourceD3D11;
class TextureSourceBasic;
class DataTextureSource;
class PTextureParent;
class TextureParent;

/**
 * A view on a TextureHost where the texture is internally represented as tiles
 * (contrast with a tiled buffer, where each texture is a tile). For iteration by
 * the texture's buffer host.
 * This is only useful when the underlying surface is too big to fit in one
 * device texture, which forces us to split it in smaller parts.
 * Tiled Compositable is a different thing.
 */
class BigImageIterator
{
public:
  virtual void BeginBigImageIteration() = 0;
  virtual void EndBigImageIteration() {};
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
class TextureSource
{
protected:
  virtual ~TextureSource();

public:
  NS_INLINE_DECL_REFCOUNTING(TextureSource)

  TextureSource();

  /**
   * Return the size of the texture in texels.
   * If this is a tile iterator, GetSize must return the size of the current tile.
   */
  virtual gfx::IntSize GetSize() const = 0;

  /**
   * Return the pixel format of this texture
   */
  virtual gfx::SurfaceFormat GetFormat() const { return gfx::SurfaceFormat::UNKNOWN; }

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
  virtual BigImageIterator* AsBigImageIterator() { return nullptr; }

  virtual void SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData);

protected:
  RefPtr<CompositableBackendSpecificData> mCompositableBackendData;
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
   * Should be overridden in order to deallocate the data that is associated
   * with the rendering backend, such as GL textures.
   */
  virtual void DeallocateDeviceData() = 0;

  virtual void SetCompositor(Compositor* aCompositor) {}

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

#ifdef DEBUG
  /**
   * Provide read access to the data as a DataSourceSurface.
   *
   * This is expected to be very slow and should be used for mostly debugging.
   * XXX - implement everywhere and make it pure virtual.
   */
  virtual TemporaryRef<gfx::DataSourceSurface> ReadBack() { return nullptr; };
#endif

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
class TextureHost
  : public AtomicRefCountedWithFinalize<TextureHost>
{
  /**
   * Called once, just before the destructor.
   *
   * Here goes the shut-down code that uses virtual methods.
   * Must only be called by Release().
   */
  void Finalize();

  friend class AtomicRefCountedWithFinalize<TextureHost>;
public:
  TextureHost(TextureFlags aFlags);

  virtual ~TextureHost();

  /**
   * Factory method.
   */
  static TemporaryRef<TextureHost> Create(const SurfaceDescriptor& aDesc,
                                          ISurfaceAllocator* aDeallocator,
                                          TextureFlags aFlags);

  /**
   * Tell to TextureChild that TextureHost is recycled.
   * This function should be called from TextureHost's RecycleCallback.
   * If SetRecycleCallback is set to TextureHost.
   * TextureHost can be recycled by calling RecycleCallback
   * when reference count becomes one.
   * One reference count is always added by TextureChild.
   */
  void CompositorRecycle();

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
  virtual void Updated(const nsIntRegion* aRegion = nullptr) {}

  /**
   * Sets this TextureHost's compositor.
   * A TextureHost can change compositor on certain occasions, in particular if
   * it belongs to an async Compositable.
   * aCompositor can be null, in which case the TextureHost must cleanup  all
   * of it's device textures.
   */
  virtual void SetCompositor(Compositor* aCompositor) {}

  /**
   * Should be overridden in order to deallocate the data that is associated
   * with the rendering backend, such as GL textures.
   */
  virtual void DeallocateDeviceData() {}

  /**
   * Should be overridden in order to deallocate the data that is shared with
   * the content side, such as shared memory.
   */
  virtual void DeallocateSharedData() {}

  /**
   * Should be overridden in order to force the TextureHost to drop all references
   * to it's shared data.
   *
   * This is important to ensure the correctness of the deallocation protocol.
   */
  virtual void ForgetSharedData() {}

  virtual gfx::IntSize GetSize() const = 0;

  /**
   * Debug facility.
   * XXX - cool kids use Moz2D. See bug 882113.
   */
  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() = 0;

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
   * Allocate and deallocate a TextureParent actor.
   *
   * TextureParent< is an implementation detail of TextureHost that is not
   * exposed to the rest of the code base. CreateIPDLActor and DestroyIPDLActor
   * are for use with the managing IPDL protocols only (so that they can
   * implement AllocPTextureParent and DeallocPTextureParent).
   */
  static PTextureParent* CreateIPDLActor(CompositableParentManager* aManager,
                                         const SurfaceDescriptor& aSharedData,
                                         TextureFlags aFlags);
  static bool DestroyIPDLActor(PTextureParent* actor);

  /**
   * Destroy the TextureChild/Parent pair.
   */
  static bool SendDeleteIPDLActor(PTextureParent* actor);

  /**
   * Get the TextureHost corresponding to the actor passed in parameter.
   */
  static TextureHost* AsTextureHost(PTextureParent* actor);

  /**
   * Return a pointer to the IPDLActor.
   *
   * This is to be used with IPDL messages only. Do not store the returned
   * pointer.
   */
  PTextureParent* GetIPDLActor();

  static void SendFenceHandleIfPresent(PTextureParent* actor);

  FenceHandle GetAndResetReleaseFenceHandle();

  /**
   * Specific to B2G's Composer2D
   * XXX - more doc here
   */
  virtual LayerRenderState GetRenderState()
  {
    // By default we return an empty render state, this should be overridden
    // by the TextureHost implementations that are used on B2G with Composer2D
    return LayerRenderState();
  }

  virtual void SetCompositableBackendSpecificData(CompositableBackendSpecificData* aBackendData);

  // If a texture host holds a reference to shmem, it should override this method
  // to forget about the shmem _without_ releasing it.
  virtual void OnShutdown() {}

  // Forget buffer actor. Used only for hacky fix for bug 966446.
  virtual void ForgetBufferActor() {}

  virtual const char *Name() { return "TextureHost"; }
  virtual void PrintInfo(std::stringstream& aStream, const char* aPrefix);

  /**
   * Indicates whether the TextureHost implementation is backed by an
   * in-memory buffer. The consequence of this is that locking the
   * TextureHost does not contend with locking the texture on the client side.
   */
  virtual bool HasInternalBuffer() const { return false; }

  /**
   * Cast to a TextureHost for each backend.
   */
  virtual TextureHostOGL* AsHostOGL() { return nullptr; }

protected:
  PTextureParent* mActor;
  TextureFlags mFlags;
  RefPtr<CompositableBackendSpecificData> mCompositableBackendData;

  friend class TextureParent;
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
  BufferTextureHost(gfx::SurfaceFormat aFormat,
                    TextureFlags aFlags);

  ~BufferTextureHost();

  virtual uint8_t* GetBuffer() = 0;

  virtual size_t GetBufferSize() = 0;

  virtual void Updated(const nsIntRegion* aRegion = nullptr) MOZ_OVERRIDE;

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
   * GetFormat will be RGB32 (even though mFormat is SurfaceFormat::YUV).
   */
  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE { return mSize; }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE;

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return true; }

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
  ShmemTextureHost(const mozilla::ipc::Shmem& aShmem,
                   gfx::SurfaceFormat aFormat,
                   ISurfaceAllocator* aDeallocator,
                   TextureFlags aFlags);

  ~ShmemTextureHost();

  virtual void DeallocateSharedData() MOZ_OVERRIDE;

  virtual void ForgetSharedData() MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() MOZ_OVERRIDE;

  virtual size_t GetBufferSize() MOZ_OVERRIDE;

  virtual const char *Name() MOZ_OVERRIDE { return "ShmemTextureHost"; }

  virtual void OnShutdown() MOZ_OVERRIDE;

protected:
  mozilla::ipc::Shmem* mShmem;
  RefPtr<ISurfaceAllocator> mDeallocator;
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
  MemoryTextureHost(uint8_t* aBuffer,
                    gfx::SurfaceFormat aFormat,
                    TextureFlags aFlags);

  ~MemoryTextureHost();

  virtual void DeallocateSharedData() MOZ_OVERRIDE;

  virtual void ForgetSharedData() MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() MOZ_OVERRIDE;

  virtual size_t GetBufferSize() MOZ_OVERRIDE;

  virtual const char *Name() MOZ_OVERRIDE { return "MemoryTextureHost"; }

protected:
  uint8_t* mBuffer;
};

/**
 * A TextureHost for shared SurfaceStream
 */
class StreamTextureHost : public TextureHost
{
public:
  StreamTextureHost(TextureFlags aFlags,
                    const SurfaceStreamDescriptor& aDesc);

  virtual ~StreamTextureHost();

  virtual void DeallocateDeviceData() MOZ_OVERRIDE {};

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual bool Lock() MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE;

  virtual NewTextureSource* GetTextureSources() MOZ_OVERRIDE
  {
    return mTextureSource;
  }

  virtual TemporaryRef<gfx::DataSourceSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "StreamTextureHost"; }
#endif

protected:
  Compositor* mCompositor;
  gfx::SurfaceStream* mStream;
  RefPtr<NewTextureSource> mTextureSource;
  RefPtr<DataTextureSource> mDataTextureSource;
};

class MOZ_STACK_CLASS AutoLockTextureHost
{
public:
  AutoLockTextureHost(TextureHost* aTexture)
    : mTexture(aTexture)
  {
    mLocked = mTexture ? mTexture->Lock() : false;
  }

  ~AutoLockTextureHost()
  {
    if (mTexture && mLocked) {
      mTexture->Unlock();
    }
  }

  bool Failed() { return mTexture && !mLocked; }

private:
  RefPtr<TextureHost> mTexture;
  bool mLocked;
};

/**
 * This can be used as an offscreen rendering target by the compositor, and
 * subsequently can be used as a source by the compositor.
 */
class CompositingRenderTarget : public TextureSource
{
public:
  CompositingRenderTarget(const gfx::IntPoint& aOrigin)
    : mOrigin(aOrigin)
  {}
  virtual ~CompositingRenderTarget() {}

#ifdef MOZ_DUMP_PAINTING
  virtual TemporaryRef<gfx::DataSourceSurface> Dump(Compositor* aCompositor) { return nullptr; }
#endif

  const gfx::IntPoint& GetOrigin() { return mOrigin; }

private:
  gfx::IntPoint mOrigin;
};

/**
 * Creates a TextureHost that can be used with any of the existing backends
 * Not all SurfaceDescriptor types are supported
 */
TemporaryRef<TextureHost>
CreateBackendIndependentTextureHost(const SurfaceDescriptor& aDesc,
                                    ISurfaceAllocator* aDeallocator,
                                    TextureFlags aFlags);

}
}

#endif
