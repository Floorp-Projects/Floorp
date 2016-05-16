/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"

#include <windows.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "mozilla/UniquePtr.h"

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
  mozilla::UniquePtr<wchar_t[]> value(new wchar_t[value_length]);
  GetEnvironmentVariable(var, value.get(), value_length);
  return std::wstring(value.get());
}

// static
std::string SysInfo::OperatingSystemName() {
  return "Windows NT";
}

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

}  // namespace base
