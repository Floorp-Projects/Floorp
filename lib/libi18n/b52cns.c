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
/*	b52cns.c	*/
#include "intlpriv.h"
extern int MK_OUT_OF_MEMORY;

/* ------------------------------------------------------------------------------------------ 
   Function should be used internal to libi18n but need to referred 
   out side this file
 * ------------------------------------------------------------------------------------------ */
MODULE_PRIVATE unsigned char* mz_euctwtob5( CCCDataObject obj, const unsigned char *in,  int32 insize);
MODULE_PRIVATE unsigned char* mz_b5toeuctw( CCCDataObject obj, const unsigned char *in,  int32 insize);

/* ------------------------------------------------------------------------------------------ *
   Small function to isolate conversion code from CCCDataObject 
 * ------------------------------------------------------------------------------------------ */
PRIVATE void intl_fc_restore_conv_state(CCCDataObject obj, uint32* pst1, uint32* pst2); 
PRIVATE void intl_fc_save_conv_state(CCCDataObject obj, uint32 st1, uint32 st2); 
PRIVATE unsigned char* intl_fc_report_out_of_memory(CCCDataObject obj);
PRIVATE unsigned char* intl_fc_return_buf_and_len( CCCDataObject obj,  unsigned char* out, int32 outlen);
PRIVATE uint32 intl_same_outsize(uint32 insize);
PRIVATE uint32 intl_x2_outsize(uint32 insize);

PRIVATE XP_Bool intl_convcnstobig5(uint16 cns, uint8 cnsplane, uint16* big5);
PRIVATE XP_Bool intl_convbig5tocns(uint16 big5, uint16* cns, uint8* cnsplane);
/* ------------------------------------------------------------------------------------------ 
   BIG5 to CNS conversion table stuff 
 * ------------------------------------------------------------------------------------------ */
typedef struct {
  uint16 cnsfromseq;
  uint16 cnstoseq;
  uint16 big5fromseq;
} cnstobig5;
/* ------------------------------------------------------------------------------------------ */
typedef struct {
  uint16 big5fromseq;
  uint16 big5toseq;
  uint16 cnsfromseq;
  uint8  cnsplane;
} big5tocns;
/* ------------------------------------------------------------------------------------------ */
#define BIG5HIBASE     0x00A1
#define BIG5GAPBASE    0x007F
#define BIG5LOWBASE    0x0040
#define BIG5LOWERROW   ( BIG5GAPBASE - BIG5LOWBASE )
#define BIG5GAP        ( BIG5HIBASE - BIG5GAPBASE )
#define BIG5HIGHERROW  ( 0x00FF - BIG5HIBASE )
#define BIG5ROW        ( BIG5LOWERROW + BIG5HIGHERROW )

#define CNSROWBASE     0x0021
#define CNSROW         ( 0x007F - CNSROWBASE )
													  
#ifdef HIBYTE
#undef HIBYTE
#endif

#define HIBYTE(w)      (((w) & 0xff00) >> 8)
#define LOWBYTE(w)     ((w) & 0x00ff)
#define CNSTOSEQ(c)    ((CNSROW * (HIBYTE(c) - CNSROWBASE)) + (LOWBYTE(c) - CNSROWBASE))
#define SEQTOCNS(s)    (((((s) / CNSROW) + CNSROWBASE) << 8 ) | (((s) % CNSROW) + CNSROWBASE))
#define BIG5TOSEQ(b)   ((HIBYTE(b) - 0x00A1) * BIG5ROW + ( LOWBYTE(b) - 0x40 ) - ( (b & 0x0080) ? BIG5GAP : 0 ))
#define SEQTOBIG5(s)   (((((s) / BIG5ROW) + BIG5HIBASE) << 8 ) | ( ((s) % BIG5ROW ) + BIG5LOWBASE + ((( (s) % BIG5ROW )  >= BIG5LOWERROW ) ? BIG5GAP : 0)))

#define CNSTOB5_CELL_BEGIN_END(c1, c2, b)     { CNSTOSEQ(c1), CNSTOSEQ(c2), BIG5TOSEQ(b) } ,
#define B5TOCNS_CELL_BEGIN_END(b1, b2, p, c)  { BIG5TOSEQ(b1), BIG5TOSEQ(b2), CNSTOSEQ(c), (p) } ,

#define CNSTOB5_CELL_BEGIN(c, b)              { CNSTOSEQ(c), CNSTOSEQ(c), BIG5TOSEQ(b) } ,
#define B5TOCNS_CELL_BEGIN(b, p, c)           { BIG5TOSEQ(b), BIG5TOSEQ(b), CNSTOSEQ(c), (p) } ,

