/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoChildProcessHost.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/process_watcher.h"
#ifdef MOZ_WIDGET_COCOA
#include "chrome/common/mach_ipc_mac.h"
#include "base/rand_util.h"
#include "nsILocalFileMac.h"
#endif

#include "prprf.h"
#include "prenv.h"

#if defined(OS_LINUX)
#  define XP_LINUX 1
#endif
#include "nsExceptionHandler.h"

#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"

#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/Omnijar.h"
#include <sys/stat.h>

#ifdef XP_WIN
#include "nsIWinTaskbar.h"
#define NS_TASKBAR_CONTRACTID "@mozilla.org/windows-taskbar;1"
#endif

#ifdef MOZ_WIDGET_ANDROID
#include "APKOpen.h"
#endif

using mozilla::MonitorAutoLock;
using mozilla::ipc::GeckoChildProcessHost;

#ifdef ANDROID
// Like its predecessor in nsExceptionHandler.cpp, this is
// the magic number of a file descriptor remapping we must
// preserve for the child process.
static const int kMagicAndroidSystemPropFd = 5;
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

GeckoChildProcessHost::GeckoChildProcessHost(GeckoProcessType aProcessType,
                                             ChildPrivileges aPrivileges)
  : ChildProcessHost(RENDER_PROCESS), // FIXME/cjones: we should own this enum
    mProcessType(aProcessType),
    mPrivileges(aPrivileges),
    mMonitor("mozilla.ipc.GeckChildProcessHost.mMonitor"),
    mProcessState(CREATING_CHANNEL),
    mDelegate(nullptr),
    mChildProcessHandle(0)
#if defined(MOZ_WIDGET_COCOA)
  , mChildTask(MACH_PORT_NULL)
#endif
{
    MOZ_COUNT_CTOR(GeckoChildProcessHost);
    
    MessageLoop* ioLoop = XRE_GetIOMessageLoop();
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

#if defined(MOZ_WIDGET_COCOA)
  if (mChildTask != MACH_PORT_NULL)
    mach_port_deallocate(mach_task_self(), mChildTask);
#endif
}

void GetPathToBinary(FilePath& exePath)
{
  if (ShouldHaveDirectoryService()) {
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    NS_ASSERTION(directoryService, "Expected XPCOM to be available");
    if (directoryService) {
      nsCOMPtr<nsIFile> greDir;
      nsresult rv = directoryService->Get(NS_GRE_DIR, NS_GET_IID(nsIFile), getter_AddRefs(greDir));
      if (NS_SUCCEEDED(rv)) {
#ifdef OS_WIN
        nsString path;
        greDir->GetPath(path);
#else
        nsCString path;
        greDir->GetNativePath(path);
#endif
        exePath = FilePath(path.get());
#ifdef MOZ_WIDGET_COCOA
        // We need to use an App Bundle on OS X so that we can hide
        // the dock icon. See Bug 557225.
        exePath = exePath.AppendASCII(MOZ_CHILD_PROCESS_BUNDLE);
#endif
      }
    }
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
}

#ifdef MOZ_WIDGET_COCOA
class AutoCFTypeObject {
public:
  AutoCFTypeObject(CFTypeRef object)
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

void
GeckoChildProcessHost::PrepareLaunch()
{
#ifdef MOZ_CRASHREPORTER
  if (CrashReporter::GetEnabled()) {
    CrashReporter::OOPInit();
  }
#endif

#ifdef XP_WIN
  InitWindowsGroupID();
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
      mGroupId.AssignLiteral("-");
    }
  }
}
#endif

