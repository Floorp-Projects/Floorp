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
/*      vietnam.c       */
 
#include "intlpriv.h"
 
extern int MK_OUT_OF_MEMORY;
 
 
/**************************< vietnam.c >*******************************/
/*** Purpose: Please put all conversion routines that related to    ***/
/*** -------  the vietnamese character sets here for easy maintenance**/
/*** Modifications                                                  ***/
/*** 08/17/98: Vuong: - Add support for CP1258                      ***/
/***                  - Reduce similar functions into 1 function    ***/
/***                  - Cover the unsupported buffer for VNI & CP1258**/
/**********************************************************************/
 
 
/**********************************************************************/
/*** The following section handles conversion from VIQR to VISCII   ***/
/*** Implementer:  Vuong Nguyen (vlangsj@pluto.ftos.net)            ***/
/**********************************************************************/
/*** VISCII - VIQR - VISCII - VIQR - VISCII - VIQR - VISCII - VIQR  ***/
/**********************************************************************/
 
       /***************************************************/
       /*** For Mozilla, I disable the state feature of ***/
       /*** VIQR  (\V \M \L) by not defining TRUE_VIQR  ***/
       /***************************************************/
 
#ifdef TRUE_VIQR
PRIVATE unsigned char VIQRStateName[6] =
              /*----0----1----2----3----4----5----*/
              {    'v', 'V', 'm', 'M', 'l', 'L'   };
#endif
 
PRIVATE unsigned char DefaultVIQR[10] =
              /*----0----1----2----3----4----5----6----7----8----9---*/
              /*----'----`----?----~----.----(----^----+----d----\---*/
              {    39,  96,  63, 126,  46,  40,  94,  43, 100,  92   };
 
 
PRIVATE unsigned char HotChars[66] =
     /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y----*/
     {    97, 229, 226, 101, 234, 105, 111, 244, 189, 117, 223, 121,
     /*----a'---a`---a?---a~---a.---e'---e`---e?---e~---e.--------*/
         225, 224, 228, 227, 213, 233, 232, 235, 168, 169,
     /*----o'---o`---o?---o~---o.---u'---u`---u?---u~---u.---d----*/
         243, 242, 246, 245, 247, 250, 249, 252, 251, 248, 100,
     /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y----*/
          65, 197, 194,  69, 202,  73,  79, 212, 180,  85, 191,  89,
     /*----A'---A`---A?---A~---A.---E'---E`---E?---E~---E.--------*/
         193, 192, 196, 195, 128, 201, 200, 203, 136, 137,
     /*----O'---O`---O?---O~---O.---U'---U`---U?---U~---U.---D----*/
         211, 210, 153, 160, 154, 218, 217, 156, 157, 158,  68    };
 
PRIVATE unsigned char CombiToneChar[5][24] =
     /***************************  Acute ' ******************************/
     { /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   225, 161, 164, 233, 170, 237, 243, 175, 190, 250, 209, 253,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           193, 129, 132, 201, 138, 205, 211, 143, 149, 218, 186, 221   },
     /***************************  Grave ` ******************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   224, 162, 165, 232, 171, 236, 242, 176, 182, 249, 215, 207,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           192, 130, 133, 200, 139, 204, 210, 144, 150, 217, 187, 159   },
     /***************************  Hook Above ? *************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   228, 198, 166, 235, 172, 239, 246, 177, 183, 252, 216, 214,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           196,   2, 134, 203, 140, 155, 153, 145, 151, 156, 188,  20   },
     /***************************  Tilde ~ ******************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   227, 199, 231, 168, 173, 238, 245, 178, 222, 251, 230,  219,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           195,   5,   6, 136, 141, 206, 160, 146, 179, 157, 255,  25   },
     /***************************  Dot Below . **************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   213, 163, 167, 169, 174, 184, 247, 181, 254, 248, 241,  220,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           128, 131, 135, 137, 142, 152, 154, 147, 148, 158, 185,  30   }
     };
 
PRIVATE unsigned char CombiModiChar[3][66] =
     /***************************  Breve ( ******************************/
     { /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   229,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----a'---a`---a?---a~---a.---e'---e`---e?---e~---e.--------*/
           161, 162, 198, 199, 163,   0,   0,   0,   0,   0,
       /*----o'---o`---o?---o~---o.---u'---u`---u?---u~---u.---d----*/
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           197,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----A'---A`---A?---A~---A.---E'---E`---E?---E~---E.--------*/
           129, 130,   2,   5, 131,   0,   0,   0,   0,   0,
       /*----O'---O`---O?---O~---O.---U'---U`---U?---U~---U.---D----*/
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0    },
     /***************************  Circumflex ^ *************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {   226,   0,   0, 234,   0,   0, 244,   0,   0,   0,   0,   0,
       /*----a'---a`---a?---a~---a.---e'---e`---e?---e~---e.--------*/
           164, 165, 166, 231, 167, 170, 171, 172, 173, 174,
       /*----o'---o`---o?---o~---o.---u'---u`---u?---u~---u.---d----*/
           175, 176, 177, 178, 181,   0,   0,   0,   0,   0,   0,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
           194,   0,   0, 202,   0,   0, 212,   0,   0,   0,   0,   0,
       /*----A'---A`---A?---A~---A.---E'---E`---E?---E~---E.--------*/
           183, 133, 134,   6, 135, 138, 139, 140, 141, 142,
       /*----O'---O`---O?---O~---O.---U'---U`---U?---U~---U.---D----*/
           143, 144, 145, 146, 147,   0,   0,   0,   0,   0,   0    },
     /***************************  Horn + *******************************/
       /*----a----a(---a^---e----e^---i----o----o^---o+---u----u+---y---*/
       {     0,   0,   0,   0,   0,   0, 189,   0,   0, 223,   0,   0,
       /*----a'---a`---a?---a~---a.---e'---e`---e?---e~---e.--------*/
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----o'---o`---o?---o~---o.---u'---u`---u?---u~---u.---d----*/
           190, 182, 183, 222, 254, 209, 215, 216, 230, 241,   0,
       /*----A----A(---A^---E----E^---I----O----O^---O+---U----U+---Y---*/
             0,   0,   0,   0,   0,   0, 180,   0,   0, 191,   0,   0,
       /*----A'---A`---A?---A~---A.---E'---E`---E?---E~---E.--------*/
             0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
       /*----O'---O`---O?---O~---O.---U'---U`---U?---U~---U.---D----*/
           149, 150, 151, 179, 148, 186, 187, 188, 255, 185,   0    }
     };
 
