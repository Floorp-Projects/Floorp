/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MacIOSurfaceTextureHostOGL.h"
#include "mozilla/gfx/MacIOSurface.h"
#include "mozilla/webrender/RenderMacIOSurfaceTextureHostOGL.h"
#include "mozilla/webrender/RenderThread.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "GLContextCGL.h"

namespace mozilla {
namespace layers {

MacIOSurfaceTextureHostOGL::MacIOSurfaceTextureHostOGL(TextureFlags aFlags,
                                                       const SurfaceDescriptorMacIOSurface& aDescriptor)
  : TextureHost(aFlags)
{
  MOZ_COUNT_CTOR(MacIOSurfaceTextureHostOGL);
  mSurface = MacIOSurface::LookupSurface(aDescriptor.surfaceId(),
                                         aDescriptor.scaleFactor(),
                                         !aDescriptor.isOpaque());
}

MacIOSurfaceTextureHostOGL::~MacIOSurfaceTextureHostOGL()
{
  MOZ_COUNT_DTOR(MacIOSurfaceTextureHostOGL);
}

GLTextureSource*
MacIOSurfaceTextureHostOGL::CreateTextureSourceForPlane(size_t aPlane)
{
  GLuint textureHandle;
  gl::GLContext* gl = mProvider->GetGLContext();
  gl->fGenTextures(1, &textureHandle);
  gl->fBindTexture(LOCAL_GL_TEXTURE_RECTANGLE_ARB, textureHandle);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_RECTANGLE_ARB, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);

  gfx::SurfaceFormat readFormat = gfx::SurfaceFormat::UNKNOWN;
  mSurface->CGLTexImageIOSurface2D(gl,
                                   gl::GLContextCGL::Cast(gl)->GetCGLContext(),
                                   aPlane,
                                   &readFormat);
  // With compositorOGL, we doesn't support the yuv interleaving format yet.
  MOZ_ASSERT(readFormat != gfx::SurfaceFormat::YUV422);

  return new GLTextureSource(mProvider, textureHandle, LOCAL_GL_TEXTURE_RECTANGLE_ARB,
                             gfx::IntSize(mSurface->GetDevicePixelWidth(aPlane),
                                          mSurface->GetDevicePixelHeight(aPlane)),
                             // XXX: This isn't really correct (but isn't used), we should be using the
                             // format of the individual plane, not of the whole buffer.
                             mSurface->GetFormat());
}

bool
MacIOSurfaceTextureHostOGL::Lock()
{
  if (!gl() || !gl()->MakeCurrent() || !mSurface) {
    return false;
  }

  if (!mTextureSource) {
    mTextureSource = CreateTextureSourceForPlane(0);

    RefPtr<TextureSource> prev = mTextureSource;
    for (size_t i = 1; i < mSurface->GetPlaneCount(); i++) {
      RefPtr<TextureSource> next = CreateTextureSourceForPlane(i);
      prev->SetNextSibling(next);
      prev = next;
    }
  }
  return true;
}

void
MacIOSurfaceTextureHostOGL::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  if (!aProvider || !aProvider->GetGLContext()) {
    mTextureSource = nullptr;
    mProvider = nullptr;
    return;
  }

  if (mProvider != aProvider) {
    // Cannot share GL texture identifiers across compositors.
    mTextureSource = nullptr;
  }

  mProvider = aProvider;
}

gfx::SurfaceFormat
MacIOSurfaceTextureHostOGL::GetFormat() const {
  return mSurface->GetFormat();
}

gfx::SurfaceFormat
MacIOSurfaceTextureHostOGL::GetReadFormat() const {
  return mSurface->GetReadFormat();
}

gfx::IntSize
MacIOSurfaceTextureHostOGL::GetSize() const {
  if (!mSurface) {
    return gfx::IntSize();
  }
  return gfx::IntSize(mSurface->GetDevicePixelWidth(),
                      mSurface->GetDevicePixelHeight());
}

gl::GLContext*
MacIOSurfaceTextureHostOGL::gl() const
{
  return mProvider ? mProvider->GetGLContext() : nullptr;
}

void
MacIOSurfaceTextureHostOGL::CreateRenderTexture(const wr::ExternalImageId& aExternalImageId)
{
  RefPtr<wr::RenderTextureHost> texture =
      new wr::RenderMacIOSurfaceTextureHostOGL(GetMacIOSurface());

  wr::RenderThread::Get()->RegisterExternalImage(wr::AsUint64(aExternalImageId), texture.forget());
}

