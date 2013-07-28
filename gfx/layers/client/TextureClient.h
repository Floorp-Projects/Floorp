/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//  * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURECLIENT_H
#define MOZILLA_GFX_TEXTURECLIENT_H

#include "mozilla/layers/LayersSurfaces.h"
#include "gfxASurface.h"
#include "mozilla/layers/CompositorTypes.h" // for TextureInfo
#include "mozilla/RefPtr.h"

class gfxReusableSurfaceWrapper;

namespace mozilla {

namespace gl {
class GLContext;
}

namespace layers {

class ContentClient;
class PlanarYCbCrImage;
class Image;
class CompositableForwarder;

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
  virtual gfx::DrawTarget* LockDrawTarget() { return nullptr; }

  virtual SurfaceDescriptor* LockSurfaceDescriptor() { return GetDescriptor(); }
  virtual void ReleaseResources() {}
  /**
   * This unlocks the current DrawableTexture and allows the host to composite
   * it directly.
   */
  virtual void Unlock() {}

  /**
   * Ensure that the texture client is suitable for the given size and content
   * type and that any initialisation has taken place.
   */
  virtual void EnsureAllocated(gfx::IntSize aSize,
                               gfxASurface::gfxContentType aType) = 0;

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

  virtual gfxASurface::gfxContentType GetContentType() = 0;

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
  ~DeprecatedTextureClientShmem() { ReleaseResources(); }

  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE
  {
    return aType == TEXTURE_SHMEM || aType == TEXTURE_CONTENT;
  }
  virtual gfxImageSurface* LockImageSurface() MOZ_OVERRIDE;
  virtual gfxASurface* LockSurface() MOZ_OVERRIDE { return GetSurface(); }
  virtual gfx::DrawTarget* LockDrawTarget();
  virtual void Unlock() MOZ_OVERRIDE;
  virtual void EnsureAllocated(gfx::IntSize aSize, gfxASurface::gfxContentType aType) MOZ_OVERRIDE;

  virtual void ReleaseResources() MOZ_OVERRIDE;
  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual gfxASurface::gfxContentType GetContentType() MOZ_OVERRIDE { return mContentType; }
private:
  gfxASurface* GetSurface();

  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<gfxImageSurface> mSurfaceAsImage;
  RefPtr<gfx::DrawTarget> mDrawTarget;

  gfxASurface::gfxContentType mContentType;
  gfx::IntSize mSize;

  friend class CompositingFactory;
};

class DeprecatedTextureClientShmemYCbCr : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientShmemYCbCr(CompositableForwarder* aForwarder, const TextureInfo& aTextureInfo)
    : DeprecatedTextureClient(aForwarder, aTextureInfo)
  { }
  ~DeprecatedTextureClientShmemYCbCr() { ReleaseResources(); }

  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE { return aType == TEXTURE_YCBCR; }
  void EnsureAllocated(gfx::IntSize aSize, gfxASurface::gfxContentType aType) MOZ_OVERRIDE;
  virtual void SetDescriptorFromReply(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual void ReleaseResources();
  virtual gfxASurface::gfxContentType GetContentType() MOZ_OVERRIDE { return gfxASurface::CONTENT_COLOR_ALPHA; }
};

class DeprecatedTextureClientTile : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientTile(const DeprecatedTextureClientTile& aOther);
  DeprecatedTextureClientTile(CompositableForwarder* aForwarder,
                    const TextureInfo& aTextureInfo);
  ~DeprecatedTextureClientTile();

  virtual void EnsureAllocated(gfx::IntSize aSize,
                               gfxASurface::gfxContentType aType) MOZ_OVERRIDE;

  virtual gfxImageSurface* LockImageSurface() MOZ_OVERRIDE;

  gfxReusableSurfaceWrapper* GetReusableSurfaceWrapper()
  {
    return mSurface;
  }

  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE
  {
    MOZ_ASSERT(false, "Tiled texture clients don't use SurfaceDescriptors.");
  }


  virtual gfxASurface::gfxContentType GetContentType() { return mContentType; }

private:
  gfxASurface::gfxContentType mContentType;
  nsRefPtr<gfxReusableSurfaceWrapper> mSurface;

  friend class CompositingFactory;
};

/**
 * Base class for AutoLock*Client.
 * handles lock/unlock
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
