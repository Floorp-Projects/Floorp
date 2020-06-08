/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* GLScreenBuffer is the abstraction for the "default framebuffer" used
 * by an offscreen GLContext. Since it's only for offscreen GLContext's,
 * it's only useful for things like WebGL, and is NOT used by the
 * compositor's GLContext. Remember that GLContext provides an abstraction
 * so that even if you want to draw to the 'screen', even if that's not
 * actually the screen, just draw to 0. This GLScreenBuffer class takes the
 * logic handling out of GLContext.
 */

#ifndef SCREEN_BUFFER_H_
#define SCREEN_BUFFER_H_

#include "GLContextTypes.h"
#include "GLDefs.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/UniquePtr.h"
#include "SurfaceTypes.h"

namespace mozilla {
namespace gl {

class SharedSurface;
class SurfaceFactory;
class SwapChain;

class SwapChainPresenter final {
  friend class SwapChain;

  SwapChain* mSwapChain;
  UniquePtr<SharedSurface> mBackBuffer;

 public:
  explicit SwapChainPresenter(SwapChain& swapChain);
  ~SwapChainPresenter();

  const auto& BackBuffer() const { return mBackBuffer; }

  UniquePtr<SharedSurface> SwapBackBuffer(UniquePtr<SharedSurface>);
  GLuint Fb() const;
};

// -

class SwapChain final {
  friend class SwapChainPresenter;

 public:
  UniquePtr<SurfaceFactory> mFactory;

 private:
  UniquePtr<SharedSurface> mFrontBuffer;
  SwapChainPresenter* mPresenter = nullptr;

 public:
  SwapChain();
  virtual ~SwapChain();

  const auto& FrontBuffer() const { return mFrontBuffer; }
  UniquePtr<SwapChainPresenter> Acquire(const gfx::IntSize&);
};

}  // namespace gl
}  // namespace mozilla

#endif  // SCREEN_BUFFER_H_
