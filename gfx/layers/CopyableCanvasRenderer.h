/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_COPYABLECANVASRENDERER_H
#define GFX_COPYABLECANVASRENDERER_H

#include <stdint.h>                     // for uint32_t
#include "CanvasRenderer.h"
#include "GLContextTypes.h"             // for GLContext
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxTypes.h"
#include "gfxPlatform.h"                // for gfxImageFormat
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/2D.h"             // for DrawTarget
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc

namespace mozilla {

namespace gl {
class SharedSurface;
} // namespace gl

namespace layers {

/**
 * A shared CanvasRenderer implementation that supports copying
 * its contents into a gfxASurface using RebackSurface.
 */
class CopyableCanvasRenderer : public CanvasRenderer
{
public:
  CopyableCanvasRenderer();
  virtual ~CopyableCanvasRenderer();

public:
  void Initialize(const CanvasInitializeData& aData) override;
  bool IsDataValid(const CanvasInitializeData& aData) override;

  void ClearCachedResources() override;
  void Destroy() override;

  CopyableCanvasRenderer* AsCopyableCanvasRenderer() override { return this; }

  bool NeedsYFlip() const { return mOriginPos == gl::OriginPos::BottomLeft; }
  bool HasGLContext() const { return !!mGLContext; }
  bool IsOpaque() const { return mOpaque; }

  PersistentBufferProvider* GetBufferProvider() { return mBufferProvider; }

  already_AddRefed<gfx::SourceSurface> ReadbackSurface();

protected:
  RefPtr<gl::GLContext> mGLContext;
  RefPtr<PersistentBufferProvider> mBufferProvider;
  UniquePtr<gl::SharedSurface> mGLFrontbuffer;
  RefPtr<AsyncCanvasRenderer> mAsyncRenderer;

  bool mIsAlphaPremultiplied;
  gl::OriginPos mOriginPos;

  bool mOpaque;

  RefPtr<gfx::DataSourceSurface> mCachedTempSurface;

  gfx::DataSourceSurface* GetTempSurface(const gfx::IntSize& aSize,
                                         const gfx::SurfaceFormat aFormat);
};

} // namespace layers
} // namespace mozilla

#endif
