/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_COPYABLECANVASLAYER_H
#define GFX_COPYABLECANVASLAYER_H

#include <stdint.h>                     // for uint32_t
#include "GLContextTypes.h"             // for GLContext
#include "Layers.h"                     // for CanvasLayer, etc
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
 * A shared CanvasLayer implementation that supports copying
 * its contents into a gfxASurface using UpdateSurface.
 */
class CopyableCanvasLayer : public CanvasLayer
{
public:
  CopyableCanvasLayer(LayerManager* aLayerManager, void *aImplData);

protected:
  virtual ~CopyableCanvasLayer();

public:
  virtual void Initialize(const Data& aData) override;

  virtual bool IsDataValid(const Data& aData) override;

  bool IsGLLayer() { return !!mGLContext; }

protected:
  void UpdateTarget(gfx::DrawTarget* aDestTarget = nullptr);

  RefPtr<gfx::SourceSurface> mSurface;
  RefPtr<gl::GLContext> mGLContext;
  GLuint mCanvasFrontbufferTexID;
  RefPtr<PersistentBufferProvider> mBufferProvider;

  UniquePtr<gl::SharedSurface> mGLFrontbuffer;

  bool mIsAlphaPremultiplied;
  gl::OriginPos mOriginPos;
  bool mIsMirror;

  RefPtr<gfx::DataSourceSurface> mCachedTempSurface;

  gfx::DataSourceSurface* GetTempSurface(const gfx::IntSize& aSize,
                                         const gfx::SurfaceFormat aFormat);

  void DiscardTempSurface();
};

} // namespace layers
} // namespace mozilla

#endif
