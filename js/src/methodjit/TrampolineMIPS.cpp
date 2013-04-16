/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jstypes.h"

/*
 * The MIPS VMFrame is 112 bytes as follows.
 *
 * 108  [ unused4      ] For alignment.
 * 104  [ ra           ]
 * 100  [ gp           ] If PIC code is generated, we will save gp.
 *  96  [ s7           ]
 *  92  [ s6           ]
 *  88  [ s5           ]
 *  84  [ s4           ]
 *  80  [ s3           ]
 *  76  [ s2           ]
 *  72  [ s1           ]
 *  68  [ s0           ]
 *  64  [ stubRejoin   ]
 *  60  [ entrycode    ]
 *  56  [ entryfp      ]
 *  52  [ stkLimit     ]
 *  48  [ cx           ]
 *  44  [ regs.fp_     ]
 *  40  [ regs.inlined_]
 *  36  [ regs.pc      ]
 *  32  [ regs.sp      ]
 *  28  [ scratch      ]
 *  24  [ previous     ]
 *  20  [ args.ptr2    ]  [ dynamicArgc ]  (union)
 *  16  [ args.ptr     ]  [ lazyArgsObj ]  (union)
 *  12  [ unused3      ] O32 ABI, space for a3 (used in callee)
 *   8  [ unused2      ] O32 ABI, space for a2 (used in callee)
 *   4  [ unused1      ] O32 ABI, space for a1 (used in callee)
 *   0  [ unused0      ] O32 ABI, space for a0 (used in callee)
 */

asm (
    ".text"     "\n"
    ".align 2"  "\n"
    ".set   noreorder"  "\n"
    ".set   nomacro"    "\n"
    ".set   nomips16"   "\n"
    ".globl JaegerThrowpoline"  "\n"
    ".ent   JaegerThrowpoline"  "\n"
    ".type  JaegerThrowpoline,@function"    "\n"
"JaegerThrowpoline:"    "\n"
#if defined(__PIC__)
    "lw     $28,100($29)"    "\n"
    "la     $25,js_InternalThrow"   "\n"
    ".reloc 1f,R_MIPS_JALR,js_InternalThrow"    "\n"
"1: jalr    $25"    "\n"
    "move   $4,$29  # set up a0"    "\n"
#else
    "jal    js_InternalThrow"       "\n"
    "move   $4,$29  # set up a0"    "\n"
#endif
    "beq    $2,$0,1f"   "\n"
    "nop"   "\n"
    "jr     $2  # jump to a scripted handler"   "\n"
    "nop"   "\n"
"1:"        "\n"
#if defined(__PIC__)
    "lw     $28,100($29)"    "\n"
    "la     $25,PopActiveVMFrame"   "\n"
    ".reloc 1f,R_MIPS_JALR,PopActiveVMFrame"    "\n"
"1:  jalr   $25"    "\n"
    "move   $4,$29  # set up a0"    "\n"
#else
    "jal    PopActiveVMFrame"       "\n"
    "move   $4,$29  # set up a0"    "\n"
#endif
    "lw     $31,104($29)"    "\n"
#if defined(__PIC__)
    "lw     $28,100($29)"    "\n"
#endif
    "lw     $23,96($29)"    "\n"
    "lw     $22,92($29)"    "\n"
    "lw     $21,88($29)"    "\n"
    "lw     $20,84($29)"    "\n"
    "lw     $19,80($29)"    "\n"
    "lw     $18,76($29)"    "\n"
    "lw     $17,72($29)"    "\n"
    "lw     $16,68($29)"    "\n"
    "li     $2,0    # return 0 to represent an unhandled exception."    "\n"
    "jr     $31"            "\n"
    "addiu  $29,$29,112"    "\n"
    ".set   reorder"        "\n"
    ".set   macro"          "\n"
    ".end   JaegerThrowpoline" "\n"
    ".size  JaegerThrowpoline,.-JaegerThrowpoline"  "\n"
);

