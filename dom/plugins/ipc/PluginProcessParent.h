/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_plugins_PluginProcessParent_h
#define dom_plugins_PluginProcessParent_h 1

#include "mozilla/Attributes.h"
#include "base/basictypes.h"

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "chrome/common/child_process_host.h"

#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace plugins {
//-----------------------------------------------------------------------------

class PluginProcessParent : public mozilla::ipc::GeckoChildProcessHost
{
public:
    PluginProcessParent(const std::string& aPluginFilePath);
    ~PluginProcessParent();

    /**
     * Synchronously launch the plugin process. If the process fails to launch
     * after timeoutMs, this method will return false.
     */
    bool Launch(int32_t timeoutMs);

    void Delete();

    virtual bool CanShutdown() MOZ_OVERRIDE
    {
        return true;
    }

    const std::string& GetPluginFilePath() { return mPluginFilePath; }

    using mozilla::ipc::GeckoChildProcessHost::GetShutDownEvent;
    using mozilla::ipc::GeckoChildProcessHost::GetChannel;
    using mozilla::ipc::GeckoChildProcessHost::GetChildProcessHandle;

private:
    std::string mPluginFilePath;

    DISALLOW_EVIL_CONSTRUCTORS(PluginProcessParent);
};


} // namespace plugins
} // namespace mozilla

#endif // ifndef dom_plugins_PluginProcessParent_h
