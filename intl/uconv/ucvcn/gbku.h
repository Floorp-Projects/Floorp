// =======================================================================
// Original Author: Yueheng Xu
// email: yueheng.xu@intel.com
// phone: (503)264-2248
// Intel Corporation, Oregon, USA
// Last Update: September 7, 1999
// Revision History: 
// 09/07/1999 - initial version.
// =======================================================================
// This table maps the GBK code to its unicode.
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

#ifdef _GBKU_TABLE_

#define GB_UNDEFINED 0xFFFF
#define MAX_GBK_LENGTH	  24066   /* (0xfe-0x80)*(0xfe-0x3f) */

typedef struct
{
    char leftbyte;
    char rightbyte;

} DByte;
extern PRUnichar GBKToUnicodeTable[MAX_GBK_LENGTH];


#else

#define _GBKU_TABLE_


#define GB_UNDEFINED 0xFFFF
#define MAX_GBK_LENGTH	  24066   /* (0xfe-0x80)*(0xfe-0x3f) */

typedef struct
{
    char leftbyte;
    char rightbyte;

} DByte;



PRUnichar GBKToUnicodeTable[MAX_GBK_LENGTH] =
{
#include "cp936map.h"
};



#endif /* ifdef _GBKU_TABLE_ */

