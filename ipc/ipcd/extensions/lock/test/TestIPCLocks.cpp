/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

// This test program spawns N copies of itself, and each copy spawns M threads.
// Each thread acquires and releases a named, interprocess lock.
// Randomized delays are injected at various points to exercise the system, and
// help expose any race conditions that may exist.
//
// Usage: TestIPCLocks [-N]

#include <stdlib.h>
#include <stdio.h>
#include "ipcILockService.h"
#include "ipcLockCID.h"
#include "nsIServiceManagerUtils.h"
#include "nsIEventQueueService.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "prproces.h"
#include "prprf.h"

#if defined(XP_UNIX) || defined(XP_OS2) || defined(XP_BEOS)
#include <unistd.h>
static unsigned GetPID()
{
  return (unsigned) getpid();
}
#elif defined(XP_WIN)
#include <windows.h>
static unsigned GetPID()
{
  return (unsigned) GetCurrentProcessId();  
}
#else
static unsigned int GetPID()
{
  return 0; // implement me!
}
#endif

static void LOG(const char *fmt, ... )
{
  va_list ap;
  va_start(ap, fmt);
  PRUint32 nb = 0;
  char buf[512];

  nb = PR_snprintf(buf, sizeof(buf), "[%u:%p] ", GetPID(), PR_GetCurrentThread());

  PR_vsnprintf(buf + nb, sizeof(buf) - nb, fmt, ap);
  buf[sizeof(buf) - 1] = '\0';

  fwrite(buf, strlen(buf), 1, stdout);
  fflush(stdout);

  va_end(ap);
}

static void RandomSleep(PRUint32 fromMS, PRUint32 toMS)
{
  PRUint32 ms = fromMS + (PRUint32) ((toMS - fromMS) * ((double) rand() / RAND_MAX));
  //LOG("putting thread to sleep for %u ms\n", ms);
  PR_Sleep(PR_MillisecondsToInterval(ms));
}

static ipcILockService *gLockService;

PR_STATIC_CALLBACK(void) TestThread(void *arg)
{
  const char *lockName = (const char *) arg;

  LOG("entering TestThread [lock=%s]\n", lockName);

  nsresult rv;

  RandomSleep(1000, 1100);

  //LOG("done sleeping\n");

  rv = gLockService->AcquireLock(lockName, PR_TRUE);
  if (NS_SUCCEEDED(rv))
  {
    //LOG("acquired lock \"%s\"\n", lockName);
    RandomSleep(500, 1000);
    //LOG("releasing lock \"%s\"\n", lockName);
    rv = gLockService->ReleaseLock(lockName);
    if (NS_FAILED(rv))
    {
      LOG("failed to release lock [rv=%x]\n", rv);
      NS_ERROR("failed to release lock");
    }
  }
  else
  {
    LOG("failed to acquire lock [rv=%x]\n", rv);
    NS_NOTREACHED("failed to acquire lock");
  }

  LOG("exiting TestThread [lock=%s rv=%x]\n", lockName, rv);
}

static const char *kLockNames[] = {
  "foopy",
  "test",
  "1",
  "xyz",
  "moz4ever",
  nsnull
};

static nsresult DoTest()
{
  nsresult rv;

  nsCOMPtr<nsIEventQueueService> eqs =
      do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = eqs->CreateMonitoredThreadEventQueue();
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIEventQueue> eq;
  rv = eqs->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(eq));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<ipcILockService> lockService =
      do_GetService(IPC_LOCKSERVICE_CONTRACTID);

  gLockService = lockService;

  PRThread *threads[10] = {0};
  int i = 0;

  for (const char **lockName = kLockNames; *lockName; ++lockName, ++i)
  {
    threads[i] = PR_CreateThread(PR_USER_THREAD,
                                 TestThread,
                                 (void *) *lockName,
                                 PR_PRIORITY_NORMAL,
                                 PR_GLOBAL_THREAD,
                                 PR_JOINABLE_THREAD,
                                 0);
  }

  for (i=0; threads[i]; ++i)
  {
    PR_JoinThread(threads[i]);
    threads[i] = nsnull;
  }

  gLockService = nsnull;

  LOG("joined with all threads; exiting DoTest\n");
  return NS_OK;
}

int main(int argc, char **argv)
{
  LOG("entering main\n");

  int numProcs = 10;

  // if this is a child process, then just run the test
  if (argc > 1)
  {
    if (strcmp(argv[1], "-child") == 0)
    {
      RandomSleep(1000, 1000);
      LOG("running child test\n");
      NS_InitXPCOM2(nsnull, nsnull, nsnull);
      DoTest();
      NS_ShutdownXPCOM(nsnull);
      return 0;
    }
    else if (argv[1][0] == '-')
    {
      // argument is a number
      numProcs = atoi(argv[1] + 1);
      if (numProcs == 0)
      {
        printf("### usage: TestIPCLocks [-N]\n"
               "where, N is the number of test processes to spawn.\n");
        return -1;
      }
    }
  }

  LOG("sleeping for 1 second\n");
  PR_Sleep(PR_SecondsToInterval(1));

  PRProcess **procs = (PRProcess **) malloc(sizeof(PRProcess*) * numProcs);
  int i;

  // else, spawn the child processes
  for (i=0; i<numProcs; ++i)
  {
    char *const argv[] = {"./TestIPCLocks", "-child", nsnull};
    LOG("spawning child test\n");
    procs[i] = PR_CreateProcess("./TestIPCLocks", argv, nsnull, nsnull);
  }

  PRInt32 exitCode;
  for (i=0; i<numProcs; ++i)
    PR_WaitProcess(procs[i], &exitCode);
  
  return 0;
}
