/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "avutil.h"

// cpu_internal.c
int ff_get_cpu_flags_aarch64(void) { return 0; }
int ff_get_cpu_flags_arm(void) { return 0; }
int ff_get_cpu_flags_ppc(void) { return 0; }

// float_dsp.c
#include "float_dsp.h"
void ff_float_dsp_init_aarch64(AVFloatDSPContext *fdsp) {}
void ff_float_dsp_init_arm(AVFloatDSPContext *fdsp) {}
void ff_float_dsp_init_ppc(AVFloatDSPContext *fdsp, int strict) {}
void ff_float_dsp_init_mips(AVFloatDSPContext *fdsp) {}

int av_hwframe_get_buffer(struct AVBufferRef* hwframe_ref, struct AVFrame* frame, int flags) { return 0; }
