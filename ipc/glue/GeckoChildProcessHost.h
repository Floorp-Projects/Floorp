/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_GECKOCHILDPROCESSHOST_H__
#define __IPC_GLUE_GECKOCHILDPROCESSHOST_H__

#include "base/file_path.h"
#include "base/process_util.h"
#include "base/waitable_event.h"
#include "chrome/common/child_process_host.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/Monitor.h"
#include "mozilla/StaticPtr.h"

#include "nsCOMPtr.h"
#include "nsXULAppAPI.h"        // for GeckoProcessType
#include "nsString.h"

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#include "sandboxBroker.h"
#endif

namespace mozilla {
namespace ipc {

class GeckoChildProcessHost : public ChildProcessHost
{
protected:
  typedef mozilla::Monitor Monitor;
  typedef std::vector<std::string> StringVector;

public:
  typedef base::ChildPrivileges ChildPrivileges;
  typedef base::ProcessHandle ProcessHandle;

  static ChildPrivileges DefaultChildPrivileges();

  explicit GeckoChildProcessHost(GeckoProcessType aProcessType,
                                 ChildPrivileges aPrivileges=base::PRIVILEGES_DEFAULT);

  ~GeckoChildProcessHost();

  static nsresult GetArchitecturesForBinary(const char *path, uint32_t *result);

  static uint32_t GetSupportedArchitecturesForProcessType(GeckoProcessType type);

  static uint32_t GetUniqueID();

  // Block until the IPC channel for our subprocess is initialized,
  // but no longer.  The child process may or may not have been
  // created when this method returns.
  bool AsyncLaunch(StringVector aExtraOpts=StringVector(),
                   base::ProcessArchitecture arch=base::GetCurrentProcessArchitecture());

  virtual bool WaitUntilConnected(int32_t aTimeoutMs = 0);

  // Block until the IPC channel for our subprocess is initialized and
  // the OS process is created.  The subprocess may or may not have
  // connected back to us when this method returns.
  //
  // NB: on POSIX, this method is relatively cheap, and doesn't
  // require disk IO.  On win32 however, it requires at least the
  // analogue of stat().  This difference induces a semantic
  // difference in this method: on POSIX, when we return, we know the
  // subprocess has been created, but we don't know whether its
  // executable image can be loaded.  On win32, we do know that when
  // we return.  But we don't know if dynamic linking succeeded on
  // either platform.
  bool LaunchAndWaitForProcessHandle(StringVector aExtraOpts=StringVector());

  // Block until the child process has been created and it connects to
  // the IPC channel, meaning it's fully initialized.  (Or until an
  // error occurs.)
  bool SyncLaunch(StringVector aExtraOpts=StringVector(),
                  int32_t timeoutMs=0,
                  base::ProcessArchitecture arch=base::GetCurrentProcessArchitecture());

  virtual bool PerformAsyncLaunch(StringVector aExtraOpts=StringVector(),
                                  base::ProcessArchitecture aArch=base::GetCurrentProcessArchitecture());

  virtual void OnChannelConnected(int32_t peer_pid);
  virtual void OnMessageReceived(const IPC::Message& aMsg);
  virtual void OnChannelError();
  virtual void GetQueuedMessages(std::queue<IPC::Message>& queue);

  virtual void InitializeChannel();

  virtual bool CanShutdown() { return true; }

  virtual void OnWaitableEventSignaled(base::WaitableEvent *event);

  IPC::Channel* GetChannel() {
    return channelp();
  }

  base::WaitableEvent* GetShutDownEvent() {
    return GetProcessEvent();
  }

  // Returns a "borrowed" handle to the child process - the handle returned
  // by this function must not be closed by the caller.
  ProcessHandle GetChildProcessHandle() {
    return mChildProcessHandle;
  }

  GeckoProcessType GetProcessType() {
    return mProcessType;
  }

#ifdef XP_MACOSX
  task_t GetChildTask() {
    return mChildTask;
  }
#endif

