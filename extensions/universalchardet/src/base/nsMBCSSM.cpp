/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCodingStateMachine.h"

/*
Modification from frank tang's original work:
. 0x00 is allowed as a legal character. Since some web pages contains this char in 
  text stream.
*/

static const uint32_t EUCJP_cls [ 256 / 8 ] = {
//PCK4BITS(5,4,4,4,4,4,4,4),  // 00 - 07 
PCK4BITS(4,4,4,4,4,4,4,4),  // 00 - 07 
PCK4BITS(4,4,4,4,4,4,5,5),  // 08 - 0f 
PCK4BITS(4,4,4,4,4,4,4,4),  // 10 - 17 
PCK4BITS(4,4,4,5,4,4,4,4),  // 18 - 1f 
PCK4BITS(4,4,4,4,4,4,4,4),  // 20 - 27 
PCK4BITS(4,4,4,4,4,4,4,4),  // 28 - 2f 
PCK4BITS(4,4,4,4,4,4,4,4),  // 30 - 37 
PCK4BITS(4,4,4,4,4,4,4,4),  // 38 - 3f 
PCK4BITS(4,4,4,4,4,4,4,4),  // 40 - 47 
PCK4BITS(4,4,4,4,4,4,4,4),  // 48 - 4f 
PCK4BITS(4,4,4,4,4,4,4,4),  // 50 - 57 
PCK4BITS(4,4,4,4,4,4,4,4),  // 58 - 5f 
PCK4BITS(4,4,4,4,4,4,4,4),  // 60 - 67 
PCK4BITS(4,4,4,4,4,4,4,4),  // 68 - 6f 
PCK4BITS(4,4,4,4,4,4,4,4),  // 70 - 77 
PCK4BITS(4,4,4,4,4,4,4,4),  // 78 - 7f 
PCK4BITS(5,5,5,5,5,5,5,5),  // 80 - 87 
PCK4BITS(5,5,5,5,5,5,1,3),  // 88 - 8f 
PCK4BITS(5,5,5,5,5,5,5,5),  // 90 - 97 
PCK4BITS(5,5,5,5,5,5,5,5),  // 98 - 9f 
PCK4BITS(5,2,2,2,2,2,2,2),  // a0 - a7 
PCK4BITS(2,2,2,2,2,2,2,2),  // a8 - af 
PCK4BITS(2,2,2,2,2,2,2,2),  // b0 - b7 
PCK4BITS(2,2,2,2,2,2,2,2),  // b8 - bf 
PCK4BITS(2,2,2,2,2,2,2,2),  // c0 - c7 
PCK4BITS(2,2,2,2,2,2,2,2),  // c8 - cf 
PCK4BITS(2,2,2,2,2,2,2,2),  // d0 - d7 
PCK4BITS(2,2,2,2,2,2,2,2),  // d8 - df 
PCK4BITS(0,0,0,0,0,0,0,0),  // e0 - e7 
PCK4BITS(0,0,0,0,0,0,0,0),  // e8 - ef 
PCK4BITS(0,0,0,0,0,0,0,0),  // f0 - f7 
PCK4BITS(0,0,0,0,0,0,0,5)   // f8 - ff 
};


static const uint32_t EUCJP_st [ 5] = {
PCK4BITS(     3,     4,     3,     5,eStart,eError,eError,eError),//00-07 
PCK4BITS(eError,eError,eError,eError,eItsMe,eItsMe,eItsMe,eItsMe),//08-0f 
PCK4BITS(eItsMe,eItsMe,eStart,eError,eStart,eError,eError,eError),//10-17 
PCK4BITS(eError,eError,eStart,eError,eError,eError,     3,eError),//18-1f 
PCK4BITS(     3,eError,eError,eError,eStart,eStart,eStart,eStart) //20-27 
};

static const uint32_t EUCJPCharLenTable[] = {2, 2, 2, 3, 1, 0};

const SMModel EUCJPSMModel = {
  {eIdxSft4bits, eSftMsk4bits, eBitSft4bits, eUnitMsk4bits, EUCJP_cls },
   6,
  {eIdxSft4bits, eSftMsk4bits, eBitSft4bits, eUnitMsk4bits, EUCJP_st },
  CHAR_LEN_TABLE(EUCJPCharLenTable),
  "EUC-JP",
};

