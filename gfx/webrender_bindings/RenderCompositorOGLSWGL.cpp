/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 *
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RenderCompositorOGLSWGL.h"

#include "GLContext.h"
#include "GLContextEGL.h"
#include "mozilla/layers/BuildConstants.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/layers/TextureHostOGL.h"
#include "mozilla/widget/CompositorWidget.h"
#include "OGLShaderProgram.h"

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/GeckoSurfaceTextureWrappers.h"
#  include "mozilla/widget/AndroidCompositorWidget.h"
#  include <android/native_window.h>
#  include <android/native_window_jni.h>
#endif

#ifdef MOZ_WIDGET_GTK
#  include "mozilla/widget/GtkCompositorWidget.h"
#  include <gdk/gdk.h>
#  ifdef MOZ_X11
#    include <gdk/gdkx.h>
#  endif
#endif

namespace mozilla {
using namespace layers;
using namespace gfx;
namespace wr {

extern LazyLogModule gRenderThreadLog;
#define LOG(...) MOZ_LOG(gRenderThreadLog, LogLevel::Debug, (__VA_ARGS__))

UniquePtr<RenderCompositor> RenderCompositorOGLSWGL::Create(
    const RefPtr<widget::CompositorWidget>& aWidget, nsACString& aError) {
  if (!aWidget->GetCompositorOptions().AllowSoftwareWebRenderOGL()) {
    return nullptr;
  }

  RefPtr<Compositor> compositor;
#ifdef MOZ_WIDGET_ANDROID
  RefPtr<gl::GLContext> context =
      RenderThread::Get()->SingletonGLForCompositorOGL();
  if (!context) {
    gfxCriticalNote << "SingletonGL does not exist for SWGL";
    return nullptr;
  }
  auto programs = RenderThread::Get()->GetProgramsForCompositorOGL();
  if (!programs) {
    gfxCriticalNote << "Failed to get Programs for CompositorOGL for SWGL";
    return nullptr;
  }

  nsCString log;
  RefPtr<CompositorOGL> compositorOGL;
  compositorOGL = new CompositorOGL(aWidget, /* aSurfaceWidth */ -1,
                                    /* aSurfaceHeight */ -1,
                                    /* aUseExternalSurfaceSize */ true);
  if (!compositorOGL->Initialize(context, programs, &log)) {
    gfxCriticalNote << "Failed to initialize CompositorOGL for SWGL: "
                    << log.get();
    return nullptr;
  }
  compositor = compositorOGL;
#elif defined(MOZ_WIDGET_GTK)
  nsCString log;
  RefPtr<CompositorOGL> compositorOGL;
  compositorOGL = new CompositorOGL(aWidget);
  if (!compositorOGL->Initialize(&log)) {
    gfxCriticalNote << "Failed to initialize CompositorOGL for SWGL: "
                    << log.get();
    return nullptr;
  }
  compositor = compositorOGL;
#endif

  if (!compositor) {
    return nullptr;
  }

  void* ctx = wr_swgl_create_context();
  if (!ctx) {
    gfxCriticalNote << "Failed SWGL context creation for WebRender";
    return nullptr;
  }

  return MakeUnique<RenderCompositorOGLSWGL>(compositor, aWidget, ctx);
}

RenderCompositorOGLSWGL::RenderCompositorOGLSWGL(
    Compositor* aCompositor, const RefPtr<widget::CompositorWidget>& aWidget,
    void* aContext)
    : RenderCompositorLayersSWGL(aCompositor, aWidget, aContext) {
  LOG("RenderCompositorOGLSWGL::RenderCompositorOGLSWGL()");
}

RenderCompositorOGLSWGL::~RenderCompositorOGLSWGL() {
  LOG("RRenderCompositorOGLSWGL::~RenderCompositorOGLSWGL()");
#ifdef MOZ_WIDGET_ANDROID
  java::GeckoSurfaceTexture::DestroyUnused((int64_t)GetGLContext());
  DestroyEGLSurface();
#endif
}

gl::GLContext* RenderCompositorOGLSWGL::GetGLContext() {
  return mCompositor->AsCompositorOGL()->gl();
}

bool RenderCompositorOGLSWGL::MakeCurrent() {
  GetGLContext()->MakeCurrent();
#ifdef MOZ_WIDGET_ANDROID
  if (GetGLContext()->GetContextType() == gl::GLContextType::EGL) {
    gl::GLContextEGL::Cast(GetGLContext())->SetEGLSurfaceOverride(mEGLSurface);
  }
#endif
  RenderCompositorLayersSWGL::MakeCurrent();
  return true;
}

EGLSurface RenderCompositorOGLSWGL::CreateEGLSurface() {
  MOZ_ASSERT(GetGLContext()->GetContextType() == gl::GLContextType::EGL);

  EGLSurface surface = EGL_NO_SURFACE;
  surface = gl::GLContextEGL::CreateEGLSurfaceForCompositorWidget(
      mWidget, gl::GLContextEGL::Cast(GetGLContext())->mConfig);
  if (surface == EGL_NO_SURFACE) {
    const auto* renderThread = RenderThread::Get();
    gfxCriticalNote << "Failed to create EGLSurface. "
                    << renderThread->RendererCount() << " renderers, "
                    << renderThread->ActiveRendererCount() << " active.";
  }

  // The subsequent render after creating a new surface must be a full render.
  mFullRender = true;

  return surface;
}

void RenderCompositorOGLSWGL::DestroyEGLSurface() {
  MOZ_ASSERT(GetGLContext()->GetContextType() == gl::GLContextType::EGL);

  const auto& gle = gl::GLContextEGL::Cast(GetGLContext());
  const auto& egl = gle->mEgl;

  // Release EGLSurface of back buffer before calling ResizeBuffers().
  if (mEGLSurface) {
    gle->SetEGLSurfaceOverride(EGL_NO_SURFACE);
    if (!egl->fMakeCurrent(EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)) {
      const EGLint err = egl->mLib->fGetError();
      gfxCriticalNote << "Error in eglMakeCurrent: " << gfx::hexa(err);
    }
    if (!egl->fDestroySurface(mEGLSurface)) {
      const EGLint err = egl->mLib->fGetError();
      gfxCriticalNote << "Error in eglDestroySurface: " << gfx::hexa(err);
    }
    mEGLSurface = EGL_NO_SURFACE;
  }
}

bool RenderCompositorOGLSWGL::BeginFrame() {
  MOZ_ASSERT(!mInFrame);
  RenderCompositorLayersSWGL::BeginFrame();

#ifdef MOZ_WIDGET_ANDROID
  java::GeckoSurfaceTexture::DestroyUnused((int64_t)GetGLContext());
  GetGLContext()
      ->MakeCurrent();  // DestroyUnused can change the current context!
#endif

  return true;
}

RenderedFrameId RenderCompositorOGLSWGL::EndFrame(
    const nsTArray<DeviceIntRect>& aDirtyRects) {
  mFullRender = false;

  return RenderCompositorLayersSWGL::EndFrame(aDirtyRects);
}

void RenderCompositorOGLSWGL::HandleExternalImage(
    RenderTextureHost* aExternalImage, FrameSurface& aFrameSurface) {
  MOZ_ASSERT(aExternalImage);

#ifdef MOZ_WIDGET_ANDROID
  GLenum target =
      LOCAL_GL_TEXTURE_EXTERNAL;  // This is required by SurfaceTexture
  GLenum wrapMode = LOCAL_GL_CLAMP_TO_EDGE;

  auto* host = aExternalImage->AsRenderAndroidSurfaceTextureHost();
  // We need to hold the texture source separately from the effect,
  // since the effect doesn't hold a strong reference.
  RefPtr<SurfaceTextureSource> layer = new SurfaceTextureSource(
      (TextureSourceProvider*)mCompositor, host->mSurfTex, host->mFormat,
      target, wrapMode, host->mSize, host->mTransformOverride);
  RefPtr<TexturedEffect> texturedEffect =
      CreateTexturedEffect(host->mFormat, layer, aFrameSurface.mFilter,
                           /* isAlphaPremultiplied */ true);

  gfx::Rect drawRect(0, 0, host->mSize.width, host->mSize.height);

  EffectChain effect;
  effect.mPrimaryEffect = texturedEffect;
  mCompositor->DrawQuad(drawRect, aFrameSurface.mClipRect, effect, 1.0,
                        aFrameSurface.mTransform, drawRect);
#endif
}

void RenderCompositorOGLSWGL::GetCompositorCapabilities(
    CompositorCapabilities* aCaps) {
  RenderCompositor::GetCompositorCapabilities(aCaps);

  // max_update_rects are not yet handled properly
  aCaps->max_update_rects = 0;
}

bool RenderCompositorOGLSWGL::RequestFullRender() { return mFullRender; }

void RenderCompositorOGLSWGL::Pause() {
#ifdef MOZ_WIDGET_ANDROID
  DestroyEGLSurface();
#elif defined(MOZ_WIDGET_GTK)
  mCompositor->Pause();
#endif
}

bool RenderCompositorOGLSWGL::Resume() {
#ifdef MOZ_WIDGET_ANDROID
  // Destroy EGLSurface if it exists.
  DestroyEGLSurface();

  auto size = GetBufferSize();
  GLint maxTextureSize = 0;
  GetGLContext()->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE,
                               (GLint*)&maxTextureSize);

