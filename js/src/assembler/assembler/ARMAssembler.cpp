/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Copyright (C) 2009 University of Szeged
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * ***** END LICENSE BLOCK ***** */

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER && WTF_CPU_ARM_TRADITIONAL

#include "ARMAssembler.h"

namespace JSC {

// Patching helpers

void ARMAssembler::patchConstantPoolLoad(void* loadAddr, void* constPoolAddr)
{
    ARMWord *ldr = reinterpret_cast<ARMWord*>(loadAddr);
    ARMWord diff = reinterpret_cast<ARMWord*>(constPoolAddr) - ldr;
    ARMWord index = (*ldr & 0xfff) >> 1;

    ASSERT(diff >= 1);
    if (diff >= 2 || index > 0) {
        diff = (diff + index - 2) * sizeof(ARMWord);
        ASSERT(diff <= 0xfff);
        *ldr = (*ldr & ~0xfff) | diff;
    } else
        *ldr = (*ldr & ~(0xfff | ARMAssembler::DT_UP)) | sizeof(ARMWord);
}

// Handle immediates

ARMWord ARMAssembler::getOp2(ARMWord imm)
{
    int rol;

    if (imm <= 0xff)
        return OP2_IMM | imm;

    if ((imm & 0xff000000) == 0) {
        imm <<= 8;
        rol = 8;
    }
    else {
        imm = (imm << 24) | (imm >> 8);
        rol = 0;
    }

    if ((imm & 0xff000000) == 0) {
        imm <<= 8;
        rol += 4;
    }

    if ((imm & 0xf0000000) == 0) {
        imm <<= 4;
        rol += 2;
    }

    if ((imm & 0xc0000000) == 0) {
        imm <<= 2;
        rol += 1;
    }

    if ((imm & 0x00ffffff) == 0)
        return OP2_IMM | (imm >> 24) | (rol << 8);

    return INVALID_IMM;
}

ARMWord ARMAssembler::getOp2RegScale(RegisterID reg, ARMWord scale)
{
    // The field that this method constructs looks like this:
    // [11:7]   Shift immediate.
    // [ 6:5]   Shift type. Only LSL ("00") is used here.
    // [ 4:4]   0.
    // [ 3:0]   The register to shift.

    ARMWord shift;  // Shift field. This is log2(scale).
    ARMWord lz;     // Leading zeroes.

    // Calculate shift=log2(scale).
#if WTF_ARM_ARCH_AT_LEAST(5)
    asm (
    "   clz     %[lz], %[scale]\n"
    : [lz]      "=r"  (lz)
    : [scale]   "r"   (scale)
    : // No clobbers.
    );
#else
    ARMWord lz = 0; // Accumulate leading zeroes.
    for (ARMWord s = 16; s > 0; s /= 2) {
        ARMWord mask = 0xffffffff << (32-lz-s);
        if ((x & mask) == 0) {
            lz += s;
        }
    }
#endif
    if (lz >= 32) {
        return INVALID_IMM;
    }
    shift = 31-lz;
    // Check that scale was a power of 2.
    if ((1<<shift) != scale) {
        return INVALID_IMM;
    }

    return (shift << 7) | (reg);
}

int ARMAssembler::genInt(int reg, ARMWord imm, bool positive)
{
    // Step1: Search a non-immediate part
    ARMWord mask;
    ARMWord imm1;
    ARMWord imm2;
    int rol;

    mask = 0xff000000;
    rol = 8;
    while(1) {
        if ((imm & mask) == 0) {
            imm = (imm << rol) | (imm >> (32 - rol));
            rol = 4 + (rol >> 1);
            break;
        }
        rol += 2;
        mask >>= 2;
        if (mask & 0x3) {
            // rol 8
            imm = (imm << 8) | (imm >> 24);
            mask = 0xff00;
            rol = 24;
            while (1) {
                if ((imm & mask) == 0) {
                    imm = (imm << rol) | (imm >> (32 - rol));
                    rol = (rol >> 1) - 8;
                    break;
                }
                rol += 2;
                mask >>= 2;
                if (mask & 0x3)
                    return 0;
            }
            break;
        }
    }

    ASSERT((imm & 0xff) == 0);

    if ((imm & 0xff000000) == 0) {
        imm1 = OP2_IMM | ((imm >> 16) & 0xff) | (((rol + 4) & 0xf) << 8);
        imm2 = OP2_IMM | ((imm >> 8) & 0xff) | (((rol + 8) & 0xf) << 8);
    } else if (imm & 0xc0000000) {
        imm1 = OP2_IMM | ((imm >> 24) & 0xff) | ((rol & 0xf) << 8);
        imm <<= 8;
        rol += 4;

        if ((imm & 0xff000000) == 0) {
            imm <<= 8;
            rol += 4;
        }

        if ((imm & 0xf0000000) == 0) {
            imm <<= 4;
            rol += 2;
        }

        if ((imm & 0xc0000000) == 0) {
            imm <<= 2;
            rol += 1;
        }

        if ((imm & 0x00ffffff) == 0)
            imm2 = OP2_IMM | (imm >> 24) | ((rol & 0xf) << 8);
        else
            return 0;
    } else {
        if ((imm & 0xf0000000) == 0) {
            imm <<= 4;
            rol += 2;
        }

        if ((imm & 0xc0000000) == 0) {
            imm <<= 2;
            rol += 1;
        }

        imm1 = OP2_IMM | ((imm >> 24) & 0xff) | ((rol & 0xf) << 8);
        imm <<= 8;
        rol += 4;

        if ((imm & 0xf0000000) == 0) {
            imm <<= 4;
            rol += 2;
        }

        if ((imm & 0xc0000000) == 0) {
            imm <<= 2;
            rol += 1;
        }

        if ((imm & 0x00ffffff) == 0)
            imm2 = OP2_IMM | (imm >> 24) | ((rol & 0xf) << 8);
        else
            return 0;
    }

    if (positive) {
        mov_r(reg, imm1);
        orr_r(reg, reg, imm2);
    } else {
        mvn_r(reg, imm1);
        bic_r(reg, reg, imm2);
    }

    return 1;
}

#ifdef __GNUC__
// If the result of this function isn't used, the caller should probably be
// using movImm.
__attribute__((warn_unused_result))
#endif
ARMWord ARMAssembler::getImm(ARMWord imm, int tmpReg, bool invert)
{
    ARMWord tmp;

    // Do it by 1 instruction
    tmp = getOp2(imm);
    if (tmp != INVALID_IMM)
        return tmp;

    tmp = getOp2(~imm);
    if (tmp != INVALID_IMM) {
        if (invert)
            return tmp | OP2_INV_IMM;
        mvn_r(tmpReg, tmp);
        return tmpReg;
    }

    return encodeComplexImm(imm, tmpReg);
}

void ARMAssembler::moveImm(ARMWord imm, int dest)
{
    ARMWord tmp;

    // Do it by 1 instruction
    tmp = getOp2(imm);
    if (tmp != INVALID_IMM) {
        mov_r(dest, tmp);
        return;
    }

    tmp = getOp2(~imm);
    if (tmp != INVALID_IMM) {
        mvn_r(dest, tmp);
        return;
    }

    encodeComplexImm(imm, dest);
}

ARMWord ARMAssembler::encodeComplexImm(ARMWord imm, int dest)
{
#if WTF_ARM_ARCH_VERSION >= 7
    ARMWord tmp = getImm16Op2(imm);
    if (tmp != INVALID_IMM) {
        movw_r(dest, tmp);
        return dest;
    }
    movw_r(dest, getImm16Op2(imm & 0xffff));
    movt_r(dest, getImm16Op2(imm >> 16));
    return dest;
#else
    // Do it by 2 instruction
    if (genInt(dest, imm, true))
        return dest;
    if (genInt(dest, ~imm, false))
        return dest;

    ldr_imm(dest, imm);
    return dest;
#endif
}

// Memory load/store helpers
// TODO: this does not take advantage of all of ARMv7's instruction encodings, it should.
void ARMAssembler::dataTransferN(bool isLoad, bool isSigned, int size, RegisterID rt, RegisterID base, int32_t offset)
{
    bool posOffset = true;

    // There may be more elegant ways of handling this, but this one works.
    if (offset == 0x80000000) {
        // For even bigger offsets, load the entire offset into a register, then do an
        // indexed load using the base register and the index register.
        moveImm(offset, ARMRegisters::S0);
        mem_reg_off(isLoad, isSigned, size, posOffset, rt, base, ARMRegisters::S0);
        return;
    }
    if (offset < 0) {
        offset = - offset;
        posOffset = false;
    }
    if (offset <= 0xfff) {
        // LDR rd, [rb, #+offset]
        mem_imm_off(isLoad, isSigned, size, posOffset, rt, base, offset);
    } else if (offset <= 0xfffff) {
        // Add upper bits of offset to the base, and store the result into the temp register.
        if (posOffset) {
            add_r(ARMRegisters::S0, base, OP2_IMM | (offset >> 12) | getOp2RotLSL(12));
        } else {
            sub_r(ARMRegisters::S0, base, OP2_IMM | (offset >> 12) | getOp2RotLSL(12));
        }
        // Load using the lower bits of the offset.
        mem_imm_off(isLoad, isSigned, size, posOffset, rt,
                    ARMRegisters::S0, (offset & 0xfff));
    } else {
        // For even bigger offsets, load the entire offset into a register, then do an
        // indexed load using the base register and the index register.
        moveImm(offset, ARMRegisters::S0);
        mem_reg_off(isLoad, isSigned, size, posOffset, rt, base, ARMRegisters::S0);
    }
}

void ARMAssembler::dataTransfer32(bool isLoad, RegisterID srcDst, RegisterID base, int32_t offset)
{
    if (offset >= 0) {
        if (offset <= 0xfff)
            // LDR rd, [rb, +offset]
            dtr_u(isLoad, srcDst, base, offset);
        else if (offset <= 0xfffff) {
            // Add upper bits of offset to the base, and store the result into the temp register.
            add_r(ARMRegisters::S0, base, OP2_IMM | (offset >> 12) | getOp2RotLSL(12));
            // Load using the lower bits of the register.
            dtr_u(isLoad, srcDst, ARMRegisters::S0, (offset & 0xfff));
        } else {
            // For even bigger offsets, load the entire offset into a register, then do an
            // indexed load using the base register and the index register.
            moveImm(offset, ARMRegisters::S0);
            dtr_ur(isLoad, srcDst, base, ARMRegisters::S0);
        }
    } else {
        // Negative offsets.
        offset = -offset;
        if (offset <= 0xfff)
            dtr_d(isLoad, srcDst, base, offset);
        else if (offset <= 0xfffff) {
            sub_r(ARMRegisters::S0, base, OP2_IMM | (offset >> 12) | getOp2RotLSL(12));
            dtr_d(isLoad, srcDst, ARMRegisters::S0, (offset & 0xfff));
        } else {
            moveImm(offset, ARMRegisters::S0);
            dtr_dr(isLoad, srcDst, base, ARMRegisters::S0);
        }
    }
}
/* this is large, ugly and obsolete.  dataTransferN is superior.*/
void ARMAssembler::dataTransfer8(bool isLoad, RegisterID srcDst, RegisterID base, int32_t offset, bool isSigned)
{
    if (offset >= 0) {
        if (offset <= 0xfff) {
            if (isSigned)
                mem_imm_off(isLoad, true, 8, true, srcDst, base, offset);
            else
                dtrb_u(isLoad, srcDst, base, offset);
        } else if (offset <= 0xfffff) {
            add_r(ARMRegisters::S0, base, OP2_IMM | (offset >> 12) | getOp2RotLSL(12));
            if (isSigned)
                mem_imm_off(isLoad, true, 8, true, srcDst, ARMRegisters::S0, (offset & 0xfff));
            else
                dtrb_u(isLoad, srcDst, ARMRegisters::S0, (offset & 0xfff));
        } else {
            moveImm(offset, ARMRegisters::S0);
            if (isSigned)
                mem_reg_off(isLoad, true, 8, true, srcDst, base, ARMRegisters::S0);
            else
                dtrb_ur(isLoad, srcDst, base, ARMRegisters::S0);
        }
    } else {
        offset = -offset;
        if (offset <= 0xfff) {
            if (isSigned)
                mem_imm_off(isLoad, true, 8, false, srcDst, base, offset);
            else
                dtrb_d(isLoad, srcDst, base, offset);
        }
        else if (offset <= 0xfffff) {
            sub_r(ARMRegisters::S0, base, OP2_IMM | (offset >> 12) | getOp2RotLSL(12));
            if (isSigned)
                mem_imm_off(isLoad, true, 8, false, srcDst, ARMRegisters::S0, (offset & 0xfff));
            else
                dtrb_d(isLoad, srcDst, ARMRegisters::S0, (offset & 0xfff));

        } else {
            moveImm(offset, ARMRegisters::S0);
            if (isSigned)
                mem_reg_off(isLoad, true, 8, false, srcDst, base, ARMRegisters::S0);
            else
                dtrb_dr(isLoad, srcDst, base, ARMRegisters::S0);
                
        }
    }
}

// rather X86-like, implements dest <- [base, index * shift + offset]
void ARMAssembler::baseIndexTransfer32(bool isLoad, RegisterID srcDst, RegisterID base, RegisterID index, int scale, int32_t offset)
{
    ARMWord op2;

    ASSERT(scale >= 0 && scale <= 3);
    op2 = lsl(index, scale);

    if (offset >= 0 && offset <= 0xfff) {
        add_r(ARMRegisters::S0, base, op2);
        dtr_u(isLoad, srcDst, ARMRegisters::S0, offset);
        return;
    }
    if (offset <= 0 && offset >= -0xfff) {
        add_r(ARMRegisters::S0, base, op2);
        dtr_d(isLoad, srcDst, ARMRegisters::S0, -offset);
        return;
    }

    ldr_un_imm(ARMRegisters::S0, offset);
    add_r(ARMRegisters::S0, ARMRegisters::S0, op2);
    dtr_ur(isLoad, srcDst, base, ARMRegisters::S0);
}

void ARMAssembler::baseIndexTransferN(bool isLoad, bool isSigned, int size, RegisterID srcDst, RegisterID base, RegisterID index, int scale, int32_t offset)
{
    ARMWord op2;

    ASSERT(scale >= 0 && scale <= 3);
    op2 = lsl(index, scale);

    if (offset >= -0xfff && offset <= 0xfff) {
        add_r(ARMRegisters::S0, base, op2);
        bool posOffset = true;
        if (offset < 0) {
            posOffset = false;
            offset = -offset;
        }
        mem_imm_off(isLoad, isSigned, size, posOffset, srcDst, ARMRegisters::S0, offset);
        return;
    }
    ldr_un_imm(ARMRegisters::S0, offset);
    add_r(ARMRegisters::S0, ARMRegisters::S0, op2);
    mem_reg_off(isLoad, isSigned, size, true, srcDst, base, ARMRegisters::S0);
}

void ARMAssembler::doubleTransfer(bool isLoad, FPRegisterID srcDst, RegisterID base, int32_t offset)
{
    // VFP cannot directly access memory that is not four-byte-aligned, so
    // special-case support will be required for such cases. However, we don't
    // currently use any unaligned floating-point memory accesses and probably
    // never will, so for now just assert that the offset is aligned.
    //
    // Note that we cannot assert that the base register is aligned, but in
    // that case, an alignment fault will be raised at run-time.
    ASSERT((offset & 0x3) == 0);

    // Try to use a single load/store instruction, or at least a simple address
    // calculation.
    if (offset >= 0) {
        if (offset <= 0x3ff) {
            fmem_imm_off(isLoad, true, true, srcDst, base, offset >> 2);
            return;
        }
        if (offset <= 0x3ffff) {
            add_r(ARMRegisters::S0, base, OP2_IMM | (offset >> 10) | getOp2RotLSL(10));
            fmem_imm_off(isLoad, true, true, srcDst, ARMRegisters::S0, (offset >> 2) & 0xff);
            return;
        }
    } else {
        if (offset >= -0x3ff) {
            fmem_imm_off(isLoad, true, false, srcDst, base, -offset >> 2);
            return;
        }
        if (offset >= -0x3ffff) {
            sub_r(ARMRegisters::S0, base, OP2_IMM | (-offset >> 10) | getOp2RotLSL(10));
            fmem_imm_off(isLoad, true, false, srcDst, ARMRegisters::S0, (-offset >> 2) & 0xff);
            return;
        }
    }

    // Slow case for long-range accesses.
    ldr_un_imm(ARMRegisters::S0, offset);
    add_r(ARMRegisters::S0, ARMRegisters::S0, base);
    fmem_imm_off(isLoad, true, true, srcDst, ARMRegisters::S0, 0);
}

void ARMAssembler::doubleTransfer(bool isLoad, FPRegisterID srcDst, RegisterID base, int32_t offset, RegisterID index, int32_t scale)
{
    // This variant accesses memory at base+offset+(index*scale). VLDR and VSTR
    // don't have such an addressing mode, so this access will require some
    // arithmetic instructions.

    // This method does not support accesses that are not four-byte-aligned.
    ASSERT((offset & 0x3) == 0);

    // Catch the trivial case, where scale is 0.
    if (scale == 0) {
        doubleTransfer(isLoad, srcDst, base, offset);
        return;
    }

    // Calculate the address, excluding the non-scaled offset. This is
    // efficient for scale factors that are powers of two.
    ARMWord op2_index = getOp2RegScale(index, scale);
    if (op2_index == INVALID_IMM) {
        // Use MUL to calculate scale factors that are not powers of two.
        moveImm(scale, ARMRegisters::S0);
        mul_r(ARMRegisters::S0, index, ARMRegisters::S0);
        op2_index = ARMRegisters::S0;
    }

    add_r(ARMRegisters::S0, base, op2_index);
    doubleTransfer(isLoad, srcDst, ARMRegisters::S0, offset);
}

void ARMAssembler::floatTransfer(bool isLoad, FPRegisterID srcDst, RegisterID base, int32_t offset)
{
    // Assert that the access is aligned, as in doubleTransfer.
    ASSERT((offset & 0x3) == 0);

    // Try to use a single load/store instruction, or at least a simple address
    // calculation.
    if (offset >= 0) {
        if (offset <= 0x3ff) {
            fmem_imm_off(isLoad, false, true, srcDst, base, offset >> 2);
            return;
        }
        if (offset <= 0x3ffff) {
            add_r(ARMRegisters::S0, base, OP2_IMM | (offset >> 10) | getOp2RotLSL(10));
            fmem_imm_off(isLoad, false, true, srcDst, ARMRegisters::S0, (offset >> 2) & 0xff);
            return;
        }
    } else {
        if (offset >= -0x3ff) {
            fmem_imm_off(isLoad, false, false, srcDst, base, -offset >> 2);
            return;
        }
        if (offset >= -0x3ffff) {
            sub_r(ARMRegisters::S0, base, OP2_IMM | (-offset >> 10) | getOp2RotLSL(10));
            fmem_imm_off(isLoad, false, false, srcDst, ARMRegisters::S0, (-offset >> 2) & 0xff);
            return;
        }
    }

    // Slow case for long-range accesses.
    ldr_un_imm(ARMRegisters::S0, offset);
    add_r(ARMRegisters::S0, ARMRegisters::S0, base);
    fmem_imm_off(isLoad, false, true, srcDst, ARMRegisters::S0, 0);
}

void ARMAssembler::baseIndexFloatTransfer(bool isLoad, bool isDouble, FPRegisterID srcDst, RegisterID base, RegisterID index, int scale, int32_t offset)
{
    ARMWord op2;

    ASSERT(scale >= 0 && scale <= 3);
    op2 = lsl(index, scale);
    // vldr/vstr have a more restricted range than your standard ldr.
    // they get 8 bits that are implicitly shifted left by 2.
    if (offset >= -(0xff<<2) && offset <= (0xff<<2)) {
        add_r(ARMRegisters::S0, base, op2);
        bool posOffset = true;
        if (offset < 0) {
            posOffset = false;
            offset = -offset;
        }
        fmem_imm_off(isLoad, isDouble, posOffset, srcDst, ARMRegisters::S0, offset >> 2);
        return;
    }

    ldr_un_imm(ARMRegisters::S0, offset);
    // vldr/vstr do not allow register-indexed operations, so we get to do this *manually*.
    add_r(ARMRegisters::S0, ARMRegisters::S0, op2);
    add_r(ARMRegisters::S0, ARMRegisters::S0, base);

    fmem_imm_off(isLoad, isDouble, true, srcDst, ARMRegisters::S0, 0);
}

// Fix up the offsets and literal-pool loads in buffer. The buffer should
// already contain the code from m_buffer.
inline void ARMAssembler::fixUpOffsets(void * buffer)
{
    char * data = reinterpret_cast<char *>(buffer);
    for (Jumps::Iterator iter = m_jumps.begin(); iter != m_jumps.end(); ++iter) {
        // The last bit is set if the constant must be placed on constant pool.
        int pos = (*iter) & (~0x1);
        ARMWord* ldrAddr = reinterpret_cast<ARMWord*>(data + pos);
        ARMWord* addr = getLdrImmAddress(ldrAddr);
        if (*addr != InvalidBranchTarget) {
// The following is disabled for JM because we patch some branches after
// calling fixUpOffset, and the branch patcher doesn't know how to handle 'B'
// instructions.
#if 0
            if (!(*iter & 1)) {
                int diff = reinterpret_cast<ARMWord*>(data + *addr) - (ldrAddr + DefaultPrefetching);

                if ((diff <= BOFFSET_MAX && diff >= BOFFSET_MIN)) {
                    *ldrAddr = B | getConditionalField(*ldrAddr) | (diff & BRANCH_MASK);
                    continue;
                }
            }
#endif
            *addr = reinterpret_cast<ARMWord>(data + *addr);
        }
    }
}

void* ARMAssembler::executableAllocAndCopy(ExecutableAllocator* allocator, ExecutablePool **poolp, CodeKind kind)
{
    // 64-bit alignment is required for next constant pool and JIT code as well
    m_buffer.flushWithoutBarrier(true);
    if (m_buffer.uncheckedSize() & 0x7)
        bkpt(0);

    void * data = m_buffer.executableAllocAndCopy(allocator, poolp, kind);
    if (data)
        fixUpOffsets(data);
    return data;
}

// This just dumps the code into the specified buffer, fixing up absolute
// offsets and literal pool loads as it goes. The buffer is assumed to be large
// enough to hold the code, and any pre-existing literal pool is assumed to
// have been flushed.
void ARMAssembler::executableCopy(void * buffer)
{
    ASSERT(m_buffer.sizeOfConstantPool() == 0);
    memcpy(buffer, m_buffer.data(), m_buffer.size());
    fixUpOffsets(buffer);
}

} // namespace JSC

#endif // ENABLE(ASSEMBLER) && CPU(ARM_TRADITIONAL)
