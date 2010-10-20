/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5) */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *   leon.sha@oracle.com
 *   ginn.chen@oracle.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <sys/mman.h>
#include <errno.h>
#include "nanojit.h"

namespace nanojit
{
#ifdef FEATURE_NANOJIT

#ifdef NJ_VERBOSE
    const char *regNames[] = {
        "%g0", "%g1", "%g2", "%g3", "%g4", "%g5", "%g6", "%g7",
        "%o0", "%o1", "%o2", "%o3", "%o4", "%o5", "%sp", "%o7",
        "%l0", "%l1", "%l2", "%l3", "%l4", "%l5", "%l6", "%l7",
        "%i0", "%i1", "%i2", "%i3", "%i4", "%i5", "%fp", "%i7",
        "%f0", "%f1", "%f2", "%f3", "%f4", "%f5", "%f6", "%f7",
        "%f8", "%f9", "%f10", "%f11", "%f12", "%f13", "%f14", "%f15",
        "%f16", "%f17", "%f18", "%f19", "%f20", "%f21", "%f22", "%f23",
        "%f24", "%f25", "%f26", "%f27", "%f28", "%f29", "%f30", "%f31"
    };
#endif

    const Register Assembler::argRegs[] = { I0, I1, I2, I3, I4, I5 };
    const Register Assembler::retRegs[] = { O0 };
    const Register Assembler::savedRegs[] = { L1 }; // Dummy element not used, as NumSavedRegs == 0

    static const int kLinkageAreaSize = 68;
    static const int kcalleeAreaSize = 80; // The max size.

#define BIT_ROUND_UP(v,q)      ( (((uintptr_t)v)+(q)-1) & ~((q)-1) )
#define TODO(x) do{ verbose_only(outputf(#x);) NanoAssertMsgf(false, "%s", #x); } while(0)

    inline void Assembler::CALL(const CallInfo* ci) {
        int32_t offset = (ci->_address) - ((int32_t)_nIns) + 4;
        int32_t i = 0x40000000 | ((offset >> 2) & 0x3FFFFFFF);
        IMM32(i);
        asm_output("call %s",(ci->_name));
    }

    inline void Assembler::IntegerOperation
        (Register rs1, Register rs2, Register rd, int32_t op3, const char *opcode) {
        Format_3_1(2, rd, op3, rs1, 0, rs2);
        asm_output("%s %s, %s, %s", opcode, gpn(rs1), gpn(rs2), gpn(rd));
    }

    inline void Assembler::IntegerOperationI
        (Register rs1, int32_t simm13, Register rd, int32_t op3, const char *opcode) {
        Format_3_1I(2, rd, op3, rs1, simm13);
        asm_output("%s %s, %d, %s", opcode, gpn(rs1), simm13, gpn(rd));
    }