PRIVATE uint16 VIS_VIQ[128] =          /*** 128 to 255 ***/
     { 0x1041,0x2141,0x2241,0x3041,0x4141,0x4241,0x4441,0x5041,
       0x0845,0x1045,0x4145,0x4245,0x4445,0x4845,0x5045,0x414f,
       0x424f,0x444f,0x484f,0x504f,0x904f,0x814f,0x824f,0x844f,
       0x1049,0x044f,0x104f,0x0449,0x0455,0x0855,0x1055,0x0259,
       0x084f,0x2161,0x2261,0x3061,0x4161,0x4261,0x4461,0x5061,
       0x0865,0x1065,0x4165,0x4265,0x4465,0x4865,0x5065,0x416f,
       0x426f,0x446f,0x486f,0x884f,0x804f,0x506f,0x826f,0x846f,
       0x1069,0x9055,0x8155,0x8255,0x8455,0x806f,0x816f,0x8055,
       0x0241,0x0141,0x4041,0x0841,0x0441,0x2041,0x2461,0x2861,
       0x0245,0x0145,0x4045,0x0445,0x0249,0x0149,0x0849,0x0279,
       0x0000,0x8175,0x024f,0x014f,0x404f,0x1061,0x0479,0x8275,
       0x8475,0x0255,0x0155,0x0879,0x1079,0x0159,0x886f,0x8075,
       0x0261,0x0161,0x4061,0x0861,0x0461,0x2061,0x8875,0x4861,
       0x0265,0x0165,0x4065,0x0465,0x0269,0x0169,0x0869,0x0469,
       0x0000,0x9075,0x026f,0x016f,0x406f,0x086f,0x046f,0x106f,
       0x1075,0x0275,0x0175,0x0875,0x0475,0x0179,0x906f,0x8855 };
 
PRIVATE unsigned char VNI_VIS[191] = /*** Mapping from 65 .. 255 ***/
     { 0xff,0x42,0x43,0x44,0xff,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,
       0x4d,0x4e,0xff,0x50,0x51,0x52,0x53,0x54,0xff,0x56,0x57,0x58,
       0xff,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0xff,0x62,0x63,0x64,
       0xff,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0xff,0x70,
       0x71,0x72,0x73,0x74,0xff,0x76,0x77,0x78,0xff,0x7a,0x7b,0x7c,
       0x7d,0x7e,0x7f,0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,
       0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,0x90,0x91,0x92,0x93,0x94,
       0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,0xa0,
       0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,
       0xad,0xae,0xaf,0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,
       0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,0xc0,0xc1,0xc2,0xc3,0xc4,
       0xc5,0x9b,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0x1e,0xcf,0xd0,
       0xd0,0x98,0xce,0xff,0xd5,0xff,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,
       0xdd,0xde,0xdf,0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xef,0xe7,0xe8,
       0xe9,0xea,0xeb,0xec,0xed,0xdc,0xef,0xf0,0xf0,0xb8,0xee,0xff,
       0xf5,0xff,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xdc };
 
PRIVATE uint16 VNIextended_VIS[118] =
     { 0xc085,0xc184,0xc2c2,0xc306,0xc487,0xc586,0xc882,0xc981,
       0xcac5,0xcb83,0xcf80,0xd5c3,0xd8c0,0xd9c1,0xda02,0xdbc4,
       0xdc05,0xc08b,0xc18a,0xc2ca,0xc38d,0xc48e,0xc58c,0xcf89,
       0xd588,0xd8c8,0xd9c9,0xdbcb,0xc090,0xc18f,0xc2d4,0xc392,
       0xc493,0xc591,0xcf9a,0xd5a0,0xd8d2,0xd9d3,0xdb99,0xcf9e,
       0xd59d,0xd8d9,0xd9da,0xdb9c,0xcf1e,0xd519,0xd89f,0xd9dd,
       0xdb14,0xe0a5,0xe1a4,0xe2e2,0xe3e7,0xe4a7,0xe5a6,0xe8a2,
       0xe9a1,0xeae5,0xeba3,0xefd5,0xf5e3,0xf8e0,0xf9e1,0xfac6,
       0xfbe4,0xfcc7,0xe0ab,0xe1aa,0xe2ea,0xe3ad,0xe4ae,0xe5ac,
       0xefa9,0xf5a8,0xf8e8,0xf9e9,0xfbeb,0xe0b0,0xe1af,0xe2f4,
       0xe3b2,0xe4b5,0xe5b1,0xeff7,0xf5f5,0xf8f2,0xf9f3,0xfbf6,
       0xeff8,0xf5fb,0xf8f9,0xf9fa,0xfbfc,0xefdc,0xf5db,0xf8cf,
       0xf9fd,0xfbd6,0xcf94,0xd5b3,0xd896,0xd995,0xdb97,0xcfb9,
       0xd5ff,0xd8bb,0xd9ba,0xdbbc,0xeffe,0xf5de,0xf8b6,0xf9be,
       0xfbb7,0xeff1,0xf5e6,0xf8d7,0xf9d1,0xfbd8 };
 
PRIVATE uint16 VIS_VNI[128] =          /*** 128 to 255 ***/
     { 0xcf41,0xc941,0xc841,0xcb41,0xc141,0xc041,0xc541,0xc441,
       0xd545,0xcf45,0xc145,0xc045,0xc545,0xc345,0xc445,0xc14f,
       0xc04f,0xc54f,0xc34f,0xc44f,0xcfd4,0xd9d4,0xd8d4,0xdbd4,
       0x00d2,0xdb4f,0xcf4f,0x00c6,0xdb55,0xd555,0xcf55,0xd859,
       0xd54f,0xe961,0xe861,0xeb61,0xe161,0xe061,0xe561,0xe461,
       0xf565,0xef65,0xe165,0xe065,0xe565,0xe365,0xe465,0xe16f,
       0xe06f,0xe56f,0xe36f,0xd5d4,0x00d4,0xe46f,0xf8f4,0xfbf4,
       0x00f2,0xcfd6,0xd9d6,0xd8d6,0xdbd6,0x00f4,0xf9f4,0x00d6,
       0xd841,0xd941,0xc241,0xd541,0xdb41,0xca41,0xfa61,0xfc61,
       0xd845,0xd945,0xc245,0xdb45,0x00cc,0x00cd,0x00d3,0xf879,
       0x00d1,0xf9f6,0xd84f,0xd94f,0xc24f,0xef61,0xfb79,0xf8f6,
       0xfbf6,0xd855,0xd955,0xf579,0x00ee,0xd959,0xf5f4,0x00f6,
       0xf861,0xf961,0xe261,0xf561,0xfb61,0xea61,0xf5f6,0xe361,
       0xf865,0xf965,0xe265,0xfb65,0x00ec,0x00ed,0x00f3,0x00e6,
       0x00f1,0xeff6,0xf86f,0xf96f,0xe26f,0xf56f,0xfb6f,0xef6f,
       0xef75,0xf875,0xf975,0xf575,0xfb75,0xf979,0xeff4,0xd5d6 };
 
