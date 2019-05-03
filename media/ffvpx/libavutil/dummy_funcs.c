/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "avutil.h"
#include "hwcontext.h"

// cpu_internal.c
#if !defined(_ARM64_)
int ff_get_cpu_flags_aarch64(void) { return 0; }
#endif
#if !defined(__arm__)
int ff_get_cpu_flags_arm(void) { return 0; }
#endif
int ff_get_cpu_flags_ppc(void) { return 0; }

// float_dsp.c
#include "float_dsp.h"
#if !defined(_ARM64_)
void ff_float_dsp_init_aarch64(AVFloatDSPContext *fdsp) {}
#endif
void ff_float_dsp_init_ppc(AVFloatDSPContext *fdsp, int strict) {}
void ff_float_dsp_init_mips(AVFloatDSPContext *fdsp) {}
#if !defined(__arm__)
void ff_float_dsp_init_arm(AVFloatDSPContext *fdsp) {}
#endif

// cpu.c
#if !defined(_ARM64_)
size_t ff_get_cpu_max_align_aarch64() { return 0; }
#endif
size_t ff_get_cpu_max_align_ppc() { return 0; }
#if !defined(__arm__)
size_t ff_get_cpu_max_align_arm() { return 0; }
#endif