uint32_t
MacIOSurfaceTextureHostOGL::NumSubTextures() const
{
  switch (GetFormat()) {
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
    case gfx::SurfaceFormat::YUV422: {
      return 1;
    }
    case gfx::SurfaceFormat::NV12: {
      return 2;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("unexpected format");
      return 1;
    }
  }
}

void
MacIOSurfaceTextureHostOGL::PushResourceUpdates(wr::ResourceUpdateQueue& aResources,
                                                ResourceUpdateOp aOp,
                                                const Range<wr::ImageKey>& aImageKeys,
                                                const wr::ExternalImageId& aExtID)
{
  MOZ_ASSERT(mSurface);

  auto method = aOp == TextureHost::ADD_IMAGE ? &wr::ResourceUpdateQueue::AddExternalImage
                                              : &wr::ResourceUpdateQueue::UpdateExternalImage;
  auto bufferType = wr::WrExternalImageBufferType::TextureRectHandle;

  switch (GetFormat()) {
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8: {
      MOZ_ASSERT(aImageKeys.length() == 1);
      MOZ_ASSERT(mSurface->GetPlaneCount() == 0);
      wr::ImageDescriptor descriptor(GetSize(), GetFormat());
      (aResources.*method)(aImageKeys[0], descriptor, aExtID, bufferType, 0);
      break;
    }
    case gfx::SurfaceFormat::YUV422: {
      // This is the special buffer format. The buffer contents could be a
      // converted RGB interleaving data or a YCbCr interleaving data depending
      // on the different platform setting. (e.g. It will be RGB at OpenGL 2.1
      // and YCbCr at OpenGL 3.1)
      MOZ_ASSERT(aImageKeys.length() == 1);
      MOZ_ASSERT(mSurface->GetPlaneCount() == 0);
      wr::ImageDescriptor descriptor(GetSize(), gfx::SurfaceFormat::R8G8B8X8);
      (aResources.*method)(aImageKeys[0], descriptor, aExtID, bufferType, 0);
      break;
    }
    case gfx::SurfaceFormat::NV12: {
      MOZ_ASSERT(aImageKeys.length() == 2);
      MOZ_ASSERT(mSurface->GetPlaneCount() == 2);
      wr::ImageDescriptor descriptor0(gfx::IntSize(mSurface->GetDevicePixelWidth(0), mSurface->GetDevicePixelHeight(0)),
                                      gfx::SurfaceFormat::A8);
      wr::ImageDescriptor descriptor1(gfx::IntSize(mSurface->GetDevicePixelWidth(1), mSurface->GetDevicePixelHeight(1)),
                                      gfx::SurfaceFormat::R8G8);
      (aResources.*method)(aImageKeys[0], descriptor0, aExtID, bufferType, 0);
      (aResources.*method)(aImageKeys[1], descriptor1, aExtID, bufferType, 1);
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    }
  }
}

void
MacIOSurfaceTextureHostOGL::PushDisplayItems(wr::DisplayListBuilder& aBuilder,
                                             const wr::LayoutRect& aBounds,
                                             const wr::LayoutRect& aClip,
                                             wr::ImageRendering aFilter,
                                             const Range<wr::ImageKey>& aImageKeys)
{
  switch (GetFormat()) {
    case gfx::SurfaceFormat::R8G8B8X8:
    case gfx::SurfaceFormat::R8G8B8A8:
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8: {
      MOZ_ASSERT(aImageKeys.length() == 1);
      MOZ_ASSERT(mSurface->GetPlaneCount() == 0);
      aBuilder.PushImage(aBounds, aClip, true, aFilter, aImageKeys[0]);
      break;
    }
    case gfx::SurfaceFormat::YUV422: {
      MOZ_ASSERT(aImageKeys.length() == 1);
      MOZ_ASSERT(mSurface->GetPlaneCount() == 0);
      aBuilder.PushYCbCrInterleavedImage(aBounds,
                                         aClip,
                                         true,
                                         aImageKeys[0],
                                         wr::ToWrYuvColorSpace(YUVColorSpace::BT601),
                                         aFilter);
      break;
    }
    case gfx::SurfaceFormat::NV12: {
      MOZ_ASSERT(aImageKeys.length() == 2);
      MOZ_ASSERT(mSurface->GetPlaneCount() == 2);
      aBuilder.PushNV12Image(aBounds,
                             aClip,
                             true,
                             aImageKeys[0],
                             aImageKeys[1],
                             wr::ToWrYuvColorSpace(YUVColorSpace::BT601),
                             aFilter);
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    }
  }
}

} // namespace layers
} // namespace mozilla
