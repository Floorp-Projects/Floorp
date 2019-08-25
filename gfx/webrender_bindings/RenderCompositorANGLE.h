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
struct IDCompositionDevice;
struct IDCompositionTarget;
struct IDCompositionVisual;
struct IDXGIFactory2;
struct IDXGISwapChain;

namespace mozilla {
namespace gl {
class GLLibraryEGL;
}  // namespace gl

namespace wr {

class RenderCompositorANGLE : public RenderCompositor {
 public:
  static UniquePtr<RenderCompositor> Create(
      RefPtr<widget::CompositorWidget>&& aWidget);

  explicit RenderCompositorANGLE(RefPtr<widget::CompositorWidget>&& aWidget);
  virtual ~RenderCompositorANGLE();
  bool Initialize();

  bool BeginFrame(layers::NativeLayer* aNativeLayer) override;
  void EndFrame() override;
  bool WaitForGPU() override;
  void Pause() override;
  bool Resume() override;

  gl::GLContext* gl() const override { return RenderThread::Get()->SharedGL(); }

  bool MakeCurrent() override;

  bool UseANGLE() const override { return true; }

  bool UseDComp() const override { return !!mCompositionDevice; }

  bool UseTripleBuffering() const override { return mUseTripleBuffering; }

  LayoutDeviceIntSize GetBufferSize() override;

  bool IsContextLost() override;

 protected:
  void InsertPresentWaitQuery();
  bool WaitForPreviousPresentQuery();
  bool ResizeBufferIfNeeded();
  void DestroyEGLSurface();
  ID3D11Device* GetDeviceOfEGLDisplay();
  void CreateSwapChainForDCompIfPossible(IDXGIFactory2* aDXGIFactory2);
  bool SutdownEGLLibraryIfNecessary();
  RefPtr<ID3D11Query> GetD3D11Query();

  EGLConfig mEGLConfig;
  EGLSurface mEGLSurface;

  int mUseTripleBuffering;

  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11DeviceContext> mCtx;
  RefPtr<IDXGISwapChain> mSwapChain;

  RefPtr<IDCompositionDevice> mCompositionDevice;
  RefPtr<IDCompositionTarget> mCompositionTarget;
  RefPtr<IDCompositionVisual> mVisual;

  std::queue<RefPtr<ID3D11Query>> mWaitForPresentQueries;
  RefPtr<ID3D11Query> mRecycledQuery;

  Maybe<LayoutDeviceIntSize> mBufferSize;
};

}  // namespace wr
}  // namespace mozilla

#endif
