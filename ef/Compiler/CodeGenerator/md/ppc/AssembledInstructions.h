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
// AssembledInstructions.h
#ifndef _H_ASSEMBLEDINSTRUCTIONS
#define _H_ASSEMBLEDINSTRUCTIONS

const Uint32 kBlr           =	0x4e800020;	// blr
const Uint32 kBlrl          =	0x4e800021;	// blrl
const Uint32 kMtlrR0        =	0x7c0803a6;	// mtlr r0
const Uint32 kMtctrR0        =	0x7c0903a6;	// mtctr r0
const Uint32 kBctr       	 =	0x4e800420;	// bctr
const Uint32 kOriR0			=	0x60000000;	// ori r0, r0, 0
const Uint32 kAddiR0		=	0x38000000;	// addi r0, r0, 0
const Uint32 addisR0		=	0x3C000000;	// addis r0, r0, 0

const Uint32 kBla			= 	0x48000003;	// bla
const Uint32 kBl			=	0x48000001;	// bl
const Uint32 kB				=	0x48000000;	// b
const Uint32 kNop			=	0x60000000;	// nop
const Uint32 kLwzR0_R13    =    0x800D0000; // lwz r0, ?(r13)

inline Uint32
shiftLeftMask(Uint32 inValue, Uint32 inShift, Uint32 inMask = 0xFFFFFFFF)
{
	return ((inValue << inShift) & inMask);
}

inline Uint32
makeDForm(Uint8 inOpcd, Uint8 inD, Uint8 inA, Int16 inIMM)
{
	return (shiftLeftMask(inOpcd, 26) |
			shiftLeftMask(inD, 21) |
			shiftLeftMask(inA, 16) |
			shiftLeftMask(inIMM, 0, 0xFFFF));
}

const uint32 mfcrR0         =    0x7c000026;      // mfcr r0
const uint32 stwR04R1       =    0x90010004;      // stw r0, 4(r1)
const uint32 mflrR0         =    0x7c0802a6;      // mflr r0
const uint32 stwR08R1       =    0x90010008;      // stw r0, 8(r1)
const uint32 stfd_FR_offR1  =    0xd8010000;      // stfd fr?, ?(r1)
const uint32 stw_R_offR1    =    0x90010000;      // stw r?, ?(r1)
const uint32 stwuR1_offR1   =    0x94210000;      // stwu r1, ?(r1)
const uint32 lisR12_imm     =    0x3d800000;      // lis r12, ?
const uint32 oriR12R12_imm  =    0x618c0000;      // ori r12, r12, ?
const uint32 stwuxR1R1R12   =    0x7c21616e;      // stwux r1, r1, r12

const uint32 lwzR08R1       =    0x80010008;      // lwz r0, 8(r1)
const uint32 lwzR04R1       =    0x80010004;      // lwz r0, 4(r1)
const uint32 mtcrR0         =    0x7c0ff120;      // mtcr r0
const uint32 lfd_FR_offR1   =    0xc8010000;      // lfd fr?, ?(r1)
const uint32 lwz_R_offR1    =    0x80010000;      // lwz r?, ?(r1)
const uint32 addiR1R1_imm   =    0x38210000;      // addi r1, r1, ?
const uint32 addR1R1R12     =    0x7c216214;      // add r1, r1, r12

#endif //_H_ASSEMBLEDINSTRUCTIONS