asm (
    ".text"     "\n"
    ".align 2"  "\n"
    ".set   noreorder"  "\n"
    ".set   nomacro"    "\n"
    ".set   nomips16"   "\n"
    ".globl JaegerTrampoline"   "\n"
    ".ent   JaegerTrampoline"   "\n"
    ".type  JaegerTrampoline,@function" "\n"
"JaegerTrampoline:"     "\n"
#if defined(__PIC__)
    "lui    $28,%hi(_gp_disp)"  "\n"
    "addiu  $28,$28,%lo(_gp_disp)"      "\n"
    "addu   $28,$28,$25"    "\n"
#endif
    "addiu  $29,$29,-112"   "\n"
    "sw     $31,104($29)"   "\n"
#if defined(__PIC__)
    "sw     $28,100($29)"   "\n"
#endif
    "sw     $23,96($29)"    "\n"
    "sw     $22,92($29)"    "\n"
    "sw     $21,88($29)"    "\n"
    "sw     $20,84($29)"    "\n"
    "sw     $19,80($29)"    "\n"
    "sw     $18,76($29)"    "\n"
    "sw     $17,72($29)"    "\n"
    "sw     $16,68($29)"    "\n"
    "sw     $0,64($29)  # stubRejoin"	"\n"
    "sw     $5,60($29)  # entrycode"	"\n"
    "sw     $5,56($29)  # entryfp"	"\n"
    "sw     $7,52($29)  # stackLimit"	"\n"
    "sw     $4,48($29)  # cx"	"\n"
    "sw     $5,44($29)  # regs.fp"	"\n"
    "move   $16,$5      # preserve fp to s0"    "\n"
    "move   $17,$6      # preserve code to s1"  "\n"
#if defined(__PIC__)
    "la     $25,PushActiveVMFrame"  "\n"
    ".reloc 1f,R_MIPS_JALR,PushActiveVMFrame"   "\n"
"1:  jalr    $25"    "\n"
    "move   $4,$29  # set up a0"    "\n"
#else
    "jal    PushActiveVMFrame"      "\n"
    "move   $4,$29  # set up a0"    "\n"
#endif

    "move   $25,$17 # move code to $25"	"\n"
    "jr     $25     # jump to the compiled JavaScript Function"	"\n"
    "nop"       "\n"
    ".set   reorder"    "\n"
    ".set   macro"      "\n"
    ".end   JaegerTrampoline"   "\n"
    ".size  JaegerTrampoline,.-JaegerTrampoline"    "\n"
);

asm (
    ".text"     "\n"
    ".align 2"  "\n"
    ".set   noreorder"  "\n"
    ".set   nomacro"    "\n"
    ".set   nomips16"   "\n"
    ".globl JaegerTrampolineReturn" "\n"
    ".ent   JaegerTrampolineReturn" "\n"
    ".type  JaegerTrampolineReturn,@function"   "\n"
"JaegerTrampolineReturn:"   "\n"
#if defined(IS_LITTLE_ENDIAN)
    "sw     $4,28($16)  # a0: fp->rval type for LITTLE-ENDIAN"  "\n"
    "sw     $6,24($16)  # a2: fp->rval data for LITTLE-ENDIAN"  "\n"
#else
    "sw     $4,24($16)  # a0: fp->rval type for BIG-ENDIAN"     "\n"
    "sw     $6,28($16)  # a2: fp->rval data for BIG-ENDIAN"     "\n"
#endif
#if defined(__PIC__)
    "lw     $28,100($29)"    "\n"
    "la     $25,PopActiveVMFrame"   "\n"
    ".reloc 1f,R_MIPS_JALR,PopActiveVMFrame"    "\n"
"1:  jalr    $25"    "\n"
    "move   $4,$29  # set up a0"    "\n"
#else
    "jal    PopActiveVMFrame"       "\n"
    "move   $4,$29  # set up a0"    "\n"
#endif
    "lw     $31,104($29)"    "\n"
#if defined(__PIC__)
    "lw     $28,100($29)"    "\n"
#endif
    "lw     $23,96($29)"    "\n"
    "lw     $22,92($29)"    "\n"
    "lw     $21,88($29)"    "\n"
    "lw     $20,84($29)"    "\n"
    "lw     $19,80($29)"    "\n"
    "lw     $18,76($29)"    "\n"
    "lw     $17,72($29)"    "\n"
    "lw     $16,68($29)"    "\n"
    "li     $2,1    # return ture to indicate successful completion"    "\n"
    "jr     $31"    "\n"
    "addiu  $29,$29,112"    "\n"
    ".set   reorder"        "\n"
    ".set   macro"          "\n"
    ".end   JaegerTrampolineReturn" "\n"
    ".size  JaegerTrampolineReturn,.-JaegerTrampolineReturn"    "\n"
);

