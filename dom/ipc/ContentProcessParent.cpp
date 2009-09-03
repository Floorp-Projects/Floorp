#include "ContentProcessParent.h"

#include "mozilla/ipc/GeckoThread.h"

#include "TabParent.h"
#include "mozilla/ipc/TestShellParent.h"

#include "nsIObserverService.h"

#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;
using mozilla::MonitorAutoEnter;

namespace {
PRBool gSingletonDied = PR_FALSE;
}

namespace mozilla {
namespace dom {

ContentProcessParent* ContentProcessParent::gSingleton;

ContentProcessParent*
ContentProcessParent::GetSingleton()
{
    if (!gSingleton && !gSingletonDied) {
        nsRefPtr<ContentProcessParent> parent = new ContentProcessParent();
        if (parent) {
            nsCOMPtr<nsIObserverService> obs =
                do_GetService("@mozilla.org/observer-service;1");
            if (obs) {
                if (NS_SUCCEEDED(obs->AddObserver(parent, "xpcom-shutdown",
                                                  PR_FALSE))) {
                    gSingleton = parent;
                }
            }
        }
    }
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
    : mMonitor("ContentProcessParent::mMonitor")
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    // TODO: async launching!
    mSubprocess = new GeckoChildProcessHost(GeckoProcessType_Content, this);
    mSubprocess->SyncLaunch();
    Open(mSubprocess->GetChannel());
}

ContentProcessParent::~ContentProcessParent()
{
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    NS_ASSERTION(gSingleton == this, "More than one singleton?!");
    gSingletonDied = PR_TRUE;
    gSingleton = nsnull;
}

NS_IMPL_ISUPPORTS1(ContentProcessParent, nsIObserver)

NS_IMETHODIMP
ContentProcessParent::Observe(nsISupports* aSubject,
                              const char* aTopic,
                              const PRUnichar* aData)
{
    if (!strcmp(aTopic, "xpcom-shutdown") && mSubprocess) {
        SendQuit();
#ifdef OS_WIN
        MonitorAutoEnter mon(mMonitor);
        while (mSubprocess) {
            mon.Wait();
        }
#endif
    }
    return NS_OK;
}

void
ContentProcessParent::OnWaitableEventSignaled(base::WaitableEvent *event)
{
    // The child process has died! Sadly we're on the wrong thread to do much.
    NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
    MonitorAutoEnter mon(mMonitor);
    mSubprocess = nsnull;
    mon.Notify();
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
