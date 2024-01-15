/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalTypes.h"
#include <sys/types.h>
#include <sys/sysctl.h>
#include "mozilla/BitSet.h"

namespace mozilla::hal_impl {

mozilla::Maybe<hal::HeterogeneousCpuInfo> CreateHeterogeneousCpuInfo() {
#ifdef __aarch64__
  // As of now on Apple Silicon the number of *.logicalcpu_max is the same as
  // *.physicalcpu_max.
  size_t len = sizeof(uint32_t);
  uint32_t pCores = 0;
  if (sysctlbyname("hw.perflevel0.logicalcpu_max", &pCores, &len, nullptr, 0)) {
    return Nothing();
  }

  len = sizeof(uint32_t);
  uint32_t eCores = 0;
  if (sysctlbyname("hw.perflevel1.logicalcpu_max", &eCores, &len, nullptr, 0)) {
    return Nothing();
  }

  hal::HeterogeneousCpuInfo info;
  info.mTotalNumCpus = pCores + eCores;

  // The API has currently a limit how many cpu cores it can tell about.
  for (uint32_t i = 0; i < hal::HeterogeneousCpuInfo::MAX_CPUS; ++i) {
    if (pCores) {
      --pCores;
      info.mBigCpus[i] = true;
    } else if (eCores) {
      --eCores;
      info.mLittleCpus[i] = true;
    } else {
      break;
    }
  }

  return Some(info);
#else
  return Nothing();
#endif
}

const Maybe<hal::HeterogeneousCpuInfo>& GetHeterogeneousCpuInfo() {
  static const Maybe<hal::HeterogeneousCpuInfo> cpuInfo =
      CreateHeterogeneousCpuInfo();
  return cpuInfo;
}

}  // namespace mozilla::hal_impl
