/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cpu.h"
#include <intrin.h>
#include <string>

namespace base {

CPU::CPU()
  : type_(0),
    family_(0),
    model_(0),
    stepping_(0),
    ext_model_(0),
    ext_family_(0),
    cpu_vendor_("unknown") {
  Initialize();
}

void CPU::Initialize() {
  int cpu_info[4] = {-1};
  char cpu_string[0x20];

  // __cpuid with an InfoType argument of 0 returns the number of
  // valid Ids in CPUInfo[0] and the CPU identification string in
  // the other three array elements. The CPU identification string is
  // not in linear order. The code below arranges the information
  // in a human readable form.
  //
  // More info can be found here:
  // http://msdn.microsoft.com/en-us/library/hskdteyh.aspx
  __cpuid(cpu_info, 0);
  int num_ids = cpu_info[0];
  memset(cpu_string, 0, sizeof(cpu_string));
  *(reinterpret_cast<int*>(cpu_string)) = cpu_info[1];
  *(reinterpret_cast<int*>(cpu_string+4)) = cpu_info[3];
  *(reinterpret_cast<int*>(cpu_string+8)) = cpu_info[2];

  // Interpret CPU feature information.
  if (num_ids > 0) {
    __cpuid(cpu_info, 1);
    stepping_ = cpu_info[0] & 0xf;
    model_ = (cpu_info[0] >> 4) & 0xf;
    family_ = (cpu_info[0] >> 8) & 0xf;
    type_ = (cpu_info[0] >> 12) & 0x3;
    ext_model_ = (cpu_info[0] >> 16) & 0xf;
    ext_family_ = (cpu_info[0] >> 20) & 0xff;
    cpu_vendor_ = cpu_string;
  }
}

}  // namespace base
