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
#include "chrome/common/mach_ipc_mac.h"
#include "base/rand_util.h"
#include "nsILocalFileMac.h"
#include "SharedMemoryBasic.h"
#endif

#include "MainThreadUtils.h"
#include "mozilla/Sprintf.h"
#include "prenv.h"
#include "nsXPCOMPrivate.h"

#if defined(MOZ_CONTENT_SANDBOX)
#include "mozilla/SandboxSettings.h"
#if defined(XP_MACOSX)
#include "nsAppDirectoryServiceDefs.h"
#endif
#endif

#include "nsExceptionHandler.h"

#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsPrintfCString.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/EnvironmentMap.h"
#include "mozilla/Omnijar.h"
#include "mozilla/Telemetry.h"
#include "ProtocolUtils.h"
#include <sys/stat.h>

#ifdef XP_WIN
#include "nsIWinTaskbar.h"
#include <stdlib.h>
#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

#if defined(MOZ_SANDBOX)
#include "mozilla/Preferences.h"
#include "mozilla/sandboxing/sandboxLogging.h"
#include "WinUtils.h"
#endif
#endif

#if defined(XP_LINUX) && defined(MOZ_SANDBOX)
#include "mozilla/SandboxLaunch.h"
#endif

#include "nsTArray.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsNativeCharsetUtils.h"
#include "nscore.h" // for NS_FREE_PERMANENT_DATA

using mozilla::MonitorAutoLock;
using mozilla::ipc::GeckoChildProcessHost;

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#include "GeneratedJNIWrappers.h"
#include "mozilla/jni/Refs.h"
#include "mozilla/jni/Utils.h"
#endif

static bool
ShouldHaveDirectoryService()
{
  return GeckoProcessType_Default == XRE_GetProcessType();
}

GeckoChildProcessHost::GeckoChildProcessHost(GeckoProcessType aProcessType,
                                             bool aIsFileContent)
  : mProcessType(aProcessType),
    mIsFileContent(aIsFileContent),
    mMonitor("mozilla.ipc.GeckChildProcessHost.mMonitor"),
    mLaunchOptions(MakeUnique<base::LaunchOptions>()),
    mProcessState(CREATING_CHANNEL),
#if defined(MOZ_SANDBOX) && defined(XP_WIN)
    mEnableSandboxLogging(false),
    mSandboxLevel(0),
#endif
    mChildProcessHandle(0)
#if defined(MOZ_WIDGET_COCOA)
  , mChildTask(MACH_PORT_NULL)
#endif
{
    MOZ_COUNT_CTOR(GeckoChildProcessHost);
}

GeckoChildProcessHost::~GeckoChildProcessHost()

{
  AssertIOThread();

  MOZ_COUNT_DTOR(GeckoChildProcessHost);

  if (mChildProcessHandle != 0) {
#if defined(MOZ_WIDGET_COCOA)
    SharedMemoryBasic::CleanupForPid(mChildProcessHandle);
#endif
    ProcessWatcher::EnsureProcessTerminated(mChildProcessHandle
#ifdef NS_FREE_PERMANENT_DATA
    // If we're doing leak logging, shutdown can be slow.
                                            , false // don't "force"
#endif
    );
  }

#if defined(MOZ_WIDGET_COCOA)
  if (mChildTask != MACH_PORT_NULL)
    mach_port_deallocate(mach_task_self(), mChildTask);
#endif
}

//static
auto
GeckoChildProcessHost::GetPathToBinary(FilePath& exePath, GeckoProcessType processType) -> BinaryPathType
{
  if (sRunSelfAsContentProc &&
      (processType == GeckoProcessType_Content || processType == GeckoProcessType_GPU)) {
#if defined(OS_WIN)
    wchar_t exePathBuf[MAXPATHLEN];
    if (!::GetModuleFileNameW(nullptr, exePathBuf, MAXPATHLEN)) {
      MOZ_CRASH("GetModuleFileNameW failed (FIXME)");
    }
#if defined(MOZ_SANDBOX)
    // We need to start the child process using the real path, so that the
    // sandbox policy rules will match for DLLs loaded from the bin dir after
    // we have lowered the sandbox.
    std::wstring exePathStr = exePathBuf;
    if (widget::WinUtils::ResolveJunctionPointsAndSymLinks(exePathStr)) {
      exePath = FilePath::FromWStringHack(exePathStr);
    } else
#endif
    {
      exePath = FilePath::FromWStringHack(exePathBuf);
    }
#elif defined(OS_POSIX)
    exePath = FilePath(CommandLine::ForCurrentProcess()->argv()[0]);
#else
#  error Sorry; target OS not supported yet.
#endif
    return BinaryPathType::Self;
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
    exePath = FilePath::FromWStringHack(CommandLine::ForCurrentProcess()->program());
#else
    exePath = FilePath(CommandLine::ForCurrentProcess()->argv()[0]);
#endif
    exePath = exePath.DirName();
  }

  exePath = exePath.AppendASCII(MOZ_CHILD_PROCESS_NAME);

  return BinaryPathType::PluginContainer;
}

