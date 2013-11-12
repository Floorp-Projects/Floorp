/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <windows.h>

// make sure we only ever spawn one thread
DWORD tid = -1;

DWORD WINAPI CrashingThread(
  LPVOID lpParameter
)
{
  // not a very friendly DLL
  volatile int* x = (int *)0x0;
  *x = 1;
  return 0;
}

BOOL WINAPI DllMain(
  HANDLE hinstDLL,
  DWORD dwReason,
  LPVOID lpvReserved
)
{
  if (tid == -1)
    // we have to crash on another thread because LoadLibrary() will
    // catch memory access errors and return failure to the calling process
    CreateThread(
                 nullptr,                // default security attributes
                 0,                      // use default stack size
                 CrashingThread,         // thread function name
                 nullptr,                // argument to thread function
                 0,                      // use default creation flags
                 &tid);                  // returns the thread identifier
  return TRUE;
}
