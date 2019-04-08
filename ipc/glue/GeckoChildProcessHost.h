/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __IPC_GLUE_GECKOCHILDPROCESSHOST_H__
#define __IPC_GLUE_GECKOCHILDPROCESSHOST_H__

#include "base/file_path.h"
#include "base/process_util.h"
#include "base/waitable_event.h"
#include "chrome/common/child_process_host.h"

#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/Atomics.h"
#include "mozilla/Monitor.h"
#include "mozilla/MozPromise.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/UniquePtr.h"

#include "nsCOMPtr.h"
#include "nsXULAppAPI.h"  // for GeckoProcessType
#include "nsString.h"

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#  include "sandboxBroker.h"
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include "mozilla/Sandbox.h"
#endif

struct _MacSandboxInfo;
typedef _MacSandboxInfo MacSandboxInfo;

namespace mozilla {
namespace ipc {

class GeckoChildProcessHost : public ChildProcessHost {
 protected:
  typedef mozilla::Monitor Monitor;
  typedef std::vector<std::string> StringVector;

 public:
  typedef base::ProcessHandle ProcessHandle;

  explicit GeckoChildProcessHost(GeckoProcessType aProcessType,
                                 bool aIsFileContent = false);

  // Causes the object to be deleted, on the I/O thread, after any
  // pending asynchronous work (like launching) is complete.  This
  // method can be called from any thread.  If called from the I/O
  // thread itself, deletion won't happen until the event loop spins;
  // otherwise, it could happen immediately.
  //
  // GeckoChildProcessHost instances must not be deleted except
  // through this method.
  void Destroy();

  static uint32_t GetUniqueID();

  // Does not block.  The IPC channel may not be initialized yet, and
  // the child process may or may not have been created when this
  // method returns.
  bool AsyncLaunch(StringVector aExtraOpts = StringVector());

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
  bool LaunchAndWaitForProcessHandle(StringVector aExtraOpts = StringVector());

  // Block until the child process has been created and it connects to
  // the IPC channel, meaning it's fully initialized.  (Or until an
  // error occurs.)
  bool SyncLaunch(StringVector aExtraOpts = StringVector(),
                  int32_t timeoutMs = 0);

  virtual void OnChannelConnected(int32_t peer_pid) override;
  virtual void OnMessageReceived(IPC::Message&& aMsg) override;
  virtual void OnChannelError() override;
  virtual void GetQueuedMessages(std::queue<IPC::Message>& queue) override;

  struct LaunchError {};
  template <typename T>
  using LaunchPromise = mozilla::MozPromise<T, LaunchError, /* excl: */ false>;
  using HandlePromise = LaunchPromise<base::ProcessHandle>;

  // Resolves to the process handle when it's available (see
  // LaunchAndWaitForProcessHandle); use with AsyncLaunch.
  RefPtr<HandlePromise> WhenProcessHandleReady();

  virtual void InitializeChannel();

  virtual bool CanShutdown() override { return true; }

  IPC::Channel* GetChannel() { return channelp(); }

  // Returns a "borrowed" handle to the child process - the handle returned
  // by this function must not be closed by the caller.
  ProcessHandle GetChildProcessHandle() { return mChildProcessHandle; }

  GeckoProcessType GetProcessType() { return mProcessType; }

#ifdef XP_MACOSX
  task_t GetChildTask() { return mChildTask; }
#endif

#ifdef XP_WIN
  void AddHandleToShare(HANDLE aHandle) {
    mLaunchOptions->handles_to_inherit.push_back(aHandle);
  }
#else
  void AddFdToRemap(int aSrcFd, int aDstFd) {
    mLaunchOptions->fds_to_remap.push_back(std::make_pair(aSrcFd, aDstFd));
  }
#endif

  /**
   * Must run on the IO thread.  Cause the OS process to exit and
   * ensure its OS resources are cleaned up.
   */
  void Join();

