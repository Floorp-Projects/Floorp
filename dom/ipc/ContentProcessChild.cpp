/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#include "ContentProcessChild.h"
#include "TabChild.h"

#include "mozilla/ipc/TestShellChild.h"

#include "nsXULAppAPI.h"

using namespace mozilla::ipc;

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
    IFrameEmbeddingProtocolChild* iframe = new TabChild(hwnd);
    if (iframe && mIFrames.AppendElement(iframe)) {
        return iframe;
    }
    delete iframe;
    return nsnull;
}

nsresult
ContentProcessChild::IFrameEmbeddingDestructor(IFrameEmbeddingProtocolChild* iframe)
{
    mIFrames.RemoveElement(iframe);
    return NS_OK;
}

TestShellProtocolChild*
ContentProcessChild::TestShellConstructor()
{
    TestShellProtocolChild* testshell = new TestShellChild();
    if (testshell && mTestShells.AppendElement(testshell)) {
        return testshell;
    }
    delete testshell;
    return nsnull;
}

nsresult
ContentProcessChild::TestShellDestructor(TestShellProtocolChild* shell)
{
    mTestShells.RemoveElement(shell);
    return NS_OK;
}

void
ContentProcessChild::Quit()
{
    mIFrames.Clear();
    mTestShells.Clear();

    XRE_ShutdownChildProcess();
}

} // namespace dom
} // namespace mozilla
