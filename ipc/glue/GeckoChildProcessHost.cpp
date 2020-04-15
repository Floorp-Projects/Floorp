/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoChildProcessHost.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/process_watcher.h"
#ifdef MOZ_WIDGET_COCOA
#  include "chrome/common/mach_ipc_mac.h"
#  include "base/rand_util.h"
#  include "nsILocalFileMac.h"
#  include "SharedMemoryBasic.h"
#endif

#include "MainThreadUtils.h"
#include "mozilla/Sprintf.h"
#include "prenv.h"
#include "nsXPCOMPrivate.h"

#if defined(MOZ_SANDBOX)
#  include "mozilla/SandboxSettings.h"
#  include "nsAppDirectoryServiceDefs.h"
#endif

#include "nsExceptionHandler.h"

#include "nsDirectoryService.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsPrintfCString.h"
#include "nsIObserverService.h"

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/EnvironmentMap.h"
#include "mozilla/net/SocketProcessHost.h"
#include "mozilla/LinkedList.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/Omnijar.h"
#include "mozilla/RDDProcessHost.h"
#include "mozilla/Scoped.h"
#include "mozilla/Services.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Telemetry.h"
#include "ProtocolUtils.h"
#include <sys/stat.h>

#ifdef XP_WIN
#  include "nsIWinTaskbar.h"
#  include <stdlib.h>
#  define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

#  if defined(MOZ_SANDBOX)
#    include "mozilla/Preferences.h"
#    include "mozilla/sandboxing/sandboxLogging.h"
#    include "WinUtils.h"
#    if defined(_ARM64_)
#      include "mozilla/remoteSandboxBroker.h"
#    endif
#  endif
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#  include "mozilla/SandboxLaunch.h"
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
#  include "GMPProcessParent.h"
#  include "nsMacUtilsImpl.h"
#endif

#include "nsTArray.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsNativeCharsetUtils.h"
#include "nscore.h"  // for NS_FREE_PERMANENT_DATA
#include "private/pprio.h"

using mozilla::MonitorAutoLock;
using mozilla::Preferences;
using mozilla::StaticMutexAutoLock;

namespace mozilla {
MOZ_TYPE_SPECIFIC_SCOPED_POINTER_TEMPLATE(ScopedPRFileDesc, PRFileDesc,
                                          PR_Close)
}

using mozilla::ScopedPRFileDesc;

#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidBridge.h"
#  include "GeneratedJNIWrappers.h"
#  include "mozilla/jni/Refs.h"
#  include "mozilla/jni/Utils.h"
#endif

#ifdef MOZ_ENABLE_FORKSERVER
#  include "mozilla/ipc/ForkServiceChild.h"
#endif

static bool ShouldHaveDirectoryService() {
  return GeckoProcessType_Default == XRE_GetProcessType();
}

namespace mozilla {
namespace ipc {

static Atomic<int32_t> gChildCounter;

static inline nsISerialEventTarget* IOThread() {
  return XRE_GetIOMessageLoop()->SerialEventTarget();
}

class BaseProcessLauncher {
 public:
  BaseProcessLauncher(GeckoChildProcessHost* aHost,
                      std::vector<std::string>&& aExtraOpts)
      : mProcessType(aHost->mProcessType),
        mLaunchOptions(std::move(aHost->mLaunchOptions)),
        mExtraOpts(std::move(aExtraOpts)),
#ifdef XP_WIN
        mGroupId(aHost->mGroupId),
#endif
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
        mAllowedFilesRead(aHost->mAllowedFilesRead),
        mSandboxLevel(aHost->mSandboxLevel),
        mIsFileContent(aHost->mIsFileContent),
        mEnableSandboxLogging(aHost->mEnableSandboxLogging),
#endif
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
        mDisableOSActivityMode(aHost->mDisableOSActivityMode),
#endif
        mTmpDirName(aHost->mTmpDirName),
        mChildId(++gChildCounter) {
    SprintfLiteral(mPidString, "%d", base::GetCurrentProcId());

    // Compute the serial event target we'll use for launching.
    nsCOMPtr<nsIEventTarget> threadOrPool = GetIPCLauncher();
    mLaunchThread = new TaskQueue(threadOrPool.forget());

    if (ShouldHaveDirectoryService()) {
      // "Current process directory" means the app dir, not the current
      // working dir or similar.
      mozilla::Unused
          << nsDirectoryService::gService->GetCurrentProcessDirectory(
                 getter_AddRefs(mAppDir));
    }
  }

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(BaseProcessLauncher);

  RefPtr<ProcessLaunchPromise> Launch(GeckoChildProcessHost*);

 protected:
  virtual ~BaseProcessLauncher() = default;

  RefPtr<ProcessLaunchPromise> PerformAsyncLaunch();
  RefPtr<ProcessLaunchPromise> FinishLaunch();

  // Overrideable hooks. If superclass behavior is invoked, it's always at the
  // top of the override.
  virtual bool DoSetup();
  virtual RefPtr<ProcessHandlePromise> DoLaunch() = 0;
  virtual bool DoFinishLaunch() { return true; };

  void MapChildLogging();

  static BinPathType GetPathToBinary(FilePath&, GeckoProcessType);

  void GetChildLogName(const char* origLogName, nsACString& buffer);

  const char* ChildProcessType() {
    return XRE_GeckoProcessTypeToString(mProcessType);
  }

  nsCOMPtr<nsISerialEventTarget> mLaunchThread;
  GeckoProcessType mProcessType;
  UniquePtr<base::LaunchOptions> mLaunchOptions;
  std::vector<std::string> mExtraOpts;
#ifdef XP_WIN
  nsString mGroupId;
#endif
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  std::vector<std::wstring> mAllowedFilesRead;
  int32_t mSandboxLevel;
  bool mIsFileContent;
  bool mEnableSandboxLogging;
#endif
#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  // Controls whether or not the process will be launched with
  // environment variable OS_ACTIVITY_MODE set to "disabled".
  bool mDisableOSActivityMode;
#endif
  nsCString mTmpDirName;
  LaunchResults mResults = LaunchResults();
  int32_t mChildId;
  TimeStamp mStartTimeStamp = TimeStamp::Now();
  char mPidString[32];

  // Set during launch.
  IPC::Channel* mChannel = nullptr;
  std::wstring mChannelId;
  ScopedPRFileDesc mCrashAnnotationReadPipe;
  ScopedPRFileDesc mCrashAnnotationWritePipe;
  nsCOMPtr<nsIFile> mAppDir;
};

#ifdef XP_WIN
class WindowsProcessLauncher : public BaseProcessLauncher {
 public:
  WindowsProcessLauncher(GeckoChildProcessHost* aHost,
                         std::vector<std::string>&& aExtraOpts)
      : BaseProcessLauncher(aHost, std::move(aExtraOpts)),
        mProfileDir(aHost->mProfileDir) {}

 protected:
  virtual bool DoSetup() override;
  virtual RefPtr<ProcessHandlePromise> DoLaunch() override;
  virtual bool DoFinishLaunch() override;

  mozilla::Maybe<CommandLine> mCmdLine;
  bool mUseSandbox = false;

  nsCOMPtr<nsIFile> mProfileDir;
};
typedef WindowsProcessLauncher ProcessLauncher;
#endif  // XP_WIN

#ifdef OS_POSIX
class PosixProcessLauncher : public BaseProcessLauncher {
 public:
  PosixProcessLauncher(GeckoChildProcessHost* aHost,
                       std::vector<std::string>&& aExtraOpts)
      : BaseProcessLauncher(aHost, std::move(aExtraOpts)),
        mProfileDir(aHost->mProfileDir) {}

 protected:
  virtual bool DoSetup() override;
  virtual RefPtr<ProcessHandlePromise> DoLaunch() override;
  virtual bool DoFinishLaunch() override;

  nsCOMPtr<nsIFile> mProfileDir;

  std::vector<std::string> mChildArgv;
};

#  if defined(XP_MACOSX)
class MacProcessLauncher : public PosixProcessLauncher {
 public:
  MacProcessLauncher(GeckoChildProcessHost* aHost,
                     std::vector<std::string>&& aExtraOpts)
      : PosixProcessLauncher(aHost, std::move(aExtraOpts)),
        // Put a random number into the channel name, so that
        // a compromised renderer can't pretend being the child
        // that's forked off.
        mMachConnectionName(
            StringPrintf("org.mozilla.machname.%d",
                         base::RandInt(0, std::numeric_limits<int>::max()))),
        mParentRecvPort(mMachConnectionName.c_str()) {}

 protected:
  virtual bool DoFinishLaunch() override;

