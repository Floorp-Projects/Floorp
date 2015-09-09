/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoChildProcessHost.h"

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/process_watcher.h"
#ifdef MOZ_WIDGET_COCOA
#include "chrome/common/mach_ipc_mac.h"
#include "base/rand_util.h"
#include "nsILocalFileMac.h"
#include "SharedMemoryBasic.h"
#endif

#include "MainThreadUtils.h"
#include "prprf.h"
#include "prenv.h"
#include "nsXPCOMPrivate.h"

#include "nsExceptionHandler.h"

#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/Omnijar.h"
#include "ProtocolUtils.h"
#include <sys/stat.h>

#ifdef XP_WIN
#include "nsIWinTaskbar.h"
#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"

#if defined(MOZ_SANDBOX)
#include "mozilla/Preferences.h"
#include "mozilla/sandboxing/sandboxLogging.h"
#endif
#endif

#include "nsTArray.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsNativeCharsetUtils.h"
#include "nscore.h" // for NS_FREE_PERMANENT_DATA

using mozilla::MonitorAutoLock;
using mozilla::ipc::GeckoChildProcessHost;

#ifdef ANDROID
// Like its predecessor in nsExceptionHandler.cpp, this is
// the magic number of a file descriptor remapping we must
// preserve for the child process.
static const int kMagicAndroidSystemPropFd = 5;
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

static const bool kLowRightsSubprocesses =
  // We currently only attempt to drop privileges on gonk, because we
  // have no plugins or extensions to worry about breaking.
#ifdef MOZ_WIDGET_GONK
  true
#else
  false
#endif
  ;

static bool
ShouldHaveDirectoryService()
{
  return GeckoProcessType_Default == XRE_GetProcessType();
}

template<>
struct RunnableMethodTraits<GeckoChildProcessHost>
{
    static void RetainCallee(GeckoChildProcessHost* obj) { }
    static void ReleaseCallee(GeckoChildProcessHost* obj) { }
};

/*static*/
base::ChildPrivileges
GeckoChildProcessHost::DefaultChildPrivileges()
{
  return (kLowRightsSubprocesses ?
          base::PRIVILEGES_UNPRIVILEGED : base::PRIVILEGES_INHERIT);
}

