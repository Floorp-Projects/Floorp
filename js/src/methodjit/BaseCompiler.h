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
#if !defined jsjaeger_compilerbase_h__ && defined JS_METHODJIT
#define jsjaeger_compilerbase_h__

#include "jscntxt.h"
#include "jstl.h"
#include "assembler/assembler/MacroAssembler.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/assembler/RepatchBuffer.h"
#include "assembler/jit/ExecutableAllocator.h"
#include <limits.h>

namespace js {
namespace mjit {

struct MacroAssemblerTypedefs {
    typedef JSC::MacroAssembler::Label Label;
    typedef JSC::MacroAssembler::Imm32 Imm32;
    typedef JSC::MacroAssembler::ImmPtr ImmPtr;
    typedef JSC::MacroAssembler::RegisterID RegisterID;
    typedef JSC::MacroAssembler::FPRegisterID FPRegisterID;
    typedef JSC::MacroAssembler::Address Address;
    typedef JSC::MacroAssembler::BaseIndex BaseIndex;
    typedef JSC::MacroAssembler::AbsoluteAddress AbsoluteAddress;
    typedef JSC::MacroAssembler MacroAssembler;
    typedef JSC::MacroAssembler::Jump Jump;
    typedef JSC::MacroAssembler::JumpList JumpList;
    typedef JSC::MacroAssembler::Call Call;
    typedef JSC::MacroAssembler::DataLabelPtr DataLabelPtr;
    typedef JSC::MacroAssembler::DataLabel32 DataLabel32;
    typedef JSC::FunctionPtr FunctionPtr;
    typedef JSC::RepatchBuffer RepatchBuffer;
    typedef JSC::CodeLocationLabel CodeLocationLabel;
    typedef JSC::CodeLocationCall CodeLocationCall;
    typedef JSC::ReturnAddressPtr ReturnAddressPtr;
    typedef JSC::MacroAssemblerCodePtr MacroAssemblerCodePtr;
    typedef JSC::JITCode JITCode;
};

class BaseCompiler : public MacroAssemblerTypedefs
{
  protected:
    JSContext *cx;

  public:
    BaseCompiler() : cx(NULL)
    { }

    BaseCompiler(JSContext *cx) : cx(cx)
    { }

  protected:

    JSC::ExecutablePool *
    getExecPool(JSScript *script, size_t size) {
        return BaseCompiler::GetExecPool(cx, script, size);
    }

  public:
    static JSC::ExecutablePool *
    GetExecPool(JSContext *cx, JSScript *script, size_t size) {
        JaegerCompartment *jc = script->compartment->jaegerCompartment;
        JSC::ExecutablePool *pool = jc->poolForSize(size);
        if (!pool)
            js_ReportOutOfMemory(cx);
        return pool;
    }
};

// This class wraps JSC::LinkBuffer for Mozilla-specific memory handling.
// Every return |false| guarantees an OOM that has been correctly propagated,
// and should continue to propagate.
class LinkerHelper : public JSC::LinkBuffer
{
  protected:
    Assembler &masm;
#ifdef DEBUG
    bool verifiedRange;
#endif

  public:
    LinkerHelper(Assembler &masm) : masm(masm)
#ifdef DEBUG
        , verifiedRange(false)
#endif
    { }

    ~LinkerHelper() {
        JS_ASSERT(verifiedRange);
    }

    bool verifyRange(const JSC::JITCode &other) {
#ifdef DEBUG
        verifiedRange = true;
#endif
#ifdef JS_CPU_X64
        uintptr_t lowest = JS_MIN(uintptr_t(m_code), uintptr_t(other.start()));

        uintptr_t myEnd = uintptr_t(m_code) + m_size;
        uintptr_t otherEnd = uintptr_t(other.start()) + other.size();
        uintptr_t highest = JS_MAX(myEnd, otherEnd);

        return (highest - lowest < INT_MAX);
#else
        return true;
#endif
    }

    bool verifyRange(JITScript *jit) {
        return verifyRange(JSC::JITCode(jit->code.m_code.executableAddress(), jit->code.m_size));
    }

    JSC::ExecutablePool *init(JSContext *cx) {
        // The pool is incref'd after this call, so it's necessary to release()
        // on any failure.
        JSScript *script = cx->fp()->script();
        JSC::ExecutablePool *ep = BaseCompiler::GetExecPool(cx, script, masm.size());
        if (!ep)
            return ep;

        m_size = masm.size();
        m_code = executableCopy(masm, ep);
        if (!m_code) {
            ep->release();
            js_ReportOutOfMemory(cx);
            return NULL;
        }
        return ep;
    }

    JSC::CodeLocationLabel finalize() {
        masm.finalize(*this);
        return finalizeCodeAddendum();
    }

    void maybeLink(MaybeJump jump, JSC::CodeLocationLabel label) {
        if (!jump.isSet())
            return;
        link(jump.get(), label);
    }

    size_t size() const {
        return m_size;
    }
};

class Repatcher : public JSC::RepatchBuffer
{
  public:
    Repatcher(JITScript *jit) : JSC::RepatchBuffer(jit->code)
    { }

    Repatcher(const JSC::JITCode &code) : JSC::RepatchBuffer(code)
    { }
};

/*
 * On ARM, we periodically flush a constant pool into the instruction stream
 * where constants are found using PC-relative addressing. This is necessary
 * because the fixed-width instruction set doesn't support wide immediates.
 *
 * ICs perform repatching on the inline (fast) path by knowing small and
 * generally fixed code location offset values where the patchable instructions
 * live. Dumping a huge constant pool into the middle of an IC's inline path
 * makes the distance between emitted instructions potentially variable and/or
 * large, which makes the IC offsets invalid. We must reserve contiguous space
 * up front to prevent this from happening.
 */
#ifdef JS_CPU_ARM
class AutoReserveICSpace {
    typedef Assembler::Label Label;
    static const size_t reservedSpace = 64;

    Assembler           &masm;
#ifdef DEBUG
    Label               startLabel;
#endif

  public:
    AutoReserveICSpace(Assembler &masm) : masm(masm) {
        masm.ensureSpace(reservedSpace);
#ifdef DEBUG
        startLabel = masm.label();
#endif
    }

    ~AutoReserveICSpace() {
#ifdef DEBUG
        Label endLabel = masm.label();
        int spaceUsed = masm.differenceBetween(startLabel, endLabel);
        JS_ASSERT(spaceUsed >= 0);
        JS_ASSERT(size_t(spaceUsed) <= reservedSpace);
#endif
    }
};
# define RESERVE_IC_SPACE(__masm) AutoReserveICSpace arics(__masm)
#else
# define RESERVE_IC_SPACE(__masm) /* Nothing. */
#endif

} /* namespace js */
} /* namespace mjit */

#endif
