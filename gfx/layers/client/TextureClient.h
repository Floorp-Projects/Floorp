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
#include "mozilla/layers/PTextureChild.h" // for PTextureChild
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for TextureImage::AddRef, etc

class gfxReusableSurfaceWrapper;
class gfxASurface;
class gfxImageSurface;

namespace mozilla {
namespace layers {

class ContentClient;
class CompositableForwarder;
class ISurfaceAllocator;
class CompositableClient;
class PlanarYCbCrImage;
class PlanarYCbCrData;
class Image;
class PTextureChild;
class TextureChild;
class BufferTextureClient;

/**
 * TextureClient is the abstraction that allows us to share data between the
 * content and the compositor side.
 * TextureClient can also provide with some more "producer" facing APIs
 * such as TextureClientSurface and TextureClientYCbCr, that can be queried
 * using AsTextureCLientSurface(), etc.
 */

enum TextureAllocationFlags {
  ALLOC_DEFAULT = 0,
  ALLOC_CLEAR_BUFFER = 1
};

/**
 * Interface for TextureClients that can be updated using a gfxASurface.
 */
class TextureClientSurface
{
public:
  virtual bool UpdateSurface(gfxASurface* aSurface) = 0;
  virtual already_AddRefed<gfxASurface> GetAsSurface() = 0;
  /**
   * Allocates for a given surface size, taking into account the pixel format
   * which is part of the state of the TextureClient.
   *
   * Does not clear the surface by default, clearing the surface can be done
   * by passing the CLEAR_BUFFER flag.
   */
  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags flags = ALLOC_DEFAULT) = 0;
};

/**
 * Interface for TextureClients that can be updated using a DrawTarget.
 */
class TextureClientDrawTarget
{
public:
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
   * if (!texture->Lock(OPEN_READ_WRITE)) {
   *   return false;
   * }
   * {
   *   // Restrict this code's scope to ensure all references to dt are gone
   *   // when Unlock is called.
   *   RefPtr<DrawTarget> dt = texture->AsTextureClientDrawTarget()->GetAsDrawTarget();
   *   // use the draw target ...
   * }
   * texture->Unlock();
   *
   */
  virtual TemporaryRef<gfx::DrawTarget> GetAsDrawTarget() = 0;
  virtual gfx::SurfaceFormat GetFormat() const = 0;
  /**
   * Allocates for a given surface size, taking into account the pixel format
   * which is part of the state of the TextureClient.
   *
   * Does not clear the surface by default, clearing the surface can be done
   * by passing the CLEAR_BUFFER flag.
   */
  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags flags = ALLOC_DEFAULT) = 0;
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
 * Holds the shared data of a TextureClient, to be destroyed later.
 *
 * TextureClient's destructor initiates the destruction sequence of the
 * texture client/host pair. If the shared data is to be deallocated on the
 * host side, there is nothing to do.
 * On the other hand, if the client data must be deallocated on the client
 * side, the CompositableClient will ask the TextureClient to drop its shared
 * data in the form of a TextureClientData object. This data will be kept alive
 * until the host side confirms that it is not using the data anymore and that
 * it is completely safe to deallocate the shared data.
 *
 * See:
 *  - The PTexture IPDL protocol
 *  - CompositableChild in TextureClient.cpp
 */
class TextureClientData {
public:
  virtual void DeallocateSharedData(ISurfaceAllocator* allocator) = 0;
  virtual ~TextureClientData() {}
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
  TextureClient(TextureFlags aFlags = TEXTURE_FLAGS_DEFAULT);
  virtual ~TextureClient();

  static TemporaryRef<BufferTextureClient>
  CreateBufferTextureClient(ISurfaceAllocator* aAllocator,
                            gfx::SurfaceFormat aFormat,
                            TextureFlags aTextureFlags,
                            gfx::BackendType aMoz2dBackend);

