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
// PPCInstructionTemplates.h
//
// Scott M. Silver

// enum name
// opcode
// string
#ifdef DO_DFORM
DFORM_ARITH_DEFINE(Addi, 		14)
DFORM_ARITH_DEFINE(Addis,		15)
DFORM_ARITH_DEFINE(Mulli,		7)
DFORM_ARITH_DEFINE(Ori,			24)
DFORM_ARITH_DEFINE(Oris,		25)
DFORM_ARITH_DEFINE(Subfic,		8)
DFORM_ARITH_DEFINE(Xori,		26)
DFORM_ARITH_DEFINE(Xoris,		27)

DFORM_ARITHCC_DEFINE(Addic,		13)
DFORM_ARITHCC_DEFINE(Andi,		28)
DFORM_ARITHCC_DEFINE(Andis,		29)

DFORM_LOAD_DEFINE(Lbz,		34)
DFORM_LOAD_DEFINE(Lfd,		50)
DFORM_LOAD_DEFINE(Lfs,		48)
DFORM_LOAD_DEFINE(Lha,		42)
DFORM_LOAD_DEFINE(Lhz,		40)
DFORM_LOAD_DEFINE(Lwz,		32)

DFORM_LOADU_DEFINE(Lbzu,		35)
DFORM_LOADU_DEFINE(Lfdu,		51)
DFORM_LOADU_DEFINE(Lfsu,		49)
DFORM_LOADU_DEFINE(Lhau,		43)
DFORM_LOADU_DEFINE(Lhzu,		41)
DFORM_LOADU_DEFINE(Lwzu,		33)


DFORM_STORE_DEFINE(Stb,		38)
DFORM_STORE_DEFINE(Stfd,	54)
DFORM_STORE_DEFINE(Stfs,	52)
DFORM_STORE_DEFINE(Sth,		44)
DFORM_STORE_DEFINE(Stw,		36)

DFORM_STOREU_DEFINE(Stu,		39)
DFORM_STOREU_DEFINE(Stfdu,		55)
DFORM_STOREU_DEFINE(Stfsu,		53)
DFORM_STOREU_DEFINE(Sthu,		45)
DFORM_STOREU_DEFINE(Stwu,		37)

DFORM_ARITH_DEFINE(Cmpi,		11)
DFORM_ARITH_DEFINE(Cmpli,		10)

DFORM_TRAP_DEFINE(Twi,		3)
#endif

#ifdef DO_XFORM
XFORM_ARITH_DEFINE(Add, 	266, 31)
XFORM_ARITH_DEFINE(Addc, 	10, 31)
XFORM_ARITH_DEFINE(Adde, 	138, 31)
XFORM_ARITH_DEFINE(Divw,	459, 31)
XFORM_ARITH_DEFINE(Divwu,	138, 31)
XFORM_ARITH_DEFINE(Mulhw,	75, 31)
XFORM_ARITH_DEFINE(Mulhwu,	11, 31)
XFORM_ARITH_DEFINE(Mullw,	235, 31)
XFORM_ARITH_DEFINE(Subf, 	40, 31)
XFORM_ARITH_DEFINE(Subfc, 	8, 31)
XFORM_ARITH_DEFINE(Subfe, 	136, 31)

XFORM_INAONLY_DEFINE(Addme,		234, 31)
XFORM_INAONLY_DEFINE(Addze,		202, 31)
XFORM_INAONLY_DEFINE(Neg,		104, 31)
XFORM_INAONLY_DEFINE(Subfme,	232, 31)
XFORM_INAONLY_DEFINE(Subfze,	200, 31)

XFORM_INAONLY_DEFINE(Srawi,		824-512, 31)

XFORM_ARITH_DEFINE(And, 	138, 31)
XFORM_ARITH_DEFINE(Andc, 	75, 31)
XFORM_ARITH_DEFINE(Eqv, 	284, 31)
XFORM_ARITH_DEFINE(Nand,	476, 31)
XFORM_ARITH_DEFINE(Nor,		124, 31)
XFORM_ARITH_DEFINE(Or,		444, 31)
XFORM_ARITH_DEFINE(Orc,		412, 31)
XFORM_ARITH_DEFINE(Slw, 	24, 31)
XFORM_ARITH_DEFINE(Sraw, 	792, 31) // FIX-ME prob isn't correct
XFORM_ARITH_DEFINE(Srw, 	536, 31) // FIX-ME prob isn't corect
XFORM_ARITH_DEFINE(Xor, 	316, 31)