  /**
   * Must run on the IO thread.  Cause the OS process to exit and
   * ensure its OS resources are cleaned up.
   */
  void Join();

  // For bug 943174: Skip the EnsureProcessTerminated call in the destructor.
  void SetAlreadyDead();

protected:
  GeckoProcessType mProcessType;
  ChildPrivileges mPrivileges;
  Monitor mMonitor;
  FilePath mProcessPath;

  // This value must be accessed while holding mMonitor.
  enum {
    // This object has been constructed, but the OS process has not
    // yet.
    CREATING_CHANNEL = 0,
    // The IPC channel for our subprocess has been created, but the OS
    // process has still not been created.
    CHANNEL_INITIALIZED,
    // The OS process has been created, but it hasn't yet connected to
    // our IPC channel.
    PROCESS_CREATED,
    // The process is launched and connected to our IPC channel.  All
    // is well.
    PROCESS_CONNECTED,
    PROCESS_ERROR
  } mProcessState;

  static int32_t mChildCounter;

  void PrepareLaunch();

#ifdef XP_WIN
  void InitWindowsGroupID();
  nsString mGroupId;

#ifdef MOZ_SANDBOX
  SandboxBroker mSandboxBroker;
  std::vector<std::wstring> mAllowedFilesRead;
  std::vector<std::wstring> mAllowedFilesReadWrite;
  bool mEnableSandboxLogging;
  int32_t mSandboxLevel;
#endif
#endif // XP_WIN

#if defined(OS_POSIX)
  base::file_handle_mapping_vector mFileMap;
#endif

  base::WaitableEventWatcher::Delegate* mDelegate;

  ProcessHandle mChildProcessHandle;
#if defined(OS_MACOSX)
  task_t mChildTask;
#endif

  void OpenPrivilegedHandle(base::ProcessId aPid);

private:
  DISALLOW_EVIL_CONSTRUCTORS(GeckoChildProcessHost);

  // Does the actual work for AsyncLaunch, on the IO thread.
  bool PerformAsyncLaunchInternal(std::vector<std::string>& aExtraOpts,
                                  base::ProcessArchitecture arch);

  bool RunPerformAsyncLaunch(StringVector aExtraOpts=StringVector(),
			     base::ProcessArchitecture aArch=base::GetCurrentProcessArchitecture());

  static void GetPathToBinary(FilePath& exePath);

  // In between launching the subprocess and handing off its IPC
  // channel, there's a small window of time in which *we* might still
  // be the channel listener, and receive messages.  That's bad
  // because we have no idea what to do with those messages.  So queue
  // them here until we hand off the eventual listener.
  //
  // FIXME/cjones: this strongly indicates bad design.  Shame on us.
  std::queue<IPC::Message> mQueue;

  static uint32_t sNextUniqueID;
};

#ifdef MOZ_NUWA_PROCESS
class GeckoExistingProcessHost final : public GeckoChildProcessHost
{
public:
  GeckoExistingProcessHost(GeckoProcessType aProcessType,
                           base::ProcessHandle aProcess,
                           const FileDescriptor& aFileDescriptor,
                           ChildPrivileges aPrivileges=base::PRIVILEGES_DEFAULT);

  ~GeckoExistingProcessHost();

  virtual bool PerformAsyncLaunch(StringVector aExtraOpts=StringVector(),
          base::ProcessArchitecture aArch=base::GetCurrentProcessArchitecture()) override;

  virtual void InitializeChannel() override;

private:
  base::ProcessHandle mExistingProcessHandle;
  mozilla::ipc::FileDescriptor mExistingFileDescriptor;
};
#endif /* MOZ_NUWA_PROCESS */

} /* namespace ipc */
} /* namespace mozilla */

#endif /* __IPC_GLUE_GECKOCHILDPROCESSHOST_H__ */
