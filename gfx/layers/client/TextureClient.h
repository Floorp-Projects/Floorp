/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENT_H
#define MOZILLA_GFX_TEXTURECLIENT_H

#include <stddef.h>                     // for size_t
#include <stdint.h>                     // for uint32_t, uint8_t, uint64_t
#include "GLContextTypes.h"             // for GLContext (ptr only), etc
#include "GLTextureImage.h"             // for TextureImage
#include "ImageTypes.h"                 // for StereoMode
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Attributes.h"         // for MOZ_OVERRIDE
#include "mozilla/RefPtr.h"             // for RefPtr, RefCounted
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/gfx/Types.h"          // for SurfaceFormat
#include "mozilla/layers/FenceUtils.h"  // for FenceHandle
#include "mozilla/ipc/Shmem.h"          // for Shmem
#include "mozilla/layers/AtomicRefCountedWithFinalize.h"
#include "mozilla/layers/CompositorTypes.h"  // for TextureFlags, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for TextureImage::AddRef, etc

class gfxReusableSurfaceWrapper;
class gfxImageSurface;

namespace mozilla {
namespace gl {
class GLContext;
class SurfaceStream;
}

namespace layers {

class AsyncTransactionTracker;
class ContentClient;
class CompositableForwarder;
class ISurfaceAllocator;
class CompositableClient;
class PlanarYCbCrImage;
struct PlanarYCbCrData;
class Image;
class PTextureChild;
class TextureChild;
class BufferTextureClient;
class TextureClient;
class KeepAlive;

/**
 * TextureClient is the abstraction that allows us to share data between the
 * content and the compositor side.
 */

enum TextureAllocationFlags {
  ALLOC_DEFAULT = 0,
  ALLOC_CLEAR_BUFFER = 1,
  ALLOC_CLEAR_BUFFER_WHITE = 2
};

/**
 * Interface for TextureClients that can be updated using YCbCr data.
 */
class TextureClientYCbCr
{
public:
  /**
   * Copy aData into this texture client.
   *
   * This must never be called on a TextureClient that is not sucessfully locked.
   */
  virtual bool UpdateYCbCr(const PlanarYCbCrData& aData) = 0;

