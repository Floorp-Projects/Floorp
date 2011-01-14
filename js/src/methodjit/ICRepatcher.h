/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Mandelin <dmandelin@mozilla.com>
 *   David Anderson <danderson@mozilla.com>
 *   Chris Leary <cdleary@mozilla.com>
 *   Jacob Bramley <Jacob.Bramely@arm.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
    explicit Repatcher(JITScript *js)
      : JSC::RepatchBuffer(js->code), label(js->code.m_code.executableAddress())
    { }

    explicit Repatcher(const JSC::JITCode &code)
      : JSC::RepatchBuffer(code), label(code.start())
    { }

    using JSC::RepatchBuffer::relink;

    /* Patch a stub call. */
    void relink(CodeLocationCall call, FunctionPtr stub) {
#if defined JS_CPU_X64 || defined JS_CPU_X86
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
#else
# error
#endif
    }

    /* Patch the offset of a Value load emitted by loadValueWithAddressOffsetPatch. */
    void patchAddressOffsetForValueLoad(CodeLocationLabel label, uint32 offset) {
#if defined JS_CPU_X64 || defined JS_CPU_ARM
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

    void patchAddressOffsetForValueStore(CodeLocationLabel label, uint32 offset, bool typeConst) {
#if defined JS_CPU_ARM || defined JS_CPU_X64
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