// sjis

static const uint32_t SJIS_cls [ 256 / 8 ] = {
//PCK4BITS(0,1,1,1,1,1,1,1),  // 00 - 07 
PCK4BITS(1,1,1,1,1,1,1,1),  // 00 - 07 
PCK4BITS(1,1,1,1,1,1,0,0),  // 08 - 0f 
PCK4BITS(1,1,1,1,1,1,1,1),  // 10 - 17 
PCK4BITS(1,1,1,0,1,1,1,1),  // 18 - 1f 
PCK4BITS(1,1,1,1,1,1,1,1),  // 20 - 27 
PCK4BITS(1,1,1,1,1,1,1,1),  // 28 - 2f 
PCK4BITS(1,1,1,1,1,1,1,1),  // 30 - 37 
PCK4BITS(1,1,1,1,1,1,1,1),  // 38 - 3f 
PCK4BITS(2,2,2,2,2,2,2,2),  // 40 - 47 
PCK4BITS(2,2,2,2,2,2,2,2),  // 48 - 4f 
PCK4BITS(2,2,2,2,2,2,2,2),  // 50 - 57 
PCK4BITS(2,2,2,2,2,2,2,2),  // 58 - 5f 
PCK4BITS(2,2,2,2,2,2,2,2),  // 60 - 67 
PCK4BITS(2,2,2,2,2,2,2,2),  // 68 - 6f 
PCK4BITS(2,2,2,2,2,2,2,2),  // 70 - 77 
PCK4BITS(2,2,2,2,2,2,2,1),  // 78 - 7f 
PCK4BITS(3,3,3,3,3,3,3,3),  // 80 - 87 
PCK4BITS(3,3,3,3,3,3,3,3),  // 88 - 8f 
PCK4BITS(3,3,3,3,3,3,3,3),  // 90 - 97 
PCK4BITS(3,3,3,3,3,3,3,3),  // 98 - 9f 
//0xa0 is illegal in sjis encoding, but some pages does 
//contain such byte. We need to be more error forgiven.
PCK4BITS(2,2,2,2,2,2,2,2),  // a0 - a7     
PCK4BITS(2,2,2,2,2,2,2,2),  // a8 - af 
PCK4BITS(2,2,2,2,2,2,2,2),  // b0 - b7 
PCK4BITS(2,2,2,2,2,2,2,2),  // b8 - bf 
PCK4BITS(2,2,2,2,2,2,2,2),  // c0 - c7 
PCK4BITS(2,2,2,2,2,2,2,2),  // c8 - cf 
PCK4BITS(2,2,2,2,2,2,2,2),  // d0 - d7 
PCK4BITS(2,2,2,2,2,2,2,2),  // d8 - df 
PCK4BITS(3,3,3,3,3,3,3,3),  // e0 - e7 
PCK4BITS(3,3,3,3,3,4,4,4),  // e8 - ef 
PCK4BITS(4,4,4,4,4,4,4,4),  // f0 - f7 
PCK4BITS(4,4,4,4,4,0,0,0)   // f8 - ff 
};


static const uint32_t SJIS_st [ 3] = {
PCK4BITS(eError,eStart,eStart,     3,eError,eError,eError,eError),//00-07 
PCK4BITS(eError,eError,eError,eError,eItsMe,eItsMe,eItsMe,eItsMe),//08-0f 
PCK4BITS(eItsMe,eItsMe,eError,eError,eStart,eStart,eStart,eStart) //10-17 
};

static const uint32_t SJISCharLenTable[] = {0, 1, 1, 2, 0, 0};

const SMModel SJISSMModel = {
  {eIdxSft4bits, eSftMsk4bits, eBitSft4bits, eUnitMsk4bits, SJIS_cls },
   6,
  {eIdxSft4bits, eSftMsk4bits, eBitSft4bits, eUnitMsk4bits, SJIS_st },
  CHAR_LEN_TABLE(SJISCharLenTable),
  "Shift_JIS",
};


