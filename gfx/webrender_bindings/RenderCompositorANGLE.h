/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_RENDERCOMPOSITOR_ANGLE_H
#define MOZILLA_GFX_RENDERCOMPOSITOR_ANGLE_H

#include <queue>

#include "GLTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/webrender/RenderCompositor.h"
#include "mozilla/webrender/RenderThread.h"

struct ID3D11DeviceContext;
struct ID3D11Device;
struct ID3D11Query;
struct IDXGIFactory2;
struct IDXGISwapChain;
struct IDXGISwapChain1;

namespace mozilla {
namespace gl {
class GLLibraryEGL;
}  // namespace gl

namespace wr {

class DCLayerTree;

class RenderCompositorANGLE : public RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      const RefPtr<widget::CompositorWidget>& aWidget, nsACString& aError);

  explicit RenderCompositorANGLE(
      const RefPtr<widget::CompositorWidget>& aWidget,
      RefPtr<gl::GLContext>&& aGL);
  virtual ~RenderCompositorANGLE();
  bool Initialize(nsACString& aError);

  bool BeginFrame() override;
  RenderedFrameId EndFrame(const nsTArray<DeviceIntRect>& aDirtyRects) final;
  bool WaitForGPU() override;
  RenderedFrameId GetLastCompletedFrameId() final;
  RenderedFrameId UpdateFrameId() final;
  void Pause() override;
  bool Resume() override;
  void Update() override;

  gl::GLContext* gl() const override { return mGL; }

  bool MakeCurrent() override;

  bool UseANGLE() const override { return true; }

  bool UseDComp() const override { return !!mDCLayerTree; }

  bool UseTripleBuffering() const override { return mUseTripleBuffering; }

  layers::WebRenderCompositor CompositorType() const override {
    if (UseDComp()) {
      return layers::WebRenderCompositor::DIRECT_COMPOSITION;
    }
    return layers::WebRenderCompositor::DRAW;
  }

  LayoutDeviceIntSize GetBufferSize() override;

  GLenum IsContextLost(bool aForce) override;

  bool SurfaceOriginIsTopLeft() override { return true; }

  bool SupportAsyncScreenshot() override;

  bool ShouldUseNativeCompositor() override;

  // Interface for wr::Compositor
  void CompositorBeginFrame() override;
  void CompositorEndFrame() override;
  void Bind(wr::NativeTileId aId, wr::DeviceIntPoint* aOffset, uint32_t* aFboId,
            wr::DeviceIntRect aDirtyRect,
            wr::DeviceIntRect aValidRect) override;
  void Unbind() override;
  void CreateSurface(wr::NativeSurfaceId aId, wr::DeviceIntPoint aVirtualOffset,
                     wr::DeviceIntSize aTileSize, bool aIsOpaque) override;
  void CreateExternalSurface(wr::NativeSurfaceId aId, bool aIsOpaque) override;
  void DestroySurface(NativeSurfaceId aId) override;
  void CreateTile(wr::NativeSurfaceId aId, int32_t aX, int32_t aY) override;
  void DestroyTile(wr::NativeSurfaceId aId, int32_t aX, int32_t aY) override;
  void AttachExternalImage(wr::NativeSurfaceId aId,
                           wr::ExternalImageId aExternalImage) override;
  void AddSurface(wr::NativeSurfaceId aId,
                  const wr::CompositorSurfaceTransform& aTransform,
                  wr::DeviceIntRect aClipRect,
                  wr::ImageRendering aImageRendering) override;
  void EnableNativeCompositor(bool aEnable) override;
  void GetCompositorCapabilities(CompositorCapabilities* aCaps) override;

  // Interface for partial present
  bool UsePartialPresent() override;
  bool RequestFullRender() override;
  uint32_t GetMaxPartialPresentRects() override;

  bool MaybeReadback(const gfx::IntSize& aReadbackSize,
                     const wr::ImageFormat& aReadbackFormat,
                     const Range<uint8_t>& aReadbackBuffer,
                     bool* aNeedsYFlip) override;

 protected:
  bool UseCompositor();
  void InitializeUsePartialPresent();
  void InsertGraphicsCommandsFinishedWaitQuery(
      RenderedFrameId aRenderedFrameId);
  bool WaitForPreviousGraphicsCommandsFinishedQuery(bool aWaitAll = false);
  bool ResizeBufferIfNeeded();
  bool CreateEGLSurface();
  void DestroyEGLSurface();
  ID3D11Device* GetDeviceOfEGLDisplay(nsACString& aError);
  bool CreateSwapChain(nsACString& aError);
  void CreateSwapChainForDCompIfPossible(IDXGIFactory2* aDXGIFactory2);
  RefPtr<IDXGISwapChain1> CreateSwapChainForDComp(bool aUseTripleBuffering);
  RefPtr<ID3D11Query> GetD3D11Query();
  void ReleaseNativeCompositorResources();
  HWND GetCompositorHwnd();

  RefPtr<gl::GLContext> mGL;

  EGLConfig mEGLConfig = nullptr;
  EGLSurface mEGLSurface = nullptr;

  bool mUseTripleBuffering = false;

  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11DeviceContext> mCtx;
  RefPtr<IDXGISwapChain> mSwapChain;
  RefPtr<IDXGISwapChain1> mSwapChain1;

  UniquePtr<DCLayerTree> mDCLayerTree;

  std::queue<std::pair<RenderedFrameId, RefPtr<ID3D11Query>>>
      mWaitForPresentQueries;
  RefPtr<ID3D11Query> mRecycledQuery;
  RenderedFrameId mLastCompletedFrameId;

  Maybe<LayoutDeviceIntSize> mBufferSize;
  bool mUseNativeCompositor = true;
  bool mUsePartialPresent = false;
  bool mFullRender = false;
  // Used to know a timing of disabling native compositor.
  bool mDisablingNativeCompositor = false;
  bool mFirstPresent = true;
};

}  // namespace wr
}  // namespace mozilla

#endif
