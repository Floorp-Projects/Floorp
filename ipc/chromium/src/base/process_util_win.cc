/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <io.h>
#ifndef STDOUT_FILENO
#  define STDOUT_FILENO 1
#endif

#include "base/command_line.h"
#include "base/histogram.h"
#include "base/logging.h"
#include "base/win_util.h"

#include "mozilla/ipc/LaunchError.h"
#include "mozilla/Result.h"

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>

namespace {

typedef BOOL(WINAPI* InitializeProcThreadAttributeListFn)(
    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList, DWORD dwAttributeCount,
    DWORD dwFlags, PSIZE_T lpSize);

typedef BOOL(WINAPI* DeleteProcThreadAttributeListFn)(
    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList);

typedef BOOL(WINAPI* UpdateProcThreadAttributeFn)(
    LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList, DWORD dwFlags,
    DWORD_PTR Attribute, PVOID lpValue, SIZE_T cbSize, PVOID lpPreviousValue,
    PSIZE_T lpReturnSize);

static InitializeProcThreadAttributeListFn InitializeProcThreadAttributeListPtr;
static DeleteProcThreadAttributeListFn DeleteProcThreadAttributeListPtr;
static UpdateProcThreadAttributeFn UpdateProcThreadAttributePtr;

static mozilla::EnvironmentLog gProcessLog("MOZ_PROCESS_LOG");

}  // namespace

namespace base {

ProcessId GetCurrentProcId() { return ::GetCurrentProcessId(); }

ProcessHandle GetCurrentProcessHandle() { return ::GetCurrentProcess(); }

bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle) {
  // TODO(phajdan.jr): Take even more permissions out of this list.
  ProcessHandle result =
      OpenProcess(PROCESS_DUP_HANDLE | PROCESS_TERMINATE |
                      PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                  FALSE, pid);

  if (result == NULL) {
    return false;
  }

  *handle = result;
  return true;
}

