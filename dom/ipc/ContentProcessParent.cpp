#include "ContentProcessParent.h"

#include "mozilla/ipc/GeckoThread.h"

#include "TabParent.h"
#include "mozilla/ipc/TestShellParent.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

ContentProcessParent* ContentProcessParent::gSingleton;

ContentProcessParent*
ContentProcessParent::GetSingleton()
{
  if (!gSingleton)
    gSingleton = new ContentProcessParent();

  return gSingleton;
}

TabParent*
ContentProcessParent::CreateTab(const MagicWindowHandle& hwnd)
{
  return static_cast<TabParent*>(SendIFrameEmbeddingConstructor(hwnd));
}

TestShellParent*
ContentProcessParent::CreateTestShell()
{
  return static_cast<TestShellParent*>(SendTestShellConstructor());
}

ContentProcessParent::ContentProcessParent()
    : mSubprocess(GeckoChildProcess_Tab)
{
    // TODO: async launching!
    mSubprocess.SyncLaunch();
    Open(mSubprocess.GetChannel());
}

ContentProcessParent::~ContentProcessParent()
{
}

IFrameEmbeddingProtocolParent*
ContentProcessParent::IFrameEmbeddingConstructor(const MagicWindowHandle& parentWidget)
{
    return new TabParent();
}

nsresult
ContentProcessParent::IFrameEmbeddingDestructor(IFrameEmbeddingProtocolParent* frame)
{
  delete frame;
  return NS_OK;
}

TestShellProtocolParent*
ContentProcessParent::TestShellConstructor()
{
  return new TestShellParent();
}

nsresult
ContentProcessParent::TestShellDestructor(TestShellProtocolParent* shell)
{
  delete shell;
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