  std::string mMachConnectionName;
  // We add a mach port to the command line so the child can communicate its
  // 'task_t' back to the parent.
  ReceivePort mParentRecvPort;

  friend class PosixProcessLauncher;
};
typedef MacProcessLauncher ProcessLauncher;
#  elif defined(MOZ_WIDGET_ANDROID)
class AndroidProcessLauncher : public PosixProcessLauncher {
 public:
  AndroidProcessLauncher(GeckoChildProcessHost* aHost,
                         std::vector<std::string>&& aExtraOpts)
      : PosixProcessLauncher(aHost, std::move(aExtraOpts)) {}

 protected:
  virtual RefPtr<ProcessHandlePromise> DoLaunch() override;
  RefPtr<ProcessHandlePromise> LaunchAndroidService(
      const GeckoProcessType aType, const std::vector<std::string>& argv,
      const base::file_handle_mapping_vector& fds_to_remap);
};
typedef AndroidProcessLauncher ProcessLauncher;
// NB: Technically Android is linux (i.e. XP_LINUX is defined), but we want
// orthogonal IPC machinery there. Conversely, there are tier-3 non-Linux
// platforms (BSD and Solaris) where we want the "linux" IPC machinery. So
// we use MOZ_WIDGET_* to choose the platform backend.
#  elif defined(MOZ_WIDGET_GTK)
class LinuxProcessLauncher : public PosixProcessLauncher {
 public:
  LinuxProcessLauncher(GeckoChildProcessHost* aHost,
                       std::vector<std::string>&& aExtraOpts)
      : PosixProcessLauncher(aHost, std::move(aExtraOpts)) {}

 protected:
  virtual bool DoSetup() override;
};
typedef LinuxProcessLauncher ProcessLauncher;
#  elif
#    error "Unknown platform"
#  endif
#endif  // OS_POSIX

using base::ProcessHandle;
using mozilla::ipc::BaseProcessLauncher;
using mozilla::ipc::ProcessLauncher;

mozilla::StaticAutoPtr<mozilla::LinkedList<GeckoChildProcessHost>>
    GeckoChildProcessHost::sGeckoChildProcessHosts;

mozilla::StaticMutex GeckoChildProcessHost::sMutex;

GeckoChildProcessHost::GeckoChildProcessHost(GeckoProcessType aProcessType,
                                             bool aIsFileContent)
    : mProcessType(aProcessType),
      mIsFileContent(aIsFileContent),
      mMonitor("mozilla.ipc.GeckChildProcessHost.mMonitor"),
      mLaunchOptions(MakeUnique<base::LaunchOptions>()),
      mProcessState(CREATING_CHANNEL),
#ifdef XP_WIN
      mGroupId(u"-"),
#endif
#if defined(MOZ_SANDBOX) && defined(XP_WIN)
      mEnableSandboxLogging(false),
      mSandboxLevel(0),
#endif
      mChildProcessHandle(0),
#if defined(MOZ_WIDGET_COCOA)
      mChildTask(MACH_PORT_NULL),
#endif
#if defined(MOZ_SANDBOX) && defined(XP_MACOSX)
      mDisableOSActivityMode(false),
#endif
      mDestroying(false) {
  MOZ_COUNT_CTOR(GeckoChildProcessHost);
  StaticMutexAutoLock lock(sMutex);
  if (!sGeckoChildProcessHosts) {
    sGeckoChildProcessHosts = new mozilla::LinkedList<GeckoChildProcessHost>();
  }
  sGeckoChildProcessHosts->insertBack(this);
#if defined(MOZ_SANDBOX) && defined(XP_LINUX)
  // The content process needs the content temp dir:
  if (aProcessType == GeckoProcessType_Content) {
    nsCOMPtr<nsIFile> contentTempDir;
    nsresult rv = NS_GetSpecialDirectory(NS_APP_CONTENT_PROCESS_TEMP_DIR,
                                         getter_AddRefs(contentTempDir));
    if (NS_SUCCEEDED(rv)) {
      contentTempDir->GetNativePath(mTmpDirName);
    }
  }
#endif
#if defined(MOZ_ENABLE_FORKSERVER)
  if (aProcessType == GeckoProcessType_Content && ForkServiceChild::Get()) {
    mLaunchOptions->use_forkserver = true;
  }
#endif
}

GeckoChildProcessHost::~GeckoChildProcessHost()

{
  AssertIOThread();
  MOZ_RELEASE_ASSERT(mDestroying);

  MOZ_COUNT_DTOR(GeckoChildProcessHost);

  if (mChildProcessHandle != 0) {
#if defined(MOZ_WIDGET_COCOA)
    SharedMemoryBasic::CleanupForPidWithLock(mChildProcessHandle);
#endif
    ProcessWatcher::EnsureProcessTerminated(
        mChildProcessHandle
#ifdef NS_FREE_PERMANENT_DATA
        // If we're doing leak logging, shutdown can be slow.
        ,
        false  // don't "force"
#endif
    );
  }

#if defined(MOZ_WIDGET_COCOA)
  if (mChildTask != MACH_PORT_NULL)
    mach_port_deallocate(mach_task_self(), mChildTask);
#endif

  if (mChildProcessHandle != 0) {
#if defined(XP_WIN)
    CrashReporter::DeregisterChildCrashAnnotationFileDescriptor(
        base::GetProcId(mChildProcessHandle));
#else
    CrashReporter::DeregisterChildCrashAnnotationFileDescriptor(
        mChildProcessHandle);
#endif
  }
#if defined(MOZ_SANDBOX) && defined(XP_WIN)
  if (mSandboxBroker) {
    mSandboxBroker->Shutdown();
    mSandboxBroker = nullptr;
  }
#endif
}

void GeckoChildProcessHost::RemoveFromProcessList() {
  StaticMutexAutoLock lock(sMutex);
  if (!sGeckoChildProcessHosts) {
    return;
  }
  LinkedListElement<GeckoChildProcessHost>::removeFrom(
      *sGeckoChildProcessHosts);
}

void GeckoChildProcessHost::Destroy() {
  MOZ_RELEASE_ASSERT(!mDestroying);
  // We can remove from the list before it's really destroyed
  RemoveFromProcessList();
  RefPtr<ProcessHandlePromise> whenReady = mHandlePromise;

  if (!whenReady) {
    // AsyncLaunch not called yet, so dispatch immediately.
    whenReady = ProcessHandlePromise::CreateAndReject(LaunchError{}, __func__);
  }

  using Value = ProcessHandlePromise::ResolveOrRejectValue;
  mDestroying = true;
  whenReady->Then(XRE_GetIOMessageLoop()->SerialEventTarget(), __func__,
                  [this](const Value&) { delete this; });
}

// static
mozilla::BinPathType BaseProcessLauncher::GetPathToBinary(
    FilePath& exePath, GeckoProcessType processType) {
  BinPathType pathType = XRE_GetChildProcBinPathType(processType);

  if (pathType == BinPathType::Self) {
#if defined(OS_WIN)
    wchar_t exePathBuf[MAXPATHLEN];
    if (!::GetModuleFileNameW(nullptr, exePathBuf, MAXPATHLEN)) {
      MOZ_CRASH("GetModuleFileNameW failed (FIXME)");
    }
#  if defined(MOZ_SANDBOX)
    // We need to start the child process using the real path, so that the
    // sandbox policy rules will match for DLLs loaded from the bin dir after
    // we have lowered the sandbox.
    std::wstring exePathStr = exePathBuf;
    if (widget::WinUtils::ResolveJunctionPointsAndSymLinks(exePathStr)) {
      exePath = FilePath::FromWStringHack(exePathStr);
    } else
#  endif
    {
      exePath = FilePath::FromWStringHack(exePathBuf);
    }
#elif defined(OS_POSIX)
    exePath = FilePath(CommandLine::ForCurrentProcess()->argv()[0]);
#else
#  error Sorry; target OS not supported yet.
#endif
    return pathType;
  }

  if (ShouldHaveDirectoryService()) {
    MOZ_ASSERT(gGREBinPath);
#ifdef OS_WIN
    exePath = FilePath(char16ptr_t(gGREBinPath));
#elif MOZ_WIDGET_COCOA
    nsCOMPtr<nsIFile> childProcPath;
    NS_NewLocalFile(nsDependentString(gGREBinPath), false,
                    getter_AddRefs(childProcPath));

    // We need to use an App Bundle on OS X so that we can hide
    // the dock icon. See Bug 557225.
    childProcPath->AppendNative(NS_LITERAL_CSTRING("plugin-container.app"));
    childProcPath->AppendNative(NS_LITERAL_CSTRING("Contents"));
    childProcPath->AppendNative(NS_LITERAL_CSTRING("MacOS"));
    nsCString tempCPath;
    childProcPath->GetNativePath(tempCPath);
    exePath = FilePath(tempCPath.get());
#else
    nsCString path;
    NS_CopyUnicodeToNative(nsDependentString(gGREBinPath), path);
    exePath = FilePath(path.get());
#endif
  }

  if (exePath.empty()) {
#ifdef OS_WIN
    exePath =
        FilePath::FromWStringHack(CommandLine::ForCurrentProcess()->program());
#else
    exePath = FilePath(CommandLine::ForCurrentProcess()->argv()[0]);
#endif
    exePath = exePath.DirName();
  }

  exePath = exePath.AppendASCII(MOZ_CHILD_PROCESS_NAME);

  return pathType;
}

#ifdef MOZ_WIDGET_COCOA
class AutoCFTypeObject {
 public:
  explicit AutoCFTypeObject(CFTypeRef object) { mObject = object; }
  ~AutoCFTypeObject() { ::CFRelease(mObject); }