  static TemporaryRef<TextureClient>
  CreateTextureClientForDrawing(ISurfaceAllocator* aAllocator,
                                gfx::SurfaceFormat aFormat,
                                TextureFlags aTextureFlags,
                                gfx::BackendType aMoz2dBackend,
                                const gfx::IntSize& aSizeHint);

  virtual TextureClientSurface* AsTextureClientSurface() { return nullptr; }
  virtual TextureClientDrawTarget* AsTextureClientDrawTarget() { return nullptr; }
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
   * TextureChild is an implementation detail of TextureHost that is not
   * exposed to the rest of the code base. CreateIPDLActor and DestroyIPDLActor
   * are for use with the maging IPDL protocols only (so that they can
   * implement AllocPextureChild and DeallocPTextureChild).
   */
  static PTextureChild* CreateIPDLActor();
  static bool DestroyIPDLActor(PTextureChild* actor);

  /**
   * Get the TextureClient corresponding to the actor passed in parameter.
   */
  static TextureClient* AsTextureClient(PTextureChild* actor);

  virtual bool IsAllocated() const = 0;

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor) = 0;

  virtual gfx::IntSize GetSize() const = 0;

  /**
   * TextureFlags contain important information about various aspects
   * of the texture, like how its liferime is managed, and how it
   * should be displayed.
   * See TextureFlags in CompositorTypes.h.
   */
  TextureFlags GetFlags() const { return mFlags; }

  /**
   * valid only for TEXTURE_RECYCLE TextureClient.
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
  bool IsImmutable() const { return mFlags & TEXTURE_IMMUTABLE; }

  void MarkImmutable() { AddFlags(TEXTURE_IMMUTABLE); }

  bool IsSharedWithCompositor() const { return mShared; }

  bool ShouldDeallocateInDestructor() const;

  /**
   * If this method returns false users of TextureClient are not allowed
   * to access the shared data.
   */
  bool IsValid() const { return mValid; }

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
   * If the texture flags contain TEXTURE_DEALLOCATE_CLIENT, the destruction
   * will be synchronously coordinated with the compositor side, otherwise it
   * will be done asynchronously.
   */
  void ForceRemove();

  virtual void SetReleaseFenceHandle(FenceHandle aReleaseFenceHandle) {}

  const FenceHandle& GetReleaseFenceHandle() const
  {
    return mReleaseFenceHandle;
  }

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
   * Drop the shared data into a TextureClientData object and mark this
   * TextureClient as invalid.
   *
   * The TextureClient must not hold any reference to the shared data
   * after this method has been called.
   * The TextureClientData is owned by the caller.
   */
  virtual TextureClientData* DropTextureData() = 0;

  void AddFlags(TextureFlags  aFlags)
  {
    MOZ_ASSERT(!IsSharedWithCompositor());
    mFlags |= aFlags;
  }

  RefPtr<TextureChild> mActor;
  TextureFlags mFlags;
  bool mShared;
  bool mValid;
  FenceHandle mReleaseFenceHandle;

  friend class TextureChild;
};

/**
 * TextureClient that wraps a random access buffer such as a Shmem or raw memory.
 * This class must be inherited to implement the memory allocation and access bits.
 * (see ShmemTextureClient and MemoryTextureClient)
 */
