/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsCodingStateMachine.h"

static const uint32_t ISO2022JP_cls [ 256 / 8 ] = {
PCK4BITS(2,0,0,0,0,0,0,0),  // 00 - 07
PCK4BITS(0,0,0,0,0,0,2,2),  // 08 - 0f
PCK4BITS(0,0,0,0,0,0,0,0),  // 10 - 17
PCK4BITS(0,0,0,1,0,0,0,0),  // 18 - 1f
PCK4BITS(0,0,0,0,7,0,0,0),  // 20 - 27
PCK4BITS(3,0,0,0,0,0,0,0),  // 28 - 2f
PCK4BITS(0,0,0,0,0,0,0,0),  // 30 - 37
PCK4BITS(0,0,0,0,0,0,0,0),  // 38 - 3f
PCK4BITS(6,0,4,0,8,0,0,0),  // 40 - 47
PCK4BITS(0,9,5,0,0,0,0,0),  // 48 - 4f
PCK4BITS(0,0,0,0,0,0,0,0),  // 50 - 57
PCK4BITS(0,0,0,0,0,0,0,0),  // 58 - 5f
PCK4BITS(0,0,0,0,0,0,0,0),  // 60 - 67
PCK4BITS(0,0,0,0,0,0,0,0),  // 68 - 6f
PCK4BITS(0,0,0,0,0,0,0,0),  // 70 - 77
PCK4BITS(0,0,0,0,0,0,0,0),  // 78 - 7f
PCK4BITS(2,2,2,2,2,2,2,2),  // 80 - 87
PCK4BITS(2,2,2,2,2,2,2,2),  // 88 - 8f
PCK4BITS(2,2,2,2,2,2,2,2),  // 90 - 97
PCK4BITS(2,2,2,2,2,2,2,2),  // 98 - 9f
PCK4BITS(2,2,2,2,2,2,2,2),  // a0 - a7
PCK4BITS(2,2,2,2,2,2,2,2),  // a8 - af
PCK4BITS(2,2,2,2,2,2,2,2),  // b0 - b7
PCK4BITS(2,2,2,2,2,2,2,2),  // b8 - bf
PCK4BITS(2,2,2,2,2,2,2,2),  // c0 - c7
PCK4BITS(2,2,2,2,2,2,2,2),  // c8 - cf
PCK4BITS(2,2,2,2,2,2,2,2),  // d0 - d7
PCK4BITS(2,2,2,2,2,2,2,2),  // d8 - df
PCK4BITS(2,2,2,2,2,2,2,2),  // e0 - e7
PCK4BITS(2,2,2,2,2,2,2,2),  // e8 - ef
PCK4BITS(2,2,2,2,2,2,2,2),  // f0 - f7
PCK4BITS(2,2,2,2,2,2,2,2)   // f8 - ff
};


static const uint32_t ISO2022JP_st [ 9] = {
PCK4BITS(eStart,     3,eError,eStart,eStart,eStart,eStart,eStart),//00-07
PCK4BITS(eStart,eStart,eError,eError,eError,eError,eError,eError),//08-0f
PCK4BITS(eError,eError,eError,eError,eItsMe,eItsMe,eItsMe,eItsMe),//10-17
PCK4BITS(eItsMe,eItsMe,eItsMe,eItsMe,eItsMe,eItsMe,eError,eError),//18-1f
PCK4BITS(eError,     5,eError,eError,eError,     4,eError,eError),//20-27
PCK4BITS(eError,eError,eError,     6,eItsMe,eError,eItsMe,eError),//28-2f
PCK4BITS(eError,eError,eError,eError,eError,eError,eItsMe,eItsMe),//30-37
PCK4BITS(eError,eError,eError,eItsMe,eError,eError,eError,eError),//38-3f
PCK4BITS(eError,eError,eError,eError,eItsMe,eError,eStart,eStart) //40-47
};

static const uint32_t ISO2022JPCharLenTable[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

const SMModel ISO2022JPSMModel = {
  {eIdxSft4bits, eSftMsk4bits, eBitSft4bits, eUnitMsk4bits, ISO2022JP_cls },
  10,
  {eIdxSft4bits, eSftMsk4bits, eBitSft4bits, eUnitMsk4bits, ISO2022JP_st },
  CHAR_LEN_TABLE(ISO2022JPCharLenTable),
  "ISO-2022-JP",
};
