/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef __UCONV_TIL_H__
#define __UCONV_TIL_H__

#include "prcpucfg.h"


/*=====================================*/
#define PACK(h,l)   (int16)(( (h) << 8) | (l))

#if defined(IS_LITTLE_ENDIAN)
#define ShiftCell(sub,len,min,max,minh,minl,maxh,maxl)  \
    PACK(len,sub), PACK(max,min), PACK(minl,minh), PACK(maxl,maxh)
#else
#define ShiftCell(sub,len,min,max,minh,minl,maxh,maxl)  \
    PACK(sub,len), PACK(min, max), PACK(minh,minl), PACK(maxh,maxl)
#endif

typedef enum {
        u1ByteCharset = 0,
        u2BytesCharset,
        uMultibytesCharset,
        u2BytesGRCharset,
        u2BytesGRPrefix8FCharset,
        u2BytesGRPrefix8EA2Charset,
        u2BytesSwapCharset,
        u4BytesCharset,
        u4BytesSwapCharset,
        u2BytesGRPrefix8EA3Charset,
        u2BytesGRPrefix8EA4Charset,
        u2BytesGRPrefix8EA5Charset,
        u2BytesGRPrefix8EA6Charset,
        u2BytesGRPrefix8EA7Charset,
        u1ByteGLCharset,
        uDecomposedHangulCharset,
        uDecomposedHangulGLCharset,
        uJohabHangulCharset,
        uJohabSymbolCharset,
        u4BytesGB18030Charset,
        u2BytesGR128Charset,
        uNumOfCharsetType
} uScanClassID;

typedef enum {
        u1ByteChar                      = 0,
        u2BytesChar,
        u2BytesGRChar,
        u1BytePrefix8EChar,             /* Used by JIS0201 GR in EUC_JP */
        u2BytesUTF8,                    /* Used by UTF8 */
        u3BytesUTF8,                    /* Used by UTF8 */
        uNumOfCharType
} uScanSubClassID;

typedef struct  {
        unsigned char MinHB;
        unsigned char MinLB;
        unsigned char MaxHB;
        unsigned char MaxLB;
} uShiftOut;

typedef struct  {
        unsigned char Min;
        unsigned char Max;
} uShiftIn;

typedef struct  {
        unsigned char   classID;
        unsigned char   reserveLen;
        uShiftIn        shiftin;
        uShiftOut       shiftout;
} uShiftCell;

typedef struct  {
        PRInt16         numOfItem;
        PRInt16         classID; 
        uShiftCell      shiftcell[1];
} uShiftTable;

/*=====================================*/

typedef struct {
        unsigned char min;
        unsigned char max;
} uRange;

/*=====================================*/

typedef PRUint16* uMappingTable; 
 
#endif

