/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TexturePoolOGL.h"
#include <stdlib.h>                     // for malloc
#include "GLContext.h"                  // for GLContext
#include "mozilla/Monitor.h"            // for Monitor, MonitorAutoLock
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "nsDebug.h"                    // for NS_ASSERTION, NS_ERROR, etc
#include "nsDeque.h"                    // for nsDeque

#define TEXTURE_POOL_SIZE 10

namespace mozilla {
namespace gl {

static GLContext* sActiveContext = nullptr;

static Monitor* sMonitor = nullptr;
static nsDeque* sTextures = nullptr;

GLuint TexturePoolOGL::AcquireTexture()
{
  NS_ASSERTION(sMonitor, "not initialized");

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
  NS_ASSERTION(aContext, "NULL GLContext");
  NS_ASSERTION(sMonitor, "not initialized");

  MonitorAutoLock lock(*sMonitor);

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

  sMonitor->NotifyAll();
}

GLContext* TexturePoolOGL::GetGLContext()
{
  return sActiveContext;
}

void TexturePoolOGL::Init()
{
  sMonitor = new Monitor("TexturePoolOGL.sMonitor");
  sTextures = new nsDeque();
}

void TexturePoolOGL::Shutdown()
{
  delete sMonitor;
  delete sTextures;
}

} // namespace gl
} // namespace mozilla