PRIVATE unsigned char CP1258_VIS[191] = /*** Mapping from 65 .. 255 ***/
     { 0xff,0x42,0x43,0x44,0xff,0x46,0x47,0x48,0xff,0x4a,0x4b,0x4c,
       0x4d,0x4e,0xff,0x50,0x51,0x52,0x53,0x54,0xff,0x56,0x57,0x58,
       0xff,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,0x60,0xff,0x62,0x63,0x64,
       0xff,0x66,0x67,0x68,0xff,0x6a,0x6b,0x6c,0x6d,0x6e,0xff,0x70,
       0x71,0x72,0x73,0x74,0xff,0x76,0x77,0x78,0xff,0x7a,0x7b,0x7c,
       0x7d,0x7e,0x7f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,
       0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,
       0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,
       0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,
       0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,
       0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0x5f,0xc0,0xc1,0xff,0xff,0x5f,
       0x5f,0x5f,0x5f,0xc8,0xc9,0xff,0x5f,0x5f,0xcd,0x5f,0x5f,0xd0,
       0x5f,0x5f,0xd3,0xff,0xff,0x5f,0x5f,0x5f,0xd9,0xda,0x5f,0x5f,
       0xff,0x5f,0x5f,0xe0,0xe1,0xff,0xff,0x5f,0x5f,0x5f,0x5f,0xe8,
       0xe9,0xff,0x5f,0x5f,0xed,0x5f,0x5f,0xf0,0x5f,0x5f,0xf3,0xff,
       0xff,0x5f,0x5f,0x5f,0xf9,0xfa,0x5f,0x5f,0xff,0x5f,0x5f };
 
PRIVATE uint16 CP1258extended_VIS[104] =
     { 0x49cc,0x4fd2,0x599f,0x69ec,0x6ff2,0x79cf,0xc285,0xc382,
       0xca8b,0xd490,0xd596,0xddbb,0xe2a5,0xe3a2,0xeaab,0xf4b0,
       0xf5b6,0xfdd7,0x41c4,0x45cb,0x499b,0x4f99,0x559c,0x5914,
       0x61e4,0x65eb,0x69ef,0x6ff6,0x75fc,0x79d6,0xc286,0xc302,
       0xca8c,0xd491,0xd597,0xddbc,0xe2a6,0xe3c6,0xeaac,0xf4b1,
       0xf5b7,0xfdd8,0x41c3,0x4588,0x49ce,0x4fa0,0x559d,0x5919,
       0x61e3,0x65a8,0x69ee,0x6ff5,0x75fb,0x79db,0xc206,0xc305,
       0xca8d,0xd492,0xd5b3,0xddff,0xe2e7,0xe3c7,0xeaad,0xf4b2,
       0xf5de,0xfde6,0x59dd,0x79fd,0xc284,0xc381,0xca8a,0xd48f,
       0xd595,0xddba,0xe2a4,0xe3a1,0xeaaa,0xf4af,0xf5be,0xfdd1,
       0x4180,0x4589,0x4998,0x4f9a,0x559e,0x591e,0x61d5,0x65a9,
       0x69b8,0x6ff7,0x75f8,0x79dc,0xc287,0xc383,0xca8e,0xd493,
       0xd594,0xddb9,0xe2a7,0xe3a3,0xeaae,0xf4b5,0xf5fe,0xfdf1 };
 
PRIVATE uint16 VIS_CP1258[128] =          /*** 128 to 255 ***/
     { 0xf241,0xecc3,0xccc3,0xf2c3,0xecc2,0xccc2,0xd2c2,0xf2c2,
       0xde45,0xf245,0xecca,0xccca,0xd2ca,0xdeca,0xf2ca,0xecd4,
       0xccd4,0xd2d4,0xded4,0xf2d4,0xf2d5,0xecd5,0xccd5,0xd2d5,
       0xf249,0xd24f,0xf24f,0xd249,0xd255,0xde55,0xf255,0xcc59,
       0xde4f,0xece3,0xcce3,0xf2e3,0xece2,0xcce2,0xd2e2,0xf2e2,
       0xde65,0xf265,0xecea,0xccea,0xd2ea,0xdeea,0xf2ea,0xecf4,
       0xccf4,0xd2f4,0xdef4,0xded5,0x00d5,0xf2f4,0xccf5,0xd2f5,
       0xf269,0xf2dd,0xecdd,0xccdd,0xd2dd,0x00f5,0xecf5,0x00dd,
       0x00c0,0x00c1,0x00c2,0xde41,0xd241,0x00c3,0xd2e3,0xdee3,
       0x00c8,0x00c9,0x00ca,0xd245,0xcc49,0x00cd,0xde49,0xcc79,
       0x00d0,0xecfd,0xcc4f,0x00d3,0x00d4,0xf261,0xd279,0xccfd,
       0xd2fd,0x00d9,0x00da,0xde79,0xf279,0xec59,0xdef5,0x00fd,
       0x00e0,0x00e1,0x00e2,0xde61,0xd261,0x00e3,0xdefd,0xdee2,
       0x00e8,0x00e9,0x00ea,0xd265,0xcc69,0x00ed,0xde69,0xd269,
       0x00f0,0xf2fd,0xcc6f,0x00f3,0x00f4,0xde6f,0xd26f,0xf26f,
       0xf275,0x00f9,0x00fa,0xde75,0xd275,0xec79,0xf2f5,0xdedd };
 
typedef int32 (*VietFunc)(const unsigned char *frombuffer, unsigned char *tobuffer, int32 maxsz, unsigned char *uncvtbuf);
 
PRIVATE unsigned char charType( unsigned char c);
PRIVATE unsigned char IsVIQRWord(const unsigned char *s,
                                 int32 len_s,
                                 int32 fromI,
                                 int32 *toI);
PRIVATE unsigned char C_VIQR_to_VISCII( unsigned char *HotKeys,
                                        unsigned char *CurState,
                                        unsigned char *LastChar,
                                        unsigned char  NewChar);
PRIVATE unsigned char C_VISCII_to_VIQR( unsigned char *HotKeys,
                                        unsigned char VISCII,
                                        unsigned char *char1,
                                        unsigned char *char2,
                                        unsigned char *char3);
PRIVATE int32 S_VIQR_to_VISCII( const unsigned char *frombuffer,
                                unsigned char *tobuffer,
                                int32 maxsz,
                                unsigned char *uncvtbuf);