class BufferTextureClient : public TextureClient
                          , public TextureClientSurface
                          , public TextureClientYCbCr
                          , public TextureClientDrawTarget
{
public:
  BufferTextureClient(ISurfaceAllocator* aAllocator, gfx::SurfaceFormat aFormat,
                      gfx::BackendType aBackend, TextureFlags aFlags);

  virtual ~BufferTextureClient();

  virtual bool IsAllocated() const = 0;

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor) = 0;

  virtual uint8_t* GetBuffer() const = 0;

  virtual gfx::IntSize GetSize() const { return mSize; }

  virtual bool Lock(OpenMode aMode) MOZ_OVERRIDE;

  virtual void Unlock() MOZ_OVERRIDE;

  virtual bool IsLocked() const MOZ_OVERRIDE { return mLocked; }

  // TextureClientSurface

  virtual TextureClientSurface* AsTextureClientSurface() MOZ_OVERRIDE { return this; }

  virtual bool UpdateSurface(gfxASurface* aSurface) MOZ_OVERRIDE;

  virtual already_AddRefed<gfxASurface> GetAsSurface() MOZ_OVERRIDE;

  virtual bool AllocateForSurface(gfx::IntSize aSize,
                                  TextureAllocationFlags aFlags = ALLOC_DEFAULT) MOZ_OVERRIDE;

  // TextureClientDrawTarget

  virtual TextureClientDrawTarget* AsTextureClientDrawTarget() MOZ_OVERRIDE { return this; }

  virtual TemporaryRef<gfx::DrawTarget> GetAsDrawTarget() MOZ_OVERRIDE;

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
  bool mUsingFallbackDrawTarget;
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

  ~ShmemTextureClient();

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;

  virtual bool Allocate(uint32_t aSize) MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() const MOZ_OVERRIDE;

  virtual size_t GetBufferSize() const MOZ_OVERRIDE;

  virtual bool IsAllocated() const MOZ_OVERRIDE { return mAllocated; }

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE;

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

  ~MemoryTextureClient();

  virtual bool ToSurfaceDescriptor(SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;

  virtual bool Allocate(uint32_t aSize) MOZ_OVERRIDE;

  virtual uint8_t* GetBuffer() const MOZ_OVERRIDE { return mBuffer; }

  virtual size_t GetBufferSize() const MOZ_OVERRIDE { return mBufSize; }

  virtual bool IsAllocated() const MOZ_OVERRIDE { return mBuffer != nullptr; }

  virtual bool HasInternalBuffer() const MOZ_OVERRIDE { return true; }

  virtual TextureClientData* DropTextureData() MOZ_OVERRIDE;

protected:
  uint8_t* mBuffer;
  size_t mBufSize;
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

/**
 * XXX - This class is deprecated, will be removed soon.
 *
 * This class allows texture clients to draw into textures through Azure or
 * thebes and applies locking semantics to allow GPU or CPU level
 * synchronization.
 * DeprecatedTextureClient's purpose is for the texture data to be
 * forwarded to the right place on the compositor side and with correct locking
 * semantics.
 *
 * When modifying a DeprecatedTextureClient's data, first call LockDescriptor, modify the
 * data in the descriptor, and then call Unlock. This makes sure that if the
 * data is shared with the compositor, the later will not try to read while the
 * data is being modified (on the other side, DeprecatedTextureHost also has Lock/Unlock
 * semantics).
 * After unlocking, call Updated in order to add the modification to the current
 * layer transaction.
 * Depending on whether the data is shared or copied, Lock/Unlock and Updated
 * can be no-ops. What's important is that the Client/Host pair implement the
 * same semantics.
 *
 * Ownership of the surface descriptor depends on how the DeprecatedTextureClient/Host is
 * used by the CompositableClient/Host.
 */
class DeprecatedTextureClient : public RefCounted<DeprecatedTextureClient>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(DeprecatedTextureClient)
  typedef gl::SharedTextureHandle SharedTextureHandle;
  typedef gl::GLContext GLContext;
  typedef gl::TextureImage TextureImage;

  virtual ~DeprecatedTextureClient();

  /* This will return an identifier that can be sent accross a process or
   * thread boundary and used to construct a DeprecatedTextureHost object
   * which can then be used as a texture for rendering by a compatible
   * compositor. This texture should have been created with the
   * DeprecatedTextureHostIdentifier specified by the compositor that this identifier
   * is to be used with.
   */
  virtual const TextureInfo& GetTextureInfo() const
  {
    return mTextureInfo;
  }

  virtual bool SupportsType(DeprecatedTextureClientType aType) { return false; }

  /**
   * The Lock* methods lock the texture client for drawing into, providing some
   * object that can be used for drawing to. Once the user is finished
   * with the object it should call Unlock.
   */
  virtual gfxImageSurface* LockImageSurface() { return nullptr; }
  virtual gfxASurface* LockSurface() { return nullptr; }
  // If you implement LockDrawTarget, you MUST implement BackendType()
  virtual gfx::DrawTarget* LockDrawTarget() { return nullptr; }

  // The type of draw target returned by LockDrawTarget.
  virtual gfx::BackendType BackendType()
  {
    return gfx::BackendType::NONE;
  }

  virtual void ReleaseResources() {}
  /**
   * This unlocks the current DrawableTexture and allows the host to composite
   * it directly.
   */
  virtual void Unlock() {}

  /**
   * Ensure that the texture client is suitable for the given size and content
   * type and that any initialisation has taken place.
   * Returns true if succeeded, false if failed.
   */
  virtual bool EnsureAllocated(gfx::IntSize aSize,
                               gfxContentType aType) = 0;

  /**
   * _Only_ used at the end of the layer transaction when receiving a reply from
   *  the compositor.
   */
  virtual void SetDescriptorFromReply(const SurfaceDescriptor& aDescriptor)
  {
    // default implementation
    SetDescriptor(aDescriptor);
  }
  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor)
  {
    mDescriptor = aDescriptor;
  }
  SurfaceDescriptor* GetDescriptor() { return &mDescriptor; }
  /**
   * Use LockSurfaceDescriptor to get the descriptor if it will be sent across IPC.
   * Use GetDescriptor if you want to keep the descriptor on one thread.
   */
  virtual SurfaceDescriptor* LockSurfaceDescriptor() { return GetDescriptor(); }

  CompositableForwarder* GetForwarder() const
  {
    return mForwarder;
  }

  void SetFlags(TextureFlags aFlags)
  {
    mTextureInfo.mTextureFlags = aFlags;
  }

  enum AccessMode
  {
    ACCESS_NONE = 0x0,
    ACCESS_READ_ONLY  = 0x1,
    ACCESS_READ_WRITE = 0x2
  };

  void SetAccessMode(AccessMode aAccessMode)
  {
    mAccessMode = aAccessMode;
  }

  AccessMode GetAccessMode() const
  {
    return mAccessMode;
  }

  virtual gfxContentType GetContentType() = 0;

