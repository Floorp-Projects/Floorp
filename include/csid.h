/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


/*	csid.h	*/

#ifndef _CSID_H_
#define _CSID_H_

/* Codeset type */
#define SINGLEBYTE   0x0000 /* 0000 0000 0000 0000 =    0 */
#define MULTIBYTE    0x0100 /* 0000 0001 0000 0000 =  256 */
#define STATEFUL     0x0200 /* 0000 0010 0000 0000 =  512 */
#define WIDECHAR     0x0300 /* 0000 0011 0000 0000 =  768 */
#define CODESET_MASK 0x0F00 /* 0000 1111 0000 0000 = 3840 */

/*
 * Check for double byte encodings
 * (should distinguish 2 byte from true multibyte)
 */
#define IS_16BIT_ENCODING(x) (((x)&CODESET_MASK) == MULTIBYTE)

/* line-break on spaces */
#define CS_SPACE   0x0400 /* 0000 0100 0000 0000 = 1024 */

/* Auto Detect Mode */
#define CS_AUTO    0x0800 /* 0000 1000 0000 0000 = 2048 */


/* Code Set IDs */
/* CS_DEFAULT: used if no charset param in header */
/* CS_UNKNOWN: used for unrecognized charset */

                    /* type                  id   */
#define CS_DEFAULT    (SINGLEBYTE         |   0) /*    0 */
#define CS_ASCII      (SINGLEBYTE         |   1) /*    1 */
#define CS_LATIN1     (SINGLEBYTE         |   2) /*    2 */
#define CS_JIS        (STATEFUL           |   3) /*  515 */
#define CS_SJIS       (MULTIBYTE          |   4) /*  260 */
#define CS_EUCJP      (MULTIBYTE          |   5) /*  261 */

#define CS_JIS_AUTO   (CS_AUTO|STATEFUL   |   3) /* 2563 */
#define CS_SJIS_AUTO  (CS_AUTO|MULTIBYTE  |   4) /* 2308 */
#define CS_EUCJP_AUTO (CS_AUTO|MULTIBYTE  |   5) /* 2309 */

#define CS_MAC_ROMAN  (SINGLEBYTE         |   6) /*    6 */
#define CS_BIG5       (MULTIBYTE          |   7) /*  263 */
#define CS_GB_8BIT    (MULTIBYTE          |   8) /*  264 */
#define CS_CNS_8BIT   (MULTIBYTE          |   9) /*  265 */
#define CS_LATIN2     (SINGLEBYTE         |  10) /*   10 */
#define CS_MAC_CE     (SINGLEBYTE         |  11) /*   11 */

#define CS_KSC_8BIT   (MULTIBYTE|CS_SPACE |  12) /* 1292 */
#define CS_KSC_8BIT_AUTO   (CS_AUTO | MULTIBYTE|CS_SPACE |  12)

#define CS_2022_KR    (STATEFUL           |  13) /*  525 */
#define CS_8859_3     (SINGLEBYTE         |  14) /*   14 */
#define CS_8859_4     (SINGLEBYTE         |  15) /*   15 */
#define CS_8859_5     (SINGLEBYTE         |  16) /*   16 ISO Cyrillic */
#define CS_8859_6     (SINGLEBYTE         |  17) /*   17 ISO Arabic */
#define CS_8859_7     (SINGLEBYTE         |  18) /*   18 ISO Greek */
#define CS_8859_8     (SINGLEBYTE         |  19) /*   19 ISO Hebrew */
#define CS_8859_9     (SINGLEBYTE         |  20) /*   20 */
#define CS_SYMBOL     (SINGLEBYTE         |  21) /*   21 */
#define CS_DINGBATS   (SINGLEBYTE         |  22) /*   22 */
#define CS_DECTECH    (SINGLEBYTE         |  23) /*   23 */
#define CS_CNS11643_1 (MULTIBYTE          |  24) /*  280 */
#define CS_CNS11643_2 (MULTIBYTE          |  25) /*  281 */
#define CS_JISX0208   (MULTIBYTE          |  26) /*  282 */
#define CS_JISX0201   (SINGLEBYTE         |  27) /*   27 */
#define CS_KSC5601    (MULTIBYTE          |  28) /*  284 */
#define CS_TIS620     (SINGLEBYTE         |  29) /*   29 */
#define CS_JISX0212   (MULTIBYTE          |  30) /*  286 */
#define CS_GB2312     (MULTIBYTE          |  31) /*  287 */
#define CS_UCS2       (WIDECHAR           |  32) /*  810 */
#define CS_UCS4       (WIDECHAR           |  33) /*  811 */
#define CS_UTF8       (MULTIBYTE          |  34) /*  290 */
#define CS_UTF7       (STATEFUL           |  35) /*   35 */
#define CS_NPC        (MULTIBYTE          |  36) /*  292 */
#define CS_X_BIG5     (MULTIBYTE          |  37) /*  293 */
#define CS_USRDEF2    (SINGLEBYTE         |  38) /*   38 */

