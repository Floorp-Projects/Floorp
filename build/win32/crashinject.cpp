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
 * The Original Code is Crash Injection Utility
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ted Mielczarek <ted.mielczarek@gmail.com>
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

/*
 * Given a PID, this program attempts to inject a DLL into the process
 * with that PID. The DLL it attempts to inject, "crashinjectdll.dll",
 * must exist alongside this exe. The DLL will then crash the process.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>

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
  if (GetModuleFileNameW(NULL, filename, sizeof(filename) / sizeof(wchar_t)) == 0)
    return 1;

  wchar_t* slash = wcsrchr(filename, L'\\');
  if (slash == NULL)
    return 1;

  slash++;
  wcscpy(slash, L"crashinjectdll.dll");

  // now find our target process
  HANDLE targetProc = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_CREATE_THREAD,
                                  FALSE,
                                  pid);
  if (targetProc == NULL) {
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
  void*   pLibRemote = VirtualAllocEx(targetProc, NULL, sizeof(filename),
                                      MEM_COMMIT, PAGE_READWRITE);
  if (pLibRemote == NULL) {
    fprintf(stderr, "Error %d in VirtualAllocEx\n", GetLastError());
    CloseHandle(targetProc);
    return 1;
  }

  if (!WriteProcessMemory(targetProc, pLibRemote, (void*)filename,
                          sizeof(filename), NULL)) {
    fprintf(stderr, "Error %d in WriteProcessMemory\n", GetLastError());
    VirtualFreeEx(targetProc, pLibRemote, sizeof(filename), MEM_RELEASE);
    CloseHandle(targetProc);
    return 1;
  }
  // Now create a thread in the target process that will load our DLL
  HANDLE hThread = CreateRemoteThread(
                     targetProc, NULL, 0,
                     (LPTHREAD_START_ROUTINE)GetProcAddress(hKernel32,
                                                            "LoadLibraryW"),
                     pLibRemote, 0, NULL);
  if (hThread == NULL) {
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