 private:
  CFTypeRef mObject;
};
#endif

// We start the unique IDs at 1 so that 0 can be used to mean that
// a component has no unique ID assigned to it.
uint32_t GeckoChildProcessHost::sNextUniqueID = 1;

/* static */
uint32_t GeckoChildProcessHost::GetUniqueID() { return sNextUniqueID++; }

void GeckoChildProcessHost::PrepareLaunch() {
  if (CrashReporter::GetEnabled()) {
    CrashReporter::OOPInit();
  }

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
  SandboxLaunchPrepare(mProcessType, mLaunchOptions.get());
#endif

#ifdef XP_WIN
  if (mProcessType == GeckoProcessType_Plugin) {
    InitWindowsGroupID();
  }

#  if defined(MOZ_SANDBOX)
  // We need to get the pref here as the process is launched off main thread.
  if (mProcessType == GeckoProcessType_Content) {
    mSandboxLevel = GetEffectiveContentSandboxLevel();
    mEnableSandboxLogging =
        Preferences::GetBool("security.sandbox.logging.enabled");

    // We currently have to whitelist certain paths for tests to work in some
    // development configurations.
    nsAutoString readPaths;
    nsresult rv = Preferences::GetString(
        "security.sandbox.content.read_path_whitelist", readPaths);
    if (NS_SUCCEEDED(rv)) {
      for (const nsAString& readPath : readPaths.Split(',')) {
        nsString trimmedPath(readPath);
        trimmedPath.Trim(" ", true, true);
        std::wstring resolvedPath(trimmedPath.Data());
        // Before resolving check if path ends with '\' as this indicates we
        // want to give read access to a directory and so it needs a wildcard.
        bool addWildcard = (resolvedPath.back() == L'\\');
        if (!widget::WinUtils::ResolveJunctionPointsAndSymLinks(resolvedPath)) {
          NS_ERROR("Failed to resolve test read policy rule.");
          continue;
        }

        if (addWildcard) {
          resolvedPath.append(L"\\*");
        }
        mAllowedFilesRead.push_back(resolvedPath);
      }
    }
  }
#  endif

#  if defined(MOZ_SANDBOX)
  // For other process types we can't rely on them being launched on main
  // thread and they may not have access to prefs in the child process, so allow
  // them to turn on logging via an environment variable.
  mEnableSandboxLogging =
      mEnableSandboxLogging || !!PR_GetEnv("MOZ_SANDBOX_LOGGING");

  if (ShouldHaveDirectoryService() && mProcessType == GeckoProcessType_GPU) {
    mozilla::Unused << NS_GetSpecialDirectory(NS_APP_PROFILE_DIR_STARTUP,
                                              getter_AddRefs(mProfileDir));
  }
#  endif
#elif defined(XP_MACOSX)
#  if defined(MOZ_SANDBOX)
  if (ShouldHaveDirectoryService() &&
      mProcessType != GeckoProcessType_GMPlugin) {
    mozilla::Unused << NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR,
                                              getter_AddRefs(mProfileDir));
  }
#  endif
#endif
}

#ifdef XP_WIN
void GeckoChildProcessHost::InitWindowsGroupID() {
  // On Win7+, pass the application user model to the child, so it can
  // register with it. This insures windows created by the container
  // properly group with the parent app on the Win7 taskbar.
  nsCOMPtr<nsIWinTaskbar> taskbarInfo = do_GetService(NS_TASKBAR_CONTRACTID);
  if (taskbarInfo) {
    bool isSupported = false;
    taskbarInfo->GetAvailable(&isSupported);
    nsAutoString appId;
    if (isSupported && NS_SUCCEEDED(taskbarInfo->GetDefaultGroupId(appId))) {
      MOZ_ASSERT(mGroupId.EqualsLiteral("-"));
      mGroupId.Assign(appId);
    }
  }
}
#endif

bool GeckoChildProcessHost::SyncLaunch(std::vector<std::string> aExtraOpts,
                                       int aTimeoutMs) {
  if (!AsyncLaunch(std::move(aExtraOpts))) {
    return false;
  }
  return WaitUntilConnected(aTimeoutMs);
}

// Note: for most process types, we currently call AsyncLaunch, and therefore
// the *ProcessLauncher constructor, on the main thread, while the
// ProcessLauncher methods to actually execute the launch are called on the IO
// or IPC launcher thread. GMP processes are an exception - the GMP code
// invokes GeckoChildProcessHost from non-main-threads, and therefore we cannot
// rely on having access to mainthread-only services (like the directory
// service) from this code if we're launching that type of process.
bool GeckoChildProcessHost::AsyncLaunch(std::vector<std::string> aExtraOpts) {
  PrepareLaunch();

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
  if (IsMacSandboxLaunchEnabled() && !AppendMacSandboxParams(aExtraOpts)) {
    return false;
  }
#endif

  RefPtr<BaseProcessLauncher> launcher =
      new ProcessLauncher(this, std::move(aExtraOpts));

  // Note: Destroy() waits on mHandlePromise to delete |this|. As such, we want
  // to be sure that all of our post-launch processing on |this| happens before
  // mHandlePromise notifies.
  MOZ_ASSERT(mHandlePromise == nullptr);
  RefPtr<ProcessHandlePromise::Private> p =
      new ProcessHandlePromise::Private(__func__);
  mHandlePromise = p;

  mozilla::InvokeAsync<GeckoChildProcessHost*>(
      IOThread(), launcher.get(), __func__, &BaseProcessLauncher::Launch, this)
      ->Then(
          IOThread(), __func__,
          [this, p](const LaunchResults aResults) {
            {
              if (!OpenPrivilegedHandle(base::GetProcId(aResults.mHandle))
#ifdef XP_WIN
                  // If we failed in opening the process handle, try harder by
                  // duplicating one.
                  && !::DuplicateHandle(::GetCurrentProcess(), aResults.mHandle,
                                        ::GetCurrentProcess(),
                                        &mChildProcessHandle,
                                        PROCESS_DUP_HANDLE | PROCESS_TERMINATE |
                                            PROCESS_QUERY_INFORMATION |
                                            PROCESS_VM_READ | SYNCHRONIZE,
                                        FALSE, 0)
#endif  // XP_WIN
              ) {
                MOZ_CRASH("cannot open handle to child process");
              }

#ifdef XP_MACOSX
              this->mChildTask = aResults.mChildTask;
#endif
#if defined(XP_WIN) && defined(MOZ_SANDBOX)
              this->mSandboxBroker = aResults.mSandboxBroker;
#endif

              MonitorAutoLock lock(mMonitor);
              // The OnChannel{Connected,Error} may have already advanced the
              // state.
              if (mProcessState < PROCESS_CREATED) {
                mProcessState = PROCESS_CREATED;
              }
              lock.Notify();
            }
            p->Resolve(aResults.mHandle, __func__);
          },
          [this, p](const LaunchError aError) {
            // WaitUntilConnected might be waiting for us to signal.
            // If something failed let's set the error state and notify.
            CHROMIUM_LOG(ERROR)
                << "Failed to launch "
                << XRE_GeckoProcessTypeToString(mProcessType) << " subprocess";
            Telemetry::Accumulate(
                Telemetry::SUBPROCESS_LAUNCH_FAILURE,
                nsDependentCString(XRE_GeckoProcessTypeToString(mProcessType)));
            {
              MonitorAutoLock lock(mMonitor);
              mProcessState = PROCESS_ERROR;
              lock.Notify();
            }
            p->Reject(aError, __func__);
          });
  return true;
}

bool GeckoChildProcessHost::WaitUntilConnected(int32_t aTimeoutMs) {
  AUTO_PROFILER_LABEL("GeckoChildProcessHost::WaitUntilConnected", OTHER);

  // NB: this uses a different mechanism than the chromium parent
  // class.
  TimeDuration timeout = (aTimeoutMs > 0)
                             ? TimeDuration::FromMilliseconds(aTimeoutMs)
                             : TimeDuration::Forever();

  MonitorAutoLock lock(mMonitor);
  TimeStamp waitStart = TimeStamp::Now();
  TimeStamp current;

  // We'll receive several notifications, we need to exit when we
  // have either successfully launched or have timed out.
  while (mProcessState != PROCESS_CONNECTED) {
    // If there was an error then return it, don't wait out the timeout.
    if (mProcessState == PROCESS_ERROR) {
      break;
    }

    CVStatus status = lock.Wait(timeout);
    if (status == CVStatus::Timeout) {
      break;
    }

    if (timeout != TimeDuration::Forever()) {
      current = TimeStamp::Now();
      timeout -= current - waitStart;
      waitStart = current;
    }
  }

  return mProcessState == PROCESS_CONNECTED;
}

bool GeckoChildProcessHost::WaitForProcessHandle() {
  MonitorAutoLock lock(mMonitor);
  while (mProcessState < PROCESS_CREATED) {
    lock.Wait();
  }
  MOZ_ASSERT(mProcessState == PROCESS_ERROR || mChildProcessHandle);

  return mProcessState < PROCESS_ERROR;
}

bool GeckoChildProcessHost::LaunchAndWaitForProcessHandle(
    StringVector aExtraOpts) {
  if (!AsyncLaunch(std::move(aExtraOpts))) {
    return false;
  }
  return WaitForProcessHandle();
}

void GeckoChildProcessHost::InitializeChannel() {
  CreateChannel();

  MonitorAutoLock lock(mMonitor);
  mProcessState = CHANNEL_INITIALIZED;
  lock.Notify();
}

void GeckoChildProcessHost::Join() {
  AssertIOThread();

  if (!mChildProcessHandle) {
    return;
  }

  // If this fails, there's nothing we can do.
  base::KillProcess(mChildProcessHandle, 0, /*wait*/ true);
  SetAlreadyDead();
}

void GeckoChildProcessHost::SetAlreadyDead() {
  if (mChildProcessHandle && mChildProcessHandle != kInvalidProcessHandle) {
    base::CloseProcessHandle(mChildProcessHandle);
  }

  mChildProcessHandle = 0;
}

void BaseProcessLauncher::GetChildLogName(const char* origLogName,
                                          nsACString& buffer) {
#ifdef XP_WIN
  // On Windows we must expand relative paths because sandboxing rules
  // bound only to full paths.  fopen fowards to NtCreateFile which checks
  // the path against the sanboxing rules as passed to fopen (left relative).
  char absPath[MAX_PATH + 2];
  if (_fullpath(absPath, origLogName, sizeof(absPath))) {
#  ifdef MOZ_SANDBOX
    // We need to make sure the child log name doesn't contain any junction
    // points or symlinks or the sandbox will reject rules to allow writing.
    std::wstring resolvedPath(NS_ConvertUTF8toUTF16(absPath).get());
    if (widget::WinUtils::ResolveJunctionPointsAndSymLinks(resolvedPath)) {
      AppendUTF16toUTF8(
          MakeSpan(reinterpret_cast<const char16_t*>(resolvedPath.data()),
                   resolvedPath.size()),
          buffer);
    } else
#  endif
    {
      buffer.Append(absPath);
    }
  } else
#endif
  {
    buffer.Append(origLogName);
  }

  // Remove .moz_log extension to avoid its duplication, it will be added
  // automatically by the logging backend
  static NS_NAMED_LITERAL_CSTRING(kMozLogExt, MOZ_LOG_FILE_EXTENSION);
  if (StringEndsWith(buffer, kMozLogExt)) {
    buffer.Truncate(buffer.Length() - kMozLogExt.Length());
  }

  // Append child-specific postfix to name
  buffer.AppendLiteral(".child-");
  buffer.AppendInt(gChildCounter);
}

// Windows needs a single dedicated thread for process launching,
// because of thread-safety restrictions/assertions in the sandbox
// code.
//
// Android also needs a single dedicated thread to simplify thread
// safety in java.
//
// Fork server needs a dedicated thread for accessing
// |ForkServiceChild|.
#if defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) || \
    defined(MOZ_ENABLE_FORKSERVER)

static mozilla::StaticMutex gIPCLaunchThreadMutex;
static mozilla::StaticRefPtr<nsIThread> gIPCLaunchThread;

class IPCLaunchThreadObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
 protected:
  virtual ~IPCLaunchThreadObserver() = default;
};