/* ------------------------------------------------------------------------------------------ */
PRIVATE cnstobig5 cns1_map[] = {
/* A.1.  Big5 (ETen, IBM, and Microsoft version) symbol set correspondence*/
/*       to CNS 11643 Plane 1:*/
CNSTOB5_CELL_BEGIN_END(0x2121, 0x2256, 0xA140) /*       0xA140-0xA1F5 <-> 0x2121-0x2256 */
CNSTOB5_CELL_BEGIN    (0x2257        , 0xA1F7) /*              0xA1F7 <-> 0x2257 */
CNSTOB5_CELL_BEGIN    (0x2258        , 0xA1F6) /*              0xA1F6 <-> 0x2258 */
CNSTOB5_CELL_BEGIN_END(0x2259, 0x234E, 0xA1F8) /*       0xA1F8-0xA2AE <-> 0x2259-0x234E */
CNSTOB5_CELL_BEGIN_END(0x2421, 0x2570, 0xA2AF) /*       0xA2AF-0xA3BF <-> 0x2421-0x2570 */
/* A.4.  Big5 (ETen and IBM Version) specific numeric symbols*/
/*       correspondence to CNS 11643 Plane 1: (Microsoft version defined*/
/*       this area as UDC - User Defined Character)*/
CNSTOB5_CELL_BEGIN_END(0x2621, 0x263E, 0xC6A1) /*       0xC6A1-0xC6BE <-> 0x2621-0x263E */
/* A.5.  Big5 (ETen and IBM Version) specific KangXi radicals*/
/*       correspondence to CNS 11643 Plane 1: (Microsoft version defined as*/
/*       UDC - User Definable Character)*/
CNSTOB5_CELL_BEGIN    (0x2723        , 0xC6BF) /*              0xC6BF <-> 0x2723 */
CNSTOB5_CELL_BEGIN    (0x2724        , 0xC6C0) /*              0xC6C0 <-> 0x2724 */
CNSTOB5_CELL_BEGIN    (0x2726        , 0xC6C1) /*              0xC6C1 <-> 0x2726 */
CNSTOB5_CELL_BEGIN    (0x2728        , 0xC6C2) /*              0xC6C2 <-> 0x2728 */
CNSTOB5_CELL_BEGIN    (0x272D        , 0xC6C3) /*              0xC6C3 <-> 0x272D */
CNSTOB5_CELL_BEGIN    (0x272E        , 0xC6C4) /*              0xC6C4 <-> 0x272E */
CNSTOB5_CELL_BEGIN    (0x272F        , 0xC6C5) /*              0xC6C5 <-> 0x272F */
CNSTOB5_CELL_BEGIN    (0x2734        , 0xC6C6) /*              0xC6C6 <-> 0x2734 */
CNSTOB5_CELL_BEGIN    (0x2737        , 0xC6C7) /*              0xC6C7 <-> 0x2737 */
CNSTOB5_CELL_BEGIN    (0x273A        , 0xC6C8) /*              0xC6C8 <-> 0x273A */
CNSTOB5_CELL_BEGIN    (0x273C        , 0xC6C9) /*              0xC6C9 <-> 0x273C */
CNSTOB5_CELL_BEGIN    (0x2742        , 0xC6CA) /*              0xC6CA <-> 0x2742 */
CNSTOB5_CELL_BEGIN    (0x2747        , 0xC6CB) /*              0xC6CB <-> 0x2747 */
CNSTOB5_CELL_BEGIN    (0x274E        , 0xC6CC) /*              0xC6CC <-> 0x274E */
CNSTOB5_CELL_BEGIN    (0x2753        , 0xC6CD) /*              0xC6CD <-> 0x2753 */
CNSTOB5_CELL_BEGIN    (0x2754        , 0xC6CE) /*              0xC6CE <-> 0x2754 */
CNSTOB5_CELL_BEGIN    (0x2755        , 0xC6CF) /*              0xC6CF <-> 0x2755 */
CNSTOB5_CELL_BEGIN    (0x2759        , 0xC6D0) /*              0xC6D0 <-> 0x2759 */
CNSTOB5_CELL_BEGIN    (0x275A        , 0xC6D1) /*              0xC6D1 <-> 0x275A */
CNSTOB5_CELL_BEGIN    (0x2761        , 0xC6D2) /*              0xC6D2 <-> 0x2761 */
CNSTOB5_CELL_BEGIN    (0x2766        , 0xC6D3) /*              0xC6D3 <-> 0x2766 */
CNSTOB5_CELL_BEGIN    (0x2829        , 0xC6D4) /*              0xC6D4 <-> 0x2829 */
CNSTOB5_CELL_BEGIN    (0x282A        , 0xC6D5) /*              0xC6D5 <-> 0x282A */
CNSTOB5_CELL_BEGIN    (0x2863        , 0xC6D6) /*              0xC6D6 <-> 0x2863 */
CNSTOB5_CELL_BEGIN    (0x286C        , 0xC6D7) /*              0xC6D7 <-> 0x286C */
/* A.2.  Big5 (ETen, IBM, and Microsoft version) correspondence*/
/*       to CNS 11643 Plane 1:*/
CNSTOB5_CELL_BEGIN_END(0x4221, 0x4241, 0xA3C0) /*       0xA3C0-0xA3E0 <-> 0x4221-0x4241  */
CNSTOB5_CELL_BEGIN_END(0x4421, 0x5322, 0xA440) /*       0xA440-0xACFD <-> 0x4421-0x5322 */
CNSTOB5_CELL_BEGIN_END(0x5323, 0x5752, 0xAD40) /*       0xAD40-0xAFCF <-> 0x5323-0x5752 */
CNSTOB5_CELL_BEGIN    (0x5753        , 0xACFE) /*              0xACFE <-> 0x5753 */
CNSTOB5_CELL_BEGIN_END(0x5754, 0x6B4F, 0xAFD0) /*       0xAFD0-0xBBC7 <-> 0x5754-0x6B4F */
CNSTOB5_CELL_BEGIN    (0x6B50        , 0xBE52) /*              0xBE52 <-> 0x6B50 */
CNSTOB5_CELL_BEGIN_END(0x6B51, 0x6F5B, 0xBBC8) /*       0xBBC8-0xBE51 <-> 0x6B51-0x6F5B */
CNSTOB5_CELL_BEGIN_END(0x6F5C, 0x7534, 0xBE53) /*       0xBE53-0xC1AA <-> 0x6F5C-0x7534 */
CNSTOB5_CELL_BEGIN    (0x7535        , 0xC2CB) /*              0xC2CB <-> 0x7535 */
CNSTOB5_CELL_BEGIN_END(0x7536, 0x7736, 0xC1AB) /*       0xC1AB-0xC2CA <-> 0x7536-0x7736 */
CNSTOB5_CELL_BEGIN_END(0x7737, 0x782C, 0xC2CC) /*       0xC2CC-0xC360 <-> 0x7737-0x782C */
CNSTOB5_CELL_BEGIN    (0x782D        , 0xC456) /*              0xC456 <-> 0x782D */
CNSTOB5_CELL_BEGIN_END(0x782E, 0x7863, 0xC361) /*       0xC361-0xC3B8 <-> 0x782E-0x7863 */
CNSTOB5_CELL_BEGIN    (0x7864        , 0xC3BA) /*              0xC3BA <-> 0x7864 */
CNSTOB5_CELL_BEGIN    (0x7865        , 0xC3B9) /*              0xC3B9 <-> 0x7865 */
CNSTOB5_CELL_BEGIN_END(0x7866, 0x7961, 0xC3BB) /*       0xC3BB-0xC455 <-> 0x7866-0x7961 */
CNSTOB5_CELL_BEGIN_END(0x7962, 0x7D4B, 0xC457) /*       0xC457-0xC67E <-> 0x7962-0x7D4B */
};
/* -------------------------------------------------------------------- */
PRIVATE cnstobig5 cns2_map[] = {
/* A.3.  Big5 (ETen, IBM, and Microsoft version) Level 2 correspondence to*/
/*       CNS 11643-1992 Plane 2:*/
CNSTOB5_CELL_BEGIN_END(0x2121, 0x212A, 0xC940) /*       0xC940-0xC949 <-> 0x2121-0x212A */
CNSTOB5_CELL_BEGIN_END(0x212B, 0x214B, 0xC94B) /*       0xC94B-0xC96B <-> 0x212B-0x214B */
CNSTOB5_CELL_BEGIN    (0x214C        , 0xC9BE) /*              0xC9BE <-> 0x214C */
CNSTOB5_CELL_BEGIN_END(0x214D, 0x217C, 0xC96C) /*       0xC96C-0xC9BD <-> 0x214D-0x217C */
CNSTOB5_CELL_BEGIN_END(0x217D, 0x224C, 0xC9BF) /*       0xC9BF-0xC9EC <-> 0x217D-0x224C */
CNSTOB5_CELL_BEGIN    (0x224D        , 0xCAF7) /*              0xCAF7 <-> 0x224D */
CNSTOB5_CELL_BEGIN_END(0x224E, 0x2438, 0xC9ED) /*       0xC9ED-0xCAF6 <-> 0x224E-0x2438 */
CNSTOB5_CELL_BEGIN_END(0x2439, 0x387D, 0xCAF8) /*       0xCAF8-0xD779 <-> 0x2439-0x387D */
CNSTOB5_CELL_BEGIN_END(0x387E, 0x3F69, 0xD77B) /*       0xD77B-0xDBA6 <-> 0x387E-0x3F69 */
CNSTOB5_CELL_BEGIN    (0x3F6A        , 0xD77A) /*              0xD77A <-> 0x3F6A */
CNSTOB5_CELL_BEGIN_END(0x3F6B, 0x4423, 0xDBA7) /*       0xDBA7-0xDDFB <-> 0x3F6B-0x4423 */
CNSTOB5_CELL_BEGIN_END(0x4424, 0x554A, 0xDDFD) /*       0xDDFD-0xE8A2 <-> 0x4424-0x554A */
CNSTOB5_CELL_BEGIN    (0x554B        , 0xEBF1) /*              0xEBF1 <-> 0x554B */
CNSTOB5_CELL_BEGIN_END(0x554C, 0x5721, 0xE8A3) /*       0xE8A3-0xE975 <-> 0x554C-0x5721 */
CNSTOB5_CELL_BEGIN    (0x5722        , 0xECDE) /*              0xECDE <-> 0x5722 */
CNSTOB5_CELL_BEGIN_END(0x5723, 0x5A27, 0xE976) /*       0xE976-0xEB5A <-> 0x5723-0x5A27 */
CNSTOB5_CELL_BEGIN    (0x5A28        , 0xF0CB) /*              0xF0CB <-> 0x5A28 */
CNSTOB5_CELL_BEGIN_END(0x5A29, 0x5B3E, 0xEB5B) /*       0xEB5B-0xEBF0 <-> 0x5A29-0x5B3E */
CNSTOB5_CELL_BEGIN_END(0x5B3F, 0x5C69, 0xEBF2) /*       0xEBF2-0xECDD <-> 0x5B3F-0x5C69 */
CNSTOB5_CELL_BEGIN_END(0x5C6A, 0x5D73, 0xECDF) /*       0xECDF-0xEDA9 <-> 0x5C6A-0x5D73 */
CNSTOB5_CELL_BEGIN    (0x5D74        , 0xF056) /*              0xF056 <-> 0x5D74 */
CNSTOB5_CELL_BEGIN_END(0x5D75, 0x6038, 0xEDAA) /*       0xEDAA-0xEEEA <-> 0x5D75-0x6038 */
CNSTOB5_CELL_BEGIN_END(0x6039, 0x6242, 0xEEEC) /*       0xEEEC-0xF055 <-> 0x6039-0x6242 */
CNSTOB5_CELL_BEGIN_END(0x6243, 0x6336, 0xF057) /*       0xF057-0xF0CA <-> 0x6243-0x6336 */
CNSTOB5_CELL_BEGIN_END(0x6337, 0x642E, 0xF0CC) /*       0xF0CC-0xF162 <-> 0x6337-0x642E */
CNSTOB5_CELL_BEGIN    (0x642F        , 0xEEEB) /*              0xEEEB <-> 0x642F */
CNSTOB5_CELL_BEGIN_END(0x6430, 0x6437, 0xF163) /*       0xF163-0xF16A <-> 0x6430-0x6437 */
CNSTOB5_CELL_BEGIN_END(0x6438, 0x6572, 0xF16C) /*       0xF16C-0xF267 <-> 0x6438-0x6572 */
CNSTOB5_CELL_BEGIN_END(0x6573, 0x664C, 0xF269) /*       0xF269-0xF2C2 <-> 0x6573-0x664C */
CNSTOB5_CELL_BEGIN    (0x664D        , 0xF4B5) /*              0xF4B5 <-> 0x664D */
CNSTOB5_CELL_BEGIN_END(0x664E, 0x6760, 0xF2C3) /*       0xF2C3-0xF374 <-> 0x664E-0x6760 */
CNSTOB5_CELL_BEGIN    (0x6761        , 0xF16B) /*              0xF16B <-> 0x6761 */
CNSTOB5_CELL_BEGIN_END(0x6762, 0x6933, 0xF375) /*       0xF375-0xF465 <-> 0x6762-0x6933 */
CNSTOB5_CELL_BEGIN    (0x6934        , 0xF268) /*              0xF268 <-> 0x6934 */
CNSTOB5_CELL_BEGIN_END(0x6935, 0x6961, 0xF466) /*       0xF466-0xF4B4 <-> 0x6935-0x6961 */
CNSTOB5_CELL_BEGIN_END(0x6962, 0x6A4A, 0xF4B6) /*       0xF4B6-0xF4FC <-> 0x6962-0x6A4A */
CNSTOB5_CELL_BEGIN    (0x6A4B        , 0xF663) /*              0xF663 <-> 0x6A4B */
CNSTOB5_CELL_BEGIN_END(0x6A4C, 0x6C51, 0xF4FD) /*       0xF4FD-0xF662 <-> 0x6A4C-0x6C51 */
CNSTOB5_CELL_BEGIN_END(0x6C52, 0x7165, 0xF664) /*       0xF664-0xF976 <-> 0x6C52-0x7165 */
CNSTOB5_CELL_BEGIN    (0x7166        , 0xF9C4) /*              0xF9C4 <-> 0x7166 */
CNSTOB5_CELL_BEGIN_END(0x7167, 0x7233, 0xF977) /*       0xF977-0xF9C3 <-> 0x7167-0x7233 */
CNSTOB5_CELL_BEGIN    (0x7234        , 0xF9C5) /*              0xF9C5 <-> 0x7234 */
CNSTOB5_CELL_BEGIN_END(0x7235, 0x723F, 0xF9C7) /*       0xF9C7-0xF9D1 <-> 0x7235-0x723F */
CNSTOB5_CELL_BEGIN    (0x7240        , 0xF9C6) /*              0xF9C6 <-> 0x7240 */
CNSTOB5_CELL_BEGIN_END(0x7241, 0x7244, 0xF9D2) /*       0xF9D2-0xF9D5 <-> 0x7241-0x7244 */
};
/* -------------------------------------------------------------------- */
PRIVATE cnstobig5 cns3_map[] = {
/* A.6.  Big5 (ETen and Microsoft version) specific Ideographs*/
/*       correspondence to CNS 11643 Plane 3: (IBM version defined as UDC)*/
CNSTOB5_CELL_BEGIN    (0x2C5D        , 0xF9DA) /*              0xF9DA <-> 0x2C5D */
CNSTOB5_CELL_BEGIN    (0x3D7E        , 0xF9DB) /*              0xF9DB <-> 0x3D7E */
CNSTOB5_CELL_BEGIN    (0x4337        , 0xF9D6) /*              0xF9D6 <-> 0x4337 */
CNSTOB5_CELL_BEGIN    (0x444E        , 0xF9D8) /*              0xF9D8 <-> 0x444E */
CNSTOB5_CELL_BEGIN    (0x4B5C        , 0xF9DC) /*              0xF9DC <-> 0x4B5C */
CNSTOB5_CELL_BEGIN    (0x4F50        , 0xF9D7) /*              0xF9D7 <-> 0x4F50 */
CNSTOB5_CELL_BEGIN    (0x504A        , 0xF9D9) /*              0xF9D9 <-> 0x504A */
};
/* -------------------------------------------------------------------- */
PRIVATE cnstobig5 cns4_map[] = {
/* A.7.  Big5 (ETen version only) specific symbols correspondence to CNS*/
/*       11643 Plane 4:*/
CNSTOB5_CELL_BEGIN    (0x2123        , 0xC879) /*              0xC879 <-> 0x2123 */
CNSTOB5_CELL_BEGIN    (0x2124        , 0xC87B) /*              0xC87B <-> 0x2124 */
CNSTOB5_CELL_BEGIN    (0x212A        , 0xC87D) /*              0xC87D <-> 0x212A */
CNSTOB5_CELL_BEGIN    (0x2152        , 0xC8A2) /*              0xC8A2 <-> 0x2152 */
};
/* -------------------------------------------------------------------- */
PRIVATE cnstobig5 *cns_map[4] = {
  cns1_map,
  cns2_map,
  cns3_map,
  cns4_map
};
/* -------------------------------------------------------------------- */
PRIVATE uint32 cns_map_size[4] = {
  sizeof(cns1_map)/sizeof(cnstobig5),
  sizeof(cns2_map)/sizeof(cnstobig5),
  sizeof(cns3_map)/sizeof(cnstobig5),
  sizeof(cns4_map)/sizeof(cnstobig5)
};
/* -------------------------------------------------------------------- */
PRIVATE big5tocns big5_map[] = {
#define CNSPLANE 1
/* A.1.  Big5 (ETen, IBM, and Microsoft version) symbol set correspondence*/
/*       to CNS 11643 Plane 1:*/
B5TOCNS_CELL_BEGIN_END( 0xA140, 0xA1F5, CNSPLANE , 0x2121) /*       0xA140-0xA1F5 <-> 0x2121-0x2256 */
B5TOCNS_CELL_BEGIN    ( 0xA1F6,         CNSPLANE , 0x2258) /*              0xA1F6 <-> 0x2258 */
B5TOCNS_CELL_BEGIN    ( 0xA1F7,         CNSPLANE , 0x2257) /*              0xA1F7 <-> 0x2257 */
B5TOCNS_CELL_BEGIN_END( 0xA1F8, 0xA2AE, CNSPLANE , 0x2259) /*       0xA1F8-0xA2AE <-> 0x2259-0x234E */
B5TOCNS_CELL_BEGIN_END( 0xA2AF, 0xA3BF, CNSPLANE , 0x2421) /*       0xA2AF-0xA3BF <-> 0x2421-0x2570 */
B5TOCNS_CELL_BEGIN_END( 0xA3C0, 0xA3E0, CNSPLANE , 0x4221) /*       0xA3C0-0xA3E0 <-> 0x4221-0x4241  */
/* A.2.  Big5 (ETen, IBM, and Microsoft version) correspondence*/
/*       to CNS 11643 Plane 1:*/
B5TOCNS_CELL_BEGIN_END( 0xA440, 0xACFD, CNSPLANE , 0x4421) /*       0xA440-0xACFD <-> 0x4421-0x5322 */
B5TOCNS_CELL_BEGIN    ( 0xACFE,         CNSPLANE , 0x5753) /*              0xACFE <-> 0x5753 */
B5TOCNS_CELL_BEGIN_END( 0xAD40, 0xAFCF, CNSPLANE , 0x5323) /*       0xAD40-0xAFCF <-> 0x5323-0x5752 */
B5TOCNS_CELL_BEGIN_END( 0xAFD0, 0xBBC7, CNSPLANE , 0x5754) /*       0xAFD0-0xBBC7 <-> 0x5754-0x6B4F */
B5TOCNS_CELL_BEGIN_END( 0xBBC8, 0xBE51, CNSPLANE , 0x6B51) /*       0xBBC8-0xBE51 <-> 0x6B51-0x6F5B */
B5TOCNS_CELL_BEGIN    ( 0xBE52,         CNSPLANE , 0x6B50) /*              0xBE52 <-> 0x6B50 */
B5TOCNS_CELL_BEGIN_END( 0xBE53, 0xC1AA, CNSPLANE , 0x6F5C) /*       0xBE53-0xC1AA <-> 0x6F5C-0x7534 */
B5TOCNS_CELL_BEGIN_END( 0xC1AB, 0xC2CA, CNSPLANE , 0x7536) /*       0xC1AB-0xC2CA <-> 0x7536-0x7736 */
B5TOCNS_CELL_BEGIN    ( 0xC2CB,         CNSPLANE , 0x7535) /*              0xC2CB <-> 0x7535 */
B5TOCNS_CELL_BEGIN_END( 0xC2CC, 0xC360, CNSPLANE , 0x7737) /*       0xC2CC-0xC360 <-> 0x7737-0x782C */
B5TOCNS_CELL_BEGIN_END( 0xC361, 0xC3B8, CNSPLANE , 0x782E) /*       0xC361-0xC3B8 <-> 0x782E-0x7863 */
B5TOCNS_CELL_BEGIN    ( 0xC3B9,         CNSPLANE , 0x7865) /*              0xC3B9 <-> 0x7865 */
B5TOCNS_CELL_BEGIN    ( 0xC3BA,         CNSPLANE , 0x7864) /*              0xC3BA <-> 0x7864 */
B5TOCNS_CELL_BEGIN_END( 0xC3BB, 0xC455, CNSPLANE , 0x7866) /*       0xC3BB-0xC455 <-> 0x7866-0x7961 */
B5TOCNS_CELL_BEGIN    ( 0xC456,         CNSPLANE , 0x782D) /*              0xC456 <-> 0x782D */
B5TOCNS_CELL_BEGIN_END( 0xC457, 0xC67E, CNSPLANE , 0x7962) /*       0xC457-0xC67E <-> 0x7962-0x7D4B */
/* A.4.  Big5 (ETen and IBM Version) specific numeric symbols*/
/*       correspondence to CNS 11643 Plane 1: (Microsoft version defined*/
/*       this area as UDC - User Defined Character)*/
B5TOCNS_CELL_BEGIN_END( 0xC6A1, 0xC6BE, CNSPLANE , 0x2621) /*       0xC6A1-0xC6BE <-> 0x2621-0x263E */
/* A.5.  Big5 (ETen and IBM Version) specific KangXi radicals*/
/*       correspondence to CNS 11643 Plane 1: (Microsoft version defined as*/
/*       UDC - User Definable Character)*/
B5TOCNS_CELL_BEGIN    ( 0xC6BF,         CNSPLANE , 0x2723) /*              0xC6BF <-> 0x2723 */
B5TOCNS_CELL_BEGIN    ( 0xC6C0,         CNSPLANE , 0x2724) /*              0xC6C0 <-> 0x2724 */
B5TOCNS_CELL_BEGIN    ( 0xC6C1,         CNSPLANE , 0x2726) /*              0xC6C1 <-> 0x2726 */
B5TOCNS_CELL_BEGIN    ( 0xC6C2,         CNSPLANE , 0x2728) /*              0xC6C2 <-> 0x2728 */
B5TOCNS_CELL_BEGIN    ( 0xC6C3,         CNSPLANE , 0x272D) /*              0xC6C3 <-> 0x272D */
B5TOCNS_CELL_BEGIN    ( 0xC6C4,         CNSPLANE , 0x272E) /*              0xC6C4 <-> 0x272E */
B5TOCNS_CELL_BEGIN    ( 0xC6C5,         CNSPLANE , 0x272F) /*              0xC6C5 <-> 0x272F */
B5TOCNS_CELL_BEGIN    ( 0xC6C6,         CNSPLANE , 0x2734) /*              0xC6C6 <-> 0x2734 */
B5TOCNS_CELL_BEGIN    ( 0xC6C7,         CNSPLANE , 0x2737) /*              0xC6C7 <-> 0x2737 */
B5TOCNS_CELL_BEGIN    ( 0xC6C8,         CNSPLANE , 0x273A) /*              0xC6C8 <-> 0x273A */
B5TOCNS_CELL_BEGIN    ( 0xC6C9,         CNSPLANE , 0x273C) /*              0xC6C9 <-> 0x273C */
B5TOCNS_CELL_BEGIN    ( 0xC6CA,         CNSPLANE , 0x2742) /*              0xC6CA <-> 0x2742 */
B5TOCNS_CELL_BEGIN    ( 0xC6CB,         CNSPLANE , 0x2747) /*              0xC6CB <-> 0x2747 */
B5TOCNS_CELL_BEGIN    ( 0xC6CC,         CNSPLANE , 0x274E) /*              0xC6CC <-> 0x274E */
B5TOCNS_CELL_BEGIN    ( 0xC6CD,         CNSPLANE , 0x2753) /*              0xC6CD <-> 0x2753 */
B5TOCNS_CELL_BEGIN    ( 0xC6CE,         CNSPLANE , 0x2754) /*              0xC6CE <-> 0x2754 */
B5TOCNS_CELL_BEGIN    ( 0xC6CF,         CNSPLANE , 0x2755) /*              0xC6CF <-> 0x2755 */
B5TOCNS_CELL_BEGIN    ( 0xC6D0,         CNSPLANE , 0x2759) /*              0xC6D0 <-> 0x2759 */
B5TOCNS_CELL_BEGIN    ( 0xC6D1,         CNSPLANE , 0x275A) /*              0xC6D1 <-> 0x275A */
B5TOCNS_CELL_BEGIN    ( 0xC6D2,         CNSPLANE , 0x2761) /*              0xC6D2 <-> 0x2761 */
B5TOCNS_CELL_BEGIN    ( 0xC6D3,         CNSPLANE , 0x2766) /*              0xC6D3 <-> 0x2766 */
B5TOCNS_CELL_BEGIN    ( 0xC6D4,         CNSPLANE , 0x2829) /*              0xC6D4 <-> 0x2829 */
B5TOCNS_CELL_BEGIN    ( 0xC6D5,         CNSPLANE , 0x282A) /*              0xC6D5 <-> 0x282A */
B5TOCNS_CELL_BEGIN    ( 0xC6D6,         CNSPLANE , 0x2863) /*              0xC6D6 <-> 0x2863 */
B5TOCNS_CELL_BEGIN    ( 0xC6D7,         CNSPLANE , 0x286C) /*              0xC6D7 <-> 0x286C */
#undef CNSPLANE

#define CNSPLANE 4
/* A.7.  Big5 (ETen version only) specific symbols correspondence to CNS*/
/*       11643 Plane 4:*/
B5TOCNS_CELL_BEGIN    ( 0xC879,         CNSPLANE , 0x2123) /*              0xC879 <-> 0x2123 */
B5TOCNS_CELL_BEGIN    ( 0xC87B,         CNSPLANE , 0x2124) /*              0xC87B <-> 0x2124 */
B5TOCNS_CELL_BEGIN    ( 0xC87D,         CNSPLANE , 0x212A) /*              0xC87D <-> 0x212A */
B5TOCNS_CELL_BEGIN    ( 0xC8A2,         CNSPLANE , 0x2152) /*              0xC8A2 <-> 0x2152 */
#undef CNSPLANE

#define CNSPLANE 2
/* A.3.  Big5 (ETen, IBM, and Microsoft version) Level 2 correspondence to*/
/*       CNS 11643-1992 Plane 2:*/
B5TOCNS_CELL_BEGIN_END( 0xC940, 0xC949, CNSPLANE , 0x2121) /*       0xC940-0xC949 <-> 0x2121-0x212A */
#undef CNSPLANE
#define CNSPLANE 1
B5TOCNS_CELL_BEGIN    ( 0xC94A,         CNSPLANE , 0x4442) /*              0xC94A <-> 0x4442       # duplicate of Level 1's 0xA461 */
#undef CNSPLANE
#define CNSPLANE 2
B5TOCNS_CELL_BEGIN_END( 0xC94B, 0xC96B, CNSPLANE , 0x212B) /*       0xC94B-0xC96B <-> 0x212B-0x214B */
B5TOCNS_CELL_BEGIN_END( 0xC96C, 0xC9BD, CNSPLANE , 0x214D) /*       0xC96C-0xC9BD <-> 0x214D-0x217C */
B5TOCNS_CELL_BEGIN    ( 0xC9BE,         CNSPLANE , 0x214C) /*              0xC9BE <-> 0x214C */
B5TOCNS_CELL_BEGIN_END( 0xC9BF, 0xC9EC, CNSPLANE , 0x217D) /*       0xC9BF-0xC9EC <-> 0x217D-0x224C */
B5TOCNS_CELL_BEGIN_END( 0xC9ED, 0xCAF6, CNSPLANE , 0x224E) /*       0xC9ED-0xCAF6 <-> 0x224E-0x2438 */
B5TOCNS_CELL_BEGIN    ( 0xCAF7,         CNSPLANE , 0x224D) /*              0xCAF7 <-> 0x224D */
B5TOCNS_CELL_BEGIN_END( 0xCAF8, 0xD779, CNSPLANE , 0x2439) /*       0xCAF8-0xD779 <-> 0x2439-0x387D */
B5TOCNS_CELL_BEGIN    ( 0xD77A,         CNSPLANE , 0x3F6A) /*              0xD77A <-> 0x3F6A */
B5TOCNS_CELL_BEGIN_END( 0xD77B, 0xDBA6, CNSPLANE , 0x387E) /*       0xD77B-0xDBA6 <-> 0x387E-0x3F69 */
B5TOCNS_CELL_BEGIN_END( 0xDBA7, 0xDDFB, CNSPLANE , 0x3F6B) /*       0xDBA7-0xDDFB <-> 0x3F6B-0x4423 */
B5TOCNS_CELL_BEGIN    ( 0xDDFC,         CNSPLANE , 0x4176) /*              0xDDFC <-> 0x4176         # duplicate of 0xDCD1 */
B5TOCNS_CELL_BEGIN_END( 0xDDFD, 0xE8A2, CNSPLANE , 0x4424) /*       0xDDFD-0xE8A2 <-> 0x4424-0x554A */
B5TOCNS_CELL_BEGIN_END( 0xE8A3, 0xE975, CNSPLANE , 0x554C) /*       0xE8A3-0xE975 <-> 0x554C-0x5721 */
B5TOCNS_CELL_BEGIN_END( 0xE976, 0xEB5A, CNSPLANE , 0x5723) /*       0xE976-0xEB5A <-> 0x5723-0x5A27 */
B5TOCNS_CELL_BEGIN_END( 0xEB5B, 0xEBF0, CNSPLANE , 0x5A29) /*       0xEB5B-0xEBF0 <-> 0x5A29-0x5B3E */
B5TOCNS_CELL_BEGIN    ( 0xEBF1,         CNSPLANE , 0x554B) /*              0xEBF1 <-> 0x554B */
B5TOCNS_CELL_BEGIN_END( 0xEBF2, 0xECDD, CNSPLANE , 0x5B3F) /*       0xEBF2-0xECDD <-> 0x5B3F-0x5C69 */
B5TOCNS_CELL_BEGIN    ( 0xECDE,         CNSPLANE , 0x5722) /*              0xECDE <-> 0x5722 */
B5TOCNS_CELL_BEGIN_END( 0xECDF, 0xEDA9, CNSPLANE , 0x5C6A) /*       0xECDF-0xEDA9 <-> 0x5C6A-0x5D73 */
B5TOCNS_CELL_BEGIN_END( 0xEDAA, 0xEEEA, CNSPLANE , 0x5D75) /*       0xEDAA-0xEEEA <-> 0x5D75-0x6038 */
B5TOCNS_CELL_BEGIN    ( 0xEEEB,         CNSPLANE , 0x642F) /*              0xEEEB <-> 0x642F */
B5TOCNS_CELL_BEGIN_END( 0xEEEC, 0xF055, CNSPLANE , 0x6039) /*       0xEEEC-0xF055 <-> 0x6039-0x6242 */
B5TOCNS_CELL_BEGIN    ( 0xF056,         CNSPLANE , 0x5D74) /*              0xF056 <-> 0x5D74 */
B5TOCNS_CELL_BEGIN_END( 0xF057, 0xF0CA, CNSPLANE , 0x6243) /*       0xF057-0xF0CA <-> 0x6243-0x6336 */
B5TOCNS_CELL_BEGIN    ( 0xF0CB,         CNSPLANE , 0x5A28) /*              0xF0CB <-> 0x5A28 */
B5TOCNS_CELL_BEGIN_END( 0xF0CC, 0xF162, CNSPLANE , 0x6337) /*       0xF0CC-0xF162 <-> 0x6337-0x642E */
B5TOCNS_CELL_BEGIN_END( 0xF163, 0xF16A, CNSPLANE , 0x6430) /*       0xF163-0xF16A <-> 0x6430-0x6437 */
B5TOCNS_CELL_BEGIN    ( 0xF16B,         CNSPLANE , 0x6761) /*              0xF16B <-> 0x6761 */
B5TOCNS_CELL_BEGIN_END( 0xF16C, 0xF267, CNSPLANE , 0x6438) /*       0xF16C-0xF267 <-> 0x6438-0x6572 */
B5TOCNS_CELL_BEGIN    ( 0xF268,         CNSPLANE , 0x6934) /*              0xF268 <-> 0x6934 */
B5TOCNS_CELL_BEGIN_END( 0xF269, 0xF2C2, CNSPLANE , 0x6573) /*       0xF269-0xF2C2 <-> 0x6573-0x664C */
B5TOCNS_CELL_BEGIN_END( 0xF2C3, 0xF374, CNSPLANE , 0x664E) /*       0xF2C3-0xF374 <-> 0x664E-0x6760 */
B5TOCNS_CELL_BEGIN_END( 0xF375, 0xF465, CNSPLANE , 0x6762) /*       0xF375-0xF465 <-> 0x6762-0x6933 */
B5TOCNS_CELL_BEGIN_END( 0xF466, 0xF4B4, CNSPLANE , 0x6935) /*       0xF466-0xF4B4 <-> 0x6935-0x6961 */
B5TOCNS_CELL_BEGIN    ( 0xF4B5,         CNSPLANE , 0x664D) /*              0xF4B5 <-> 0x664D */
B5TOCNS_CELL_BEGIN_END( 0xF4B6, 0xF4FC, CNSPLANE , 0x6962) /*       0xF4B6-0xF4FC <-> 0x6962-0x6A4A */
B5TOCNS_CELL_BEGIN_END( 0xF4FD, 0xF662, CNSPLANE , 0x6A4C) /*       0xF4FD-0xF662 <-> 0x6A4C-0x6C51 */
B5TOCNS_CELL_BEGIN    ( 0xF663,         CNSPLANE , 0x6A4B) /*              0xF663 <-> 0x6A4B */
B5TOCNS_CELL_BEGIN_END( 0xF664, 0xF976, CNSPLANE , 0x6C52) /*       0xF664-0xF976 <-> 0x6C52-0x7165 */
B5TOCNS_CELL_BEGIN_END( 0xF977, 0xF9C3, CNSPLANE , 0x7167) /*       0xF977-0xF9C3 <-> 0x7167-0x7233 */
B5TOCNS_CELL_BEGIN    ( 0xF9C4,         CNSPLANE , 0x7166) /*              0xF9C4 <-> 0x7166 */
B5TOCNS_CELL_BEGIN    ( 0xF9C5,         CNSPLANE , 0x7234) /*              0xF9C5 <-> 0x7234 */
B5TOCNS_CELL_BEGIN    ( 0xF9C6,         CNSPLANE , 0x7240) /*              0xF9C6 <-> 0x7240 */
B5TOCNS_CELL_BEGIN_END( 0xF9C7, 0xF9D1, CNSPLANE , 0x7235) /*       0xF9C7-0xF9D1 <-> 0x7235-0x723F */
B5TOCNS_CELL_BEGIN_END( 0xF9D2, 0xF9D5, CNSPLANE , 0x7241) /*       0xF9D2-0xF9D5 <-> 0x7241-0x7244 */
#undef CNSPLANE

#define CNSPLANE 3
/* A.6.  Big5 (ETen and Microsoft version) specific Ideographs*/
/*       correspondence to CNS 11643 Plane 3: (IBM version defined as UDC)*/
B5TOCNS_CELL_BEGIN    ( 0xF9D6,         CNSPLANE , 0x4337) /*              0xF9D6 <-> 0x4337 */
B5TOCNS_CELL_BEGIN    ( 0xF9D7,         CNSPLANE , 0x4F50) /*              0xF9D7 <-> 0x4F50 */
B5TOCNS_CELL_BEGIN    ( 0xF9D8,         CNSPLANE , 0x444E) /*              0xF9D8 <-> 0x444E */
B5TOCNS_CELL_BEGIN    ( 0xF9D9,         CNSPLANE , 0x504A) /*              0xF9D9 <-> 0x504A */
B5TOCNS_CELL_BEGIN    ( 0xF9DA,         CNSPLANE , 0x2C5D) /*              0xF9DA <-> 0x2C5D */
B5TOCNS_CELL_BEGIN    ( 0xF9DB,         CNSPLANE , 0x3D7E) /*              0xF9DB <-> 0x3D7E */
B5TOCNS_CELL_BEGIN    ( 0xF9DC,         CNSPLANE , 0x4B5C) /*              0xF9DC <-> 0x4B5C */
#undef CNSPLANE
};




