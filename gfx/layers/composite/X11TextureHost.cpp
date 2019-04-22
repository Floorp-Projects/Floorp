/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "X11TextureHost.h"

#include "mozilla/layers/BasicCompositor.h"
#include "mozilla/layers/CompositorOGL.h"
#include "mozilla/layers/X11TextureSourceBasic.h"
#include "mozilla/layers/X11TextureSourceOGL.h"
#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "gfxXlibSurface.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

X11TextureHost::X11TextureHost(TextureFlags aFlags,
                               const SurfaceDescriptorX11& aDescriptor)
    : TextureHost(aFlags) {
  mSurface = aDescriptor.OpenForeign();

  if (mSurface && !(aFlags & TextureFlags::DEALLOCATE_CLIENT)) {
    mSurface->TakePixmap();
  }
}

bool X11TextureHost::Lock() {
  if (!mCompositor || !mSurface) {
    return false;
  }

  if (!mTextureSource) {
    switch (mCompositor->GetBackendType()) {
      case LayersBackend::LAYERS_BASIC:
        mTextureSource = new X11TextureSourceBasic(
            mCompositor->AsBasicCompositor(), mSurface);
        break;
      case LayersBackend::LAYERS_OPENGL:
        mTextureSource =
            new X11TextureSourceOGL(mCompositor->AsCompositorOGL(), mSurface);
        break;
      default:
        return false;
    }
  }

  return true;
}

void X11TextureHost::SetTextureSourceProvider(
    TextureSourceProvider* aProvider) {
  mProvider = aProvider;
  if (mProvider) {
    mCompositor = mProvider->AsCompositor();
  } else {
    mCompositor = nullptr;
  }
  if (mTextureSource) {
    mTextureSource->SetTextureSourceProvider(aProvider);
  }
}

SurfaceFormat X11TextureHost::GetFormat() const {
  if (!mSurface) {
    return SurfaceFormat::UNKNOWN;
  }
  gfxContentType type = mSurface->GetContentType();
  if (mCompositor->GetBackendType() == LayersBackend::LAYERS_OPENGL) {
    return X11TextureSourceOGL::ContentTypeToSurfaceFormat(type);
  }
  return X11TextureSourceBasic::ContentTypeToSurfaceFormat(type);
}

IntSize X11TextureHost::GetSize() const {
  if (!mSurface) {
    return IntSize();
  }
  return mSurface->GetSize();
}

already_AddRefed<gfx::DataSourceSurface> X11TextureHost::GetAsSurface() {
  if (!mTextureSource || !mTextureSource->AsSourceBasic()) {
    return nullptr;
  }
  RefPtr<DrawTarget> tempDT =
      gfxPlatform::GetPlatform()->CreateOffscreenContentDrawTarget(GetSize(),
                                                                   GetFormat());
  if (!tempDT) {
    return nullptr;
  }
  RefPtr<SourceSurface> surf =
      mTextureSource->AsSourceBasic()->GetSurface(tempDT);
  if (!surf) {
    return nullptr;
  }
  return surf->GetDataSurface();
}

}  // namespace layers
}  // namespace mozilla
