/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Intel Corporation.
 * Portions created by Intel Corporation are
 * Copyright (C) 1999 Intel Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Contributor(s): Yueheng Xu <yueheng.xu@intel.com>
 *                 Hoa Nguyen <hoa.nguyen@intel.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */
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
// 04/10/1999 - changed leftbyte. rightbyte to PRUint8 in struct DByte;
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
 (((PRUint8)(a) <= (PRUint8)(b))&&((PRUint8)(b) <= (PRUint8)(c)))
#define UNICHAR_IN_RANGE(a, b, c) \
 (((PRUnichar)(a) <= (PRUnichar)(b))&&((PRUnichar)(b) <= (PRUnichar)(c)))
#define CAST_CHAR_TO_UNICHAR(a) ((PRUnichar)((unsigned char)(a)))
#define CAST_UNICHAR_TO_CHAR(a) ((char)a)

#define IS_ASCII(a) (0==(0xff80 & (a)))
#define IS_GBK_EURO(c) ((char)0x80 == (c))
#define UCS2_EURO  ((PRUnichar) 0x20ac)

#include "nsGBKConvUtil.h"

#endif /* _GBKU_H__ */