  // For bug 943174: Skip the EnsureProcessTerminated call in the destructor.
  void SetAlreadyDead();

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // To allow filling a MacSandboxInfo from the child
  // process without an instance of RDDProcessHost.
  // Only needed for late-start sandbox enabling.
  static void StaticFillMacSandboxInfo(MacSandboxInfo& aInfo);

  // Start the sandbox from the child process.
  static bool StartMacSandbox(int aArgc, char** aArgv,
                              std::string& aErrorMessage);

  // The sandbox type that will be use when sandboxing is
  // enabled in the derived class and FillMacSandboxInfo
  // has not been overridden.
  static MacSandboxType GetDefaultMacSandboxType() {
    return MacSandboxType_Utility;
  };
#endif

 protected:
  ~GeckoChildProcessHost();

  GeckoProcessType mProcessType;
  bool mIsFileContent;
  Monitor mMonitor;
  FilePath mProcessPath;
  // GeckoChildProcessHost holds the launch options so they can be set
  // up on the main thread using main-thread-only APIs like prefs, and
  // then used for the actual launch on another thread.  This pointer
  // is set to null to free the options after the child is launched.
  UniquePtr<base::LaunchOptions> mLaunchOptions;

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

#  ifdef MOZ_SANDBOX
  RefPtr<AbstractSandboxBroker> mSandboxBroker;
  std::vector<std::wstring> mAllowedFilesRead;
  bool mEnableSandboxLogging;
  int32_t mSandboxLevel;
#  endif
#endif  // XP_WIN

  ProcessHandle mChildProcessHandle;
#if defined(OS_MACOSX)
  task_t mChildTask;
#endif
  RefPtr<HandlePromise::Private> mHandlePromise;

  bool OpenPrivilegedHandle(base::ProcessId aPid);

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // Override this method to return true to launch the child process
  // using the Mac utility (by default) sandbox. Override
  // FillMacSandboxInfo() to change the sandbox type and settings.
  virtual bool IsMacSandboxLaunchEnabled() { return false; }

  // Fill a MacSandboxInfo to configure the sandbox
  virtual void FillMacSandboxInfo(MacSandboxInfo& aInfo);

  // Adds the command line arguments needed to enable
  // sandboxing of the child process at startup before
  // the child event loop is up.
  virtual void AppendMacSandboxParams(StringVector& aArgs);
#endif

 private:
  DISALLOW_EVIL_CONSTRUCTORS(GeckoChildProcessHost);

  // Does the actual work for AsyncLaunch; run in a thread pool
  // (or, on Windows, a dedicated thread).
  bool PerformAsyncLaunch(StringVector aExtraOpts);

  // Called on the I/O thread; creates channel, dispatches
  // PerformAsyncLaunch, and consolidates error handling.
  void RunPerformAsyncLaunch(StringVector aExtraOpts);

  static BinPathType GetPathToBinary(FilePath& exePath,
                                     GeckoProcessType processType);

  // The buffer is passed to preserve its lifetime until we are done
  // with launching the sub-process.
  void GetChildLogName(const char* origLogName, nsACString& buffer);

  // In between launching the subprocess and handing off its IPC
  // channel, there's a small window of time in which *we* might still
  // be the channel listener, and receive messages.  That's bad
  // because we have no idea what to do with those messages.  So queue
  // them here until we hand off the eventual listener.
  //
  // FIXME/cjones: this strongly indicates bad design.  Shame on us.
  std::queue<IPC::Message> mQueue;

  // Set this up before we're called from a different thread.
#if defined(OS_LINUX)
  nsCString mTmpDirName;
#endif

  mozilla::Atomic<bool> mDestroying;

  static uint32_t sNextUniqueID;

#if defined(MOZ_WIDGET_ANDROID)
  void LaunchAndroidService(
      const char* type, const std::vector<std::string>& argv,
      const base::file_handle_mapping_vector& fds_to_remap,
      ProcessHandle* process_handle);
#endif  // defined(MOZ_WIDGET_ANDROID)
};

} /* namespace ipc */
} /* namespace mozilla */

#endif /* __IPC_GLUE_GECKOCHILDPROCESSHOST_H__ */
