/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "Fundamentals.h"
#include "HPPAInstruction.h"
#include "HPPAFuncs.h"
#include "ControlNodes.h"

HPPAInstructionInfo hiInfo[nHPPAInstructionKind] =
{
  { ADD,     0x02, 0x18,  6, "add",     HPPA_NONE }, // *****
  { ADDBF,   0xff, 0x00,  0, "addbf",   HPPA_UNIMP },
  { ADDBT,   0xff, 0x00,  0, "addbt",   HPPA_UNIMP },
  { ADDO,    0xff, 0x00,  0, "addo",    HPPA_UNIMP },
  { ADDIBF,  0xff, 0x00,  0, "addibf",  HPPA_UNIMP },
  { ADDIBT,  0xff, 0x00,  0, "addibt",  HPPA_UNIMP },
  { ADDIL,   0xff, 0x00,  0, "addil",   HPPA_UNIMP },
  { ADDL,	 0x02, 0x28,  6, "addl",    HPPA_NONE }, // *****
  { ADDI,	 0xff, 0x00,  0, "addi",    HPPA_UNIMP },
  { ADDIT,   0xff, 0x00,  0, "addit",   HPPA_UNIMP },
  { ADDITO,  0xff, 0x00,  0, "addito",  HPPA_UNIMP },
  { ADDC,    0xff, 0x00,  0, "addc",    HPPA_UNIMP },
  { ADDCO,   0xff, 0x00,  0, "addco",   HPPA_UNIMP },
  { AND,     0x02, 0x08,  6, "and",     HPPA_NONE }, // *****
  { ANDCM,   0xff, 0x00,  0, "andcm",   HPPA_UNIMP },
  { BL,      0x3a, 0x00, 13, "bl",      HPPA_NONE }, // *****
  { BLE,     0xff, 0x00,  0, "ble",     HPPA_UNIMP },
  { BLR,     0xff, 0x00,  0, "blr",     HPPA_UNIMP },
  { BE,      0xff, 0x00,  0, "be",      HPPA_UNIMP },
  { BB,      0xff, 0x00,  0, "bb",      HPPA_UNIMP },
  { BVB,     0xff, 0x00,  0, "bvb",     HPPA_UNIMP },
  { BV,      0xff, 0x00,  0, "bv",      HPPA_UNIMP },
  { BREAK,   0xff, 0x00,  0, "break",   HPPA_UNIMP },
  { COMBF,   0x22, 0x00, 11, "combf",   HPPA_NONE }, // *****
  { COMBT,   0x20, 0x00, 11, "combt",   HPPA_NONE }, // *****
  { COMCLR,  0xff, 0x00,  0, "comclr",  HPPA_UNIMP },
  { COMIBF,  0x23, 0x00,  0, "comibf",  HPPA_DATA_HAS_IM5 }, // *****
  { COMIBT,  0x21, 0x00, 11, "comibt",  HPPA_DATA_HAS_IM5 }, // *****
  { COMICLR, 0xff, 0x00,  0, "comiclr", HPPA_UNIMP },
  { CLDDX,   0xff, 0x00,  0, "clddx",   HPPA_UNIMP },
  { CLDDS,   0xff, 0x00,  0, "cldds",   HPPA_UNIMP },
  { CLDWX,   0xff, 0x00,  0, "cldwx",   HPPA_UNIMP },
  { CLDWS,   0xff, 0x00,  0, "cldws",   HPPA_UNIMP },
  { COPR,    0xff, 0x00,  0, "copr",    HPPA_UNIMP },
  { CSTDX,   0xff, 0x00,  0, "cstdx",   HPPA_UNIMP },
  { CSTDS,   0xff, 0x00,  0, "cstds",   HPPA_UNIMP },
  { CSTWX,   0xff, 0x00,  0, "cstwx",   HPPA_UNIMP },
  { CSTWS,   0xff, 0x00,  0, "cstws",   HPPA_UNIMP },
  { COPY,    0x02, 0x09,  6, "copy",    HPPA_R1_IS_ZERO | HPPA_IS_COPY }, // *****
  { DCOR,    0xff, 0x00,  0, "dcor",    HPPA_UNIMP },
  { DEP,     0xff, 0x00,  0, "dep",     HPPA_UNIMP },
  { DEPI,    0x35, 0x07,  9, "depi",    HPPA_DATA_HAS_IM5 }, // *****
  { DIAG,    0xff, 0x00,  0, "diag",    HPPA_UNIMP },
  { DS,      0xff, 0x00,  0, "ds",      HPPA_UNIMP },
  { XOR,     0x02, 0x0a,  6, "xor",     HPPA_NONE }, // ****
  { EXTRS,   0x34, 0x07,  8, "extrs",   HPPA_NONE }, // *****
  { EXTRU,   0x34, 0x06,  8, "extru",   HPPA_NONE }, // *****
  { XMPYU,   0x0e, 0x00, 40, "xmpyu",   HPPA_NONE },
  { FABS,    0xff, 0x00,  0, "fabs",    HPPA_UNIMP },
  { FCMP,    0xff, 0x00,  0, "fcmp",    HPPA_UNIMP },
  { FCNVXF,  0xff, 0x00,  0, "fcnvxf",  HPPA_UNIMP },
  { FCNVFX,  0xff, 0x00,  0, "fcnvfx",  HPPA_UNIMP },
  { FCNVFXT, 0xff, 0x00,  0, "fcnvfxt", HPPA_UNIMP },
  { FCNVFF,  0xff, 0x00,  0, "fcnvff",  HPPA_UNIMP },
  { FDIV,    0xff, 0x00,  0, "fdiv",    HPPA_UNIMP },
  { FLDDX,   0xff, 0x00,  0, "flddx",   HPPA_UNIMP },
  { FLDDS,   0xff, 0x00,  0, "fldds",   HPPA_UNIMP },
  { FLDWX,   0xff, 0x00,  0, "fldwx",   HPPA_UNIMP },
  { FLDWS,   0x09, 0x00, 31, "fldws",   HPPA_DATA_HAS_IM5 }, // *****
  { FMPY,    0xff, 0x00,  0, "fmpy",    HPPA_UNIMP },
  { FMPYADD, 0xff, 0x00,  0, "fmpyadd", HPPA_UNIMP },
  { FMPYSUB, 0xff, 0x00,  0, "fmpysub", HPPA_UNIMP },
  { FRND,    0xff, 0x00,  0, "frnd",    HPPA_UNIMP },
  { FSQRT,   0xff, 0x00,  0, "fsqrt",   HPPA_UNIMP },
  { FSTDX,   0xff, 0x00,  0, "fstdx",   HPPA_UNIMP },
  { FSTDS,   0xff, 0x00,  0, "fstds",   HPPA_UNIMP },
  { FSTWS,   0x09, 0x00, 32, "fstws",   HPPA_DATA_HAS_IM5 }, // *****
  { FSUB,    0xff, 0x00,  0, "fsub",    HPPA_UNIMP },
  { FTEST,   0xff, 0x00,  0, "ftest",   HPPA_UNIMP },
  { FDC,     0xff, 0x00,  0, "fdc",     HPPA_UNIMP },
  { FDCE,    0xff, 0x00,  0, "fdce",    HPPA_UNIMP },
  { FIC,     0xff, 0x00,  0, "fic",     HPPA_UNIMP },
  { FICE,    0xff, 0x00,  0, "fice",    HPPA_UNIMP },
  { GATE,    0xff, 0x00,  0, "gate",    HPPA_UNIMP },
  { DEBUGID, 0xff, 0x00,  0, "debugid", HPPA_UNIMP },
  { OR,      0x02, 0x09,  6, "or",      HPPA_NONE }, // ***** 
  { IDTLBA,  0xff, 0x00,  0, "idtlba",  HPPA_UNIMP },
  { IDTLBP,  0xff, 0x00,  0, "idtlbp",  HPPA_UNIMP },
  { IITLBA,  0xff, 0x00,  0, "iitlba",  HPPA_UNIMP },
  { IITLBP,  0xff, 0x00,  0, "iitlbp",  HPPA_UNIMP },
  { IDCOR,   0xff, 0x00,  0, "idcor",   HPPA_UNIMP },
  { LDCWX,   0xff, 0x00,  0, "ldcwx",   HPPA_UNIMP },
  { LDCWS,   0xff, 0x00,  0, "ldcws",   HPPA_UNIMP },
  { LDB,     0xff, 0x00,  0, "ldb",     HPPA_UNIMP },
  { LDBX,    0xff, 0x00,  0, "ldbx",    HPPA_UNIMP },
  { LDBS,    0xff, 0x00,  0, "ldbs",    HPPA_UNIMP },
  { LCI,     0xff, 0x00,  0, "lci",     HPPA_UNIMP },
  { LDH,     0xff, 0x00,  0, "ldh",     HPPA_UNIMP },
  { LDHX,    0xff, 0x00,  0, "ldhx",    HPPA_UNIMP },
  { LDHS,    0xff, 0x00,  0, "ldhs",    HPPA_UNIMP },
  { LDI,     0x0d, 0x00,  1, "ldi",     HPPA_DATA_HAS_IM14 | HPPA_R1_IS_ZERO }, // ***** 
  { LDIL,    0x08, 0x00,  5, "ldil",    HPPA_DATA_HAS_IM21 }, // ***** 
  { LDO,     0x0d, 0x00,  1, "ldo",     HPPA_DATA_HAS_IM14 }, // ***** 
  { LPA,     0xff, 0x00,  0, "lpa",     HPPA_UNIMP },
  { LDSID,   0xff, 0x00,  0, "ldsid",   HPPA_UNIMP },
  { LDW,     0x12, 0x00,  1, "ldw",     HPPA_DATA_HAS_IM14 }, // ***** 
  { LDWAX,   0xff, 0x00,  0, "ldwax",   HPPA_UNIMP },
  { LDWAS,   0xff, 0x00,  0, "ldwas",   HPPA_UNIMP },
  { LDWM,    0xff, 0x00,  0, "ldwm",    HPPA_UNIMP },
  { LDWX,    0xff, 0x00,  0, "ldwx",    HPPA_UNIMP },
  { LDWS,    0xff, 0x00,  0, "ldws",    HPPA_UNIMP },
  { MOVB,    0xff, 0x00,  0, "movb",    HPPA_UNIMP },
  { MFCTL,   0xff, 0x00,  0, "mfctl",   HPPA_UNIMP },
  { MFDBAM,  0xff, 0x00,  0, "mfdbam",  HPPA_UNIMP },
  { MFDBAO,  0xff, 0x00,  0, "mfdbao",  HPPA_UNIMP },
  { MFIBAM,  0xff, 0x00,  0, "mfibam",  HPPA_UNIMP },
  { MFIBAO,  0xff, 0x00,  0, "mfibao",  HPPA_UNIMP },
  { MFSP,    0xff, 0x00,  0, "mfsp",    HPPA_UNIMP },
  { MOVIB,   0xff, 0x00,  0, "movib",   HPPA_UNIMP },
  { MCTL,    0xff, 0x00,  0, "mctl",    HPPA_UNIMP },
  { MTBAM,   0xff, 0x00,  0, "mtbam",   HPPA_UNIMP },
  { MTBAO,   0xff, 0x00,  0, "mtbao",   HPPA_UNIMP },
  { MTIBAM,  0xff, 0x00,  0, "mtibam",  HPPA_UNIMP },
  { MTIBAO,  0xff, 0x00,  0, "mtibao",  HPPA_UNIMP },
  { MTSP,    0xff, 0x00,  0, "mtsp",    HPPA_UNIMP },
  { MTSM,    0xff, 0x00,  0, "mtsm",    HPPA_UNIMP },
  { PMDIS,   0xff, 0x00,  0, "pmdis",   HPPA_UNIMP },
  { PMENB,   0xff, 0x00,  0, "pmenb",   HPPA_UNIMP },
  { PROBER,  0xff, 0x00,  0, "prober",  HPPA_UNIMP },
  { PROBERI, 0xff, 0x00,  0, "proberi", HPPA_UNIMP },
  { PROBEW,  0xff, 0x00,  0, "probew",  HPPA_UNIMP },
  { PROBEWI, 0xff, 0x00,  0, "probewi", HPPA_UNIMP },
  { PDC,     0xff, 0x00,  0, "pdc",     HPPA_UNIMP },
  { PDTLB,   0xff, 0x00,  0, "pdtlb",   HPPA_UNIMP },
  { PDTLBE,  0xff, 0x00,  0, "pdtlbe",  HPPA_UNIMP },
  { PITLB,   0xff, 0x00,  0, "pitlb",   HPPA_UNIMP },
  { PITLBE,  0xff, 0x00,  0, "pitlbe",  HPPA_UNIMP },
  { RSM,     0xff, 0x00,  0, "rsm",     HPPA_UNIMP },
  { RFI,     0xff, 0x00,  0, "rfi",     HPPA_UNIMP },
  { RFIR,    0xff, 0x00,  0, "rfir",    HPPA_UNIMP },
  { SSM,     0xff, 0x00,  0, "ssm",     HPPA_UNIMP },
  { SHD,     0xff, 0x00,  0, "shd",     HPPA_UNIMP },
  { SH1ADD,  0xff, 0x00,  0, "sh1add",  HPPA_UNIMP },
  { SH1ADDL, 0x02, 0x29,  6, "sh1addl", HPPA_NONE }, // ***** 
  { SH1ADDO, 0xff, 0x00,  0, "sh1addo", HPPA_UNIMP },
  { SH2ADD,  0xff, 0x00,  0, "sh2add",  HPPA_UNIMP },
  { SH2ADDL, 0x02, 0x2a,  6, "sh2addl", HPPA_NONE }, // ***** 
  { SH2ADDO, 0xff, 0x00,  0, "sh2addo", HPPA_UNIMP },
  { SH3ADD,  0xff, 0x00,  0, "sh3add",  HPPA_UNIMP },
  { SH3ADDL, 0x02, 0x2b,  6, "sh3addl", HPPA_NONE }, // ***** 
  { SH3ADDO, 0xff, 0x00,  0, "sh3addo", HPPA_UNIMP },
  { SPOP0,   0xff, 0x00,  0, "spop0",   HPPA_UNIMP },
  { SPOP1,   0xff, 0x00,  0, "spop1",   HPPA_UNIMP },
  { SPOP2,   0xff, 0x00,  0, "spop2",   HPPA_UNIMP },
  { SPOP3,   0xff, 0x00,  0, "spop3",   HPPA_UNIMP },
  { STB,     0xff, 0x00,  0, "stb",     HPPA_UNIMP },
  { STBS,    0xff, 0x00,  0, "stbs",    HPPA_UNIMP },
  { STBYS,   0xff, 0x00,  0, "stbys",   HPPA_UNIMP },
  { STH,     0xff, 0x00,  0, "sth",     HPPA_UNIMP },
  { STHS,    0xff, 0x00,  0, "sths",    HPPA_UNIMP },
  { STW,     0x1a, 0x00,  1, "stw",     HPPA_DATA_HAS_IM14 }, // *****
  { STWAS,   0xff, 0x00,  0, "stwas",   HPPA_UNIMP },
  { STWM,    0xff, 0x00,  0, "stwm",    HPPA_UNIMP },
  { STWS,    0xff, 0x00,  0, "stws",    HPPA_UNIMP },
  { SUB,     0x02, 0x10,  6, "sub",     HPPA_NONE }, // ***** 
  { SUBT,    0xff, 0x00,  0, "subt",    HPPA_UNIMP },
  { SUBTO,   0xff, 0x00,  0, "subto",   HPPA_UNIMP },
  { SUBO,    0xff, 0x00,  0, "subo",    HPPA_UNIMP },
  { SUBI,    0xff, 0x00,  0, "subi",    HPPA_UNIMP },
  { SUBIO,   0xff, 0x00,  0, "subio",   HPPA_UNIMP },
  { SUBB,    0xff, 0x00,  0, "subb",    HPPA_UNIMP },
  { SUBBO,   0xff, 0x00,  0, "subbo",   HPPA_UNIMP },
  { SYNC,    0xff, 0x00,  0, "sync",    HPPA_UNIMP },
  { SYNCDMA, 0xff, 0x00,  0, "syncdma", HPPA_UNIMP },
  { UADDCM,  0xff, 0x00,  0, "uaddcm",  HPPA_UNIMP },
  { UADDCMT, 0xff, 0x00,  0, "uaddcmt", HPPA_UNIMP },
  { UXOR,    0xff, 0x00,  0, "uxor",    HPPA_UNIMP },
  { VDEP,    0xff, 0x00,  0, "vdep",    HPPA_UNIMP },
  { VDEPI,   0xff, 0x00,  0, "vdepi",   HPPA_UNIMP },
  { VEXTRU,  0xff, 0x00,  0, "vextru",  HPPA_UNIMP },
  { VSHD,    0xff, 0x00,  0, "vshd",    HPPA_UNIMP },
  { ZDEP,    0x35, 0x02,  9, "zdep",    HPPA_R1_IS_ZERO }, // ***** 
  { ZDEPI,   0xff, 0x00,  0, "zdepi",   HPPA_UNIMP },
  { ZVDEP,   0xff, 0x00,  0, "zvdep",   HPPA_UNIMP },
  { ZVDEPI,  0xff, 0x00,  0, "zvdepi",  HPPA_UNIMP }
};

