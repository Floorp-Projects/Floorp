/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 */
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
#include "chrome/common/process_watcher.h"

#include "prprf.h"

#if defined(OS_LINUX)
#  define XP_LINUX 1
#endif
#include "nsExceptionHandler.h"

#include "mozilla/ipc/BrowserProcessSubThread.h"

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
    mChannelInitialized(false),
    mDelegate(aDelegate),
    mChildProcessHandle(0)
{
    MOZ_COUNT_CTOR(GeckoChildProcessHost);
    
    MessageLoop* ioLoop = 
      BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO);
    ioLoop->PostTask(FROM_HERE,
                     NewRunnableMethod(this,
                                       &GeckoChildProcessHost::InitializeChannel));
}

GeckoChildProcessHost::~GeckoChildProcessHost()

{
  AssertIOThread();

  MOZ_COUNT_DTOR(GeckoChildProcessHost);

  if (mChildProcessHandle > 0)
    ProcessWatcher::EnsureProcessTerminated(mChildProcessHandle
#if defined(NS_BUILD_REFCNT_LOGGING)
                                            , false // don't "force"
#endif
    );
}

bool
GeckoChildProcessHost::SyncLaunch(std::vector<std::string> aExtraOpts)
{
  MessageLoop* ioLoop = 
    BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO);
  NS_ASSERTION(MessageLoop::current() != ioLoop, "sync launch from the IO thread NYI");

  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this,
                                     &GeckoChildProcessHost::PerformAsyncLaunch,
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
GeckoChildProcessHost::AsyncLaunch(std::vector<std::string> aExtraOpts)
{
  MessageLoop* ioLoop = 
    BrowserProcessSubThread::GetMessageLoop(BrowserProcessSubThread::IO);
  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this,
                                     &GeckoChildProcessHost::PerformAsyncLaunch,
                                     aExtraOpts));

  // This may look like the sync launch wait, but we only delay as
  // long as it takes to create the channel.
  MonitorAutoEnter mon(mMonitor);
  while (!mChannelInitialized) {
    mon.Wait();
  }

  return true;
}

void
GeckoChildProcessHost::InitializeChannel()
{
  CreateChannel();

  MonitorAutoEnter mon(mMonitor);
  mChannelInitialized = true;
  mon.Notify();
}

bool
GeckoChildProcessHost::PerformAsyncLaunch(std::vector<std::string> aExtraOpts)
{
  // FIXME/cjones: make this work from non-IO threads, too

  // We rely on the fact that InitializeChannel() has already been processed
  // on the IO thread before this point is reached.
  if (!GetChannel()) {
    return false;
  }

  base::ProcessHandle process;

  // send the child the PID so that it can open a ProcessHandle back to us.
  // probably don't want to do this in the long run
  char pidstring[32];
  PR_snprintf(pidstring, sizeof(pidstring) - 1,
	      "%ld", base::Process::Current().pid());

  const char* const childProcessType =
      XRE_ChildProcessTypeToString(mProcessType);

//--------------------------------------------------
#if defined(OS_POSIX)
  // For POSIX, we have to be extremely anal about *not* using
  // std::wstring in code compiled with Mozilla's -fshort-wchar
  // configuration, because chromium is compiled with -fno-short-wchar
  // and passing wstrings from one config to the other is unsafe.  So
  // we split the logic here.

  FilePath exePath = FilePath(CommandLine::ForCurrentProcess()->argv()[0]);
  exePath = exePath.DirName();
  exePath = exePath.AppendASCII(MOZ_CHILD_PROCESS_NAME);

  // remap the IPC socket fd to a well-known int, as the OS does for
  // STDOUT_FILENO, for example
  int srcChannelFd, dstChannelFd;
  channel().GetClientFileDescriptorMapping(&srcChannelFd, &dstChannelFd);
  mFileMap.push_back(std::pair<int,int>(srcChannelFd, dstChannelFd));

  // no need for kProcessChannelID, the child process inherits the
  // other end of the socketpair() from us

  std::vector<std::string> childArgv;

  childArgv.push_back(exePath.value());

  childArgv.insert(childArgv.end(), aExtraOpts.begin(), aExtraOpts.end());

  childArgv.push_back(pidstring);
  childArgv.push_back(childProcessType);

#if defined(MOZ_CRASHREPORTER)
  int childCrashFd, childCrashRemapFd;
  if (!CrashReporter::CreateNotificationPipeForChild(
        &childCrashFd, &childCrashRemapFd))
    return false;
  if (0 <= childCrashFd) {
    mFileMap.push_back(std::pair<int,int>(childCrashFd, childCrashRemapFd));
    // "true" == crash reporting enabled
    childArgv.push_back("true");
  }
  else {
    // "false" == crash reporting disabled
    childArgv.push_back("false");
  }
#endif

  base::LaunchApp(childArgv, mFileMap, false, &process);

//--------------------------------------------------
#elif defined(OS_WIN)

  FilePath exePath =
    FilePath::FromWStringHack(CommandLine::ForCurrentProcess()->program());
  exePath = exePath.DirName();

  exePath = exePath.AppendASCII(MOZ_CHILD_PROCESS_NAME);

  CommandLine cmdLine(exePath.ToWStringHack());
  cmdLine.AppendSwitchWithValue(switches::kProcessChannelID, channel_id());

  for (std::vector<std::string>::iterator it = aExtraOpts.begin();
       it != aExtraOpts.end();
       ++it) {
      cmdLine.AppendLooseValue(UTF8ToWide(*it));
  }

  cmdLine.AppendLooseValue(UTF8ToWide(pidstring));
  cmdLine.AppendLooseValue(UTF8ToWide(childProcessType));
#if defined(MOZ_CRASHREPORTER)
  cmdLine.AppendLooseValue(
    UTF8ToWide(CrashReporter::GetChildNotificationPipe()));
#endif

  base::LaunchApp(cmdLine, false, false, &process);

#else
#  error Sorry
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

  if (!base::OpenProcessHandle(peer_pid, &mChildProcessHandle))
      NS_RUNTIMEABORT("can't open handle to child process");

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
