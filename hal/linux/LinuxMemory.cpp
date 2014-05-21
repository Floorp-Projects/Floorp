/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "Hal.h"

namespace mozilla {
namespace hal_impl {

uint32_t
GetTotalSystemMemory()
{
  static uint32_t sTotalMemory;
  static bool sTotalMemoryObtained = false;

  if (!sTotalMemoryObtained) {
    sTotalMemoryObtained = true;

    FILE* fd = fopen("/proc/meminfo", "r");
    if (!fd) {
      return 0;
    }

    int rv = fscanf(fd, "MemTotal: %i kB", &sTotalMemory);

    if (fclose(fd) || rv != 1) {
      return 0;
    }
  }

  return sTotalMemory * 1024;
}

uint32_t
GetTotalSystemMemoryLevel()
{
  static uint32_t sTotalMemoryLevel = 1;
  uint32_t sTotalMemory;
  static bool sTotalMemoryObtained = false;

  if (!sTotalMemoryObtained) {
    sTotalMemoryObtained = true;

    FILE* fd = fopen("/proc/meminfo", "r");
    if (!fd) {
      return 0;
    }

    int rv = fscanf(fd, "MemTotal: %i kB", &sTotalMemory);

    if (fclose(fd) || rv != 1) {
      return 0;
    }

    // From KB to MiB
    sTotalMemory /= 1024;

    while (sTotalMemoryLevel <= sTotalMemory) {
      sTotalMemoryLevel *= 2;
    }
  }

  return sTotalMemoryLevel;
}

} // namespace hal_impl
} // namespace mozilla
