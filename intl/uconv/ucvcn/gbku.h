/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// =======================================================================
// Original Author: Yueheng Xu
// email: yueheng.xu@intel.com
// phone: (503)264-2248
// Intel Corporation, Oregon, USA
// Last Update: September 7, 1999
// Revision History: 
// 09/07/1999 - initial version.
// 09/28/1999 - changed leftbyte and rightbyte from char to unsigned char 
//              in struct DByte
// 04/10/1999 - changed leftbyte. rightbyte to uint8_t in struct DByte;
//              added table UnicodeToGBKTable[0x5200]
//            
// 05/16/2000 - added gUnicodeToGBKTableInitialized flag for optimization
// ======================================================================================
// Table GBKToUnicode[] maps the GBK code to its unicode.
// The mapping data of this GBK table is obtained from 
// ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/WINDOWS/CP936.TXT
// Frank Tang of Netscape wrote the original perl tool to re-align the 
// mapping data into an 8-item per line format ( i.e. file cp936map.txt ).
//
// The valid GBK charset range: left byte is [0x81, 0xfe], right byte are
// [0x40, 0x7e] and [0x80, 0xfe]. But for the convenience of index 
// calculation, the table here has a single consecutive range of 
// [0x40, 0xfe] for the right byte. Those invalid chars whose right byte 
// is 0x7f will be mapped to undefined unicode 0xFFFF.
//
// 
// Table UnicodeToGBK[] maps the unicode to GBK code. To reduce memory usage, we
// only do Unicode to GBK table mapping for unicode between 0x4E00 and 0xA000; 
// Others let converter to do search from table GBKToUnicode[]. If we want further
// trade memory for performance, we can let more unicode to do table mapping to get
// its GBK instead of searching table GBKToUnicode[]. 
#ifndef _GBKU_H__
#define _GBKU_H__


#define  UCS2_NO_MAPPING ((PRUnichar) 0xfffd)
#define UINT8_IN_RANGE(a, b, c) \
 (((uint8_t)(a) <= (uint8_t)(b))&&((uint8_t)(b) <= (uint8_t)(c)))
#define UNICHAR_IN_RANGE(a, b, c) \
 (((PRUnichar)(a) <= (PRUnichar)(b))&&((PRUnichar)(b) <= (PRUnichar)(c)))
#define CAST_CHAR_TO_UNICHAR(a) ((PRUnichar)((unsigned char)(a)))
#define CAST_UNICHAR_TO_CHAR(a) ((char)a)

#define IS_ASCII(a) (0==(0xff80 & (a)))
#define IS_GBK_EURO(c) ((char)0x80 == (c))
#define UCS2_EURO  ((PRUnichar) 0x20ac)

#include "nsGBKConvUtil.h"

#endif /* _GBKU_H__ */