  /**
   * Allocates for a given surface size, taking into account the pixel format
   * which is part of the state of the TextureClient.
   *
   * Does not clear the surface, since we consider that the surface
   * be painted entirely with opaque content.
   */
  virtual bool AllocateForYCbCr(gfx::IntSize aYSize,
                                gfx::IntSize aCbCrSize,
                                StereoMode aStereoMode) = 0;
};

/**
 * TextureClient is a thin abstraction over texture data that need to be shared
 * between the content process and the compositor process. It is the
 * content-side half of a TextureClient/TextureHost pair. A corresponding
 * TextureHost lives on the compositor-side.
 *
 * TextureClient's primary purpose is to present texture data in a way that is
 * understood by the IPC system. There are two ways to use it:
 * - Use it to serialize image data that is not IPC-friendly (most likely
 * involving a copy into shared memory)
 * - preallocate it and paint directly into it, which avoids copy but requires
 * the painting code to be aware of TextureClient (or at least the underlying
 * shared memory).
 *
 * There is always one and only one TextureClient per TextureHost, and the
 * TextureClient/Host pair only owns one buffer of image data through its
 * lifetime. This means that the lifetime of the underlying shared data
 * matches the lifetime of the TextureClient/Host pair. It also means
 * TextureClient/Host do not implement double buffering, which is the
 * responsibility of the compositable (which would use two Texture pairs).
 * In order to send several different buffers to the compositor side, use
 * several TextureClients.
 */
class TextureClient
  : public AtomicRefCountedWithFinalize<TextureClient>
{
public:
  TextureClient(TextureFlags aFlags = TextureFlags::DEFAULT);
  virtual ~TextureClient();

  // Creates and allocates a TextureClient usable with Moz2D.
  static TemporaryRef<TextureClient>
  CreateForDrawing(ISurfaceAllocator* aAllocator,
                   gfx::SurfaceFormat aFormat,
                   gfx::IntSize aSize,
                   gfx::BackendType aMoz2dBackend,
                   TextureFlags aTextureFlags,
                   TextureAllocationFlags flags = ALLOC_DEFAULT);

  // Creates and allocates a BufferTextureClient supporting the YCbCr format.
  static TemporaryRef<BufferTextureClient>
  CreateForYCbCr(ISurfaceAllocator* aAllocator,
                 gfx::IntSize aYSize,
                 gfx::IntSize aCbCrSize,
                 StereoMode aStereoMode,
                 TextureFlags aTextureFlags);

  // Creates and allocates a BufferTextureClient (can beaccessed through raw
  // pointers).
  static TemporaryRef<BufferTextureClient>
  CreateForRawBufferAccess(ISurfaceAllocator* aAllocator,
                           gfx::SurfaceFormat aFormat,
                           gfx::IntSize aSize,
                           gfx::BackendType aMoz2dBackend,
                           TextureFlags aTextureFlags,
                           TextureAllocationFlags flags = ALLOC_DEFAULT);

  // Creates and allocates a BufferTextureClient (can beaccessed through raw
  // pointers) with a certain buffer size. It's unfortunate that we need this.
  // providing format and sizes could let us do more optimization.
  static TemporaryRef<BufferTextureClient>
  CreateWithBufferSize(ISurfaceAllocator* aAllocator,
                       gfx::SurfaceFormat aFormat,
                       size_t aSize,
                       TextureFlags aTextureFlags);

  virtual TextureClientYCbCr* AsTextureClientYCbCr() { return nullptr; }

  /**
   * Locks the shared data, allowing the caller to get access to it.
   *
   * Please always lock/unlock when accessing the shared data.
   * If Lock() returns false, you should not attempt to access the shared data.
   */
  virtual bool Lock(OpenMode aMode) { return IsValid(); }

  virtual void Unlock() {}

  virtual bool IsLocked() const = 0;

  virtual bool CanExposeDrawTarget() const { return false; }

  /**
   * Returns a DrawTarget to draw into the TextureClient.
   *
   * This must never be called on a TextureClient that is not sucessfully locked.
   * When called several times within one Lock/Unlock pair, this method should
   * return the same DrawTarget.
   * The DrawTarget is automatically flushed by the TextureClient when the latter
   * is unlocked, and the DrawTarget that will be returned within the next
   * lock/unlock pair may or may not be the same object.
   * Do not keep references to the DrawTarget outside of the lock/unlock pair.
   *
   * This is typically used as follows:
   *
   * if (!texture->Lock(OpenMode::OPEN_READ_WRITE)) {
   *   return false;
   * }
   * {
   *   // Restrict this code's scope to ensure all references to dt are gone
   *   // when Unlock is called.
   *   DrawTarget* dt = texture->BorrowDrawTarget();
   *   // use the draw target ...
   * }
   * texture->Unlock();
   *
   */
  virtual gfx::DrawTarget* BorrowDrawTarget() { return nullptr; }

  // TextureClients that can expose a DrawTarget should override this method.
  virtual gfx::SurfaceFormat GetFormat() const
  {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  /**
   * Copies a rectangle from this texture client to a position in aTarget.
   * It is assumed that the necessary locks are in place; so this should at
   * least have a read lock and aTarget should at least have a write lock.
   */
  virtual bool CopyToTextureClient(TextureClient* aTarget,
                                   const gfx::IntRect* aRect,
                                   const gfx::IntPoint* aPoint);

  /**
   * Returns true if this texture has a lock/unlock mechanism.
   * Textures that do not implement locking should be immutable or should
   * use immediate uploads (see TextureFlags in CompositorTypes.h)
   */
  virtual bool ImplementsLocking() const { return false; }

  /**
   * Indicates whether the TextureClient implementation is backed by an
   * in-memory buffer. The consequence of this is that locking the
   * TextureClient does not contend with locking the texture on the host side.
   */
  virtual bool HasInternalBuffer() const = 0;

  /**
   * Allocate and deallocate a TextureChild actor.
   *
   * TextureChild is an implementation detail of TextureClient that is not
   * exposed to the rest of the code base. CreateIPDLActor and DestroyIPDLActor
   * are for use with the managing IPDL protocols only (so that they can
   * implement AllocPextureChild and DeallocPTextureChild).
   */
  static PTextureChild* CreateIPDLActor();
  static bool DestroyIPDLActor(PTextureChild* actor);

  /**
   * Get the TextureClient corresponding to the actor passed in parameter.
   */
  static TextureClient* AsTextureClient(PTextureChild* actor);

  virtual bool IsAllocated() const = 0;

  virtual gfx::IntSize GetSize() const = 0;

  /**
   * TextureFlags contain important information about various aspects
   * of the texture, like how its liferime is managed, and how it
   * should be displayed.
   * See TextureFlags in CompositorTypes.h.
   */
  TextureFlags GetFlags() const { return mFlags; }

  /**
   * valid only for TextureFlags::RECYCLE TextureClient.
   * When called this texture client will grab a strong reference and release
   * it once the compositor notifies that it is done with the texture.
   * NOTE: In this stage the texture client can no longer be used by the
   * client in a transaction.
   */
  void WaitForCompositorRecycle();

  /**
   * After being shared with the compositor side, an immutable texture is never
   * modified, it can only be read. It is safe to not Lock/Unlock immutable
   * textures.
   */
  bool IsImmutable() const { return !!(mFlags & TextureFlags::IMMUTABLE); }

  void MarkImmutable() { AddFlags(TextureFlags::IMMUTABLE); }

  bool IsSharedWithCompositor() const { return mShared; }

  bool ShouldDeallocateInDestructor() const;

  /**
   * If this method returns false users of TextureClient are not allowed
   * to access the shared data.
   */
  bool IsValid() const { return mValid; }

  /**
   * kee the passed object alive until the IPDL actor is destroyed. This can
   * help avoid race conditions in some cases.
   * It's a temporary hack to ensure that DXGI textures don't get destroyed
   * between serialization and deserialization.
   */
  void KeepUntilFullDeallocation(KeepAlive* aKeep);

  /**
   * Create and init the TextureChild/Parent IPDL actor pair.
   *
   * Should be called only once per TextureClient.
   */
  bool InitIPDLActor(CompositableForwarder* aForwarder);

  /**
   * Return a pointer to the IPDLActor.
   *
   * This is to be used with IPDL messages only. Do not store the returned
   * pointer.
   */
  PTextureChild* GetIPDLActor();

  /**
   * Triggers the destruction of the shared data and the corresponding TextureHost.
   *
   * If the texture flags contain TextureFlags::DEALLOCATE_CLIENT, the destruction
   * will be synchronously coordinated with the compositor side, otherwise it
   * will be done asynchronously.
   */
  void ForceRemove();

  virtual void SetReleaseFenceHandle(FenceHandle aReleaseFenceHandle)
  {
    mReleaseFenceHandle = aReleaseFenceHandle;
  }

  const FenceHandle& GetReleaseFenceHandle() const
  {
    return mReleaseFenceHandle;
  }

  virtual void SetAcquireFenceHandle(FenceHandle aAcquireFenceHandle)
  {
    mAcquireFenceHandle = aAcquireFenceHandle;
  }

  const FenceHandle& GetAcquireFenceHandle() const
  {
    return mAcquireFenceHandle;
  }

  /**
   * Set AsyncTransactionTracker of RemoveTextureFromCompositableAsync() transaction.
   */
  virtual void SetRemoveFromCompositableTracker(AsyncTransactionTracker* aTracker) {}

  /**
   * This function waits until the buffer is no longer being used.
   */
  virtual void WaitForBufferOwnership() {}

private:
  /**
   * Called once, just before the destructor.
   *
   * Here goes the shut-down code that uses virtual methods.
   * Must only be called by Release().
   */
  void Finalize();

  friend class AtomicRefCountedWithFinalize<TextureClient>;

protected:
  /**
   * An invalid TextureClient cannot provide access to its shared data
   * anymore. This usually means it will soon be destroyed.
   */
  void MarkInvalid() { mValid = false; }

  /**
   * Allocates for a given surface size, taking into account the pixel format
   * which is part of the state of the TextureClient.
   *
   * Does not clear the surface by default, clearing the surface can be done
   * by passing the CLEAR_BUFFER flag.
   *
   * TextureClients that can expose a DrawTarget should override this method.
   */
  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags flags = ALLOC_DEFAULT)
  {
    return false;
  }


  /**
   * Should only be called *once* per texture, in TextureClient::InitIPDLActor.
   * Some texture implementations rely on the fact that the descriptor will be
   * deserialized.
   * Calling ToSurfaceDescriptor again after it has already returned true,
   * or never constructing a TextureHost with aDescriptor may result in a memory
   * leak (see CairoTextureClientD3D9 for example).
   */
  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor) = 0;