bool
GeckoChildProcessHost::SyncLaunch(std::vector<std::string> aExtraOpts, int aTimeoutMs, base::ProcessArchitecture arch)
{
  PrepareLaunch();

  PRIntervalTime timeoutTicks = (aTimeoutMs > 0) ? 
    PR_MillisecondsToInterval(aTimeoutMs) : PR_INTERVAL_NO_TIMEOUT;
  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  NS_ASSERTION(MessageLoop::current() != ioLoop, "sync launch from the IO thread NYI");

  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this,
                                     &GeckoChildProcessHost::PerformAsyncLaunch,
                                     aExtraOpts, arch));
  // NB: this uses a different mechanism than the chromium parent
  // class.
  MonitorAutoLock lock(mMonitor);
  PRIntervalTime waitStart = PR_IntervalNow();
  PRIntervalTime current;

  // We'll receive several notifications, we need to exit when we
  // have either successfully launched or have timed out.
  while (mProcessState < PROCESS_CONNECTED) {
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
GeckoChildProcessHost::AsyncLaunch(std::vector<std::string> aExtraOpts)
{
  PrepareLaunch();

  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this,
                                     &GeckoChildProcessHost::PerformAsyncLaunch,
                                     aExtraOpts, base::GetCurrentProcessArchitecture()));

  // This may look like the sync launch wait, but we only delay as
  // long as it takes to create the channel.
  MonitorAutoLock lock(mMonitor);
  while (mProcessState < CHANNEL_INITIALIZED) {
    lock.Wait();
  }

  return true;
}