HPPAConditionInfo hcInfo[nHPPAConditionKind] =
{				   
  { hcNever,  hcNever,  0, false, ""    },
  { hcE,      hcE,      1, false, "="   },
  { hcL,      hcGe,     2, false, "<"   },
  { hcLe,     hcG,      3, false, "<="  },
  { hcLu,     hcGeu,    4, false, "<<"  },
  { hcLeu,    hcGu,     5, false, "<<=" },
  { hcOver,   hcOver,   6, false, "SV"  },
  { hcOdd,    hcOdd,    7, false, "OD"  },
  { hcAlways, hcAlways, 0, true,  "TR"  },
  { hcNe,     hcNe,     1, true,  "<>"  },
  { hcGe,     hcL,      2, true,  ">="  },
  { hcG,      hcLe,     3, true,  ">"   },
  { hcGeu,    hcLu,     4, true,  ">>=" },
  { hcGu,     hcLeu,    5, true,  ">>"  },
  { hcNOver,  hcNOver,  6, true,  "NSV" },
  { hcEven,   hcEven,   7, true,  "EV"  }
};

char* registerNumberToString[] =
{				   
  // General registers
  "0",      "%r1",    "%rp",    "%r3",    "%r4",    "%r5",    "%r6",    "%r7",
  "%r8",    "%r9",    "%r10",   "%r11",   "%r12",   "%r13",   "%r14",   "%r15",
  "%r16",   "%r17",   "%r18",   "%r19",   "%r20",   "%r21",   "%r22",   "%arg3",
  "%arg2",  "%arg1",  "%arg0",  "%dp",    "%ret0",  "%ret1",  "%sp",    "%r31",
  // Floating point registers
  "%fr0L",  "%fr0R",  "%fr1L",  "%fr1R",  "%fr2L",  "%fr2R",  "%fr3L",  "%fr3R",
  "%fr4L",  "%fr4R",  "%fr5L",  "%fr5R",  "%fr6L",  "%fr6R",  "%fr7L",  "%fr7R",
  "%fr8L",  "%fr8R",  "%fr9L",  "%fr9R",  "%fr10L", "%fr10R", "%fr11L", "%fr11R",
  "%fr12L", "%fr12R", "%fr13L", "%fr13R", "%fr14L", "%fr14R", "%fr15L", "%fr15R",
  "%fr16L", "%fr16R", "%fr17L", "%fr17R", "%fr18L", "%fr18R", "%fr19L", "%fr19R",
  "%fr20L", "%fr20R", "%fr21L", "%fr21R", "%fr22L", "%fr22R", "%fr23L", "%fr23R",
  "%fr24L", "%fr24R", "%fr25L", "%fr25R", "%fr26L", "%fr26R", "%fr27L", "%fr27R",
  "%fr28L", "%fr28R", "%fr29L", "%fr29R", "%fr30L", "%fr30R", "%fr31L", "%fr31R",
  // Other strings
  "%fr0",   "%fr1",   "%fr2",   "%fr3",   "%fr4",   "%fr5",   "%fr6",   "%fr7",
  "%fr8",   "%fr9",   "%fr10",  "%fr11",  "%fr12",  "%fr13",  "%fr14",  "%fr15",
  "%fr16",  "%fr17",  "%fr18",  "%fr19",  "%fr20",  "%fr21",  "%fr22",  "%fr23",
  "%fr24",  "%fr25",  "%fr26",  "%fr27",  "%fr28",  "%fr29",  "%fr30",  "%fr31",
};				   