  void AddFlags(TextureFlags  aFlags)
  {
    MOZ_ASSERT(!IsSharedWithCompositor());
    mFlags |= aFlags;
  }

  ISurfaceAllocator* GetAllocator()
  {
    return mAllocator;
  }

  RefPtr<TextureChild> mActor;
  RefPtr<ISurfaceAllocator> mAllocator;
  TextureFlags mFlags;
  bool mShared;
  bool mValid;
  FenceHandle mReleaseFenceHandle;
  FenceHandle mAcquireFenceHandle;

  friend class TextureChild;
  friend class RemoveTextureFromCompositableTracker;
  friend void TestTextureClientSurface(TextureClient*, gfxImageSurface*);
  friend void TestTextureClientYCbCr(TextureClient*, PlanarYCbCrData&);
};

/**
 * Task that releases TextureClient pointer on a specified thread.
 */
class TextureClientReleaseTask : public Task
{
public:
    TextureClientReleaseTask(TextureClient* aClient)
        : mTextureClient(aClient) {
    }

    virtual void Run() MOZ_OVERRIDE
    {
        mTextureClient = nullptr;
    }

private:
    mozilla::RefPtr<TextureClient> mTextureClient;
};

/**
 * TextureClient that wraps a random access buffer such as a Shmem or raw memory.
 * This class must be inherited to implement the memory allocation and access bits.
 * (see ShmemTextureClient and MemoryTextureClient)
 */
class BufferTextureClient : public TextureClient
                          , public TextureClientYCbCr
{
public:
  BufferTextureClient(ISurfaceAllocator* aAllocator, gfx::SurfaceFormat aFormat,
                      gfx::BackendType aBackend, TextureFlags aFlags);

  virtual ~BufferTextureClient();

  virtual bool IsAllocated() const = 0;

  virtual uint8_t* GetBuffer() const = 0;

  virtual gfx::IntSize GetSize() const { return mSize; }

  virtual bool Lock(OpenMode aMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mLocked; }

  virtual bool CanExposeDrawTarget() const MOZ_OVERRIDE { return true; }

  virtual gfx::DrawTarget* BorrowDrawTarget() MOZ_OVERRIDE;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) MOZ_OVERRIDE;

