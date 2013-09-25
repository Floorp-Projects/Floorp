/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TEXTURED3D9_H
#define MOZILLA_GFX_TEXTURED3D9_H

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/GfxMessageUtils.h"
#include "gfxWindowsPlatform.h"
#include "d3d9.h"
#include <vector>

namespace mozilla {
namespace layers {

class CompositorD3D9;

class TextureSourceD3D9
{
public:
  virtual IDirect3DTexture9* GetD3D9Texture() { return mTextures[0]; }
  virtual bool IsYCbCrSource() const { return false; }

  struct YCbCrTextures
  {
    IDirect3DTexture9 *mY;
    IDirect3DTexture9 *mCb;
    IDirect3DTexture9 *mCr;
    StereoMode mStereoMode;
  };

  virtual YCbCrTextures GetYCbCrTextures() {
    YCbCrTextures textures = { mTextures[0],
                               mTextures[1],
                               mTextures[2],
                               mStereoMode };
    return textures;
  }

protected:
  virtual gfx::IntSize GetSize() const { return mSize; }
  void SetSize(const gfx::IntSize& aSize) { mSize = aSize; }

  gfx::IntSize mSize;
  StereoMode mStereoMode;
  RefPtr<IDirect3DTexture9> mTextures[3];
};

class CompositingRenderTargetD3D9 : public CompositingRenderTarget,
                                    public TextureSourceD3D9
{
public:
  CompositingRenderTargetD3D9(IDirect3DTexture9* aTexture,
                              SurfaceInitMode aInit,
                              const gfx::IntSize& aSize);
  // use for rendering to the main window, cannot be rendered as a texture
  CompositingRenderTargetD3D9(IDirect3DSurface9* aSurface,
                              SurfaceInitMode aInit,
                              const gfx::IntSize& aSize);
  ~CompositingRenderTargetD3D9();

  virtual TextureSourceD3D9* AsSourceD3D9() MOZ_OVERRIDE
  {
    MOZ_ASSERT(mTextures[0],
               "No texture, can't be indirectly rendered. Is this the screen backbuffer?");
    return this;
  }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  void BindRenderTarget(IDirect3DDevice9* aDevice);

  IDirect3DSurface9* GetD3D9Surface() const { return mSurface; }

private:
  friend class CompositorD3D9;

  nsRefPtr<IDirect3DSurface9> mSurface;
  SurfaceInitMode mInitMode;
  bool mInitialized;
};

// Shared functionality for non-YCbCr texture hosts
class DeprecatedTextureHostD3D9 : public DeprecatedTextureHost
                      , public TextureSourceD3D9
                      , public TileIterator
{
public:
  DeprecatedTextureHostD3D9();
  virtual ~DeprecatedTextureHostD3D9();

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual TextureSourceD3D9* AsSourceD3D9() MOZ_OVERRIDE { return this; }

  virtual IDirect3DTexture9 *GetD3D9Texture() MOZ_OVERRIDE {
    return mIsTiled ? mTileTextures[mCurrentTile].get()
                    : TextureSourceD3D9::GetD3D9Texture();
  }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual LayerRenderState GetRenderState() { return LayerRenderState(); }

  virtual bool Lock() MOZ_OVERRIDE { return true; }

  virtual already_AddRefed<gfxImageSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // TODO: cf bug 872568
  }

  virtual void BeginTileIteration() MOZ_OVERRIDE
  {
    mIterating = true;
    mCurrentTile = 0;
  }
  virtual void EndTileIteration() MOZ_OVERRIDE
  {
    mIterating = false;
  }
  virtual nsIntRect GetTileRect() MOZ_OVERRIDE;
  virtual size_t GetTileCount() MOZ_OVERRIDE { return mTileTextures.size(); }
  virtual bool NextTile() MOZ_OVERRIDE
  {
    return (++mCurrentTile < mTileTextures.size());
  }

