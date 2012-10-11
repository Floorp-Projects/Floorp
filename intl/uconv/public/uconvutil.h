/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __UCONV_TIL_H__
#define __UCONV_TIL_H__

#include "prcpucfg.h"


/*=====================================*/
#define PACK(h,l)   (int16_t)(( (h) << 8) | (l))

#if defined(IS_LITTLE_ENDIAN)
#define ShiftInCell(sub,len,min,max)  \
    PACK(len,sub), PACK(max,min)
#define ShiftOutCell(sub,len,minh,minl,maxh,maxl)  \
    PACK(len,sub), PACK(minl,minh), PACK(maxl,maxh)
#else
#define ShiftInCell(sub,len,min,max)  \
    PACK(sub,len), PACK(min, max)
#define ShiftOutCell(sub,len,minh,minl,maxh,maxl)  \
    PACK(sub,len), PACK(minh,minl), PACK(maxh,maxl)
#endif

typedef enum {
        u1ByteCharset = 0,
        u2BytesCharset,
        u2BytesGRCharset,
        u2BytesGRPrefix8FCharset,
        u2BytesGRPrefix8EA2Charset,
        u2BytesGRPrefix8EA3Charset,
        u2BytesGRPrefix8EA4Charset,
        u2BytesGRPrefix8EA5Charset,
        u2BytesGRPrefix8EA6Charset,
        u2BytesGRPrefix8EA7Charset,
        uDecomposedHangulCharset,
        uJohabHangulCharset,
        uJohabSymbolCharset,
        u4BytesGB18030Charset,
        u2BytesGR128Charset,
        uMultibytesCharset,
        uNumOfCharsetType = uMultibytesCharset
} uScanClassID;

typedef enum {
        u1ByteChar                      = 0,
        u2BytesChar,
        u2BytesGRChar,
        u1BytePrefix8EChar,             /* Used by JIS0201 GR in EUC_JP */
        uNumOfCharType
} uScanSubClassID;

typedef struct  {
        unsigned char   classID;
        unsigned char   reserveLen;
        unsigned char   shiftin_Min;
        unsigned char   shiftin_Max;
} uShiftInCell;

typedef struct  {
        int16_t         numOfItem;
        uShiftInCell    shiftcell[1];
} uShiftInTableMutable;

typedef const uShiftInTableMutable uShiftInTable;

typedef struct  {
        unsigned char   classID;
        unsigned char   reserveLen;
        unsigned char   shiftout_MinHB;
        unsigned char   shiftout_MinLB;
        unsigned char   shiftout_MaxHB;
        unsigned char   shiftout_MaxLB;
} uShiftOutCell;

typedef struct  {
        int16_t         numOfItem;
        uShiftOutCell   shiftcell[1];
} uShiftOutTableMutable;

typedef const uShiftOutTableMutable uShiftOutTable;


/*=====================================*/

typedef struct {
        unsigned char min;
        unsigned char max;
} uRange;

/*=====================================*/

typedef uint16_t* uMappingTableMutable; 
typedef const uint16_t uMappingTable; 
 
#endif

