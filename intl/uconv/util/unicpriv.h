/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __UNIPRIV__
#define __UNIPRIV__

#include <stdint.h>
#include "umap.h"
#include "uconvutil.h"

#ifdef __cplusplus
extern "C" {
#endif

int uMapCode(const uTable *uT,
             uint16_t in,
             uint16_t* out);

int uGenerate(uScanClassID scanClass,
              int32_t* state,
              uint16_t in,
              unsigned char* out,
              uint32_t outbuflen,
              uint32_t* outlen);

int uScan(uScanClassID scanClass,
          int32_t *state,
          unsigned char *in,
          uint16_t *out,
          uint32_t inbuflen,
          uint32_t* inscanlen);

int uGenerateShift(uShiftOutTable *shift,
                   int32_t* state,
                   uint16_t in,
                   unsigned char* out,
                   uint32_t outbuflen,
                   uint32_t* outlen);

int uScanShift(uShiftInTable *shift,
               int32_t *state,
               unsigned char *in,
               uint16_t *out,
               uint32_t inbuflen,
               uint32_t* inscanlen);

#ifdef __cplusplus
}
#endif

#endif /* __UNIPRIV__ */