bool OpenPrivilegedProcessHandle(ProcessId pid, ProcessHandle* handle) {
  ProcessHandle result =
      OpenProcess(PROCESS_DUP_HANDLE | PROCESS_TERMINATE |
                      PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE,
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

ProcessId GetProcId(ProcessHandle process) {
  if (process == base::kInvalidProcessHandle || process == nullptr) {
    return 0;
  }
  // This returns 0 if we have insufficient rights to query the process handle.
  // Invalid handles or non-process handles will cause a diagnostic assert.
  ProcessId result = GetProcessId(process);
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  CHECK(result != 0 || GetLastError() != ERROR_INVALID_HANDLE)
  << "process handle = " << process;
#endif
  return result;
}

// from sandbox_policy_base.cc in a later version of the chromium ipc code...
bool IsInheritableHandle(HANDLE handle) {
  if (!handle) return false;
  if (handle == INVALID_HANDLE_VALUE) return false;
  // File handles (FILE_TYPE_DISK) and pipe handles are known to be
  // inheritable.  Console handles (FILE_TYPE_CHAR) are not
  // inheritable via PROC_THREAD_ATTRIBUTE_HANDLE_LIST.
  DWORD handle_type = GetFileType(handle);
  return handle_type == FILE_TYPE_DISK || handle_type == FILE_TYPE_PIPE;
}

void LoadThreadAttributeFunctions() {
  HMODULE kernel32 = GetModuleHandle(L"kernel32.dll");
  InitializeProcThreadAttributeListPtr =
      reinterpret_cast<InitializeProcThreadAttributeListFn>(
          GetProcAddress(kernel32, "InitializeProcThreadAttributeList"));
  DeleteProcThreadAttributeListPtr =
      reinterpret_cast<DeleteProcThreadAttributeListFn>(
          GetProcAddress(kernel32, "DeleteProcThreadAttributeList"));
  UpdateProcThreadAttributePtr = reinterpret_cast<UpdateProcThreadAttributeFn>(
      GetProcAddress(kernel32, "UpdateProcThreadAttribute"));
}

// Creates and returns a "thread attribute list" to pass to the child process.
// On return, is a pointer to a THREAD_ATTRIBUTE_LIST or NULL if either the
// functions we need aren't available (eg, XP or earlier) or the functions we
// need failed.
// The result of this function must be passed to FreeThreadAttributeList.
// Note that the pointer to the HANDLE array ends up embedded in the result of
// this function and must stay alive until FreeThreadAttributeList is called,
// hence it is passed in so the owner is the caller of this function.
LPPROC_THREAD_ATTRIBUTE_LIST CreateThreadAttributeList(HANDLE* handlesToInherit,
                                                       int handleCount) {
  if (!InitializeProcThreadAttributeListPtr ||
      !DeleteProcThreadAttributeListPtr || !UpdateProcThreadAttributePtr)
    LoadThreadAttributeFunctions();
  // shouldn't happen as we are only called for Vista+, but better safe than
  // sorry...
  if (!InitializeProcThreadAttributeListPtr ||
      !DeleteProcThreadAttributeListPtr || !UpdateProcThreadAttributePtr)
    return NULL;

  SIZE_T threadAttrSize;
  LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList = NULL;

  if (!(*InitializeProcThreadAttributeListPtr)(NULL, 1, 0, &threadAttrSize) &&
      GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    goto fail;
  lpAttributeList =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(threadAttrSize));
  if (!lpAttributeList || !(*InitializeProcThreadAttributeListPtr)(
                              lpAttributeList, 1, 0, &threadAttrSize))
    goto fail;

  if (!(*UpdateProcThreadAttributePtr)(
          lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
          handlesToInherit, sizeof(handlesToInherit[0]) * handleCount, NULL,
          NULL)) {
    (*DeleteProcThreadAttributeListPtr)(lpAttributeList);
    goto fail;
  }
  return lpAttributeList;

fail:
  if (lpAttributeList) free(lpAttributeList);
  return NULL;
}

// Frees the data returned by CreateThreadAttributeList.
void FreeThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList) {
  // must be impossible to get a NULL DeleteProcThreadAttributeListPtr, as
  // we already checked it existed when creating the data we are now freeing.
  (*DeleteProcThreadAttributeListPtr)(lpAttributeList);
  free(lpAttributeList);
}

// The next two functions are from chromium/base/environment.cc
//
// Parses a null-terminated input string of an environment block. The key is
// placed into the given string, and the total length of the line, including
// the terminating null, is returned.
static size_t ParseEnvLine(const NativeEnvironmentString::value_type* input,
                           NativeEnvironmentString* key) {
  // Skip to the equals or end of the string, this is the key.
  size_t cur = 0;
  while (input[cur] && input[cur] != '=') cur++;
  *key = NativeEnvironmentString(&input[0], cur);

  // Now just skip to the end of the string.
  while (input[cur]) cur++;
  return cur + 1;
}

std::wstring AlterEnvironment(const wchar_t* env,
                              const EnvironmentMap& changes) {
  std::wstring result;

  // First copy all unmodified values to the output.
  size_t cur_env = 0;
  std::wstring key;
  while (env[cur_env]) {
    const wchar_t* line = &env[cur_env];
    size_t line_length = ParseEnvLine(line, &key);

    // Keep only values not specified in the change vector.
    EnvironmentMap::const_iterator found_change = changes.find(key);
    if (found_change == changes.end()) result.append(line, line_length);

    cur_env += line_length;
  }

  // Now append all modified and new values.
  for (EnvironmentMap::const_iterator i = changes.begin(); i != changes.end();
       ++i) {
    if (!i->second.empty()) {
      result.append(i->first);
      result.push_back('=');
      result.append(i->second);
      result.push_back(0);
    }
  }

  // An additional null marks the end of the list. We always need a double-null
  // in case nothing was added above.
  if (result.empty()) result.push_back(0);
  result.push_back(0);
  return result;
}

Result<Ok, LaunchError> LaunchApp(const std::wstring& cmdline,
                                  const LaunchOptions& options,
                                  ProcessHandle* process_handle) {
  // We want to inherit the std handles so dump() statements and assertion
  // messages in the child process can be seen - but we *do not* want to
  // blindly have all handles inherited.  Vista and later has a technique
  // where only specified handles are inherited - so we use this technique.
  // If that fails we just don't inherit anything.
  DWORD dwCreationFlags = 0;
  BOOL bInheritHandles = FALSE;

  // We use a STARTUPINFOEX, but if we can't do the thread attribute thing, we
  // just pass the size of a STARTUPINFO.
  STARTUPINFOEX startup_info_ex;
  ZeroMemory(&startup_info_ex, sizeof(startup_info_ex));
  STARTUPINFO& startup_info = startup_info_ex.StartupInfo;
  startup_info.cb = sizeof(startup_info);
  startup_info.dwFlags = STARTF_USESHOWWINDOW | STARTF_FORCEOFFFEEDBACK;
  startup_info.wShowWindow = options.start_hidden ? SW_HIDE : SW_SHOW;

  // Per the comment in CreateThreadAttributeList, lpAttributeList will contain
  // a pointer to handlesToInherit, so make sure they have the same lifetime.
  LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList = NULL;
  std::vector<HANDLE> handlesToInherit;
  for (HANDLE h : options.handles_to_inherit) {
    if (SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT) ==
        0) {
      MOZ_DIAGNOSTIC_ASSERT(false, "SetHandleInformation failed");
      return Err(LaunchError("SetHandleInformation", GetLastError()));
    }
    handlesToInherit.push_back(h);
  }

  // setup our handle array first - if we end up with no handles that can
  // be inherited we can avoid trying to do the ThreadAttributeList dance...
  HANDLE stdOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
  HANDLE stdErr = ::GetStdHandle(STD_ERROR_HANDLE);

  if (IsInheritableHandle(stdOut)) handlesToInherit.push_back(stdOut);
  if (stdErr != stdOut && IsInheritableHandle(stdErr))
    handlesToInherit.push_back(stdErr);

  if (!handlesToInherit.empty()) {
    lpAttributeList = CreateThreadAttributeList(handlesToInherit.data(),
                                                handlesToInherit.size());
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

  dwCreationFlags |= CREATE_UNICODE_ENVIRONMENT;
  if (options.start_independent) {
    dwCreationFlags |= CREATE_BREAKAWAY_FROM_JOB;
  }

  LPTCH original_environment = GetEnvironmentStrings();
  base::NativeEnvironmentString new_environment =
      AlterEnvironment(original_environment, options.env_map);
  // Ignore return value? What can we do?
  FreeEnvironmentStrings(original_environment);
  LPVOID new_env_ptr = (void*)new_environment.data();

  PROCESS_INFORMATION process_info;

  BOOL createdOK = CreateProcess(
      NULL, const_cast<wchar_t*>(cmdline.c_str()), NULL, NULL, bInheritHandles,
      dwCreationFlags, new_env_ptr, NULL, &startup_info, &process_info);
  if (lpAttributeList) FreeThreadAttributeList(lpAttributeList);
  if (!createdOK) {
    DLOG(WARNING) << "CreateProcess Failed: " << GetLastError();
    return Err(LaunchError("CreateProcess", GetLastError()));
  }

  gProcessLog.print("==> process %d launched child process %d (%S)\n",
                    GetCurrentProcId(), process_info.dwProcessId,
                    cmdline.c_str());

  // Handles must be closed or they will leak
  CloseHandle(process_info.hThread);

  if (options.wait) WaitForSingleObject(process_info.hProcess, INFINITE);

  // If the caller wants the process handle, we won't close it.
  if (process_handle) {
    *process_handle = process_info.hProcess;
  } else {
    CloseHandle(process_info.hProcess);
  }
  return Ok();
}

Result<Ok, LaunchError> LaunchApp(const CommandLine& cl,
                                  const LaunchOptions& options,
                                  ProcessHandle* process_handle) {
  return LaunchApp(cl.command_line_string(), options, process_handle);
}

bool KillProcess(ProcessHandle process, int exit_code) {
  // INVALID_HANDLE_VALUE is not actually an invalid handle value, but
  // instead is the value returned by GetCurrentProcess().  nullptr,
  // in contrast, *is* an invalid handle value.  Both values are too
  // easy to accidentally try to kill, and neither is legitimately
  // used by this function's callers, so reject them.
  if (!process || process == INVALID_HANDLE_VALUE) {
    CHROMIUM_LOG(WARNING)
        << "base::KillProcess refusing to terminate process handle " << process;
    return false;
  }
  bool result = (TerminateProcess(process, exit_code) != FALSE);
  if (!result) {
    DLOG(ERROR) << "Unable to terminate process: " << GetLastError();
  }
  return result;
}

}  // namespace base

namespace mozilla {

EnvironmentLog::EnvironmentLog(const char* varname, size_t len) {
  wchar_t wvarname[len];
  std::copy(varname, varname + len, wvarname);
  const wchar_t* e = _wgetenv(wvarname);
  if (e && *e) {
    fname_ = e;
  }
}

void EnvironmentLog::print(const char* format, ...) {
  if (!fname_.size()) return;

  FILE* f;
  if (fname_.compare(L"-") == 0) {
    f = fdopen(dup(STDOUT_FILENO), "a");
  } else {
    f = _wfopen(fname_.c_str(), L"a");
  }

  if (!f) return;

  va_list a;
  va_start(a, format);
  vfprintf(f, format, a);
  va_end(a);
  fclose(f);
}

}  // namespace mozilla
