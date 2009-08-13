/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#include "ContentProcessChild.h"
#include "TabChild.h"

namespace mozilla {
namespace dom {

ContentProcessChild* ContentProcessChild::sSingleton;

ContentProcessChild::ContentProcessChild()
{
}

ContentProcessChild::~ContentProcessChild()
{
}

bool
ContentProcessChild::Init(MessageLoop* aIOLoop, IPC::Channel* aChannel)
{
    NS_ASSERTION(!sSingleton, "only one ContentProcessChild per child");
  
    Open(aChannel, aIOLoop);
    sSingleton = this;

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
    return NS_OK;
}

} // namespace dom
} // namespace mozilla
