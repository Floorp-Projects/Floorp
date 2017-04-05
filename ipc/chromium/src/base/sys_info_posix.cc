/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_info.h"
#include "base/basictypes.h"

#include <errno.h>
#include <string.h>
#ifndef ANDROID
#include <sys/statvfs.h>
#endif
#include <sys/utsname.h>
#include <unistd.h>

#if defined(OS_MACOSX)
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#endif

#if defined(OS_NETBSD)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#include "base/logging.h"
#include "base/string_util.h"

namespace base {

int SysInfo::NumberOfProcessors() {
  // It seems that sysconf returns the number of "logical" processors on both
  // mac and linux.  So we get the number of "online logical" processors.
#ifdef _SC_NPROCESSORS_ONLN
  static long res = sysconf(_SC_NPROCESSORS_ONLN);
#else
  static long res = 1;
#endif
  if (res == -1) {
    NOTREACHED();
    return 1;
  }

  return static_cast<int>(res);
}

// static
int64_t SysInfo::AmountOfPhysicalMemory() {
  // _SC_PHYS_PAGES is not part of POSIX and not available on OS X
#if defined(OS_MACOSX)
  struct host_basic_info hostinfo;
  mach_msg_type_number_t count = HOST_BASIC_INFO_COUNT;
  int result = host_info(mach_host_self(),
                         HOST_BASIC_INFO,
                         reinterpret_cast<host_info_t>(&hostinfo),
                         &count);
  DCHECK_EQ(HOST_BASIC_INFO_COUNT, count);
  if (result != KERN_SUCCESS) {
    NOTREACHED();
    return 0;
  }

  return static_cast<int64_t>(hostinfo.max_mem);
#elif defined(OS_NETBSD)
  int mib[2];
  int rc;
  int64_t memSize;
  size_t len = sizeof(memSize);

  mib[0] = CTL_HW;
  mib[1] = HW_PHYSMEM64;
  rc = sysctl( mib, 2, &memSize, &len, NULL, 0 );
  if (-1 != rc)  {
    return memSize;
  }
  return 0;

#else
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages == -1 || page_size == -1) {
    NOTREACHED();
    return 0;
  }

  return static_cast<int64_t>(pages) * page_size;
#endif
}

// static
int64_t SysInfo::AmountOfFreeDiskSpace(const std::wstring& path) {
#ifndef ANDROID
  struct statvfs stats;
  if (statvfs(WideToUTF8(path).c_str(), &stats) != 0) {
    return -1;
  }
  return static_cast<int64_t>(stats.f_bavail) * stats.f_frsize;
#else
  return -1;
#endif
}

// static
bool SysInfo::HasEnvVar(const wchar_t* var) {
  std::string var_utf8 = WideToUTF8(std::wstring(var));
  return getenv(var_utf8.c_str()) != NULL;
}

// static
std::wstring SysInfo::GetEnvVar(const wchar_t* var) {
  std::string var_utf8 = WideToUTF8(std::wstring(var));
  char* value = getenv(var_utf8.c_str());
  if (!value) {
    return L"";
  } else {
    return UTF8ToWide(value);
  }
}

// static
std::string SysInfo::OperatingSystemName() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return "";
  }
  return std::string(info.sysname);
}

// static
std::string SysInfo::CPUArchitecture() {
  struct utsname info;
  if (uname(&info) < 0) {
    NOTREACHED();
    return "";
  }
  return std::string(info.machine);
}

// static
void SysInfo::GetPrimaryDisplayDimensions(int* width, int* height) {
  NOTIMPLEMENTED();
}

// static
int SysInfo::DisplayCount() {
  NOTIMPLEMENTED();
  return 1;
}

// static
size_t SysInfo::VMAllocationGranularity() {
  return getpagesize();
}

}  // namespace base