  // When window size is too big, hardware buffer allocation could fail.
  if (maxTextureSize < size.width || maxTextureSize < size.height) {
    gfxCriticalNote << "Too big ANativeWindow size(" << size.width << ", "
                    << size.height << ") MaxTextureSize " << maxTextureSize;
    return false;
  }

  mEGLSurface = CreateEGLSurface();
  if (mEGLSurface == EGL_NO_SURFACE) {
    RenderThread::Get()->HandleWebRenderError(WebRenderError::NEW_SURFACE);
    return false;
  }

  gl::GLContextEGL::Cast(GetGLContext())->SetEGLSurfaceOverride(mEGLSurface);
  mCompositor->SetDestinationSurfaceSize(size.ToUnknownSize());
#elif defined(MOZ_WIDGET_GTK)
  bool resumed = mCompositor->Resume();
  if (!resumed) {
    RenderThread::Get()->HandleWebRenderError(WebRenderError::NEW_SURFACE);
    return false;
  }
#endif
  return true;
}

bool RenderCompositorOGLSWGL::IsPaused() {
#ifdef MOZ_WIDGET_ANDROID
  return mEGLSurface == EGL_NO_SURFACE;
#endif
  return false;
}

LayoutDeviceIntSize RenderCompositorOGLSWGL::GetBufferSize() {
  return mWidget->GetClientSize();
}

UniquePtr<RenderCompositorLayersSWGL::Tile>
RenderCompositorOGLSWGL::DoCreateTile(Surface* aSurface) {
  auto source = MakeRefPtr<TextureImageTextureSourceOGL>(
      mCompositor->AsCompositorOGL(), layers::TextureFlags::NO_FLAGS);

  return MakeUnique<TileOGL>(std::move(source), aSurface->TileSize());
}

bool RenderCompositorOGLSWGL::MaybeReadback(
    const gfx::IntSize& aReadbackSize, const wr::ImageFormat& aReadbackFormat,
    const Range<uint8_t>& aReadbackBuffer, bool* aNeedsYFlip) {
#ifdef MOZ_WIDGET_ANDROID
  MOZ_ASSERT(aReadbackFormat == wr::ImageFormat::RGBA8);
  const GLenum format = LOCAL_GL_RGBA;
#else
  MOZ_ASSERT(aReadbackFormat == wr::ImageFormat::BGRA8);
  const GLenum format = LOCAL_GL_BGRA;
#endif

  GetGLContext()->fReadPixels(0, 0, aReadbackSize.width, aReadbackSize.height,
                              format, LOCAL_GL_UNSIGNED_BYTE,
                              &aReadbackBuffer[0]);

  if (aNeedsYFlip) {
    *aNeedsYFlip = true;
  }

  return true;
}

// This is a DataSourceSurface that represents a 0-based PBO for GLTextureImage.
class PBOUnpackSurface : public gfx::DataSourceSurface {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(PBOUnpackSurface, override)