    inline void Assembler::ADD(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0, "add");
    }
    inline void Assembler::ADDCC(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x10, "addcc");
    }
    inline void Assembler::AND(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x1, "and");
    }
    inline void Assembler::ANDCC(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x11, "andcc");
    }
    inline void Assembler::OR(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x2, "or");
    }
    inline void Assembler::ORI(Register rs1, int32_t simm13, Register rd) {
        IntegerOperationI(rs1, simm13, rd, 0x2, "or");
    }
    inline void Assembler::ORN(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x6, "orn");
    }
    inline void Assembler::SMULCC(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x1b, "smulcc");
    }
    inline void Assembler::SUB(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x4, "sub");
    };
    inline void Assembler::SUBCC(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x14, "subcc");
    };
    inline void Assembler::SUBI(Register rs1, int32_t simm13, Register rd) {
        IntegerOperationI(rs1, simm13, rd, 0x4, "sub");
    }
    inline void Assembler::XOR(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x3, "xor");
    };

    inline void Assembler::Bicc(int32_t a, int32_t dsp22, int32_t cond, const char *opcode) {
        Format_2_2(a, cond, 0x2, dsp22);
        asm_output("%s 0x%x", opcode, _nIns + dsp22 - 1);
    }

    inline void Assembler::BA  (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0x8, "ba");   }
    inline void Assembler::BE  (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0x1, "be");   }
    inline void Assembler::BNE (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0x9, "bne");  }
    inline void Assembler::BG  (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0xa, "bg");   }
    inline void Assembler::BGU (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0xc, "bgu");  }
    inline void Assembler::BGE (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0xb, "bge");  }
    inline void Assembler::BL  (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0x3, "bl");  }
    inline void Assembler::BLE (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0x2, "ble");  }
    inline void Assembler::BLEU(int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0x4, "bleu"); }
    inline void Assembler::BCC (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0xd, "bcc");  }
    inline void Assembler::BCS (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0x5, "bcs");  }
    inline void Assembler::BVC (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0xf, "bvc");  }
    inline void Assembler::BVS (int32_t a, int32_t dsp22) { Bicc(a, dsp22, 0x7, "bvs");  }

    inline void Assembler::FABSS(Register rs2, Register rd) {
        Format_3_8(2, rd, 0x34, G0, 0x9, rs2);
        asm_output("fabs %s, %s", gpn(rs2), gpn(rd));
    }

    inline void Assembler::FADDD(Register rs1, Register rs2, Register rd) {
        Format_3_8(2, rd, 0x34, rs1, 0x42, rs2);
        asm_output("faddd %s, %s, %s", gpn(rs1), gpn(rs2), gpn(rd));
    }

    inline void Assembler::FBfcc(int32_t a, int32_t dsp22, int32_t cond, const char *opcode) {
        Format_2_2(a, cond, 0x6, dsp22);
        asm_output("%s 0x%x", opcode, _nIns + dsp22 - 1);
    }

    inline void Assembler::FBE  (int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0x9, "fbe");   }
    inline void Assembler::FBNE (int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0x1, "fbne");  }
    inline void Assembler::FBUE (int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0xa, "fbue");  }
    inline void Assembler::FBG  (int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0x6, "fbg");   }
    inline void Assembler::FBUG (int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0x5, "fbug");  }
    inline void Assembler::FBGE (int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0xb, "fbge");  }
    inline void Assembler::FBUGE(int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0xc, "fbuge"); }
    inline void Assembler::FBL  (int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0x4, "fbl");   }
    inline void Assembler::FBUL (int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0x3, "fbul");  }
    inline void Assembler::FBLE (int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0xd, "fble");  }
    inline void Assembler::FBULE(int32_t a, int32_t dsp22) { FBfcc(a, dsp22, 0xe, "fbule"); }

    inline void Assembler::FCMPD(Register rs1, Register rs2) {
        Format_3_9(2, 0, 0, 0x35, rs1, 0x52, rs2);
        asm_output("fcmpd %s, %s", gpn(rs1), gpn(rs2));
    }

    inline void Assembler::FloatOperation
        (Register rs1, Register rs2, Register rd, int32_t opf, const char *opcode) {
        Format_3_8(2, rd, 0x34, rs1, opf, rs2);
        if (rs1 != G0) {
          asm_output("%s %s, %s, %s", opcode, gpn(rs1), gpn(rs2), gpn(rd));
        } else {
          asm_output("%s %s, %s", opcode, gpn(rs2), gpn(rd));
        }
    }

    inline void Assembler::FSUBD(Register rs1, Register rs2, Register rd) {
        FloatOperation(rs1, rs2, rd, 0x46, "fsubd");
    }
    inline void Assembler::FMULD(Register rs1, Register rs2, Register rd) {
        FloatOperation(rs1, rs2, rd, 0x4a, "fsubd");
    }
    inline void Assembler::FDTOI(Register rs2, Register rd) {
        FloatOperation(G0, rs2, rd, 0xd2, "fdtoi");
    }
    inline void Assembler::FDIVD(Register rs1, Register rs2, Register rd) {
        FloatOperation(rs1, rs2, rd, 0x4e, "fdivd");
    }
    inline void Assembler::FMOVD(Register rs2, Register rd) {
        FloatOperation(G0, rs2, rd, 0x2, "fmovd");
    }
    inline void Assembler::FNEGD(Register rs2, Register rd) {
        FloatOperation(G0, rs2, rd, 0x6, "fnegd");
    }
    inline void Assembler::FITOD(Register rs2, Register rd) {
        FloatOperation(G0, rs2, rd, 0xc8, "fitod");
    }

    inline void Assembler::JMPL(Register rs1, Register rs2, Register rd) {
        Format_3_1(2, rd, 0x38, rs1, 0, rs2);
        asm_output("jmpl [%s + %s]", gpn(rs1), gpn(rs2));
    }

    inline void Assembler::JMPLI(Register rs1, int32_t simm13, Register rd) {
        Format_3_1I(2, rd, 0x38, rs1, simm13);
        asm_output("jmpl [%s + 0x%x]", gpn(rs1), simm13);
    }

    inline void Assembler::LoadOperation
        (Register rs1, Register rs2, Register rd, int32_t op3, const char* opcode) {
        Format_3_1(3, rd, op3, rs1, 0, rs2);
        asm_output("%s [%s + %s], %s", opcode, gpn(rs1), gpn(rs2), gpn(rd));
    }

    inline void Assembler::LoadOperationI
        (Register rs1, int32_t simm13, Register rd, int32_t op3, const char* opcode) {
        Format_3_1I(3, rd, op3, rs1, simm13);
        asm_output("%s [%s + 0x%x], %s", opcode, gpn(rs1), simm13, gpn(rd));
    }

    inline void Assembler::LDF(Register rs1, Register rs2, Register rd) {
        LoadOperation(rs1, rs2, rd, 0x20, "ldf");
    }
    inline void Assembler::LDFI(Register rs1, int32_t simm13, Register rd) {
        LoadOperationI(rs1, simm13, rd, 0x20, "ldf");
    }

    inline void Assembler::LDDF32(Register rs1, int32_t immI, Register rd) {
        if (isIMM13(immI+4)) {
            LDFI(rs1, immI+4, REGINC(rd));
            LDFI(rs1, immI, rd);
        } else {
            LDF(rs1, L0, REGINC(rd));
            SET32(immI+4, L0);
            LDF(rs1, L0, rd);
            SET32(immI, L0);
        }
    }

    inline void Assembler::LDUB(Register rs1, Register rs2, Register rd) {
        LoadOperation(rs1, rs2, rd,  0x1, "ldub");
    }
    inline void Assembler::LDUBI(Register rs1, int32_t simm13, Register rd) {
        LoadOperationI(rs1, simm13, rd, 0x1, "ldub");
    }

    inline void Assembler::LDUB32(Register rs1, int32_t immI, Register rd) {
        if (isIMM13(immI)) {
            LDUBI(rs1, immI, rd);
        } else {
            LDUB(rs1, L0, rd);
            SET32(immI, L0);
        }
    }

    inline void Assembler::LDUH(Register rs1, Register rs2, Register rd) {
        LoadOperation(rs1, rs2, rd,  0x2, "lduh");
    }
    inline void Assembler::LDUHI(Register rs1, int32_t simm13, Register rd) {
        LoadOperationI(rs1, simm13, rd, 0x2, "lduh");
    }

    inline void Assembler::LDUH32(Register rs1, int32_t immI, Register rd) {
        if (isIMM13(immI)) {
            LDUHI(rs1, immI, rd);
        } else {
            LDUH(rs1, L0, rd);
            SET32(immI, L0);
        }
    }

    inline void Assembler::LDSW(Register rs1, Register rs2, Register rd) {
        LoadOperation(rs1, rs2, rd,  0x8, "ldsw");
    }
    inline void Assembler::LDSWI(Register rs1, int32_t simm13, Register rd) {
        LoadOperationI(rs1, simm13, rd, 0x8, "ldsw");
    }

    inline void Assembler::LDSW32(Register rs1, int32_t immI, Register rd) {
        if (isIMM13(immI)) {
            LDSWI(rs1, immI, rd);
        } else {
            LDSW(rs1, L0, rd);
            SET32(immI, L0);
        }
    }

    inline void Assembler::MOVcc
        (Register rs, int32_t cc2, int32_t cc1, int32_t cc0, Register rd, int32_t cond, const char *opcode) {
        Format_4_2(rd, 0x2c, cc2, cond, cc1, cc0, rs);
        asm_output("%s %s, %s", opcode, gpn(rs), gpn(rd));
    }

    inline void Assembler::MOVccI
        (int32_t simm11, int32_t cc2, int32_t cc1, int32_t cc0, Register rd, int32_t cond, const char *opcode) {
        Format_4_2I(rd, 0x2c, cc2, cond, cc1, cc0, simm11);
        asm_output("%s 0x%x, %s", opcode, simm11, gpn(rd));
    }

    inline void Assembler::MOVE  (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0x1, "move");   }
    inline void Assembler::MOVNE (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0x9, "movne");  }
    inline void Assembler::MOVL  (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0x3, "movl");   }
    inline void Assembler::MOVLE (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0x2, "movle");  }
    inline void Assembler::MOVG  (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0xa, "movg");   }
    inline void Assembler::MOVGE (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0xb, "movge");  }
    inline void Assembler::MOVLEU(Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0x4, "movleu"); }
    inline void Assembler::MOVGU (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0xc, "movgu");  }
    inline void Assembler::MOVCC (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0xd, "movcc");  }
    inline void Assembler::MOVCS (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0x5, "movcs");  }
    inline void Assembler::MOVVC (Register rs, Register rd) { MOVcc(rs, 1, 0, 0, rd, 0xf, "movvc");  }
    inline void Assembler::MOVEI  (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0x1, "move");   }
    inline void Assembler::MOVFEI (int32_t simm11, Register rd) { MOVccI(simm11, 0, 0, 0, rd, 0x9, "movfe");  }
    inline void Assembler::MOVNEI (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0x9, "movne");  }
    inline void Assembler::MOVLI  (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0x3, "movl");   }
    inline void Assembler::MOVFLI (int32_t simm11, Register rd) { MOVccI(simm11, 0, 0, 0, rd, 0x4, "movfl");  }
    inline void Assembler::MOVLEI (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0x2, "movle");  }
    inline void Assembler::MOVFLEI(int32_t simm11, Register rd) { MOVccI(simm11, 0, 0, 0, rd, 0xd, "movfle"); }
    inline void Assembler::MOVGI  (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0xa, "movg");   }
    inline void Assembler::MOVFGI (int32_t simm11, Register rd) { MOVccI(simm11, 0, 0, 0, rd, 0x6, "movfg");  }
    inline void Assembler::MOVGEI (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0xb, "movge");  }
    inline void Assembler::MOVFGEI(int32_t simm11, Register rd) { MOVccI(simm11, 0, 0, 0, rd, 0xb, "movfge"); }
    inline void Assembler::MOVLEUI(int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0x4, "movleu"); }
    inline void Assembler::MOVGUI (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0xc, "movgu");  }
    inline void Assembler::MOVCCI (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0xd, "movcc");  }
    inline void Assembler::MOVCSI (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0x5, "movcs");  }
    inline void Assembler::MOVVSI (int32_t simm11, Register rd) { MOVccI(simm11, 1, 0, 0, rd, 0x7, "movvs");  }

    inline void Assembler::NOP() {
        Format_2(0, 0x4, 0);
        asm_output("nop");
    }

    inline void Assembler::RDY(Register rd) {
        Format_3_1(2, rd, 0x28, G0, 0, G0);
        asm_output("rdy %s", gpn(rd));
    }

    inline void Assembler::RESTORE(Register rs1, Register rs2, Register rd) {
        Format_3_1(2, rd, 0x3d, rs1, 0, rs2);
        asm_output("restore");
    }

    inline void Assembler::SAVE(Register rs1, Register rs2, Register rd) {
        IntegerOperation(rs1, rs2, rd, 0x3c, "save");
    }
    inline void Assembler::SAVEI(Register rs1, int32_t simm13, Register rd) {
        IntegerOperationI(rs1, simm13, rd, 0x3c, "save");
    }

    inline void Assembler::SETHI(int32_t immI, Register rd) {
        Format_2A(rd, 0x4, immI >> 10);
        asm_output("sethi 0x%x, %s     ! 0x%x", immI >> 10, gpn(rd), immI);
    }

    inline void Assembler::SET32(int32_t immI, Register rd) {
        if (isIMM13(immI)) {
            ORI(G0, immI, rd);
        } else {
            ORI(rd, immI & 0x3FF, rd);
            SETHI(immI, rd);
        }
    }

    inline void Assembler::ShiftOperation
        (Register rs1, Register rs2, Register rd, int32_t op3, const char* opcode) {
        Format_3_5(2, rd, op3, rs1, 0, rs2);
        asm_output("%s %s, %s, %s", opcode, gpn(rs1), gpn(rs2), gpn(rd));
    }

    inline void Assembler::ShiftOperationI
        (Register rs1, int32_t shcnt32, Register rd, int32_t op3, const char* opcode) {
        Format_3_6(2, rd, op3, rs1, shcnt32);
        asm_output("%s %s, %d, %s", opcode, gpn(rs1), shcnt32, gpn(rd));
    }

    inline void Assembler::SLL(Register rs1, Register rs2, Register rd) {
        ShiftOperation(rs1, rs2, rd, 0x25, "sll");
    }
    inline void Assembler::SRA(Register rs1, Register rs2, Register rd) {
        ShiftOperation(rs1, rs2, rd, 0x27, "sra");
    }
    inline void Assembler::SRAI(Register rs1, int32_t shcnt32, Register rd) {
        ShiftOperationI(rs1, shcnt32, rd, 0x27, "sra");
    }
    inline void Assembler::SRL(Register rs1, Register rs2, Register rd) {
        ShiftOperation(rs1, rs2, rd, 0x26, "srl");
    }

    inline void Assembler::Store
        (Register rd, Register rs1, Register rs2, int32_t op3, const char* opcode) {
        Format_3_1(3, rd, op3, rs1, 0, rs2);
        asm_output("%s %s, [%s + %s]", opcode, gpn(rd), gpn(rs1), gpn(rs2));
    }

    inline void Assembler::StoreI
        (Register rd, int32_t simm13, Register rs1, int32_t op3, const char* opcode) {
        Format_3_1I(3, rd, op3, rs1, simm13);
        asm_output("%s %s, [%s + 0x%x]", opcode, gpn(rd), gpn(rs1), simm13);
    }

    inline void Assembler::STF(Register rd, Register rs1, Register rs2) {
        Store(rd, rs1, rs2, 0x24, "stf");
    }
    inline void Assembler::STFI(Register rd, int32_t simm13, Register rs1) {
        StoreI(rd, simm13, rs1, 0x24, "stf");
    }

    inline void Assembler::STF32(Register rd, int32_t immI, Register rs1) {
        if (isIMM13(immI)) {
            STFI(rd, immI, rs1);
        } else {
            STF(rd, L0, rs1);
            SET32(immI, L0);
        }
    }

    inline void Assembler::STDF32(Register rd, int32_t immI, Register rs1) {
        if (isIMM13(immI+4)) {
            STFI(REGINC(rd), immI+4, rs1);;
            STFI(rd, immI, rs1);
        } else {
            STF(REGINC(rd), L0, rs1);
            SET32(immI+4, L0);
            STF(rd, L0, rs1);
            SET32(immI, L0);
        }
    }

    inline void Assembler::STW(Register rd, Register rs1, Register rs2) {
        Store(rd, rs1, rs2, 0x4, "st");
    }
    inline void Assembler::STWI(Register rd, int32_t simm13, Register rs1) {
        StoreI(rd, simm13, rs1, 0x4, "st");
    }

    inline void Assembler::STW32(Register rd, int32_t immI, Register rs1) {
        if (isIMM13(immI)) {
            STWI(rd, immI, rs1);
         } else {
            STW(rd, L0, rs1);
            SET32(immI, L0);
         }
    }

    inline void Assembler::STB(Register rd, Register rs1, Register rs2) {
        Store(rd, rs1, rs2, 0x5, "stb");
    }
    inline void Assembler::STBI(Register rd, int32_t simm13, Register rs1) {
        StoreI(rd, simm13, rs1, 0x5, "stb");
    }

    inline void Assembler::STB32(Register rd, int32_t immI, Register rs1) {
        if (isIMM13(immI)) {
            STBI(rd, immI, rs1);
        } else {
            STB(rd, L0, rs1);
            SET32(immI, L0);
        }
    }

    // general Assemble
    inline void Assembler::JMP_long_nocheck(int32_t t) {
        NOP();
        JMPL(G0, G2, G0);
        ORI(G2, t & 0x3FF, G2);
        SETHI(t, G2);
    }

    inline void Assembler::JMP_long(int32_t t) {
        underrunProtect(16);
        JMP_long_nocheck(t);
    }

    inline void Assembler::JMP_long_placeholder() {
        JMP_long(0);
    }

    inline int32_t Assembler::JCC(void *t) {
        underrunProtect(32);
        int32_t tt = ((intptr_t)t - (intptr_t)_nIns + 8) >> 2;
        if( !(isIMM22(tt)) ) {
            NOP();
            JMPL(G0, G2, G0);
            SET32((intptr_t)t, G2);
            NOP();
            BA(0, 5);
            tt = 4;
        }
        NOP();
        return tt;
    }

    inline void Assembler::JMP(void *t) {
        if (!t) {
            JMP_long_placeholder();
        } else {
            int32_t tt = JCC(t);
            BA(0, tt);
        }
    }

    void Assembler::MR(Register rd, Register rs) {
        underrunProtect(4);
        ORI(rs, 0, rd);
    }

    void Assembler::nInit(AvmCore* core)
    {
        has_cmov = true;
    }

    void Assembler::nBeginAssembly() {
    }

    NIns* Assembler::genPrologue()
    {
        /**
         * Prologue
         */
        underrunProtect(16);
        uint32_t stackNeeded = STACK_GRANULARITY * _activation.stackSlotsNeeded();
        uint32_t frameSize = stackNeeded + kcalleeAreaSize + kLinkageAreaSize;
        frameSize = BIT_ROUND_UP(frameSize, 8);

        if (frameSize <= 4096)
            SUBI(FP, frameSize, SP);
        else {
            SUB(FP, G1, SP);
            ORI(G1, frameSize & 0x3FF, G1);
            SETHI(frameSize, G1);
        }

        verbose_only(
        if (_logc->lcbits & LC_Native) {
            outputf("        0x%x:",_nIns);
            outputf("        patch entry:");
        })
        NIns *patchEntry = _nIns;

        // The frame size in SAVE is faked. We will still re-caculate SP later.
        // We can use 0 here but it is not good for debuggers.
        SAVEI(SP, -148, SP);

        // align the entry point
        asm_align_code();

        return patchEntry;
    }

    void Assembler::asm_align_code() {
        while(uintptr_t(_nIns) & 15) {
            NOP();
        }
    }

    void Assembler::nFragExit(LIns* guard)
    {
        SideExit* exit = guard->record()->exit;
        Fragment *frag = exit->target;
        GuardRecord *lr;
        if (frag && frag->fragEntry)
            {
                JMP(frag->fragEntry);
                lr = 0;
            }
        else
            {
                // Target doesn't exit yet. Emit jump to epilog, and set up to patch later.
                if (!_epilogue)
                    _epilogue = genEpilogue();
                lr = guard->record();
                JMP_long((intptr_t)_epilogue);
                lr->jmp = _nIns;
            }

        // return value is GuardRecord*
        SET32(int(lr), O0);
    }

    NIns *Assembler::genEpilogue()
    {
        underrunProtect(12);
        RESTORE(G0, G0, G0); //restore
        JMPLI(I7, 8, G0); //ret
        ORI(O0, 0, I0);
        return  _nIns;
    }

    void Assembler::asm_call(LIns* ins)
    {
        if (!ins->isop(LIR_callv)) {
            Register retReg = ( ins->isop(LIR_calld) ? F0 : retRegs[0] );
            deprecated_prepResultReg(ins, rmask(retReg));
        }

        // Do this after we've handled the call result, so we don't
        // force the call result to be spilled unnecessarily.
        evictScratchRegsExcept(0);

        const CallInfo* ci = ins->callInfo();

        underrunProtect(8);
        NOP();

        ArgType argTypes[MAXARGS];
        uint32_t argc = ci->getArgTypes(argTypes);

        NanoAssert(ins->isop(LIR_callv) || ins->isop(LIR_callp) ||
                   ins->isop(LIR_calld));
        verbose_only(if (_logc->lcbits & LC_Native)
                     outputf("        0x%x:", _nIns);
                     )
        bool indirect = ci->isIndirect();
        if (!indirect) {
            CALL(ci);
        }
        else {
            argc--;
            Register r = findSpecificRegFor(ins->arg(argc), I0);
            JMPL(G0, I0, O7);
        }

        Register GPRIndex = O0;
        uint32_t offset = kLinkageAreaSize; // start of parameters stack postion.

        for(int i=0; i<argc; i++)
            {
                uint32_t j = argc-i-1;
                ArgType ty = argTypes[j];
                if (ty == ARGTYPE_D) {
                    Register r = findRegFor(ins->arg(j), FpRegs);

                    underrunProtect(48);
                    // We might be calling a varargs function.
                    // So, make sure the GPR's are also loaded with
                    // the value, or the stack contains it.
                    if (REGNUM(GPRIndex) <= REGNUM(O5)) {
                        LDSW32(SP, offset, GPRIndex);
                    }
                    GPRIndex = REGINC(GPRIndex);
                    if (REGNUM(GPRIndex) <= REGNUM(O5)) {
                        LDSW32(SP, offset+4, GPRIndex);
                    }
                    GPRIndex = REGINC(GPRIndex);
                    STDF32(r, offset, SP);
                    offset += 8;
                } else {
                    if (REGNUM(GPRIndex) > REGNUM(O5)) {
                        underrunProtect(12);
                        Register r = findRegFor(ins->arg(j), GpRegs);
                        STW32(r, offset, SP);
                    } else {
                        Register r = findSpecificRegFor(ins->arg(j), GPRIndex);
                    }
                    GPRIndex = REGINC(GPRIndex);
                    offset += 4;
                }
            }
    }

    Register Assembler::nRegisterAllocFromSet(RegisterMask set)
    {
        // need to implement faster way
        Register i = G0;
        while (!(set & rmask(i)))
            i = REGINC(i);
        _allocator.free &= ~rmask(i);
        return i;
    }

    void Assembler::nRegisterResetAll(RegAlloc& a)
    {
        a.clear();
        a.free = GpRegs | FpRegs;
    }

    void Assembler::nPatchBranch(NIns* branch, NIns* location)
    {
        *(uint32_t*)&branch[0] &= 0xFFC00000;
        *(uint32_t*)&branch[0] |= ((intptr_t)location >> 10) & 0x3FFFFF;
        *(uint32_t*)&branch[1] &= 0xFFFFFC00;
        *(uint32_t*)&branch[1] |= (intptr_t)location & 0x3FF;
    }

    RegisterMask Assembler::nHint(LIns* ins)
    {
        // Never called, because no entries in nHints[] == PREFER_SPECIAL.
        NanoAssert(0);
        return 0;
    }

    bool Assembler::canRemat(LIns* ins)
    {
        return ins->isImmI() || ins->isop(LIR_allocp);
    }

    void Assembler::asm_restore(LIns* i, Register r)
    {
        underrunProtect(24);
        if (i->isop(LIR_allocp)) {
            ADD(FP, L2, r);
            int32_t d = deprecated_disp(i);
            SET32(d, L2);
        }
        else if (i->isImmI()) {
            int v = i->immI();
            SET32(v, r);
        } else {
            int d = findMemFor(i);
            if (rmask(r) & FpRegs) {
                LDDF32(FP, d, r);
            } else {
                LDSW32(FP, d, r);
            }
        }
    }

    void Assembler::asm_store32(LOpcode op, LIns *value, int dr, LIns *base)
    {
        switch (op) {
            case LIR_sti:
            case LIR_sti2c:
                // handled by mainline code below for now
                break;
            case LIR_sti2s:
                NanoAssertMsg(0, "NJ_EXPANDED_LOADSTORE_SUPPORTED not yet supported for this architecture");
                return;
            default:
                NanoAssertMsg(0, "asm_store32 should never receive this LIR opcode");
                return;
        }

        underrunProtect(20);
        if (value->isImmI())
            {
                Register rb = getBaseReg(base, dr, GpRegs);
                int c = value->immI();
                switch (op) {
                case LIR_sti:
                    STW32(L2, dr, rb);
                    break;
                case LIR_sti2c:
                    STB32(L2, dr, rb);
                    break;
                }
                SET32(c, L2);
            }
        else
            {
                // make sure what is in a register
                Register ra, rb;
                if (base->isImmI()) {
                    // absolute address
                    dr += base->immI();
                    ra = findRegFor(value, GpRegs);
                    rb = G0;
                } else {
                    getBaseReg2(GpRegs, value, ra, GpRegs, base, rb, dr);
                }
                switch (op) {
                case LIR_sti:
                    STW32(ra, dr, rb);
                    break;
                case LIR_sti2c:
                    STB32(ra, dr, rb);
                    break;
                }
            }
    }

    void Assembler::asm_spill(Register rr, int d, bool quad)
    {
        underrunProtect(24);
        (void)quad;
        NanoAssert(d);
        if (rmask(rr) & FpRegs) {
            STDF32(rr, d, FP);
        } else {
            STW32(rr, d, FP);
        }
    }

    void Assembler::asm_load64(LIns* ins)
    {
        switch (ins->opcode()) {
            case LIR_ldd:
                // handled by mainline code below for now
                break;
            case LIR_ldf2d:
                NanoAssertMsg(0, "NJ_EXPANDED_LOADSTORE_SUPPORTED not yet supported for this architecture");
                return;
            default:
                NanoAssertMsg(0, "asm_load64 should never receive this LIR opcode");
                return;
        }

        underrunProtect(72);
        LIns* base = ins->oprnd1();
        int db = ins->disp();
        Register rr = ins->deprecated_getReg();

        int dr = deprecated_disp(ins);
        Register rb;
        if (base->isop(LIR_allocp)) {
            rb = FP;
            db += findMemFor(base);
        } else {
            rb = findRegFor(base, GpRegs);
        }
        ins->clearReg();

        // don't use an fpu reg to simply load & store the value.
        if (dr)
            asm_mmq(FP, dr, rb, db);

        deprecated_freeRsrcOf(ins);

        if (rr != deprecated_UnknownReg)
            {
                NanoAssert(rmask(rr)&FpRegs);
                _allocator.retire(rr);
                LDDF32(rb, db, rr);
            }
    }

    void Assembler::asm_store64(LOpcode op, LIns* value, int dr, LIns* base)
    {
        switch (op) {
            case LIR_std:
                // handled by mainline code below for now
                break;
            case LIR_std2f:
                NanoAssertMsg(0, "NJ_EXPANDED_LOADSTORE_SUPPORTED not yet supported for this architecture");
                return;
            default:
                NanoAssertMsg(0, "asm_store64 should never receive this LIR opcode");
                return;
        }

        underrunProtect(48);
        if (value->isImmD())
            {
                // if a constant 64-bit value just store it now rather than
                // generating a pointless store/load/store sequence
                Register rb = findRegFor(base, GpRegs);
                STW32(L2, dr+4, rb);
                SET32(value->immDlo(), L2);
                STW32(L2, dr, rb);
                SET32(value->immDhi(), L2);
                return;
            }

        if (value->isop(LIR_ldd))
            {
                // value is 64bit struct or int64_t, or maybe a double.
                // it may be live in an FPU reg.  Either way, don't
                // put it in an FPU reg just to load & store it.

                // a) if we know it's not a double, this is right.
                // b) if we guarded that its a double, this store could be on
                // the side exit, copying a non-double.
                // c) maybe its a double just being stored.  oh well.

                int da = findMemFor(value);
                Register rb;
                if (base->isop(LIR_allocp)) {
                    rb = FP;
                    dr += findMemFor(base);
                } else {
                    rb = findRegFor(base, GpRegs);
                }
                asm_mmq(rb, dr, FP, da);
                return;
            }

        Register rb;
        if (base->isop(LIR_allocp)) {
            rb = FP;
            dr += findMemFor(base);
        } else {
            rb = findRegFor(base, GpRegs);
        }

        // if value already in a reg, use that, otherwise
        // try to get it into XMM regs before FPU regs.
        Register rv = ( !value->isInReg()
                      ? findRegFor(value, FpRegs)
                      : value->deprecated_getReg() );

        STDF32(rv, dr, rb);
    }

    /**
     * copy 64 bits: (rd+dd) <- (rs+ds)
     */
    void Assembler::asm_mmq(Register rd, int dd, Register rs, int ds)
    {
        // value is either a 64bit struct or maybe a float
        // that isn't live in an FPU reg.  Either way, don't
        // put it in an FPU reg just to load & store it.
        Register t = registerAllocTmp(GpRegs & ~(rmask(rd)|rmask(rs)));
        STW32(t, dd+4, rd);
        LDSW32(rs, ds+4, t);
        STW32(t, dd, rd);
        LDSW32(rs, ds, t);
    }

    NIns* Assembler::asm_branch(bool branchOnFalse, LIns* cond, NIns* targ)
    {
        NIns* at = 0;
        LOpcode condop = cond->opcode();
        NanoAssert(cond->isCmp());
        if (isCmpDOpcode(condop))
            {
                return asm_branchd(branchOnFalse, cond, targ);
            }

        underrunProtect(32);
        intptr_t tt = ((intptr_t)targ - (intptr_t)_nIns + 8) >> 2;
        // !targ means that it needs patch.
        if( !(isIMM22((int32_t)tt)) || !targ ) {
            JMP_long_nocheck((intptr_t)targ);
            at = _nIns;
            NOP();
            BA(0, 5);
            tt = 4;
        }
        NOP();

        // produce the branch
        if (branchOnFalse)
            {
                if (condop == LIR_eqi)
                    BNE(0, tt);
                else if (condop == LIR_lti)
                    BGE(0, tt);
                else if (condop == LIR_lei)
                    BG(0, tt);
                else if (condop == LIR_gti)
                    BLE(0, tt);
                else if (condop == LIR_gei)
                    BL(0, tt);
                else if (condop == LIR_ltui)
                    BCC(0, tt);
                else if (condop == LIR_leui)
                    BGU(0, tt);
                else if (condop == LIR_gtui)
                    BLEU(0, tt);
                else //if (condop == LIR_geui)
                    BCS(0, tt);
            }
        else // op == LIR_xt
            {
                if (condop == LIR_eqi)
                    BE(0, tt);
                else if (condop == LIR_lti)
                    BL(0, tt);
                else if (condop == LIR_lei)
                    BLE(0, tt);
                else if (condop == LIR_gti)
                    BG(0, tt);
                else if (condop == LIR_gei)
                    BGE(0, tt);
                else if (condop == LIR_ltui)
                    BCS(0, tt);
                else if (condop == LIR_leui)
                    BLEU(0, tt);
                else if (condop == LIR_gtui)
                    BGU(0, tt);
                else //if (condop == LIR_geui)
                    BCC(0, tt);
            }
        asm_cmp(cond);
        return at;
    }

    NIns* Assembler::asm_branch_ov(LOpcode op, NIns* targ)
    {
        NIns* at = 0;
        underrunProtect(32);
        intptr_t tt = ((intptr_t)targ - (intptr_t)_nIns + 8) >> 2;
        // !targ means that it needs patch.
        if( !(isIMM22((int32_t)tt)) || !targ ) {
            JMP_long_nocheck((intptr_t)targ);
            at = _nIns;
            NOP();
            BA(0, 5);
            tt = 4;
        }
        NOP();

        if( op == LIR_mulxovi || op == LIR_muljovi )
            BNE(0, tt);
        else
            BVS(0, tt);
        return at;
    }

    void Assembler::asm_cmp(LIns *cond)
    {
        underrunProtect(12);

        LIns* lhs = cond->oprnd1();
        LIns* rhs = cond->oprnd2();

        NanoAssert(lhs->isI() && rhs->isI());

        // ready to issue the compare
        if (rhs->isImmI())
            {
                int c = rhs->immI();
                Register r = findRegFor(lhs, GpRegs);
                if (c == 0 && cond->isop(LIR_eqi)) {
                    ANDCC(r, r, G0);
                }
                else {
                    SUBCC(r, L2, G0);
                    SET32(c, L2);
                }
            }
        else
            {
                Register ra, rb;
                findRegFor2(GpRegs, lhs, ra, GpRegs, rhs, rb);
                SUBCC(ra, rb, G0);
            }
    }

    void Assembler::asm_condd(LIns* ins)
    {
        // only want certain regs
        Register r = deprecated_prepResultReg(ins, AllowableFlagRegs);
        underrunProtect(8);
        LOpcode condop = ins->opcode();
        NanoAssert(isCmpDOpcode(condop));
        if (condop == LIR_eqd)
            MOVFEI(1, r);
        else if (condop == LIR_led)
            MOVFLEI(1, r);
        else if (condop == LIR_ltd)
            MOVFLI(1, r);
        else if (condop == LIR_ged)
            MOVFGEI(1, r);
        else // if (condop == LIR_gtd)
            MOVFGI(1, r);
        ORI(G0, 0, r);
        asm_cmpd(ins);
    }

    void Assembler::asm_cond(LIns* ins)
    {
        underrunProtect(8);
        // only want certain regs
        LOpcode op = ins->opcode();
        Register r = deprecated_prepResultReg(ins, AllowableFlagRegs);

        if (op == LIR_eqi)
            MOVEI(1, r);
        else if (op == LIR_lti)
            MOVLI(1, r);
        else if (op == LIR_lei)
            MOVLEI(1, r);
        else if (op == LIR_gti)
            MOVGI(1, r);
        else if (op == LIR_gei)
            MOVGEI(1, r);
        else if (op == LIR_ltui)
            MOVCSI(1, r);
        else if (op == LIR_leui)
            MOVLEUI(1, r);
        else if (op == LIR_gtui)
            MOVGUI(1, r);
        else // if (op == LIR_geui)
            MOVCCI(1, r);
        ORI(G0, 0, r);
        asm_cmp(ins);
    }

    void Assembler::asm_arith(LIns* ins)
    {
        underrunProtect(28);
        LOpcode op = ins->opcode();
        LIns* lhs = ins->oprnd1();
        LIns* rhs = ins->oprnd2();

        Register rb = deprecated_UnknownReg;
        RegisterMask allow = GpRegs;
        bool forceReg = (op == LIR_muli || op == LIR_mulxovi || op == LIR_muljovi || !rhs->isImmI());

        if (lhs != rhs && forceReg)
            {
                if ((rb = asm_binop_rhs_reg(ins)) == deprecated_UnknownReg) {
                    rb = findRegFor(rhs, allow);
                }
                allow &= ~rmask(rb);
            }
        else if ((op == LIR_addi || op == LIR_addxovi) && lhs->isop(LIR_allocp) && rhs->isImmI()) {
            // add alloc+const, use lea
            Register rr = deprecated_prepResultReg(ins, allow);
            int d = findMemFor(lhs) + rhs->immI();
            ADD(FP, L2, rr);
            SET32(d, L2);
            return;
        }

        Register rr = deprecated_prepResultReg(ins, allow);
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a register assigned.
        Register ra = ( !lhs->isInReg()
                      ? findSpecificRegFor(lhs, rr)
                      : lhs->deprecated_getReg() );

        if (forceReg)
            {
                if (lhs == rhs)
                    rb = ra;

                if (op == LIR_addi || op == LIR_addxovi)
                    ADDCC(rr, rb, rr);
                else if (op == LIR_subi || op == LIR_subxovi)
                    SUBCC(rr, rb, rr);
                else if (op == LIR_muli)
                    SMULCC(rr, rb, rr);
                else if (op == LIR_mulxovi || op == LIR_muljovi) {
                    SUBCC(L4, L6, L4);
                    SRAI(rr, 31, L6);
                    RDY(L4);
                    SMULCC(rr, rb, rr);
                }
                else if (op == LIR_andi)
                    AND(rr, rb, rr);
                else if (op == LIR_ori)
                    OR(rr, rb, rr);
                else if (op == LIR_xori)
                    XOR(rr, rb, rr);
                else if (op == LIR_lshi)
                    SLL(rr, rb, rr);
                else if (op == LIR_rshi)
                    SRA(rr, rb, rr);
                else if (op == LIR_rshui)
                    SRL(rr, rb, rr);
                else
                    NanoAssertMsg(0, "Unsupported");
            }
        else
            {
                int c = rhs->immI();
                if (op == LIR_addi || op == LIR_addxovi)
                    ADDCC(rr, L2, rr);
                else if (op == LIR_subi || op == LIR_subxovi)
                    SUBCC(rr, L2, rr);
                else if (op == LIR_andi)
                    AND(rr, L2, rr);
                else if (op == LIR_ori)
                    OR(rr, L2, rr);
                else if (op == LIR_xori)
                    XOR(rr, L2, rr);
                else if (op == LIR_lshi)
                    SLL(rr, L2, rr);
                else if (op == LIR_rshi)
                    SRA(rr, L2, rr);
                else if (op == LIR_rshui)
                    SRL(rr, L2, rr);
                else
                    NanoAssertMsg(0, "Unsupported");
                SET32(c, L2);
            }

        if ( rr != ra )
            ORI(ra, 0, rr);
    }

    void Assembler::asm_neg_not(LIns* ins)
    {
        underrunProtect(8);
        LOpcode op = ins->opcode();
        Register rr = deprecated_prepResultReg(ins, GpRegs);

        LIns* lhs = ins->oprnd1();
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a register assigned.
        Register ra = ( !lhs->isInReg()
                      ? findSpecificRegFor(lhs, rr)
                      : lhs->deprecated_getReg() );

        if (op == LIR_noti)
            ORN(G0, rr, rr);
        else
            SUB(G0, rr, rr);

        if ( rr != ra )
            ORI(ra, 0, rr);
    }

    void Assembler::asm_load32(LIns* ins)
    {
        underrunProtect(12);
        LOpcode op = ins->opcode();
        LIns* base = ins->oprnd1();
        int d = ins->disp();
        Register rr = deprecated_prepResultReg(ins, GpRegs);
        Register ra = getBaseReg(base, d, GpRegs);
        switch(op) {
            case LIR_lduc2ui:
                LDUB32(ra, d, rr);
                break;
            case LIR_ldus2ui:
                LDUH32(ra, d, rr);
                break;
            case LIR_ldi:
                LDSW32(ra, d, rr);
                break;
            case LIR_ldc2i:
            case LIR_lds2i:
                NanoAssertMsg(0, "NJ_EXPANDED_LOADSTORE_SUPPORTED not yet supported for this architecture");
                return;
            default:
                NanoAssertMsg(0, "asm_load32 should never receive this LIR opcode");
                return;
        }
    }

    void Assembler::asm_cmov(LIns* ins)
    {
        underrunProtect(4);
        LOpcode op = ins->opcode();
        LIns* condval = ins->oprnd1();
        LIns* iftrue  = ins->oprnd2();
        LIns* iffalse = ins->oprnd3();

        NanoAssert(condval->isCmp());
        NanoAssert(op == LIR_cmovi && iftrue->isI() && iffalse->isI());

        const Register rr = deprecated_prepResultReg(ins, GpRegs);

        // this code assumes that neither LD nor MR nor MRcc set any of the condition flags.
        // (This is true on Intel, is it true on all architectures?)
        const Register iffalsereg = findRegFor(iffalse, GpRegs & ~rmask(rr));
        if (op == LIR_cmovi) {
            switch (condval->opcode()) {
                // note that these are all opposites...
            case LIR_eqi:  MOVNE (iffalsereg, rr); break;
            case LIR_lti:  MOVGE (iffalsereg, rr); break;
            case LIR_lei:  MOVG  (iffalsereg, rr); break;
            case LIR_gti:  MOVLE (iffalsereg, rr); break;
            case LIR_gei:  MOVL  (iffalsereg, rr); break;
            case LIR_ltui: MOVCC (iffalsereg, rr); break;
            case LIR_leui: MOVGU (iffalsereg, rr); break;
            case LIR_gtui: MOVLEU(iffalsereg, rr); break;
            case LIR_geui: MOVCS (iffalsereg, rr); break;
                debug_only( default: NanoAssert(0); break; )
                    }
        }
        /*const Register iftruereg =*/ findSpecificRegFor(iftrue, rr);
        asm_cmp(condval);
    }

    void Assembler::asm_param(LIns* ins)
    {
        uint32_t a = ins->paramArg();
        uint32_t kind = ins->paramKind();
        deprecated_prepResultReg(ins, rmask(argRegs[a]));
    }

    void Assembler::asm_immi(LIns* ins)
    {
        underrunProtect(8);
        Register rr = deprecated_prepResultReg(ins, GpRegs);
        int32_t val = ins->immI();
        if (val == 0)
            XOR(rr, rr, rr);
        else
            SET32(val, rr);
    }

    void Assembler::asm_immd(LIns* ins)
    {
        underrunProtect(64);
        Register rr = ins->deprecated_getReg();
        if (rr != deprecated_UnknownReg)
            {
                // @todo -- add special-cases for 0 and 1
                _allocator.retire(rr);
                ins->clearReg();
                NanoAssert((rmask(rr) & FpRegs) != 0);
                findMemFor(ins);
                int d = deprecated_disp(ins);
                LDDF32(FP, d, rr);
            }

        // @todo, if we used xor, ldsd, fldz, etc above, we don't need mem here
        int d = deprecated_disp(ins);
        deprecated_freeRsrcOf(ins);
        if (d)
            {
                STW32(L2, d+4, FP);
                SET32(ins->immDlo(), L2);
                STW32(L2, d, FP);
                SET32(ins->immDhi(), L2);
            }
    }

    void Assembler::asm_fneg(LIns* ins)
    {
        underrunProtect(4);
        Register rr = deprecated_prepResultReg(ins, FpRegs);
        LIns* lhs = ins->oprnd1();

        // lhs into reg, prefer same reg as result
        // if this is last use of lhs in reg, we can re-use result reg
        // else, lhs already has a different reg assigned
        Register ra = ( !lhs->isInReg()
                      ? findSpecificRegFor(lhs, rr)
                      : findRegFor(lhs, FpRegs) );

        FNEGD(ra, rr);
    }

    void Assembler::asm_fop(LIns* ins)
    {
        underrunProtect(4);
        LOpcode op = ins->opcode();
        LIns *lhs = ins->oprnd1();
        LIns *rhs = ins->oprnd2();

        RegisterMask allow = FpRegs;
        Register ra, rb;
        findRegFor2(allow, lhs, ra, allow, rhs, rb);
        Register rr = deprecated_prepResultReg(ins, allow);

        if (op == LIR_addd)
            FADDD(ra, rb, rr);
        else if (op == LIR_subd)
            FSUBD(ra, rb, rr);
        else if (op == LIR_muld)
            FMULD(ra, rb, rr);
        else //if (op == LIR_divd)
            FDIVD(ra, rb, rr);

    }

    void Assembler::asm_i2d(LIns* ins)
    {
        underrunProtect(32);
        // where our result goes
        Register rr = deprecated_prepResultReg(ins, FpRegs);
        int d = findMemFor(ins->oprnd1());
        FITOD(rr, rr);
        LDDF32(FP, d, rr);
    }

    void Assembler::asm_ui2d(LIns* ins)
    {
        underrunProtect(72);
        // where our result goes
        Register rr = deprecated_prepResultReg(ins, FpRegs);
        Register rt = registerAllocTmp(FpRegs & ~(rmask(rr)));
        Register gr = findRegFor(ins->oprnd1(), GpRegs);
        int disp = -8;

        FABSS(rr, rr);
        FSUBD(rt, rr, rr);
        LDDF32(SP, disp, rr);
        STWI(G0, disp+4, SP);
        LDDF32(SP, disp, rt);
        STWI(gr, disp+4, SP);
        STWI(G1, disp, SP);
        SETHI(0x43300000, G1);
    }

    void Assembler::asm_d2i(LIns* ins) {
        LIns *lhs = ins->oprnd1();
        Register rr = prepareResultReg(ins, GpRegs);
        Register ra = findRegFor(lhs, FpRegs);
        int d = findMemFor(ins);
        LDSW32(FP, d, rr);
        STF32(ra, d, FP);
        FDTOI(ra, ra);
    }

    void Assembler::asm_nongp_copy(Register r, Register s)
    {
        underrunProtect(4);
        NanoAssert((rmask(r) & FpRegs) && (rmask(s) & FpRegs));
        FMOVD(s, r);
    }

    NIns * Assembler::asm_branchd(bool branchOnFalse, LIns *cond, NIns *targ)
    {
        NIns *at = 0;
        LOpcode condop = cond->opcode();
        NanoAssert(isCmpDOpcode(condop));
        underrunProtect(32);
        intptr_t tt = ((intptr_t)targ - (intptr_t)_nIns + 8) >> 2;
        // !targ means that it needs patch.
        if( !(isIMM22((int32_t)tt)) || !targ ) {
            JMP_long_nocheck((intptr_t)targ);
            at = _nIns;
            NOP();
            BA(0, 5);
            tt = 4;
        }
        NOP();

        // produce the branch
        if (branchOnFalse)
            {
                if (condop == LIR_eqd)
                    FBNE(0, tt);
                else if (condop == LIR_led)
                    FBUG(0, tt);
                else if (condop == LIR_ltd)
                    FBUGE(0, tt);
                else if (condop == LIR_ged)
                    FBUL(0, tt);
                else //if (condop == LIR_gtd)
                    FBULE(0, tt);
            }
        else // op == LIR_xt
            {
                if (condop == LIR_eqd)
                    FBE(0, tt);
                else if (condop == LIR_led)
                    FBLE(0, tt);
                else if (condop == LIR_ltd)
                    FBL(0, tt);
                else if (condop == LIR_ged)
                    FBGE(0, tt);
                else //if (condop == LIR_gtd)
                    FBG(0, tt);
            }
        asm_cmpd(cond);
        return at;
    }

    void Assembler::asm_cmpd(LIns *cond)
    {
        underrunProtect(4);
        LIns* lhs = cond->oprnd1();
        LIns* rhs = cond->oprnd2();

        Register rLhs = findRegFor(lhs, FpRegs);
        Register rRhs = findRegFor(rhs, FpRegs);

        FCMPD(rLhs, rRhs);
    }

    void Assembler::nativePageReset()
    {
    }

    Register Assembler::asm_binop_rhs_reg(LIns* ins)
    {
        return deprecated_UnknownReg;
    }

    void Assembler::nativePageSetup()
    {
        NanoAssert(!_inExit);
        if (!_nIns)
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
    }

    // Increment the 32-bit profiling counter at pCtr, without
    // changing any registers.
    verbose_only(
    void Assembler::asm_inc_m32(uint32_t*)
    {
        // todo: implement this
    }
    )

    void
    Assembler::underrunProtect(int n)
    {
        NIns *eip = _nIns;
        // This may be in a normal code chunk or an exit code chunk.
        if (eip - n < codeStart) {
            codeAlloc(codeStart, codeEnd, _nIns verbose_only(, codeBytes));
            JMP_long_nocheck((intptr_t)eip);
        }
    }

    void Assembler::asm_ret(LIns* ins)
    {
        genEpilogue();
        releaseRegisters();
        assignSavedRegs();
        LIns *val = ins->oprnd1();
        if (ins->isop(LIR_reti)) {
            findSpecificRegFor(val, retRegs[0]);
        } else {
            NanoAssert(ins->isop(LIR_retd));
            findSpecificRegFor(val, F0);
        }
    }

    void Assembler::swapCodeChunks() {
        if (!_nExitIns)
            codeAlloc(exitStart, exitEnd, _nExitIns verbose_only(, exitBytes));
        SWAP(NIns*, _nIns, _nExitIns);
        SWAP(NIns*, codeStart, exitStart);
        SWAP(NIns*, codeEnd, exitEnd);
        verbose_only( SWAP(size_t, codeBytes, exitBytes); )
    }

#endif /* FEATURE_NANOJIT */
}