PRIVATE int32 S_VISCII_to_VIQR( const unsigned char *frombuffer,
                                unsigned char *tobuffer,
                                int32 maxsz,
                                unsigned char *uncvtbuf);
PRIVATE int32 S_VNI_to_VISCII(  const unsigned char *frombuffer,
                                unsigned char *tobuffer,
                                int32 maxsz,
                                unsigned char *uncvtbuf);
PRIVATE int32 S_VISCII_to_VNI(  const unsigned char *frombuffer,
                                unsigned char *tobuffer,
                                int32 maxsz,
                                unsigned char *uncvtbuf);
PRIVATE int32 S_CP1258_to_VISCII(const unsigned char *frombuffer,
                                 unsigned char *tobuffer,
                                 int32 maxsz,
                                 unsigned char *uncvtbuf);
PRIVATE int32 S_VISCII_to_CP1258(const unsigned char *frombuffer,
                                 unsigned char *tobuffer,
                                 int32 maxsz,
                                 unsigned char *uncvtbuf);
PRIVATE unsigned char *xlat_vis_ucs2_X(CCCDataObject obj,
                                       const unsigned char *buf,
                                       int32 bufsz);
PRIVATE unsigned char *xlat_X_ucs2_vis(CCCDataObject obj,
                                       const unsigned char *buf,
                                       int32 bufsz);
PRIVATE unsigned char *xlat_any_2_any (
                                       CCCDataObject obj,
                                       const unsigned char *in_buf,
                                       int32 in_bufsz,
                                       VietFunc customfunc,
                                       int16 buf_ratio,
                                       int16 do_unconverted);
 
/**********************************************************************/
/*** Function: charType()                                           ***/
/*** Purpose:  Determine the type of a character.                   ***/
/*** Returns:  0 - If c is a non-alpha character                    ***/
/***           1 - If c is a consonant.                             ***/
/***           2 - If c is a diacritical mark                       ***/
/***           3 - If c is a hi-bit character                       ***/
/***           4 - If c is a vowel (AEIOUY)                         ***/
/**********************************************************************/
PRIVATE unsigned char charType( unsigned char c)
{
    if (c < 0x27)      /*** below ' ***/
             return (0);
    else if (c > 0x7e) /*** above ~ ***/
             return (3);
         else {
                if (XP_STRCHR("),-/0123456789:;<=>@[\\]_{|}",c)) return(0);
                if (XP_IS_ALPHA(c)) {
                     if (XP_STRCHR("AEIOUYaeiouy",c)) return(4);
                     else                             return(1);
                }
                else return(2);
         }
}
 
/**********************************************************************/
/*** Function: IsVIQRWord()                                         ***/
/*** Purpose:  Parse a string, marks the location of the first word.***/
/*** Returns:  0: if the word is not a VIQR word                    ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE unsigned char IsVIQRWord(const unsigned char *s,
                                int32 len_s,
                                int32 fromI,
                                int32 *toI)
{   unsigned char c, t, flg;
    int32 i, ctr_letter, ctr_mark, ctr_afterperiod, ctr_vowel;
 
    flg = 0;
    ctr_afterperiod = ctr_vowel = - len_s;  /* set it a big negative number */
    ctr_letter = ctr_mark = 0;
    for (i=fromI; ((i < len_s) && ('\0' != (c = s[i]))); i++) {
         t = charType(c);
         if ((t == 0) || (t == 3))  /*** is control or hi-bit char? ***/
                flg = 1;        /**If it is not-viqr, mark the flag. **/
         else { if (flg == 1) { /**And if after the not-viqr is viqr,**/
                    break;      /**then done **/
                }
                switch (t) {
                  case 2: /*If a mark appears before any letter, ignore*/
                          if (ctr_letter > 0) ctr_mark++;
                          if (c == '.') { /** Pay attention to the period **/
                              if (ctr_afterperiod < 0) ctr_afterperiod=0;
                          }
                          break;
                  case 4: /* Vowel's first encounter will reset the counter*/
                          if (ctr_vowel > 0) { /** if Vowel,Consonant, and */
                              ctr_letter = 10; /** vowel again, then we    */
                          }                    /** force an invalid word   */
                          ctr_vowel = -1;            /** flow to next case */
                  case 1: /* Consonant */
                          ctr_vowel++;
                          ctr_letter++;        /** Count the # of letters  */
                          ctr_afterperiod++;
                }
         }
    }
    *toI = i-1; /*** So the first word will be between fromI and toI ***/
 
    /*******************************************************************/
    /*** Now, the hard part, I should check the semantic of the word,***/
    /*** But it will take more comparisions, and I don't think it is ***/
    /*** necessary, so I temporarily use the common sense here to    ***/
    /*** detect a vietnamese word.  I'll implement semantic checking ***/
    /*** later if it's really necessary                              ***/
    if (*toI > fromI) {
        /*********************************************************/
        /**First: If there are more 2 letters after a "." -> NO **/
        if (ctr_afterperiod > 2) return(0);
        /*********************************************************/
        /**Then: Check for special cases of DD, Dd, dD, dd     ***/
        c = s[fromI];
        if ((c == 'D') || (c == 'd')) {
             c = s[fromI+1];
             if ((c == 'D') || (c == 'd')) {
                  ctr_letter--;
                  if (ctr_mark == 0) ctr_mark++;
             }
        }
        /*********************************************************/
        /**Then: Check for common knowledge: A vietnamese word ***/
        /**always has less than 8 letters and less than 4      ***/
        /**diacritical marks at the same time                  ***/
        if ((ctr_letter > 0) && (ctr_letter < 8)) {
             if ((ctr_mark > 0) && (ctr_mark < 4)) return(1);
        }
    }
    return(0);
}
 
