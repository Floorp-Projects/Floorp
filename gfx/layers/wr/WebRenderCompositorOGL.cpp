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
#include "mozilla/layers/TextureHostOGL.h"  // for TextureSourceOGL, etc

namespace mozilla {

using namespace gfx;
using namespace gl;

namespace layers {

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
WebRenderCompositorOGL::UpdateExternalImages()
{
  for (auto iter = mCompositableHosts.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<CompositableHost>& host = iter.Data();
    // XXX Change to correct TextrueSource handling here.
    host->BindTextureSource();
  }
}

void
WebRenderCompositorOGL::ScheduleComposition()
{
  MOZ_ASSERT(mCompositorBridge);
  mCompositorBridge->ScheduleComposition();
}

} // namespace layers
} // namespace mozilla
