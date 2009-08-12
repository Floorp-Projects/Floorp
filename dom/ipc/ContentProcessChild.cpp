/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8; -*- */

#include "ContentProcessChild.h"
#include "TabChild.h"

namespace mozilla {
namespace dom {

ContentProcessChild::ContentProcessChild()
{
}

ContentProcessChild::~ContentProcessChild()
{
}

bool
ContentProcessChild::Init(MessageLoop* aIOLoop, IPC::Channel* aChannel)
{
  Open(aChannel, aIOLoop);
  return true;
}

IFrameEmbeddingProtocolChild*
ContentProcessChild::IFrameEmbeddingConstructor(const MagicWindowHandle& hwnd)
{
  return new TabChild(hwnd);
}

nsresult
ContentProcessChild::IFrameEmbeddingDestructor(IFrameEmbeddingProtocolChild* iframe)
{
  delete iframe;
}

} // namespace dom
} // namespace mozilla