GeckoChildProcessHost::GeckoChildProcessHost(GeckoProcessType aProcessType,
                                             ChildPrivileges aPrivileges)
  : ChildProcessHost(RENDER_PROCESS), // FIXME/cjones: we should own this enum
    mProcessType(aProcessType),
    mPrivileges(aPrivileges),
    mMonitor("mozilla.ipc.GeckChildProcessHost.mMonitor"),
    mProcessState(CREATING_CHANNEL),
    mDelegate(nullptr),
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

  if (mChildProcessHandle > 0) {
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
void
GeckoChildProcessHost::GetPathToBinary(FilePath& exePath)
{
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

#ifdef MOZ_WIDGET_ANDROID
  exePath = exePath.AppendASCII("lib");

  // We must use the PIE binary on 5.0 and higher
  const char* processName = mozilla::AndroidBridge::Bridge()->GetAPIVersion() >= 21 ?
    MOZ_CHILD_PROCESS_NAME_PIE : MOZ_CHILD_PROCESS_NAME;

  exePath = exePath.AppendASCII(processName);
#else
  exePath = exePath.AppendASCII(MOZ_CHILD_PROCESS_NAME);
#endif
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

nsresult GeckoChildProcessHost::GetArchitecturesForBinary(const char *path, uint32_t *result)
{
  *result = 0;

#ifdef MOZ_WIDGET_COCOA
  CFURLRef url = ::CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                                           (const UInt8*)path,
                                                           strlen(path),
                                                           false);
  if (!url) {
    return NS_ERROR_FAILURE;
  }
  AutoCFTypeObject autoPluginContainerURL(url);

  CFArrayRef pluginContainerArchs = ::CFBundleCopyExecutableArchitecturesForURL(url);
  if (!pluginContainerArchs) {
    return NS_ERROR_FAILURE;
  }
  AutoCFTypeObject autoPluginContainerArchs(pluginContainerArchs);

  CFIndex pluginArchCount = ::CFArrayGetCount(pluginContainerArchs);
  for (CFIndex i = 0; i < pluginArchCount; i++) {
    CFNumberRef currentArch = static_cast<CFNumberRef>(::CFArrayGetValueAtIndex(pluginContainerArchs, i));
    int currentArchInt = 0;
    if (!::CFNumberGetValue(currentArch, kCFNumberIntType, &currentArchInt)) {
      continue;
    }
    switch (currentArchInt) {
      case kCFBundleExecutableArchitectureI386:
        *result |= base::PROCESS_ARCH_I386;
        break;
      case kCFBundleExecutableArchitectureX86_64:
        *result |= base::PROCESS_ARCH_X86_64;
        break;
      case kCFBundleExecutableArchitecturePPC:
        *result |= base::PROCESS_ARCH_PPC;
        break;
      default:
        break;
    }
  }

  return (*result ? NS_OK : NS_ERROR_FAILURE);
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

uint32_t GeckoChildProcessHost::GetSupportedArchitecturesForProcessType(GeckoProcessType type)
{
#ifdef MOZ_WIDGET_COCOA
  if (type == GeckoProcessType_Plugin) {

    // Cache this, it shouldn't ever change.
    static uint32_t pluginContainerArchs = 0;
    if (pluginContainerArchs == 0) {
      FilePath exePath;
      GetPathToBinary(exePath);
      nsresult rv = GetArchitecturesForBinary(exePath.value().c_str(), &pluginContainerArchs);
      NS_ASSERTION(NS_SUCCEEDED(rv) && pluginContainerArchs != 0, "Getting architecture of plugin container failed!");
      if (NS_FAILED(rv) || pluginContainerArchs == 0) {
        pluginContainerArchs = base::GetCurrentProcessArchitecture();
      }
    }
    return pluginContainerArchs;
  }
#endif

  return base::GetCurrentProcessArchitecture();
}

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
#ifdef MOZ_CRASHREPORTER
  if (CrashReporter::GetEnabled()) {
    CrashReporter::OOPInit();
  }
#endif

#ifdef XP_WIN
  if (mProcessType == GeckoProcessType_Plugin) {
    InitWindowsGroupID();
  }

#if defined(MOZ_CONTENT_SANDBOX)
  // We need to get the pref here as the process is launched off main thread.
  if (mProcessType == GeckoProcessType_Content) {
    mSandboxLevel = Preferences::GetInt("security.sandbox.content.level");
    mEnableSandboxLogging =
      Preferences::GetBool("security.sandbox.windows.log");
  }
#endif

#if defined(MOZ_SANDBOX)
  // For other process types we can't rely on them being launched on main
  // thread and they may not have access to prefs in the child process, so allow
  // them to turn on logging via an environment variable.
  mEnableSandboxLogging = mEnableSandboxLogging
                          || !!PR_GetEnv("MOZ_WIN_SANDBOX_LOGGING");
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
GeckoChildProcessHost::SyncLaunch(std::vector<std::string> aExtraOpts, int aTimeoutMs, base::ProcessArchitecture arch)
{
  PrepareLaunch();

  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  NS_ASSERTION(MessageLoop::current() != ioLoop, "sync launch from the IO thread NYI");

  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this,
                                     &GeckoChildProcessHost::RunPerformAsyncLaunch,
                                     aExtraOpts, arch));

  return WaitUntilConnected(aTimeoutMs);
}

bool
GeckoChildProcessHost::AsyncLaunch(std::vector<std::string> aExtraOpts,
                                   base::ProcessArchitecture arch)
{
  PrepareLaunch();

  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this,
                                     &GeckoChildProcessHost::RunPerformAsyncLaunch,
                                     aExtraOpts, arch));

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
  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this,
                                     &GeckoChildProcessHost::RunPerformAsyncLaunch,
                                     aExtraOpts, base::GetCurrentProcessArchitecture()));

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