/*********************************************************************/
/*** Function : C_VIQR_to_VISCII()                                 ***/
/*** Purpose:   Given 2 VIQR characters, combine into 1 VISCII     ***/
/*** -------    character if possible                              ***/
/*** Return:  0-FALSE: Couldn't combined.                          ***/
/*** ------:  1-TRUE:  if LastChar is modified to be a VISCII char ***/
/***          2-TRUE:  A state is changing (return BackSpace)      ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)         ***/
/*********************************************************************/
PRIVATE unsigned char C_VIQR_to_VISCII( unsigned char *HotKeys,
                                        unsigned char *CurState,
                                        unsigned char *LastChar,
                                        unsigned char  NewChar)
{   unsigned char idkey, idchar, ochar, nchar, converted;
 
    converted = 0;
    ochar=*LastChar;
 
#ifdef TRUE_VIQR
    unsigned char statechange, cs;
    /***********************************************************/
    statechange=(ochar==HotKeys[9]);
    cs = *CurState;
    if (cs > 0) {
        if  (cs >= 'a')  {
             cs -= 32;  /*** Convert from lowercase to uppercase ***/
            *CurState = cs;
            *LastChar=NewChar;
             return(1);
        }
        else if (statechange)  {
                 for (idkey=0;idkey < 6; idkey++)
                      if (NewChar==VIQRStateName[idkey]) break;
                 statechange=(idkey & 6);
                 if (statechange < 6) { /**A state change is detected**/
                     cs = VIQRStateName[statechange | 1];
                    *CurState=cs;
                    *LastChar=8;
                     return(2);
                 }
             }
    }
    /********************************************************************/
    if      (cs==VIQRStateName[5]) { } /******** Literal **************/
    else if (cs==VIQRStateName[3]) {   /******** English **************/
                       if (statechange) { cs = '\\';
                                         *CurState=cs;
                                         *LastChar=NewChar;
                                          converted=1;
                       }
         }
    else
#endif
         { /*************************************************************/
           /******** Vietnamese (V) or English+ESC (\) mode *************/
           /*************************************************************/
           /****** Solve the problem of DD (4 different cases ***********/
           nchar = NewChar;
           switch (nchar) {
                case 'D': /*** if (HotKeys[8]=='d') ***/
                          nchar = 'd';
                case 'd': if (ochar == '-') ochar = nchar;
                          break;
                case '*': nchar = '+';  /** sometimes a * is used */
                          break;        /** for + instead, */
           }
           /*********************************************************/
           for (idkey=0; idkey < 9; idkey++)
                if (nchar == HotKeys[idkey]) {
#ifdef TRUE_VIQR
                    if (statechange) /*** Escape character ***/
                         converted=1;
                    else
#endif
                         for (idchar=0; idchar < 66; idchar++)
                              if (ochar==HotChars[idchar]) {
                                  if (idkey < 5) {
                                       /******** For '`?~. tones ****/
                                       ochar=idchar % 33;
                                       if (ochar < 12)
                                            NewChar=CombiToneChar[idkey]
                                                [ochar+(idchar / 33)*12];
                                       else NewChar=0;
                                  }    /*****************************/
                                  else
                                  if (idkey==8) { /****** For DD ****/
                                       if ((idchar % 33) == 32)
                                            NewChar=ochar+140;
                                       else NewChar=0;
                                  }    /*****************************/
                                  else /****** For ^(+ modifiers ****/
                                       NewChar=CombiModiChar[idkey-5]
                                                            [idchar];
                                  converted=(NewChar != 0);
                                  break;
                              }
                    if (converted) *LastChar=NewChar;
                    break;
                }
#ifdef TRUE_VIQR
           if ((!converted) && (cs=='\\')) *CurState=VIQRStateName[3];
#endif
         }
    /********************************************************************/
    return(converted);
}
 
/*********************************************************************/
/*** Function : C_VISCII_to_VIQR()                                 ***/
/*** Purpose:   Given 1 VISCII character, split to VIQR chars      ***/
/*** Return:    Number of returned characters.                     ***/
/*** Implementer: Vuong Nguyen (vlangsj@pluto.ftos.net)            ***/
/*********************************************************************/
PRIVATE unsigned char C_VISCII_to_VIQR( unsigned char *HotKeys,
                                        unsigned char VISCII,
                                        unsigned char *char1,
                                        unsigned char *char2,
                                        unsigned char *char3)
{   uint16 key;
    unsigned char mask, NumChars;
    NumChars=1;
    *char2=*char3=0;
    key=VISCII;
    /******************* Get the secret code *************************/
    if (VISCII < 128) {
        if (VISCII < 32)
            switch (VISCII) {
               case 2 : key= 9281; break;    case 20 : key=1113; break;
               case 5 : key=10305; break;    case 25 : key=2137; break;
               case 6 : key=18497; break;    case 30 : key=4185; break;
            }
    }
    else if ((VISCII==240) || (VISCII==208)) {
              key-=140;
              if (HotKeys[8]=='d') *char2=(unsigned char) key;
              else                 *char2=HotKeys[8];
              NumChars++;
         }
         else key=VIS_VIQ[VISCII-128];
    /*****************************************************************/
    *char1=key & 0xFF;      /*** Get the base letter *******/
    if ((key >>= 8))        /*** Get the diacritic marks ***/
         for (mask=0x80,VISCII=7; key ; mask >>= 1, VISCII--)
              if (key & mask) {
                  if (++NumChars ==2) *char2=HotKeys[VISCII];
                  else                *char3=HotKeys[VISCII];
                  key &= (~mask);
              }
    return(NumChars);
}
 
/**********************************************************************/
/*** Procedure : S_VIQR_to_VISCII()                                 ***/
/*** Purpose   : Convert a VIQR string to a VISCII string.          ***/
/*** -------     This routine will not convert any word that seems  ***/
/***             not to be a Vietnamese word. Doing this, we will   ***/
/***             avoid the problem of blind converting email, url,  ***/
/***             html tags.                                         ***/
/***             I would suggest all other vietnamese encodings     ***/
/***             using this routine to convert from VIQR to their   ***/
/***             codes (using VISCII as intermediate code)          ***/
/***             eg:   VIQR -> VISCII -> Vxxx                       ***/
/***             With this technique, then we will not need many    ***/
/***             (duplicate) VIQR conversion routines.              ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/*** Note: 08/19/98: Implemented the unconverted buffer processing  ***/
/*** ----            but then removed because it is too complicated ***/
/***                 (must examine the whole word).                 ***/
/***                 Anyway the user always understand VIQR text    ***/
/**********************************************************************/
PRIVATE int32 S_VIQR_to_VISCII( const unsigned char *frombuffer,
                                unsigned char *tobuffer,
                                int32 maxsz,
                                unsigned char *uncvtbuf)
{  unsigned char oldc, newc, InitState, j, *p;
   int32 fromI, toI, ctr, p_sz;
 
   ctr=0;
   InitState = 0;
        p = (unsigned char *) frombuffer;
        p_sz = maxsz;
        fromI = 0;
        while (fromI < p_sz) {
           if ( IsVIQRWord(p, p_sz,fromI,&toI) ) {
                  /*******************************************************/
                  /*** This is a VIQR word, need conversion **************/
                  j = 0;
                  oldc = p[fromI++];
                  while ((fromI <= toI) && ('\0' != (newc = p[fromI]))) {
                     j = C_VIQR_to_VISCII(DefaultVIQR,&InitState,&oldc,newc);
                     if (j == 0) {
                         tobuffer[ctr++] = oldc;
                         oldc = newc;
                     }
                     fromI++;
                  }
                  if (j != 2) tobuffer[ctr++] = oldc;
           }
           else { /*******************************************************/
                  /*** This is not a VIQR word, simply copy it ***********/
                  while ((fromI <= toI) && ('\0' != (newc = p[fromI]))) {
                     tobuffer[ctr++] = newc;
                     fromI++;
                  }
           }
        }
 
   tobuffer[ctr]=0;
   return(ctr);
}
 