NS_IMPL_ISUPPORTS(IPCLaunchThreadObserver, nsIObserver, nsISupports)

NS_IMETHODIMP
IPCLaunchThreadObserver::Observe(nsISupports* aSubject, const char* aTopic,
                                 const char16_t* aData) {
  MOZ_RELEASE_ASSERT(strcmp(aTopic, "xpcom-shutdown-threads") == 0);
  StaticMutexAutoLock lock(gIPCLaunchThreadMutex);

  nsresult rv = NS_OK;
  if (gIPCLaunchThread) {
    rv = gIPCLaunchThread->Shutdown();
    gIPCLaunchThread = nullptr;
  }
  mozilla::Unused << NS_WARN_IF(NS_FAILED(rv));
  return rv;
}

nsCOMPtr<nsIEventTarget> GetIPCLauncher() {
  StaticMutexAutoLock lock(gIPCLaunchThreadMutex);
  if (!gIPCLaunchThread) {
    nsCOMPtr<nsIThread> thread;
    nsresult rv = NS_NewNamedThread(NS_LITERAL_CSTRING("IPC Launch"),
                                    getter_AddRefs(thread));
    if (!NS_WARN_IF(NS_FAILED(rv))) {
      NS_DispatchToMainThread(
          NS_NewRunnableFunction("GeckoChildProcessHost::GetIPCLauncher", [] {
            nsCOMPtr<nsIObserverService> obsService =
                mozilla::services::GetObserverService();
            nsCOMPtr<nsIObserver> obs = new IPCLaunchThreadObserver();
            obsService->AddObserver(obs, "xpcom-shutdown-threads", false);
          }));
      gIPCLaunchThread = thread.forget();
    }
  }

  nsCOMPtr<nsIEventTarget> thread = gIPCLaunchThread.get();
  MOZ_DIAGNOSTIC_ASSERT(thread);
  return thread;
}

#else  // defined(XP_WIN) || defined(MOZ_WIDGET_ANDROID) ||
       // defined(MOZ_ENABLE_FORKSERVER)

// Other platforms use an on-demand thread pool.

nsCOMPtr<nsIEventTarget> GetIPCLauncher() {
  nsCOMPtr<nsIEventTarget> pool =
      mozilla::SharedThreadPool::Get(NS_LITERAL_CSTRING("IPC Launch"));
  MOZ_DIAGNOSTIC_ASSERT(pool);
  return pool;
}

#endif  // XP_WIN || MOZ_WIDGET_ANDROID || MOZ_ENABLE_FORKSERVER

void
#if defined(XP_WIN)
AddAppDirToCommandLine(CommandLine& aCmdLine, nsIFile* aAppDir)
#else
AddAppDirToCommandLine(std::vector<std::string>& aCmdLine, nsIFile* aAppDir,
    nsIFile* aProfileDir)
#endif
{
  // Content processes need access to application resources, so pass
  // the full application directory path to the child process.
  if (aAppDir) {
#if defined(XP_WIN)
    nsString path;
    MOZ_ALWAYS_SUCCEEDS(aAppDir->GetPath(path));
    aCmdLine.AppendLooseValue(UTF8ToWide("-appdir"));
    std::wstring wpath(path.get());
    aCmdLine.AppendLooseValue(wpath);
#else
    nsAutoCString path;
    MOZ_ALWAYS_SUCCEEDS(aAppDir->GetNativePath(path));
    aCmdLine.push_back("-appdir");
    aCmdLine.push_back(path.get());
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
    // Full path to the profile dir
    if (aProfileDir) {
      // If the profile doesn't exist, normalization will
      // fail. But we don't return an error here because some
      // tests require startup with a missing profile dir.
      // For users, almost universally, the profile will be in
      // the home directory and normalization isn't required.
      mozilla::Unused << aProfileDir->Normalize();
      nsAutoCString path;
      MOZ_ALWAYS_SUCCEEDS(aProfileDir->GetNativePath(path));
      aCmdLine.push_back("-profile");
      aCmdLine.push_back(path.get());
    }
#endif
  }
}

