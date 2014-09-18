// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <windows.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"

namespace base {

// static
int SysInfo::NumberOfProcessors() {
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  return static_cast<int>(info.dwNumberOfProcessors);
}

// static
int64_t SysInfo::AmountOfPhysicalMemory() {
  MEMORYSTATUSEX memory_info;
  memory_info.dwLength = sizeof(memory_info);
  if (!GlobalMemoryStatusEx(&memory_info)) {
    NOTREACHED();
    return 0;
  }

  int64_t rv = static_cast<int64_t>(memory_info.ullTotalPhys);
  if (rv < 0)
    rv = kint64max;
  return rv;
}

// static
int64_t SysInfo::AmountOfFreeDiskSpace(const std::wstring& path) {
  ULARGE_INTEGER available, total, free;
  if (!GetDiskFreeSpaceExW(path.c_str(), &available, &total, &free)) {
    return -1;
  }
  int64_t rv = static_cast<int64_t>(available.QuadPart);
  if (rv < 0)
    rv = kint64max;
  return rv;
}

// static
bool SysInfo::HasEnvVar(const wchar_t* var) {
  return GetEnvironmentVariable(var, NULL, 0) != 0;
}

// static
std::wstring SysInfo::GetEnvVar(const wchar_t* var) {
  DWORD value_length = GetEnvironmentVariable(var, NULL, 0);
  if (value_length == 0) {
    return L"";
  }
  scoped_array<wchar_t> value(new wchar_t[value_length]);
  GetEnvironmentVariable(var, value.get(), value_length);
  return std::wstring(value.get());
}

// static
std::string SysInfo::OperatingSystemName() {
  return "Windows NT";
}

// static
std::string SysInfo::OperatingSystemVersion() {
  OSVERSIONINFO info = {0};
  info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&info);

  return StringPrintf("%lu.%lu", info.dwMajorVersion, info.dwMinorVersion);
}

// TODO: Implement OperatingSystemVersionComplete, which would include
// patchlevel/service pack number. See chrome/browser/views/bug_report_view.cc,
// BugReportView::SetOSVersion.

// static
std::string SysInfo::CPUArchitecture() {
  // TODO: Make this vary when we support any other architectures.
  return "x86";
}

// static
void SysInfo::GetPrimaryDisplayDimensions(int* width, int* height) {
  if (width)
    *width = GetSystemMetrics(SM_CXSCREEN);

  if (height)
    *height = GetSystemMetrics(SM_CYSCREEN);
}

// static
int SysInfo::DisplayCount() {
  return GetSystemMetrics(SM_CMONITORS);
}

// static
size_t SysInfo::VMAllocationGranularity() {
  SYSTEM_INFO sysinfo;
  GetSystemInfo(&sysinfo);

  return sysinfo.dwAllocationGranularity;
}

// static
void SysInfo::OperatingSystemVersionNumbers(int32_t *major_version,
                                            int32_t *minor_version,
                                            int32_t *bugfix_version) {
  OSVERSIONINFO info = {0};
  info.dwOSVersionInfoSize = sizeof(info);
  GetVersionEx(&info);
  *major_version = info.dwMajorVersion;
  *minor_version = info.dwMinorVersion;
  *bugfix_version = 0;
}

}  // namespace base