asm (
    ".text"     "\n"
    ".align 2" "\n"
    ".set   noreorder"  "\n"
    ".set   nomacro"    "\n"
    ".set   nomips16"   "\n"
    ".globl JaegerStubVeneer"   "\n"
    ".ent   JaegerStubVeneer"   "\n"
    ".type  JaegerStubVeneer,@function" "\n"
"JaegerStubVeneer:"     "\n"
    "addiu  $29,$29,-24 # Need 16 (a0-a3) + 4 (align) + 4 ($31) bytes"  "\n"
    "sw     $31,20($29) # Store $31 to 20($29)" "\n"
    "move   $25,$2      # the target address is passed from $2"  "\n"
    "jalr   $25"    "\n"
    "nop"           "\n"
    "lw     $31,20($29)"        "\n"
    "jr     $31"    "\n"
    "addiu  $29,$29,24"         "\n"
    ".set   reorder"    "\n"
    ".set   macro"      "\n"
    ".end   JaegerStubVeneer"   "\n"
    ".size  JaegerStubVeneer,.-JaegerStubVeneer"    "\n"
);

asm (
    ".text"     "\n"
    ".align 2" "\n"
    ".set   noreorder"  "\n"
    ".set   nomacro"    "\n"
    ".set   nomips16"   "\n"
    ".globl JaegerInterpolineScripted"	"\n"
    ".ent   JaegerInterpolineScripted"	"\n"
    ".type  JaegerInterpolineScripted,@function"    "\n"
"JaegerInterpolineScripted:"	"\n"
    "lw     $16,16($16) # Load f->prev_"  "\n"
    "b      JaegerInterpoline"  "\n"
    "sw     $16,44($29) # Update f->regs->fp_"  "\n"
    ".set   reorder"    "\n"
    ".set   macro"      "\n"
    ".end   JaegerInterpolineScripted"   "\n"
    ".size  JaegerInterpolineScripted,.-JaegerInterpolineScripted"  "\n"
);

asm (
    ".text"     "\n"
    ".align 2" "\n"
    ".set   noreorder"  "\n"
    ".set   nomacro"    "\n"
    ".set   nomips16"   "\n"
    ".globl JaegerInterpoline"	"\n"
    ".ent   JaegerInterpoline"	"\n"
    ".type  JaegerInterpoline,@function"    "\n"
"JaegerInterpoline:"	"\n"
    "move   $5,$4   # returntype"   "\n"
    "move   $4,$6   # returnData"   "\n"
    "move   $6,$2   # returnReg"    "\n"
#if defined(__PIC__)
    "lw     $28,100($29)"    "\n"
    "la     $25,js_InternalInterpret"   "\n"
    ".reloc 1f,R_MIPS_JALR,js_InternalInterpret"    "\n"
"1:  jalr    $25"    "\n"
    "move   $7,$29  # f"    "\n"
#else
    "jal    js_InternalInterpret"   "\n"
    "move   $7,$29  # f"    "\n"
#endif
    "lw     $16,44($29) # Load f->regs->fp_ to s0"  "\n"
#if defined(IS_LITTLE_ENDIAN)
    "lw     $4,28($16)  # a0: fp->rval type for LITTLE-ENDIAN"  "\n"
    "lw     $6,24($16)  # a2: fp->rval data for LITTLE-ENDIAN"  "\n"
#else
    "lw     $4,24($16)  # a0: fp->rval type for BIG-ENDIAN"     "\n"
    "lw     $6,28($16)  # a2: fp->rval data for BIG-ENDIAN"     "\n"
#endif
    "lw     $5,28($29)  # Load sctrach -> argc"     "\n"
    "beq    $2,$0,1f"   "\n"
    "nop"               "\n"
    "jr     $2"         "\n"
    "nop"               "\n"
"1:"    "\n"
#if defined(__PIC__)
    "lw     $28,100($29)"    "\n"
    "la     $25,PopActiveVMFrame"   "\n"
    ".reloc 1f,R_MIPS_JALR,PopActiveVMFrame"    "\n"
"1:  jalr   $25"    "\n"
    "move   $4,$29  # set up a0"    "\n"
#else
    "jal    PopActiveVMFrame"       "\n"
    "move   $4,$29  # set up a0"    "\n"
#endif
    "lw     $31,104($29)"    "\n"
#if defined(__PIC__)
    "lw     $28,100($29)"    "\n"
#endif
    "lw     $23,96($29)"    "\n"
    "lw     $22,92($29)"    "\n"
    "lw     $21,88($29)"    "\n"
    "lw     $20,84($29)"    "\n"
    "lw     $19,80($29)"    "\n"
    "lw     $18,76($29)"    "\n"
    "lw     $17,72($29)"    "\n"
    "lw     $16,68($29)"    "\n"
    "li     $2,0    # return 0"     "\n"
    "jr     $31"    "\n"
    "addiu  $29,$29,112"    "\n"
    ".set   reorder"    "\n"
    ".set   macro"      "\n"
    ".end   JaegerInterpoline"	"\n"
    ".size  JaegerInterpoline,.-JaegerInterpoline"  "\n"
);