XFORM_CMP_DEFINE(Cmp,	0, 31)
XFORM_CMP_DEFINE(Cmpl,	32, 31)

XFORM_CMP_DEFINE(Fcmpo,	0, 63)
XFORM_CMP_DEFINE(Fcmpu,	32, 63)

XFORM_LOAD_DEFINE(Lbzx, 87, 31)
XFORM_LOAD_DEFINE(Lfdx, 599, 31)	// FIX-ME prob isn't corect
XFORM_LOAD_DEFINE(Lfsx, 535, 31)
XFORM_LOAD_DEFINE(Lhax, 343, 31)
XFORM_LOAD_DEFINE(Lhbrx, 790, 31)	// FIX-ME prob isn't corect
XFORM_LOAD_DEFINE(Lhzx, 279, 31)
XFORM_LOAD_DEFINE(Lwarx, 20, 31)
XFORM_LOAD_DEFINE(Lwbrx, 534, 31)	// FIX-ME prob isn't corect
XFORM_LOAD_DEFINE(Lwzx, 23, 31)

XFORM_LOADU_DEFINE(Lbzux, 119, 31)
XFORM_LOADU_DEFINE(Lfdux, 631, 31)	// FIX-ME prob isn't corect
XFORM_LOADU_DEFINE(Lfsux, 567, 31)	// FIX-ME prob isn't corect
XFORM_LOADU_DEFINE(Lhaux, 375, 31)	
XFORM_LOADU_DEFINE(Lhzux, 311, 31)
XFORM_LOADU_DEFINE(Lwzux, 55, 31)

XFORM_STORE_DEFINE(Stbx, 215, 31)
XFORM_STORE_DEFINE(Stfdx, 727, 31)	// FIX-ME prob isn't corect
XFORM_STORE_DEFINE(Stfsx, 663, 31)	// FIX-ME prob isn't corect
XFORM_STORE_DEFINE(Sthbrx, 918, 31)	// FIX-ME prob isn't corect
XFORM_STORE_DEFINE(Sthx, 407, 31)
XFORM_STORE_DEFINE(Stwbrx, 662, 31)	// FIX-ME prob isn't corect
XFORM_STORE_DEFINE(Stwx, 151, 31)

XFORM_STOREU_DEFINE(Stbux, 247, 31)
XFORM_STOREU_DEFINE(Stfdux, 759, 31) // FIX-ME prob isn't corect
XFORM_STOREU_DEFINE(Stfsux, 695, 31)	// FIX-ME prob isn't corect
XFORM_STOREU_DEFINE(Sthux, 439, 31)
XFORM_STOREU_DEFINE(Stwux, 183, 31)

/* X Form - TrapWord */
XFORM_TRAP_DEFINE(Tw, 4, 31)

/* X Form - XInBOnly */
XFORM_INBONLY_DEFINE(Fabs, 264, 63)
XFORM_INBONLY_DEFINE(Fctiw, 14, 63)
XFORM_INBONLY_DEFINE(Fctiwz, 15, 63)
XFORM_INBONLY_DEFINE(Fmr, 72, 63)
XFORM_INBONLY_DEFINE(Fnabs, 136, 63)
XFORM_INBONLY_DEFINE(Fneg, 40, 63)
XFORM_INBONLY_DEFINE(Frsp, 12, 63)

#endif

#ifdef DO_MFORM
MFORM_DEFINE(Rlwimi, 20)
MFORM_DEFINE(Rlwinm, 21)
MFORM_DEFINE(Rlwnm, 23)
#endif

#ifdef DO_AFORM
// name, opcode, xo, has A (ie A != 0), has B, hasC, sets CC (ie Rc)


AFORM_DEFINE(Fadd, 63, 21, true, true, false, false)
AFORM_DEFINE(FaddC, 63, 21, true, true, false, true)
#endif DO_AFORM