//
// Wrapper function for handling GECKO_SEPARATE_NSPR_LOGS
//
bool
GeckoChildProcessHost::PerformAsyncLaunch(std::vector<std::string> aExtraOpts, base::ProcessArchitecture arch)
{
  // If NSPR log files are not requested, we're done.
  const char* origLogName = PR_GetEnv("NSPR_LOG_FILE");
  if (!origLogName) {
    return PerformAsyncLaunchInternal(aExtraOpts, arch);
  }

  // We currently have no portable way to launch child with environment
  // different than parent.  So temporarily change NSPR_LOG_FILE so child
  // inherits value we want it to have. (NSPR only looks at NSPR_LOG_FILE at
  // startup, so it's 'safe' to play with the parent's environment this way.)
  nsAutoCString setChildLogName("NSPR_LOG_FILE=");
  setChildLogName.Append(origLogName);

  // remember original value so we can restore it.
  // - buffer needs to be permanently allocated for PR_SetEnv()
  // - Note: this code is not called re-entrantly, nor are restoreOrigLogName
  //   or mChildCounter touched by any other thread, so this is safe.
  static char* restoreOrigLogName = 0;
  if (!restoreOrigLogName)
    restoreOrigLogName = strdup(setChildLogName.get());

  // Append child-specific postfix to name
  setChildLogName.AppendLiteral(".child-");
  setChildLogName.AppendInt(++mChildCounter);

  // Passing temporary to PR_SetEnv is ok here because env gets copied
  // by exec, etc., to permanent storage in child when process launched.
  PR_SetEnv(setChildLogName.get());
  bool retval = PerformAsyncLaunchInternal(aExtraOpts, arch);

  // Revert to original value
  PR_SetEnv(restoreOrigLogName);

  return retval;
}

bool
GeckoChildProcessHost::RunPerformAsyncLaunch(std::vector<std::string> aExtraOpts,
                                             base::ProcessArchitecture aArch)
{
  InitializeChannel();
  return PerformAsyncLaunch(aExtraOpts, aArch);
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
        nsAutoCString path;
        MOZ_ALWAYS_TRUE(NS_SUCCEEDED(appDir->GetNativePath(path)));
#if defined(XP_WIN)
        aCmdLine.AppendLooseValue(UTF8ToWide("-appdir"));
        aCmdLine.AppendLooseValue(UTF8ToWide(path.get()));
#else
        aCmdLine.push_back("-appdir");
        aCmdLine.push_back(path.get());
#endif
      }
    }
  }
}

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
static void
MaybeAddNsprLogFileAccess(std::vector<std::wstring>& aAllowedFilesReadWrite)
{
  const char* nsprLogFileEnv = PR_GetEnv("NSPR_LOG_FILE");
  if (!nsprLogFileEnv) {
    return;
  }

  nsDependentCString nsprLogFilePath(nsprLogFileEnv);
  nsCOMPtr<nsIFile> nsprLogFile;
  nsresult rv = NS_NewNativeLocalFile(nsprLogFilePath, true,
                                      getter_AddRefs(nsprLogFile));
  if (NS_FAILED(rv)) {
    // Not an absolute path, try it as a relative one.
    nsresult rv = NS_GetSpecialDirectory(NS_OS_CURRENT_WORKING_DIR,
                                         getter_AddRefs(nsprLogFile));
    if (NS_FAILED(rv) || !nsprLogFile) {
      NS_WARNING("Failed to get current working directory");
      return;
    }

    rv = nsprLogFile->AppendRelativeNativePath(nsprLogFilePath);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  }

  nsAutoString resolvedFilePath;
  rv = nsprLogFile->GetPath(resolvedFilePath);
  if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
  }

  // Update the environment variable as well as adding the rule, because the
  // Chromium sandbox can only allow access to fully qualified file paths. This
  // only affects the environment for the child process we're about to create,
  // because this will get reset to the original value in PerformAsyncLaunch.
  aAllowedFilesReadWrite.push_back(std::wstring(resolvedFilePath.get()));
  nsAutoCString resolvedEnvVar("NSPR_LOG_FILE=");
  AppendUTF16toUTF8(resolvedFilePath, resolvedEnvVar);
  PR_SetEnv(resolvedEnvVar.get());
}
#endif