/* ------------------------------------------------------------------------------------------ 
   CVTable 
 * ------------------------------------------------------------------------------------------ */

typedef unsigned char*   (*Fn_return_buf_and_len)   ( CCCDataObject obj, unsigned char* out, int32 outlen);
typedef unsigned char*   (*Fn_report_out_of_memory) ( CCCDataObject obj);
typedef void             (*Fn_restore_conv_state)   ( CCCDataObject obj, uint32* pst1, uint32* pst2);
typedef void             (*Fn_save_conv_state)      ( CCCDataObject obj, uint32 st1, uint32 st2);

typedef void*            (*Fn_state_create)         ();
typedef void             (*Fn_state_destroy)        (void* state);
typedef void             (*Fn_state_init)           (void* state, uint32 st1, uint32 st2);
typedef void             (*Fn_state_get_value)      (void* state, uint32 *pst1, uint32 *pst2);

typedef uint32            (*Fn_outsize)              (uint32 insize);
typedef XP_Bool          (*Fn_scan)                 (void *state, unsigned char  in);
typedef uint32           (*Fn_generate)             (void *state, unsigned char* out);

typedef struct {
  Fn_report_out_of_memory report_out_of_memory;
  Fn_return_buf_and_len   return_buf_and_len;
  Fn_restore_conv_state   restore_conv_state;
  Fn_save_conv_state      save_conv_state;

  Fn_state_create         state_create;
  Fn_state_destroy        state_destroy;
  Fn_state_init           state_init;
  Fn_state_get_value      state_get_value;

  Fn_outsize              outsize;
  Fn_scan                 scan;
  Fn_generate             generate;
} CVTable;
/* ------------------------------------------------------------------------------------------ */
PRIVATE unsigned char* intl_fc_conv_generic( CCCDataObject obj, const unsigned char *in, int32 insize, CVTable *vtbl);

