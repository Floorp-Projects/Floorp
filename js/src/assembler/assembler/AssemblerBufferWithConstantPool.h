/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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

#ifndef AssemblerBufferWithConstantPool_h
#define AssemblerBufferWithConstantPool_h

#include "assembler/wtf/Platform.h"

#if ENABLE_ASSEMBLER

#include "AssemblerBuffer.h"
#include "assembler/wtf/SegmentedVector.h"
#include "assembler/wtf/Assertions.h"

#include "jsnum.h"
#define ASSEMBLER_HAS_CONSTANT_POOL 1

namespace JSC {

/*
    On a constant pool 4 or 8 bytes data can be stored. The values can be
    constants or addresses. The addresses should be 32 or 64 bits. The constants
    should be double-precisions float or integer numbers which are hard to be
    encoded as few machine instructions.

    TODO: The pool is desinged to handle both 32 and 64 bits values, but
    currently only the 4 bytes constants are implemented and tested.

    The AssemblerBuffer can contain multiple constant pools. Each pool is inserted
    into the instruction stream - protected by a jump instruction from the
    execution flow.

    The flush mechanism is called when no space remain to insert the next instruction
    into the pool. Three values are used to determine when the constant pool itself
    have to be inserted into the instruction stream (Assembler Buffer):

    - maxPoolSize: size of the constant pool in bytes, this value cannot be
        larger than the maximum offset of a PC relative memory load

    - barrierSize: size of jump instruction in bytes which protects the
        constant pool from execution

    - maxInstructionSize: maximum length of a machine instruction in bytes

    There are some callbacks which solve the target architecture specific
    address handling:

    - TYPE patchConstantPoolLoad(TYPE load, int value):
        patch the 'load' instruction with the index of the constant in the
        constant pool and return the patched instruction.

    - void patchConstantPoolLoad(void* loadAddr, void* constPoolAddr):
        patch the a PC relative load instruction at 'loadAddr' address with the
        final relative offset. The offset can be computed with help of
        'constPoolAddr' (the address of the constant pool) and index of the
        constant (which is stored previously in the load instruction itself).

    - TYPE placeConstantPoolBarrier(int size):
        return with a constant pool barrier instruction which jumps over the
        constant pool.

    The 'put*WithConstant*' functions should be used to place a data into the
    constant pool.
*/

template <int maxPoolSize, int barrierSize, int maxInstructionSize, class AssemblerType>
class AssemblerBufferWithConstantPool: public AssemblerBuffer {
    typedef SegmentedVector<uint32_t, 512> LoadOffsets;
public:
    enum {
        UniqueConst,
        ReusableConst,
        UnusedEntry
    };

    AssemblerBufferWithConstantPool()
        : AssemblerBuffer()
        , m_numConsts(0)
        , m_maxDistance(maxPoolSize)
        , m_lastConstDelta(0)
        , m_flushCount(0)
    {
        m_pool = static_cast<uint32_t*>(malloc(maxPoolSize));
        m_mask = static_cast<char*>(malloc(maxPoolSize / sizeof(uint32_t)));
    }

    ~AssemblerBufferWithConstantPool()
    {
        free(m_mask);
        free(m_pool);
    }

    void ensureSpace(int space)
    {
        flushIfNoSpaceFor(space);
        AssemblerBuffer::ensureSpace(space);
    }

    void ensureSpace(int insnSpace, int constSpace)
    {
        flushIfNoSpaceFor(insnSpace, constSpace);
        AssemblerBuffer::ensureSpace(insnSpace);
    }

    bool isAligned(int alignment)
    {
        flushIfNoSpaceFor(alignment);
        return AssemblerBuffer::isAligned(alignment);
    }

    void putByteUnchecked(int value)
    {
        AssemblerBuffer::putByteUnchecked(value);
        correctDeltas(1);
    }

    void putByte(int value)
    {
        flushIfNoSpaceFor(1);
        AssemblerBuffer::putByte(value);
        correctDeltas(1);
    }