bool
GeckoChildProcessHost::PerformAsyncLaunchInternal(std::vector<std::string>& aExtraOpts, base::ProcessArchitecture arch)
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

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_BSD)
  base::environment_map newEnvVars;
  ChildPrivileges privs = mPrivileges;
  if (privs == base::PRIVILEGES_DEFAULT) {
    privs = DefaultChildPrivileges();
  }
  // XPCOM may not be initialized in some subprocesses.  We don't want
  // to initialize XPCOM just for the directory service, especially
  // since LD_LIBRARY_PATH is already set correctly in subprocesses
  // (meaning that we don't need to set that up in the environment).
  if (ShouldHaveDirectoryService()) {
    MOZ_ASSERT(gGREBinPath);
    nsCString path;
    NS_CopyUnicodeToNative(nsDependentString(gGREBinPath), path);
# if defined(OS_LINUX) || defined(OS_BSD)
#  if defined(MOZ_WIDGET_ANDROID)
    path += "/lib";
#  endif  // MOZ_WIDGET_ANDROID
    const char *ld_library_path = PR_GetEnv("LD_LIBRARY_PATH");
    nsCString new_ld_lib_path(path.get());

#  if (MOZ_WIDGET_GTK == 3)
    if (mProcessType == GeckoProcessType_Plugin) {
      new_ld_lib_path.Append("/gtk2:");
      new_ld_lib_path.Append(path.get());
    }
#endif
    if (ld_library_path && *ld_library_path) {
      new_ld_lib_path.Append(':');
      new_ld_lib_path.Append(ld_library_path);
    }
    newEnvVars["LD_LIBRARY_PATH"] = new_ld_lib_path.get();

# elif OS_MACOSX
    newEnvVars["DYLD_LIBRARY_PATH"] = path.get();
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
    newEnvVars["DYLD_INSERT_LIBRARIES"] = interpose.get();
# endif  // OS_LINUX
  }
#endif  // OS_LINUX || OS_MACOSX

  FilePath exePath;
  GetPathToBinary(exePath);

#ifdef MOZ_WIDGET_ANDROID
  // The java wrapper unpacks this for us but can't make it executable
  chmod(exePath.value().c_str(), 0700);
#endif  // MOZ_WIDGET_ANDROID

#ifdef ANDROID
  // Remap the Android property workspace to a well-known int,
  // and update the environment to reflect the new value for the
  // child process.
  const char *apws = getenv("ANDROID_PROPERTY_WORKSPACE");
  if (apws) {
    int fd = atoi(apws);
    mFileMap.push_back(std::pair<int, int>(fd, kMagicAndroidSystemPropFd));

    char buf[32];
    char *szptr = strchr(apws, ',');

    snprintf(buf, sizeof(buf), "%d%s", kMagicAndroidSystemPropFd, szptr);
    newEnvVars["ANDROID_PROPERTY_WORKSPACE"] = buf;
  }
#endif  // ANDROID

#ifdef MOZ_WIDGET_GONK
  if (const char *ldPreloadPath = getenv("LD_PRELOAD")) {
    newEnvVars["LD_PRELOAD"] = ldPreloadPath;
  }
#endif // MOZ_WIDGET_GONK

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

  childArgv.push_back(pidstring);

#if defined(MOZ_CRASHREPORTER)
#  if defined(OS_LINUX) || defined(OS_BSD)
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
#  elif defined(MOZ_WIDGET_COCOA)
  childArgv.push_back(CrashReporter::GetChildNotificationPipe());
#  endif  // OS_LINUX
#endif

#ifdef MOZ_WIDGET_COCOA
  // Add a mach port to the command line so the child can communicate its
  // 'task_t' back to the parent.
  //
  // Put a random number into the channel name, so that a compromised renderer
  // can't pretend being the child that's forked off.
  std::string mach_connection_name = StringPrintf("org.mozilla.machname.%d",
                                                  base::RandInt(0, std::numeric_limits<int>::max()));
  childArgv.push_back(mach_connection_name.c_str());
#endif

  childArgv.push_back(childProcessType);

  base::LaunchApp(childArgv, mFileMap,
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_BSD)
                  newEnvVars, privs,
#endif
                  false, &process, arch);

  // We're in the parent and the child was launched. Close the child FD in the
  // parent as soon as possible, which will allow the parent to detect when the
  // child closes its FD (either due to normal exit or due to crash).
  GetChannel()->CloseClientFileDescriptor();

