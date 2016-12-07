/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We need extended process and thread attribute support
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0600

#include "base/process_util.h"

#include <windows.h>
#include <winternl.h>
#include <psapi.h>

#include "base/histogram.h"
#include "base/logging.h"
#include "base/win_util.h"

#include <algorithm>
#include "prenv.h"

#include "mozilla/WindowsVersion.h"

namespace {

// System pagesize. This value remains constant on x86/64 architectures.
const int PAGESIZE_KB = 4;

// HeapSetInformation function pointer.
typedef BOOL (WINAPI* HeapSetFn)(HANDLE, HEAP_INFORMATION_CLASS, PVOID, SIZE_T);

typedef BOOL (WINAPI * InitializeProcThreadAttributeListFn)(
  LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
  DWORD dwAttributeCount,
  DWORD dwFlags,
  PSIZE_T lpSize
);

typedef BOOL (WINAPI * DeleteProcThreadAttributeListFn)(
  LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList
);

typedef BOOL (WINAPI * UpdateProcThreadAttributeFn)(
  LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
  DWORD dwFlags,
  DWORD_PTR Attribute,
  PVOID lpValue,
  SIZE_T cbSize,
  PVOID lpPreviousValue,
  PSIZE_T lpReturnSize
);

static InitializeProcThreadAttributeListFn InitializeProcThreadAttributeListPtr;
static DeleteProcThreadAttributeListFn DeleteProcThreadAttributeListPtr;
static UpdateProcThreadAttributeFn UpdateProcThreadAttributePtr;

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

ProcessId GetCurrentProcId() {
  return ::GetCurrentProcessId();
}

ProcessHandle GetCurrentProcessHandle() {
  return ::GetCurrentProcess();
}

bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle) {
  // TODO(phajdan.jr): Take even more permissions out of this list.
  ProcessHandle result = OpenProcess(PROCESS_DUP_HANDLE |
                                         PROCESS_TERMINATE |
                                         PROCESS_QUERY_INFORMATION |
                                         SYNCHRONIZE,
                                     FALSE, pid);

  if (result == NULL) {
    return false;
  }

  *handle = result;
  return true;
}

bool OpenPrivilegedProcessHandle(ProcessId pid, ProcessHandle* handle) {
  ProcessHandle result = OpenProcess(PROCESS_DUP_HANDLE |
                                         PROCESS_TERMINATE |
                                         PROCESS_QUERY_INFORMATION |
                                         PROCESS_VM_READ |
                                         SYNCHRONIZE,
                                     FALSE, pid);

  if (result == NULL) {
    return false;
  }

  *handle = result;
  return true;
}

void CloseProcessHandle(ProcessHandle process) {
  // closing a handle twice on Windows can be catastrophic - after the first
  // close the handle value may be reused, so the second close will kill that
  // other new handle.
  BOOL ok = CloseHandle(process);
  DCHECK(ok);
}

// Helper for GetProcId()
bool GetProcIdViaGetProcessId(ProcessHandle process, DWORD* id) {
  // Dynamically get a pointer to GetProcessId().
  typedef DWORD (WINAPI *GetProcessIdFunction)(HANDLE);
  static GetProcessIdFunction GetProcessIdPtr = NULL;
  static bool initialize_get_process_id = true;
  if (initialize_get_process_id) {
    initialize_get_process_id = false;
    HMODULE kernel32_handle = GetModuleHandle(L"kernel32.dll");
    if (!kernel32_handle) {
      NOTREACHED();
      return false;
    }
    GetProcessIdPtr = reinterpret_cast<GetProcessIdFunction>(GetProcAddress(
        kernel32_handle, "GetProcessId"));
  }
  if (!GetProcessIdPtr)
    return false;
  // Ask for the process ID.
  *id = (*GetProcessIdPtr)(process);
  return true;
}