PRIVATE void* intl_b5cns_state_create         ();
PRIVATE void  intl_b5cns_state_destroy        (void* state);
PRIVATE void  intl_b5cns_state_init           (void* state, uint32 st1, uint32 st2);
PRIVATE void  intl_b5cns_state_get_value      (void* state, uint32 *pst1, uint32 *pst2);

PRIVATE XP_Bool intl_big5_scan             (void *state, unsigned char  in);
PRIVATE uint32  intl_big5_euctw_generate   (void *state, unsigned char* out);

PRIVATE XP_Bool intl_euctw_scan            (void *state, unsigned char  in);
PRIVATE uint32  intl_euctw_big5_generate   (void *state, unsigned char* out);

/* ------------------------------------------------------------------------------------------ */
PRIVATE CVTable intl_euctw_b5_vtbl = 
{
  &intl_fc_report_out_of_memory,
  &intl_fc_return_buf_and_len,
  &intl_fc_restore_conv_state,
  &intl_fc_save_conv_state,

  &intl_b5cns_state_create,
  &intl_b5cns_state_destroy,
  &intl_b5cns_state_init,
  &intl_b5cns_state_get_value,

  &intl_same_outsize,
  &intl_euctw_scan,
  &intl_euctw_big5_generate,
};
/* ------------------------------------------------------------------------------------------ */
PRIVATE CVTable intl_b5_euctw_vtbl = 
{
  &intl_fc_report_out_of_memory,
  &intl_fc_return_buf_and_len,
  &intl_fc_restore_conv_state,
  &intl_fc_save_conv_state,

  &intl_b5cns_state_create,
  &intl_b5cns_state_destroy,
  &intl_b5cns_state_init,
  &intl_b5cns_state_get_value,

  &intl_x2_outsize,
  &intl_big5_scan,
  &intl_big5_euctw_generate,
};
/* ------------------------------------------------------------------------------------------ */
typedef enum {
  kError = -1,
  kDone = 0,
  k1MoreBytes,
  k2MoreBytes,
  k3MoreBytes
} parseStatus;
/* ------------------------------------------------------------------------------------------ */
typedef struct {
  parseStatus status;
  uint8 num_byte;
  unsigned char bytes[4];
} B5CNSstate;
/* ------------------------------------------------------------------------------------------ */
PRIVATE void* intl_b5cns_state_create()
{
  return XP_ALLOC(sizeof(B5CNSstate));
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE void  intl_b5cns_state_destroy(void* state) 
{
  XP_FREE(state);
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE void  intl_b5cns_state_init(void* state, uint32 st1, uint32 st2) 
{
  B5CNSstate* obj = (B5CNSstate*)state;
  obj->status =   (parseStatus)((st1 >> 8) & 0x00FF);
  obj->num_byte = (uint8)(st1) & 0x00FF;
  obj->bytes[0] = (unsigned char)(st2      ) & 0x00FF;
  obj->bytes[1] = (unsigned char)(st2 >>  8) & 0x00FF;
  obj->bytes[2] = (unsigned char)(st2 >> 16) & 0x00FF;
  obj->bytes[3] = (unsigned char)(st2 >> 24) & 0x00FF;
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE void  intl_b5cns_state_get_value(void* state, uint32 *pst1, uint32 *pst2) 
{
  B5CNSstate* obj = (B5CNSstate*)state;
  *pst1 =  (obj->status << 8) |  obj->num_byte;
  *pst2 =  obj->bytes[0] | (obj->bytes[1] << 8) | (obj->bytes[2] << 16) | (obj->bytes[3] << 24) ;
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE XP_Bool intl_big5_scan(void *state, unsigned char  in)
{
  B5CNSstate* obj = (B5CNSstate*)state;
  obj->bytes[obj->num_byte++] = in;

  XP_ASSERT( ( kDone == obj->status ) || ( k1MoreBytes == obj->status ));
  switch( obj->status )
  {
    case kDone:
      if((0x00FE >= in) && ( in >= 0x00A1))
      {
        obj->status = k1MoreBytes;
        return FALSE;
      } else {
 	    XP_ASSERT( ( in <= 0x7F ) || (0xA0 == in));	   /* legal single byte range + hacky 0xa0 */
        obj->status = kDone; 
        return TRUE;
      }

    case k1MoreBytes:
	  XP_ASSERT( ((0x0040 <= in) && ( in <= 0x007E)) ||
				 ((0x00A1 <= in) && ( in <= 0x00FE)));  /* legal 2nd byte */
      obj->status = kDone; 
      return TRUE;

    default:
      XP_ASSERT(0);
      obj->num_byte = 0;    /* reset */
      obj->status = kDone;  /* reset */
      return FALSE;
  }
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE uint32  intl_big5_euctw_generate(void *state, unsigned char* out)
{
  B5CNSstate* obj = (B5CNSstate*)state;
  uint8 n = obj->num_byte;
  obj->num_byte = 0; 
  XP_ASSERT( ( 1 == n) || ( 2 == n ));
  switch(n)
  {
    case 1:
      *out = obj->bytes[0];
      break;

    case 2:
      if(((0x007E >= obj->bytes[1]) && ( obj->bytes[1] >= 0x0040)) ||
         ((0x00FE >= obj->bytes[1]) && ( obj->bytes[1] >= 0x00A1))
        )
      {
         uint16 cns;
         uint8  plane;
         uint16 big5 = (obj->bytes[0] << 8) | (obj->bytes[1]);
         if(intl_convbig5tocns(big5, &cns, &plane)) 
         {
           XP_ASSERT(plane > 0);
           XP_ASSERT(plane <= 16);
           if(plane > 1)
           {
             *out++ = (unsigned char)0x8E;
             *out++ = (unsigned char)(((unsigned char)0xA0) + plane);
             n = 4;
           } else {
             n = 2;
           }
           *out++ = (unsigned char)((cns >> 8) & 0xFF) | 0x80;
           *out   = (unsigned char)(cns & 0x00FF) | 0x80;
           return n;
         }
      } 
      XP_ASSERT(0);
      *out++ = '?'; /* error handling */
      *out = '?'; /* error handling */
      return 2;

    default:
      XP_ASSERT(0);
      n = 0;
      break;
  }
  return n;
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE XP_Bool intl_euctw_scan(void *state, unsigned char  in)
{
  B5CNSstate* obj = (B5CNSstate*)state;
  obj->bytes[obj->num_byte++] = in;
  XP_ASSERT( ( k1MoreBytes == obj->status ) || 
	         ( k2MoreBytes == obj->status ) || 
 	         ( k3MoreBytes == obj->status ) || 
	         ( kDone == obj->status ));
  switch( obj->status )
  {
    case kDone:
      if((0x00FE >= in) && ( in >= 0x00A1))
      {
        obj->status = k1MoreBytes;
        return FALSE;
      } else {
        if(0x008E == in)
        {
          obj->status = k3MoreBytes; 
          return FALSE;
        } else {
		  XP_ASSERT( ( in <= 0x7F ) || (0xA0 == in));	   /* legal single byte range + hacky 0xa0 */
          obj->status = kDone; 
          return TRUE;
        }
      }

    case k3MoreBytes:
	  XP_ASSERT( (0x00B0 >= in) && ( in >= 0x00A1));	   /* legal plane byte */
	  /* no break; here */
    case k1MoreBytes:
    case k2MoreBytes:
	  XP_ASSERT( (0x00FE >= in) && ( in >= 0x00A1));	   /* legal 2-3 bytes*/
      obj->status--;
      if(kDone == obj->status)
        return TRUE;
      else
        return FALSE;

    default:
      XP_ASSERT(0);
      obj->num_byte = 0;    /* reset */
      obj->status = kDone;  /* reset */
      return FALSE;
  }
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE uint32  intl_euctw_big5_generate(void *state, unsigned char* out)
{
  B5CNSstate* obj = (B5CNSstate*)state;
  uint8 n = obj->num_byte;
  unsigned char hb, lb;
  obj->num_byte = 0; 
  XP_ASSERT( ( 1 == n) || ( 2 == n ) || ( 4 == n ));
  switch(n)
  {
    case 1:
      *out = obj->bytes[0];
      break;

    case 2:
      hb = obj->bytes[0];
      lb = obj->bytes[1];
      if((0x00FE >= lb) && ( lb >= 0x00A1))
      {
        uint16 big5;
        uint16 cns = ((hb & 0x7F) << 8) | (lb & 0x7F);
        if(intl_convcnstobig5(cns, 1, &big5)) 
        {
          *out++ = (unsigned char)((big5 >> 8) & 0xFF);
          *out   = (unsigned char)(big5 & 0x00FF);
          return 2;
        }
		else
		{
			/* do not assert if it failed into some range we know there are no conversion */
			XP_ASSERT(( 0x7D == (hb & 0x7f) ) ||
				      ( 0x7E == (hb & 0x7f) ) ||
				      ( (0x27 <= (hb & 0x7f)) && ((hb & 0x7f) <= 0x43)) 
				);
		}
      } 
	  else
	  {
		 XP_ASSERT(0);
	  }
      *out++ = '?'; /* error handling */
      *out   = '?'; /* error handling */
      n = 2;
      break;

    case 4:
      hb = obj->bytes[2];
      lb = obj->bytes[3];
      if(((0x00FE >= hb) && ( hb >= 0x00A1)) &&  
         ((0x00FE >= lb) && ( lb >= 0x00A1)) && 
         ((0x00B0 >= obj->bytes[1]) && ( obj->bytes[1] >= 0x00A1))
        )
      {
        uint16 big5;
        uint8 plane = obj->bytes[1] - 0xA0;
        uint16 cns = ((hb & 0x7F) << 8) | (lb & 0x7F);
        if(intl_convcnstobig5(cns, plane, &big5)) 
        {
          *out++ = (unsigned char)((big5 >> 8) & 0xFF);
          *out   = (unsigned char)(big5 & 0x00FF);
          return 2;
        }
		else
		{
			/* do not assert if it failed into some range we know there are no conversion */
			XP_ASSERT(( 0x72 <= (hb & 0x7f) ) && ( 2 == plane));
		}
      }
	  else
	  {
		 XP_ASSERT(0);
	  }
      *out++ = '?'; /* error handling */
      *out   = '?'; /* error handling */
      n = 2;
      break;

    default:
      XP_ASSERT(0);
      n = 0;
      break;
  }
  return n;
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE XP_Bool intl_convbig5tocns(uint16 big5, uint16* cns, uint8* cnsplane)
{
  uint16 big5seq = BIG5TOSEQ(big5);
  uint32 l,m,r;

  /* do binary search to the table */
  l=0;
  r= ((sizeof(big5_map) / sizeof(cnstobig5))-1);
  m= (l+r)/2; ;  

  while(1)
  {
    if( (big5_map[m].big5fromseq <= big5seq ) && (big5seq <= big5_map[m].big5toseq ) )
    {
      uint16 cnsseq = big5_map[m].cnsfromseq + big5seq - big5_map[m].big5fromseq;
      *cns = SEQTOCNS(cnsseq);
      *cnsplane = big5_map[m].cnsplane;
      return TRUE;
    }

    if( (l > m) || ( m > r))
      return FALSE;

    if( big5seq <= big5_map[m].big5fromseq ) 
    {
      r = m - 1;
    } else {
      l = m + 1;
    }
    m = (l+r) >> 1;
  }
  return FALSE;
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE XP_Bool intl_convcnstobig5(uint16 cns, uint8 cnsplane, uint16* big5)
{
  uint16 cnsseq = CNSTOSEQ(cns);
  cnstobig5 *map;
  uint32 l,m,r;

  if((cnsplane > 4) || (cnsplane <= 0 ))
    return FALSE;

  map = cns_map[cnsplane - 1];

  /* do binary search to the table */
  l=0;
  r= cns_map_size[cnsplane -1] - 1;
  m= (l+r)/2; ;  

  while(1)
  {
    if( (map[m].cnsfromseq <= cnsseq ) && (cnsseq <= map[m].cnstoseq ) )
    {
      uint16 big5seq = map[m].big5fromseq + cnsseq - map[m].cnsfromseq;
      *big5 = SEQTOBIG5(big5seq);
      return TRUE;
    }

    if( (l > m) || ( m > r))
      return FALSE;

    if( cnsseq <= map[m].cnsfromseq ) 
    {
      r = m - 1;
    } else {
      l = m + 1;
    }
    m = (l+r) >> 1;
  }
  
  return FALSE;
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE uint32 intl_same_outsize(uint32 insize)
{
  return insize + 1;
}
/* ------------------------------------------------------------------------------------------ */
PRIVATE uint32 intl_x2_outsize(uint32 insize)
{
  return 2 * insize + 1;
}

/* -------------------------------------------------------------------- */
PRIVATE void intl_fc_restore_conv_state( CCCDataObject obj, uint32 *st1, uint32 *st2) 
{

   *st1 = INTL_GetCCCJismode(obj);
   *st2 = INTL_GetCCCCvtflag(obj);
}
/* -------------------------------------------------------------------- */
PRIVATE void intl_fc_save_conv_state( CCCDataObject obj, uint32 st1, uint32 st2)
{
   INTL_SetCCCJismode(obj, st1);
   INTL_SetCCCCvtflag(obj, st2);
}
/* -------------------------------------------------------------------- */
PRIVATE unsigned char* intl_fc_report_out_of_memory(CCCDataObject obj)
{
     INTL_SetCCCRetval( obj, MK_OUT_OF_MEMORY );
     return NULL;
}
/* -------------------------------------------------------------------- */
PRIVATE unsigned char* intl_fc_return_buf_and_len( CCCDataObject obj, unsigned char *out, int32 outlen)
{
   XP_ASSERT(out);
   XP_ASSERT(outlen >= 0);

   INTL_SetCCCLen(obj, outlen);

   return out;
}
/* -------------------------------------------------------------------- */
PRIVATE unsigned char* 
intl_fc_conv_generic( CCCDataObject obj, const unsigned char *in,  int32 insize, CVTable* vtbl)
{

   char unsigned *outbuf;
   char unsigned *out;
   int32 i;
   int32 outlen = 0;
   int32 outalloclen = 0;
   void *state;

   XP_ASSERT(insize >= 0);
   XP_ASSERT(in);

   /* copy the state information from CCCDataObject into state */	
   {  /* 
         We need to do all this because currently CCCDataObject
         static init itself in the table and there are no destroy routine
         Once we remove those bad OO code we could just use 
         state as the object instead of copy date between
         CCCDataObject and sate 
      */
      uint32 st1, st2;
      vtbl->restore_conv_state(obj, &st1, &st2);
      state = vtbl->state_create();
      XP_ASSERT(NULL != state);
      vtbl->state_init(state, st1, st2);
   }

   /* Try to allocate memory */
   outalloclen  =  vtbl->outsize(insize); 
   XP_ASSERT(outalloclen >= 0);

   outbuf  =  (unsigned char*) XP_ALLOC( outalloclen ); 

   /* in case we cannot allocate memory */
   XP_ASSERT( NULL != outbuf ); 
   if((unsigned char*) NULL == outbuf )
     return vtbl->report_out_of_memory(obj); 

   for(out=outbuf, i = 0 ; i < insize ; i++, in++)
   {
      int32 genlen;
      if( vtbl->scan(state, *in ))
      {
         genlen = vtbl->generate(state, out);
     
         XP_ASSERT( genlen >= 0 );

         out += genlen;
         outlen += genlen;
 
         /* vtbl->outsize should ensure us that this is always true */
         XP_ASSERT(outlen <= outalloclen); 
      }
   }
   *out = '\0';  /* null terminate */

   {
      uint32 st1, st2;
      vtbl->state_get_value(state, &st1, &st2);
      vtbl->save_conv_state(obj, st1, st2);
      vtbl->state_destroy(state);
   }

   return vtbl->return_buf_and_len(obj, outbuf, out - outbuf);
}
/* -------------------------------------------------------------------- */
MODULE_PRIVATE unsigned char* 
mz_euctwtob5( CCCDataObject obj, const unsigned char *in,  int32 insize)
{
  return intl_fc_conv_generic(obj, in, insize, &intl_euctw_b5_vtbl);
}
/* -------------------------------------------------------------------- */
MODULE_PRIVATE unsigned char* 
mz_b5toeuctw( CCCDataObject obj, const unsigned char *in,  int32 insize)
{
  return intl_fc_conv_generic(obj, in, insize, &intl_b5_euctw_vtbl);
}