  // TextureClientYCbCr

  virtual TextureClientYCbCr* AsTextureClientYCbCr() MOZ_OVERRIDE { return this; }

  virtual bool UpdateYCbCr(const PlanarYCbCrData& aData) MOZ_OVERRIDE;

  virtual bool AllocateForYCbCr(gfx::IntSize aYSize,
                                gfx::IntSize aCbCrSize,
                                StereoMode aStereoMode) MOZ_OVERRIDE;

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE { return mFormat; }

  // XXX - Bug 908196 - Make Allocate(uint32_t) and GetBufferSize() protected.
  // these two methods should only be called by methods of BufferTextureClient
  // that are overridden in GrallocTextureClient (which does not implement the
  // two methods below)
  virtual bool Allocate(uint32_t aSize) = 0;

  virtual size_t GetBufferSize() const = 0;

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return true; }

  ISurfaceAllocator* GetAllocator() const;

protected:
  RefPtr<gfx::DrawTarget> mDrawTarget;
  RefPtr<ISurfaceAllocator> mAllocator;
  gfx::SurfaceFormat mFormat;
  gfx::IntSize mSize;
  gfx::BackendType mBackend;
  OpenMode mOpenMode;
  bool mLocked;
};

/**
 * TextureClient that wraps shared memory.
 * the corresponding texture on the host side is ShmemTextureHost.
 */
