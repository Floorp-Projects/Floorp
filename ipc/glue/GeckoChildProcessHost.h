/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  virtual void GetQueuedMessages(std::queue<IPC::Message>& queue);

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

  static PRInt32 mChildCounter;

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

  // Does the actual work for AsyncLaunch, on the IO thread.
  bool PerformAsyncLaunchInternal(std::vector<std::string>& aExtraOpts,
                                  base::ProcessArchitecture arch);

  // In between launching the subprocess and handing off its IPC
  // channel, there's a small window of time in which *we* might still
  // be the channel listener, and receive messages.  That's bad
  // because we have no idea what to do with those messages.  So queue
  // them here until we hand off the eventual listener.
  //
  // FIXME/cjones: this strongly indicates bad design.  Shame on us.
  std::queue<IPC::Message> mQueue;
};

} /* namespace ipc */
} /* namespace mozilla */

#endif /* __IPC_GLUE_GECKOCHILDPROCESSHOST_H__ */