static const uint32_t UTF8_cls [ 256 / 8 ] = {
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 00 - 07
PCK4BITS( 1, 1, 1, 1, 1, 1, 0, 0),  // 08 - 0f
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 10 - 17
PCK4BITS( 1, 1, 1, 0, 1, 1, 1, 1),  // 18 - 1f
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 20 - 27
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 28 - 2f
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 30 - 37
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 38 - 3f
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 40 - 47
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 48 - 4f
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 50 - 57
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 58 - 5f
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 60 - 67
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 68 - 6f
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 70 - 77
PCK4BITS( 1, 1, 1, 1, 1, 1, 1, 1),  // 78 - 7f
PCK4BITS( 2, 2, 2, 2, 2, 2, 2, 2),  // 80 - 87
PCK4BITS( 2, 2, 2, 2, 2, 2, 2, 2),  // 88 - 8f
PCK4BITS( 3, 3, 3, 3, 3, 3, 3, 3),  // 90 - 97
PCK4BITS( 3, 3, 3, 3, 3, 3, 3, 3),  // 98 - 9f
PCK4BITS( 4, 4, 4, 4, 4, 4, 4, 4),  // a0 - a7
PCK4BITS( 4, 4, 4, 4, 4, 4, 4, 4),  // a8 - af
PCK4BITS( 4, 4, 4, 4, 4, 4, 4, 4),  // b0 - b7
PCK4BITS( 4, 4, 4, 4, 4, 4, 4, 4),  // b8 - bf
PCK4BITS( 0, 0, 5, 5, 5, 5, 5, 5),  // c0 - c7
PCK4BITS( 5, 5, 5, 5, 5, 5, 5, 5),  // c8 - cf
PCK4BITS( 5, 5, 5, 5, 5, 5, 5, 5),  // d0 - d7
PCK4BITS( 5, 5, 5, 5, 5, 5, 5, 5),  // d8 - df
PCK4BITS( 6, 7, 7, 7, 7, 7, 7, 7),  // e0 - e7
PCK4BITS( 7, 7, 7, 7, 7, 8, 7, 7),  // e8 - ef
PCK4BITS( 9,10,10,10,11, 0, 0, 0),  // f0 - f7
PCK4BITS( 0, 0, 0, 0, 0, 0, 0, 0)   // f8 - ff
};


static const uint32_t UTF8_st [ 15] = {
PCK4BITS(eError,eStart,eError,eError,eError,     3,     4,     5),  // 00 - 07
PCK4BITS(     6,     7,     8,     9,eError,eError,eError,eError),  // 08 - 0f
PCK4BITS(eError,eError,eError,eError,eError,eError,eError,eError),  // 10 - 17
PCK4BITS(eItsMe,eItsMe,eItsMe,eItsMe,eItsMe,eItsMe,eItsMe,eItsMe),  // 18 - 1f
PCK4BITS(eItsMe,eItsMe,eItsMe,eItsMe,eError,eError,eStart,eStart),  // 20 - 27
PCK4BITS(eStart,eError,eError,eError,eError,eError,eError,eError),  // 28 - 2f
PCK4BITS(eError,eError,eError,eError,     3,eError,eError,eError),  // 30 - 37
PCK4BITS(eError,eError,eError,eError,eError,eError,     3,     3),  // 38 - 3f
PCK4BITS(     3,eError,eError,eError,eError,eError,eError,eError),  // 40 - 47
PCK4BITS(eError,eError,     3,     3,eError,eError,eError,eError),  // 48 - 4f
PCK4BITS(eError,eError,eError,eError,eError,eError,     5,     5),  // 50 - 57
PCK4BITS(eError,eError,eError,eError,eError,eError,eError,eError),  // 58 - 5f
PCK4BITS(eError,eError,     5,     5,     5,eError,eError,eError),  // 60 - 67
PCK4BITS(eError,eError,eError,eError,eError,eError,     5,eError),  // 68 - 6f
PCK4BITS(eError,eError,eError,eError,eError,eError,eError,eError)   // 70 - 77
};

static const uint32_t UTF8CharLenTable[] = {0, 1, 0, 0, 0, 2, 3, 3, 3, 4, 4, 4};

const SMModel UTF8SMModel = {
  {eIdxSft4bits, eSftMsk4bits, eBitSft4bits, eUnitMsk4bits, UTF8_cls },
   12,
  {eIdxSft4bits, eSftMsk4bits, eBitSft4bits, eUnitMsk4bits, UTF8_st },
  CHAR_LEN_TABLE(UTF8CharLenTable),
  "UTF-8",
};
