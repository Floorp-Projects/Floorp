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

#ifndef __IPC_GLUE_GECKOCHILDPROCESSHOST_H__
#define __IPC_GLUE_GECKOCHILDPROCESSHOST_H__

#include "base/file_path.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/waitable_event.h"
#include "chrome/common/child_process_host.h"

#include "mozilla/Monitor.h"

#include "nsXULAppAPI.h"        // for GeckoProcessType
#include "nsString.h"

namespace mozilla {
namespace ipc {

class GeckoChildProcessHost : public ChildProcessHost
{
protected:
  typedef mozilla::Monitor Monitor;

public:
  typedef base::ProcessHandle ProcessHandle;

  GeckoChildProcessHost(GeckoProcessType aProcessType=GeckoProcessType_Default,
                        base::WaitableEventWatcher::Delegate* aDelegate=nsnull);

  ~GeckoChildProcessHost();

  static nsresult GetArchitecturesForBinary(const char *path, uint32 *result);

  static uint32 GetSupportedArchitecturesForProcessType(GeckoProcessType type);

  bool SyncLaunch(std::vector<std::string> aExtraOpts=std::vector<std::string>(),
                  int32 timeoutMs=0,
                  base::ProcessArchitecture arch=base::GetCurrentProcessArchitecture());
  bool AsyncLaunch(std::vector<std::string> aExtraOpts=std::vector<std::string>());
  bool PerformAsyncLaunch(std::vector<std::string> aExtraOpts=std::vector<std::string>(),
                          base::ProcessArchitecture arch=base::GetCurrentProcessArchitecture());

  virtual void OnChannelConnected(int32 peer_pid);
  virtual void OnMessageReceived(const IPC::Message& aMsg);
  virtual void OnChannelError();

  void InitializeChannel();

  virtual bool CanShutdown() { return true; }

  virtual void OnWaitableEventSignaled(base::WaitableEvent *event);

  IPC::Channel* GetChannel() {
    return channelp();
  }

  base::WaitableEvent* GetShutDownEvent() {
    return GetProcessEvent();
  }

  ProcessHandle GetChildProcessHandle() {
    return mChildProcessHandle;
  }

#ifdef XP_MACOSX
  task_t GetChildTask() {
    return mChildTask;
  }
#endif


protected:
  GeckoProcessType mProcessType;
  Monitor mMonitor;
  bool mLaunched;
  bool mChannelInitialized;
  FilePath mProcessPath;

#ifdef XP_WIN
  void InitWindowsGroupID();
  nsString mGroupId;
#endif

#if defined(OS_POSIX)
  base::file_handle_mapping_vector mFileMap;
#endif

  base::WaitableEventWatcher::Delegate* mDelegate;

  ProcessHandle mChildProcessHandle;
#if defined(OS_MACOSX)
  task_t mChildTask;
#endif

private:
  DISALLOW_EVIL_CONSTRUCTORS(GeckoChildProcessHost);
};

} /* namespace ipc */
} /* namespace mozilla */

#endif /* __IPC_GLUE_GECKOCHILDPROCESSHOST_H__ */
