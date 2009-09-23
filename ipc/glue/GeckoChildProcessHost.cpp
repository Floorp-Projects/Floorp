/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "GeckoChildProcessHost.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"

#include "mozilla/ipc/GeckoThread.h"

using mozilla::MonitorAutoEnter;
using mozilla::ipc::GeckoChildProcessHost;

template<>
struct RunnableMethodTraits<GeckoChildProcessHost>
{
    static void RetainCallee(GeckoChildProcessHost* obj) { }
    static void ReleaseCallee(GeckoChildProcessHost* obj) { }
};

GeckoChildProcessHost::GeckoChildProcessHost(GeckoProcessType aProcessType,
                                             base::WaitableEventWatcher::Delegate* aDelegate)
  : ChildProcessHost(RENDER_PROCESS), // FIXME/cjones: we should own this enum
    mProcessType(aProcessType),
    mMonitor("mozilla.ipc.GeckChildProcessHost.mMonitor"),
    mLaunched(false),
    mDelegate(aDelegate)
{
}

bool
GeckoChildProcessHost::SyncLaunch(std::vector<std::wstring> aExtraOpts)
{
  MessageLoop* loop = MessageLoop::current();
  MessageLoop* ioLoop = 
    BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO);
  NS_ASSERTION(loop != ioLoop, "sync launch from the IO thread NYI");

  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this,
                                     &GeckoChildProcessHost::AsyncLaunch,
                                     aExtraOpts));

  // NB: this uses a different mechanism than the chromium parent
  // class.
  MonitorAutoEnter mon(mMonitor);
  while (!mLaunched) {
    mon.Wait();
  }

  return true;
}

bool
GeckoChildProcessHost::AsyncLaunch(std::vector<std::wstring> aExtraOpts)
{
  // FIXME/cjones: make this work from non-IO threads, too

  if (!CreateChannel()) {
    return false;
  }

  FilePath exePath =
    FilePath::FromWStringHack(CommandLine::ForCurrentProcess()->program());
  exePath = exePath.DirName();

  exePath = exePath.AppendASCII(MOZ_CHILD_PROCESS_NAME);

  // remap the IPC socket fd to a well-known int, as the OS does for
  // STDOUT_FILENO, for example
#if defined(OS_POSIX)
  int srcChannelFd, dstChannelFd;
  channel().GetClientFileDescriptorMapping(&srcChannelFd, &dstChannelFd);
  mFileMap.push_back(std::pair<int,int>(srcChannelFd, dstChannelFd));
#endif

  CommandLine cmdLine(exePath.ToWStringHack());
  cmdLine.AppendSwitchWithValue(switches::kProcessChannelID, channel_id());

  for (std::vector<std::wstring>::iterator it = aExtraOpts.begin();
       it != aExtraOpts.end();
       ++it) {
    cmdLine.AppendLooseValue((*it).c_str());
  }

  cmdLine.AppendLooseValue(UTF8ToWide(XRE_ChildProcessTypeToString(mProcessType)));

  base::ProcessHandle process;
#if defined(OS_WIN)
  base::LaunchApp(cmdLine, false, false, &process);
#elif defined(OS_POSIX)
  base::LaunchApp(cmdLine.argv(), mFileMap, false, &process);
#else
#error Bad!
#endif

  if (!process) {
    return false;
  }
  SetHandle(process);

  return true;
}

void
GeckoChildProcessHost::OnChannelConnected(int32 peer_pid)
{
  MonitorAutoEnter mon(mMonitor);
  mLaunched = true;
  mon.Notify();
}

// XXX/cjones: these next two methods should basically never be called.
// after the process is launched, its channel will be used to create
// one of our channels, AsyncChannel et al.
void
GeckoChildProcessHost::OnMessageReceived(const IPC::Message& aMsg)
{
}
void
GeckoChildProcessHost::OnChannelError()
{
  // XXXbent Notify that the child process is gone?
}

void
GeckoChildProcessHost::OnWaitableEventSignaled(base::WaitableEvent *event)
{
  if (mDelegate) {
    mDelegate->OnWaitableEventSignaled(event);
  }
  ChildProcessHost::OnWaitableEventSignaled(event);
}
