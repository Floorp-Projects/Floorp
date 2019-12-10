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
  void WaitForCommitCompletion();

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

  // Get or create an FBO with depth buffer suitable for specified dimensions
  GLuint GetOrCreateFbo(int aWidth, int aHeight);

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

  // A list of layer IDs as they are added to the visual tree this frame.
  std::vector<uint64_t> mCurrentLayers;

  // The previous frame's list of layer IDs in visual order.
  std::vector<uint64_t> mPrevLayers;

  // Information about a cached FBO that is retained between frames.
  struct CachedFrameBuffer {
    int width;
    int height;
    GLuint fboId;
    GLuint depthRboId;
  };

  // A cache of FBOs, containing a depth buffer allocated to a specific size.
  // TODO(gw): Might be faster as a hashmap? The length is typically much less
  // than 10.
  std::vector<CachedFrameBuffer> mFrameBuffers;
};

class DCLayer {
 public:
  explicit DCLayer(DCLayerTree* aDCLayerTree);
  ~DCLayer();
  bool Initialize(wr::DeviceIntSize aSize, bool aIsOpaque);
  GLuint CreateEGLSurfaceForCompositionSurface(wr::DeviceIntRect aDirtyRect,
                                               wr::DeviceIntPoint* aOffset);
  void EndDraw();

  IDCompositionSurface* GetCompositionSurface() const {
    return mCompositionSurface;
  }
  IDCompositionVisual2* GetVisual() const { return mVisual; }

 protected:
  RefPtr<IDCompositionSurface> CreateCompositionSurface(wr::DeviceIntSize aSize,
                                                        bool aIsOpaque);
  void DestroyEGLSurface();

 protected:
  DCLayerTree* mDCLayerTree;

  RefPtr<IDCompositionSurface> mCompositionSurface;

  // The EGL image that is bound to the D3D texture provided by
  // DirectComposition.
  EGLImage mEGLImage;

  // The GL render buffer ID that maps the EGLImage to an RBO for attaching to
  // an FBO.
  GLuint mColorRBO;

  LayoutDeviceIntSize mBufferSize;

  RefPtr<IDCompositionVisual2> mVisual;
};

}  // namespace wr
}  // namespace mozilla

#endif
