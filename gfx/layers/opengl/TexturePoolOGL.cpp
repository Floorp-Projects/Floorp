/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TexturePoolOGL.h"
#include <stdlib.h>                     // for malloc
#include "GLContext.h"                  // for GLContext
#include "mozilla/Logging.h"
#include "mozilla/Monitor.h"            // for Monitor, MonitorAutoLock
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "mozilla/layers/CompositorThread.h"
#include "nsDebug.h"                    // for NS_ASSERTION, NS_ERROR, etc
#include "nsDeque.h"                    // for nsDeque
#include "nsThreadUtils.h"

static const unsigned int TEXTURE_POOL_SIZE = 10;
static const unsigned int TEXTURE_REFILL_THRESHOLD = TEXTURE_POOL_SIZE / 2;

static mozilla::LazyLogModule gTexturePoolLog("TexturePoolOGL");
#define LOG(arg, ...) MOZ_LOG(gTexturePoolLog, mozilla::LogLevel::Debug, ("TexturePoolOGL::%s: " arg, __func__, ##__VA_ARGS__))

namespace mozilla {
namespace gl {

static GLContext* sActiveContext = nullptr;

static Monitor* sMonitor = nullptr;
static nsDeque* sTextures = nullptr;

enum class PoolState : uint8_t {
  NOT_INITIALIZE,
  INITIALIZED,
  SHUTDOWN
};
static PoolState sPoolState = PoolState::NOT_INITIALIZE;

static bool sHasPendingFillTask = false;

void TexturePoolOGL::MaybeFillTextures()
{
  if (sTextures->GetSize() < TEXTURE_REFILL_THRESHOLD &&
      !sHasPendingFillTask) {
    LOG("need to refill the texture pool.");
    sHasPendingFillTask = true;
    MessageLoop* loop = mozilla::layers::CompositorThreadHolder::Loop();
    MOZ_ASSERT(loop);
    loop->PostTask(
      NS_NewRunnableFunction(
        "TexturePoolOGL::MaybeFillTextures",
        [] () {
          TexturePoolOGL::Fill(sActiveContext);
    }));
  }
}

GLuint TexturePoolOGL::AcquireTexture()
{
  MOZ_ASSERT(sPoolState != PoolState::NOT_INITIALIZE, "not initialized");
  MOZ_ASSERT(sPoolState != PoolState::SHUTDOWN, "should not be called after shutdown");

  MonitorAutoLock lock(*sMonitor);

  if (!sActiveContext) {
    // Wait for a context
    sMonitor->Wait();

    if (!sActiveContext)
      return 0;
  }

  GLuint texture = 0;
  if (sActiveContext->IsOwningThreadCurrent()) {
    sActiveContext->MakeCurrent();

    sActiveContext->fGenTextures(1, &texture);
  } else {
    while (sTextures->GetSize() == 0) {
      NS_WARNING("Waiting for texture");
      sMonitor->Wait();
    }

    GLuint* popped = (GLuint*) sTextures->Pop();
    if (!popped) {
      NS_ERROR("Failed to pop texture pool item");
      return 0;
    }

    texture = *popped;
    delete popped;

    NS_ASSERTION(texture, "Failed to retrieve texture from pool");

    MaybeFillTextures();
    LOG("remaining textures num = %zu", sTextures->GetSize());
  }

  return texture;
}

static void Clear()
{
  const bool isCurrent = sActiveContext && sActiveContext->MakeCurrent();

  GLuint* item;
  while (sTextures->GetSize()) {
    item = (GLuint*)sTextures->Pop();
    if (isCurrent) {
      sActiveContext->fDeleteTextures(1, item);
    }
    delete item;
  }
}

void TexturePoolOGL::Fill(GLContext* aContext)
{
  MOZ_ASSERT(aContext, "NULL GLContext");
  MOZ_ASSERT(sPoolState != PoolState::NOT_INITIALIZE, "not initialized");
  MOZ_ASSERT(sPoolState != PoolState::SHUTDOWN, "should not be called after shutdown");

  MonitorAutoLock lock(*sMonitor);
  sHasPendingFillTask = false;

  if (sActiveContext != aContext) {
    Clear();
    sActiveContext = aContext;
  }

  if (sTextures->GetSize() == TEXTURE_POOL_SIZE)
    return;

  DebugOnly<bool> ok = sActiveContext->MakeCurrent();
  MOZ_ASSERT(ok);

  GLuint* texture = nullptr;
  while (sTextures->GetSize() < TEXTURE_POOL_SIZE) {
    texture = (GLuint*)malloc(sizeof(GLuint));
    sActiveContext->fGenTextures(1, texture);
    sTextures->Push((void*) texture);
  }

  LOG("fill texture pool to %d", TEXTURE_POOL_SIZE);
  sMonitor->NotifyAll();
}

GLContext* TexturePoolOGL::GetGLContext()
{
  return sActiveContext;
}

void TexturePoolOGL::Init()
{
  MOZ_ASSERT(sPoolState != PoolState::INITIALIZED);
  sMonitor = new Monitor("TexturePoolOGL.sMonitor");
  sTextures = new nsDeque();

  sPoolState = PoolState::INITIALIZED;
}

void TexturePoolOGL::Shutdown()
{
  MOZ_ASSERT(sPoolState == PoolState::INITIALIZED);
  sPoolState = PoolState::SHUTDOWN;
  delete sMonitor;
  delete sTextures;
}

} // namespace gl
} // namespace mozilla