/**********************************************************************/
/*** Procedure :  S_VISCII_to_VIQR()                                ***/
/*** Purpose   :  Convert a VISCII string to a VIQR string.         ***/
/*** IMPORTANT :  The size of tobuffer must be bigger than the size ***/
/*** ---------    of frombuffer about 3 times (worst case)          ***/
/*** Implementer: Vuong Nguyen (vlangsj@pluto.ftos.net)             ***/
/**********************************************************************/
PRIVATE int32 S_VISCII_to_VIQR( const unsigned char *frombuffer,
                                unsigned char *tobuffer,
                                int32 maxsz,
                                unsigned char *uncvtbuf)
{  int32 indx, i, n, ctr;
   unsigned char c, cc[3];
 
   indx = ctr = 0;
   for (indx=0;((indx < maxsz) && ('\0' != (c=frombuffer[indx])));indx++) {
          n = C_VISCII_to_VIQR(DefaultVIQR,c,cc,cc+1,cc+2);
          if ((n==2) && (cc[1]=='d')) cc[1]=cc[0];
          for (i=0; (i < n); i++) tobuffer[ctr++] = cc[i];
   }
   tobuffer[ctr]=0;
   return(ctr);
}
 
/**********************************************************************/
/*** Function: S_VNI_to_VISCII()                                    ***/
/*** Purpose:  Converts a VNI string to a VISCII string             ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE int32 S_VNI_to_VISCII( const unsigned char *frombuffer,
                               unsigned char *tobuffer,
                               int32 maxsz,
                               unsigned char *uncvtbuf)
{    unsigned char  c, v, *p;
     int32 i, ctr, p_sz, istart;
     uint16 j, fr_i, to_i, x, idex, ibuf;
 
     ctr = 0;
     for (ibuf=0; ibuf < 2; ibuf++) {
          if (ibuf == 0) {
                 /*** Process the un-converted buffer first ***/
                 p = uncvtbuf;
                 p_sz = XP_STRLEN((char*)p);
                 istart = 0;
          }
          else { /*** Then the input buffer next ***/
                 p = (unsigned char *) frombuffer;
                 p_sz = maxsz;
                 if (istart != 1) istart=0; /*** adjust the index if ***/
          }                                 /*** the 1st char used   ***/
          /***************************************************/
          for (i = istart;(i < p_sz) && ('\0' != (c = p[i])); i++) {
               if (c < 65) v = c;   /*** saving 65 characters in the table ***/
               else        v = VNI_VIS[ c - 65 ];
               if (v != 255) tobuffer[ctr++] = v;
               else {
                      switch (c) {
                         case  65: idex = 0x0010; break;
                         case  69: idex = 0x111b; break;
                         case  79: idex = 0x1c26; break;
                         case  85: idex = 0x272b; break;
                         case  89: idex = 0x2c30; break;
                         case  97: idex = 0x3141; break;
                         case 101: idex = 0x424c; break;
                         case 111: idex = 0x4d57; break;
                         case 117: idex = 0x585c; break;
                         case 121: idex = 0x5d61; break;
                         case 212: idex = 0x6266; break;
                         case 214: idex = 0x676b; break;
                         case 244: idex = 0x6c70; break;
                         case 246: idex = 0x7175; break;
                         default:  idex = 0;
                      }
                      if (idex != 0) {
                          j = i+1;
                          if (j >= p_sz) v = 0;
                          else           v = p[j];
                          if ((v == 0) && (ibuf == 0) && (maxsz > 0)) {
                               /*** If end of un-convert buffer,  ***/
                               /*** get first char of input buffer***/
                               v = (unsigned char) frombuffer[0];
                               istart = 2;
                          }
                          if (v) {
                               fr_i = (idex >> 8);
                               to_i = (idex & 0xFF);
                               for (j = fr_i; j <= to_i; j++) {
                                    idex = VNIextended_VIS[j];
                                    x = (idex >> 8);
                                    if (x == v) {
                                        x = (idex & 0xFF);
                                        c = (unsigned char) x;
                                        i++;
                                        if (istart == 2) istart=1;
                                        break;
                                    }
                               }
                               if (j > to_i) {
                                   switch (c) {
                                     case 212: c = 180; break;
                                     case 214: c = 191; break;
                                     case 244: c = 189; break;
                                     case 246: c = 223; break;
                                   }
                               }
                          }
                      }
                      tobuffer[ctr++] = c;
               }
          }
     }
     tobuffer[ctr] = 0;
     return(ctr);
}
 
