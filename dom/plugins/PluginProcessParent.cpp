/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