// available caller saved registers:
//   %r19, %r20, %r21, %r22, %arg3, %arg2, %arg1, %arg0, %ret0, %ret1, %r31 
// available callee saved registers:
//   %r4, %r5, %r6, %r7, %r8, %r9, %r10, %r11, %r12, %r13, %r14, %r15, %r16, %r17, %r18

PRUint8 registerNumberToColor[] = 
{
   79,  77,  12,  13,  14,  15,  16,  17,
   18,  19,  20,  21,  22,  23,  24,  25,
   26,  27,  28,   0,   1,   2,   3,   4,
    5,   6,   7,   8,   9,  10,  78,  11
};

HPPARegisterNumber colorToRegisterNumber[] =
{
  // caller saved GR
/*0*/   r19,   r20,   r21,   r22,   arg3,  arg2,  arg1,  arg0,
/*8*/   dp,    ret0,  ret1,  r31,   rp,
  // callee saved GR
/*13*/  r3,    r4,    r5,    r6,    r7,    r8,    r9,    r10,
/*21*/  r11,   r12,   r13,   r14,   r15,   r16,   r17,   r18,
  // caller saved FPR
/*29*/  fr8L,  fr8R,  fr9L,  fr9R,  fr10L, fr10R, fr11L, fr11R,
/*37*/  fr22L, fr22R, fr23L, fr23R, fr24L, fr24R, fr25L, fr25R,
/*45*/  fr26L, fr26R, fr27L, fr27R, fr28L, fr28R, fr29L, fr29R,
/*53*/  fr30L, fr30R, fr31L, fr31R,
  // callee saved FPR
/*57*/  fr12L, fr12R, fr13L, fr13R, fr14L, fr14R, fr15L, fr15R,
/*65*/  fr16L, fr16R, fr17L, fr17R, fr18L, fr18R, fr19L, fr19R,
/*73*/  fr20L, fr20R, fr21L, fr21R,
  // special registers
/*77*/  r1,    sp,    zero
};