#ifdef MOZ_WIDGET_COCOA
class AutoCFTypeObject {
public:
  explicit AutoCFTypeObject(CFTypeRef object)
  {
    mObject = object;
  }
  ~AutoCFTypeObject()
  {
    ::CFRelease(mObject);
  }
private:
  CFTypeRef mObject;
};
#endif

// We start the unique IDs at 1 so that 0 can be used to mean that
// a component has no unique ID assigned to it.
uint32_t GeckoChildProcessHost::sNextUniqueID = 1;

/* static */
uint32_t
GeckoChildProcessHost::GetUniqueID()
{
  return sNextUniqueID++;
}

void
GeckoChildProcessHost::PrepareLaunch()
{
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

#if defined(MOZ_CONTENT_SANDBOX)
  // We need to get the pref here as the process is launched off main thread.
  if (mProcessType == GeckoProcessType_Content) {
    mSandboxLevel = GetEffectiveContentSandboxLevel();
    mEnableSandboxLogging =
      Preferences::GetBool("security.sandbox.logging.enabled");

    // We currently have to whitelist certain paths for tests to work in some
    // development configurations.
    nsAutoString readPaths;
    nsresult rv =
      Preferences::GetString("security.sandbox.content.read_path_whitelist",
                             readPaths);
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
#endif

#if defined(MOZ_SANDBOX)
  // For other process types we can't rely on them being launched on main
  // thread and they may not have access to prefs in the child process, so allow
  // them to turn on logging via an environment variable.
  mEnableSandboxLogging = mEnableSandboxLogging
                          || !!PR_GetEnv("MOZ_SANDBOX_LOGGING");
#endif
#endif
}

#ifdef XP_WIN
void GeckoChildProcessHost::InitWindowsGroupID()
{
  // On Win7+, pass the application user model to the child, so it can
  // register with it. This insures windows created by the container
  // properly group with the parent app on the Win7 taskbar.
  nsCOMPtr<nsIWinTaskbar> taskbarInfo =
    do_GetService(NS_TASKBAR_CONTRACTID);
  if (taskbarInfo) {
    bool isSupported = false;
    taskbarInfo->GetAvailable(&isSupported);
    nsAutoString appId;
    if (isSupported && NS_SUCCEEDED(taskbarInfo->GetDefaultGroupId(appId))) {
      mGroupId.Append(appId);
    } else {
      mGroupId.Assign('-');
    }
  }
}
#endif

bool
GeckoChildProcessHost::SyncLaunch(std::vector<std::string> aExtraOpts, int aTimeoutMs)
{
  PrepareLaunch();

  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  NS_ASSERTION(MessageLoop::current() != ioLoop, "sync launch from the IO thread NYI");

  ioLoop->PostTask(NewNonOwningRunnableMethod<std::vector<std::string>>(
    "ipc::GeckoChildProcessHost::RunPerformAsyncLaunch",
    this,
    &GeckoChildProcessHost::RunPerformAsyncLaunch,
    aExtraOpts));

  return WaitUntilConnected(aTimeoutMs);
}

bool
GeckoChildProcessHost::AsyncLaunch(std::vector<std::string> aExtraOpts)
{
  PrepareLaunch();

  MessageLoop* ioLoop = XRE_GetIOMessageLoop();

  ioLoop->PostTask(NewNonOwningRunnableMethod<std::vector<std::string>>(
    "ipc::GeckoChildProcessHost::RunPerformAsyncLaunch",
    this,
    &GeckoChildProcessHost::RunPerformAsyncLaunch,
    aExtraOpts));

  // This may look like the sync launch wait, but we only delay as
  // long as it takes to create the channel.
  MonitorAutoLock lock(mMonitor);
  while (mProcessState < CHANNEL_INITIALIZED) {
    lock.Wait();
  }

  return true;
}

bool
GeckoChildProcessHost::WaitUntilConnected(int32_t aTimeoutMs)
{
  AUTO_PROFILER_LABEL("GeckoChildProcessHost::WaitUntilConnected", OTHER);

  // NB: this uses a different mechanism than the chromium parent
  // class.
  PRIntervalTime timeoutTicks = (aTimeoutMs > 0) ?
    PR_MillisecondsToInterval(aTimeoutMs) : PR_INTERVAL_NO_TIMEOUT;

  MonitorAutoLock lock(mMonitor);
  PRIntervalTime waitStart = PR_IntervalNow();
  PRIntervalTime current;

  // We'll receive several notifications, we need to exit when we
  // have either successfully launched or have timed out.
  while (mProcessState != PROCESS_CONNECTED) {
    // If there was an error then return it, don't wait out the timeout.
    if (mProcessState == PROCESS_ERROR) {
      break;
    }

    lock.Wait(timeoutTicks);

    if (timeoutTicks != PR_INTERVAL_NO_TIMEOUT) {
      current = PR_IntervalNow();
      PRIntervalTime elapsed = current - waitStart;
      if (elapsed > timeoutTicks) {
        break;
      }
      timeoutTicks = timeoutTicks - elapsed;
      waitStart = current;
    }
  }

  return mProcessState == PROCESS_CONNECTED;
}

bool
GeckoChildProcessHost::LaunchAndWaitForProcessHandle(StringVector aExtraOpts)
{
  PrepareLaunch();

  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  ioLoop->PostTask(NewNonOwningRunnableMethod<std::vector<std::string>>(
    "ipc::GeckoChildProcessHost::RunPerformAsyncLaunch",
    this,
    &GeckoChildProcessHost::RunPerformAsyncLaunch,
    aExtraOpts));

  MonitorAutoLock lock(mMonitor);
  while (mProcessState < PROCESS_CREATED) {
    lock.Wait();
  }
  MOZ_ASSERT(mProcessState == PROCESS_ERROR || mChildProcessHandle);

  return mProcessState < PROCESS_ERROR;
}

void
GeckoChildProcessHost::InitializeChannel()
{
  CreateChannel();

  MonitorAutoLock lock(mMonitor);
  mProcessState = CHANNEL_INITIALIZED;
  lock.Notify();
}

void
GeckoChildProcessHost::Join()
{
  AssertIOThread();

  if (!mChildProcessHandle) {
    return;
  }

  // If this fails, there's nothing we can do.
  base::KillProcess(mChildProcessHandle, 0, /*wait*/true);
  SetAlreadyDead();
}

void
GeckoChildProcessHost::SetAlreadyDead()
{
  if (mChildProcessHandle &&
      mChildProcessHandle != kInvalidProcessHandle) {
    base::CloseProcessHandle(mChildProcessHandle);
  }

  mChildProcessHandle = 0;
}

int32_t GeckoChildProcessHost::mChildCounter = 0;

void
GeckoChildProcessHost::GetChildLogName(const char* origLogName,
                                       nsACString &buffer)
{
#ifdef XP_WIN
  // On Windows we must expand relative paths because sandboxing rules
  // bound only to full paths.  fopen fowards to NtCreateFile which checks
  // the path against the sanboxing rules as passed to fopen (left relative).
  char absPath[MAX_PATH + 2];
  if (_fullpath(absPath, origLogName, sizeof(absPath))) {
#ifdef MOZ_SANDBOX
    // We need to make sure the child log name doesn't contain any junction
    // points or symlinks or the sandbox will reject rules to allow writing.
    std::wstring resolvedPath(NS_ConvertUTF8toUTF16(absPath).get());
    if (widget::WinUtils::ResolveJunctionPointsAndSymLinks(resolvedPath)) {
      AppendUTF16toUTF8(resolvedPath.c_str(), buffer);
    } else
#endif
    {
      buffer.Append(absPath);
    }
  } else
#endif
  {
    buffer.Append(origLogName);
  }

  // Append child-specific postfix to name
  buffer.AppendLiteral(".child-");
  buffer.AppendInt(mChildCounter);
}

bool
GeckoChildProcessHost::PerformAsyncLaunch(std::vector<std::string> aExtraOpts)
{
#ifdef MOZ_GECKO_PROFILER
  AutoSetProfilerEnvVarsForChildProcess profilerEnvironment;
#endif

  // - Note: this code is not called re-entrantly, nor are restoreOrig*LogName
  //   or mChildCounter touched by any other thread, so this is safe.
  ++mChildCounter;

  const char* origNSPRLogName = PR_GetEnv("NSPR_LOG_FILE");
  const char* origMozLogName = PR_GetEnv("MOZ_LOG_FILE");

  if (origNSPRLogName) {
    nsAutoCString nsprLogName;
    GetChildLogName(origNSPRLogName, nsprLogName);
    mLaunchOptions->env_map[ENVIRONMENT_LITERAL("NSPR_LOG_FILE")]
        = ENVIRONMENT_STRING(nsprLogName);
  }
  if (origMozLogName) {
    nsAutoCString mozLogName;
    GetChildLogName(origMozLogName, mozLogName);
    mLaunchOptions->env_map[ENVIRONMENT_LITERAL("MOZ_LOG_FILE")]
        = ENVIRONMENT_STRING(mozLogName);
  }

  // `RUST_LOG_CHILD` is meant for logging child processes only.
  nsAutoCString childRustLog(PR_GetEnv("RUST_LOG_CHILD"));
  if (!childRustLog.IsEmpty()) {
    mLaunchOptions->env_map[ENVIRONMENT_LITERAL("RUST_LOG")]
        = ENVIRONMENT_STRING(childRustLog);
  }

  return PerformAsyncLaunchInternal(aExtraOpts);
}

bool
GeckoChildProcessHost::RunPerformAsyncLaunch(std::vector<std::string> aExtraOpts)
{
  InitializeChannel();

  bool ok = PerformAsyncLaunch(aExtraOpts);
  if (!ok) {
    // WaitUntilConnected might be waiting for us to signal.
    // If something failed let's set the error state and notify.
    MonitorAutoLock lock(mMonitor);
    mProcessState = PROCESS_ERROR;
    lock.Notify();
    CHROMIUM_LOG(ERROR) << "Failed to launch " <<
      XRE_ChildProcessTypeToString(mProcessType) << " subprocess";
    Telemetry::Accumulate(Telemetry::SUBPROCESS_LAUNCH_FAILURE,
      nsDependentCString(XRE_ChildProcessTypeToString(mProcessType)));
  }
  return ok;
}

void
#if defined(XP_WIN)
AddAppDirToCommandLine(CommandLine& aCmdLine)
#else
AddAppDirToCommandLine(std::vector<std::string>& aCmdLine)
#endif
{
  // Content processes need access to application resources, so pass
  // the full application directory path to the child process.
  if (ShouldHaveDirectoryService()) {
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    NS_ASSERTION(directoryService, "Expected XPCOM to be available");
    if (directoryService) {
      nsCOMPtr<nsIFile> appDir;
      // NS_XPCOM_CURRENT_PROCESS_DIR really means the app dir, not the
      // current process dir.
      nsresult rv = directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                                          NS_GET_IID(nsIFile),
                                          getter_AddRefs(appDir));
      if (NS_SUCCEEDED(rv)) {
#if defined(XP_WIN)
        nsString path;
        MOZ_ALWAYS_SUCCEEDS(appDir->GetPath(path));
        aCmdLine.AppendLooseValue(UTF8ToWide("-appdir"));
        std::wstring wpath(path.get());
        aCmdLine.AppendLooseValue(wpath);
#else
        nsAutoCString path;
        MOZ_ALWAYS_SUCCEEDS(appDir->GetNativePath(path));
        aCmdLine.push_back("-appdir");
        aCmdLine.push_back(path.get());
#endif
      }

#if defined(XP_MACOSX) && defined(MOZ_CONTENT_SANDBOX)
      // Full path to the profile dir
      nsCOMPtr<nsIFile> profileDir;
      rv = directoryService->Get(NS_APP_USER_PROFILE_50_DIR,
                                 NS_GET_IID(nsIFile),
                                 getter_AddRefs(profileDir));
      if (NS_SUCCEEDED(rv)) {
        nsAutoCString path;
        MOZ_ALWAYS_SUCCEEDS(profileDir->GetNativePath(path));
        aCmdLine.push_back("-profile");
        aCmdLine.push_back(path.get());
      }
#endif
    }
  }
}

