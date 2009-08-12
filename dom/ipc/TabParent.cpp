#include "TabParent.h"

#include "mozilla/ipc/GeckoThread.h"

#include "nsIURI.h"

using mozilla::ipc::BrowserProcessSubThread;

namespace mozilla {
namespace dom {

TabParent::TabParent()
{
}

TabParent::~TabParent()
{
}

void
TabParent::LoadURL(nsIURI* aURI)
{
    nsCString spec;
    aURI->GetSpec(spec);

    SendloadURL(spec);
}

void
TabParent::Move(PRUint32 x, PRUint32 y, PRUint32 width, PRUint32 height)
{
    Sendmove(x, y, width, height);
}

} // namespace tabs
} // namespace mozilla
