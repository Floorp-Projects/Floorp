/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "Hal.h"
#include "HalLog.h"
#include "HalTypes.h"

#include "mozilla/BitSet.h"
#include "mozilla/CheckedInt.h"
#include "prsystem.h"
#include <fstream>

namespace mozilla::hal_impl {

mozilla::Maybe<HeterogeneousCpuInfo> CreateHeterogeneousCpuInfo() {
  CheckedInt<size_t> cpuCount = PR_GetNumberOfProcessors();
  if (!cpuCount.isValid()) {
    HAL_ERR("HeterogeneousCpuInfo: Failed to query processor count");
    return Nothing();
  }

  if (cpuCount.value() > HeterogeneousCpuInfo::MAX_CPUS) {
    HAL_ERR("HeterogeneousCpuInfo: Too many cpus");
    return Nothing();
  }

  std::vector<std::pair<int, int>> cpu_freqs;
  cpu_freqs.reserve(cpuCount.value());
  for (size_t i = 0; i < cpuCount.value(); i++) {
    std::stringstream freq_filename;
    // On all devices tested, even with isolated content processes we do have
    // permission to read this file. If, however, this function stops working
    // one day, then this may be a good place to start investigating.
    freq_filename << "/sys/devices/system/cpu/cpu" << i
                  << "/cpufreq/cpuinfo_max_freq";
    std::ifstream ifs(freq_filename.str());
    if (!ifs) {
      HAL_ERR("HeterogeneousCpuInfo: Failed to open file %s",
              freq_filename.str().c_str());
      return Nothing();
    }
    int freq;
    if (!(ifs >> freq)) {
      HAL_ERR("HeterogeneousCpuInfo: Failed to parse file %s",
              freq_filename.str().c_str());
      return Nothing();
    }
    cpu_freqs.push_back(std::make_pair(i, freq));
  }

  std::sort(cpu_freqs.begin(), cpu_freqs.end(),
            [](auto lhs, auto rhs) { return lhs.second < rhs.second; });

  HeterogeneousCpuInfo info;
  info.mTotalNumCpus = cpuCount.value();

  int lowest_freq = cpu_freqs.front().second;
  int highest_freq = cpu_freqs.back().second;
  for (const auto& [cpu, freq] : cpu_freqs) {
    if (freq == highest_freq) {
      info.mBigCpus[cpu] = true;
    } else if (freq == lowest_freq) {
      info.mLittleCpus[cpu] = true;
    } else {
      info.mMediumCpus[cpu] = true;
    }
  }

  HAL_LOG("HeterogeneousCpuInfo: little: %zu, med: %zu, big: %zu",
          info.mLittleCpus.Count(), info.mMediumCpus.Count(),
          info.mBigCpus.Count());

  return Some(info);
}

const Maybe<HeterogeneousCpuInfo>& GetHeterogeneousCpuInfo() {
  static const Maybe<HeterogeneousCpuInfo> cpuInfo =
      CreateHeterogeneousCpuInfo();
  return cpuInfo;
}

}  // namespace mozilla::hal_impl
