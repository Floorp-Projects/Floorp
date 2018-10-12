/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/plugins/PluginProcessParent.h"

#include "base/string_util.h"
#include "base/process_util.h"

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/plugins/PluginMessageUtils.h"
#include "mozilla/Telemetry.h"
#include "nsThreadUtils.h"

using std::vector;
using std::string;

using mozilla::ipc::BrowserProcessSubThread;
using mozilla::ipc::GeckoChildProcessHost;
using mozilla::plugins::LaunchCompleteTask;
using mozilla::plugins::PluginProcessParent;

#ifdef XP_WIN
PluginProcessParent::PidSet* PluginProcessParent::sPidSet = nullptr;
#endif

PluginProcessParent::PluginProcessParent(const std::string& aPluginFilePath) :
      GeckoChildProcessHost(GeckoProcessType_Plugin)
    , mPluginFilePath(aPluginFilePath)
    , mTaskFactory(this)
    , mMainMsgLoop(MessageLoop::current())
#ifdef XP_WIN
    , mChildPid(0)
#endif
{
}

PluginProcessParent::~PluginProcessParent()
{
#ifdef XP_WIN
    if (sPidSet && mChildPid) {
        sPidSet->RemoveEntry(mChildPid);
        if (sPidSet->IsEmpty()) {
            delete sPidSet;
            sPidSet = nullptr;
        }
    }
#endif
}

bool
PluginProcessParent::Launch(mozilla::UniquePtr<LaunchCompleteTask> aLaunchCompleteTask,
                            int32_t aSandboxLevel,
                            bool aIsSandboxLoggingEnabled)
{
#if (defined(XP_WIN) || defined(XP_MACOSX)) && defined(MOZ_SANDBOX)
    // At present, the Mac Flash plugin sandbox does not support different
    // levels and is enabled via a boolean pref or environment variable.
    // On Mac, when |aSandboxLevel| is positive, we enable the sandbox.
#if defined(XP_WIN)
    mSandboxLevel = aSandboxLevel;

    // The sandbox process sometimes needs read access to the plugin file.
    if (aSandboxLevel >= 3) {
        std::wstring pluginFile(NS_ConvertUTF8toUTF16(mPluginFilePath.c_str()).get());
        mAllowedFilesRead.push_back(pluginFile);
    }
#endif // XP_WIN
#else
    if (aSandboxLevel != 0) {
        MOZ_ASSERT(false,
                   "Can't enable an NPAPI process sandbox for platform/build.");
    }
#endif

    mLaunchCompleteTask = std::move(aLaunchCompleteTask);

    vector<string> args;
    args.push_back(MungePluginDsoPath(mPluginFilePath));

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
    if (aSandboxLevel > 0) {
        args.push_back("-flashSandboxLevel");
        args.push_back(std::to_string(aSandboxLevel));
        if (aIsSandboxLoggingEnabled) {
            args.push_back("-flashSandboxLogging");
        }
    }
#endif

    bool result = AsyncLaunch(args);
    if (!result) {
        mLaunchCompleteTask = nullptr;
    }
    return result;
}

void
PluginProcessParent::Delete()
{
  MessageLoop* currentLoop = MessageLoop::current();
  MessageLoop* ioLoop = XRE_GetIOMessageLoop();

  if (currentLoop == ioLoop) {
      delete this;
      return;
  }

  ioLoop->PostTask(
    NewNonOwningRunnableMethod("plugins::PluginProcessParent::Delete",
                               this,
                               &PluginProcessParent::Delete));
}

/**
 * This function exists so that we may provide an additional level of
 * indirection between the task being posted to main event loop (a
 * RunnableMethod) and the launch complete task itself. This is needed
 * for cases when both WaitUntilConnected or OnChannel* race to invoke the
 * task.
 */
void
PluginProcessParent::RunLaunchCompleteTask()
{
    if (mLaunchCompleteTask) {
        mLaunchCompleteTask->Run();
        mLaunchCompleteTask = nullptr;
    }
}

bool
PluginProcessParent::WaitUntilConnected(int32_t aTimeoutMs)
{
    bool result = GeckoChildProcessHost::WaitUntilConnected(aTimeoutMs);
    if (mLaunchCompleteTask) {
        if (result) {
            mLaunchCompleteTask->SetLaunchSucceeded();
        }
        RunLaunchCompleteTask();
    }
    return result;
}

void
PluginProcessParent::OnChannelConnected(int32_t peer_pid)
{
#ifdef XP_WIN
    mChildPid = static_cast<uint32_t>(peer_pid);
    if (!sPidSet) {
        sPidSet = new PluginProcessParent::PidSet();
    }
    sPidSet->PutEntry(mChildPid);
#endif

    GeckoChildProcessHost::OnChannelConnected(peer_pid);
}

void
PluginProcessParent::OnChannelError()
{
    GeckoChildProcessHost::OnChannelError();
}

bool
PluginProcessParent::IsConnected()
{
    mozilla::MonitorAutoLock lock(mMonitor);
    return mProcessState == PROCESS_CONNECTED;
}

bool
PluginProcessParent::IsPluginProcessId(base::ProcessId procId) {
#ifdef XP_WIN
    MOZ_ASSERT(XRE_IsParentProcess());
    return sPidSet && sPidSet->Contains(static_cast<uint32_t>(procId));
#else
    NS_ERROR("IsPluginProcessId not available on this platform.");
    return false;
#endif
}
