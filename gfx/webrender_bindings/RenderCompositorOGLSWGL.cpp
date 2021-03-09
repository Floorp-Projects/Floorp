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

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/GeckoSurfaceTextureWrappers.h"
#  include "mozilla/widget/AndroidCompositorWidget.h"
#  include <android/native_window.h>
#  include <android/native_window_jni.h>
#endif

#ifdef MOZ_WIDGET_GTK
#  include "mozilla/widget/GtkCompositorWidget.h"
#  include <gdk/gdk.h>
#  include <gdk/gdkx.h>
#endif

namespace mozilla {
using namespace layers;

namespace wr {

UniquePtr<RenderCompositor> RenderCompositorOGLSWGL::Create(
    RefPtr<widget::CompositorWidget>&& aWidget, nsACString& aError) {
  if (!aWidget->GetCompositorOptions().AllowSoftwareWebRenderOGL()) {
    return nullptr;
  }

  RefPtr<Compositor> compositor;
#ifdef MOZ_WIDGET_ANDROID
  RefPtr<gl::GLContext> context = RenderThread::Get()->SharedGL();
  if (!context) {
    gfxCriticalNote << "SharedGL does not exist for SWGL";
    return nullptr;
  }
  nsCString log;
  RefPtr<CompositorOGL> compositorOGL;
  compositorOGL = new CompositorOGL(nullptr, aWidget, /* aSurfaceWidth */ -1,
                                    /* aSurfaceHeight */ -1,
                                    /* aUseExternalSurfaceSize */ true);
  if (!compositorOGL->Initialize(context, &log)) {
    gfxCriticalNote << "Failed to initialize CompositorOGL for SWGL: "
                    << log.get();
    return nullptr;
  }
  compositor = compositorOGL;
#elif defined(MOZ_WIDGET_GTK)
  nsCString log;
  RefPtr<CompositorOGL> compositorOGL;
  compositorOGL = new CompositorOGL(nullptr, aWidget);
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

  return MakeUnique<RenderCompositorOGLSWGL>(compositor, std::move(aWidget),
                                             ctx);
}

RenderCompositorOGLSWGL::RenderCompositorOGLSWGL(
    Compositor* aCompositor, RefPtr<widget::CompositorWidget>&& aWidget,
    void* aContext)
    : RenderCompositorLayersSWGL(aCompositor, std::move(aWidget), aContext) {}

RenderCompositorOGLSWGL::~RenderCompositorOGLSWGL() {
#ifdef OZ_WIDGET_ANDROID
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
    gfxCriticalNote << "Failed to create EGLSurface";
  }
  return surface;
}

void RenderCompositorOGLSWGL::DestroyEGLSurface() {
  MOZ_ASSERT(GetGLContext()->GetContextType() == gl::GLContextType::EGL);

  const auto& gle = gl::GLContextEGL::Cast(GetGLContext());
  const auto& egl = gle->mEgl;

  // Release EGLSurface of back buffer before calling ResizeBuffers().
  if (mEGLSurface) {
    gle->SetEGLSurfaceOverride(EGL_NO_SURFACE);
    egl->fDestroySurface(mEGLSurface);
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
      target, wrapMode, host->mSize, /* aIgnoreTransform */ true);
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

  // Query the new surface size as this may have changed. We cannot use
  // mWidget->GetClientSize() due to a race condition between
  // nsWindow::Resize() being called and the frame being rendered after the
  // surface is resized.
  EGLNativeWindowType window = mWidget->AsAndroid()->GetEGLNativeWindow();
  JNIEnv* const env = jni::GetEnvForThread();
  ANativeWindow* const nativeWindow =
      ANativeWindow_fromSurface(env, reinterpret_cast<jobject>(window));
  const int32_t width = ANativeWindow_getWidth(nativeWindow);
  const int32_t height = ANativeWindow_getHeight(nativeWindow);

  GLint maxTextureSize = 0;
  GetGLContext()->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE,
                               (GLint*)&maxTextureSize);

  // When window size is too big, hardware buffer allocation could fail.
  if (maxTextureSize < width || maxTextureSize < height) {
    gfxCriticalNote << "Too big ANativeWindow size(" << width << ", " << height
                    << ") MaxTextureSize " << maxTextureSize;
    return false;
  }

  mEGLSurface = CreateEGLSurface();
  if (mEGLSurface == EGL_NO_SURFACE) {
    RenderThread::Get()->HandleWebRenderError(WebRenderError::NEW_SURFACE);
    return false;
  }

  gl::GLContextEGL::Cast(GetGLContext())->SetEGLSurfaceOverride(mEGLSurface);
  mEGLSurfaceSize = Some(LayoutDeviceIntSize(width, height));
  ANativeWindow_release(nativeWindow);
  mCompositor->SetDestinationSurfaceSize(gfx::IntSize(width, height));
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
  if (mEGLSurfaceSize) {
    return *mEGLSurfaceSize;
  }
  return mWidget->GetClientSize();
}

UniquePtr<RenderCompositorLayersSWGL::Tile>
RenderCompositorOGLSWGL::DoCreateTile(Surface* aSurface) {
  const auto tileSize = aSurface->TileSize();

  RefPtr<DataTextureSource> source = new TextureImageTextureSourceOGL(
      mCompositor->AsCompositorOGL(), layers::TextureFlags::NO_FLAGS);

  RefPtr<gfx::DataSourceSurface> surf = gfx::Factory::CreateDataSourceSurface(
      gfx::IntSize(tileSize.width, tileSize.height),
      gfx::SurfaceFormat::B8G8R8A8);

  return MakeUnique<TileOGL>(source, surf);
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
  return true;
}

RenderCompositorOGLSWGL::TileOGL::TileOGL(layers::DataTextureSource* aTexture,
                                          gfx::DataSourceSurface* aSurface)
    : Tile(), mTexture(aTexture), mSurface(aSurface) {}

bool RenderCompositorOGLSWGL::TileOGL::Map(wr::DeviceIntRect aDirtyRect,
                                           wr::DeviceIntRect aValidRect,
                                           void** aData, int32_t* aStride) {
  gfx::DataSourceSurface::MappedSurface map;
  if (!mSurface->Map(gfx::DataSourceSurface::READ_WRITE, &map)) {
    return false;
  }
  *aData =
      map.mData + aValidRect.origin.y * map.mStride + aValidRect.origin.x * 4;
  *aStride = map.mStride;
  return true;
}

void RenderCompositorOGLSWGL::TileOGL::Unmap(const gfx::IntRect& aDirtyRect) {
  mSurface->Unmap();
  nsIntRegion dirty(aDirtyRect);
  mTexture->Update(mSurface, &dirty);
}

}  // namespace wr
}  // namespace mozilla