#define COLOR_MASK1(regNum) (1 << registerNumberToColor[regNum])
#define COLOR_MASK2(regNum) (1 << (registerNumberToColor[regNum] - 32))
#define COLOR_MASK3(regNum) (1 << (registerNumberToColor[regNum] - 64))

HPPASpecialCallInfo hscInfo[nSpecialCalls] =
{
  { RemI, "$$remI", 2, 1, 0, 0, COLOR_MASK3(r1), HPPAFuncAddress(HPPAremI) }, // inter with r1
  { RemU, "$$remU", 2, 1, 0, 0, COLOR_MASK3(r1), HPPAFuncAddress(HPPAremU) }, // inter with r1
  { DivI, "$$divI", 2, 1, 0, 0, COLOR_MASK3(r1), HPPAFuncAddress(HPPAdivI) }, // inter with r1
  { DivU, "$$divU", 2, 1, 0, 0, COLOR_MASK3(r1), HPPAFuncAddress(HPPAdivU) }, // inter with r1
};

HPPAInstructionKind shiftAddl[4] = 
{
  INVALID, SH1ADDL, SH2ADDL, SH3ADDL
};


static inline PRUint32
dis_assemble_3(PRUint32 x)
{
  return (((x & 4) >> 2) | ((x & 3) << 1)) & 7;
}

