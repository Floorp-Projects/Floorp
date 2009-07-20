#include "TabParent.h"

#include "mozilla/ipc/GeckoThread.h"

#include "nsIURI.h"

using mozilla::ipc::BrowserProcessSubThread;

namespace mozilla {
namespace tabs {

TabParent::TabParent(MagicWindowHandle parentWidget)
    : mSubprocess(GeckoChildProcess_Tab)
{
    mSubprocess.SyncLaunch();
    Open(mSubprocess.GetChannel());

    Sendinit(parentWidget);
}

TabParent::~TabParent()
{
}

void
TabParent::LoadURL(nsIURI* aURI)
{
    nsCString spec;
    aURI->GetSpec(spec);

    SendloadURL(spec.get());
}

void
TabParent::Move(PRUint32 x, PRUint32 y, PRUint32 width, PRUint32 height)
{
    Sendmove(x, y, width, height);
}

} // namespace tabs
} // namespace mozilla
