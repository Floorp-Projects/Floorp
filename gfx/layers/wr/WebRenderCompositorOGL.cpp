/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCompositorOGL.h"

#include "GLContext.h"                  // for GLContext
#include "GLUploadHelpers.h"
#include "mozilla/layers/TextureHost.h"  // for TextureSource, etc
#include "mozilla/layers/TextureHostOGL.h"  // for TextureSourceOGL, etc

namespace mozilla {

using namespace gfx;
using namespace gl;

namespace layers {

WebRenderCompositorOGL::WebRenderCompositorOGL(GLContext* aGLContext)
  : Compositor(nullptr, nullptr)
  , mDestroyed(false)
{
  MOZ_COUNT_CTOR(WebRenderCompositorOGL);
}

WebRenderCompositorOGL::~WebRenderCompositorOGL()
{
  MOZ_COUNT_DTOR(WebRenderCompositorOGL);
  Destroy();
}

void
WebRenderCompositorOGL::Destroy()
{
  Compositor::Destroy();

  if (!mDestroyed) {
    mDestroyed = true;
    mGLContext = nullptr;;
  }
}

bool
WebRenderCompositorOGL::Initialize(nsCString* const out_failureReason)
{
  MOZ_ASSERT(mGLContext);
  return true;
}

already_AddRefed<DataTextureSource>
WebRenderCompositorOGL::CreateDataTextureSource(TextureFlags aFlags)
{
  return nullptr;
}

bool
WebRenderCompositorOGL::SupportsPartialTextureUpdate()
{
  return CanUploadSubTextures(mGLContext);
}

int32_t
WebRenderCompositorOGL::GetMaxTextureSize() const
{
  MOZ_ASSERT(mGLContext);
  GLint texSize = 0;
  mGLContext->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE,
                            &texSize);
  MOZ_ASSERT(texSize != 0);
  return texSize;
}

void
WebRenderCompositorOGL::MakeCurrent(MakeCurrentFlags aFlags) {
  if (mDestroyed) {
    NS_WARNING("Call on destroyed layer manager");
    return;
  }
  mGLContext->MakeCurrent(aFlags & ForceMakeCurrent);
}

} // namespace layers
} // namespace mozilla