static inline void
dis_assemble_12(PRUint32 as12, PRUint32& x, PRUint32& y)
{
  y = (as12 & 0x800) >> 11;
  x = ((as12 & 0x3ff) << 1) | ((as12 & 0x400) >> 10);
}

static inline void
dis_assemble_17(PRUint32 as17, PRUint32& x, PRUint32& y, PRUint32& z)
{
  z = (as17 & 0x10000) >> 16;
  x = (as17 & 0x0f800) >> 11;
  y = (((as17 & 0x00400) >> 10) | ((as17 & 0x3ff) << 1)) & 0x7ff;
}

static inline PRUint32
dis_assemble_21 (PRUint32 as21)
{
  PRUint32 temp;

  temp = (as21 & 0x100000) >> 20;
  temp |= (as21 & 0x0ffe00) >> 8;
  temp |= (as21 & 0x000180) << 7;
  temp |= (as21 & 0x00007c) << 14;
  temp |= (as21 & 0x000003) << 12;

  return temp;
}

static inline PRUint16
low_sign_unext_14(PRUint16 x)
{
  return ((x & 0x1fff) << 1) | ((x & 0x2000) ? 1 : 0);
}

static inline PRUint8
low_sign_unext_5(PRUint8 x)
{
  return ((x & 0xf) << 1) | ((x & 0x10) ? 1 : 0);
}