  virtual TileIterator* AsTileIterator() MOZ_OVERRIDE
  {
    return mIsTiled ? this : nullptr;
  }

protected:
  gfx::IntRect GetTileRect(uint32_t aID) const;

  RefPtr<IDirect3DDevice9> mDevice;
  RefPtr<CompositorD3D9> mCompositor;
  bool mIsTiled;
  std::vector< RefPtr<IDirect3DTexture9> > mTileTextures;
  uint32_t mCurrentTile;
  bool mIterating;
};

class DeprecatedTextureHostShmemD3D9 : public DeprecatedTextureHostD3D9
{
public:
#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "DeprecatedTextureHostShmemD3D9"; }
#endif

protected:
  virtual void UpdateImpl(const SurfaceDescriptor& aSurface,
                          nsIntRegion* aRegion,
                          nsIntPoint *aOffset = nullptr) MOZ_OVERRIDE;
};

class DeprecatedTextureHostSystemMemD3D9 : public DeprecatedTextureHostD3D9
{
public:
#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "DeprecatedTextureHostSystemMemD3D9"; }
#endif

protected:
  virtual void UpdateImpl(const SurfaceDescriptor& aSurface,
                          nsIntRegion* aRegion,
                          nsIntPoint *aOffset = nullptr) MOZ_OVERRIDE;
};

class DeprecatedTextureHostYCbCrD3D9 : public DeprecatedTextureHost
                           , public TextureSourceD3D9
{
public:
  DeprecatedTextureHostYCbCrD3D9()
    : mDevice(nullptr)
  {
    mFormat = gfx::FORMAT_YUV;
  }

  virtual void SetCompositor(Compositor* aCompositor) MOZ_OVERRIDE;

  virtual TextureSourceD3D9* AsSourceD3D9() MOZ_OVERRIDE { return this; }

  virtual gfx::IntSize GetSize() const MOZ_OVERRIDE;

  virtual bool IsYCbCrSource() const MOZ_OVERRIDE { return true; }

  virtual already_AddRefed<gfxImageSurface> GetAsSurface() MOZ_OVERRIDE
  {
    return nullptr; // TODO: cf bug 872568
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() MOZ_OVERRIDE
  {
    return "TextureImageDeprecatedTextureHostD3D11";
  }
#endif

protected:
  virtual void UpdateImpl(const SurfaceDescriptor& aSurface,
                          nsIntRegion* aRegion,
                          nsIntPoint* aOffset = nullptr) MOZ_OVERRIDE;

private:
  RefPtr<IDirect3DDevice9> mDevice;
};

class DeprecatedTextureHostDIB : public DeprecatedTextureHostD3D9
{
public:
#ifdef MOZ_LAYERS_HAVE_LOG
  virtual const char* Name() { return "DeprecatedTextureHostDIB"; }
#endif
  virtual void SetBuffer(SurfaceDescriptor* aBuffer, ISurfaceAllocator* aAllocator) MOZ_OVERRIDE
  {
    MOZ_ASSERT(aBuffer->type() == SurfaceDescriptor::TSurfaceDescriptorDIB);
    // We are responsible for keeping the surface alive. But the client will have AddRefed it
    // for transport to the host. So we don't need to AddRef here.
    mSurface = dont_AddRef(reinterpret_cast<gfxWindowsSurface*>(aBuffer->get_SurfaceDescriptorDIB().surface()));
    DeprecatedTextureHost::SetBuffer(aBuffer, aAllocator);
  }
  virtual SurfaceDescriptor* LockSurfaceDescriptor() const MOZ_OVERRIDE
  {
    NS_ASSERTION(!mBuffer ||
      (mBuffer->type() == SurfaceDescriptor::TSurfaceDescriptorDIB &&
       mSurface == reinterpret_cast<gfxWindowsSurface*>(mBuffer->get_SurfaceDescriptorDIB().surface())),
                 "SurfaceDescriptor is not up to date");
    // We are either going to pass the surface descriptor to the content thread
    // or we are going to give the our surface descriptor to a compositable host,
    // pretending that it is from the client thread. In either case it is going to
    // get released, so we need to AddRef here.
    if (mBuffer) {
      mSurface->AddRef();
    }
    return DeprecatedTextureHost::LockSurfaceDescriptor();
  }

protected:
  virtual void UpdateImpl(const SurfaceDescriptor& aSurface,
                          nsIntRegion* aRegion,
                          nsIntPoint *aOffset = nullptr) MOZ_OVERRIDE;

  // Used to keep the surface alive if the host has responsibility for the
  // lifetime of the surface.
  nsRefPtr<gfxWindowsSurface> mSurface;
};

// If we want to use d3d9 textures for transport, use this class.
// If we are using shmem, then use DeprecatedTextureClientShmem with DeprecatedTextureHostShmemD3D9
// Since we pass a raw pointer, you should not use this texture client for
// multi-process compositing.
class DeprecatedTextureClientD3D9 : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientD3D9(CompositableForwarder* aCompositableForwarder,
                    const TextureInfo& aTextureInfo);
  virtual ~DeprecatedTextureClientD3D9();

  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE
  {
    return aType == TEXTURE_CONTENT;
  }

  virtual bool EnsureAllocated(gfx::IntSize aSize,
                               gfxContentType aType) MOZ_OVERRIDE;

  virtual gfxASurface* LockSurface() MOZ_OVERRIDE;
  virtual gfx::DrawTarget* LockDrawTarget() MOZ_OVERRIDE;
  virtual gfx::BackendType BackendType() MOZ_OVERRIDE
  {
    return gfx::BACKEND_CAIRO;
  }
  virtual void Unlock() MOZ_OVERRIDE;

  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual gfxContentType GetContentType() MOZ_OVERRIDE
  {
    return mContentType;
  }

private:
  nsRefPtr<IDirect3DTexture9> mTexture;
  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<IDirect3DSurface9> mD3D9Surface;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  gfxContentType mContentType;
};

// Retains a DIB and uses it for transport.
// Used where we can't efficently use a gfxWindowsSurface wrapped around
// a DC from a IDirect3DSurface9, which is for surfaces with alpha.
class DeprecatedTextureClientDIB : public DeprecatedTextureClient
{
public:
  DeprecatedTextureClientDIB(CompositableForwarder* aCompositableForwarder,
                             const TextureInfo& aTextureInfo);
  virtual ~DeprecatedTextureClientDIB();