  explicit PBOUnpackSurface(const gfx::IntSize& aSize) : mSize(aSize) {}

  uint8_t* GetData() override { return nullptr; }
  int32_t Stride() override { return mSize.width * sizeof(uint32_t); }
  gfx::SurfaceType GetType() const override {
    return gfx::SurfaceType::DATA_ALIGNED;
  }
  gfx::IntSize GetSize() const override { return mSize; }
  gfx::SurfaceFormat GetFormat() const override {
    return gfx::SurfaceFormat::B8G8R8A8;
  }

  // PBO offsets need to start from a 0 address, but DataSourceSurface::Map
  // checks for failure by comparing the address against nullptr. Override Map
  // to work around this.
  bool Map(MapType, MappedSurface* aMappedSurface) override {
    aMappedSurface->mData = GetData();
    aMappedSurface->mStride = Stride();
    return true;
  }

  void Unmap() override {}

 private:
  gfx::IntSize mSize;
};

RenderCompositorOGLSWGL::TileOGL::TileOGL(
    RefPtr<layers::TextureImageTextureSourceOGL>&& aTexture,
    const gfx::IntSize& aSize)
    : mTexture(aTexture) {
  auto* gl = mTexture->gl();
  if (gl && gl->HasPBOState() && gl->MakeCurrent()) {
    mSurface = new PBOUnpackSurface(aSize);
    // Create a PBO large enough to encompass any valid rects within the tile.
    gl->fGenBuffers(1, &mPBO);
    gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, mPBO);
    gl->fBufferData(LOCAL_GL_PIXEL_UNPACK_BUFFER,
                    mSurface->Stride() * aSize.height, nullptr,
                    LOCAL_GL_DYNAMIC_DRAW);
    gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
  } else {
    // Couldn't allocate a PBO, so just use a memory surface instead.
    mSurface = gfx::Factory::CreateDataSourceSurface(
        aSize, gfx::SurfaceFormat::B8G8R8A8);
  }
}