// HPPAInstruction methods

#if defined(DEBUG)
void HPPAInstruction::
printPretty(FILE* f)
{
  fprintf(f, "unimp(%s)", hiInfo[kind].string);
}
#endif

HPPARegisterNumber HPPAInstruction::
getR1()
{
  InstructionUse* use = getInstructionUseBegin();
  InstructionUse* limit = getInstructionUseEnd();

  while (use < limit && !use->isVirtualRegister()) use++;
  
  if (use >= limit)
	return zero;
  else
	return udToRegisterNumber(use[0]);
}

HPPARegisterNumber HPPAInstruction::
getR2()
{
  InstructionUse* use = getInstructionUseBegin();
  InstructionUse* limit = getInstructionUseEnd();

  while (use < limit && !use->isVirtualRegister()) use++;

  if (use < &limit[-1] && use[1].isVirtualRegister())
	use++; // skip R1
  
  if (use >= limit || !use->isVirtualRegister())
	return zero;
  else
	return udToRegisterNumber(use[0]);
}

HPPARegisterNumber HPPAInstruction::
getT()
{
  InstructionDefine* def = getInstructionDefineBegin();
  InstructionDefine* limit = getInstructionDefineEnd();

  while (def < limit && !def->isVirtualRegister()) def++;
  
  if (def >= limit)
	return zero;
  else
	return udToRegisterNumber(def[0]);
}

// HPPAFormat1 methods
// |31               |25            |20            |15   |13                                      0|
// |-----------------|--------------|--------------|-----|-----------------------------------------|
// |       op        |       b      |      t/r     |  s  |                  im14                   |
// |-----------------|--------------|--------------|-----|-----------------------------------------|

void HPPAFormat1::
formatToMemory(void* where, uint32 /*offset*/)
{
  *(PRUint32 *)where = (hiInfo[kind].opcode << 26) | (getR1() << 21) | (getT() << 16) | low_sign_unext_14(getIm14());
}

#if defined(DEBUG)
void HPPAFormat1::
printPretty(FILE* f)
{
  if (flags & HPPA_R1_IS_ZERO)
	fprintf(f, "%s %d,%s", hiInfo[kind].string, (PRInt16) getIm14(), getTString());
  else
	fprintf(f, "%s %d(%s),%s", hiInfo[kind].string, (PRInt16) getIm14(), getR1String(), getTString());
}
#endif

#if defined(DEBUG)
void HPPALoad::
printPretty(FILE* f)
{
  fprintf(f, "%s %d(0,%s),%s", hiInfo[kind].string, (PRInt16) getIm14(), getR1String(), getTString());
}
#endif

void HPPAStore::
formatToMemory(void* where, uint32 /*offset*/)
{
  *(PRUint32 *)where = (hiInfo[kind].opcode << 26) | (getR1() << 21) | (getR2() << 16) | low_sign_unext_14(getIm14());
}

#if defined(DEBUG)
void HPPAStore::
printPretty(FILE* f)
{
  fprintf(f, "%s %s,%d(0,%s)", hiInfo[kind].string, getR2String(), (PRInt16) getIm14(), getR1String());
}
#endif