protected:
  DeprecatedTextureClient(CompositableForwarder* aForwarder,
                const TextureInfo& aTextureInfo);

  CompositableForwarder* mForwarder;
  // So far all DeprecatedTextureClients use a SurfaceDescriptor, so it makes sense to
  // keep the reference here.
  SurfaceDescriptor mDescriptor;
  TextureInfo mTextureInfo;
  AccessMode mAccessMode;
};

class DeprecatedTextureClientShmem : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientShmem(CompositableForwarder* aForwarder, const TextureInfo& aTextureInfo);
  ~DeprecatedTextureClientShmem();
  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE
  {
    return aType == TEXTURE_SHMEM || aType == TEXTURE_CONTENT || aType == TEXTURE_FALLBACK;
  }
  virtual gfxImageSurface* LockImageSurface() MOZ_OVERRIDE;
  virtual gfxASurface* LockSurface() MOZ_OVERRIDE { return GetSurface(); }
  virtual gfx::DrawTarget* LockDrawTarget();
  virtual gfx::BackendType BackendType() MOZ_OVERRIDE
  {
    return gfx::BackendType::CAIRO;
  }
  virtual void Unlock() MOZ_OVERRIDE;
  virtual bool EnsureAllocated(gfx::IntSize aSize, gfxContentType aType) MOZ_OVERRIDE;

  virtual void ReleaseResources() MOZ_OVERRIDE;
  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual gfxContentType GetContentType() MOZ_OVERRIDE { return mContentType; }