/**********************************************************************/
/*** Function: S_VISCII_to_VNI()                                    ***/
/*** Purpose:  Converts a VISCII string to a VNI string             ***/
/*** Note:     tobuffer must have size at least 2 times bigger than ***/
/*** ----      than frombuffer (1-byte charset -> 2-byte charset)   ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE int32 S_VISCII_to_VNI(  const unsigned char *frombuffer,
                                unsigned char *tobuffer,
                                int32 maxsz,
                                unsigned char *uncvtbuf)
{    unsigned char  c;
     uint16 idex;
     int32 i, ctr;
 
     ctr = 0;
     for (i = 0;(i < maxsz) && ('\0' != (c = (unsigned char) frombuffer[i])); i++) {
          idex = 0;
          if (c < 32)
               switch (c) {
                   case  2: idex = 0xda41; break;
                   case  5: idex = 0xdc41; break;
                   case  6: idex = 0xc341; break;
                   case 20: idex = 0xdb59; break;
                   case 25: idex = 0xd559; break;
                   case 30: c = 0xCE;      break;
               }
          else if (c > 127) idex = VIS_VNI[c-128];
          if (idex) {
               tobuffer[ctr++] = (unsigned char) (idex & 0xFF);
               c = (unsigned char) (idex >> 8);
               if (c) tobuffer[ctr++] = c;
          }
          else tobuffer[ctr++] = c;
     }
     tobuffer[ctr] = 0;
     return(ctr);
}
 
/**********************************************************************/
/*** Function: S_CP1258_to_VISCII()                                 ***/
/*** Purpose:  Converts a CP1258 string to a VISCII string          ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE int32 S_CP1258_to_VISCII( const unsigned char *frombuffer,
                                  unsigned char *tobuffer,
                                  int32 maxsz,
                                  unsigned char *uncvtbuf)
{    unsigned char  c, v, *p;
     int32 i, ctr, p_sz, istart;
     uint16 j, fr_i, to_i, x, idex, ibuf;
 
     ctr = 0;
     for (ibuf=0; ibuf < 2; ibuf++) {
          if (ibuf == 0) {
                 /*** Process the un-converted buffer first ***/
                 p = uncvtbuf;
                 p_sz = XP_STRLEN((char*)p);
                 istart = 0;
          }
          else { /*** Then the input buffer next ***/
                 p = (unsigned char *) frombuffer;
                 p_sz = maxsz;
                 if (istart != 1) istart=0; /*** adjust the index if ***/
          }                                 /*** the 1st char used   ***/
          /***************************************************/
          for (i = istart;(i < p_sz) && ('\0' != (c = p[i])); i++) {
               if (c < 65) v = c;  /*** saving 65 characters in the table ***/
               else        v = CP1258_VIS[ c - 65 ];
               if (v != 255) tobuffer[ctr++] = v;
               else { /*** now c contains the base letter *****************/
                      /*** Look for the next char as a diacritical mark ***/
                      j = i+1;
                      if (j >= p_sz) v = 0;
                      else           v = p[j];
                      if ((v == 0) && (ibuf == 0) && (maxsz > 0)) {
                           /*** If end of un-convert buffer,  ***/
                           /*** get first char of input buffer***/
                           v = (unsigned char) frombuffer[0];
                           istart = 2;
                      }
                      if (v) {
                          switch (v) {
                            case 0xcc: idex=0x0011; break;
                            case 0xd2: idex=0x1229; break;
                            case 0xde: idex=0x2a41; break;
                            case 0xec: idex=0x424f; break;
                            case 0xf2: idex=0x5067; break;
                            default:   idex=0;
                          }
                          if (idex) {
                              fr_i = (idex >> 8);
                              to_i = (idex & 0xFF);
                              for (j = fr_i; j <= to_i; j++) {
                                   idex = CP1258extended_VIS[j];
                                   x = (idex >> 8);
                                   if (x == c) {
                                       x = (idex & 0xFF);
                                       c = (unsigned char) x;
                                       i++;
                                       if (istart == 2) istart=1;
                                       break;
                                   }
                              }
                          }
                      }
                      tobuffer[ctr++] = c;
               }
          }
     }
     tobuffer[ctr] = 0;
     return(ctr);
}
 
/**********************************************************************/
/*** Function: S_VISCII_to_CP1258()                                 ***/
/*** Purpose:  Converts a VISCII string to a CP1258 string          ***/
/*** Note:     tobuffer must have size at least 2 times bigger than ***/
/*** ----      than frombuffer (1-byte charset -> 2-byte charset)   ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE int32 S_VISCII_to_CP1258( const unsigned char *frombuffer,
                                  unsigned char *tobuffer,
                                  int32 maxsz,
                                  unsigned char *uncvtbuf)
{    unsigned char  c;
     uint16 idex;
     int32 i, ctr;
 
     ctr = 0;
     for (i = 0;(i < maxsz) && ('\0' != (c = (unsigned char) frombuffer[i])); i++) {
          idex = 0;
          if (c < 32)
               switch (c) {
                   case  2: idex = 0xd2c3; break;
                   case  5: idex = 0xdec3; break;
                   case  6: idex = 0xdec2; break;
                   case 20: idex = 0xd259; break;
                   case 25: idex = 0xde59; break;
                   case 30: idex = 0xf259; break;
               }
          else if (c > 127) idex = VIS_CP1258[c-128];
          if (idex) {
               tobuffer[ctr++] = (unsigned char) (idex & 0xFF);
               c = (unsigned char) (idex >> 8);
               if (c) tobuffer[ctr++] = c;
          }
          else tobuffer[ctr++] = c;
     }
     tobuffer[ctr] = 0;
     return(ctr);
}
 
/**********************************************************************/
/*** Function:  xlat_vis_ucs2_X()                                   ***/
/*** Purpose:   Converts a converted temporary VISCII buffer to a   ***/
/*** -------    to_charset via mz_AnyToAnyThroughUCS2               ***/
/**********************************************************************/
PRIVATE unsigned char *xlat_vis_ucs2_X(CCCDataObject obj,
                                       const unsigned char *buf,
                                       int32 bufsz)
{   unsigned char *out_buf;
    uint16 oldcsid;
    if (buf) {
         oldcsid = INTL_GetCCCFromCSID(obj); /* Save current setting */
         INTL_SetCCCFromCSID(obj, CS_VIET_VISCII); /* then set viscii */
         out_buf = (unsigned char *) mz_AnyToAnyThroughUCS2(obj, buf, bufsz);
         INTL_SetCCCFromCSID(obj, oldcsid);  /* Restore current setting */
         return(out_buf);
    }
    else return (NULL);
}
 
/**********************************************************************/
/*** Function:  xlat_X_ucs2_vis()                                   ***/
/*** Purpose:   Converts a from_charset buffer to a temporary       ***/
/*** -------    VISCII buffer via mz_AnyToAnyThroughUCS2            ***/
/**********************************************************************/
PRIVATE unsigned char *xlat_X_ucs2_vis(CCCDataObject obj,
                                       const unsigned char *buf,
                                       int32 bufsz)
{   unsigned char *out_buf;
    uint16 oldcsid;
    if (buf) {
         oldcsid = INTL_GetCCCToCSID(obj); /* Save current setting */
         INTL_SetCCCToCSID(obj, CS_VIET_VISCII); /* then set viscii */
         out_buf = (unsigned char *) mz_AnyToAnyThroughUCS2(obj, buf, bufsz);
         INTL_SetCCCToCSID(obj, oldcsid);  /* Restore current setting */
         return (out_buf);
    }
    else return (NULL);
}
 
