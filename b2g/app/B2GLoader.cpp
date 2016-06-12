/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"
#include "application.ini.h"
#include "nsXPCOMGlue.h"
#include "nsStringGlue.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "BinaryPath.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <dlfcn.h>

#include "nsXPCOMPrivate.h" // for MAXPATHLEN and XPCOM_DLL
#include "mozilla/UniquePtr.h"

#define ASSERT(x) if (!(x)) { MOZ_CRASH(); }

// Functions being loaded by XPCOMGlue
XRE_ProcLoaderServiceRunType XRE_ProcLoaderServiceRun;
XRE_ProcLoaderClientInitType XRE_ProcLoaderClientInit;
XRE_ProcLoaderPreloadType XRE_ProcLoaderPreload;
extern XRE_CreateAppDataType XRE_CreateAppData;
extern XRE_GetFileFromPathType XRE_GetFileFromPath;

static const nsDynamicFunctionLoad kXULFuncs[] = {
  { "XRE_ProcLoaderServiceRun", (NSFuncPtr*) &XRE_ProcLoaderServiceRun },
  { "XRE_ProcLoaderClientInit", (NSFuncPtr*) &XRE_ProcLoaderClientInit },
  { "XRE_ProcLoaderPreload", (NSFuncPtr*) &XRE_ProcLoaderPreload },
  { "XRE_CreateAppData", (NSFuncPtr*) &XRE_CreateAppData },
  { "XRE_GetFileFromPath", (NSFuncPtr*) &XRE_GetFileFromPath },
  { nullptr, nullptr }
};

typedef mozilla::Vector<int> FdArray;
static const int kReservedFileDescriptors = 5;
static const int kBeginReserveFileDescriptor = STDERR_FILENO + 1;

static int
GetDirnameSlash(const char *aPath, char *aOutDir, int aMaxLen)
{
  char *lastSlash = strrchr(aPath, XPCOM_FILE_PATH_SEPARATOR[0]);
  if (lastSlash == nullptr) {
    return 0;
  }
  int cpsz = lastSlash - aPath + 1; // include slash
  if (aMaxLen <= cpsz) {
    return 0;
  }
  strncpy(aOutDir, aPath, cpsz);
  aOutDir[cpsz] = 0;
  return cpsz;
}

static bool
GetXPCOMPath(const char *aProgram, char *aOutPath, int aMaxLen)
{
  auto progBuf = mozilla::MakeUnique<char[]>(aMaxLen);
  nsresult rv = mozilla::BinaryPath::Get(aProgram, progBuf.get());
  NS_ENSURE_SUCCESS(rv, false);

  int len = GetDirnameSlash(progBuf.get(), aOutPath, aMaxLen);
  NS_ENSURE_TRUE(!!len, false);

  NS_ENSURE_TRUE((len + sizeof(XPCOM_DLL)) < (unsigned)aMaxLen, false);
  char *afterSlash = aOutPath + len;
  strcpy(afterSlash, XPCOM_DLL);
  return true;
}

