/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPkgInt_h__
#define nsPkgInt_h__
#include "nscore.h"

typedef enum {
  eIdxSft4bits  = 3,
  eIdxSft8bits  = 2,
  eIdxSft16bits = 1
} nsIdxSft; 

typedef enum {
  eSftMsk4bits  = 7,
  eSftMsk8bits  = 3,
  eSftMsk16bits = 1
} nsSftMsk; 

typedef enum {
  eBitSft4bits  = 2,
  eBitSft8bits  = 3,
  eBitSft16bits = 4
} nsBitSft; 

typedef enum {
  eUnitMsk4bits  = 0x0000000FL,
  eUnitMsk8bits  = 0x000000FFL,
  eUnitMsk16bits = 0x0000FFFFL
} nsUnitMsk; 

typedef struct nsPkgInt {
  nsIdxSft  idxsft;
  nsSftMsk  sftmsk;
  nsBitSft  bitsft;
  nsUnitMsk unitmsk;
  const uint32_t* const data;
} nsPkgInt;


#define PCK16BITS(a,b)            ((uint32_t)(((b) << 16) | (a)))

#define PCK8BITS(a,b,c,d)         PCK16BITS( ((uint32_t)(((b) << 8) | (a))),  \
                                             ((uint32_t)(((d) << 8) | (c))))

#define PCK4BITS(a,b,c,d,e,f,g,h) PCK8BITS(  ((uint32_t)(((b) << 4) | (a))), \
                                             ((uint32_t)(((d) << 4) | (c))), \
                                             ((uint32_t)(((f) << 4) | (e))), \
                                             ((uint32_t)(((h) << 4) | (g))) )

#define GETFROMPCK(i, c) \
 (((((c).data)[(i)>>(c).idxsft])>>(((i)&(c).sftmsk)<<(c).bitsft))&(c).unitmsk)

#endif /* nsPkgInt_h__ */

