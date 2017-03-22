/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderTextureHost.h"

#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/webrender/RenderTextureHost.h"
#include "mozilla/webrender/RenderThread.h"

namespace mozilla {
namespace layers {

uint64_t WebRenderTextureHost::sSerialCounter(0);

WebRenderTextureHost::WebRenderTextureHost(TextureFlags aFlags,
                                           TextureHost* aTexture)
  : TextureHost(aFlags)
  , mExternalImageId(++sSerialCounter)
{
  MOZ_COUNT_CTOR(WebRenderTextureHost);
  mWrappedTextureHost = aTexture;

  // XXX support only BufferTextureHost for now.
  BufferTextureHost* bufferTexture = aTexture->AsBufferTextureHost();
  MOZ_ASSERT(bufferTexture);
  RefPtr<wr::RenderTextureHost> texture =
    new wr::RenderTextureHost(bufferTexture->GetBuffer(),
                              bufferTexture->GetBufferDescriptor());
  wr::RenderThread::Get()->RegisterExternalImage(mExternalImageId, texture);
}

WebRenderTextureHost::~WebRenderTextureHost()
{
  MOZ_COUNT_DTOR(WebRenderTextureHost);
  wr::RenderThread::Get()->UnregisterExternalImage(mExternalImageId);
}

bool
WebRenderTextureHost::Lock()
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return false;
}

void
WebRenderTextureHost::Unlock()
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return;
}

bool
WebRenderTextureHost::BindTextureSource(CompositableTextureSourceRef& aTexture)
{
  MOZ_ASSERT_UNREACHABLE("unexpected to be called");
  return false;
}

already_AddRefed<gfx::DataSourceSurface>
WebRenderTextureHost::GetAsSurface()
{
  if (!mWrappedTextureHost) {
    return nullptr;
  }
  return mWrappedTextureHost->GetAsSurface();
}

void
WebRenderTextureHost::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
}

YUVColorSpace
WebRenderTextureHost::GetYUVColorSpace() const
{
  if (mWrappedTextureHost) {
    return mWrappedTextureHost->GetYUVColorSpace();
  }
  return YUVColorSpace::UNKNOWN;
}

gfx::IntSize
WebRenderTextureHost::GetSize() const
{
  if (!mWrappedTextureHost) {
    return gfx::IntSize();
  }
  return mWrappedTextureHost->GetSize();
}

gfx::SurfaceFormat
WebRenderTextureHost::GetFormat() const
{
  if (!mWrappedTextureHost) {
    return gfx::SurfaceFormat::UNKNOWN;
  }
  return mWrappedTextureHost->GetFormat();
}

int32_t
WebRenderTextureHost::GetRGBStride()
{
  if (!mWrappedTextureHost) {
    return 0;
  }
  gfx::SurfaceFormat format = GetFormat();
  if (GetFormat() == SurfaceFormat::YUV) {
    // XXX this stride is used until yuv image rendering by webrender is used.
    // Software converted RGB buffers strides are aliened to 16
    return GetAlignedStride<16>(GetSize().width, BytesPerPixel(SurfaceFormat::B8G8R8A8));
  }
  return ImageDataSerializer::ComputeRGBStride(format, GetSize().width);
}

} // namespace layers
} // namespace mozilla
