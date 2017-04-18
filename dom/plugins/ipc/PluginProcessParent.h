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
#include "base/task.h"
#include "base/thread.h"
#include "chrome/common/child_process_host.h"

#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "mozilla/ipc/TaskFactory.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"

namespace mozilla {
namespace plugins {

class LaunchCompleteTask : public Runnable
{
public:
  LaunchCompleteTask()
    : Runnable("plugins::LaunchCompleteTask")
    , mLaunchSucceeded(false)
  {
    }

    void SetLaunchSucceeded() { mLaunchSucceeded = true; }

protected:
    bool mLaunchSucceeded;
};

class PluginProcessParent : public mozilla::ipc::GeckoChildProcessHost
{
public:
    explicit PluginProcessParent(const std::string& aPluginFilePath);
    ~PluginProcessParent();

    /**
     * Launch the plugin process. If the process fails to launch,
     * this method will return false.
     *
     * @param aLaunchCompleteTask Task that is executed on the main
     * thread once the asynchonous launch has completed.
     * @param aSandboxLevel Determines the strength of the sandbox.
     * <= 0 means no sandbox.
     */
    bool Launch(UniquePtr<LaunchCompleteTask> aLaunchCompleteTask = UniquePtr<LaunchCompleteTask>(),
                int32_t aSandboxLevel = 0);

    void Delete();

    virtual bool CanShutdown() override
    {
        return true;
    }

    const std::string& GetPluginFilePath() { return mPluginFilePath; }

    using mozilla::ipc::GeckoChildProcessHost::GetChannel;

    void SetCallRunnableImmediately();
    virtual bool WaitUntilConnected(int32_t aTimeoutMs = 0) override;

    virtual void OnChannelConnected(int32_t peer_pid) override;
    virtual void OnChannelError() override;

    bool IsConnected();

    static bool IsPluginProcessId(base::ProcessId procId);

private:
    void RunLaunchCompleteTask();

    std::string mPluginFilePath;
    ipc::TaskFactory<PluginProcessParent> mTaskFactory;
    UniquePtr<LaunchCompleteTask> mLaunchCompleteTask;
    MessageLoop* mMainMsgLoop;
    bool mRunCompleteTaskImmediately;
#ifdef XP_WIN
    typedef nsTHashtable<nsUint32HashKey> PidSet;
    // Set of PIDs for all plugin child processes or NULL if empty.
    static PidSet* sPidSet;
    uint32_t mChildPid;
#endif

    DISALLOW_EVIL_CONSTRUCTORS(PluginProcessParent);
};


} // namespace plugins
} // namespace mozilla

#endif // ifndef dom_plugins_PluginProcessParent_h
