/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#ifndef mozilla_tabs_TabProcessParent_h
#define mozilla_tabs_TabProcessParent_h

#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace tabs {

class TabProcessParent
    : public mozilla::ipc::GeckoChildProcessHost
{
public:
    TabProcessParent();
    ~TabProcessParent();

    IPC::Channel* GetChannel() {
        return channelp();
    }

    virtual bool CanShutdown() {
        return true;
    }

    base::WaitableEvent* GetShutDownEvent() {
        return GetProcessEvent();
    }

private:
    DISALLOW_EVIL_CONSTRUCTORS(TabProcessParent);
};

}
}

#endif