bool
GeckoChildProcessHost::LaunchAndWaitForProcessHandle(StringVector aExtraOpts)
{
  PrepareLaunch();

  MessageLoop* ioLoop = XRE_GetIOMessageLoop();
  ioLoop->PostTask(FROM_HERE,
                   NewRunnableMethod(this,
                                     &GeckoChildProcessHost::PerformAsyncLaunch,
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
  mChildProcessHandle = 0;
}

int32_t GeckoChildProcessHost::mChildCounter = 0;

//
// Wrapper function for handling GECKO_SEPARATE_NSPR_LOGS
//
bool
GeckoChildProcessHost::PerformAsyncLaunch(std::vector<std::string> aExtraOpts, base::ProcessArchitecture arch)
{
  // If separate NSPR log files are not requested, we're done.
  const char* origLogName = PR_GetEnv("NSPR_LOG_FILE");
  const char* separateLogs = PR_GetEnv("GECKO_SEPARATE_NSPR_LOGS");
  if (!origLogName || !separateLogs || !*separateLogs ||
      *separateLogs == '0' || *separateLogs == 'N' || *separateLogs == 'n') {
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
        appDir->GetNativePath(path);
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

bool
GeckoChildProcessHost::PerformAsyncLaunchInternal(std::vector<std::string>& aExtraOpts, base::ProcessArchitecture arch)
{
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

#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_BSD)
  base::environment_map newEnvVars;
  ChildPrivileges privs = mPrivileges;
  if (privs == base::PRIVILEGES_DEFAULT) {
    privs = kLowRightsSubprocesses ?
            base::PRIVILEGES_UNPRIVILEGED : base::PRIVILEGES_INHERIT;
  }
  // XPCOM may not be initialized in some subprocesses.  We don't want
  // to initialize XPCOM just for the directory service, especially
  // since LD_LIBRARY_PATH is already set correctly in subprocesses
  // (meaning that we don't need to set that up in the environment).
  if (ShouldHaveDirectoryService()) {
    nsCOMPtr<nsIProperties> directoryService(do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID));
    NS_ASSERTION(directoryService, "Expected XPCOM to be available");
    if (directoryService) {
      nsCOMPtr<nsIFile> greDir;
      nsresult rv = directoryService->Get(NS_GRE_DIR, NS_GET_IID(nsIFile), getter_AddRefs(greDir));
      if (NS_SUCCEEDED(rv)) {
        nsCString path;
        greDir->GetNativePath(path);
# if defined(OS_LINUX) || defined(OS_BSD)
#  if defined(MOZ_WIDGET_ANDROID)
        path += "/lib";
#  endif  // MOZ_WIDGET_ANDROID
        const char *ld_library_path = PR_GetEnv("LD_LIBRARY_PATH");
        nsCString new_ld_lib_path;
        if (ld_library_path && *ld_library_path) {
            new_ld_lib_path.Assign(path.get());
            new_ld_lib_path.AppendLiteral(":");
            new_ld_lib_path.Append(ld_library_path);
            newEnvVars["LD_LIBRARY_PATH"] = new_ld_lib_path.get();
        } else {
            newEnvVars["LD_LIBRARY_PATH"] = path.get();
        }
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
        if (prevInterpose) {
          interpose.Assign(prevInterpose);
          interpose.AppendLiteral(":");
        }
        interpose.Append(path.get());
        interpose.AppendLiteral("/libplugin_child_interpose.dylib");
        newEnvVars["DYLD_INSERT_LIBRARIES"] = interpose.get();
# endif  // OS_LINUX
      }
    }
  }
#endif  // OS_LINUX || OS_MACOSX

  FilePath exePath;
  GetPathToBinary(exePath);

#ifdef MOZ_WIDGET_ANDROID
  // The java wrapper unpacks this for us but can't make it executable
  chmod(exePath.value().c_str(), 0700);
  int cacheCount = 0;
  const struct lib_cache_info * cache = getLibraryCache();
  nsCString cacheStr;
  while (cache &&
         cacheCount++ < MAX_LIB_CACHE_ENTRIES &&
         strlen(cache->name)) {
    mFileMap.push_back(std::pair<int,int>(cache->fd, cache->fd));
    cacheStr.Append(cache->name);
    cacheStr.AppendPrintf(":%d;", cache->fd);
    cache++;
  }
  // fill the last arg with something if there's no cache
  if (cacheStr.IsEmpty())
    cacheStr.AppendLiteral("-");
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

#ifdef MOZ_WIDGET_ANDROID
  childArgv.push_back(cacheStr.get());
#endif

  base::LaunchApp(childArgv, mFileMap,
#if defined(OS_LINUX) || defined(OS_MACOSX) || defined(OS_BSD)
                  newEnvVars, privs,
#endif
                  false, &process, arch);

#ifdef MOZ_WIDGET_COCOA
  // Wait for the child process to send us its 'task_t' data.
  const int kTimeoutMs = 10000;

  MachReceiveMessage child_message;
  ReceivePort parent_recv_port(mach_connection_name.c_str());
  kern_return_t err = parent_recv_port.WaitForMessage(&child_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    std::string errString = StringPrintf("0x%x %s", err, mach_error_string(err));
    LOG(ERROR) << "parent WaitForMessage() failed: " << errString;
    return false;
  }

  task_t child_task = child_message.GetTranslatedPort(0);
  if (child_task == MACH_PORT_NULL) {
    LOG(ERROR) << "parent GetTranslatedPort(0) failed.";
    return false;
  }

  if (child_message.GetTranslatedPort(1) == MACH_PORT_NULL) {
    LOG(ERROR) << "parent GetTranslatedPort(1) failed.";
    return false;
  }
  MachPortSender parent_sender(child_message.GetTranslatedPort(1));

  MachSendMessage parent_message(/* id= */0);
  if (!parent_message.AddDescriptor(bootstrap_port)) {
    LOG(ERROR) << "parent AddDescriptor(" << bootstrap_port << ") failed.";
    return false;
  }

  err = parent_sender.SendMessage(parent_message, kTimeoutMs);
  if (err != KERN_SUCCESS) {
    std::string errString = StringPrintf("0x%x %s", err, mach_error_string(err));
    LOG(ERROR) << "parent SendMessage() failed: " << errString;
    return false;
  }
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

  // Add the application directory path (-appdir path)
  AddAppDirToCommandLine(cmdLine);

  // XXX Command line params past this point are expected to be at
  // the end of the command line string, and in a specific order.
  // See XRE_InitChildProcess in nsEmbedFunction.

  // Win app model id
  cmdLine.AppendLooseValue(std::wstring(mGroupId.get()));

  // Process id
  cmdLine.AppendLooseValue(UTF8ToWide(pidstring));

#if defined(MOZ_CRASHREPORTER)
  cmdLine.AppendLooseValue(
    UTF8ToWide(CrashReporter::GetChildNotificationPipe()));
#endif

  // Process type
  cmdLine.AppendLooseValue(UTF8ToWide(childProcessType));

  base::LaunchApp(cmdLine, false, false, &process);

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
