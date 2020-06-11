/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 4; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLScreenBuffer.h"

#include "CompositorTypes.h"
#include "GLContext.h"
#include "gfx2DGlue.h"
#include "MozFramebuffer.h"
#include "SharedSurface.h"

namespace mozilla::gl {

// -
// SwapChainPresenter

UniquePtr<SwapChainPresenter> SwapChain::Acquire(const gfx::IntSize& size) {
  MOZ_ASSERT(mFactory);
  auto back = mFactory->CreateShared(size);
  if (!back) return nullptr;

  auto ret = MakeUnique<SwapChainPresenter>(*this);
  ret->SwapBackBuffer(std::move(back));
  return ret;
}

// -

SwapChainPresenter::SwapChainPresenter(SwapChain& swapChain)
    : mSwapChain(&swapChain) {
  MOZ_RELEASE_ASSERT(mSwapChain->mPresenter == nullptr);
  mSwapChain->mPresenter = this;
}

SwapChainPresenter::~SwapChainPresenter() {
  if (!mSwapChain) return;
  MOZ_RELEASE_ASSERT(mSwapChain->mPresenter == this);
  mSwapChain->mPresenter = nullptr;

  auto newFront = SwapBackBuffer(nullptr);
  if (newFront) {
    mSwapChain->mFrontBuffer = std::move(newFront);
  }
}

UniquePtr<SharedSurface> SwapChainPresenter::SwapBackBuffer(
    UniquePtr<SharedSurface> back) {
  if (mBackBuffer) {
    mBackBuffer->UnlockProd();
    mBackBuffer->ProducerRelease();
    mBackBuffer->Commit();
  }
  auto old = std::move(mBackBuffer);
  mBackBuffer = std::move(back);
  if (mBackBuffer) {
    mBackBuffer->WaitForBufferOwnership();
    mBackBuffer->ProducerAcquire();
    mBackBuffer->LockProd();
  }
  return old;
}

GLuint SwapChainPresenter::Fb() const {
  if (!mBackBuffer) return 0;
  const auto& fb = mBackBuffer->mFb;
  if (!fb) return 0;
  return fb->mFB;
}

// -
// SwapChain

SwapChain::SwapChain() = default;

SwapChain::~SwapChain() {
  if (mPresenter) {
    // Out of order destruction, but ok.
    (void)mPresenter->SwapBackBuffer(nullptr);
    mPresenter->mSwapChain = nullptr;
    mPresenter = nullptr;
  }
}

}  // namespace mozilla::gl