#if defined(XP_WIN) && (defined(MOZ_SANDBOX) || defined(_ARM64_))
static bool Contains(const std::vector<std::string>& aExtraOpts,
                     const char* aValue) {
  return std::any_of(aExtraOpts.begin(), aExtraOpts.end(),
                     [&](const std::string arg) {
                       return arg.find(aValue) != std::string::npos;
                     });
}
#endif  // defined(XP_WIN) && (defined(MOZ_SANDBOX) || defined(_ARM64_))

RefPtr<ProcessLaunchPromise> BaseProcessLauncher::PerformAsyncLaunch() {
  if (!DoSetup()) {
    return ProcessLaunchPromise::CreateAndReject(LaunchError{}, __func__);
  }
  RefPtr<BaseProcessLauncher> self = this;
  return DoLaunch()->Then(
      mLaunchThread, __func__,
      [self](base::ProcessHandle aHandle) {
        self->mResults.mHandle = aHandle;
        return self->FinishLaunch();
      },
      [](LaunchError aError) {
        return ProcessLaunchPromise::CreateAndReject(aError, __func__);
      });
}

bool BaseProcessLauncher::DoSetup() {
#ifdef MOZ_GECKO_PROFILER
  RefPtr<BaseProcessLauncher> self = this;
  GetProfilerEnvVarsForChildProcess([self](const char* key, const char* value) {
    self->mLaunchOptions->env_map[ENVIRONMENT_STRING(key)] =
        ENVIRONMENT_STRING(value);
  });
#endif

  MapChildLogging();

  return PR_CreatePipe(&mCrashAnnotationReadPipe.rwget(),
                       &mCrashAnnotationWritePipe.rwget()) == PR_SUCCESS;
}

void BaseProcessLauncher::MapChildLogging() {
  const char* origNSPRLogName = PR_GetEnv("NSPR_LOG_FILE");
  const char* origMozLogName = PR_GetEnv("MOZ_LOG_FILE");

  if (origNSPRLogName) {
    nsAutoCString nsprLogName;
    GetChildLogName(origNSPRLogName, nsprLogName);
    mLaunchOptions->env_map[ENVIRONMENT_LITERAL("NSPR_LOG_FILE")] =
        ENVIRONMENT_STRING(nsprLogName.get());
  }
  if (origMozLogName) {
    nsAutoCString mozLogName;
    GetChildLogName(origMozLogName, mozLogName);
    mLaunchOptions->env_map[ENVIRONMENT_LITERAL("MOZ_LOG_FILE")] =
        ENVIRONMENT_STRING(mozLogName.get());
  }

  // `RUST_LOG_CHILD` is meant for logging child processes only.
  nsAutoCString childRustLog(PR_GetEnv("RUST_LOG_CHILD"));
  if (!childRustLog.IsEmpty()) {
    mLaunchOptions->env_map[ENVIRONMENT_LITERAL("RUST_LOG")] =
        ENVIRONMENT_STRING(childRustLog.get());
  }
}

#if defined(MOZ_WIDGET_GTK)
bool LinuxProcessLauncher::DoSetup() {
  if (!PosixProcessLauncher::DoSetup()) {
    return false;
  }

  if (mProcessType == GeckoProcessType_Content) {
    // disable IM module to avoid sandbox violation
    mLaunchOptions->env_map["GTK_IM_MODULE"] = "gtk-im-context-simple";

    // Disable ATK accessibility code in content processes because it conflicts
    // with the sandbox, and we proxy that information through the main process
    // anyway.
    mLaunchOptions->env_map["NO_AT_BRIDGE"] = "1";
  }

#  ifdef MOZ_SANDBOX
  if (!mTmpDirName.IsEmpty()) {
    // Point a bunch of things that might want to write from content to our
    // shiny new content-process specific tmpdir
    mLaunchOptions->env_map[ENVIRONMENT_LITERAL("TMPDIR")] =
        ENVIRONMENT_STRING(mTmpDirName.get());
    // Partial fix for bug 1380051 (not persistent - should be)
    mLaunchOptions->env_map[ENVIRONMENT_LITERAL("MESA_GLSL_CACHE_DIR")] =
        ENVIRONMENT_STRING(mTmpDirName.get());
  }
#  endif  // MOZ_SANDBOX

  return true;
}
#endif  // MOZ_WIDGET_GTK

