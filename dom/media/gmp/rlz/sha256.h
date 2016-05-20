/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Stripped down version of security/nss/lib/freebl/blapi.h
// and related headers.

#ifndef _SHA256_H_
#define _SHA256_H_

#define SHA256_LENGTH 32 

#include "prtypes.h"	/* for PRUintXX */
#include "prlong.h"

#ifdef __cplusplus 
extern "C" {
#endif

struct SHA256Context {
  union {
    PRUint32 w[64];	    /* message schedule, input buffer, plus 48 words */
    PRUint8  b[256];
  } u;
  PRUint32 h[8];		/* 8 state variables */
  PRUint32 sizeHi,sizeLo;	/* 64-bit count of hashed bytes. */
};

typedef struct SHA256Context SHA256Context;

extern void
SHA256_Begin(SHA256Context *ctx);

extern void 
SHA256_Update(SHA256Context *ctx, const unsigned char *input,
		    unsigned int inputLen);

extern void
SHA256_End(SHA256Context *ctx, unsigned char *digest,
           unsigned int *digestLen, unsigned int maxDigestLen);

#ifdef __cplusplus 
} /* extern C */
#endif

#endif /* _SHA256_H_ */
