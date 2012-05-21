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

using std::vector;
using std::string;

using mozilla::ipc::BrowserProcessSubThread;
using mozilla::ipc::GeckoChildProcessHost;
using mozilla::plugins::PluginProcessParent;
using base::ProcessArchitecture;

template<>
struct RunnableMethodTraits<PluginProcessParent>
{
    static void RetainCallee(PluginProcessParent* obj) { }
    static void ReleaseCallee(PluginProcessParent* obj) { }
};

PluginProcessParent::PluginProcessParent(const std::string& aPluginFilePath) :
    GeckoChildProcessHost(GeckoProcessType_Plugin),
    mPluginFilePath(aPluginFilePath)
{
}

PluginProcessParent::~PluginProcessParent()
{
}

bool
PluginProcessParent::Launch(PRInt32 timeoutMs)
{
    ProcessArchitecture currentArchitecture = base::GetCurrentProcessArchitecture();
    uint32 containerArchitectures = GetSupportedArchitecturesForProcessType(GeckoProcessType_Plugin);

    uint32 pluginLibArchitectures = currentArchitecture;
#ifdef XP_MACOSX
    nsresult rv = GetArchitecturesForBinary(mPluginFilePath.c_str(), &pluginLibArchitectures);
    if (NS_FAILED(rv)) {
        // If the call failed just assume that we want the current architecture.
        pluginLibArchitectures = currentArchitecture;
    }
#endif

    ProcessArchitecture selectedArchitecture = currentArchitecture;
    if (!(pluginLibArchitectures & containerArchitectures & currentArchitecture)) {
        // Prefererence in order: x86_64, i386, PPC. The only particularly important thing
        // about this order is that we'll prefer 64-bit architectures first.
        if (base::PROCESS_ARCH_X86_64 & pluginLibArchitectures & containerArchitectures) {
            selectedArchitecture = base::PROCESS_ARCH_X86_64;
        }
        else if (base::PROCESS_ARCH_I386 & pluginLibArchitectures & containerArchitectures) {
            selectedArchitecture = base::PROCESS_ARCH_I386;
        }
        else if (base::PROCESS_ARCH_PPC & pluginLibArchitectures & containerArchitectures) {
            selectedArchitecture = base::PROCESS_ARCH_PPC;
        }
        else if (base::PROCESS_ARCH_ARM & pluginLibArchitectures & containerArchitectures) {
          selectedArchitecture = base::PROCESS_ARCH_ARM;
        }
        else {
            return false;
        }
    }

    vector<string> args;
    args.push_back(MungePluginDsoPath(mPluginFilePath));
    return SyncLaunch(args, timeoutMs, selectedArchitecture);
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

  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this, &PluginProcessParent::Delete));
}
