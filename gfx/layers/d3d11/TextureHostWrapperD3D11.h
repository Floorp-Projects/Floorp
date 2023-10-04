/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_TextureHostWrapperD3D11_H
#define MOZILLA_GFX_TextureHostWrapperD3D11_H

#include <deque>
#include <unordered_map>

#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/TextureHost.h"
#include "mozilla/UniquePtr.h"

class nsIThreadPool;
struct ID3D11Texture2D;

namespace mozilla {
namespace layers {

class DXGITextureHostD3D11;

/**
 * A Class that allocates and recycles ID3D11Texture2D in TextureUpdate thread.
 * And manages in use TextureHostWrapperD3D11s in compositor thread.
 */
class TextureWrapperD3D11Allocator {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(TextureWrapperD3D11Allocator)

  TextureWrapperD3D11Allocator();

  RefPtr<ID3D11Texture2D> CreateOrRecycle(gfx::SurfaceFormat aSurfaceFormat,
                                          gfx::IntSize aSize);

  void EnsureStagingTextureNV12(RefPtr<ID3D11Device> aDevice);

  RefPtr<ID3D11Texture2D> GetStagingTextureNV12();

  RefPtr<ID3D11Device> GetDevice();

  void RecycleTexture(RefPtr<ID3D11Texture2D>& aTexture);

  void RegisterTextureHostWrapper(const wr::ExternalImageId& aExternalImageId,
                                  RefPtr<TextureHost> aTextureHost);
  void UnregisterTextureHostWrapper(
      const wr::ExternalImageId& aExternalImageId);

  RefPtr<TextureHost> GetTextureHostWrapper(
      const wr::ExternalImageId& aExternalImageId);

  // Holds TextureUpdate thread.
  // Use the SharedThreadPool to create an nsIThreadPool with a maximum of one
  // thread, which is equivalent to an nsIThread for our purposes.
  const nsCOMPtr<nsIThreadPool> mThread;

 protected:
  ~TextureWrapperD3D11Allocator();

  void ClearAllTextures(const MutexAutoLock& aProofOfLock);

  // Holds TextureHostWrapperD3D11s that are in use. TextureHostWrapperD3D11 is
  // wrapped by WebRenderTextureHost. Accessed only from compositor thread
  std::unordered_map<uint64_t, RefPtr<TextureHost>> mTextureHostWrappers;

  // Accessed only from TextureUpdate thread
  RefPtr<ID3D11Texture2D> mStagingTexture;

  Mutex mMutex;

  // Protected by mMutex

  RefPtr<ID3D11Device> mDevice;
  gfx::IntSize mSize;
  std::deque<RefPtr<ID3D11Texture2D>> mRecycledTextures;
};

/**
 * A TextureHost that exposes DXGITextureHostD3D11 as wrapping YUV
 * BufferTextureHost. The DXGITextureHostD3D11 holds GpuProcessTextureId of
 * ID3D11Texture2D. The ID3D11Texture2D is allocated and data updated in
 * TextureUpdate thread.
 */
class TextureHostWrapperD3D11 : public TextureHost {
 public:
  static RefPtr<TextureHost> CreateFromBufferTexture(
      const RefPtr<TextureWrapperD3D11Allocator>& aAllocator,
      TextureHost* aTextureHost);

  void DeallocateDeviceData() override {}

  gfx::SurfaceFormat GetFormat() const override;

  already_AddRefed<gfx::DataSourceSurface> GetAsSurface() override {
    return nullptr;  // XXX - implement this (for MOZ_DUMP_PAINTING)
  }

  gfx::ColorRange GetColorRange() const override;

  gfx::IntSize GetSize() const override;

  bool IsValid() override;

#ifdef MOZ_LAYERS_HAVE_LOG
  const char* Name() override { return "TextureHostWrapperD3D11"; }
#endif

  void CreateRenderTexture(
      const wr::ExternalImageId& aExternalImageId) override;

  void MaybeDestroyRenderTexture() override;

  uint32_t NumSubTextures() override;

  void PushResourceUpdates(wr::TransactionBuilder& aResources,
                           ResourceUpdateOp aOp,
                           const Range<wr::ImageKey>& aImageKeys,
                           const wr::ExternalImageId& aExtID) override;

  void PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                        const wr::LayoutRect& aBounds,
                        const wr::LayoutRect& aClip, wr::ImageRendering aFilter,
                        const Range<wr::ImageKey>& aImageKeys,
                        PushDisplayItemFlagSet aFlags) override;

  bool SupportsExternalCompositing(WebRenderBackend aBackend) override;

  void UnbindTextureSource() override;

  void NotifyNotUsed() override;

  BufferTextureHost* AsBufferTextureHost() override;

  bool IsWrappingSurfaceTextureHost() override;

  TextureHostType GetTextureHostType() override;

  bool NeedsDeferredDeletion() const override;

  TextureHostWrapperD3D11* AsTextureHostWrapperD3D11() override { return this; }

  void PostTask();

  bool UpdateTextureData();

 protected:
  TextureHostWrapperD3D11(
      TextureFlags aFlags,
      const RefPtr<TextureWrapperD3D11Allocator>& aAllocator,
      const GpuProcessTextureId aTextureId,
      DXGITextureHostD3D11* aTextureHostD3D11, TextureHost* aWrappedTextureHost,
      const wr::ExternalImageId aWrappedExternalImageId);

  virtual ~TextureHostWrapperD3D11();

  TextureHost* EnsureWrappedTextureHost();

  const RefPtr<TextureWrapperD3D11Allocator> mAllocator;
  const GpuProcessTextureId mTextureId;
  // DXGITextureHostD3D11 that replaces the wrapping TextureHost.
  const RefPtr<DXGITextureHostD3D11> mTextureHostD3D11;
  // WebRenderTextureHost that wraps BufferTextureHost
  // BufferTextureHost might be further wrapped by GPUVideoTextureHost.
  CompositableTextureHostRef mWrappedTextureHost;
  const wr::ExternalImageId mWrappedExternalImageId;
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_GFX_TextureHostWrapperD3D11_H
