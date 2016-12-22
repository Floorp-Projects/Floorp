/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderCompositorOGL.h"

#include "CompositableHost.h"
#include "GLContext.h"                  // for GLContext
#include "GLUploadHelpers.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/TextureHost.h"  // for TextureSource, etc
#include "mozilla/layers/TextureHostWebRenderOGL.h"

#include <sstream>
#include <stdio.h>

namespace mozilla {

using namespace gfx;
using namespace gl;

namespace layers {

WRExternalImage LockExternalImage(void* aObj, WRExternalImageId aId)
{
  MOZ_ASSERT(aObj);
  WebRenderCompositorOGL* compositor = static_cast<WebRenderCompositorOGL*>(aObj);
  CompositableHost* compositable = compositor->mCompositableHosts.Get(aId.id).get();
  TextureSource* textureSource = compositable->BindTextureSource();

  // XXX: the textureSource should ready here.
  MOZ_RELEASE_ASSERT(textureSource);
  // XXX: the textureSource should be TextureImageTextureSourceWebRenderOGL.
  MOZ_RELEASE_ASSERT(textureSource->AsSourceWebRenderOGL());
  MOZ_RELEASE_ASSERT(textureSource->AsSourceWebRenderOGL()->AsTextureImageTextureSource());
  // XXX: the textureImage should be ready.
  MOZ_RELEASE_ASSERT(textureSource->AsSourceWebRenderOGL()->AsTextureImageTextureSource()->GetTextureImage());

  TextureImageTextureSourceWebRenderOGL* source = textureSource->AsSourceWebRenderOGL()->AsTextureImageTextureSource();
  gl::TextureImage* image = source->GetTextureImage();

#if 0
  // dump external image to file
  TextureHost* textureHost = compositable->GetAsTextureHost();
  RefPtr<DataSourceSurface> dSurf = textureHost->GetAsSurface();
  static int count = 0;
  ++count;

  std::stringstream sstream;
  sstream << "/tmp/img/" << count << '_' <<
      dSurf->GetSize().width << '_' << dSurf->GetSize().height << ".data";

  FILE *pWritingFile = fopen(sstream.str().c_str(), "wb+");
  if (pWritingFile) {
    printf_stderr("gecko write DT(%d,%d) to file",dSurf->GetSize().width,dSurf->GetSize().height);
    fwrite(dSurf->GetData(), dSurf->GetSize().width*dSurf->GetSize().height*4, 1, pWritingFile);
    fclose(pWritingFile);
  }
#endif

  if (image) {
    return WRExternalImage {
      TEXTURE_HANDLE,
      0.0f, 0.0f,
      static_cast<float>(image->GetSize().width), static_cast<float>(image->GetSize().height),
      image->GetTextureID()
    };
  }

  return WRExternalImage { TEXTURE_HANDLE, 0.0f, 0.0f, 0.0f, 0.0f, 0 };
}

void UnlockExternalImage(void* aObj, WRExternalImageId aId)
{
  MOZ_ASSERT(aObj);
  WebRenderCompositorOGL* compositor = static_cast<WebRenderCompositorOGL*>(aObj);
  CompositableHost* compositable = compositor->mCompositableHosts.Get(aId.id).get();
  compositable->UnbindTextureSource();
}

void ReleaseExternalImage(void* aObj, WRExternalImageId aId)
{
  MOZ_ASSERT(aObj);
  WebRenderCompositorOGL* compositor = static_cast<WebRenderCompositorOGL*>(aObj);
  compositor->RemoveExternalImageId(aId.id);
}

WebRenderCompositorOGL::WebRenderCompositorOGL(CompositorBridgeParent* aCompositorBridge,
                                               GLContext* aGLContext)
  : Compositor(nullptr, nullptr)
  , mCompositorBridge(aCompositorBridge)
  , mGLContext(aGLContext)
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

  mCompositableHosts.Clear();
  mCompositorBridge = nullptr;

  if (!mDestroyed) {
    mDestroyed = true;
    CleanupResources();
  }
}

void
WebRenderCompositorOGL::CleanupResources()
{
  if (!mGLContext) {
    return;
  }

  // On the main thread the Widget will be destroyed soon and calling MakeCurrent
  // after that could cause a crash (at least with GLX, see bug 1059793), unless
  // context is marked as destroyed.
  // There may be some textures still alive that will try to call MakeCurrent on
  // the context so let's make sure it is marked destroyed now.
  mGLContext->MarkDestroyed();

  mGLContext = nullptr;
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
  return MakeAndAddRef<TextureImageTextureSourceWebRenderOGL>(this, aFlags);
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

void
WebRenderCompositorOGL::CompositeUntil(TimeStamp aTimeStamp)
{
  Compositor::CompositeUntil(aTimeStamp);
  // We're not really taking advantage of the stored composite-again-time here.
  // We might be able to skip the next few composites altogether. However,
  // that's a bit complex to implement and we'll get most of the advantage
  // by skipping compositing when we detect there's nothing invalid. This is why
  // we do "composite until" rather than "composite again at".
  ScheduleComposition();
}

void
WebRenderCompositorOGL::AddExternalImageId(uint64_t aExternalImageId, CompositableHost* aHost)
{
  MOZ_ASSERT(!mCompositableHosts.Get(aExternalImageId));
  mCompositableHosts.Put(aExternalImageId, aHost);
}

void
WebRenderCompositorOGL::RemoveExternalImageId(uint64_t aExternalImageId)
{
  MOZ_ASSERT(mCompositableHosts.Get(aExternalImageId));
  mCompositableHosts.Remove(aExternalImageId);
}

void
WebRenderCompositorOGL::ScheduleComposition()
{
  MOZ_ASSERT(mCompositorBridge);
  mCompositorBridge->ScheduleComposition();
}

WRExternalImageHandler
WebRenderCompositorOGL::GetExternalImageHandler()
{
  return WRExternalImageHandler {
    this,
    LockExternalImage,
    UnlockExternalImage,
    ReleaseExternalImage,
  };
}

} // namespace layers
} // namespace mozilla