#ifdef MOZ_WIDGET_COCOA
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
  MachPortSender* parent_recv_port_memory_ack = new MachPortSender(child_message.GetTranslatedPort(2));

  if (child_message.GetTranslatedPort(3) == MACH_PORT_NULL) {
    CHROMIUM_LOG(ERROR) << "parent GetTranslatedPort(3) failed.";
  }
  MachPortSender* parent_send_port_memory = new MachPortSender(child_message.GetTranslatedPort(3));

  MachSendMessage parent_message(/* id= */0);
  if (!parent_message.AddDescriptor(MachMsgPortDescriptor(bootstrap_port))) {
    CHROMIUM_LOG(ERROR) << "parent AddDescriptor(" << bootstrap_port << ") failed.";
    return false;
  }

  ReceivePort* parent_recv_port_memory = new ReceivePort();
  if (!parent_message.AddDescriptor(MachMsgPortDescriptor(parent_recv_port_memory->GetPort()))) {
    CHROMIUM_LOG(ERROR) << "parent AddDescriptor(" << parent_recv_port_memory->GetPort() << ") failed.";
    return false;
  }

  ReceivePort* parent_send_port_memory_ack = new ReceivePort();
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

#endif

//--------------------------------------------------
#elif defined(OS_WIN)

  FilePath exePath;
  GetPathToBinary(exePath);

  CommandLine cmdLine(exePath.ToWStringHack());
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

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  bool shouldSandboxCurrentProcess = false;

  // XXX: Bug 1124167: We should get rid of the process specific logic for
  // sandboxing in this class at some point. Unfortunately it will take a bit
  // of reorganizing so I don't think this patch is the right time.
  switch (mProcessType) {
    case GeckoProcessType_Content:
#if defined(MOZ_CONTENT_SANDBOX)
      if (!PR_GetEnv("MOZ_DISABLE_CONTENT_SANDBOX")) {
        mSandboxBroker.SetSecurityLevelForContentProcess(mSandboxLevel);
        cmdLine.AppendLooseValue(UTF8ToWide("-sandbox"));
        shouldSandboxCurrentProcess = true;
      }
#endif // MOZ_CONTENT_SANDBOX
      break;
    case GeckoProcessType_Plugin:
      if (mSandboxLevel > 0 &&
          !PR_GetEnv("MOZ_DISABLE_NPAPI_SANDBOX")) {
        mSandboxBroker.SetSecurityLevelForPluginProcess(mSandboxLevel);
        cmdLine.AppendLooseValue(UTF8ToWide("-sandbox"));
        shouldSandboxCurrentProcess = true;
      }
      break;
    case GeckoProcessType_IPDLUnitTest:
      // XXX: We don't sandbox this process type yet
      // mSandboxBroker.SetSecurityLevelForIPDLUnitTestProcess();
      // cmdLine.AppendLooseValue(UTF8ToWide("-sandbox"));
      // shouldSandboxCurrentProcess = true;
      break;
    case GeckoProcessType_GMPlugin:
      if (!PR_GetEnv("MOZ_DISABLE_GMP_SANDBOX")) {
        mSandboxBroker.SetSecurityLevelForGMPlugin();
        cmdLine.AppendLooseValue(UTF8ToWide("-sandbox"));
        shouldSandboxCurrentProcess = true;
      }
      break;
    case GeckoProcessType_Default:
    default:
      MOZ_CRASH("Bad process type in GeckoChildProcessHost");
      break;
  };

  if (shouldSandboxCurrentProcess) {
    MaybeAddNsprLogFileAccess(mAllowedFilesReadWrite);
    for (auto it = mAllowedFilesRead.begin();
         it != mAllowedFilesRead.end();
         ++it) {
      mSandboxBroker.AllowReadFile(it->c_str());
    }

    for (auto it = mAllowedFilesReadWrite.begin();
         it != mAllowedFilesReadWrite.end();
         ++it) {
      mSandboxBroker.AllowReadWriteFile(it->c_str());
    }
  }
#endif // XP_WIN && MOZ_SANDBOX

  // Add the application directory path (-appdir path)
  AddAppDirToCommandLine(cmdLine);

  // XXX Command line params past this point are expected to be at
  // the end of the command line string, and in a specific order.
  // See XRE_InitChildProcess in nsEmbedFunction.

  // Win app model id
  cmdLine.AppendLooseValue(mGroupId.get());

  // Process id
  cmdLine.AppendLooseValue(UTF8ToWide(pidstring));

#if defined(MOZ_CRASHREPORTER)
  cmdLine.AppendLooseValue(
    UTF8ToWide(CrashReporter::GetChildNotificationPipe()));
