/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jsjaeger_icrepatcher_h__ && defined JS_METHODJIT
#define jsjaeger_icrepatcher_h__

#include "assembler/assembler/RepatchBuffer.h"
#include "assembler/moco/MocoStubs.h"
#include "methodjit/ICChecker.h"

namespace js {
namespace mjit {
namespace ic {

class Repatcher : public JSC::RepatchBuffer
{
    typedef JSC::CodeLocationLabel  CodeLocationLabel;
    typedef JSC::CodeLocationCall   CodeLocationCall;
    typedef JSC::FunctionPtr        FunctionPtr;

    CodeLocationLabel label;

  public:
    explicit Repatcher(JITChunk *js)
      : JSC::RepatchBuffer(js->code), label(js->code.m_code.executableAddress())
    { }

    explicit Repatcher(const JSC::JITCode &code)
      : JSC::RepatchBuffer(code), label(code.start())
    { }

    using JSC::RepatchBuffer::relink;

    /* Patch a stub call. */
    void relink(CodeLocationCall call, FunctionPtr stub) {
#if defined JS_CPU_X64 || defined JS_CPU_X86 || defined JS_CPU_SPARC
        JSC::RepatchBuffer::relink(call, stub);
#elif defined JS_CPU_ARM
        /*
         * Stub calls on ARM look like this:
         *
         *                  ldr     ip, =stub
         * call label ->    ldr     r8, =JaegerStubVeneer
         *                  blx     r8
         *
         * ARM has to run stub calls through a veneer in order for THROW to
         * work properly. The address that must be patched is the load into
         * 'ip', not the load into 'r8'.
         */
        CheckIsStubCall(call.labelAtOffset(0));
        JSC::RepatchBuffer::relink(call.callAtOffset(-4), stub);
#elif defined JS_CPU_MIPS
        /*
         * Stub calls on MIPS look like this:
         *
         *                  lui     v0, hi(stub)
         *                  ori     v0, v0, lo(stub)
         *                  lui     t9, hi(JaegerStubVeneer)
         *                  ori     t9, t9, lo(JaegerStubVeneer)
         *                  jalr    t9
         *                  nop
         * call label ->    xxx
         *
         * MIPS has to run stub calls through a veneer in order for THROW to
         * work properly. The address that must be patched is the load into
         * 'v0', not the load into 't9'.
         */
        JSC::RepatchBuffer::relink(call.callAtOffset(-8), stub);
#else
# error
#endif
    }

    /* Patch the offset of a Value load emitted by loadValueWithAddressOffsetPatch. */
    void patchAddressOffsetForValueLoad(CodeLocationLabel label, uint32_t offset) {
#if defined JS_CPU_X64 || defined JS_CPU_ARM || defined JS_CPU_SPARC || defined JS_CPU_MIPS
        repatch(label.dataLabel32AtOffset(0), offset);
#elif defined JS_CPU_X86
        static const unsigned LOAD_TYPE_OFFSET = 6;
        static const unsigned LOAD_DATA_OFFSET = 12;

        /*
         * We have the following sequence to patch:
         *
         *      mov     <offset+4>($base), %<type>
         *      mov     <offset+0>($base), %<data>
         */
        repatch(label.dataLabel32AtOffset(LOAD_DATA_OFFSET), offset);
        repatch(label.dataLabel32AtOffset(LOAD_TYPE_OFFSET), offset + 4);
#else
# error
#endif
    }

    void patchAddressOffsetForValueStore(CodeLocationLabel label, uint32_t offset, bool typeConst) {
#if defined JS_CPU_ARM || defined JS_CPU_X64 || defined JS_CPU_SPARC || defined JS_CPU_MIPS
        (void) typeConst;
        repatch(label.dataLabel32AtOffset(0), offset);
#elif defined JS_CPU_X86
        static const unsigned STORE_TYPE_OFFSET = 6;
        static const unsigned STORE_DATA_CONST_TYPE_OFFSET = 16;
        static const unsigned STORE_DATA_TYPE_OFFSET = 12;

        /*
         * The type is stored first, then the payload. Both stores can vary in
         * size, depending on whether or not the data is a constant in the
         * instruction stream (though only the first store matters for the
         * purpose of locating both offsets for patching).
         *
         * We have one of the following sequences to patch. Offsets are located
         * before 6B into a given move instruction, but the mov instructions
         * carrying constant payloads are 10B wide overall.
         *
         *  typeConst=false, dataConst=false
         *      mov     %<type>, <offset+4>($base)  ; Length is 6
         *      mov     %<data>, <offset+0>($base)  ; Offset @ len(prev) + 6 = 12
         *  typeConst=true, dataConst=false
         *      mov     $<type>, <offset+4>($base)  ; Length is 10
         *      mov     %<data>, <offset+0>($base)  ; Offset @ len(prev) + 6 = 16
         *  typeConst=true, dataConst=true
         *      mov     $<type>, <offset+4>($base)  ; Length is 10
         *      mov     $<data>, <offset+0>($base)  ; Offset @ len(prev) + 6 = 16
         *
         * Note that we only need to know whether type is const to determine the
         * correct patch offsets. In all cases, the label points to the start
         * of the sequence.
         */
        repatch(label.dataLabel32AtOffset(STORE_TYPE_OFFSET), offset + 4);

        unsigned payloadOffset = typeConst ? STORE_DATA_CONST_TYPE_OFFSET : STORE_DATA_TYPE_OFFSET;
        repatch(label.dataLabel32AtOffset(payloadOffset), offset);
#else
# error
#endif
    }
};

} /* namespace ic */
} /* namespace mjit */
} /* namespace js */

#endif