/**********************************************************************/
/*** Function:  xlat_any_2_any()                                    ***/
/*** Purpose:   Converts a from_csid buffer to a to_csid buffer     ***/
/*** Arguments:   in_buf:      Ptr to the input buffer              ***/
/*** ---------    in_bufsize:  Size in bytes of in_buf              ***/
/***              customfunc:  Address to the conversion function   ***/
/***              buf_ratio:   Contains the multiplication ratio for***/
/***                           getting sufficient output memory     ***/
/***              do_unconverted: flag telling to process unconverted**/
/***                              buffer, it also used as a signal  ***/
/***                              where should stop when putting char**/
/***                              into unconverted buffer           ***/
/*** Returns:   Returns NULL on failure, otherwise it returns a     ***/
/*** -------    pointer to a buffer converted characters.           ***/
/***            Caller must XP_FREE() this memory.                  ***/
/*** Implementer:    Vuong Nguyen (vlangsj@pluto.ftos.net)          ***/
/**********************************************************************/
PRIVATE unsigned char *xlat_any_2_any(
                                      CCCDataObject obj,
                                      const unsigned char *in_buf,
                                      int32 in_bufsz,
                                      VietFunc customfunc,
                                      int16 buf_ratio,
                                      int16 do_unconverted)
{
    int32  out_bufsz, uncvt_bufsz, i, ctr;
    unsigned char *uncvtbuf, *out_buf;
    unsigned char c, nextunconv[UNCVTBUF_SIZE];
 
    /***** Take care the un-converted buffer *********************/
    ctr = 0;
    uncvtbuf = INTL_GetCCCUncvtbuf(obj);
    if (do_unconverted) {
           uncvt_bufsz = XP_STRLEN((char*)uncvtbuf);
 
           /*** Save the uncvtbuf only if the buffer is big, otherwise ***/
           /*** we can assume this is the last packet, process all ***/
           if (in_bufsz >= 128) { /*** is 128 big enough ***/
               /*** We go backward until see a non-vowel character ***/
               for (i=(in_bufsz-1); (i >= 0); i--) {
                    c = in_buf[i];
                    /*** We also use do_unconverted as a stop char       **/
                    /*** For viqr, stop when seeing a non-alpha char (<1)**/
                    /*** else, stop when NOT seeing vowel or hi-bit  (<3)**/
                    if (charType(c) < do_unconverted)
                        break;
                    if (++ctr > UNCVTBUF_SIZE-1) {
                        /*** Worst case, for CP1258 & VNI, the combining */
                        /*** vowels always happen at the last 6 letters, */
                        /*** otherwise, we can conclude it's not a viet  */
                        /*** word, don't need to keep un-converted buffer*/
                        ctr = 0;
                        break;
                    }
               }
           }
           for (i=0; i < ctr;i++) {
                nextunconv[i] = in_buf[ in_bufsz - ctr + i ];
           }
           nextunconv[i] = 0; /*** save next uncnvbuf ***/
           in_bufsz -= ctr;   /*** reduce the size of input buffer ***/
    }
    else { /*** This happens for single-byte charset, we don't ***/
           uncvtbuf[0] = 0; /*** need the un-converted buffer, ***/
           uncvt_bufsz = 0; /*** so we clear it                ***/
    }
    /*************************************************************/
    out_buf = NULL;
    out_bufsz = ((in_bufsz + uncvt_bufsz) * buf_ratio) + 1;
    if ((out_buf = (unsigned char *)XP_ALLOC(out_bufsz)) == (unsigned char *)NULL) {
         INTL_SetCCCRetval(obj, MK_OUT_OF_MEMORY);
         return(NULL);
    }
    out_bufsz = customfunc(in_buf, out_buf, in_bufsz, uncvtbuf);
    INTL_SetCCCLen(obj, out_bufsz);   /* length not counting null */
    /*************************************************************/
    if (do_unconverted) {
        XP_STRCPY((char*)uncvtbuf,(char*)nextunconv);
    }
    return(out_buf);
}
 
/*####################################################################*/
/*### The following routines are called by Mozilla                 ###*/
/*####################################################################*/
 
 
/**********************************************************************/
/*** Function:  viet_any_2_any()                                    ***/
/*** Purpose:   Converts a from_csid buffer to a to_csif buffer.    ***/
/**********************************************************************/
MODULE_PRIVATE unsigned char *viet_any_2_any(
                                      CCCDataObject obj,
                                      const unsigned char *in_buf,
                                      int32 in_bufsz)
{
    unsigned char *out_buf, *tmp_buf;
    uint16 fcsid, tcsid, use_tmp_mem;
    int32  bufsz;
 
    fcsid = INTL_GetCCCFromCSID(obj);
    tcsid = INTL_GetCCCToCSID(obj);
    if (fcsid == tcsid)  /*** This case is similar to (CCCFunc) 0 ***/
                 return ((unsigned char *) in_buf);
 
    /*******************************************************************/
    /*** Convert from from_csid buffer to a temporary VISCII buffer ****/
    use_tmp_mem = 1;     /*** Assume we will use a temporary VIS buf ***/
    switch (fcsid) {
        case CS_VIET_VISCII:
                  /*** No intermediate conversion is needed ***/
                  out_buf = (unsigned char *) in_buf;
                  bufsz = in_bufsz;
                  use_tmp_mem = 0;
                  break;
 
                  /*** These are multi-bytes charsets, cannot convert ***/
                  /*** via UCS2, so we use our internal conversion    ***/
 
        case CS_VIET_VIQR:  /*** disable unconverted buffer processing **/
                            /*** by setting last parameter to be 0     **/
                  out_buf = xlat_any_2_any(obj,in_buf,in_bufsz,
                                (VietFunc) S_VIQR_to_VISCII, 1, 0);
                  break;
        case CS_VIET_VNI:
                  out_buf = xlat_any_2_any(obj,in_buf,in_bufsz,
                                (VietFunc) S_VNI_to_VISCII, 1, 3);
                  break;
        case CS_CP_1258:
                  out_buf = xlat_any_2_any(obj,in_buf,in_bufsz,
                                (VietFunc) S_CP1258_to_VISCII, 1, 3);
                  break;
 
        default:  /*** A via-UCS2 conversion is needed        ***/
                  out_buf = xlat_X_ucs2_vis(obj, in_buf, in_bufsz);
    }
 
    /*******************************************************************/
    /*** Convert the temporary VISCII buffer to to_csid buffer      ****/
    if (out_buf) {
        if (use_tmp_mem) bufsz = INTL_GetCCCLen(obj);
        switch (tcsid) {
           case CS_VIET_VIQR:
                   tmp_buf = xlat_any_2_any(obj,out_buf,bufsz,
                                 (VietFunc) S_VISCII_to_VIQR, 3, 0);
                   break;
           case CS_VIET_VNI:
                   tmp_buf = xlat_any_2_any(obj,out_buf,bufsz,
                                 (VietFunc) S_VISCII_to_VNI, 2, 0);
                   break;
           case CS_CP_1258:
                   tmp_buf = xlat_any_2_any(obj,out_buf,bufsz,
                                 (VietFunc) S_VISCII_to_CP1258, 2, 0);
                   break;
           case CS_VIET_VISCII:
                   use_tmp_mem = 0;
                   tmp_buf = out_buf;
                   break;
           default:
                   tmp_buf = xlat_vis_ucs2_X(obj, out_buf, bufsz);
        }
        if (use_tmp_mem) { /*** Multi-byte functions already got memory*/
            XP_FREE(out_buf); /* so we need to free the temp VIS buf ***/
        }
        out_buf = tmp_buf;
    }
 
    return(out_buf);
}

