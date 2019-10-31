/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DCLAYER_TREE_H
#define MOZILLA_GFX_DCLAYER_TREE_H

#include <unordered_map>
#include <windows.h>

#include "GLTypes.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/webrender/WebRenderTypes.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDCompositionDevice2;
struct IDCompositionSurface;
struct IDCompositionTarget;
struct IDCompositionVisual2;
struct IDXGISwapChain1;

namespace mozilla {

namespace gl {
class GLContext;
}

namespace wr {

// Currently, MinGW build environment does not handle IDCompositionDesktopDevice
// and IDCompositionDevice2
#if !defined(__MINGW32__)

class DCLayer;

/**
 * DCLayerTree manages direct composition layers.
 * It does not manage gecko's layers::Layer.
 */
class DCLayerTree {
 public:
  static UniquePtr<DCLayerTree> Create(gl::GLContext* aGL, EGLConfig aEGLConfig,
                                       ID3D11Device* aDevice, HWND aHwnd);
  explicit DCLayerTree(gl::GLContext* aGL, EGLConfig aEGLConfig,
                       ID3D11Device* aDevice,
                       IDCompositionDevice2* aCompositionDevice);
  ~DCLayerTree();

  void SetDefaultSwapChain(IDXGISwapChain1* aSwapChain);
  void MaybeUpdateDebug();

  // Interface for wr::Compositor
  void CompositorBeginFrame();
  void CompositorEndFrame();
  void Bind(wr::NativeSurfaceId aId, wr::DeviceIntPoint* aOffset,
            uint32_t* aFboId, wr::DeviceIntRect aDirtyRect);
  void Unbind();
  void CreateSurface(wr::NativeSurfaceId aId, wr::DeviceIntSize aSize,
                     bool aIsOpaque);
  void DestroySurface(NativeSurfaceId aId);
  void AddSurface(wr::NativeSurfaceId aId, wr::DeviceIntPoint aPosition,
                  wr::DeviceIntRect aClipRect);

  gl::GLContext* GetGLContext() const { return mGL; }
  EGLConfig GetEGLConfig() const { return mEGLConfig; }
  ID3D11Device* GetDevice() const { return mDevice; }
  IDCompositionDevice2* GetCompositionDevice() const {
    return mCompositionDevice;
  }

 protected:
  bool Initialize(HWND aHwnd);
  bool MaybeUpdateDebugCounter();
  bool MaybeUpdateDebugVisualRedrawRegions();

  RefPtr<gl::GLContext> mGL;
  EGLConfig mEGLConfig;

  RefPtr<ID3D11Device> mDevice;

  RefPtr<IDCompositionDevice2> mCompositionDevice;
  RefPtr<IDCompositionTarget> mCompositionTarget;
  RefPtr<IDCompositionVisual2> mRootVisual;
  RefPtr<IDCompositionVisual2> mDefaultSwapChainVisual;

  bool mDebugCounter;
  bool mDebugVisualRedrawRegions;

  Maybe<wr::NativeSurfaceId> mCurrentId;

  std::unordered_map<uint64_t, UniquePtr<DCLayer>> mDCLayers;
};

class DCLayer {
 public:
  explicit DCLayer(DCLayerTree* aDCLayerTree);
  ~DCLayer();
  bool Initialize(wr::DeviceIntSize aSize);
  bool CreateEGLSurfaceForCompositionSurface(wr::DeviceIntPoint* aOffset);
  void EndDraw();

  IDCompositionSurface* GetCompositionSurface() const {
    return mCompositionSurface;
  }
  EGLSurface GetEGLSurface() const { return mEGLSurface; }
  IDCompositionVisual2* GetVisual() const { return mVisual; }

 protected:
  RefPtr<IDCompositionSurface> CreateCompositionSurface(wr::DeviceIntSize aSize,
                                                        bool aUseAlpha);
  void DestroyEGLSurface();

 protected:
  DCLayerTree* mDCLayerTree;

  RefPtr<IDCompositionSurface> mCompositionSurface;

  LayoutDeviceIntSize mBufferSize;
  EGLSurface mEGLSurface;

  RefPtr<IDCompositionVisual2> mVisual;
};

#else
class DCLayerTree {
 public:
  static UniquePtr<DCLayerTree> Create(gl::GLContext* aGL, EGLConfig aEGLConfig,
                                       ID3D11Device* aDevice, HWND aHwnd) {
    return nullptr;
  }
  void SetDefaultSwapChain(IDXGISwapChain1* aSwapChain) {}
  void MaybeUpdateDebug() {}

  // Interface for wr::Compositor
  void CompositorBeginFrame() {}
  void CompositorEndFrame() {}
  void Bind(wr::NativeSurfaceId aId, wr::DeviceIntPoint* aOffset,
            uint32_t* aFboId, wr::DeviceIntRect aDirtyRect) {}
  void Unbind() {}
  void CreateSurface(wr::NativeSurfaceId aId, wr::DeviceIntSize aSize,
                     bool aIsOpaque) {}
  void DestroySurface(NativeSurfaceId aId) {}
  void AddSurface(wr::NativeSurfaceId aId, wr::DeviceIntPoint aPosition,
                  wr::DeviceIntRect aClipRect) {}
};
#endif

}  // namespace wr
}  // namespace mozilla

#endif