    void putShortUnchecked(int value)
    {
        AssemblerBuffer::putShortUnchecked(value);
        correctDeltas(2);
    }

    void putShort(int value)
    {
        flushIfNoSpaceFor(2);
        AssemblerBuffer::putShort(value);
        correctDeltas(2);
    }

    // Puts 1 word worth of data into the instruction stream
    void putIntUnchecked(int value)
    {
        AssemblerBuffer::putIntUnchecked(value);
        correctDeltas(4);
    }
    // Puts one word worth of data into the instruction stream, and makes sure
    // there is enough space to place it, dumping the constant pool if there isn't
    void putInt(int value)
    {
        flushIfNoSpaceFor(4);
        AssemblerBuffer::putInt(value);
        correctDeltas(4);
    }
    // puts 64 bits worth of data into the instruction stream
    void putInt64Unchecked(int64_t value)
    {
        AssemblerBuffer::putInt64Unchecked(value);
        correctDeltas(8);
    }

    int size()
    {
        flushIfNoSpaceFor(maxInstructionSize, sizeof(uint64_t));
        return AssemblerBuffer::size();
    }

    int uncheckedSize() const
    {
        return AssemblerBuffer::size();
    }

    // copy all of our instructions and pools into their final location
    void* executableAllocAndCopy(ExecutableAllocator* allocator, ExecutablePool** poolp, CodeKind kind)
    {
        flushConstantPool(false);
        return AssemblerBuffer::executableAllocAndCopy(allocator, poolp, kind);
    }

    // places 1 int worth of data into a pool, and mashes an instruction into place to
    // hold this offset.
    // the caller of putIntWithConstantInt passes in some token that represents an
    // instruction, as well as the raw data that is to be placed in the pool.
    // Traditionally, this 'token' has been the instruction that we wish to encode
    // in the end, however, I have started encoding it in a much simpler manner,
    // using bitfields and a fairly flat representation.
    void putIntWithConstantInt(uint32_t insn, uint32_t constant, bool isReusable = false)
    {
        flushIfNoSpaceFor(4, 4);

        m_loadOffsets.append(AssemblerBuffer::size());
        if (isReusable)
            for (int i = 0; i < m_numConsts; ++i) {
                if (m_mask[i] == ReusableConst && m_pool[i] == constant) {
                    AssemblerBuffer::putInt(AssemblerType::patchConstantPoolLoad(insn, i));
                    correctDeltas(4);
                    return;
                }
            }

        m_pool[m_numConsts] = constant;
        m_mask[m_numConsts] = static_cast<char>(isReusable ? ReusableConst : UniqueConst);

        AssemblerBuffer::putInt(AssemblerType::patchConstantPoolLoad(insn, m_numConsts));
        ++m_numConsts;

        correctDeltas(4, 4);
    }

    void putIntWithConstantDouble(uint32_t insn, double constant)
    {
        flushIfNoSpaceFor(4, 8);

        m_loadOffsets.append(AssemblerBuffer::size());
        bool isReusable = false;

        union DoublePun {
            struct {
#if defined(IS_LITTLE_ENDIAN)
                uint32_t lo, hi;
#else
                uint32_t hi, lo;
#endif
            } s;
            double d;
        } dpun;

        dpun.d = constant;
        
        m_pool[m_numConsts] = dpun.s.lo;
        m_pool[m_numConsts+1] = dpun.s.hi;
        m_mask[m_numConsts] = static_cast<char>(isReusable ? ReusableConst : UniqueConst);
        m_mask[m_numConsts+1] = static_cast<char>(isReusable ? ReusableConst : UniqueConst);

        AssemblerBuffer::putInt(AssemblerType::patchConstantPoolLoad(insn, m_numConsts));
        m_numConsts+=2;

        correctDeltas(4, 8);
    }

    // This flushing mechanism can be called after any unconditional jumps.
    void flushWithoutBarrier(bool isForced = false)
    {
        // Flush if constant pool is more than 60% full to avoid overuse of this function.
        if (isForced || (5 * m_numConsts * sizeof(uint32_t)) > (3 * maxPoolSize))
            flushConstantPool(false);
    }

