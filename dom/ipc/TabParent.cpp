#include "TabParent.h"

#include "mozilla/ipc/GeckoThread.h"

#include "nsIURI.h"

using mozilla::ipc::BrowserProcessSubThread;

template<>
struct RunnableMethodTraits<mozilla::tabs::TabParent>
{
    static void RetainCallee(mozilla::tabs::TabParent* obj) { }
    static void ReleaseCallee(mozilla::tabs::TabParent* obj) { }
};

namespace mozilla {
namespace tabs {

TabParent::TabParent(MagicWindowHandle parentWidget)
    : mSubprocess()
    , mMonitor("mozilla.dom.ipc.TabParent")
{
    {
        MonitorAutoEnter mon(mMonitor);
        BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO)->
            PostTask(FROM_HERE, NewRunnableMethod(this, &TabParent::LaunchSubprocess));
        mon.Wait();
    }

    Open(mSubprocess.GetChannel());

    Callinit(parentWidget);
}

TabParent::~TabParent()
{
}

void
TabParent::LaunchSubprocess()
{
    MonitorAutoEnter mon(mMonitor);
    mSubprocess.Launch();
    mon.Notify();
}

void
TabParent::LoadURL(nsIURI* aURI)
{
    nsCString spec;
    aURI->GetSpec(spec);

    CallloadURL(spec.get());
}

void
TabParent::Move(PRUint32 x, PRUint32 y, PRUint32 width, PRUint32 height)
{
    Callmove(x, y, width, height);
}

} // namespace tabs
} // namespace mozilla
