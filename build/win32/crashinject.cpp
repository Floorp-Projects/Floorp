/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Given a PID, this program attempts to inject a DLL into the process
 * with that PID. The DLL it attempts to inject, "crashinjectdll.dll",
 * must exist alongside this exe. The DLL will then crash the process.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

int main(int argc, char** argv)
{
  if (argc != 2) {
    fprintf(stderr, "Usage: crashinject <PID>\n");
    return 1;
  }

  int pid = atoi(argv[1]);
  if (pid <= 0) {
    fprintf(stderr, "Usage: crashinject <PID>\n");
    return 1;
  }

  // find our DLL to inject
  wchar_t filename[_MAX_PATH];
  if (GetModuleFileNameW(nullptr, filename,
                         sizeof(filename) / sizeof(wchar_t)) == 0)
    return 1;

  wchar_t* slash = wcsrchr(filename, L'\\');
  if (slash == nullptr)
    return 1;

  slash++;
  wcscpy(slash, L"crashinjectdll.dll");

  // now find our target process
  HANDLE targetProc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION,
                                  FALSE,
                                  pid);
  if (targetProc == nullptr) {
    fprintf(stderr, "Error %d opening target process\n", GetLastError());
    return 1;
  }

  /*
   * This is sort of insane, but we're implementing a technique described here:
   * http://www.codeproject.com/KB/threads/winspy.aspx#section_2
   *
   * The gist is to use CreateRemoteThread to create a thread in the other
   * process, but cheat and make the thread function kernel32!LoadLibrary,
   * so that the only remote data we have to pass to the other process
   * is the path to the library we want to load. The library we're loading
   * will then do its dirty work inside the other process.
   */
  HMODULE hKernel32 = GetModuleHandleW(L"Kernel32");
  // allocate some memory to hold the path in the remote process
  void*   pLibRemote = VirtualAllocEx(targetProc, nullptr, sizeof(filename),
                                      MEM_COMMIT, PAGE_READWRITE);
  if (pLibRemote == nullptr) {
    fprintf(stderr, "Error %d in VirtualAllocEx\n", GetLastError());
    CloseHandle(targetProc);
    return 1;
  }

  if (!WriteProcessMemory(targetProc, pLibRemote, (void*)filename,
                          sizeof(filename), nullptr)) {
    fprintf(stderr, "Error %d in WriteProcessMemory\n", GetLastError());
    VirtualFreeEx(targetProc, pLibRemote, sizeof(filename), MEM_RELEASE);
    CloseHandle(targetProc);
    return 1;
  }
  // Now create a thread in the target process that will load our DLL
  HANDLE hThread = CreateRemoteThread(
                     targetProc, nullptr, 0,
                     (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32,
                                                            "LoadLibraryW"),
                     pLibRemote, 0, nullptr);
  if (hThread == nullptr) {
    fprintf(stderr, "Error %d in CreateRemoteThread\n", GetLastError());
    VirtualFreeEx(targetProc, pLibRemote, sizeof(filename), MEM_RELEASE);
    CloseHandle(targetProc);
    return 1;
  }
  WaitForSingleObject(hThread, INFINITE);
  // Cleanup, not that it's going to matter at this point
  CloseHandle(hThread);
  VirtualFreeEx(targetProc, pLibRemote, sizeof(filename), MEM_RELEASE);
  CloseHandle(targetProc);

  return 0;
}
