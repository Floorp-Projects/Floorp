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
 *   David Anderson <danderson@mozilla.com>
 *   David Mandelin <dmandelin@mozilla.com>
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

#if !defined jsjaeger_mono_ic_h__ && defined JS_METHODJIT && defined JS_MONOIC
#define jsjaeger_mono_ic_h__

#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/CodeLocation.h"
#include "methodjit/MethodJIT.h"
#include "CodeGenIncludes.h"

namespace js {
namespace mjit {
namespace ic {

struct MICInfo {
#ifdef JS_CPU_X86
    static const uint32 GET_DATA_OFFSET = 6;
    static const uint32 GET_TYPE_OFFSET = 12;

    static const uint32 SET_TYPE_OFFSET = 6;
    static const uint32 SET_DATA_CONST_TYPE_OFFSET = 16;
    static const uint32 SET_DATA_TYPE_OFFSET = 12;
#elif JS_CPU_X64 || JS_CPU_ARM
    /* X64: No constants used, thanks to patchValueOffset. */
    /* ARM: No constants used as mic.load always points to an LDR that loads the offset. */
#endif

    enum Kind
#ifdef _MSC_VER
    : uint8_t
#endif
    {
        GET,
        SET,
        CALL,
        EMPTYCALL,  /* placeholder call which cannot refer to a fast native */
        TRACER
    };

    /* Used by multiple MICs. */
    JSC::CodeLocationLabel entry;
    JSC::CodeLocationLabel stubEntry;

    /* TODO: use a union-like structure for the below. */

    /* Used by GET/SET. */
    JSC::CodeLocationLabel load;
    JSC::CodeLocationDataLabel32 shape;
    JSC::CodeLocationCall stubCall;
#if defined JS_PUNBOX64
    uint32 patchValueOffset;
#endif

    /* Used by CALL. */
    uint32 argc;
    uint32 frameDepth;
    JSC::CodeLocationLabel knownObject;
    JSC::CodeLocationLabel callEnd;
    JSC::MacroAssembler::RegisterID dataReg;

    /* Used by TRACER. */
    JSC::CodeLocationJump traceHint;
    JSC::CodeLocationJump slowTraceHint;

    /* Used by all MICs. */
    Kind kind : 4;
    union {
        /* Used by GET/SET. */
        struct {
            bool touched : 1;
            bool typeConst : 1;
            bool dataConst : 1;
        } name;
        /* Used by CALL. */
        bool generated;
        /* Used by TRACER. */
        bool hasSlowTraceHint;
    } u;
};

void JS_FASTCALL GetGlobalName(VMFrame &f, uint32 index);
void JS_FASTCALL SetGlobalName(VMFrame &f, uint32 index);

#ifdef JS_CPU_X86

/* Compiler for generating fast paths for a MIC'ed native call. */
class NativeCallCompiler
{
    typedef JSC::MacroAssembler::Jump Jump;

    struct Patch {
        Patch(Jump from, uint8 *to)
          : from(from), to(to)
        { }

        Jump from;
        uint8 *to;
    };

  public:
    Assembler masm;

  private:
    /* :TODO: oom check */
    Vector<Patch, 8, SystemAllocPolicy> jumps;

  public:
    NativeCallCompiler();

    size_t size() { return masm.size(); }
    uint8 *buffer() { return masm.buffer(); }

    /* Exit from the call path to target. */
    void addLink(Jump j, uint8 *target) { jumps.append(Patch(j, target)); }

    /*
     * Finish up this native, and add an incoming jump from start
     * and an outgoing jump to fallthrough.
     */
    void finish(JSScript *script, uint8 *start, uint8 *fallthrough);
};

void CallNative(JSContext *cx, JSScript *script, MICInfo &mic, JSFunction *fun, bool isNew);

#endif /* JS_CPU_X86 */

void PurgeMICs(JSContext *cx, JSScript *script);

} /* namespace ic */
} /* namespace mjit */
} /* namespace js */

#endif /* jsjaeger_mono_ic_h__ */

