/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_CompileInfo_h
#define jit_CompileInfo_h

#include "jsfun.h"

#include "jit/Registers.h"

namespace js {
namespace jit {

inline unsigned
StartArgSlot(JSScript *script)
{
    // Reserved slots:
    // Slot 0: Scope chain.
    // Slot 1: Return value.

    // When needed:
    // Slot 2: Argumentsobject.

    // Note: when updating this, please also update the assert in SnapshotWriter::startFrame
    return 2 + (script->argumentsHasVarBinding() ? 1 : 0);
}

inline unsigned
CountArgSlots(JSScript *script, JSFunction *fun)
{
    // Slot x + 0: This value.
    // Slot x + 1: Argument 1.
    // ...
    // Slot x + n: Argument n.

    // Note: when updating this, please also update the assert in SnapshotWriter::startFrame
    return StartArgSlot(script) + (fun ? fun->nargs() + 1 : 0);
}

// Contains information about the compilation source for IR being generated.
class CompileInfo
{
  public:
    CompileInfo(JSScript *script, JSFunction *fun, jsbytecode *osrPc, bool constructing,
                ExecutionMode executionMode, bool scriptNeedsArgsObj)
      : script_(script), fun_(fun), osrPc_(osrPc), constructing_(constructing),
        executionMode_(executionMode), scriptNeedsArgsObj_(scriptNeedsArgsObj)
    {
        JS_ASSERT_IF(osrPc, JSOp(*osrPc) == JSOP_LOOPENTRY);

        // The function here can flow in from anywhere so look up the canonical
        // function to ensure that we do not try to embed a nursery pointer in
        // jit-code. Precisely because it can flow in from anywhere, it's not
        // guaranteed to be non-lazy. Hence, don't access its script!
        if (fun_) {
            fun_ = fun_->nonLazyScript()->functionNonDelazifying();
            JS_ASSERT(fun_->isTenured());
        }

        nimplicit_ = StartArgSlot(script)                   /* scope chain and argument obj */
                   + (fun ? 1 : 0);                         /* this */
        nargs_ = fun ? fun->nargs() : 0;
        nlocals_ = script->nfixed();
        nstack_ = script->nslots() - script->nfixed();
        nslots_ = nimplicit_ + nargs_ + nlocals_ + nstack_;
    }

    CompileInfo(unsigned nlocals, ExecutionMode executionMode)
      : script_(nullptr), fun_(nullptr), osrPc_(nullptr), constructing_(false),
        executionMode_(executionMode), scriptNeedsArgsObj_(false)
    {
        nimplicit_ = 0;
        nargs_ = 0;
        nlocals_ = nlocals;
        nstack_ = 1;  /* For FunctionCompiler::pushPhiInput/popPhiOutput */
        nslots_ = nlocals_ + nstack_;
    }

    JSScript *script() const {
        return script_;
    }
    JSFunction *funMaybeLazy() const {
        return fun_;
    }
    bool constructing() const {
        return constructing_;
    }
    jsbytecode *osrPc() {
        return osrPc_;
    }

    bool hasOsrAt(jsbytecode *pc) {
        JS_ASSERT(JSOp(*pc) == JSOP_LOOPENTRY);
        return pc == osrPc();
    }

    jsbytecode *startPC() const {
        return script_->code();
    }
    jsbytecode *limitPC() const {
        return script_->codeEnd();
    }

    const char *filename() const {
        return script_->filename();
    }

    unsigned lineno() const {
        return script_->lineno();
    }
    unsigned lineno(jsbytecode *pc) const {
        return PCToLineNumber(script_, pc);
    }

    // Script accessors based on PC.

    JSAtom *getAtom(jsbytecode *pc) const {
        return script_->getAtom(GET_UINT32_INDEX(pc));
    }

    PropertyName *getName(jsbytecode *pc) const {
        return script_->getName(GET_UINT32_INDEX(pc));
    }

    inline RegExpObject *getRegExp(jsbytecode *pc) const;

    JSObject *getObject(jsbytecode *pc) const {
        return script_->getObject(GET_UINT32_INDEX(pc));
    }

    inline JSFunction *getFunction(jsbytecode *pc) const;

    const Value &getConst(jsbytecode *pc) const {
        return script_->getConst(GET_UINT32_INDEX(pc));
    }