bool
GeckoChildProcessHost::PerformAsyncLaunchInternal(std::vector<std::string>& aExtraOpts)
{
  // We rely on the fact that InitializeChannel() has already been processed
  // on the IO thread before this point is reached.
  if (!GetChannel()) {
    return false;
  }

  base::ProcessHandle process = 0;

  // send the child the PID so that it can open a ProcessHandle back to us.
  // probably don't want to do this in the long run
  char pidstring[32];
  SprintfLiteral(pidstring, "%d", base::GetCurrentProcId());

  const char* const childProcessType =
      XRE_ChildProcessTypeToString(mProcessType);

//--------------------------------------------------
#if defined(OS_POSIX)
  // For POSIX, we have to be extremely anal about *not* using
  // std::wstring in code compiled with Mozilla's -fshort-wchar
  // configuration, because chromium is compiled with -fno-short-wchar
  // and passing wstrings from one config to the other is unsafe.  So
  // we split the logic here.

# if defined(OS_POSIX)
#  if defined(MOZ_WIDGET_GTK)
  if (mProcessType == GeckoProcessType_Content) {
    // disable IM module to avoid sandbox violation
    mLaunchOptions->env_map["GTK_IM_MODULE"] = "gtk-im-context-simple";

    // Disable ATK accessibility code in content processes because it conflicts
    // with the sandbox, and we proxy that information through the main process
    // anyway.
    mLaunchOptions->env_map["NO_AT_BRIDGE"] = "1";
  }
#  endif // defined(MOZ_WIDGET_GTK)

  // XPCOM may not be initialized in some subprocesses.  We don't want
  // to initialize XPCOM just for the directory service, especially
  // since LD_LIBRARY_PATH is already set correctly in subprocesses
  // (meaning that we don't need to set that up in the environment).
  if (ShouldHaveDirectoryService()) {
    MOZ_ASSERT(gGREBinPath);
    nsCString path;
    NS_CopyUnicodeToNative(nsDependentString(gGREBinPath), path);
#  if defined(OS_LINUX) || defined(OS_BSD)
    const char *ld_library_path = PR_GetEnv("LD_LIBRARY_PATH");
    nsCString new_ld_lib_path(path.get());

#   ifdef MOZ_WIDGET_GTK
    if (mProcessType == GeckoProcessType_Plugin) {
      new_ld_lib_path.AppendLiteral("/gtk2:");
      new_ld_lib_path.Append(path.get());
    }
#   endif // MOZ_WIDGET_GTK
    if (ld_library_path && *ld_library_path) {
      new_ld_lib_path.Append(':');
      new_ld_lib_path.Append(ld_library_path);
    }
    mLaunchOptions->env_map["LD_LIBRARY_PATH"] = new_ld_lib_path.get();

#  elif OS_MACOSX // defined(OS_LINUX) || defined(OS_BSD)
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
#  endif // defined(OS_LINUX) || defined(OS_BSD)
  }
# endif // defined(OS_POSIX)

  FilePath exePath;
  BinaryPathType pathType = GetPathToBinary(exePath, mProcessType);

  // remap the IPC socket fd to a well-known int, as the OS does for
  // STDOUT_FILENO, for example
  int srcChannelFd, dstChannelFd;
  channel().GetClientFileDescriptorMapping(&srcChannelFd, &dstChannelFd);
  mLaunchOptions->fds_to_remap
    .push_back(std::pair<int,int>(srcChannelFd, dstChannelFd));

  // no need for kProcessChannelID, the child process inherits the
  // other end of the socketpair() from us

  std::vector<std::string> childArgv;

  childArgv.push_back(exePath.value());

  if (pathType == BinaryPathType::Self) {
    childArgv.push_back("-contentproc");
  }

  childArgv.insert(childArgv.end(), aExtraOpts.begin(), aExtraOpts.end());

  if (Omnijar::IsInitialized()) {
    // Make sure that child processes can find the omnijar
    // See XRE_InitCommandLine in nsAppRunner.cpp
    nsAutoCString path;
    nsCOMPtr<nsIFile> file = Omnijar::GetPath(Omnijar::GRE);
    if (file && NS_SUCCEEDED(file->GetNativePath(path))) {
      childArgv.push_back("-greomni");
      childArgv.push_back(path.get());
    }
    file = Omnijar::GetPath(Omnijar::APP);
    if (file && NS_SUCCEEDED(file->GetNativePath(path))) {
      childArgv.push_back("-appomni");
      childArgv.push_back(path.get());
    }
  }

  // Add the application directory path (-appdir path)
  AddAppDirToCommandLine(childArgv);

  // Tmp dir that the GPU process should use for crash reports. This arg is
  // always populated (but possibly with an empty value) for a GPU child process.
  if (mProcessType == GeckoProcessType_GPU) {
    nsCOMPtr<nsIFile> file;
    CrashReporter::GetChildProcessTmpDir(getter_AddRefs(file));
    nsAutoCString path;
    if (file) {
      file->GetNativePath(path);
    }
    childArgv.push_back(path.get());
  }

  childArgv.push_back(pidstring);

  if (!CrashReporter::IsDummy()) {
#if defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
    int childCrashFd, childCrashRemapFd;
    if (!CrashReporter::CreateNotificationPipeForChild(&childCrashFd,
                                                       &childCrashRemapFd)) {
      return false;
    }

    if (0 <= childCrashFd) {
      mLaunchOptions->fds_to_remap
        .push_back(std::pair<int,int>(childCrashFd, childCrashRemapFd));
      // "true" == crash reporting enabled
      childArgv.push_back("true");
    } else {
      // "false" == crash reporting disabled
      childArgv.push_back("false");
    }
#elif defined(MOZ_WIDGET_COCOA) // defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
    childArgv.push_back(CrashReporter::GetChildNotificationPipe());
#endif  // defined(OS_LINUX) || defined(OS_BSD) || defined(OS_SOLARIS)
  }

# ifdef MOZ_WIDGET_COCOA
  // Add a mach port to the command line so the child can communicate its
  // 'task_t' back to the parent.
  //
  // Put a random number into the channel name, so that a compromised renderer
  // can't pretend being the child that's forked off.
  std::string mach_connection_name = StringPrintf("org.mozilla.machname.%d",
                                                  base::RandInt(0, std::numeric_limits<int>::max()));
  childArgv.push_back(mach_connection_name.c_str());
# endif // MOZ_WIDGET_COCOA

  childArgv.push_back(childProcessType);

# if defined(MOZ_WIDGET_ANDROID)
  LaunchAndroidService(childProcessType, childArgv,
                       mLaunchOptions->fds_to_remap, &process);
# else // goes with defined(MOZ_WIDGET_ANDROID)
  base::LaunchApp(childArgv, *mLaunchOptions, &process);
# endif // defined(MOZ_WIDGET_ANDROID)

  // We're in the parent and the child was launched. Close the child FD in the
  // parent as soon as possible, which will allow the parent to detect when the
  // child closes its FD (either due to normal exit or due to crash).
  GetChannel()->CloseClientFileDescriptor();

# ifdef MOZ_WIDGET_COCOA
  // Wait for the child process to send us its 'task_t' data.
  const int kTimeoutMs = 10000;

  MachReceiveMessage child_message;
  ReceivePort parent_recv_port(mach_connection_name.c_str());
  kern_return_t err = parent_recv_port.WaitForMessage(&child_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    std::string errString = StringPrintf("0x%x %s", err, mach_error_string(err));
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
  auto* parent_recv_port_memory_ack = new MachPortSender(child_message.GetTranslatedPort(2));

  if (child_message.GetTranslatedPort(3) == MACH_PORT_NULL) {
    CHROMIUM_LOG(ERROR) << "parent GetTranslatedPort(3) failed.";
  }
  auto* parent_send_port_memory = new MachPortSender(child_message.GetTranslatedPort(3));

  MachSendMessage parent_message(/* id= */0);
  if (!parent_message.AddDescriptor(MachMsgPortDescriptor(bootstrap_port))) {
    CHROMIUM_LOG(ERROR) << "parent AddDescriptor(" << bootstrap_port << ") failed.";
    return false;
  }

  auto* parent_recv_port_memory = new ReceivePort();
  if (!parent_message.AddDescriptor(MachMsgPortDescriptor(parent_recv_port_memory->GetPort()))) {
    CHROMIUM_LOG(ERROR) << "parent AddDescriptor(" << parent_recv_port_memory->GetPort() << ") failed.";
    return false;
  }

  auto* parent_send_port_memory_ack = new ReceivePort();
  if (!parent_message.AddDescriptor(MachMsgPortDescriptor(parent_send_port_memory_ack->GetPort()))) {
    CHROMIUM_LOG(ERROR) << "parent AddDescriptor(" << parent_send_port_memory_ack->GetPort() << ") failed.";
    return false;
  }

  err = parent_sender.SendMessage(parent_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    std::string errString = StringPrintf("0x%x %s", err, mach_error_string(err));
    CHROMIUM_LOG(ERROR) << "parent SendMessage() failed: " << errString;
    return false;
  }

  SharedMemoryBasic::SetupMachMemory(process, parent_recv_port_memory, parent_recv_port_memory_ack,
                                     parent_send_port_memory, parent_send_port_memory_ack, false);

# endif // MOZ_WIDGET_COCOA

//--------------------------------------------------
#elif defined(OS_WIN) // defined(OS_POSIX)

  FilePath exePath;
  BinaryPathType pathType = GetPathToBinary(exePath, mProcessType);

  CommandLine cmdLine(exePath.ToWStringHack());

  if (pathType == BinaryPathType::Self) {
    cmdLine.AppendLooseValue(UTF8ToWide("-contentproc"));
  }

  cmdLine.AppendSwitchWithValue(switches::kProcessChannelID, channel_id());

  for (std::vector<std::string>::iterator it = aExtraOpts.begin();
       it != aExtraOpts.end();
       ++it) {
      cmdLine.AppendLooseValue(UTF8ToWide(*it));
  }

  if (Omnijar::IsInitialized()) {
    // Make sure the child process can find the omnijar
    // See XRE_InitCommandLine in nsAppRunner.cpp
    nsAutoString path;
    nsCOMPtr<nsIFile> file = Omnijar::GetPath(Omnijar::GRE);
    if (file && NS_SUCCEEDED(file->GetPath(path))) {
      cmdLine.AppendLooseValue(UTF8ToWide("-greomni"));
      cmdLine.AppendLooseValue(path.get());
    }
    file = Omnijar::GetPath(Omnijar::APP);
    if (file && NS_SUCCEEDED(file->GetPath(path))) {
      cmdLine.AppendLooseValue(UTF8ToWide("-appomni"));
      cmdLine.AppendLooseValue(path.get());
    }
  }

# if defined(MOZ_SANDBOX)
  bool shouldSandboxCurrentProcess = false;

  // XXX: Bug 1124167: We should get rid of the process specific logic for
  // sandboxing in this class at some point. Unfortunately it will take a bit
  // of reorganizing so I don't think this patch is the right time.
  switch (mProcessType) {
    case GeckoProcessType_Content:
#  if defined(MOZ_CONTENT_SANDBOX)
      if (mSandboxLevel > 0) {
        // For now we treat every failure as fatal in SetSecurityLevelForContentProcess
        // and just crash there right away. Should this change in the future then we
        // should also handle the error here.
        mSandboxBroker.SetSecurityLevelForContentProcess(mSandboxLevel,
                                                         mIsFileContent);
        shouldSandboxCurrentProcess = true;
      }
#  endif // defined(MOZ_CONTENT_SANDBOX)
      break;
    case GeckoProcessType_Plugin:
      if (mSandboxLevel > 0 &&
          !PR_GetEnv("MOZ_DISABLE_NPAPI_SANDBOX")) {
        bool ok = mSandboxBroker.SetSecurityLevelForPluginProcess(mSandboxLevel);
        if (!ok) {
          return false;
        }
        shouldSandboxCurrentProcess = true;
      }
      break;
#ifdef MOZ_ENABLE_SKIA_PDF
    case GeckoProcessType_PDFium:
      if (!PR_GetEnv("MOZ_DISABLE_PDFIUM_SANDBOX")) {
        bool ok = mSandboxBroker.SetSecurityLevelForPDFiumProcess();
        if (!ok) {
          return false;
        }
        shouldSandboxCurrentProcess = true;
      }
      break;
#endif
    case GeckoProcessType_IPDLUnitTest:
      // XXX: We don't sandbox this process type yet
      break;
    case GeckoProcessType_GMPlugin:
      if (!PR_GetEnv("MOZ_DISABLE_GMP_SANDBOX")) {
        // The Widevine CDM on Windows can only load at USER_RESTRICTED,
        // not at USER_LOCKDOWN. So look in the command line arguments
        // to see if we're loading the path to the Widevine CDM, and if
        // so use sandbox level USER_RESTRICTED instead of USER_LOCKDOWN.
        bool isWidevine = std::any_of(aExtraOpts.begin(), aExtraOpts.end(),
          [](const std::string arg) { return arg.find("gmp-widevinecdm") != std::string::npos; });
        auto level = isWidevine ? SandboxBroker::Restricted : SandboxBroker::LockDown;
        bool ok = mSandboxBroker.SetSecurityLevelForGMPlugin(level);
        if (!ok) {
          return false;
        }
        shouldSandboxCurrentProcess = true;
      }
      break;
    case GeckoProcessType_GPU:
      if (mSandboxLevel > 0 && !PR_GetEnv("MOZ_DISABLE_GPU_SANDBOX")) {
        // For now we treat every failure as fatal in SetSecurityLevelForGPUProcess
        // and just crash there right away. Should this change in the future then we
        // should also handle the error here.
        mSandboxBroker.SetSecurityLevelForGPUProcess(mSandboxLevel);
        shouldSandboxCurrentProcess = true;
      }
      break;
    case GeckoProcessType_Default:
    default:
      MOZ_CRASH("Bad process type in GeckoChildProcessHost");
      break;
  };

  if (shouldSandboxCurrentProcess) {
    for (auto it = mAllowedFilesRead.begin();
         it != mAllowedFilesRead.end();
         ++it) {
      mSandboxBroker.AllowReadFile(it->c_str());
    }
  }
# endif // defined(MOZ_SANDBOX)

  // Add the application directory path (-appdir path)
  AddAppDirToCommandLine(cmdLine);

  // XXX Command line params past this point are expected to be at
  // the end of the command line string, and in a specific order.
  // See XRE_InitChildProcess in nsEmbedFunction.

  // Win app model id
  cmdLine.AppendLooseValue(mGroupId.get());

  // Tmp dir that the GPU process should use for crash reports. This arg is
  // always populated (but possibly with an empty value) for a GPU child process.
  if (mProcessType == GeckoProcessType_GPU) {
    nsCOMPtr<nsIFile> file;
    CrashReporter::GetChildProcessTmpDir(getter_AddRefs(file));
    nsString path;
    if (file) {
      MOZ_ALWAYS_SUCCEEDS(file->GetPath(path));
    }
    std::wstring wpath(path.get());
    cmdLine.AppendLooseValue(wpath);
  }

  // Process id
  cmdLine.AppendLooseValue(UTF8ToWide(pidstring));

  cmdLine.AppendLooseValue(
    UTF8ToWide(CrashReporter::GetChildNotificationPipe()));

  // Process type
  cmdLine.AppendLooseValue(UTF8ToWide(childProcessType));

# if defined(MOZ_SANDBOX)
  if (shouldSandboxCurrentProcess) {
    if (mSandboxBroker.LaunchApp(cmdLine.program().c_str(),
                                 cmdLine.command_line_string().c_str(),
                                 mLaunchOptions->env_map,
                                 mProcessType,
                                 mEnableSandboxLogging,
                                 &process)) {
      EnvironmentLog("MOZ_PROCESS_LOG").print(
        "==> process %d launched child process %d (%S)\n",
        base::GetCurrentProcId(), base::GetProcId(process),
        cmdLine.command_line_string().c_str());
    }
  } else
# endif // defined(MOZ_SANDBOX)
  {
    base::LaunchApp(cmdLine, *mLaunchOptions, &process);

# ifdef MOZ_SANDBOX
    // We need to be able to duplicate handles to some types of non-sandboxed
    // child processes.
    if (mProcessType == GeckoProcessType_Content ||
        mProcessType == GeckoProcessType_GPU ||
        mProcessType == GeckoProcessType_GMPlugin) {
      if (!mSandboxBroker.AddTargetPeer(process)) {
        NS_WARNING("Failed to add content process as target peer.");
      }
    }
# endif // MOZ_SANDBOX
  }

#else // goes with defined(OS_POSIX)
#  error Sorry
#endif // defined(OS_POSIX)

  if (!process) {
    return false;
  }
  // NB: on OS X, we block much longer than we need to in order to
  // reach this call, waiting for the child process's task_t.  The
  // best way to fix that is to refactor this file, hard.
#if defined(MOZ_WIDGET_COCOA)
  mChildTask = child_task;
#endif // defined(MOZ_WIDGET_COCOA)

  if (!OpenPrivilegedHandle(base::GetProcId(process))
#ifdef XP_WIN
      // If we failed in opening the process handle, try harder by duplicating
      // one.
      && !::DuplicateHandle(::GetCurrentProcess(), process,
                            ::GetCurrentProcess(), &mChildProcessHandle,
                            PROCESS_DUP_HANDLE | PROCESS_TERMINATE |
                            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ |
                            SYNCHRONIZE,
                            FALSE, 0)
#endif // XP_WIN
     ) {
    MOZ_CRASH("cannot open handle to child process");
  }
  MonitorAutoLock lock(mMonitor);
  mProcessState = PROCESS_CREATED;
  lock.Notify();

  mLaunchOptions = nullptr;
  return true;
}

bool
GeckoChildProcessHost::OpenPrivilegedHandle(base::ProcessId aPid)
{
  if (mChildProcessHandle) {
    MOZ_ASSERT(aPid == base::GetProcId(mChildProcessHandle));
    return true;
  }

  return base::OpenPrivilegedProcessHandle(aPid, &mChildProcessHandle);
}

void
GeckoChildProcessHost::OnChannelConnected(int32_t peer_pid)
{
  if (!OpenPrivilegedHandle(peer_pid)) {
    MOZ_CRASH("can't open handle to child process");
  }
  MonitorAutoLock lock(mMonitor);
  mProcessState = PROCESS_CONNECTED;
  lock.Notify();
}

void
GeckoChildProcessHost::OnMessageReceived(IPC::Message&& aMsg)
{
  // We never process messages ourself, just save them up for the next
  // listener.
  mQueue.push(Move(aMsg));
}

void
GeckoChildProcessHost::OnChannelError()
{
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

void
GeckoChildProcessHost::GetQueuedMessages(std::queue<IPC::Message>& queue)
{
  // If this is called off the IO thread, bad things will happen.
  DCHECK(MessageLoopForIO::current());
  swap(queue, mQueue);
  // We expect the next listener to take over processing of our queue.
}

bool GeckoChildProcessHost::sRunSelfAsContentProc(false);

#ifdef MOZ_WIDGET_ANDROID
void
GeckoChildProcessHost::LaunchAndroidService(const char* type,
                                            const std::vector<std::string>& argv,
                                            const base::file_handle_mapping_vector& fds_to_remap,
                                            ProcessHandle* process_handle)
{
  MOZ_ASSERT((fds_to_remap.size() > 0) && (fds_to_remap.size() <= 2));
  JNIEnv* const env = mozilla::jni::GetEnvForThread();
  MOZ_ASSERT(env);

  const int argvSize = argv.size();
  jni::ObjectArray::LocalRef jargs = jni::ObjectArray::New<jni::String>(argvSize);
  for (int ix = 0; ix < argvSize; ix++) {
    jargs->SetElement(ix, jni::StringParam(argv[ix].c_str(), env));
  }
  base::file_handle_mapping_vector::const_iterator it = fds_to_remap.begin();
  int32_t ipcFd = it->first;
  it++;
  // If the Crash Reporter is disabled, there will not be a second file descriptor.
  int32_t crashFd = (it != fds_to_remap.end()) ? it->first : -1;
  int32_t handle = java::GeckoProcessManager::Start(type, jargs, crashFd, ipcFd);

  if (process_handle) {
    *process_handle = handle;
  }
}
#endif
