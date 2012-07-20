/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TexturePoolOGL.h"
#include "GLContext.h"
#include "nsDeque.h"
#include "mozilla/Monitor.h"

#define TEXTURE_POOL_SIZE 10

namespace mozilla {
namespace gl {

static GLContext* sActiveContext = NULL;

static Monitor* sMonitor = NULL;
static nsDeque* sTextures = NULL;

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
  if (!sActiveContext)
    return;

  sActiveContext->MakeCurrent();

  GLuint* item;
  while (sTextures->GetSize()) {
    item = (GLuint*)sTextures->Pop();
    sActiveContext->fDeleteTextures(1, item);
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

  sActiveContext->MakeCurrent();

  GLuint* texture = NULL;
  while (sTextures->GetSize() < TEXTURE_POOL_SIZE) {
    texture = (GLuint*)malloc(sizeof(GLuint));
    sActiveContext->fGenTextures(1, texture);
    sTextures->Push((void*) texture);
  }

  sMonitor->NotifyAll();
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

} // gl
} // mozilla
