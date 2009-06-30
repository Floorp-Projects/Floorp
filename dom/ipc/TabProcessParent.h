/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#ifndef mozilla_tabs_TabProcessParent_h
#define mozilla_tabs_TabProcessParent_h

#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace tabs {

class TabProcessParent
    : private mozilla::ipc::GeckoChildProcessHost
{
public:
    TabProcessParent();
    ~TabProcessParent();

    /**
     * Asynchronously launch the plugin process.
     */
    bool Launch();

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
    static char const *const kTabProcessName;

    DISALLOW_EVIL_CONSTRUCTORS(TabProcessParent);
};

}
}

#endif