#define CS_KOI8_R     (SINGLEBYTE         |  39) /*   39 */
#define CS_MAC_CYRILLIC     (SINGLEBYTE   |  40) /*   40 */
#define CS_CP_1251    (SINGLEBYTE         |  41) /*   41 CP1251 Windows Cyrillic */
#define CS_MAC_GREEK  (SINGLEBYTE         |  42) /*   42 */
/* CS_CP_1253 should be deleted, we should use CS_8859_7 instead */
#define CS_CP_1253    (SINGLEBYTE         |  43) /*   43 CP1253 Windows Greek */
#define CS_CP_1250    (SINGLEBYTE         |  44) /*   44 CP1250 Windows C. Europe */
/* CS_CP_1254 should be deleted, we should use CS_8859_9 instead */
#define CS_CP_1254    (SINGLEBYTE         |  45) /*   45 CP1254 Windows Turkish */
#define CS_MAC_TURKISH (SINGLEBYTE        |  46) /*   46 */
#define CS_GB2312_11  (MULTIBYTE          |  47) /*  303 */
#define CS_JISX0208_11 (MULTIBYTE         |  48) /*  304 */
#define CS_KSC5601_11 (MULTIBYTE          |  49) /*  305 */
#define CS_CNS11643_1110 (MULTIBYTE       |  50) /*  306 */
#define CS_UCS2_SWAP    (WIDECHAR         |  51) /*  819 */
#define CS_IMAP4_UTF7       (STATEFUL     |  52) /*  564 */
#define CS_CP_850     (SINGLEBYTE         |  53) /*   53  PC Latin 1 */
#define CS_CP_852     (SINGLEBYTE         |  54) /*   54  PC Latin 2 */
#define CS_CP_855     (SINGLEBYTE         |  55) /*   55  PC Cyrillic */
#define CS_CP_857     (SINGLEBYTE         |  56) /*   56  PC Turkish */
#define CS_CP_862     (SINGLEBYTE         |  57) /*   57  PC Hebrew */
#define CS_CP_864     (SINGLEBYTE         |  58) /*   58  PC Arabic */
#define CS_CP_866     (SINGLEBYTE         |  59) /*   59  PC Russian */
#define CS_CP_874     (SINGLEBYTE         |  60) /*   60  PC Thai    */
#define CS_CP_1257    (SINGLEBYTE         |  61) /*   61  Windows Baltic */
#define CS_CP_1258    (SINGLEBYTE         |  62) /*   62  Windows Vietnamese */
#define INTL_CHAR_SET_MAX                    63  /* must be highest + 1 */


#define CS_USER_DEFINED_ENCODING (SINGLEBYTE | 254) /* 254 */
#define CS_UNKNOWN    (SINGLEBYTE         | 255) /* 255 */

#define IS_UTF8_CSID(x) (((x)&0xFF)== (CS_UTF8&0xFF))
#define IS_UNICODE_CSID(x) \
			(   (((x)&0xFF)== (CS_UCS2&0xFF)) \
			 || (((x)&0xFF)== (CS_UTF8&0xFF)) \
			 || (((x)&0xFF)== (CS_UTF7&0xFF)) )

/* The trigger is passing the parameter to 
   PA_FetchParamValue() to satisfy its signature */
#ifdef XP_MAC
#define CS_FE_ASCII CS_MAC_ROMAN
#else
#define CS_FE_ASCII CS_LATIN1
#endif


/* Codeset # sorted by number */
#if 0

0    CS_DEFAULT     0
1    CS_ASCII     1
2    CS_LATIN1     2
6    CS_MAC_ROMAN     6
10   CS_LATIN2    10
11   CS_MAC_CE    11
14   CS_8859_3    14
15   CS_8859_4    15
16   CS_8859_5    16
17   CS_8859_6    17
18   CS_8859_7    18 
19   CS_8859_8    19
20   CS_8859_9    20
21   CS_SYMBOL    21
22   CS_DINGBATS    22
23   CS_DECTECH    23
27   CS_JISX0201    27
29   CS_TIS620    29
35   CS_UTF7    35
38   CS_USRDEF2    38
39   CS_KOI8_R    39
40   CS_MAC_CYRILLIC    40
41   CS_CP_1251    41
42   CS_MAC_GREEK    42
43   CS_CP_1253    43
44   CS_CP_1250    44
45   CS_CP_1254    45
46   CS_MAC_TURKISH    46
53   CS_CP_850  53
54   CS_CP_852  54
55   CS_CP_855  55
56   CS_CP_857  56
57   CS_CP_862  57
58   CS_CP_864  58
59   CS_CP_866  59
59   CS_CP_874  60
60   CS_CP_1257   61
61   CS_CP_1258   62
62   INTL_CHAR_SET_MAX  63
254  CS_USER_DEFINED_ENCODING  254
255  CS_UNKNOWN  255
260  CS_SJIS   260
261  CS_EUCJP   261
263  CS_BIG5   263
264  CS_GB_8BIT   264
265  CS_CNS_8BIT   265
280  CS_CNS11643_1   280
281  CS_CNS11643_2   281
282  CS_JISX0208   282
284  CS_KSC5601   284
286  CS_JISX0212   286
287  CS_GB2312   287
290  CS_UTF8   290
292  CS_NPC   292
293  CS_X_BIG5   293
303  CS_GB2312_11   303
304  CS_JISX0208_11   304
305  CS_KSC5601_11   305
306  CS_CNS11643_1110   306
515  CS_JIS   515
525  CS_2022_KR   525
564  CS_IMAP4_UTF7 564
810  CS_UCS2   810
811  CS_UCS4   811
819  CS_UCS2_SWAP   819
1292 CS_KSC_8BIT  1292
2308 CS_SJIS_AUTO  2308
2309 CS_EUCJP_AUTO  2309
2563 CS_JIS_AUTO  2563

#endif

#endif /* _CSID_H_ */