class ShmemTextureClient : public BufferTextureClient
{
public:
  ShmemTextureClient(ISurfaceAllocator* aAllocator, gfx::SurfaceFormat aFormat,
                     gfx::BackendType aBackend, TextureFlags aFlags);

protected:
  ~ShmemTextureClient();

public:
  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;

  virtual bool Allocate(uint32_t aSize) MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() const MOZ_OVERRIDE;

  virtual size_t GetBufferSize() const MOZ_OVERRIDE;

  virtual bool IsAllocated() const MOZ_OVERRIDE { return mAllocated; }

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return true; }

  mozilla::ipc::Shmem& GetShmem() { return mShmem; }

protected:
  mozilla::ipc::Shmem mShmem;
  bool mAllocated;
};

/**
 * TextureClient that wraps raw memory.
 * The corresponding texture on the host side is MemoryTextureHost.
 * Can obviously not be used in a cross process setup.
 */
class MemoryTextureClient : public BufferTextureClient
{
public:
  MemoryTextureClient(ISurfaceAllocator* aAllocator, gfx::SurfaceFormat aFormat,
                      gfx::BackendType aBackend, TextureFlags aFlags);

protected:
  ~MemoryTextureClient();

public:
  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;

  virtual bool Allocate(uint32_t aSize) MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() const MOZ_OVERRIDE { return mBuffer; }

  virtual size_t GetBufferSize() const MOZ_OVERRIDE { return mBufSize; }

  virtual bool IsAllocated() const MOZ_OVERRIDE { return mBuffer != nullptr; }

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return true; }

protected:
  uint8_t* mBuffer;
  size_t mBufSize;
};

/**
 * A TextureClient implementation to share SurfaceStream.
 */
class StreamTextureClient : public TextureClient
{
public:
  StreamTextureClient(TextureFlags aFlags);

protected:
  ~StreamTextureClient();

public:
  virtual bool IsAllocated() const MOZ_OVERRIDE;

  virtual bool Lock(OpenMode mode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mIsLocked; }

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aOutDescriptor) MOZ_OVERRIDE;

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return false; }

  void InitWith(gl::SurfaceStream* aStream);

  virtual gfx::IntSize GetSize() const { return gfx::IntSize(); }

  virtual gfx::SurfaceFormat GetFormat() const MOZ_OVERRIDE
  {
    return gfx::SurfaceFormat::UNKNOWN;
  }

  virtual bool AllocateForSurface(gfx::IntSize aSize, TextureAllocationFlags aFlags) MOZ_OVERRIDE
  {
    MOZ_CRASH("Should never hit this.");
    return false;
  }

protected:
  bool mIsLocked;
  RefPtr<gl::SurfaceStream> mStream;
  RefPtr<gl::GLContext> mGL; // Just for reference holding.
};

struct TextureClientAutoUnlock
{
  TextureClient* mTexture;

  TextureClientAutoUnlock(TextureClient* aTexture)
  : mTexture(aTexture) {}

  ~TextureClientAutoUnlock()
  {
    mTexture->Unlock();
  }
};

class KeepAlive
{
public:
  virtual ~KeepAlive() {}
};

template<typename T>
class TKeepAlive : public KeepAlive
{
public:
  TKeepAlive(T* aData) : mData(aData) {}
protected:
  RefPtr<T> mData;
};

}
}
#endif