#ifdef OS_POSIX
bool PosixProcessLauncher::DoSetup() {
  if (!BaseProcessLauncher::DoSetup()) {
    return false;
  }

  // XPCOM may not be initialized in some subprocesses.  We don't want
  // to initialize XPCOM just for the directory service, especially
  // since LD_LIBRARY_PATH is already set correctly in subprocesses
  // (meaning that we don't need to set that up in the environment).
  if (ShouldHaveDirectoryService()) {
    MOZ_ASSERT(gGREBinPath);
    nsCString path;
    NS_CopyUnicodeToNative(nsDependentString(gGREBinPath), path);
#  if defined(OS_LINUX) || defined(OS_BSD)
    const char* ld_library_path = PR_GetEnv("LD_LIBRARY_PATH");
    nsCString new_ld_lib_path(path.get());

#    ifdef MOZ_WIDGET_GTK
    if (mProcessType == GeckoProcessType_Plugin) {
      new_ld_lib_path.AppendLiteral("/gtk2:");
      new_ld_lib_path.Append(path.get());
    }
#    endif  // MOZ_WIDGET_GTK
    if (ld_library_path && *ld_library_path) {
      new_ld_lib_path.Append(':');
      new_ld_lib_path.Append(ld_library_path);
    }
    mLaunchOptions->env_map["LD_LIBRARY_PATH"] = new_ld_lib_path.get();

#  elif OS_MACOSX  // defined(OS_LINUX) || defined(OS_BSD)
    mLaunchOptions->env_map["DYLD_LIBRARY_PATH"] = path.get();
    // XXX DYLD_INSERT_LIBRARIES should only be set when launching a plugin
    //     process, and has no effect on other subprocesses (the hooks in
    //     libplugin_child_interpose.dylib become noops).  But currently it
    //     gets set when launching any kind of subprocess.
    //
    // Trigger "dyld interposing" for the dylib that contains
    // plugin_child_interpose.mm.  This allows us to hook OS calls in the
    // plugin process (ones that don't work correctly in a background
    // process).  Don't break any other "dyld interposing" that has already
    // been set up by whatever may have launched the browser.
    const char* prevInterpose = PR_GetEnv("DYLD_INSERT_LIBRARIES");
    nsCString interpose;
    if (prevInterpose && strlen(prevInterpose) > 0) {
      interpose.Assign(prevInterpose);
      interpose.Append(':');
    }
    interpose.Append(path.get());
    interpose.AppendLiteral("/libplugin_child_interpose.dylib");
    mLaunchOptions->env_map["DYLD_INSERT_LIBRARIES"] = interpose.get();

    // Prevent connection attempts to diagnosticd(8) to save cycles. Log
    // messages can trigger these connection attempts, but access to
    // diagnosticd is blocked in sandboxed child processes.
#    ifdef MOZ_SANDBOX
    if (mDisableOSActivityMode) {
      mLaunchOptions->env_map["OS_ACTIVITY_MODE"] = "disable";
    }
#    endif  // defined(MOZ_SANDBOX)
#  endif    // defined(OS_LINUX) || defined(OS_BSD)
  }

  FilePath exePath;
  BinPathType pathType = GetPathToBinary(exePath, mProcessType);

  // remap the IPC socket fd to a well-known int, as the OS does for
  // STDOUT_FILENO, for example
  int srcChannelFd, dstChannelFd;
  mChannel->GetClientFileDescriptorMapping(&srcChannelFd, &dstChannelFd);
  mLaunchOptions->fds_to_remap.push_back(
      std::pair<int, int>(srcChannelFd, dstChannelFd));

  // no need for kProcessChannelID, the child process inherits the
  // other end of the socketpair() from us

  mChildArgv.push_back(exePath.value());

  if (pathType == BinPathType::Self) {
    mChildArgv.push_back("-contentproc");
  }

  mChildArgv.insert(mChildArgv.end(), mExtraOpts.begin(), mExtraOpts.end());

  if (mProcessType != GeckoProcessType_GMPlugin) {
#  if defined(MOZ_WIDGET_ANDROID)
    if (Omnijar::IsInitialized()) {
      // Make sure that child processes can find the omnijar
      // See XRE_InitCommandLine in nsAppRunner.cpp
      nsAutoCString path;
      nsCOMPtr<nsIFile> file = Omnijar::GetPath(Omnijar::GRE);
      if (file && NS_SUCCEEDED(file->GetNativePath(path))) {
        mChildArgv.push_back("-greomni");
        mChildArgv.push_back(path.get());
      }
      file = Omnijar::GetPath(Omnijar::APP);
      if (file && NS_SUCCEEDED(file->GetNativePath(path))) {
        mChildArgv.push_back("-appomni");
        mChildArgv.push_back(path.get());
      }
    }
#  endif
    // Add the application directory path (-appdir path)
#  ifdef XP_MACOSX
    AddAppDirToCommandLine(mChildArgv, mAppDir, mProfileDir);
#  else
    AddAppDirToCommandLine(mChildArgv, mAppDir, nullptr);
#  endif
  }

  mChildArgv.push_back(mPidString);

  if (!CrashReporter::IsDummy()) {
#  if defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
    int childCrashFd, childCrashRemapFd;
    if (!CrashReporter::CreateNotificationPipeForChild(&childCrashFd,
                                                       &childCrashRemapFd)) {
      return false;
    }

    if (0 <= childCrashFd) {
      mLaunchOptions->fds_to_remap.push_back(
          std::pair<int, int>(childCrashFd, childCrashRemapFd));
      // "true" == crash reporting enabled
      mChildArgv.push_back("true");
    } else {
      // "false" == crash reporting disabled
      mChildArgv.push_back("false");
    }
#  elif defined(MOZ_WIDGET_COCOA) /* defined(OS_LINUX) || defined(OS_BSD) || \
                                     defined(OS_SOLARIS) */
    mChildArgv.push_back(CrashReporter::GetChildNotificationPipe());
#  endif  // defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  }

  int fd = PR_FileDesc2NativeHandle(mCrashAnnotationWritePipe);
  mLaunchOptions->fds_to_remap.push_back(
      std::make_pair(fd, CrashReporter::GetAnnotationTimeCrashFd()));

#  ifdef MOZ_WIDGET_COCOA
  mChildArgv.push_back(
      static_cast<MacProcessLauncher*>(this)->mMachConnectionName.c_str());
#  endif  // MOZ_WIDGET_COCOA

  mChildArgv.push_back(ChildProcessType());
  return true;
}
#endif  // OS_POSIX

#if defined(MOZ_WIDGET_ANDROID)
RefPtr<ProcessHandlePromise> AndroidProcessLauncher::DoLaunch() {
  return LaunchAndroidService(mProcessType, mChildArgv,
                              mLaunchOptions->fds_to_remap);
}
#endif  // MOZ_WIDGET_ANDROID

#ifdef OS_POSIX
RefPtr<ProcessHandlePromise> PosixProcessLauncher::DoLaunch() {
  ProcessHandle handle = 0;
  if (!base::LaunchApp(mChildArgv, *mLaunchOptions, &handle)) {
    return ProcessHandlePromise::CreateAndReject(LaunchError{}, __func__);
  }
  return ProcessHandlePromise::CreateAndResolve(handle, __func__);
}

bool PosixProcessLauncher::DoFinishLaunch() {
  if (!BaseProcessLauncher::DoFinishLaunch()) {
    return false;
  }

  // We're in the parent and the child was launched. Close the child FD in the
  // parent as soon as possible, which will allow the parent to detect when the
  // child closes its FD (either due to normal exit or due to crash).
  mChannel->CloseClientFileDescriptor();

  return true;
}
#endif  // OS_POSIX

#ifdef XP_MACOSX
bool MacProcessLauncher::DoFinishLaunch() {
  if (!PosixProcessLauncher::DoFinishLaunch()) {
    return false;
  }

  // Wait for the child process to send us its 'task_t' data.
  const int kTimeoutMs = 10000;

  MachReceiveMessage child_message;
  kern_return_t err =
      mParentRecvPort.WaitForMessage(&child_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    std::string errString =
        StringPrintf("0x%x %s", err, mach_error_string(err));
    CHROMIUM_LOG(ERROR) << "parent WaitForMessage() failed: " << errString;
    return false;
  }

  task_t child_task = child_message.GetTranslatedPort(0);
  if (child_task == MACH_PORT_NULL) {
    CHROMIUM_LOG(ERROR) << "parent GetTranslatedPort(0) failed.";
    return false;
  }

  if (child_message.GetTranslatedPort(1) == MACH_PORT_NULL) {
    CHROMIUM_LOG(ERROR) << "parent GetTranslatedPort(1) failed.";
    return false;
  }
  MachPortSender parent_sender(child_message.GetTranslatedPort(1));

  if (child_message.GetTranslatedPort(2) == MACH_PORT_NULL) {
    CHROMIUM_LOG(ERROR) << "parent GetTranslatedPort(2) failed.";
  }
  auto* parent_recv_port_memory_ack =
      new MachPortSender(child_message.GetTranslatedPort(2));

  if (child_message.GetTranslatedPort(3) == MACH_PORT_NULL) {
    CHROMIUM_LOG(ERROR) << "parent GetTranslatedPort(3) failed.";
  }
  auto* parent_send_port_memory =
      new MachPortSender(child_message.GetTranslatedPort(3));

  MachSendMessage parent_message(/* id= */ 0);
  if (!parent_message.AddDescriptor(MachMsgPortDescriptor(bootstrap_port))) {
    CHROMIUM_LOG(ERROR) << "parent AddDescriptor(" << bootstrap_port
                        << ") failed.";
    return false;
  }

  auto* parent_recv_port_memory = new ReceivePort();
  if (!parent_message.AddDescriptor(
          MachMsgPortDescriptor(parent_recv_port_memory->GetPort()))) {
    CHROMIUM_LOG(ERROR) << "parent AddDescriptor("
                        << parent_recv_port_memory->GetPort() << ") failed.";
    return false;
  }

  auto* parent_send_port_memory_ack = new ReceivePort();
  if (!parent_message.AddDescriptor(
          MachMsgPortDescriptor(parent_send_port_memory_ack->GetPort()))) {
    CHROMIUM_LOG(ERROR) << "parent AddDescriptor("
                        << parent_send_port_memory_ack->GetPort()
                        << ") failed.";
    return false;
  }

  err = parent_sender.SendMessage(parent_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    std::string errString =
        StringPrintf("0x%x %s", err, mach_error_string(err));
    CHROMIUM_LOG(ERROR) << "parent SendMessage() failed: " << errString;
    return false;
  }

  SharedMemoryBasic::SetupMachMemory(
      mResults.mHandle, parent_recv_port_memory, parent_recv_port_memory_ack,
      parent_send_port_memory, parent_send_port_memory_ack, false);

  // NB: on OS X, we block much longer than we need to in order to
  // reach this call, waiting for the child process's task_t.  The
  // best way to fix that is to refactor this file, hard.
  mResults.mChildTask = child_task;

  return true;
}
#endif  // XP_MACOSX

#ifdef XP_WIN
bool WindowsProcessLauncher::DoSetup() {
  if (!BaseProcessLauncher::DoSetup()) {
    return false;
  }

  FilePath exePath;
  BinPathType pathType = GetPathToBinary(exePath, mProcessType);

#  if defined(MOZ_SANDBOX) || defined(_ARM64_)
  const bool isGMP = mProcessType == GeckoProcessType_GMPlugin;
  const bool isWidevine = isGMP && Contains(mExtraOpts, "gmp-widevinecdm");
#    if defined(_ARM64_)
  const bool isClearKey = isGMP && Contains(mExtraOpts, "gmp-clearkey");
  const bool isSandboxBroker =
      mProcessType == GeckoProcessType_RemoteSandboxBroker;
  if (isClearKey || isWidevine || isSandboxBroker) {
    // On Windows on ARM64 for ClearKey and Widevine, and for the sandbox
    // launcher process, we want to run the x86 plugin-container.exe in
    // the "i686" subdirectory, instead of the aarch64 plugin-container.exe.
    // So insert "i686" into the exePath.
    exePath = exePath.DirName().AppendASCII("i686").Append(exePath.BaseName());
  }
#    endif  // if defined(_ARM64_)
#  endif    // defined(MOZ_SANDBOX) || defined(_ARM64_)

  mCmdLine.emplace(exePath.ToWStringHack());

  if (pathType == BinPathType::Self) {
    mCmdLine->AppendLooseValue(UTF8ToWide("-contentproc"));
  }

  mCmdLine->AppendSwitchWithValue(switches::kProcessChannelID, mChannelId);

  for (std::vector<std::string>::iterator it = mExtraOpts.begin();
       it != mExtraOpts.end(); ++it) {
    mCmdLine->AppendLooseValue(UTF8ToWide(*it));
  }

#  if defined(MOZ_WIDGET_ANDROID)
  if (Omnijar::IsInitialized()) {
    // Make sure the child process can find the omnijar
    // See XRE_InitCommandLine in nsAppRunner.cpp
    nsAutoString path;
    nsCOMPtr<nsIFile> file = Omnijar::GetPath(Omnijar::GRE);
    if (file && NS_SUCCEEDED(file->GetPath(path))) {
      mCmdLine->AppendLooseValue(UTF8ToWide("-greomni"));
      mCmdLine->AppendLooseValue(path.get());
    }
    file = Omnijar::GetPath(Omnijar::APP);
    if (file && NS_SUCCEEDED(file->GetPath(path))) {
      mCmdLine->AppendLooseValue(UTF8ToWide("-appomni"));
      mCmdLine->AppendLooseValue(path.get());
    }
  }
#  endif

#  if defined(MOZ_SANDBOX)
#    if defined(_ARM64_)
  if (isClearKey || isWidevine)
    mResults.mSandboxBroker = new RemoteSandboxBroker();
  else
#    endif  // if defined(_ARM64_)
    mResults.mSandboxBroker = new SandboxBroker();

  // XXX: Bug 1124167: We should get rid of the process specific logic for
  // sandboxing in this class at some point. Unfortunately it will take a bit
  // of reorganizing so I don't think this patch is the right time.
  switch (mProcessType) {
    case GeckoProcessType_Content:
      if (mSandboxLevel > 0) {
        // For now we treat every failure as fatal in
        // SetSecurityLevelForContentProcess and just crash there right away.
        // Should this change in the future then we should also handle the error
        // here.
        mResults.mSandboxBroker->SetSecurityLevelForContentProcess(
            mSandboxLevel, mIsFileContent);
        mUseSandbox = true;
      }
      break;
    case GeckoProcessType_Plugin:
      if (mSandboxLevel > 0 && !PR_GetEnv("MOZ_DISABLE_NPAPI_SANDBOX")) {
        if (!mResults.mSandboxBroker->SetSecurityLevelForPluginProcess(
                mSandboxLevel)) {
          return false;
        }
        mUseSandbox = true;
      }
      break;
    case GeckoProcessType_IPDLUnitTest:
      // XXX: We don't sandbox this process type yet
      break;
    case GeckoProcessType_GMPlugin:
      if (!PR_GetEnv("MOZ_DISABLE_GMP_SANDBOX")) {
        // The Widevine CDM on Windows can only load at USER_RESTRICTED,
        // not at USER_LOCKDOWN. So look in the command line arguments
        // to see if we're loading the path to the Widevine CDM, and if
        // so use sandbox level USER_RESTRICTED instead of USER_LOCKDOWN.
        auto level =
            isWidevine ? SandboxBroker::Restricted : SandboxBroker::LockDown;
        if (!mResults.mSandboxBroker->SetSecurityLevelForGMPlugin(level)) {
          return false;
        }
        mUseSandbox = true;
      }
      break;
    case GeckoProcessType_GPU:
      if (mSandboxLevel > 0 && !PR_GetEnv("MOZ_DISABLE_GPU_SANDBOX")) {
        // For now we treat every failure as fatal in
        // SetSecurityLevelForGPUProcess and just crash there right away. Should
        // this change in the future then we should also handle the error here.
        mResults.mSandboxBroker->SetSecurityLevelForGPUProcess(mSandboxLevel,
                                                               mProfileDir);
        mUseSandbox = true;
      }
      break;
    case GeckoProcessType_VR:
      if (mSandboxLevel > 0 && !PR_GetEnv("MOZ_DISABLE_VR_SANDBOX")) {
        // TODO: Implement sandbox for VR process, Bug 1430043.
      }
      break;
    case GeckoProcessType_RDD:
      if (!PR_GetEnv("MOZ_DISABLE_RDD_SANDBOX")) {
        if (!mResults.mSandboxBroker->SetSecurityLevelForRDDProcess()) {
          return false;
        }
        mUseSandbox = true;
      }
      break;
    case GeckoProcessType_Socket:
      if (!PR_GetEnv("MOZ_DISABLE_SOCKET_PROCESS_SANDBOX")) {
        if (!mResults.mSandboxBroker->SetSecurityLevelForSocketProcess()) {
          return false;
        }
        mUseSandbox = true;
      }
      break;
    case GeckoProcessType_RemoteSandboxBroker:
      // We don't sandbox the sandbox launcher...
      break;
    case GeckoProcessType_Default:
    default:
      MOZ_CRASH("Bad process type in GeckoChildProcessHost");
      break;
  };

  if (mUseSandbox) {
    for (auto it = mAllowedFilesRead.begin(); it != mAllowedFilesRead.end();
         ++it) {
      mResults.mSandboxBroker->AllowReadFile(it->c_str());
    }
  }
#  endif  // defined(MOZ_SANDBOX)

  // Add the application directory path (-appdir path)
  AddAppDirToCommandLine(mCmdLine.ref(), mAppDir);

  // XXX Command line params past this point are expected to be at
  // the end of the command line string, and in a specific order.
  // See XRE_InitChildProcess in nsEmbedFunction.

  // Win app model id
  mCmdLine->AppendLooseValue(mGroupId.get());

  // Process id
  mCmdLine->AppendLooseValue(UTF8ToWide(mPidString));

  mCmdLine->AppendLooseValue(
      UTF8ToWide(CrashReporter::GetChildNotificationPipe()));

  if (!CrashReporter::IsDummy()) {
    PROsfd h = PR_FileDesc2NativeHandle(mCrashAnnotationWritePipe);
    mLaunchOptions->handles_to_inherit.push_back(reinterpret_cast<HANDLE>(h));
    std::string hStr = std::to_string(h);
    mCmdLine->AppendLooseValue(UTF8ToWide(hStr));
  }

  // Process type
  mCmdLine->AppendLooseValue(UTF8ToWide(ChildProcessType()));

#  ifdef MOZ_SANDBOX
  if (mUseSandbox) {
    // Mark the handles to inherit as inheritable.
    for (HANDLE h : mLaunchOptions->handles_to_inherit) {
      mResults.mSandboxBroker->AddHandleToShare(h);
    }
  }
#  endif  // MOZ_SANDBOX

  return true;
}

RefPtr<ProcessHandlePromise> WindowsProcessLauncher::DoLaunch() {
  ProcessHandle handle = 0;
#  ifdef MOZ_SANDBOX
  if (mUseSandbox) {
    if (mResults.mSandboxBroker->LaunchApp(
            mCmdLine->program().c_str(),
            mCmdLine->command_line_string().c_str(), mLaunchOptions->env_map,
            mProcessType, mEnableSandboxLogging, &handle)) {
      EnvironmentLog("MOZ_PROCESS_LOG")
          .print("==> process %d launched child process %d (%S)\n",
                 base::GetCurrentProcId(), base::GetProcId(handle),
                 mCmdLine->command_line_string().c_str());
      return ProcessHandlePromise::CreateAndResolve(handle, __func__);
    }
    return ProcessHandlePromise::CreateAndReject(LaunchError{}, __func__);
  }
#  endif  // defined(MOZ_SANDBOX)

  if (!base::LaunchApp(mCmdLine.ref(), *mLaunchOptions, &handle)) {
    return ProcessHandlePromise::CreateAndReject(LaunchError{}, __func__);
  }
  return ProcessHandlePromise::CreateAndResolve(handle, __func__);
}

bool WindowsProcessLauncher::DoFinishLaunch() {
  if (!BaseProcessLauncher::DoFinishLaunch()) {
    return false;
  }

#  ifdef MOZ_SANDBOX
  if (!mUseSandbox) {
    // We need to be able to duplicate handles to some types of non-sandboxed
    // child processes.
    switch (mProcessType) {
      case GeckoProcessType_Default:
        MOZ_CRASH("shouldn't be launching a parent process");
      case GeckoProcessType_Plugin:
      case GeckoProcessType_IPDLUnitTest:
        // No handle duplication necessary.
        break;
      default:
        if (!SandboxBroker::AddTargetPeer(mResults.mHandle)) {
          NS_WARNING("Failed to add child process as target peer.");
        }
        break;
    }
  }
#  endif  // MOZ_SANDBOX

  return true;
}
#endif  // XP_WIN

RefPtr<ProcessLaunchPromise> BaseProcessLauncher::FinishLaunch() {
  if (!DoFinishLaunch()) {
    return ProcessLaunchPromise::CreateAndReject(LaunchError{}, __func__);
  }

  MOZ_DIAGNOSTIC_ASSERT(mResults.mHandle);

  CrashReporter::RegisterChildCrashAnnotationFileDescriptor(
      base::GetProcId(mResults.mHandle), mCrashAnnotationReadPipe.forget());

  Telemetry::AccumulateTimeDelta(Telemetry::CHILD_PROCESS_LAUNCH_MS,
                                 mStartTimeStamp);

  return ProcessLaunchPromise::CreateAndResolve(mResults, __func__);
}

bool GeckoChildProcessHost::OpenPrivilegedHandle(base::ProcessId aPid) {
  if (mChildProcessHandle) {
    MOZ_ASSERT(aPid == base::GetProcId(mChildProcessHandle));
    return true;
  }

  return base::OpenPrivilegedProcessHandle(aPid, &mChildProcessHandle);
}

void GeckoChildProcessHost::OnChannelConnected(int32_t peer_pid) {
  if (!OpenPrivilegedHandle(peer_pid)) {
    MOZ_CRASH("can't open handle to child process");
  }
  MonitorAutoLock lock(mMonitor);
  mProcessState = PROCESS_CONNECTED;
  lock.Notify();
}

void GeckoChildProcessHost::OnMessageReceived(IPC::Message&& aMsg) {
  // We never process messages ourself, just save them up for the next
  // listener.
  mQueue.push(std::move(aMsg));
}

void GeckoChildProcessHost::OnChannelError() {
  // Update the process state to an error state if we have a channel
  // error before we're connected. This fixes certain failures,
  // but does not address the full range of possible issues described
  // in the FIXME comment below.
  MonitorAutoLock lock(mMonitor);
  if (mProcessState < PROCESS_CONNECTED) {
    mProcessState = PROCESS_ERROR;
    lock.Notify();
  }
  // FIXME/bug 773925: save up this error for the next listener.
}

RefPtr<ProcessHandlePromise> GeckoChildProcessHost::WhenProcessHandleReady() {
  MOZ_ASSERT(mHandlePromise != nullptr);
  return mHandlePromise;
}

void GeckoChildProcessHost::GetQueuedMessages(std::queue<IPC::Message>& queue) {
  // If this is called off the IO thread, bad things will happen.
  DCHECK(MessageLoopForIO::current());
  swap(queue, mQueue);
  // We expect the next listener to take over processing of our queue.
}

#ifdef MOZ_WIDGET_ANDROID
RefPtr<ProcessHandlePromise> AndroidProcessLauncher::LaunchAndroidService(
    const GeckoProcessType aType, const std::vector<std::string>& argv,
    const base::file_handle_mapping_vector& fds_to_remap) {
  MOZ_RELEASE_ASSERT((2 <= fds_to_remap.size()) && (fds_to_remap.size() <= 5));
  JNIEnv* const env = mozilla::jni::GetEnvForThread();
  MOZ_ASSERT(env);

  const int argvSize = argv.size();
  jni::ObjectArray::LocalRef jargs =
      jni::ObjectArray::New<jni::String>(argvSize);
  for (int ix = 0; ix < argvSize; ix++) {
    jargs->SetElement(ix, jni::StringParam(argv[ix].c_str(), env));
  }

  // XXX: this processing depends entirely on the internals of
  // ContentParent::LaunchSubprocess()
  // GeckoChildProcessHost::PerformAsyncLaunch(), and the order in
  // which they append to fds_to_remap. There must be a better way to do it.
  // See bug 1440207.
  int32_t prefsFd = fds_to_remap[0].first;
  int32_t prefMapFd = fds_to_remap[1].first;
  int32_t ipcFd = fds_to_remap[2].first;
  int32_t crashFd = -1;
  int32_t crashAnnotationFd = -1;
  if (fds_to_remap.size() == 4) {
    crashAnnotationFd = fds_to_remap[3].first;
  }
  if (fds_to_remap.size() == 5) {
    crashFd = fds_to_remap[3].first;
    crashAnnotationFd = fds_to_remap[4].first;
  }

  auto type = java::GeckoProcessType::FromInt(aType);
  auto genericResult = java::GeckoProcessManager::Start(
      type, jargs, prefsFd, prefMapFd, ipcFd, crashFd, crashAnnotationFd);
  auto typedResult = java::GeckoResult::LocalRef(std::move(genericResult));
  return ProcessHandlePromise::FromGeckoResult(typedResult);
}
#endif

#if defined(XP_MACOSX) && defined(MOZ_SANDBOX)
bool GeckoChildProcessHost::AppendMacSandboxParams(StringVector& aArgs) {
  MacSandboxInfo info;
  if (!FillMacSandboxInfo(info)) {
    return false;
  }
  info.AppendAsParams(aArgs);
  return true;
}

// Fill |aInfo| with the flags needed to launch the utility sandbox
bool GeckoChildProcessHost::FillMacSandboxInfo(MacSandboxInfo& aInfo) {
  aInfo.type = GetDefaultMacSandboxType();
  aInfo.shouldLog = Preferences::GetBool("security.sandbox.logging.enabled") ||
                    PR_GetEnv("MOZ_SANDBOX_LOGGING");

  nsAutoCString appPath;
  if (!nsMacUtilsImpl::GetAppPath(appPath)) {
    MOZ_CRASH("Failed to get app path");
  }
  aInfo.appPath.assign(appPath.get());
  return true;
}

void GeckoChildProcessHost::DisableOSActivityMode() {
  mDisableOSActivityMode = true;
}

//
// If early sandbox startup is enabled for this process type, map the
// process type to the sandbox type and enable the sandbox. Returns true
// if no errors were encountered or if early sandbox startup is not
// enabled for this process. Returns false if an error was encountered.
//
/* static */
bool GeckoChildProcessHost::StartMacSandbox(int aArgc, char** aArgv,
                                            std::string& aErrorMessage) {
  MacSandboxType sandboxType = MacSandboxType_Invalid;
  switch (XRE_GetProcessType()) {
    // For now, only support early sandbox startup for content,
    // RDD, and GMP processes. Add case statements for the additional
    // process types once early sandbox startup is implemented for them.
    case GeckoProcessType_Content:
      // Content processes don't use GeckoChildProcessHost
      // to configure sandboxing so hard code the sandbox type.
      sandboxType = MacSandboxType_Content;
      break;
    case GeckoProcessType_RDD:
      sandboxType = RDDProcessHost::GetMacSandboxType();
      break;
    case GeckoProcessType_Socket:
      sandboxType = net::SocketProcessHost::GetMacSandboxType();
      break;
    case GeckoProcessType_GMPlugin:
      sandboxType = gmp::GMPProcessParent::GetMacSandboxType();
      break;
    default:
      return true;
  }
  return mozilla::StartMacSandboxIfEnabled(sandboxType, aArgc, aArgv,
                                           aErrorMessage);
}

#endif /* XP_MACOSX && MOZ_SANDBOX */

/* static */
void GeckoChildProcessHost::GetAll(const GeckoProcessCallback& aCallback) {
  StaticMutexAutoLock lock(sMutex);
  for (GeckoChildProcessHost* gp = sGeckoChildProcessHosts->getFirst(); gp;
       gp = static_cast<mozilla::LinkedListElement<GeckoChildProcessHost>*>(gp)
                ->getNext()) {
    aCallback(gp);
  }
}

RefPtr<ProcessLaunchPromise> BaseProcessLauncher::Launch(
    GeckoChildProcessHost* aHost) {
  AssertIOThread();

  // Initializing the channel needs to happen on the I/O thread, but everything
  // else can run on the launcher thread (or pool), to avoid blocking IPC
  // messages.
  //
  // We avoid passing the host to the launcher thread to reduce the chances of
  // data races with the IO thread (where e.g. OnChannelConnected may run
  // concurrently). The pool currently needs access to the channel, which is not
  // great.
  //
  // It's also unfortunate that we need to work with raw pointers to both the
  // host and the channel. The assumption here is that the host (and therefore
  // the channel) are never torn down until the return promise is resolved or
  // rejected.
  aHost->InitializeChannel();
  mChannel = aHost->GetChannel();
  if (!mChannel) {
    return ProcessLaunchPromise::CreateAndReject(LaunchError{}, __func__);
  }
  mChannelId = aHost->GetChannelId();

  return InvokeAsync(mLaunchThread, this, __func__,
                     &BaseProcessLauncher::PerformAsyncLaunch);
}

}  // namespace ipc
}  // namespace mozilla
