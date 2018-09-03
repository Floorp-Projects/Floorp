/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderTextureHostWrapper.h"

#include "mozilla/gfx/Logging.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/SourceSurfaceSharedData.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace wr {

RenderTextureHostWrapper::RenderTextureHostWrapper()
  : mInited(false)
  , mLocked(false)
{
  MOZ_COUNT_CTOR_INHERITED(RenderTextureHostWrapper, RenderTextureHost);
}

RenderTextureHostWrapper::~RenderTextureHostWrapper()
{
  MOZ_COUNT_DTOR_INHERITED(RenderTextureHostWrapper, RenderTextureHost);
}

wr::WrExternalImage
RenderTextureHostWrapper::Lock(uint8_t aChannelIndex,
                               gl::GLContext* aGL,
                               wr::ImageRendering aRendering)
{
  if (!mTextureHost) {
    MOZ_ASSERT_UNREACHABLE("unexpected to happen");
    return InvalidToWrExternalImage();
  }

  mLocked = true;
  return mTextureHost->Lock(aChannelIndex, aGL, aRendering);
}

void
RenderTextureHostWrapper::Unlock()
{
  if (mTextureHost) {
    mTextureHost->Unlock();
  }
  mLocked = false;
}

void
RenderTextureHostWrapper::ClearCachedResources()
{
  if (mTextureHost) {
    mTextureHost->ClearCachedResources();
  }
}

void
RenderTextureHostWrapper::UpdateRenderTextureHost(RenderTextureHost* aTextureHost)
{
  MOZ_ASSERT(!mInited || RenderThread::IsInRenderThread());
  MOZ_RELEASE_ASSERT(!mLocked);

  mInited = true;
  mTextureHost = aTextureHost;
}

} // namespace wr
} // namespace mozilla