// HPPAFormat5 methods
// |31               |25            |20                                                           0|
// |-----------------|--------------|--------------------------------------------------------------|
// |       op        |     t/r      |                             im21                             |
// |-----------------|--------------|--------------------------------------------------------------|

void HPPAFormat5::
formatToMemory(void* where, uint32 /*offset*/)
{
  *(PRUint32 *)where = (hiInfo[kind].opcode << 26) | (getT() << 21) | dis_assemble_21(IM21(im21));
}

#if defined(DEBUG)
void HPPAFormat5::
printPretty(FILE* f) 
{
  fprintf(f, "%s %d,%s", hiInfo[kind].string, IM21(im21), getTString());
}
#endif

// HPPAFormat6 methods
// |31               |25            |20            |15      |12|11               |5 |4            0|
// |-----------------|--------------|--------------|--------|--|-----------------|--|--------------|
// |       op        |      r2      |      r1      |    c   | f|      ext6       |0 |       t      |
// |-----------------|--------------|--------------|--------|--|-----------------|--|--------------|

void HPPAFormat6::
formatToMemory(void* where, uint32 /*offset*/)
{
  *(PRUint32 *)where = (hiInfo[kind].opcode << 26) | (((flags & HPPA_R1_IS_ZERO) ? 0 : getR2()) << 21) | (getR1() << 16) | (hiInfo[kind].ext << 6) | getT();
}

#if defined(DEBUG)
void HPPAFormat6::
printPretty(FILE* f) 
{
  if (flags & HPPA_IS_COPY)
	fprintf(f, "%s %s,%s", hiInfo[kind].string, getR1String(), getTString());
  else if (flags & HPPA_R1_IS_ZERO)
	fprintf(f, "%s 0,%s,%s", hiInfo[kind].string, getR1String(), getTString());
  else
	fprintf(f, "%s %s,%s,%s", hiInfo[kind].string, getR1String(), getR2String(), getTString());
}
#endif


// HPPAFormat8 methods
// |31               |25            |20            |15      |12      |9             |4            0|
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|
// |       op        |       r      |       t      |    c   |  ext3  |       p      |     clen     |
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|

void HPPAFormat8::
formatToMemory(void* where, uint32 /*offset*/)
{
  *(PRUint32 *)where = (hiInfo[kind].opcode << 26) | (getR1() << 21) | (getT() << 16) | (hiInfo[kind].ext << 10) | (cp << 5) | (32 - clen);
}

#if defined(DEBUG)
void HPPAFormat8::
printPretty(FILE* f) 
{
  fprintf(f, "%s %s,%d,%d,%s", hiInfo[kind].string, getR1String(), cp, clen, getTString());
}
#endif

// HPPAFormat9 methods
// |31               |25            |20            |15      |12      |9             |4            0|
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|
// |       op        |       t      |     r/im5    |    c   |  ext3  |      cp      |     clen     |
// |-----------------|--------------|--------------|--------|--------|--------------|--------------|

void HPPAFormat9::
formatToMemory(void* where, uint32 /*offset*/)
{
  *(PRUint32 *)where = (hiInfo[kind].opcode << 26) | (getT() << 21) | ((validIM5(im5) ? low_sign_unext_5(im5) : (PRUint8) getR1()) << 16) | 
	(hiInfo[kind].ext << 10) | ((31 - cp) << 5) | (32 - clen);
}

#if defined(DEBUG)
void HPPAFormat9::
printPretty(FILE* f) 
{
  if (validIM5(im5))
	fprintf(f, "%s %d,%d,%d,%s", hiInfo[kind].string, IM5(im5), cp, clen, getTString());
  else
	fprintf(f, "%s %s,%d,%d,%s", hiInfo[kind].string, getR1String(), cp, clen, getTString());
}
#endif

// HPPAFormat11 methods
// |31               |25            |20            |15      |12                              | 1| 0|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|
// |       op        |     r2/p     |     r1/im5   |    c   |                w1              | n| w|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|

void HPPAFormat11::
formatToMemory(void* where, uint32 offset)
{
  PRUint32 w1, w;
  
  dis_assemble_12((target.getNativeOffset() - (offset + 8)) >> 2, w1, w);

  ((PRUint32 *)where)[0] = (hiInfo[kind].opcode << 26) | (getR2() << 21) | ((validIM5(im5) ? low_sign_unext_5(im5) : (PRUint8) getR1()) << 16) | 
	(hcInfo[cond].c << 13) | (w1 << 2) | ((nullification ? 1 : 0) << 1) | w;
  if (nullification)
  ((PRUint32 *)where)[1] = 0x08000240; // nop
}