RenderCompositorOGLSWGL::TileOGL::~TileOGL() {
  if (mPBO) {
    auto* gl = mTexture->gl();
    if (gl && gl->MakeCurrent()) {
      gl->fDeleteBuffers(1, &mPBO);
      mPBO = 0;
    }
  }
}

layers::DataTextureSource*
RenderCompositorOGLSWGL::TileOGL::GetTextureSource() {
  return mTexture.get();
}

bool RenderCompositorOGLSWGL::TileOGL::Map(wr::DeviceIntRect aDirtyRect,
                                           wr::DeviceIntRect aValidRect,
                                           void** aData, int32_t* aStride) {
  if (mPBO) {
    auto* gl = mTexture->gl();
    if (!gl) {
      return false;
    }
    // Map the PBO, but only within the range of the buffer that spans from the
    // linear start offset to the linear end offset. Since we don't care about
    // the previous contents of the buffer, we can just tell OpenGL to
    // invalidate the entire buffer, even though we're only mapping a sub-range.
    gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, mPBO);
    size_t stride = mSurface->Stride();
    size_t offset =
        stride * aValidRect.min.y + aValidRect.min.x * sizeof(uint32_t);
    size_t length = stride * (aValidRect.height() - 1) +
                    (aValidRect.width()) * sizeof(uint32_t);
    void* data = gl->fMapBufferRange(
        LOCAL_GL_PIXEL_UNPACK_BUFFER, offset, length,
        LOCAL_GL_MAP_WRITE_BIT | LOCAL_GL_MAP_INVALIDATE_BUFFER_BIT);
    gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
    if (!data) {
      return false;
    }
    *aData = data;
    *aStride = stride;
  } else {
    // No PBO is available, so just directly write to the memory surface.
    gfx::DataSourceSurface::MappedSurface map;
    if (!mSurface->Map(gfx::DataSourceSurface::READ_WRITE, &map)) {
      return false;
    }
    // Verify that we're not somehow using a PBOUnpackSurface.
    MOZ_ASSERT(map.mData != nullptr);
    // glTex(Sub)Image on ES doesn't support arbitrary strides without
    // the EXT_unpack_subimage extension. To avoid needing to make a
    // copy of the data we'll always draw it with stride = bpp*width
    // unless we're uploading the entire texture.
    if (!mTexture->IsValid()) {
      // If we don't have a texture we need to position our
      // data in the correct spot because we're going to upload
      // the entire surface
      *aData = map.mData + aValidRect.min.y * map.mStride +
               aValidRect.min.x * sizeof(uint32_t);

      *aStride = map.mStride;
      mSubSurface = nullptr;
    } else {
      // Otherwise, we can just use the top left as a scratch space
      *aData = map.mData;
      *aStride = aDirtyRect.width() * BytesPerPixel(mSurface->GetFormat());
      mSubSurface = Factory::CreateWrappingDataSourceSurface(
          (uint8_t*)*aData, *aStride,
          IntSize(aDirtyRect.width(), aDirtyRect.height()),
          mSurface->GetFormat());
    }
  }
  return true;
}

void RenderCompositorOGLSWGL::TileOGL::Unmap(const gfx::IntRect& aDirtyRect) {
  nsIntRegion dirty(aDirtyRect);
  if (mPBO) {
    // If there is a PBO, it must be unmapped before it can be sourced from.
    // Leave the PBO bound before the call to Update so that the texture uploads
    // will source from it.
    auto* gl = mTexture->gl();
    if (!gl) {
      return;
    }
    gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, mPBO);
    gl->fUnmapBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER);
    mTexture->Update(mSurface, &dirty);
    gl->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
  } else {
    if (mSubSurface) {
      mSurface->Unmap();
      // Our subsurface has a stride = aDirtyRect.width
      // We use a negative offset to move it to match
      // the dirty rect's top-left. These two offsets
      // will cancel each other out by the time we reach
      // TexSubImage.
      IntPoint srcOffset = {0, 0};
      IntPoint dstOffset = aDirtyRect.TopLeft();
      // adjust the dirty region to be relative to the dstOffset
      dirty.MoveBy(-dstOffset);
      mTexture->Update(mSubSurface, &dirty, &srcOffset, &dstOffset);
      mSubSurface = nullptr;
    } else {
      mSurface->Unmap();
      mTexture->Update(mSurface, &dirty);
    }
  }
}

}  // namespace wr
}  // namespace mozilla