#endif

  // Process type
  cmdLine.AppendLooseValue(UTF8ToWide(childProcessType));

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
  if (shouldSandboxCurrentProcess) {
    mSandboxBroker.LaunchApp(cmdLine.program().c_str(),
                             cmdLine.command_line_string().c_str(),
                             mEnableSandboxLogging,
                             &process);
  } else
#endif
  {
    base::LaunchApp(cmdLine, false, false, &process);

#ifdef MOZ_SANDBOX
    // We need to be able to duplicate handles to non-sandboxed content
    // processes, so add it as a target peer.
    if (mProcessType == GeckoProcessType_Content) {
      if (!mSandboxBroker.AddTargetPeer(process)) {
        NS_WARNING("Failed to add content process as target peer.");
      }
    }
#endif
  }

#else
#  error Sorry
#endif

  if (!process) {
    MonitorAutoLock lock(mMonitor);
    mProcessState = PROCESS_ERROR;
    lock.Notify();
    return false;
  }
  // NB: on OS X, we block much longer than we need to in order to
  // reach this call, waiting for the child process's task_t.  The
  // best way to fix that is to refactor this file, hard.
  SetHandle(process);
#if defined(MOZ_WIDGET_COCOA)
  mChildTask = child_task;
#endif

  OpenPrivilegedHandle(base::GetProcId(process));
  {
    MonitorAutoLock lock(mMonitor);
    mProcessState = PROCESS_CREATED;
    lock.Notify();
  }

  return true;
}

void
GeckoChildProcessHost::OpenPrivilegedHandle(base::ProcessId aPid)
{
  if (mChildProcessHandle) {
    MOZ_ASSERT(aPid == base::GetProcId(mChildProcessHandle));
    return;
  }
  if (!base::OpenPrivilegedProcessHandle(aPid, &mChildProcessHandle)) {
    NS_RUNTIMEABORT("can't open handle to child process");
  }
}

void
GeckoChildProcessHost::OnChannelConnected(int32_t peer_pid)
{
  OpenPrivilegedHandle(peer_pid);
  {
    MonitorAutoLock lock(mMonitor);
    mProcessState = PROCESS_CONNECTED;
    lock.Notify();
  }
}

void
GeckoChildProcessHost::OnMessageReceived(const IPC::Message& aMsg)
{
  // We never process messages ourself, just save them up for the next
  // listener.
  mQueue.push(aMsg);
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

void
GeckoChildProcessHost::OnWaitableEventSignaled(base::WaitableEvent *event)
{
  if (mDelegate) {
    mDelegate->OnWaitableEventSignaled(event);
  }
  ChildProcessHost::OnWaitableEventSignaled(event);
}

#ifdef MOZ_NUWA_PROCESS

using mozilla::ipc::GeckoExistingProcessHost;
using mozilla::ipc::FileDescriptor;

GeckoExistingProcessHost::
GeckoExistingProcessHost(GeckoProcessType aProcessType,
                         base::ProcessHandle aProcess,
                         const FileDescriptor& aFileDescriptor,
                         ChildPrivileges aPrivileges)
  : GeckoChildProcessHost(aProcessType, aPrivileges)
  , mExistingProcessHandle(aProcess)
  , mExistingFileDescriptor(aFileDescriptor)
{
  NS_ASSERTION(aFileDescriptor.IsValid(),
               "Expected file descriptor to be valid");
}

GeckoExistingProcessHost::~GeckoExistingProcessHost()
{
  // Bug 943174: If we don't do this, ~GeckoChildProcessHost will try
  // to wait on a process that isn't a direct child, and bad things
  // will happen.
  SetAlreadyDead();
}

bool
GeckoExistingProcessHost::PerformAsyncLaunch(StringVector aExtraOpts,
                                             base::ProcessArchitecture aArch)
{
  SetHandle(mExistingProcessHandle);

  OpenPrivilegedHandle(base::GetProcId(mExistingProcessHandle));

  MonitorAutoLock lock(mMonitor);
  mProcessState = PROCESS_CREATED;
  lock.Notify();

  return true;
}

void
GeckoExistingProcessHost::InitializeChannel()
{
  CreateChannel(mExistingFileDescriptor);

  MonitorAutoLock lock(mMonitor);
  mProcessState = CHANNEL_INITIALIZED;
  lock.Notify();
}

#endif /* MOZ_NUWA_PROCESS */