#if defined(DEBUG)
void HPPAFormat11::
printPretty(FILE* f) 
{
  if (validIM5(im5))
	fprintf(f, "%s,%s%s %d,%s,N%d%s", hiInfo[kind].string, hcInfo[cond].string, nullification ? ",n" : "",
			IM5(im5), getR2String(), target.dfsNum, nullification ? "; nop" : "");
  else
	fprintf(f, "%s,%s%s %s,%s,N%d%s", hiInfo[kind].string, hcInfo[cond].string, nullification ? ",n" : "",
			getR1String(), getR2String(), target.dfsNum, nullification ? "; nop" : "");
}
#endif

// HPPAFormat12 methods
// |31               |25            |20            |15      |12                              | 1| 0|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|
// |       op        |      b       |      w1      |    s   |                w2              | n| w|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|

void HPPAFormat12::
formatToMemory(void* where, uint32 offset)
{
  PRUint32 w2, w1, w;
  
  dis_assemble_17((target.getNativeOffset() - (offset + 8)) >> 2, w1, w2, w);

  ((PRUint32 *)where)[0] = (hiInfo[kind].opcode << 26) | /*(getT() << 21) |*/  (w1 << 16) | (w2 << 2) | ((nullification ? 1 : 0) << 1) | w;
  if (nullification)
	((PRUint32 *)where)[1] = 0x08000240; // nop
}

#if defined(DEBUG)
void HPPAFormat12::
printPretty(FILE* f) 
{
  fprintf(f, "%s%s N%d,%s%s", hiInfo[kind].string, nullification ? ",n" : "",
		  target.dfsNum, getTString(), nullification ? "; nop" : "");
}
#endif

// HPPAFormat13 methods
// |31               |25            |20            |15      |12                              | 1| 0|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|
// |       op        |      t       |      w1      |  ext3  |                w2              | n| w|
// |-----------------|--------------|--------------|--------|--------------------------------|--|--|
void HPPAFormat13::
formatToMemory(void* where, uint32 /*offset*/)
{
  PRUint32 w2, w1, w;
  
  dis_assemble_17((((PRUint32) address) - (((PRUint32) where) + 8)) >> 2, w1, w2, w);

  ((PRUint32 *)where)[0] = (hiInfo[kind].opcode << 26) | (returnRegister << 21) |  (w1 << 16) | (w2 << 2) | ((nullification ? 1 : 0) << 1) | w;
  if (nullification)
	((PRUint32 *)where)[1] = 0x08000240; // nop
}

#if defined(DEBUG)
void HPPAFormat13::
printPretty(FILE* f) 
{
  fprintf(f, "%s%s %s,%s",  hiInfo[kind].string, nullification ? ",n" : "", getAddressString(), getTString());
}
#endif

// HPPAFormat31 methods
// |31               |25            |20            |15   |13|12|11   |9 |8       |5 |4            0|
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|
// |       op        |       b      |      im5     |  s  |a |1 | cc  |0 |  uid   |m |      t       |
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|

void HPPAFormat31::
formatToMemory(void* where, uint32 /*offset*/)
{
  *(PRUint32 *)where = 0;
}

#if defined(DEBUG)
void HPPAFormat31::
printPretty(FILE* f)
{
  fprintf(f, "%s %d(0,%s),%s", hiInfo[kind].string, (PRInt8) getIm5(), getR1String(), getTString());
}
#endif

// HPPAFormat32 methods
// |31               |25            |20            |15   |13|12|11   |9 |8       |5 |4            0|
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|
// |       op        |       b      |      im5     |  s  |a |1 | cc  |1 |  uid   |m |      r       |
// |-----------------|--------------|--------------|-----|--|--|-----|--|--------|--|--------------|

void HPPAFormat32::
formatToMemory(void* where, uint32 /*offset*/)
{
  *(PRUint32 *)where = 0;
}

#if defined(DEBUG)
void HPPAFormat32::
printPretty(FILE* f)
{
  fprintf(f, "%s %s,%d(0,%s)", hiInfo[kind].string, getR2String(), (PRInt8) getIm5(), getR1String());
}
#endif

// HPPAFormat40 methods
// |31               |25            |20            |15      |12|11|10   |8 |7 |6 |5 |4            0|
// |-----------------|--------------|--------------|--------|--|--|-----|--|--|--|--|--------------|
// |       op        |       r1     |      r2      |   sop  |r2|f |  3  |x |r1|t |0 |      t       |
// |-----------------|--------------|--------------|--------|--|--|-----|--|--|--|--|--------------|

void HPPAFormat40::
formatToMemory(void* where, uint32 /*offset*/)
{
  *(PRUint32 *)where = 0;
}

#if defined(DEBUG)
void HPPAFormat40::
printPretty(FILE* f) 
{
  fprintf(f, "%s %s,%s,%s", hiInfo[kind].string, getR1String(), getR2String(), getTString());
}
#endif