  virtual bool SupportsType(DeprecatedTextureClientType aType) MOZ_OVERRIDE
  {
    return aType == TEXTURE_CONTENT;
  }

  virtual bool EnsureAllocated(gfx::IntSize aSize,
                               gfxContentType aType) MOZ_OVERRIDE;

  virtual gfxASurface* LockSurface() MOZ_OVERRIDE;
  virtual gfx::DrawTarget* LockDrawTarget() MOZ_OVERRIDE;
  virtual gfx::BackendType BackendType() MOZ_OVERRIDE
  {
    return gfx::BACKEND_CAIRO;
  }
  virtual void Unlock() MOZ_OVERRIDE;

  virtual SurfaceDescriptor* LockSurfaceDescriptor() MOZ_OVERRIDE;
  virtual void SetDescriptor(const SurfaceDescriptor& aDescriptor) MOZ_OVERRIDE;
  virtual gfxContentType GetContentType() MOZ_OVERRIDE
  {
    return mContentType;
  }

private:
  nsRefPtr<gfxWindowsSurface> mSurface;
  RefPtr<gfx::DrawTarget> mDrawTarget;
  gfx::IntSize mSize;
  gfxContentType mContentType;
};

}
}

#endif /* MOZILLA_GFX_TEXTURED3D9_H */