    jssrcnote *getNote(GSNCache &gsn, jsbytecode *pc) const {
        return GetSrcNote(gsn, script(), pc);
    }

    // Total number of slots: args, locals, and stack.
    unsigned nslots() const {
        return nslots_;
    }

    // Number of slots needed for Scope chain, return value,
    // maybe argumentsobject and this value.
    unsigned nimplicit() const {
        return nimplicit_;
    }
    // Number of arguments (without counting this value).
    unsigned nargs() const {
        return nargs_;
    }
    // Number of slots needed for local variables.
    unsigned nlocals() const {
        return nlocals_;
    }
    unsigned ninvoke() const {
        return nslots_ - nstack_;
    }

    uint32_t scopeChainSlot() const {
        JS_ASSERT(script());
        return 0;
    }
    uint32_t returnValueSlot() const {
        JS_ASSERT(script());
        return 1;
    }
    uint32_t argsObjSlot() const {
        JS_ASSERT(hasArguments());
        return 2;
    }
    uint32_t thisSlot() const {
        JS_ASSERT(funMaybeLazy());
        JS_ASSERT(nimplicit_ > 0);
        return nimplicit_ - 1;
    }
    uint32_t firstArgSlot() const {
        return nimplicit_;
    }
    uint32_t argSlotUnchecked(uint32_t i) const {
        // During initialization, some routines need to get at arg
        // slots regardless of how regular argument access is done.
        JS_ASSERT(i < nargs_);
        return nimplicit_ + i;
    }
    uint32_t argSlot(uint32_t i) const {
        // This should only be accessed when compiling functions for
        // which argument accesses don't need to go through the
        // argument object.
        JS_ASSERT(!argsObjAliasesFormals());
        return argSlotUnchecked(i);
    }
    uint32_t firstLocalSlot() const {
        return nimplicit_ + nargs_;
    }
    uint32_t localSlot(uint32_t i) const {
        return firstLocalSlot() + i;
    }
    uint32_t firstStackSlot() const {
        return firstLocalSlot() + nlocals();
    }
    uint32_t stackSlot(uint32_t i) const {
        return firstStackSlot() + i;
    }

    uint32_t startArgSlot() const {
        JS_ASSERT(script());
        return StartArgSlot(script());
    }
    uint32_t endArgSlot() const {
        JS_ASSERT(script());
        return CountArgSlots(script(), funMaybeLazy());
    }

    uint32_t totalSlots() const {
        JS_ASSERT(script() && funMaybeLazy());
        return nimplicit() + nargs() + nlocals();
    }

    bool isSlotAliased(uint32_t index) const {
        if (funMaybeLazy() && index == thisSlot())
            return false;

        uint32_t arg = index - firstArgSlot();
        if (arg < nargs()) {
            if (script()->formalIsAliased(arg))
                return true;
            return false;
        }

        uint32_t var = index - firstLocalSlot();
        if (var < nlocals()) {
            if (script()->varIsAliased(var))
                return true;
            return false;
        }

        JS_ASSERT(index >= firstStackSlot());
        return false;
    }

    bool hasArguments() const {
        return script()->argumentsHasVarBinding();
    }
    bool argumentsAliasesFormals() const {
        return script()->argumentsAliasesFormals();
    }
    bool needsArgsObj() const {
        return scriptNeedsArgsObj_;
    }
    bool argsObjAliasesFormals() const {
        return scriptNeedsArgsObj_ && !script()->strict();
    }

    ExecutionMode executionMode() const {
        return executionMode_;
    }

    bool isParallelExecution() const {
        return executionMode_ == ParallelExecution;
    }

  private:
    unsigned nimplicit_;
    unsigned nargs_;
    unsigned nlocals_;
    unsigned nstack_;
    unsigned nslots_;
    JSScript *script_;
    JSFunction *fun_;
    jsbytecode *osrPc_;
    bool constructing_;
    ExecutionMode executionMode_;

    // Whether a script needs an arguments object is unstable over compilation
    // since the arguments optimization could be marked as failed on the main
    // thread, so cache a value here and use it throughout for consistency.
    bool scriptNeedsArgsObj_;
};

} // namespace jit
} // namespace js

#endif /* jit_CompileInfo_h */
