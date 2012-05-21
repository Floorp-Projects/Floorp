/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __UNIPRIV__
#define __UNIPRIV__

#include "ubase.h"
#include "umap.h"
#include "uconvutil.h"

#ifdef __cplusplus
extern "C" {
#endif

PRBool	uMapCode(const uTable *uT, 
                    PRUint16 in, 
                    PRUint16* out);

PRBool 	uGenerate(uScanClassID scanClass,
                  PRInt32* state, 
                  PRUint16 in, 
                  unsigned char* out, 
                  PRUint32 outbuflen, 
                  PRUint32* outlen);

PRBool 	uScan(uScanClassID scanClass,
              PRInt32 *state, 
              unsigned char *in,
              PRUint16 *out, 
              PRUint32 inbuflen, 
              PRUint32* inscanlen);

PRBool 	uGenerateShift(uShiftOutTable *shift,
                       PRInt32* state, 
                       PRUint16 in, 
                       unsigned char* out, 
                       PRUint32 outbuflen, 
                       PRUint32* outlen);

PRBool 	uScanShift(uShiftInTable *shift, 
                   PRInt32 *state, 
                   unsigned char *in,
                   PRUint16 *out, 
                   PRUint32 inbuflen, 
                   PRUint32* inscanlen);

#ifdef __cplusplus
}
#endif

#endif /* __UNIPRIV__ */