static bool
LoadLibxul(const char *aXPCOMPath)
{
  nsresult rv;

  XPCOMGlueEnablePreload();
  rv = XPCOMGlueStartup(aXPCOMPath);
  NS_ENSURE_SUCCESS(rv, false);

  rv = XPCOMGlueLoadXULFunctions(kXULFuncs);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

/**
 * Return true if |arg| matches the given argument name.
 */
static bool
IsArg(const char* arg, const char* s)
{
  if (*arg == '-') {
    if (*++arg == '-') {
      ++arg;
    }
    return !strcasecmp(arg, s);
  }

#if defined(XP_WIN)
  if (*arg == '/') {
    return !strcasecmp(++arg, s);
  }
#endif

  return false;
}

static already_AddRefed<nsIFile>
GetAppIni(int argc, const char *argv[])
{
  nsCOMPtr<nsIFile> appini;
  nsresult rv;

  // Allow firefox.exe to launch XULRunner apps via -app <application.ini>
  // Note that -app must be the *first* argument.
  const char *appDataFile = getenv("XUL_APP_FILE");
  if (appDataFile && *appDataFile) {
    rv = XRE_GetFileFromPath(appDataFile, getter_AddRefs(appini));
    NS_ENSURE_SUCCESS(rv, nullptr);
  } else if (argc > 1 && IsArg(argv[1], "app")) {
    if (argc == 2) {
      return nullptr;
    }

    rv = XRE_GetFileFromPath(argv[2], getter_AddRefs(appini));
    NS_ENSURE_SUCCESS(rv, nullptr);

    char appEnv[MAXPATHLEN];
    snprintf(appEnv, MAXPATHLEN, "XUL_APP_FILE=%s", argv[2]);
    if (putenv(strdup(appEnv))) {
      return nullptr;
    }
  }

  return appini.forget();
}

static bool
LoadStaticData(int argc, const char *argv[])
{
  char xpcomPath[MAXPATHLEN];
  bool ok = GetXPCOMPath(argv[0], xpcomPath, MAXPATHLEN);
  NS_ENSURE_TRUE(ok, false);

  ok = LoadLibxul(xpcomPath);
  NS_ENSURE_TRUE(ok, false);

  char progDir[MAXPATHLEN];
  ok = GetDirnameSlash(xpcomPath, progDir, MAXPATHLEN);
  NS_ENSURE_TRUE(ok, false);

  nsCOMPtr<nsIFile> appini = GetAppIni(argc, argv);
  const nsXREAppData *appData;
  if (appini) {
    nsresult rv =
      XRE_CreateAppData(appini, const_cast<nsXREAppData**>(&appData));
    NS_ENSURE_SUCCESS(rv, false);
  } else {
    appData = &sAppData;
  }

  XRE_ProcLoaderPreload(progDir, appData);

  if (appini) {
    XRE_FreeAppData(const_cast<nsXREAppData*>(appData));
  }

  return true;
}

/**
 * Fork and run parent and child process.
 *
 * The parent is the b2g process and child for Nuwa.
 */
static int
RunProcesses(int argc, const char *argv[], FdArray& aReservedFds)
{
  /*
   * The original main() of the b2g process.  It is renamed to
   * b2g_main() for the b2g loader.
   */
  int b2g_main(int argc, const char *argv[]);

  int ipcSockets[2] = {-1, -1};
  int r = socketpair(AF_LOCAL, SOCK_STREAM, 0, ipcSockets);
  ASSERT(r == 0);
  int parentSock = ipcSockets[0];
  int childSock = ipcSockets[1];

  r = fcntl(parentSock, F_SETFL, O_NONBLOCK);
  ASSERT(r != -1);
  r = fcntl(childSock, F_SETFL, O_NONBLOCK);
  ASSERT(r != -1);

  pid_t pid = fork();
  ASSERT(pid >= 0);
  bool isChildProcess = pid == 0;

  close(isChildProcess ? parentSock : childSock);

  if (isChildProcess) {
    /* The Nuwa process */
    /* This provides the IPC service of loading Nuwa at the process.
     * The b2g process would send a IPC message of loading Nuwa
     * as the replacement of forking and executing plugin-container.
     */
    return XRE_ProcLoaderServiceRun(getppid(), childSock, argc, argv,
                                    aReservedFds);
  }

  // Reap zombie child process.
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGCHLD, &sa, nullptr);

  // The b2g process
  int childPid = pid;
  XRE_ProcLoaderClientInit(childPid, parentSock, aReservedFds);
  return b2g_main(argc, argv);
}

/**
 * Reserve the file descriptors that shouldn't be taken for other use for the
 * child process.
 */
static void
ReserveFileDescriptors(FdArray& aReservedFds)
{
  for (int i = 0; i < kReservedFileDescriptors; i++) {
    struct stat fileState;
    int target = kBeginReserveFileDescriptor + i;
    if (fstat(target, &fileState) == 0) {
      MOZ_CRASH("ProcLoader error: a magic file descriptor is occupied.");
    }

    int fd = open("/dev/null", O_RDWR);
    if (fd == -1) {
      MOZ_CRASH("ProcLoader error: failed to reserve a magic file descriptor.");
    }

    if (!aReservedFds.append(target)) {
      MOZ_CRASH("Failed to append to aReservedFds");
    }

    if (fd == target) {
      // No need to call dup2(). We already occupy the desired file descriptor.
      continue;
    }

    if (dup2(fd, target)) {
      MOZ_CRASH("ProcLoader error: failed to reserve a magic file descriptor.");
    }

    close(fd);
  }
}

/**
 * B2G Loader is responsible for loading the b2g process and the
 * Nuwa process.  It forks into the parent process, for the b2g
 * process, and the child process, for the Nuwa process.
 *
 * The loader loads libxul and performs initialization of static data
 * before forking, so relocation of libxul and static data can be
 * shared between the b2g process, the Nuwa process, and the content
 * processes.
 */
int
main(int argc, const char* argv[])
{
  /**
   * Reserve file descriptors before loading static data.
   */
  FdArray reservedFds;
  ReserveFileDescriptors(reservedFds);

  /*
   * Before fork(), libxul and static data of Gecko are loaded for
   * sharing.
   */
  bool ok = LoadStaticData(argc, argv);
  if (!ok) {
    return 255;
  }

  return RunProcesses(argc, argv, reservedFds);
}