private:
  gfxASurface* GetSurface();

  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<gfxImageSurface> mSurfaceAsImage;
  RefPtr<gfx::DrawTarget> mDrawTarget;

  gfxContentType mContentType;
  gfx::IntSize mSize;

  friend class CompositingFactory;
};

// XXX - this class can be removed as soon as we remove DeprecatedImageClientSingle
class DeprecatedTextureClientShmemYCbCr : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientShmemYCbCr(CompositableForwarder* aForwarder, const TextureInfo& aTextureInfo)
    : DeprecatedTextureClient(aForwarder, aTextureInfo)
  { }
  ~DeprecatedTextureClientShmemYCbCr() { ReleaseResources(); }

  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE { return aType == TEXTURE_YCBCR; }
  bool EnsureAllocated(gfx::IntSize aSize, gfxContentType aType) MOZ_OVERRIDE;
  virtual void SetDescriptorFromReply(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual void ReleaseResources();
  virtual gfxContentType GetContentType() MOZ_OVERRIDE { return gfxContentType::COLOR_ALPHA; }
};

class DeprecatedTextureClientTile : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientTile(const DeprecatedTextureClientTile& aOther);
  DeprecatedTextureClientTile(CompositableForwarder* aForwarder,
                              const TextureInfo& aTextureInfo,
                              gfxReusableSurfaceWrapper* aSurface = nullptr);
  ~DeprecatedTextureClientTile();

  virtual bool EnsureAllocated(gfx::IntSize aSize,
                               gfxContentType aType) MOZ_OVERRIDE;

  virtual gfxImageSurface* LockImageSurface() MOZ_OVERRIDE;

  gfxReusableSurfaceWrapper* GetReusableSurfaceWrapper()
  {
    return mSurface;
  }

  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE
  {
    MOZ_ASSERT(false, "Tiled texture clients don't use SurfaceDescriptors.");
  }


  virtual gfxContentType GetContentType() { return mContentType; }

private:
  gfxContentType mContentType;
  nsRefPtr<gfxReusableSurfaceWrapper> mSurface;

  friend class CompositingFactory;
};

/**
 * Base class for AutoLock*Client.
 * handles lock/unlock
 * XXX - this can be removed as soon as we remove DeprecatedImageClientSingle
 */
class AutoLockDeprecatedTextureClient
{
public:
  AutoLockDeprecatedTextureClient(DeprecatedTextureClient* aTexture)
  {
    mDeprecatedTextureClient = aTexture;
    mDescriptor = aTexture->LockSurfaceDescriptor();
  }

  SurfaceDescriptor* GetSurfaceDescriptor()
  {
    return mDescriptor;
  }

  virtual ~AutoLockDeprecatedTextureClient()
  {
    mDeprecatedTextureClient->Unlock();
  }
protected:
  DeprecatedTextureClient* mDeprecatedTextureClient;
  SurfaceDescriptor* mDescriptor;
};

/**
 * Writes the content of a PlanarYCbCrImage into a SurfaceDescriptor.
 * XXX - this can be removed as soon as we remove DeprecatedImageClientSingle
 */
class AutoLockYCbCrClient : public AutoLockDeprecatedTextureClient
{
public:
  AutoLockYCbCrClient(DeprecatedTextureClient* aTexture) : AutoLockDeprecatedTextureClient(aTexture) {}
  bool Update(PlanarYCbCrImage* aImage);
protected:
  bool EnsureDeprecatedTextureClient(PlanarYCbCrImage* aImage);
};

/**
 * Writes the content of a gfxASurface into a SurfaceDescriptor.
 * XXX - this can be removed as soon as we remove DeprecatedImageClientSingle
 */
class AutoLockShmemClient : public AutoLockDeprecatedTextureClient
{
public:
  AutoLockShmemClient(DeprecatedTextureClient* aTexture) : AutoLockDeprecatedTextureClient(aTexture) {}
  bool Update(Image* aImage, uint32_t aContentFlags, gfxASurface *aSurface);
};

}
}
#endif