    // return the address of the pool; we really shouldn't be using this.
    uint32_t* poolAddress()
    {
        return m_pool;
    }

    // how many constants have been placed into the pool thusfar?
    int sizeOfConstantPool()
    {
        return m_numConsts;
    }

    int flushCount()
    {
        return m_flushCount;
    }

private:
    void correctDeltas(int insnSize)
    {
        m_maxDistance -= insnSize;
        ASSERT(m_maxDistance >= 0);
        m_lastConstDelta -= insnSize;
        if (m_lastConstDelta < 0)
            m_lastConstDelta = 0;
    }

    void correctDeltas(int insnSize, int constSize)
    {
        correctDeltas(insnSize);

        m_maxDistance -= m_lastConstDelta;
        ASSERT(m_maxDistance >= 0);
        m_lastConstDelta = constSize;
    }

    // place a constant pool after the last instruction placed, and
    // optionally place a jump to ensure we don't start executing the pool.
    void flushConstantPool(bool useBarrier = true)
    {
        GenericAssembler::staticSpew(" -- FLUSHING CONSTANT POOL WITH %d CONSTANTS --\n",
                                     m_numConsts);
        if (m_numConsts == 0)
            return;
        m_flushCount++;
        int alignPool = (AssemblerBuffer::size() + (useBarrier ? barrierSize : 0)) & (sizeof(uint64_t) - 1);

        if (alignPool)
            alignPool = sizeof(uint64_t) - alignPool;

        // Callback to protect the constant pool from execution
        if (useBarrier)
            AssemblerBuffer::putInt(AssemblerType::placeConstantPoolBarrier(m_numConsts * sizeof(uint32_t) + alignPool));

        if (alignPool) {
            if (alignPool & 1)
                AssemblerBuffer::putByte(AssemblerType::padForAlign8);
            if (alignPool & 2)
                AssemblerBuffer::putShort(AssemblerType::padForAlign16);
            if (alignPool & 4)
                AssemblerBuffer::putInt(AssemblerType::padForAlign32);
        }

        int constPoolOffset = AssemblerBuffer::size();
        append(reinterpret_cast<char*>(m_pool), m_numConsts * sizeof(uint32_t));

        // Patch each PC relative load
        for (LoadOffsets::Iterator iter = m_loadOffsets.begin(); iter != m_loadOffsets.end(); ++iter) {
            void* loadAddr = reinterpret_cast<void*>(m_buffer + *iter);
            AssemblerType::patchConstantPoolLoad(loadAddr, reinterpret_cast<void*>(m_buffer + constPoolOffset));
        }

        m_loadOffsets.clear();
        m_numConsts = 0;
        m_maxDistance = maxPoolSize;
        ASSERT(m_maxDistance >= 0);

    }

    void flushIfNoSpaceFor(int nextInsnSize)
    {
        if (m_numConsts == 0) {
            m_maxDistance = maxPoolSize;
            return;
        }
        int lastConstDelta = m_lastConstDelta > nextInsnSize ? m_lastConstDelta - nextInsnSize : 0;
        if ((m_maxDistance < nextInsnSize + lastConstDelta + barrierSize + (int)sizeof(uint32_t)))
            flushConstantPool();
    }

    void flushIfNoSpaceFor(int nextInsnSize, int nextConstSize)
    {
        if (m_numConsts == 0) {
            m_maxDistance = maxPoolSize;
            return;
        }
        if ((m_maxDistance < nextInsnSize + m_lastConstDelta + nextConstSize + barrierSize + (int)sizeof(uint32_t)) ||
            (m_numConsts * sizeof(uint32_t) + nextConstSize >= maxPoolSize))
            flushConstantPool();
    }

    uint32_t* m_pool;
    char* m_mask;
    LoadOffsets m_loadOffsets;

    int m_numConsts;
    int m_maxDistance;
    int m_lastConstDelta;
    int m_flushCount;
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // AssemblerBufferWithConstantPool_h