// Helper for GetProcId()
bool GetProcIdViaNtQueryInformationProcess(ProcessHandle process, DWORD* id) {
  // Dynamically get a pointer to NtQueryInformationProcess().
  typedef NTSTATUS (WINAPI *NtQueryInformationProcessFunction)(
      HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
  static NtQueryInformationProcessFunction NtQueryInformationProcessPtr = NULL;
  static bool initialize_query_information_process = true;
  if (initialize_query_information_process) {
    initialize_query_information_process = false;
    // According to nsylvain, ntdll.dll is guaranteed to be loaded, even though
    // the Windows docs seem to imply that you should LoadLibrary() it.
    HMODULE ntdll_handle = GetModuleHandle(L"ntdll.dll");
    if (!ntdll_handle) {
      NOTREACHED();
      return false;
    }
    NtQueryInformationProcessPtr =
        reinterpret_cast<NtQueryInformationProcessFunction>(GetProcAddress(
            ntdll_handle, "NtQueryInformationProcess"));
  }
  if (!NtQueryInformationProcessPtr)
    return false;
  // Ask for the process ID.
  PROCESS_BASIC_INFORMATION info;
  ULONG bytes_returned;
  NTSTATUS status = (*NtQueryInformationProcessPtr)(process,
                                                    ProcessBasicInformation,
                                                    &info, sizeof info,
                                                    &bytes_returned);
  if (!SUCCEEDED(status) || (bytes_returned != (sizeof info)))
    return false;

  *id = static_cast<DWORD>(info.UniqueProcessId);
  return true;
}

ProcessId GetProcId(ProcessHandle process) {
  // Get a handle to |process| that has PROCESS_QUERY_INFORMATION rights.
  HANDLE current_process = GetCurrentProcess();
  HANDLE process_with_query_rights;
  if (DuplicateHandle(current_process, process, current_process,
                      &process_with_query_rights, PROCESS_QUERY_INFORMATION,
                      false, 0)) {
    // Try to use GetProcessId(), if it exists.  Fall back on
    // NtQueryInformationProcess() otherwise (< Win XP SP1).
    DWORD id;
    bool success =
        GetProcIdViaGetProcessId(process_with_query_rights, &id) ||
        GetProcIdViaNtQueryInformationProcess(process_with_query_rights, &id);
    CloseHandle(process_with_query_rights);
    if (success)
      return id;
  }

  // We're screwed.
  NOTREACHED();
  return 0;
}

// from sandbox_policy_base.cc in a later version of the chromium ipc code...
bool IsInheritableHandle(HANDLE handle) {
  if (!handle)
    return false;
  if (handle == INVALID_HANDLE_VALUE)
    return false;
  // File handles (FILE_TYPE_DISK) and pipe handles are known to be
  // inheritable.  Console handles (FILE_TYPE_CHAR) are not
  // inheritable via PROC_THREAD_ATTRIBUTE_HANDLE_LIST.
  DWORD handle_type = GetFileType(handle);
  return handle_type == FILE_TYPE_DISK || handle_type == FILE_TYPE_PIPE;
}

void LoadThreadAttributeFunctions() {
  HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
  InitializeProcThreadAttributeListPtr =
    reinterpret_cast<InitializeProcThreadAttributeListFn>
    (GetProcAddress(kernel32, "InitializeProcThreadAttributeList"));
  DeleteProcThreadAttributeListPtr =
    reinterpret_cast<DeleteProcThreadAttributeListFn>
    (GetProcAddress(kernel32, "DeleteProcThreadAttributeList"));
  UpdateProcThreadAttributePtr =
    reinterpret_cast<UpdateProcThreadAttributeFn>
    (GetProcAddress(kernel32, "UpdateProcThreadAttribute"));
}

// Creates and returns a "thread attribute list" to pass to the child process.
// On return, is a pointer to a THREAD_ATTRIBUTE_LIST or NULL if either the
// functions we need aren't available (eg, XP or earlier) or the functions we
// need failed.
// The result of this function must be passed to FreeThreadAttributeList.
// Note that the pointer to the HANDLE array ends up embedded in the result of
// this function and must stay alive until FreeThreadAttributeList is called,
// hence it is passed in so the owner is the caller of this function.
LPPROC_THREAD_ATTRIBUTE_LIST CreateThreadAttributeList(HANDLE *handlesToInherit,
                                                       int handleCount) {
  if (!InitializeProcThreadAttributeListPtr ||
      !DeleteProcThreadAttributeListPtr ||
      !UpdateProcThreadAttributePtr)
    LoadThreadAttributeFunctions();
  // shouldn't happen as we are only called for Vista+, but better safe than sorry...
  if (!InitializeProcThreadAttributeListPtr ||
      !DeleteProcThreadAttributeListPtr ||
      !UpdateProcThreadAttributePtr)
    return NULL;

  SIZE_T threadAttrSize;
  LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList = NULL;

  if (!(*InitializeProcThreadAttributeListPtr)(NULL, 1, 0, &threadAttrSize) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    goto fail;
  lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>
                              (malloc(threadAttrSize));
  if (!lpAttributeList ||
      !(*InitializeProcThreadAttributeListPtr)(lpAttributeList, 1, 0, &threadAttrSize))
    goto fail;

  if (!(*UpdateProcThreadAttributePtr)(lpAttributeList,
                  0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                  handlesToInherit,
                  sizeof(handlesToInherit[0]) * handleCount,
                  NULL, NULL)) {
    (*DeleteProcThreadAttributeListPtr)(lpAttributeList);
    goto fail;
  }
  return lpAttributeList;

fail:
  if (lpAttributeList)
    free(lpAttributeList);
  return NULL;
}

// Frees the data returned by CreateThreadAttributeList.
void FreeThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList) {
  // must be impossible to get a NULL DeleteProcThreadAttributeListPtr, as
  // we already checked it existed when creating the data we are now freeing.
  (*DeleteProcThreadAttributeListPtr)(lpAttributeList);
  free(lpAttributeList);
}

bool LaunchApp(const std::wstring& cmdline,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {

  // We want to inherit the std handles so dump() statements and assertion
  // messages in the child process can be seen - but we *do not* want to
  // blindly have all handles inherited.  Vista and later has a technique
  // where only specified handles are inherited - so we use this technique if
  // we can.  If that technique isn't available (or it fails), we just don't
  // inherit anything.  This can cause us a problem for Windows XP testing,
  // because we sometimes need the handles to get inherited for test logging to
  // work. So we also inherit when a specific environment variable is set.
  DWORD dwCreationFlags = 0;
  BOOL bInheritHandles = FALSE;
  // We use a STARTUPINFOEX, but if we can't do the thread attribute thing, we
  // just pass the size of a STARTUPINFO.
  STARTUPINFOEX startup_info_ex;
  ZeroMemory(&startup_info_ex, sizeof(startup_info_ex));
  STARTUPINFO &startup_info = startup_info_ex.StartupInfo;
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW;
  startup_info.wShowWindow = start_hidden ? SW_HIDE : SW_SHOW;

  // Per the comment in CreateThreadAttributeList, lpAttributeList will contain
  // a pointer to handlesToInherit, so make sure they have the same lifetime.
  LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList = NULL;
  HANDLE handlesToInherit[2];
  int handleCount = 0;

  // Don't even bother trying pre-Vista...
  if (mozilla::IsVistaOrLater()) {
    // setup our handle array first - if we end up with no handles that can
    // be inherited we can avoid trying to do the ThreadAttributeList dance...
    HANDLE stdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE stdErr = ::GetStdHandle(STD_ERROR_HANDLE);

    if (IsInheritableHandle(stdOut))
      handlesToInherit[handleCount++] = stdOut;
    if (stdErr != stdOut && IsInheritableHandle(stdErr))
      handlesToInherit[handleCount++] = stdErr;

    if (handleCount) {
      lpAttributeList = CreateThreadAttributeList(handlesToInherit, handleCount);
      if (lpAttributeList) {
        // it's safe to inherit handles, so arrange for that...
        startup_info.cb = sizeof(startup_info_ex);
        startup_info.dwFlags |= STARTF_USESTDHANDLES;
        startup_info.hStdOutput = stdOut;
        startup_info.hStdError = stdErr;
        startup_info.hStdInput = INVALID_HANDLE_VALUE;
        startup_info_ex.lpAttributeList = lpAttributeList;
        dwCreationFlags |= EXTENDED_STARTUPINFO_PRESENT;
        bInheritHandles = TRUE;
      }
    }
  } else if (PR_GetEnv("MOZ_WIN_INHERIT_STD_HANDLES_PRE_VISTA")) {
    // Even if we can't limit what gets inherited, we sometimes want to inherit
    // stdout/err for testing purposes.
    startup_info.dwFlags |= STARTF_USESTDHANDLES;
    startup_info.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
    startup_info.hStdError = ::GetStdHandle(STD_ERROR_HANDLE);
    startup_info.hStdInput = INVALID_HANDLE_VALUE;
    bInheritHandles = TRUE;
  }

  PROCESS_INFORMATION process_info;
  BOOL createdOK = CreateProcess(NULL,
                     const_cast<wchar_t*>(cmdline.c_str()), NULL, NULL,
                     bInheritHandles, dwCreationFlags, NULL, NULL,
                     &startup_info, &process_info);
  if (lpAttributeList)
    FreeThreadAttributeList(lpAttributeList);
  if (!createdOK)
    return false;

  gProcessLog.print("==> process %d launched child process %d (%S)\n",
                    GetCurrentProcId(),
                    process_info.dwProcessId,
                    cmdline.c_str());

  // Handles must be closed or they will leak
  CloseHandle(process_info.hThread);

  if (wait)
    WaitForSingleObject(process_info.hProcess, INFINITE);

  // If the caller wants the process handle, we won't close it.
  if (process_handle) {
    *process_handle = process_info.hProcess;
  } else {
    CloseHandle(process_info.hProcess);
  }
  return true;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {
  return LaunchApp(cl.command_line_string(), wait,
                   start_hidden, process_handle);
}

bool KillProcess(ProcessHandle process, int exit_code, bool wait) {
  bool result = (TerminateProcess(process, exit_code) != FALSE);
  if (result && wait) {
    // The process may not end immediately due to pending I/O
    if (WAIT_OBJECT_0 != WaitForSingleObject(process, 60 * 1000))
      DLOG(ERROR) << "Error waiting for process exit: " << GetLastError();
  } else if (!result) {
    DLOG(ERROR) << "Unable to terminate process: " << GetLastError();
  }
  return result;
}

bool DidProcessCrash(bool* child_exited, ProcessHandle handle) {
  DWORD exitcode = 0;

  if (child_exited)
    *child_exited = true;  // On Windows it an error to call this function if
                           // the child hasn't already exited.
  if (!::GetExitCodeProcess(handle, &exitcode)) {
    NOTREACHED();
    return false;
  }
  if (exitcode == STILL_ACTIVE) {
    // The process is likely not dead or it used 0x103 as exit code.
    NOTREACHED();
    return false;
  }

  // Warning, this is not generic code; it heavily depends on the way
  // the rest of the code kills a process.

  if (exitcode == PROCESS_END_NORMAL_TERMINATON ||
      exitcode == PROCESS_END_KILLED_BY_USER ||
      exitcode == PROCESS_END_PROCESS_WAS_HUNG ||
      exitcode == 0xC0000354 ||     // STATUS_DEBUGGER_INACTIVE.
      exitcode == 0xC000013A ||     // Control-C/end session.
      exitcode == 0x40010004) {     // Debugger terminated process/end session.
    return false;
  }

  return true;
}

void SetCurrentProcessPrivileges(ChildPrivileges privs) {

}

///////////////////////////////////////////////////////////////////////////////
// ProcesMetrics

ProcessMetrics::ProcessMetrics(ProcessHandle process) : process_(process),
                                                        last_time_(0),
                                                        last_system_time_(0) {
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);
  processor_count_ = system_info.dwNumberOfProcessors;
}

// static
ProcessMetrics* ProcessMetrics::CreateProcessMetrics(ProcessHandle process) {
  return new ProcessMetrics(process);
}

ProcessMetrics::~ProcessMetrics() { }

static uint64_t FileTimeToUTC(const FILETIME& ftime) {
  LARGE_INTEGER li;
  li.LowPart = ftime.dwLowDateTime;
  li.HighPart = ftime.dwHighDateTime;
  return li.QuadPart;
}

int ProcessMetrics::GetCPUUsage() {
  FILETIME now;
  FILETIME creation_time;
  FILETIME exit_time;
  FILETIME kernel_time;
  FILETIME user_time;

  GetSystemTimeAsFileTime(&now);

  if (!GetProcessTimes(process_, &creation_time, &exit_time,
                       &kernel_time, &user_time)) {
    // We don't assert here because in some cases (such as in the Task Manager)
    // we may call this function on a process that has just exited but we have
    // not yet received the notification.
    return 0;
  }
  int64_t system_time = (FileTimeToUTC(kernel_time) + FileTimeToUTC(user_time)) /
                        processor_count_;
  int64_t time = FileTimeToUTC(now);

  if ((last_system_time_ == 0) || (last_time_ == 0)) {
    // First call, just set the last values.
    last_system_time_ = system_time;
    last_time_ = time;
    return 0;
  }

  int64_t system_time_delta = system_time - last_system_time_;
  int64_t time_delta = time - last_time_;
  DCHECK(time_delta != 0);
  if (time_delta == 0)
    return 0;

  // We add time_delta / 2 so the result is rounded.
  int cpu = static_cast<int>((system_time_delta * 100 + time_delta / 2) /
                             time_delta);

  last_system_time_ = system_time;
  last_time_ = time;

  return cpu;
}

}  // namespace base
